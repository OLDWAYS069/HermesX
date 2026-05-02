#include "LighthouseModule.h"
#if HAS_SCREEN
#include "modules/HermesEmUiModule.h"
#endif
#include "MeshService.h"
#include "NodeDB.h"
#include "FSCommon.h"
#include "SPILock.h"
#include "configuration.h"
#include "sleep.h"
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <string>
#include "HermesXLog.h"
#include "mesh/Channels.h"
#include "mesh/HermesPortnums.h"
#include "mesh/generated/meshtastic/mesh.pb.h"
#include "mesh/mesh-pb-constants.h"
#include "mesh/Router.h"
#include "modules/HermesXInterfaceModule.h"
#if !MESHTASTIC_EXCLUDE_GPS
#include "modules/PositionModule.h"
#endif


static const char *bootFile = "/prefs/lighthouse_boot.bin";
static const char *modeFile = "/prefs/lighthouse_mode.bin";
static const char *whitelistFile = "/prefs/lighthouse_whitelist.txt";
static const char *passphraseFile = "/prefs/lighthouse_passphrase.txt";

static const uint32_t POLLING_AWAKE_MS = 300000UL; // 醒來期間
static const uint32_t POLLING_SLEEP_MS = 1800000UL; // 睡覺時間
static const uint32_t EM_SOS_GRACE_MS = 90000UL;
static const uint32_t EM_SOS_INTERVAL_MS = 90000UL;
static const uint32_t POSITION_PULSE_DEDUP_MS = 10000UL;
static const uint32_t POSITION_PULSE_RESULT_TIMEOUT_MS = 12000UL;

LighthouseModule *lighthouseModule = nullptr;

LighthouseModule::LighthouseModule()
    : SinglePortModule("lighthouse", meshtastic_PortNum_TEXT_MESSAGE_APP),
      concurrency::OSThread("lighthouseTask", 300)
{
    lighthouseModule = this;
    loadBoot();
    loadState();
    loadWhitelist();
    loadPassphrase();

    // 僅當 boot file 沒載入成功時才設為現在時間
    if (firstBootMillis == 0) {
        HERMESX_LOG_INFO("first startup or lost bootFile ， building firstBootMillis");
        firstBootMillis = millis();
        saveBoot();
    } else {
        HERMESX_LOG_INFO(" load firstBootMillis = %lu from bootfile", firstBootMillis);
    }
}

void LighthouseModule::markEmergencySafe()
{
    if (emergencySafeAcked) {
        return;
    }
    emergencySafeAcked = true;
    HERMESX_LOG_INFO("Emergency SAFE received, auto SOS disabled");
}

void LighthouseModule::resetEmergencyState(bool restartDevice)
{
    emergencyModeActive = false;
    pollingModeRequested = false;
    firstBootMillis = millis();
    awaitingEmergencyOkAck = false;
    emergencyOkRequestId = 0;
    emergencyActivatedAtMs = 0;
    emergencyLastSosAtMs = 0;
    lastEmergencyActiveId = 0;
    lastEmergencyActiveAtMs = 0;
    emergencySafeAcked = false;
    saveBoot();
    saveState();
    service->setEmergencyTxLock(false);
    restoreConfigSnapshot();
    nodeDB->saveToDisk(SEGMENT_CONFIG);
    saveState();
    roleCorrected = false;

    HERMESX_LOG_INFO("CLOSE LIGHTHOUSE,RESTARING");
    if (restartDevice) {
        delay(15000);
        ESP.restart();
    }
}

void LighthouseModule::captureConfigSnapshot()
{
    if (restoreConfigValid) {
        return;
    }
    restoreRole = config.device.role;
    restoreUsePreset = config.lora.use_preset;
    restoreModemPreset = config.lora.modem_preset;
    restorePowerSaving = config.power.is_power_saving;
    restoreConfigValid = true;
    HERMESX_LOG_INFO("Lighthouse snapshot saved role=%d preset=%d use_preset=%d power_save=%d",
                     static_cast<int>(restoreRole), static_cast<int>(restoreModemPreset), restoreUsePreset ? 1 : 0,
                     restorePowerSaving ? 1 : 0);
}

