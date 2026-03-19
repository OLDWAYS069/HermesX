#include "configuration.h"
#if !MESHTASTIC_EXCLUDE_BLUETOOTH
#include "BluetoothCommon.h"
#include "NimbleBluetooth.h"
#include "PowerFSM.h"

#include "concurrency/OSThread.h"
#include "main.h"
#include "mesh/PhoneAPI.h"
#include "mesh/mesh-pb-constants.h"
#include "platform/esp32/HermesCrashBreadcrumb.h"
#include "sleep.h"
#include <NimBLEDevice.h>
#include <array>
#include <atomic>
#include <mutex>

#define NIMBLE_BLUETOOTH_TO_PHONE_QUEUE_SIZE 8
#define NIMBLE_BLUETOOTH_FROM_PHONE_QUEUE_SIZE 4

// Isolation switch: if config still panics, keep BLE config traffic on default connection params
// so we can separate "conn param update" from "manifest scan" failures.
static constexpr bool kEnableBleConfigConnParamUpgrade = false;

NimBLECharacteristic *fromNumCharacteristic;
NimBLECharacteristic *BatteryCharacteristic;
NimBLECharacteristic *logRadioCharacteristic;
NimBLEServer *bleServer;

static bool passkeyShowing;
static std::atomic<uint16_t> nimbleBluetoothConnHandle{BLE_HS_CONN_HANDLE_NONE};

class BluetoothPhoneAPI : public PhoneAPI, public concurrency::OSThread
{
  public:
    BluetoothPhoneAPI() : concurrency::OSThread("NimbleBluetooth") { api_type = TYPE_BLE; }

    std::mutex fromPhoneMutex;
    std::atomic<size_t> fromPhoneQueueSize{0};
    std::array<NimBLEAttValue, NIMBLE_BLUETOOTH_FROM_PHONE_QUEUE_SIZE> fromPhoneQueue{};

    std::mutex toPhoneMutex;
    std::atomic<size_t> toPhoneQueueSize{0};
    std::array<std::array<uint8_t, meshtastic_FromRadio_size>, NIMBLE_BLUETOOTH_TO_PHONE_QUEUE_SIZE> toPhoneQueue{};
    std::array<size_t, NIMBLE_BLUETOOTH_TO_PHONE_QUEUE_SIZE> toPhoneQueueByteSizes{};
    size_t toPhoneQueueHead{0};
    std::array<uint8_t, meshtastic_FromRadio_size> toPhoneReadScratch{};
    std::atomic<bool> onReadCallbackIsWaitingForData{false};

    std::atomic<int32_t> readCount{0};
    std::atomic<int32_t> notifyCount{0};
    std::atomic<int32_t> writeCount{0};

    bool enqueueToPhoneQueue(const uint8_t *data, size_t numBytes)
    {
        std::lock_guard<std::mutex> guard(toPhoneMutex);
        const size_t queueSize = toPhoneQueueSize.load();
        if (queueSize >= NIMBLE_BLUETOOTH_TO_PHONE_QUEUE_SIZE) {
            return false;
        }

        const size_t storeAtIndex = (toPhoneQueueHead + queueSize) % NIMBLE_BLUETOOTH_TO_PHONE_QUEUE_SIZE;
        memcpy(toPhoneQueue[storeAtIndex].data(), data, numBytes);
        toPhoneQueueByteSizes[storeAtIndex] = numBytes;
        toPhoneQueueSize = queueSize + 1;
        return true;
    }

    size_t dequeueToPhoneQueue()
    {
        std::lock_guard<std::mutex> guard(toPhoneMutex);
        const size_t queueSize = toPhoneQueueSize.load();
        if (queueSize == 0) {
            return 0;
        }

        const size_t readIndex = toPhoneQueueHead;
        const size_t numBytes = toPhoneQueueByteSizes[readIndex];
        memcpy(toPhoneReadScratch.data(), toPhoneQueue[readIndex].data(), numBytes);
        toPhoneQueueHead = (toPhoneQueueHead + 1) % NIMBLE_BLUETOOTH_TO_PHONE_QUEUE_SIZE;
        toPhoneQueueSize = queueSize - 1;
        return numBytes;
    }

