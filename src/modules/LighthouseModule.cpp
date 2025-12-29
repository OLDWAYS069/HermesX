#include "LighthouseModule.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "FSCommon.h"
#include "SPILock.h"
#include "configuration.h"
#include "sleep.h"
#include "main.h"
#include "graphics/Screen.h"
#include "modules/HermesXInterfaceModule.h"
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <string>
#include "HermesXLog.h"
#include "mesh/Channels.h"
#include "mesh/Router.h"
#include "modules/HermesEmergencyState.h"
#include "PowerStatus.h"
#include <cstdio>


static const char *bootFile = "/prefs/lighthouse_boot.bin";
static const char *modeFile = "/prefs/lighthouse_mode.bin";
static const char *whitelistFile = "/prefs/lighthouse_whitelist.txt";
static const char *passphraseFile = "/prefs/lighthouse_passphrase.txt";

static const uint32_t WAIT_TIME_MS = 10UL * 1000UL;
static const uint32_t POLLING_AWAKE_MS = 300000UL; // 醒來期間
static const uint32_t POLLING_SLEEP_MS = 1800000UL; // 睡覺時間

LighthouseModule *lighthouseModule = nullptr;
static bool lighthouseStatusShown = false;
static uint32_t lighthouseStatusDueMs = 0;

namespace
{
// Normalize leading fullwidth '@' (U+FF20) to ASCII '@'
void normalizeAtPrefix(char *txt, size_t &len)
{
    if (len < 3)
        return;
    const unsigned char b0 = static_cast<unsigned char>(txt[0]);
    const unsigned char b1 = static_cast<unsigned char>(txt[1]);
    const unsigned char b2 = static_cast<unsigned char>(txt[2]);
    if (b0 == 0xEF && b1 == 0xBC && b2 == 0xA0) {
        txt[0] = '@';
        // shift left by two bytes
        memmove(txt + 1, txt + 3, len - 2);
        len -= 2;
        txt[len] = '\0';
    }
}

void showLocalStatusOverlay(bool emergencyModeActive, bool pollingModeRequested)
{
    if (!HermesXInterfaceModule::instance) {
        HERMESX_LOG_WARN("skip status banner: HermesX interface not ready");
        return;
    }

    const __FlashStringHelper *text = nullptr;
    uint16_t color = 0;
    uint32_t durationMs = 10000;
    if (emergencyModeActive) {
        text = F("EM：偵測到緊急狀態\n長按SAFE回報安全\nEM模式下禁用一般文字");
        color = 0xF800; // red
    } else if (pollingModeRequested) {
        text = F("節能輪詢模式");
        color = 0xFFE0; // yellow
    } else {
        text = F("行動模式");
        color = 0x07E0; // green
    }
    HERMESX_LOG_INFO("Lighthouse show status banner: %s", emergencyModeActive ? "EM" : (pollingModeRequested ? "SILENT" : "IDLE"));
    HermesXInterfaceModule::instance->showEmergencyBanner(true, text, color, durationMs);
    // Nudge UI so banner renders promptly using the same face pipeline as send/recv
    HermesXInterfaceModule::instance->startReceiveAnim();
}

void appendNodeId(String &msg)
{
    const char *sn = owner.short_name;
    char buf[24];
    if (sn && sn[0]) {
        snprintf(buf, sizeof(buf), "\nID:%s", sn);
    } else {
        snprintf(buf, sizeof(buf), "\nID:%04x", myNodeInfo.my_node_num & 0x0ffff);
    }
    msg += String(buf);
}

} // namespace

LighthouseModule::LighthouseModule()
    : SinglePortModule("lighthouse", meshtastic_PortNum_TEXT_MESSAGE_APP),
      concurrency::OSThread("lighthouseTask", 300)
{
    lighthouseModule = this;
    loadBoot();
    loadState();
    loadWhitelist();
    loadPassphrase();
    HermesLoadEmergencyAwaitingSafe();
    if (emergencyModeActive) {
        HermesSetEmergencyAwaitingSafe(true, false); // ensure awaiting SAFE after reboot; no double-save
    }

    // 僅當 boot file 沒載入成功時才設為現在時間
    if (firstBootMillis == 0) {
        HERMESX_LOG_INFO("first startup or lost bootFile ， building firstBootMillis");
        firstBootMillis = millis();
        saveBoot();
    } else {
        HERMESX_LOG_INFO(" load firstBootMillis = %lu from bootfile", firstBootMillis);
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
            HERMESX_LOG_INFO("boot loaded: firstBootMillis = %lu", firstBootMillis);
        } else {
            HERMESX_LOG_WARN("boot file too short, resetting firstBootMillis");
            firstBootMillis = 0;
        }
        f.close();
    } else {
        HERMESX_LOG_INFO("boot file not found, initializing firstBootMillis = 0");
        firstBootMillis = 0;
    }