void LighthouseModule::restoreConfigSnapshot()
{
    if (!restoreConfigValid) {
        HERMESX_LOG_WARN("Lighthouse snapshot missing; skip config restore");
        return;
    }
    config.has_device = true;
    config.device.role = restoreRole;
    config.lora.use_preset = restoreUsePreset;
    config.lora.modem_preset = restoreModemPreset;
    config.power.is_power_saving = restorePowerSaving;
    restoreConfigValid = false;
    HERMESX_LOG_INFO("Lighthouse snapshot restored role=%d preset=%d use_preset=%d power_save=%d",
                     static_cast<int>(config.device.role), static_cast<int>(config.lora.modem_preset),
                     config.lora.use_preset ? 1 : 0, config.power.is_power_saving ? 1 : 0);
}

void LighthouseModule::activateEmergencyLocal()
{
    const uint32_t now = millis();
    const bool wasActive = emergencyModeActive;

    emergencyModeActive = true;
    pollingModeRequested = false;
    emergencyActivatedAtMs = now;
    emergencyLastSosAtMs = 0;
    emergencySafeAcked = false;
    awaitingEmergencyOkAck = false;
    emergencyOkRequestId = 0;
    lastEmergencyActiveId = 0;
    lastEmergencyActiveAtMs = now;

    if (!wasActive) {
        captureConfigSnapshot();
        config.power.is_power_saving = false;
        nodeDB->saveToDisk(SEGMENT_CONFIG);
        saveState();
        HERMESX_LOG_INFO("EmergencyActive local: enter EM mode");
    } else {
        HERMESX_LOG_INFO("EmergencyActive local: refresh grace window");
    }

    HERMESX_LOG_INFO("EmergencyActive local: SAFE grace %lu ms", EM_SOS_GRACE_MS);
#if HAS_SCREEN
    if (hermesXEmUiModule != nullptr) {
        hermesXEmUiModule->enterEmergencyMode(u8"請在90秒內回復");
    }
#endif
    service->setEmergencyTxLock(false);
    broadcastEmergencyActive();
}

int32_t LighthouseModule::getEmergencyGraceRemainingSec() const
{
    if (!emergencyModeActive || emergencySafeAcked) {
        return -1;
    }

    const uint32_t now = millis();
    const uint32_t baseMs = (emergencyLastSosAtMs != 0) ? emergencyLastSosAtMs : emergencyActivatedAtMs;
    if (baseMs == 0) {
        return static_cast<int32_t>((EM_SOS_GRACE_MS + 999) / 1000);
    }

    const uint32_t windowMs = (emergencyLastSosAtMs != 0) ? EM_SOS_INTERVAL_MS : EM_SOS_GRACE_MS;
    int32_t remainingMs = static_cast<int32_t>(windowMs) -
                          static_cast<int32_t>(now - baseMs);
    if (remainingMs < 0) {
        remainingMs = 0;
    }
    return (remainingMs + 999) / 1000;
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
        uint8_t v = 0;
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
    HERMESX_LOG_INFO("EmergencyActive whitelist size=%u", static_cast<unsigned int>(emergencyWhitelist.size()));
    const bool whiteOk = (std::find(emergencyWhitelist.begin(), emergencyWhitelist.end(), from) != emergencyWhitelist.end());
    HERMESX_LOG_INFO("EmergencyActive whitelist %s from=0x%x", whiteOk ? "hit" : "miss", from);
    return whiteOk;
}

void LighthouseModule::loadPassphrase()
{
    emergencyPassphrase[0] = "";
    emergencyPassphrase[1] = "";
#ifdef FSCom
    concurrency::LockGuard g(spiLock);

    auto f = FSCom.open(passphraseFile, FILE_O_READ);
    if (!f) {
        HERMESX_LOG_INFO("lighthouse passphrase missing (%s); passphrase auth disabled", passphraseFile);
        return;
    }

    uint8_t slot = 0;
    while (f.available() && slot < 2) {
        String line = f.readStringUntil('\n');
        line.trim();
        emergencyPassphrase[slot] = line;
        slot++;
    }
    f.close();

    if (emergencyPassphrase[0].length() == 0 && emergencyPassphrase[1].length() == 0) {
        HERMESX_LOG_WARN("lighthouse passphrase file empty; passphrase auth disabled");
        return;
    }

    HERMESX_LOG_INFO("lighthouse passphrase loaded (slotA=%s, slotB=%s)",
                     emergencyPassphrase[0].length() > 0 ? "set" : "empty",
                     emergencyPassphrase[1].length() > 0 ? "set" : "empty");
#else
    HERMESX_LOG_INFO("FSCom not available; passphrase disabled");
#endif
}