    void clearToPhoneQueue()
    {
        std::lock_guard<std::mutex> guard(toPhoneMutex);
        toPhoneQueueHead = 0;
        toPhoneQueueSize = 0;
    }

  protected:
    virtual size_t getNodePrefetchDepth() const override { return 12; }

    virtual int32_t runOnce() override
    {
        while (runOnceHasWorkToDo()) {
            // Process writes before reads so app write-then-read sequences stay synchronized.
            runOnceHandleFromPhoneQueue();
            runOnceHandleToPhoneQueue();
        }

        return INT32_MAX;
    }

    virtual void onConfigStart() override
    {
        LOG_INFO("BLE onConfigStart");
        hermesCrashBreadcrumbRecord(HermesCrashBreadcrumbId::BleConfigStart);

        if (!kEnableBleConfigConnParamUpgrade) {
            LOG_WARN("BLE config conn param upgrade disabled for isolation");
            return;
        }

        if (bleServer && isConnected()) {
            uint16_t conn_handle = nimbleBluetoothConnHandle.load();
            if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
                LOG_INFO("BLE request high-throughput config params");
                requestHighThroughputConnection(conn_handle);
                LOG_INFO("BLE high-throughput config params requested");
            }
        }
    }

    virtual void onConfigComplete() override
    {
        LOG_INFO("BLE onConfigComplete");
        hermesCrashBreadcrumbRecord(HermesCrashBreadcrumbId::BleConfigComplete);

        if (!kEnableBleConfigConnParamUpgrade) {
            return;
        }

        if (bleServer && isConnected()) {
            uint16_t conn_handle = nimbleBluetoothConnHandle.load();
            if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
                requestLowerPowerConnection(conn_handle);
            }
        }
    }

    bool runOnceHasWorkToDo() { return runOnceHasWorkToPhone() || runOnceHasWorkFromPhone(); }

    bool runOnceHasWorkToPhone() { return onReadCallbackIsWaitingForData || runOnceToPhoneCanPreloadNextPacket(); }

    bool runOnceToPhoneCanPreloadNextPacket()
    {
        if (!isConnected()) {
            return false;
        } else if (isSendingPackets()) {
            return false;
        } else {
            return toPhoneQueueSize < NIMBLE_BLUETOOTH_TO_PHONE_QUEUE_SIZE;
        }
    }

    void runOnceHandleToPhoneQueue()
    {
        uint8_t fromRadioBytes[meshtastic_FromRadio_size] = {0};
        size_t numBytes = 0;
        const bool wasWaitingForData = onReadCallbackIsWaitingForData;
        const bool queueWasEmpty = toPhoneQueueSize == 0;

        if (onReadCallbackIsWaitingForData || runOnceToPhoneCanPreloadNextPacket()) {
            numBytes = getFromRadio(fromRadioBytes);

            if (numBytes != 0) {
                if (!enqueueToPhoneQueue(fromRadioBytes, numBytes)) {
                    hermesCrashBreadcrumbRecord(HermesCrashBreadcrumbId::BleToPhoneQueueFull, numBytes & 0xffffU);
                    LOG_ERROR("Shouldn't happen! Drop FromRadio packet, toPhoneQueue full (%u bytes)", numBytes);
                } else if (wasWaitingForData || queueWasEmpty) {
                    hermesCrashBreadcrumbRecord(HermesCrashBreadcrumbId::BleToPhoneEnqueue, numBytes & 0xffffU);
                }
            }

            onReadCallbackIsWaitingForData = false;
        }
    }

    bool runOnceHasWorkFromPhone() { return fromPhoneQueueSize > 0; }

    void runOnceHandleFromPhoneQueue()
    {
        if (fromPhoneQueueSize > 0) {
            LOG_DEBUG("NimbleBluetooth: handling ToRadio packet, fromPhoneQueueSize=%u", fromPhoneQueueSize.load());

            NimBLEAttValue val;
            {
                std::lock_guard<std::mutex> guard(fromPhoneMutex);
                val = fromPhoneQueue[0];

                for (uint8_t i = 1; i < fromPhoneQueueSize; i++) {
                    fromPhoneQueue[i - 1] = fromPhoneQueue[i];
                }

                if (fromPhoneQueueSize > 0)
                    fromPhoneQueueSize--;
            }

            handleToRadio(val.data(), val.length());
        }
    }

    virtual void onNowHasData(uint32_t fromRadioNum) override
    {
        PhoneAPI::onNowHasData(fromRadioNum);

        if (!fromNumCharacteristic || !bleServer || bleServer->getConnectedCount() == 0) {
            LOG_DEBUG("Skip BLE fromNum notify, no active connection");
            return;
        }

        notifyCount.fetch_add(1);

        uint8_t val[4];
        put_le32(val, fromRadioNum);

        fromNumCharacteristic->setValue(val, sizeof(val));
        fromNumCharacteristic->notify();
    }

    virtual bool checkIsConnected() override { return bleServer && bleServer->getConnectedCount() > 0; }

    void requestHighThroughputConnection(uint16_t conn_handle)
    {
        LOG_INFO("BLE requestHighThroughputConnection");
        bleServer->updateConnParams(conn_handle, 6, 12, 0, 600);
    }

    void requestLowerPowerConnection(uint16_t conn_handle)
    {
        LOG_INFO("BLE requestLowerPowerConnection");
        bleServer->updateConnParams(conn_handle, 24, 40, 2, 600);
    }
};

