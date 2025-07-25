#include "LighthouseModule.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "FSCommon.h"
#include "SPILock.h"
#include "configuration.h"
#include "sleep.h"
#include <cstring>
#include "HermesXLog.h"

static const char *bootFile = "/prefs/lighthouse_boot.bin";
static const char *modeFile = "/prefs/lighthouse_mode.bin";
//////static const uint32_t WAIT_TIME_MS = 2UL * 24UL * 60UL * 60UL * 1000UL; // two days
static const uint32_t WAIT_TIME_MS = 20UL * 60UL * 1000UL; // 20 minutes for test by oldways_20250724

LighthouseModule *lighthouseModule = nullptr;

LighthouseModule::LighthouseModule()
    : SinglePortModule("lighthouse", meshtastic_PortNum_TEXT_MESSAGE_APP),
      concurrency::OSThread("lighthouseTask", 300)
{
    lighthouseModule = this;
    loadBoot();
    loadState();
    if (firstBootMillis == 0) {
        firstBootMillis = millis();
        saveBoot();
    }
}

void LighthouseModule::loadBoot()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    auto f = FSCom.open(bootFile, FILE_O_READ);
    if (f) {
        if (f.available() >= (int)sizeof(firstBootMillis)) {
            f.read((uint8_t *)&firstBootMillis, sizeof(firstBootMillis));
        }
        f.close();
    }
#endif
}

void LighthouseModule::saveBoot()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    FSCom.mkdir("/prefs");
    if (FSCom.exists(bootFile))
        FSCom.remove(bootFile);
    auto f = FSCom.open(bootFile, FILE_O_WRITE);
    if (f) {
        f.write((uint8_t *)&firstBootMillis, sizeof(firstBootMillis));
        f.flush();
        f.close();
    }
#endif
}

void LighthouseModule::loadState()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    auto f = FSCom.open(modeFile, FILE_O_READ);
    if (f) {
        uint8_t v = 0;
        if (f.available() > 0) {
            f.read(&v, 1);
            emergencyModeActive = v != 0;
        }
        f.close();
    }
#endif
}

void LighthouseModule::saveState()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    FSCom.mkdir("/prefs");
    if (FSCom.exists(modeFile))
        FSCom.remove(modeFile);
    auto f = FSCom.open(modeFile, FILE_O_WRITE);
    if (f) {
        uint8_t v = emergencyModeActive ? 1 : 0;
        f.write(&v, 1);
        f.flush();
        f.close();
    }
#endif
}

bool LighthouseModule::wantPacket(const meshtastic_MeshPacket *p)
{
    if (p->decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP)
        return false;

    if (p->decoded.payload.size > 0 && p->decoded.payload.bytes[0] == '@')
        return true;

    if (emergencyModeActive)
        return true;

    if ((millis() - firstBootMillis) >= WAIT_TIME_MS)
        return true;

    return false;
}

ProcessMessage LighthouseModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    char txt[256];
    size_t len = mp.decoded.payload.size;
    if (len >= sizeof(txt))
        len = sizeof(txt) - 1;
    memcpy(txt, mp.decoded.payload.bytes, len);
    txt[len] = '\0';

     while (len > 0 && (txt[len - 1] == '\n' || txt[len - 1] == '\r' || txt[len - 1] == ' ')) {
        txt[--len] = '\0';
    }

    HERMESX_LOG_INFO("[Lighthouse] Received text=[%s] strlen=%d", txt, strlen(txt));


    if (strcmp(txt, "@ResetLighthouse") == 0) {
        emergencyModeActive = false;
        firstBootMillis = millis();
        saveBoot();
        saveState();
        config.device.role = meshtastic_Config_DeviceConfig_Role_CLIENT_HIDDEN;
        nodeDB->saveToDisk(SEGMENT_CONFIG);
        saveState();
       
        const char *msg = "LightHouse啟動";
        meshtastic_MeshPacket *p = allocDataPacket();
        p->to = NODENUM_BROADCAST;
        p->channel = 0;  // or 3, if channel 3 is configured correctly
        p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
        p->want_ack = true;
        
        p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
        p->decoded.payload.size = strlen(msg);
        memcpy(p->decoded.payload.bytes, msg, p->decoded.payload.size);
        service->sendToMesh(p, RX_SRC_LOCAL, true);     

   
        HERMESX_LOG_INFO("CLOSE LIGHTHOUSE,RESTARING");
        

        delay(10000);  // 確保 log 有時間送出
        ESP.restart();

        return ProcessMessage::CONTINUE;
    }

    if (strcmp(txt, "@EmergencyActive") == 0 && !emergencyModeActive) {
    emergencyModeActive = true;
    config.has_device = true;
    config.device.role = meshtastic_Config_DeviceConfig_Role_ROUTER;
    config.power.is_power_saving = false;
    roleCorrected = true;

    nodeDB->saveToDisk(SEGMENT_CONFIG);
    saveState();

    const char *msg = "LightHouse將在10秒後啟動\xE5\x95\x9F\xE5\x8B\x95"; // "LightHouse啟動"（UTF-8）
    meshtastic_MeshPacket *p = allocDataPacket();         // 分配新的封包
    p->to = NODENUM_BROADCAST;                            // 廣播模式
    p->channel = 0;                                       // 發送到 Channel 3
    p->decoded.payload.size = strlen(msg);                // 設定 payload 長度
    memcpy(p->decoded.payload.bytes, msg, p->decoded.payload.size); // 寫入內容
    service->sendToMesh(p, RX_SRC_LOCAL, true);           // 本地來源，可靠傳輸（ack）

    HERMESX_LOG_INFO("LIGHTHOUSE ACTIVE. Restarting...");
    delay(10000);  
    ESP.restart();

    return ProcessMessage::CONTINUE;
}
        
return ProcessMessage::CONTINUE;

}

int32_t LighthouseModule::runOnce()
{
    if (!emergencyModeActive && (millis() - firstBootMillis) < WAIT_TIME_MS) {
#ifdef LIGHTHOUSE_DEBUG
        HERMESX_LOG_INFO("⚠️ DEBUG MODE: Skipping deep sleep, using delay instead");
        delay(1000);  // 偵錯時保持運作方便監看 Serial
#else
    #ifdef ARCH_ESP32
        const uint32_t sleepIntervalMs = 10UL * 60UL * 1000UL; // 10分鐘
        doDeepSleep(sleepIntervalMs, false, false);
    #else
        delay(1000);
    #endif
#endif
        return 0;
    }

    return 500;
}