void LighthouseModule::savePassphrase()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    FSCom.mkdir("/prefs");
    if (FSCom.exists(passphraseFile)) {
        FSCom.remove(passphraseFile);
    }
    auto f = FSCom.open(passphraseFile, FILE_O_WRITE);
    if (!f) {
        HERMESX_LOG_WARN("lighthouse passphrase save failed (%s)", passphraseFile);
        return;
    }
    f.print(emergencyPassphrase[0]);
    f.print('\n');
    f.print(emergencyPassphrase[1]);
    f.flush();
    f.close();
#endif
}

bool LighthouseModule::setEmergencyPassphraseSlot(uint8_t slot, const String &value)
{
    if (slot >= 2) {
        return false;
    }
    emergencyPassphrase[slot] = value;
#ifdef FSCom
    savePassphrase();
    HERMESX_LOG_INFO("lighthouse passphrase slot %u updated", static_cast<unsigned int>(slot));
    return true;
#else
    HERMESX_LOG_WARN("FSCom not available; passphrase will not persist");
    return true;
#endif
}

String LighthouseModule::getEmergencyPassphrase(uint8_t slot) const
{
    if (slot >= 2) {
        return "";
    }
    return emergencyPassphrase[slot];
}

bool LighthouseModule::isEmergencyActiveAuthorized(const char *txt, NodeNum from) const
{
    return isEmergencyCommandAuthorized(txt, "@EmergencyActive", from, true) ||
           isEmergencyCommandAuthorized(txt, "ACTIVATE: EMAC", from, true);
}

bool LighthouseModule::isEmergencyCommandAuthorized(const char *txt, const char *prefix, NodeNum from, bool allowWhitelist) const
{
    bool passOk = false;
    const bool hasPass = (emergencyPassphrase[0].length() > 0 || emergencyPassphrase[1].length() > 0);
    if (hasPass) {
        const size_t prefixLen = strlen(prefix);
        if (strncmp(txt, prefix, prefixLen) == 0) {
            const char *p = txt + prefixLen;
            while (*p == ' ' || *p == ':' || *p == '=') {
                ++p;
            }
            if (*p != '\0') {
                const String provided(p);
                passOk = (provided == emergencyPassphrase[0]) || (provided == emergencyPassphrase[1]);
            }
        }
    }

    const bool whiteOk = allowWhitelist ? isEmergencyActiveAllowed(from) : false;
    HERMESX_LOG_INFO("EM auth prefix=%s pass=%s white=%s", prefix, passOk ? "ok" : "no", whiteOk ? "ok" : "no");
    return passOk || whiteOk;
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
    uint32_t elapsed = now - firstBootMillis;

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

    if (msg.length() == 0) return;

    meshtastic_MeshPacket *p = allocDataPacket();
    if (!p) return;

    p->to = NODENUM_BROADCAST;
    p->channel = 2;
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
    uint32_t now = millis();
    uint32_t elapsed = now - firstBootMillis;
    
    msg = u8"[HermeS]\n大家好，我是 HermeS Shine1，一台可以遠端控制的無人管理站點\n"
              u8"使用說明：https://www.facebook.com/share/p/1EEThBhZeR/";
  

    if (msg.length() == 0) return;

    meshtastic_MeshPacket *p = allocDataPacket();
    if (!p) return;

    p->to = NODENUM_BROADCAST;
    p->channel = 2;
    p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    p->want_ack = false;
    p->decoded.payload.size = strlen(msg.c_str());
    memcpy(p->decoded.payload.bytes, msg.c_str(), p->decoded.payload.size);

    service->sendToMesh(p, RX_SRC_LOCAL, false);
    LOG_INFO("Broadcast status: %s", msg.c_str());
}