static BluetoothPhoneAPI *bluetoothPhoneAPI;
/**
 * Subclasses can use this as a hook to provide custom notifications for their transport (i.e. bluetooth notifies)
 */

// Last ToRadio value received from the phone
static uint8_t lastToRadio[MAX_TO_FROM_RADIO_SIZE];
static size_t lastToRadioLen = 0;

class NimbleBluetoothToRadioCallback : public NimBLECharacteristicCallbacks
{
    virtual void onWrite(NimBLECharacteristic *pCharacteristic)
    {
        bluetoothPhoneAPI->writeCount.fetch_add(1);
        auto val = pCharacteristic->getValue();
        const size_t len = val.length();
        hermesCrashBreadcrumbRecord(HermesCrashBreadcrumbId::BleFromPhoneWrite, len & 0xffffU);

        if (lastToRadioLen != len || memcmp(lastToRadio, val.data(), len) != 0) {
            if (bluetoothPhoneAPI->fromPhoneQueueSize < NIMBLE_BLUETOOTH_FROM_PHONE_QUEUE_SIZE) {
                if (len <= sizeof(lastToRadio)) {
                    memcpy(lastToRadio, val.data(), len);
                    lastToRadioLen = len;
                } else {
                    lastToRadioLen = 0;
                }

                {
                    std::lock_guard<std::mutex> guard(bluetoothPhoneAPI->fromPhoneMutex);
                    bluetoothPhoneAPI->fromPhoneQueue.at(bluetoothPhoneAPI->fromPhoneQueueSize) = val;
                    bluetoothPhoneAPI->fromPhoneQueueSize++;
                }

                bluetoothPhoneAPI->setIntervalFromNow(0);
                concurrency::mainDelay.interrupt();
            } else {
                LOG_WARN("Drop ToRadio packet, fromPhoneQueue full (%u bytes)", len);
            }
        } else {
            LOG_DEBUG("Drop dup ToRadio packet we just saw");
        }
    }
};