#endif
}

void LighthouseModule::saveBoot()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);

    if (!FSCom.exists("/prefs"))
        FSCom.mkdir("/prefs");

    if (FSCom.exists(bootFile))
        FSCom.remove(bootFile);

    auto f = FSCom.open(bootFile, FILE_O_WRITE);
    if (f) {
        f.write((uint8_t *)&firstBootMillis, sizeof(firstBootMillis));
        f.flush();
        f.close();
        HERMESX_LOG_INFO("save firstBootMillis = %lu", firstBootMillis);
    } else {
        HERMESX_LOG_WARN("can`t rite bootFile");
    }
#endif
}

void LighthouseModule::loadState()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    auto f = FSCom.open(modeFile, FILE_O_READ);
    if (f) {
        if (f && f.available() >= 2) {
        uint8_t flags[2];
        f.read(flags, sizeof(flags));
        emergencyModeActive = (flags[0] == 1);
        pollingModeRequested = (flags[1] == 1);
        f.close();
    }
      
    }
#endif
}

void LighthouseModule::loadWhitelist()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    emergencyWhitelist.clear();

    auto f = FSCom.open(whitelistFile, FILE_O_READ);
    if (!f) {
        HERMESX_LOG_INFO("lighthouse whitelist missing (%s); @EmergencyActive will be ignored", whitelistFile);
        return;
    }

    auto processLine = [&](String &line) {
        line.trim();
        if (line.length() == 0 || line[0] == '#')
            return;

        char *end = nullptr;
        uint32_t value = strtoul(line.c_str(), &end, 0);
        if (end && *end == '\0') {
            emergencyWhitelist.push_back(static_cast<NodeNum>(value));
        } else {
            HERMESX_LOG_WARN("skip invalid whitelist entry: %s", line.c_str());
        }
    };

    String line;
    while (f.available()) {
        char c = f.read();
        if (c == '\n' || c == '\r') {
            processLine(line);
            line = "";
        } else {
            line += c;
        }
    }
    processLine(line);
    f.close();

    if (emergencyWhitelist.empty()) {
        HERMESX_LOG_WARN("lighthouse whitelist loaded but empty; @EmergencyActive will be rejected");
    } else {
        HERMESX_LOG_INFO("lighthouse whitelist loaded (%u entries)",
                         static_cast<unsigned int>(emergencyWhitelist.size()));
    }
#else
    HERMESX_LOG_INFO("FSCom not available; whitelist disabled");
#endif
}

bool LighthouseModule::isEmergencyActiveAllowed(NodeNum from) const
{
    if (emergencyWhitelist.empty())
        return false;
    return std::find(emergencyWhitelist.begin(), emergencyWhitelist.end(), from) != emergencyWhitelist.end();
}

void LighthouseModule::loadPassphrase()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    emergencyPassphrase = "";

    auto f = FSCom.open(passphraseFile, FILE_O_READ);
    if (!f) {
        HERMESX_LOG_INFO("lighthouse passphrase missing (%s); passphrase auth disabled", passphraseFile);
        return;
    }

    String content;
    while (f.available()) {
        content += static_cast<char>(f.read());
    }
    f.close();

    content.trim();
    if (content.length() == 0) {
        HERMESX_LOG_WARN("lighthouse passphrase file empty; passphrase auth disabled");
        emergencyPassphrase = "";
        return;
    }

    emergencyPassphrase = content;
    HERMESX_LOG_INFO("lighthouse passphrase loaded (len=%d)", emergencyPassphrase.length());
#else
    HERMESX_LOG_INFO("FSCom not available; passphrase disabled");
#endif
}