void LighthouseModule::sendEmergencyOk(NodeNum dest)
{
    if (dest == 0) {
        HERMESX_LOG_WARN("Emergency OK skip: destination is 0 (phone)");
        return;
    }

    meshtastic_MeshPacket *p = allocDataPacket();
    if (!p) {
        HERMESX_LOG_WARN("Emergency OK alloc failed");
        return;
    }

    p->to = dest;
    p->channel = channels.getPrimaryIndex();
    p->decoded.portnum = PORTNUM_HERMESX_EMERGENCY;
    p->want_ack = true;

    const char *payload = "STATUS: OK";
    p->decoded.payload.size = strlen(payload);
    memcpy(p->decoded.payload.bytes, payload, p->decoded.payload.size);

    emergencyOkRequestId = p->id;
    awaitingEmergencyOkAck = true;
    HERMESX_LOG_INFO("Emergency OK send to=0x%x req_id=%u", dest, emergencyOkRequestId);
    service->sendToMesh(p, RX_SRC_LOCAL, false);
    HERMESX_LOG_INFO("Emergency OK sent, awaiting ACK id=%u", emergencyOkRequestId);
}

void LighthouseModule::broadcastEmergencyActive()
{
    meshtastic_MeshPacket *p = allocDataPacket();
    if (!p) {
        HERMESX_LOG_WARN("EmergencyActive broadcast alloc failed");
        return;
    }

    p->to = NODENUM_BROADCAST;
    p->channel = channels.getPrimaryIndex();
    p->decoded.portnum = PORTNUM_HERMESX_EMERGENCY;
    p->want_ack = false;

    String payload = "ACTIVATE: EMAC";
    const String &pass = emergencyPassphrase[0].length() > 0 ? emergencyPassphrase[0] : emergencyPassphrase[1];
    if (pass.length() > 0) {
        payload += " ";
        payload += pass;
    }
    p->decoded.payload.size = payload.length();
    memcpy(p->decoded.payload.bytes, payload.c_str(), p->decoded.payload.size);

    service->sendToMesh(p, RX_SRC_LOCAL, false);
    HERMESX_LOG_INFO("EmergencyActive broadcast sent via EM port (local trigger)");
}

void LighthouseModule::sendEmergencySos()
{
    meshtastic_MeshPacket *p = allocDataPacket();
    if (!p) {
        HERMESX_LOG_WARN("Emergency LOST alloc failed");
        return;
    }

    p->to = NODENUM_BROADCAST;
    p->channel = channels.getPrimaryIndex();
    p->decoded.portnum = PORTNUM_HERMESX_EMERGENCY;
    p->want_ack = false;

    const char *payload = "STATUS: LOST";
    p->decoded.payload.size = strlen(payload);
    memcpy(p->decoded.payload.bytes, payload, p->decoded.payload.size);

    service->sendToMesh(p, RX_SRC_LOCAL, false);
    HERMESX_LOG_WARN("Emergency LOST sent");
}

void LighthouseModule::triggerPositionPulse(uint8_t channel)
{
#if !MESHTASTIC_EXCLUDE_GPS
    if (!positionModule) {
        HERMESX_LOG_WARN("Position pulse ignored: positionModule unavailable");
        return;
    }

    positionModule->sendOurPosition(NODENUM_BROADCAST, false, channel);
    HERMESX_LOG_INFO("Position pulse broadcasted on channel=%u", static_cast<unsigned int>(channel));
#else
    HERMESX_LOG_WARN("Position pulse ignored: GPS/position support excluded");
    (void)channel;
#endif
}

bool LighthouseModule::requestPositionPulse()
{
    meshtastic_MeshPacket *p = allocDataPacket();
    if (!p) {
        HERMESX_LOG_WARN("Position pulse request alloc failed");
        return false;
    }

    p->to = NODENUM_BROADCAST;
    p->channel = channels.getPrimaryIndex();
    p->decoded.portnum = PORTNUM_HERMESX_EMERGENCY;
    p->want_ack = false;

    String payload = "REQUEST: POS";
    const String &pass = emergencyPassphrase[0].length() > 0 ? emergencyPassphrase[0] : emergencyPassphrase[1];
    if (pass.length() > 0) {
        payload += " ";
        payload += pass;
    }
    p->decoded.payload.size = payload.length();
    memcpy(p->decoded.payload.bytes, payload.c_str(), p->decoded.payload.size);

    awaitingPositionPulseResult = true;
    collectingPositionPulseResponses = true;
    positionPulseRequestAtMs = millis();
    lastPositionPulseResponder = 0;
    positionPulseUiResult = PositionPulseUiResult::None;
    lastPositionPulseResponders.clear();
    service->sendToMesh(p, RX_SRC_LOCAL, false);
    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->playSendFeedback();
    }
    HERMESX_LOG_INFO("Position pulse request sent via EM port");
    return true;
}