class NimbleBluetoothFromRadioCallback : public NimBLECharacteristicCallbacks
{
    virtual void onRead(NimBLECharacteristic *pCharacteristic)
    {
        bluetoothPhoneAPI->readCount.fetch_add(1);
        int tries = 0;

        if (bluetoothPhoneAPI->toPhoneQueueSize == 0) {
            hermesCrashBreadcrumbRecord(HermesCrashBreadcrumbId::BleReadWait);
            bluetoothPhoneAPI->onReadCallbackIsWaitingForData = true;
            while (bluetoothPhoneAPI->onReadCallbackIsWaitingForData && tries < 4000) {
                bluetoothPhoneAPI->setIntervalFromNow(0);
                concurrency::mainDelay.interrupt();

                if (!bluetoothPhoneAPI->onReadCallbackIsWaitingForData) {
                    break;
                }

                delay(tries < 20 ? 1 : 5);
                tries++;
            }
        }

        const size_t numBytes = bluetoothPhoneAPI->dequeueToPhoneQueue();
        if (numBytes == 0 && tries > 0) {
            hermesCrashBreadcrumbRecord(HermesCrashBreadcrumbId::BleReadWait, tries & 0xffffU);
        }
        hermesCrashBreadcrumbRecord(HermesCrashBreadcrumbId::BleReadDequeue, numBytes & 0xffffU);
        pCharacteristic->setValue(bluetoothPhoneAPI->toPhoneReadScratch.data(), numBytes);

        if (numBytes != 0) {
            bluetoothPhoneAPI->setIntervalFromNow(0);
            concurrency::mainDelay.interrupt();
        }
    }
};

class NimbleBluetoothServerCallback : public NimBLEServerCallbacks
{
    virtual uint32_t onPassKeyRequest()
    {
        uint32_t passkey = config.bluetooth.fixed_pin;

        if (config.bluetooth.mode == meshtastic_Config_BluetoothConfig_PairingMode_RANDOM_PIN) {
            LOG_INFO("Use random passkey");
            // This is the passkey to be entered on peer - we pick a number >100,000 to ensure 6 digits
            passkey = random(100000, 999999);
        }
        LOG_INFO("*** Enter passkey %d on the peer side ***", passkey);

        powerFSM.trigger(EVENT_BLUETOOTH_PAIR);
        bluetoothStatus->updateStatus(new meshtastic::BluetoothStatus(std::to_string(passkey)));

#if HAS_SCREEN // Todo: migrate this display code back into Screen class, and observe bluetoothStatus
        screen->startAlert([passkey](OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) -> void {
            char btPIN[16] = "888888";
            snprintf(btPIN, sizeof(btPIN), "%06u", passkey);
            int x_offset = display->width() / 2;
            int y_offset = display->height() <= 80 ? 0 : 32;
            display->setTextAlignment(TEXT_ALIGN_CENTER);
            display->setFont(FONT_MEDIUM);
            display->drawString(x_offset + x, y_offset + y, "Bluetooth");

            display->setFont(FONT_SMALL);
            y_offset = display->height() == 64 ? y_offset + FONT_HEIGHT_MEDIUM - 4 : y_offset + FONT_HEIGHT_MEDIUM + 5;
            display->drawString(x_offset + x, y_offset + y, "Enter this code");

            display->setFont(FONT_LARGE);
            String displayPin(btPIN);
            String pin = displayPin.substring(0, 3) + " " + displayPin.substring(3, 6);
            y_offset = display->height() == 64 ? y_offset + FONT_HEIGHT_SMALL - 5 : y_offset + FONT_HEIGHT_SMALL + 5;
            display->drawString(x_offset + x, y_offset + y, pin);

            display->setFont(FONT_SMALL);
            String deviceName = "Name: ";
            deviceName.concat(getDeviceName());
            y_offset = display->height() == 64 ? y_offset + FONT_HEIGHT_LARGE - 6 : y_offset + FONT_HEIGHT_LARGE + 5;
            display->drawString(x_offset + x, y_offset + y, deviceName);
        });
#endif
        passkeyShowing = true;

        return passkey;
    }