bool LighthouseModule::isEmergencyActiveAuthorized(const char *txt, NodeNum from) const
{
    bool passOk = false;
    if (emergencyPassphrase.length() > 0) {
        constexpr const char *kPrefix = "@EmergencyActive";
        const size_t prefixLen = strlen(kPrefix);
        if (strncmp(txt, kPrefix, prefixLen) == 0) {
            const char *p = txt + prefixLen;
            while (*p == ' ' || *p == ':' || *p == '=') {
                ++p;
            }
            if (*p != '\0') {
                passOk = (emergencyPassphrase == String(p));
            }
        }
    }

    const bool whiteOk = isEmergencyActiveAllowed(from);
    if (!passOk && !whiteOk) {
        HERMESX_LOG_WARN("reject @EmergencyActive from 0x%x (pass %s, whitelist %s)", from,
                         emergencyPassphrase.length() ? "mismatch" : "disabled",
                         emergencyWhitelist.empty() ? "empty" : "miss");
    }
    return passOk || whiteOk;
}

void LighthouseModule::exitEmergencyMode()
{
    emergencyModeActive = false;
    pollingModeRequested = false;
    HermesClearEmergencyAwaitingSafe();
    saveState();
    HERMESX_LOG_INFO("Lighthouse exit EM and clear awaiting SAFE; rebooting to normal mode");
#ifdef ARDUINO_ARCH_ESP32
    delay(500); // allow log flush
    ESP.restart();
#endif
}

void flushDelaySleep(uint32_t extraDelay = 1000, uint32_t sleepMs = 1800000UL)
{
    HERMESX_LOG_INFO("wait fo pak（delay %ums）...", extraDelay);
    delay(extraDelay);  // 模擬封包 flush 等待
    HERMESX_LOG_INFO("go Deep Sleep（%lu ms）", sleepMs);
    doDeepSleep(sleepMs, false, false);
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
        uint8_t flags[2];
        flags[0] = emergencyModeActive ? 1 : 0;
        flags[1] = pollingModeRequested ? 1 : 0;

        f.write(flags, sizeof(flags));

        f.flush();
        f.close();
    }
#endif
}

void LighthouseModule::broadcastStatusMessage()
{
    
    String msg;
    uint32_t now = millis();

    if (emergencyModeActive) {
        msg = u8"[HermeS]\n模式：Lighthouse Active\n緊急模式已啟動";
    }
       else if (pollingModeRequested) {
        uint32_t awakeElapsed = now % POLLING_AWAKE_MS;
        uint32_t remainingToSleep = (POLLING_AWAKE_MS - awakeElapsed) / 1000;
        uint32_t nextWakeIn = POLLING_SLEEP_MS / 1000;

        msg = u8"[HermeS]\n模式：Silent\n節能輪詢中，將於 ";
        msg += String(remainingToSleep);
        msg += u8" 秒後進入 Deep Sleep";

        msg += u8"\n預計將在 ";
        msg += String(nextWakeIn);
        msg += u8" 秒後重新喚醒";
    }

    else if (!emergencyModeActive && !pollingModeRequested) {
    msg = u8"[HermeS]\n模式：中繼站";
    }   

    appendNodeId(msg);

    if (msg.length() == 0) return;

    meshtastic_MeshPacket *p = allocDataPacket();
    if (!p) return;

    p->to = NODENUM_BROADCAST;
    p->channel = 0;
    p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    p->want_ack = false;
    p->decoded.payload.size = strlen(msg.c_str());
    memcpy(p->decoded.payload.bytes, msg.c_str(), p->decoded.payload.size);

    service->sendToMesh(p, RX_SRC_LOCAL, false);
    HERMESX_LOG_INFO("Broadcast status: %s", msg.c_str());
}

void LighthouseModule::IntroduceMessage()
{
    
    String msg;
    
    msg = u8"[HermeS]\n大家好，我是 HermeS Shine1，一台可以遠端控制的無人管理站點\n"
              u8"使用說明：https://www.facebook.com/share/p/1EEThBhZeR/";
  
    appendNodeId(msg);

    if (msg.length() == 0) return;

    meshtastic_MeshPacket *p = allocDataPacket();
    if (!p) return;

    p->to = NODENUM_BROADCAST;
    p->channel = 0;
    p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    p->want_ack = false;
    p->decoded.payload.size = strlen(msg.c_str());
    memcpy(p->decoded.payload.bytes, msg.c_str(), p->decoded.payload.size);

    service->sendToMesh(p, RX_SRC_LOCAL, false);
    LOG_INFO("Broadcast status: %s", msg.c_str());
}