bool LighthouseModule::isPositionPulseAwaitingResult() const
{
    return awaitingPositionPulseResult;
}

LighthouseModule::PositionPulseUiResult LighthouseModule::consumePositionPulseUiResult()
{
    const PositionPulseUiResult result = positionPulseUiResult;
    positionPulseUiResult = PositionPulseUiResult::None;
    return result;
}

void LighthouseModule::cancelPositionPulseRequest(bool clearResponders)
{
    awaitingPositionPulseResult = false;
    collectingPositionPulseResponses = false;
    positionPulseRequestAtMs = 0;
    lastPositionPulseResponder = 0;
    positionPulseUiResult = PositionPulseUiResult::None;
    if (clearResponders) {
        lastPositionPulseResponders.clear();
    }
}

void LighthouseModule::finishPositionPulseRequest(PositionPulseUiResult result, NodeNum responder)
{
    awaitingPositionPulseResult = false;
    lastPositionPulseResponder = responder;
    positionPulseUiResult = result;
    if (result == PositionPulseUiResult::Success) {
        if (HermesXInterfaceModule::instance) {
            HermesXInterfaceModule::instance->playAckSuccess();
        }
        HERMESX_LOG_INFO("Position pulse result success responder=0x%x", responder);
    } else if (result == PositionPulseUiResult::Timeout) {
        collectingPositionPulseResponses = false;
        positionPulseRequestAtMs = 0;
        if (HermesXInterfaceModule::instance) {
            HermesXInterfaceModule::instance->playNackFail();
        }
        HERMESX_LOG_WARN("Position pulse result timeout");
    }
}

bool LighthouseModule::didNodeRespondToLastPositionPulse(NodeNum nodeNum) const
{
    if (nodeNum == 0) {
        return false;
    }
    for (NodeNum responder : lastPositionPulseResponders) {
        if (responder == nodeNum) {
            return true;
        }
    }
    return false;
}


bool LighthouseModule::wantPacket(const meshtastic_MeshPacket *p)
{
    if (awaitingEmergencyOkAck && p->decoded.portnum == meshtastic_PortNum_ROUTING_APP)
        return true;

    if (collectingPositionPulseResponses && p->decoded.portnum == meshtastic_PortNum_POSITION_APP)
        return true;

    if (p->decoded.portnum == PORTNUM_HERMESX_EMERGENCY)
        return true;

    if (p->decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP)
        return false;

    if (p->decoded.payload.size > 0 && p->decoded.payload.bytes[0] == '@')
        return true;

    return false;
}