    virtual void onAuthenticationComplete(ble_gap_conn_desc *desc)
    {
        LOG_INFO("BLE authentication complete");

        bluetoothStatus->updateStatus(new meshtastic::BluetoothStatus(meshtastic::BluetoothStatus::ConnectionState::CONNECTED));

        // Todo: migrate this display code back into Screen class, and observe bluetoothStatus
        if (passkeyShowing) {
            passkeyShowing = false;
            if (screen)
                screen->endAlert();
        }

        nimbleBluetoothConnHandle = desc->conn_handle;
    }

    virtual void onDisconnect(NimBLEServer *pServer, ble_gap_conn_desc *desc)
    {
        LOG_INFO("BLE disconnect");
        hermesCrashBreadcrumbRecord(HermesCrashBreadcrumbId::BleDisconnect);
        memset(lastToRadio, 0, sizeof(lastToRadio));
        lastToRadioLen = 0;

        bluetoothStatus->updateStatus(
            new meshtastic::BluetoothStatus(meshtastic::BluetoothStatus::ConnectionState::DISCONNECTED));

        if (bluetoothPhoneAPI) {
            bluetoothPhoneAPI->close();

            {
                std::lock_guard<std::mutex> guard(bluetoothPhoneAPI->fromPhoneMutex);
                bluetoothPhoneAPI->fromPhoneQueueSize = 0;
            }

            bluetoothPhoneAPI->onReadCallbackIsWaitingForData = false;
            bluetoothPhoneAPI->clearToPhoneQueue();

            bluetoothPhoneAPI->readCount = 0;
            bluetoothPhoneAPI->notifyCount = 0;
            bluetoothPhoneAPI->writeCount = 0;
        }

        nimbleBluetoothConnHandle = BLE_HS_CONN_HANDLE_NONE;
    }
};

static NimbleBluetoothToRadioCallback *toRadioCallbacks;
static NimbleBluetoothFromRadioCallback *fromRadioCallbacks;

void NimbleBluetooth::shutdown()
{
    // No measurable power saving for ESP32 during light-sleep(?)
#ifndef ARCH_ESP32
    // Shutdown bluetooth for minimum power draw
    LOG_INFO("Disable bluetooth");
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->reset();
    pAdvertising->stop();
#endif
}

// Proper shutdown for ESP32. Needs reboot to reverse.
void NimbleBluetooth::deinit()
{
#ifdef ARCH_ESP32
    LOG_INFO("Disable bluetooth until reboot");

#ifdef BLE_LED
    digitalWrite(BLE_LED, LOW);
#endif

    NimBLEDevice::deinit();
#endif
}

// Has initial setup been completed
bool NimbleBluetooth::isActive()
{
    return bleServer;
}

bool NimbleBluetooth::isConnected()
{
    return bleServer->getConnectedCount() > 0;
}

int NimbleBluetooth::getRssi()
{
    if (bleServer && isConnected()) {
        auto service = bleServer->getServiceByUUID(MESH_SERVICE_UUID);
        uint16_t handle = service->getHandle();
        return NimBLEDevice::getClientByID(handle)->getRssi();
    }
    return 0; // FIXME figure out where to source this
}

void NimbleBluetooth::setup()
{
    // Uncomment for testing
    // NimbleBluetooth::clearBonds();

    LOG_INFO("Init the NimBLE bluetooth module");

    NimBLEDevice::init(getDeviceName());
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    if (config.bluetooth.mode != meshtastic_Config_BluetoothConfig_PairingMode_NO_PIN) {
        NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM | BLE_SM_PAIR_AUTHREQ_SC);
        NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
        NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
        NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
    }
    bleServer = NimBLEDevice::createServer();

    NimbleBluetoothServerCallback *serverCallbacks = new NimbleBluetoothServerCallback();
    bleServer->setCallbacks(serverCallbacks, true);
    setupService();
    startAdvertising();
}

