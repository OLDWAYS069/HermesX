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
    if (emergencyModeActive)
        return true;
    if ((millis() - firstBootMillis) >= WAIT_TIME_MS) {
        HERMESX_LOG_INFO("STARTING OBSEVERD");
        return true;
    }
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

    if (strstr(txt, "@ResetLighthouse")) {
        emergencyModeActive = false;
        firstBootMillis = millis();
        saveBoot();
        saveState();
        HERMESX_LOG_INFO("CLOSE LIGHTHOUSE");
        return ProcessMessage::CONTINUE;
    }

    if (!emergencyModeActive && strstr(txt, "@EmergencyActive")) {
        emergencyModeActive = true;
        config.device.role = meshtastic_Config_DeviceConfig_Role_CLIENT;
        config.power.is_power_saving = false;
        roleCorrected = true;
        nodeDB->saveToDisk(SEGMENT_CONFIG);
        saveState();
        HERMESX_LOG_INFO("start LIGHTHOUSE");
    }
    return ProcessMessage::CONTINUE;
}

int32_t LighthouseModule::runOnce()
{
    if (!emergencyModeActive && (millis() - firstBootMillis) < WAIT_TIME_MS) {
#ifdef ARCH_ESP32
    const uint32_t sleepIntervalMs = 10UL * 60UL * 1000UL; // 10分鐘
    doDeepSleep(sleepIntervalMs, false, false);
    HERMESX_LOG_INFO("I`M SLEEP");
#else
        delay(1000);
#endif
        return 0;
    }

    // no periodic tasks needed, just keep thread alive
    return 500;
}