bool LighthouseModule::wantPacket(const meshtastic_MeshPacket *p)
{
    if (p->decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP)
        return false;

    if (p->decoded.payload.size > 0 &&
        (p->decoded.payload.bytes[0] == '@' ||
         (p->decoded.payload.size >= 3 && static_cast<unsigned char>(p->decoded.payload.bytes[0]) == 0xEF &&
          static_cast<unsigned char>(p->decoded.payload.bytes[1]) == 0xBC &&
          static_cast<unsigned char>(p->decoded.payload.bytes[2]) == 0xA0)))
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

    normalizeAtPrefix(txt, len);

     while (len > 0 && (txt[len - 1] == '\n' || txt[len - 1] == '\r' || txt[len - 1] == ' ')) {
        txt[--len] = '\0';
    }

    HERMESX_LOG_INFO("[Lighthouse] Received text=[%s] strlen=%d", txt, strlen(txt));


    if (strcmp(txt, "@Repeater") == 0) {
        emergencyModeActive = false;
        pollingModeRequested = false;
        firstBootMillis = millis();
        saveBoot();

        // 切換為固定中繼站並拉高發射功率
        config.has_device = true;
        config.device.role = meshtastic_Config_DeviceConfig_Role_REPEATER;
        config.has_lora = true;
        config.lora.tx_power = 27;
        config.has_power = true;
        config.power.is_power_saving = false;

        saveState();
        nodeDB->saveToDisk(SEGMENT_CONFIG);
        roleCorrected = true;
                     
        HERMESX_LOG_INFO("Switch LIGHTHOUSE to REPEATER (tx_power=27dBm), restarting...");
        
        delay(15000);  // 確保 log 有時間送出
        ESP.restart();

        return ProcessMessage::CONTINUE;
    }

    if (strncmp(txt, "@EmergencyActive", 16) == 0) {
        if (!isEmergencyActiveAuthorized(txt, mp.from)) {
            HERMESX_LOG_WARN("ignore @EmergencyActive from 0x%x (not authorized)", mp.from);
            return ProcessMessage::CONTINUE;
        }

        if (!emergencyModeActive) {
            HERMESX_LOG_INFO("Accept @EmergencyActive from 0x%x", mp.from);
            emergencyModeActive = true;
            pollingModeRequested = false;
            config.has_device = true;
            config.device.role = meshtastic_Config_DeviceConfig_Role_ROUTER;
            config.power.is_power_saving = false;
            HermesSetEmergencyAwaitingSafe(true);

            roleCorrected = true;

            nodeDB->saveToDisk(SEGMENT_CONFIG);
            saveState();

            HERMESX_LOG_INFO("LIGHTHOUSE ACTIVE. Restarting...");
            delay(15000);
            ESP.restart();
        }

        return ProcessMessage::CONTINUE;
    }

    if (strcmp(txt, "@GoToSleep") == 0) {
        emergencyModeActive = false;
        pollingModeRequested = true;
        firstBootMillis = millis();
        config.device.role = meshtastic_Config_DeviceConfig_Role_ROUTER_LATE;

        saveBoot();
        saveState();

        delay(15000); // 確保 log 有時間送出
        ESP.restart();

        return ProcessMessage::CONTINUE;
    }

    if (strcmp(txt, "@Status") == 0) {
        if (HERMESX_LH_BROADCAST_ON_BOOT) {
            broadcastStatusMessage();
        } else {
            if (HermesXInterfaceModule::instance) {
                showLocalStatusOverlay(emergencyModeActive, pollingModeRequested);
                lighthouseStatusShown = true;
            } else {
                HERMESX_LOG_WARN("no HermesX interface; skip local status");
            }
        }
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(txt, "@BAT") == 0) {
        String msg;
        if (!powerStatus || !powerStatus->getHasBattery()) {
            msg = u8"[HermeS]\n電池：無電池或未檢測";
        } else {
            const int pct = powerStatus->getBatteryChargePercent();
            const int mv = powerStatus->getBatteryVoltageMv();
            const bool charging = powerStatus->getIsCharging();

            char buf[96];
            const int v_int = mv / 1000;
            const int v_dec = (mv % 1000) / 10;
            snprintf(buf, sizeof(buf), "[HermeS]\n電池：%d%%%s\n電壓：%d.%02dV",
                     pct, charging ? "(充電中)" : "", v_int, v_dec);
            msg = String(buf);
        }

        appendNodeId(msg);

        meshtastic_MeshPacket *p = allocDataPacket();
        if (p) {
            p->to = NODENUM_BROADCAST;
            p->channel = 2;
            p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
            p->want_ack = false;
            p->decoded.payload.size = strlen(msg.c_str());
            memcpy(p->decoded.payload.bytes, msg.c_str(), p->decoded.payload.size);

            service->sendToMesh(p, RX_SRC_LOCAL, false);
            HERMESX_LOG_INFO("Broadcast battery status: %s", msg.c_str());
        } else {
            HERMESX_LOG_WARN("Unable to alloc packet for battery status");
        }

        return ProcessMessage::CONTINUE;
    }

    if (strcmp(txt, "@HiHermes") == 0) {
        hihermes = true;
        emergencyModeActive = false;
        pollingModeRequested = false;
        IntroduceMessage();
        HERMESX_LOG_INFO("@HiHermes INTRODUCING");

        return ProcessMessage::CONTINUE;
    }

    if (strcmp(txt, "@戳") == 0) {
        const char *reply = u8"討厭><";
        meshtastic_MeshPacket *p = allocDataPacket();
        if (p) {
            p->to = NODENUM_BROADCAST;
            p->channel = 0;
            p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
            p->want_ack = false;
            p->decoded.payload.size = strlen(reply);
            memcpy(p->decoded.payload.bytes, reply, p->decoded.payload.size);

            service->sendToMesh(p, RX_SRC_LOCAL, false);
            HERMESX_LOG_INFO("Broadcast poke reply");
        } else {
            HERMESX_LOG_WARN("Unable to alloc packet for poke reply");
        }
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(txt, "@HermesBase") == 0) {
        const char *reply =
            u8"HermesBase是一套可以提供遠端管理、離網布告欄的系統\n更多資訊：連結";
        meshtastic_MeshPacket *p = allocDataPacket();
        if (p) {
            p->to = NODENUM_BROADCAST;
            p->channel = 0;
            p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
            p->want_ack = false;
            p->decoded.payload.size = strlen(reply);
            memcpy(p->decoded.payload.bytes, reply, p->decoded.payload.size);

            service->sendToMesh(p, RX_SRC_LOCAL, false);
            HERMESX_LOG_INFO("Broadcast HermesBase info");
        } else {
            HERMESX_LOG_WARN("Unable to alloc packet for HermesBase reply");
        }
        return ProcessMessage::CONTINUE;
    }

    return ProcessMessage::CONTINUE;

}