void NimbleBluetooth::setupService()
{
    NimBLEService *bleService = bleServer->createService(MESH_SERVICE_UUID);
    NimBLECharacteristic *ToRadioCharacteristic;
    NimBLECharacteristic *FromRadioCharacteristic;
    // Define the characteristics that the app is looking for
    if (config.bluetooth.mode == meshtastic_Config_BluetoothConfig_PairingMode_NO_PIN) {
        ToRadioCharacteristic = bleService->createCharacteristic(TORADIO_UUID, NIMBLE_PROPERTY::WRITE);
        FromRadioCharacteristic = bleService->createCharacteristic(FROMRADIO_UUID, NIMBLE_PROPERTY::READ);
        fromNumCharacteristic = bleService->createCharacteristic(FROMNUM_UUID, NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ);
        logRadioCharacteristic =
            bleService->createCharacteristic(LOGRADIO_UUID, NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ, 512U);
    } else {
        ToRadioCharacteristic = bleService->createCharacteristic(
            TORADIO_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_AUTHEN | NIMBLE_PROPERTY::WRITE_ENC);
        FromRadioCharacteristic = bleService->createCharacteristic(
            FROMRADIO_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_AUTHEN | NIMBLE_PROPERTY::READ_ENC);
        fromNumCharacteristic =
            bleService->createCharacteristic(FROMNUM_UUID, NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ |
                                                               NIMBLE_PROPERTY::READ_AUTHEN | NIMBLE_PROPERTY::READ_ENC);
        logRadioCharacteristic = bleService->createCharacteristic(
            LOGRADIO_UUID,
            NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_AUTHEN | NIMBLE_PROPERTY::READ_ENC, 512U);
    }
    bluetoothPhoneAPI = new BluetoothPhoneAPI();

    toRadioCallbacks = new NimbleBluetoothToRadioCallback();
    ToRadioCharacteristic->setCallbacks(toRadioCallbacks);

    fromRadioCallbacks = new NimbleBluetoothFromRadioCallback();
    FromRadioCharacteristic->setCallbacks(fromRadioCallbacks);

    bleService->start();

    // Setup the battery service
    NimBLEService *batteryService = bleServer->createService(NimBLEUUID((uint16_t)0x180f)); // 0x180F is the Battery Service
    BatteryCharacteristic = batteryService->createCharacteristic( // 0x2A19 is the Battery Level characteristic)
        (uint16_t)0x2a19, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 1);

    NimBLE2904 *batteryLevelDescriptor = (NimBLE2904 *)BatteryCharacteristic->createDescriptor((uint16_t)0x2904);
    batteryLevelDescriptor->setFormat(NimBLE2904::FORMAT_UINT8);
    batteryLevelDescriptor->setNamespace(1);
    batteryLevelDescriptor->setUnit(0x27ad);

    batteryService->start();
}

void NimbleBluetooth::startAdvertising()
{
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->reset();
    pAdvertising->addServiceUUID(MESH_SERVICE_UUID);
    pAdvertising->addServiceUUID(NimBLEUUID((uint16_t)0x180f)); // 0x180F is the Battery Service
    pAdvertising->start(0);
}

/// Given a level between 0-100, update the BLE attribute
void updateBatteryLevel(uint8_t level)
{
    if ((config.bluetooth.enabled == true) && bleServer && nimbleBluetooth->isConnected()) {
        BatteryCharacteristic->setValue(&level, 1);
        BatteryCharacteristic->notify();
    }
}

void NimbleBluetooth::clearBonds()
{
    LOG_INFO("Clearing bluetooth bonds!");
    NimBLEDevice::deleteAllBonds();
}

void NimbleBluetooth::sendLog(const uint8_t *logMessage, size_t length)
{
    if (!bleServer || !isConnected() || length > 512) {
        return;
    }
    logRadioCharacteristic->notify(logMessage, length, true);
}

void clearNVS()
{
    NimBLEDevice::deleteAllBonds();
#ifdef ARCH_ESP32
    ESP.restart();
#endif
}
#endif