ProcessMessage LighthouseModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (collectingPositionPulseResponses && mp.decoded.portnum == meshtastic_PortNum_POSITION_APP) {
        const NodeNum ourNode = nodeDB ? nodeDB->getNodeNum() : 0;
        if (mp.from != 0 && mp.from != ourNode) {
            bool knownResponder = false;
            for (NodeNum responder : lastPositionPulseResponders) {
                if (responder == mp.from) {
                    knownResponder = true;
                    break;
                }
            }
            if (!knownResponder) {
                lastPositionPulseResponders.push_back(mp.from);
            }
            if (awaitingPositionPulseResult) {
                finishPositionPulseRequest(PositionPulseUiResult::Success, mp.from);
            }
        }
        return ProcessMessage::CONTINUE;
    }

    if (awaitingEmergencyOkAck && mp.decoded.portnum == meshtastic_PortNum_ROUTING_APP && mp.decoded.request_id != 0 &&
        mp.decoded.request_id == emergencyOkRequestId) {
        meshtastic_Routing decoded = meshtastic_Routing_init_default;
        pb_decode_from_bytes(mp.decoded.payload.bytes, mp.decoded.payload.size, meshtastic_Routing_fields, &decoded);
        if (decoded.error_reason == meshtastic_Routing_Error_NONE) {
            awaitingEmergencyOkAck = false;
            emergencyOkRequestId = 0;
            service->setEmergencyTxLock(true);
            HERMESX_LOG_WARN("Emergency OK ACK received, EM Tx lock enabled");
        } else {
            HERMESX_LOG_WARN("Emergency OK NACK error_reason=%d", decoded.error_reason);
        }
        return ProcessMessage::CONTINUE;
    }

    if (mp.decoded.portnum == PORTNUM_HERMESX_EMERGENCY) {
        char payload[128];
        size_t payloadLen = mp.decoded.payload.size;
        if (payloadLen >= sizeof(payload)) {
            payloadLen = sizeof(payload) - 1;
        }
        memcpy(payload, mp.decoded.payload.bytes, payloadLen);
        payload[payloadLen] = '\0';

        while (payloadLen > 0 && (payload[payloadLen - 1] == '\n' || payload[payloadLen - 1] == '\r' || payload[payloadLen - 1] == ' ')) {
            payload[--payloadLen] = '\0';
        }

        if (strncmp(payload, "ACTIVATE: EMAC", 14) == 0) {
            HERMESX_LOG_INFO("EM activate received from=0x%x text=[%s]", mp.from, payload);
            if (!isEmergencyCommandAuthorized(payload, "ACTIVATE: EMAC", mp.from, true)) {
                HERMESX_LOG_WARN("ignore ACTIVATE: EMAC from 0x%x (not authorized)", mp.from);
                return ProcessMessage::CONTINUE;
            }

            const bool wasActive = emergencyModeActive;
            const uint32_t now = millis();
            if (wasActive && mp.id != 0 && mp.id == lastEmergencyActiveId) {
                HERMESX_LOG_INFO("ACTIVATE: EMAC duplicate id=0x%08x ignored", static_cast<unsigned int>(mp.id));
                return ProcessMessage::CONTINUE;
            }

            lastEmergencyActiveId = mp.id;
            lastEmergencyActiveAtMs = now;
            emergencyModeActive = true;
            pollingModeRequested = false;
            emergencyActivatedAtMs = now;
            emergencyLastSosAtMs = 0;
            emergencySafeAcked = false;
            awaitingEmergencyOkAck = false;
            emergencyOkRequestId = 0;

            if (!wasActive) {
                captureConfigSnapshot();
                config.power.is_power_saving = false;
                nodeDB->saveToDisk(SEGMENT_CONFIG);
                saveState();
                HERMESX_LOG_INFO("LIGHTHOUSE ACTIVE via EM port. EM UI popup without restart.");
            } else {
                HERMESX_LOG_INFO("ACTIVATE: EMAC refresh: reset SAFE grace window");
            }

            HERMESX_LOG_INFO("ACTIVATE: EMAC SAFE grace %lu ms", EM_SOS_GRACE_MS);
#if HAS_SCREEN
            if (!wasActive && hermesXEmUiModule != nullptr) {
                hermesXEmUiModule->enterEmergencyMode(u8"請在90秒內回復");
            }
#endif
            service->setEmergencyTxLock(false);
            if (mp.from == 0) {
                HERMESX_LOG_WARN("ACTIVATE: EMAC from phone, enable EM Tx lock immediately");
                service->setEmergencyTxLock(true);
            } else {
                sendEmergencyOk(mp.from);
            }
            return ProcessMessage::CONTINUE;
        }

        if (strncmp(payload, "RESET: EMAC", 11) == 0) {
            HERMESX_LOG_INFO("EM reset received from=0x%x text=[%s]", mp.from, payload);
            if (!isEmergencyCommandAuthorized(payload, "RESET: EMAC", mp.from, false)) {
                HERMESX_LOG_WARN("ignore RESET: EMAC from 0x%x (bad pass)", mp.from);
                return ProcessMessage::CONTINUE;
            }
            resetEmergencyState(true);
            return ProcessMessage::CONTINUE;
        }

        if (strcmp(payload, "STATUS: LOST") == 0) {
            const bool wasActive = emergencyModeActive;
            const uint32_t now = millis();
            emergencyModeActive = true;
            pollingModeRequested = false;
            emergencyActivatedAtMs = now;
            emergencyLastSosAtMs = now;
            emergencySafeAcked = false;
            awaitingEmergencyOkAck = false;
            emergencyOkRequestId = 0;
            saveState();
            HERMESX_LOG_WARN("Emergency LOST received from=0x%x wasActive=%d", mp.from, wasActive ? 1 : 0);
#if HAS_SCREEN
            if (!wasActive && hermesXEmUiModule != nullptr) {
                hermesXEmUiModule->enterEmergencyMode(u8"隊友失聯");
            }
#endif
            return ProcessMessage::CONTINUE;
        }

        if (strncmp(payload, "REQUEST: POS", 12) == 0) {
            HERMESX_LOG_INFO("Position pulse request received from=0x%x text=[%s]", mp.from, payload);
            if (!isEmergencyCommandAuthorized(payload, "REQUEST: POS", mp.from, true)) {
                HERMESX_LOG_WARN("ignore REQUEST: POS from 0x%x (not authorized)", mp.from);
                return ProcessMessage::CONTINUE;
            }

            const uint32_t now = millis();
            if (mp.id != 0 && mp.id == lastPositionPulseRequestId &&
                static_cast<int32_t>(now - lastPositionPulseAtMs) < static_cast<int32_t>(POSITION_PULSE_DEDUP_MS)) {
                HERMESX_LOG_INFO("REQUEST: POS duplicate id=0x%08x ignored", static_cast<unsigned int>(mp.id));
                return ProcessMessage::CONTINUE;
            }

            lastPositionPulseRequestId = mp.id;
            lastPositionPulseAtMs = now;
            triggerPositionPulse(mp.channel);
            return ProcessMessage::CONTINUE;
        }
    }

    char txt[256];
    size_t len = mp.decoded.payload.size;
    if (len >= sizeof(txt))
        len = sizeof(txt) - 1;
    memcpy(txt, mp.decoded.payload.bytes, len);
    txt[len] = '\0';

    while (len > 0 && (txt[len - 1] == '\n' || txt[len - 1] == '\r' || txt[len - 1] == ' ')) {
        txt[--len] = '\0';
    }

    if (txt[0] != '@') {
        return ProcessMessage::CONTINUE;
    }

    HERMESX_LOG_INFO("[Lighthouse] Received text=[%s] strlen=%d", txt, strlen(txt));


    if (strcmp(txt, "@ResetLighthouse") == 0) {
        resetEmergencyState(true);
        return ProcessMessage::CONTINUE;
    }

    if (strncmp(txt, "@EmergencyActive", 16) == 0) {
        HERMESX_LOG_INFO("EmergencyActive received from=0x%x text=[%s]", mp.from, txt);
        if (!isEmergencyActiveAuthorized(txt, mp.from)) {
            HERMESX_LOG_WARN("ignore @EmergencyActive from 0x%x (not authorized)", mp.from);
            return ProcessMessage::CONTINUE;
        }

        const bool wasActive = emergencyModeActive;
        const uint32_t now = millis();
        if (wasActive && mp.id != 0 && mp.id == lastEmergencyActiveId) {
            HERMESX_LOG_INFO("EmergencyActive duplicate id=0x%08x ignored", static_cast<unsigned int>(mp.id));
            return ProcessMessage::CONTINUE;
        }

        lastEmergencyActiveId = mp.id;
        lastEmergencyActiveAtMs = now;
        emergencyModeActive = true;
        pollingModeRequested = false;
        emergencyActivatedAtMs = now;
        emergencyLastSosAtMs = 0;
        emergencySafeAcked = false;
        awaitingEmergencyOkAck = false;
        emergencyOkRequestId = 0;

        if (!wasActive) {
            captureConfigSnapshot();
            config.power.is_power_saving = false;
            nodeDB->saveToDisk(SEGMENT_CONFIG);
            saveState();

            HERMESX_LOG_INFO("LIGHTHOUSE ACTIVE. EM UI popup without restart.");
        } else {
            HERMESX_LOG_INFO("EmergencyActive refresh: reset SAFE grace window");
        }

        HERMESX_LOG_INFO("EmergencyActive: SAFE grace %lu ms", EM_SOS_GRACE_MS);
#if HAS_SCREEN
        if (!wasActive && hermesXEmUiModule != nullptr) {
            hermesXEmUiModule->enterEmergencyMode(u8"請在90秒內回復");
        }
#endif
        service->setEmergencyTxLock(false);
        if (mp.from == 0) {
            HERMESX_LOG_WARN("EmergencyActive from phone, enable EM Tx lock immediately");
            service->setEmergencyTxLock(true);
        } else {
            sendEmergencyOk(mp.from);
        }

        return ProcessMessage::CONTINUE;
    }

    if (strcmp(txt, "@GoToSleep") == 0) {
        emergencyModeActive = false;
        pollingModeRequested = true;
        firstBootMillis = millis();
        awaitingEmergencyOkAck = false;
        emergencyOkRequestId = 0;
        emergencyActivatedAtMs = 0;
        emergencyLastSosAtMs = 0;
        lastEmergencyActiveId = 0;
        lastEmergencyActiveAtMs = 0;
        emergencySafeAcked = false;
        service->setEmergencyTxLock(false);
        restoreConfigSnapshot();

        saveBoot();
        nodeDB->saveToDisk(SEGMENT_CONFIG);
        saveState();

        delay(15000); // 確保 log 有時間送出
        ESP.restart();

        return ProcessMessage::CONTINUE;
    }

    if (strcmp(txt, "@Status") == 0) {
        broadcastStatusMessage();
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(txt, "@HiHermes") == 0) {
        hihermes = true;
        emergencyModeActive = false;
        pollingModeRequested = false;
        awaitingEmergencyOkAck = false;
        emergencyOkRequestId = 0;
        emergencyActivatedAtMs = 0;
        emergencyLastSosAtMs = 0;
        emergencySafeAcked = false;
        service->setEmergencyTxLock(false);
        IntroduceMessage();
        HERMESX_LOG_INFO("@HiHermes INTRODUCING");

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
    const uint32_t now = millis();

    if (firstTime) {
        firstTime = false;
        awakeStart = millis();
        HERMESX_LOG_INFO("startup status broadcast disabled");
        if (emergencyModeActive) {
            HERMESX_LOG_INFO("boot resume check: previous shutdown left EM active");
#if HAS_SCREEN
            if (hermesXEmUiModule != nullptr && !hermesXEmUiModule->isActive()) {
                hermesXEmUiModule->enterEmergencyMode(u8"EMAC 上次未解除");
            }
#endif
        }
    }

    if (emergencyModeActive && !emergencySafeAcked) {
        if (emergencyActivatedAtMs == 0) {
            emergencyActivatedAtMs = now;
            HERMESX_LOG_INFO("EmergencyActive: SAFE grace %lu ms (resume)", EM_SOS_GRACE_MS);
        }
        if (static_cast<int32_t>(now - emergencyActivatedAtMs) >= static_cast<int32_t>(EM_SOS_GRACE_MS)) {
            if (emergencyLastSosAtMs == 0 ||
                static_cast<int32_t>(now - emergencyLastSosAtMs) >= static_cast<int32_t>(EM_SOS_INTERVAL_MS)) {
                sendEmergencySos();
                emergencyLastSosAtMs = now;
                emergencyActivatedAtMs = now;
                HERMESX_LOG_INFO("EmergencyActive: grace window reset after LOST");
            }
        }
    }

    if (pollingModeRequested && !emergencyModeActive) {
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

    if (collectingPositionPulseResponses && positionPulseRequestAtMs != 0 &&
        static_cast<int32_t>(now - positionPulseRequestAtMs) >= static_cast<int32_t>(POSITION_PULSE_RESULT_TIMEOUT_MS)) {
        if (awaitingPositionPulseResult) {
            finishPositionPulseRequest(PositionPulseUiResult::Timeout);
        } else {
            collectingPositionPulseResponses = false;
            positionPulseRequestAtMs = 0;
        }
    }

    return 1000;  // 非輪詢模式，每 1 秒檢查一次
}