bool firstTime = true;

int32_t LighthouseModule::runOnce()
{
    static const uint32_t POLLING_AWAKE_MS = 300000UL;  // 醒來運作 300 秒
    static const uint32_t POLLING_SLEEP_MS = 1800000UL;  // deep sleep 30分鐘
    static uint32_t awakeStart = 0;

    if (firstTime) {
        firstTime = false;
        awakeStart = millis();
        if (HERMESX_LH_BROADCAST_ON_BOOT) {
            broadcastStatusMessage();
            HERMESX_LOG_INFO("broadcasting...");
        } else {
            lighthouseStatusDueMs = millis() + 13000; // wait for boot screen to finish
        }
    }

    if (!HERMESX_LH_BROADCAST_ON_BOOT && !lighthouseStatusShown && HermesXInterfaceModule::instance &&
        lighthouseStatusDueMs && millis() >= lighthouseStatusDueMs) {
        showLocalStatusOverlay(emergencyModeActive, pollingModeRequested);
        lighthouseStatusShown = true;
    }

    if (pollingModeRequested && !emergencyModeActive) {
        uint32_t now = millis();
        uint32_t awakeElapsed = now - awakeStart;

#ifdef LIGHTHOUSE_DEBUG
        HERMESX_LOG_INFO("DEBUG 模式，延遲中...");
        delay(1000);
#else
        if (awakeElapsed >= POLLING_AWAKE_MS) {
            HERMESX_LOG_INFO("awake is out,wait fo broadcast");
            delay(1000);  // 模擬 Radio Flush

            HERMESX_LOG_INFO("Starting Deep Sleep %lu ms", POLLING_SLEEP_MS);
            doDeepSleep(POLLING_SLEEP_MS, false, false);
        }
#endif
        return 100;  // 醒來時每 100ms 檢查一次
    }

    return 1000;  // 非輪詢模式，每 1 秒檢查一次
}
