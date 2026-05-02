#include "HermesEmUiModule.h"

#if HAS_SCREEN

#include "MeshService.h"
#include "HermesXLog.h"
#include "graphics/ScreenFonts.h"
#include "graphics/fonts/HermesX_zh/HermesX_CN12.h"
#include "mesh/HermesPortnums.h"
#include "mesh/mesh-pb-constants.h"
#include "mesh/generated/meshtastic/module_config.pb.h"
#include "mesh/generated/meshtastic/mesh.pb.h"
#include "modules/CannedMessageModule.h"
#include "modules/NodeInfoModule.h"
#include "modules/LighthouseModule.h"
#include "Default.h"
#include "NodeDB.h"
#include "PowerStatus.h"
#include "RTC.h"
#include "gps/GeoCoord.h"
#include "main.h"
#include "modules/RoutingModule.h"
#include "FSCommon.h"
#include "SPILock.h"
#include "pb_encode.h"
#include <OLEDDisplay.h>
#include <OLEDDisplayUi.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>

HermesXEmUiModule *hermesXEmUiModule = nullptr;

namespace {
constexpr uint32_t kAckBannerMs = 2000;
constexpr uint32_t kEmHeaderHeight = 14;
constexpr uint32_t kEmRowHeight = 12;
constexpr uint32_t kEmSendCooldownMs = 1500;
constexpr uint32_t kEmNavArmMs = 5000;
constexpr uint32_t kEmNavMinIntervalMs = 80;
constexpr uint32_t kEmNavFlipGuardMs = 800;
constexpr const char *kEmItems[] = {"TRAPPED", "MEDICAL", "SUPPLIES", "SAFE", "REPORT_STATS",
                                    "DEVICE_STATUS", "EMINFO_TOGGLE", "SET_PASS_A", "SET_PASS_B", "RESET_EMAC"};
constexpr const char *kEmItemsZh[] = {"受困", "醫療", "物資", "安全", "回報統計", "各裝置狀態", "EMINFO廣播", "設定密碼A", "設定密碼B", "EMAC解除"};
constexpr uint8_t kEmActionCount = 4;
constexpr const char *kEmHeaderZh = "人員尋回模式啟動";
constexpr const int16_t kRightColumnWidth = 42;
constexpr float kScreamFreq = 2600.0f;
constexpr uint32_t kScreamToneMs = 600;
constexpr uint32_t kScreamGapMs = 200;
constexpr const char *kEmUiBuzzerFile = "/prefs/hermesx_emui_buzzer.txt";
constexpr const char *kEmInfoEnabledFile = "/prefs/hermesx_eminfo_enabled.txt";
constexpr const char *kEmInfoIntervalFile = "/prefs/hermesx_eminfo_interval.txt";
constexpr const char *kEmHeartbeatIntervalFile = "/prefs/hermesx_emheartbeat_interval.txt";
constexpr const char *kEmOfflineThresholdFile = "/prefs/hermesx_emoffline_threshold.txt";
constexpr const char *kEmBatteryIncludedFile = "/prefs/hermesx_embattery_included.txt";
constexpr uint8_t kEmMenuCount = sizeof(kEmItems) / sizeof(kEmItems[0]);
constexpr uint8_t kEmVisibleRows = 4;
constexpr size_t kPassMaxLen = 20;
constexpr size_t kReportFieldMaxLen = 20;
constexpr uint8_t kDeviceStatusVisibleRows = 4;
constexpr uint8_t kDeviceDetailVisibleRows = 4;
constexpr const char *kReportItemsZh[] = {u8"返回", u8"人", u8"事", u8"時", u8"地", u8"物", u8"傳送"};
constexpr uint8_t kReportItemCount = sizeof(kReportItemsZh) / sizeof(kReportItemsZh[0]);
constexpr uint8_t kReportVisibleRows = 4;

constexpr const char *kKeyRows[][10] = {
    {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
    {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},
    {"A", "S", "D", "F", "G", "H", "J", "K", "L", nullptr},
    {"Z", "X", "C", "V", "B", "N", "M", "DEL", "OK", nullptr},
};
constexpr uint8_t kKeyRowLengths[] = {10, 10, 9, 9};
constexpr uint8_t kKeyRowCount = sizeof(kKeyRowLengths) / sizeof(kKeyRowLengths[0]);
constexpr const char *kPassHeaderA = "設定密碼A";
constexpr const char *kPassHeaderB = "設定密碼B";
constexpr const char *kReportHeader = "五線回報";
constexpr const char *kStatsHeader = "回報統計";
constexpr const char *kStatsLabels[] = {"受困", "醫療需求", "物資需求", "安全"};
constexpr const char *kDeviceStatusHeader = "各裝置狀態";
constexpr const char *kDeviceDetailHeader = "裝置詳情";
constexpr const char *kRelativePositionHeader = "相對位置";
constexpr const char *kEmInfoStateIdle = "IDLE";
constexpr const char *kEmInfoStateTrapped = "TRAPPED";
constexpr const char *kEmInfoStateMedical = "MEDICAL";
constexpr const char *kEmInfoStateSupplies = "SUPPLIES";
constexpr const char *kEmInfoStateSafe = "SAFE";
constexpr uint8_t kEmInfoSignature0 = 0x48; // H
constexpr uint8_t kEmInfoSignature1 = 0x58; // X
constexpr uint8_t kEmInfoVersion = 1;
constexpr uint8_t kEmHeartbeatSignature0 = 0x48; // H
constexpr uint8_t kEmHeartbeatSignature1 = 0x42; // B
constexpr uint8_t kEmHeartbeatVersion = 1;
constexpr uint32_t kDefaultEmHeartbeatIntervalSec = 5;
constexpr uint8_t kDefaultEmOfflineThresholdCount = 3;
constexpr uint32_t kNodeStaleFallbackMs = 15 * 1000;
constexpr int16_t kEmScrollbarWidth = 4;

enum class EmInfoStateCode : uint8_t {
    Idle = 0,
    Trapped = 1,
    Medical = 2,
    Supplies = 3,
    Safe = 4,
};

const char *emInfoStateCodeToZh(const char *code)
{
    if (!code || !*code) {
        return u8"未知";
    }
    if (strcmp(code, kEmInfoStateTrapped) == 0) {
        return u8"受困";
    }
    if (strcmp(code, kEmInfoStateMedical) == 0) {
        return u8"醫療";
    }
    if (strcmp(code, kEmInfoStateSupplies) == 0) {
        return u8"物資";
    }
    if (strcmp(code, kEmInfoStateSafe) == 0) {
        return u8"安全";
    }
    if (strcmp(code, kEmInfoStateIdle) == 0) {
        return u8"待命";
    }
    return code;
}

const char *emInfoStateByteToCode(uint8_t state)
{
    switch (static_cast<EmInfoStateCode>(state)) {
    case EmInfoStateCode::Trapped:
        return kEmInfoStateTrapped;
    case EmInfoStateCode::Medical:
        return kEmInfoStateMedical;
    case EmInfoStateCode::Supplies:
        return kEmInfoStateSupplies;
    case EmInfoStateCode::Safe:
        return kEmInfoStateSafe;
    case EmInfoStateCode::Idle:
    default:
        return kEmInfoStateIdle;
    }
}

template <typename T> T clampValue(T value, T minValue, T maxValue)
{
    return std::max(minValue, std::min(maxValue, value));
}

uint32_t parseUintOrDefault(const String &content, uint32_t fallback)
{
    if (content.length() == 0) {
        return fallback;
    }
    const long parsed = strtol(content.c_str(), nullptr, 10);
    return parsed >= 0 ? static_cast<uint32_t>(parsed) : fallback;
}

void drawWarningIcon(OLEDDisplay *display, int16_t x, int16_t y)
{
    if (!display) {
        return;
    }

    const auto fg = display->getColor();
    const auto inv = (fg == OLEDDISPLAY_COLOR::WHITE) ? OLEDDISPLAY_COLOR::BLACK : OLEDDISPLAY_COLOR::WHITE;
    const int16_t size = 10;
    const int16_t mid = x + (size / 2);
    display->fillTriangle(mid, y, x, y + size - 1, x + size - 1, y + size - 1);

    display->setColor(inv);
    display->fillRect(mid - 1, y + 3, 2, 4);
    display->fillRect(mid - 1, y + size - 2, 2, 2);
    display->setColor(fg);
}

void drawRightColumn(OLEDDisplay *display,
                     int16_t x,
                     int16_t width,
                     int16_t height,
                     const char *countdownText,
                     const char *statusText)
{
    if (!display || width <= 0) {
        return;
    }

    const int16_t topHeight = height / 2;
    const int16_t bottomTop = topHeight;
    const int16_t bottomHeight = height - topHeight;

#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
    display->fillRect(x, 0, width, height);
#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(FONT_MEDIUM);
    display->drawString(x + width / 2, (topHeight - FONT_HEIGHT_MEDIUM) / 2, countdownText);
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    if (statusText) {
        int lineCount = 1;
        for (const char *p = statusText; *p; ++p) {
            if (*p == '\n') {
                ++lineCount;
            }
        }
        const int16_t textHeight = static_cast<int16_t>(lineCount * graphics::HermesX_zh::GLYPH_HEIGHT);
        const int16_t textBoxX = x + 1;
        const int16_t textBoxWidth = width - 2;
        int16_t textX = textBoxX;
        int16_t maxWidth = textBoxWidth;
        const int textWidth = graphics::HermesX_zh::stringAdvance(statusText, graphics::HermesX_zh::GLYPH_WIDTH, display);
        if (textWidth > 0 && textWidth < textBoxWidth) {
            textX = x + (width - textWidth) / 2;
            maxWidth = width - (textX - x);
        }
        int16_t textY = bottomTop + (bottomHeight - textHeight) / 2;
        if (textY < bottomTop) {
            textY = bottomTop;
        }
        graphics::HermesX_zh::drawMixedBounded(*display, textX, textY, maxWidth, statusText,
                                               graphics::HermesX_zh::GLYPH_WIDTH, graphics::HermesX_zh::GLYPH_HEIGHT,
                                               nullptr);
    }
}

void drawSimpleScrollbar(OLEDDisplay *display, int16_t x, int16_t y, int16_t h, int total, int visible, int offset)
{
    if (!display || total <= visible || visible <= 0 || h <= 4) {
        return;
    }

    display->drawRect(x, y, kEmScrollbarWidth, h);
    const int16_t thumbH = std::max<int16_t>(6, static_cast<int16_t>((static_cast<int32_t>(h - 2) * visible) / total));
    const int maxOffset = std::max(1, total - visible);
    const int16_t thumbTravel = std::max<int16_t>(0, h - thumbH - 2);
    const int16_t thumbY = y + 1 + static_cast<int16_t>((static_cast<int32_t>(thumbTravel) * offset) / maxOffset);
    display->fillRect(x + 1, thumbY, kEmScrollbarWidth - 2, thumbH);
}
} // namespace

HermesXEmUiModule::HermesXEmUiModule()
    : SinglePortModule("HermesXEmUi", PORTNUM_HERMESX_EMERGENCY), concurrency::OSThread("HermesEmUi")
{
    hermesXEmUiModule = this;
    if (inputBroker) {
        inputObserver.observe(inputBroker);
    }
    currentEmInfoStateCode = static_cast<uint8_t>(EmInfoStateCode::Idle);
    setIntervalFromNow(setStartDelay());

#ifdef FSCom
    {
        concurrency::LockGuard g(spiLock);
        auto f = FSCom.open(kEmUiBuzzerFile, FILE_O_READ);
        if (f) {
            String content;
            while (f.available()) {
                content += static_cast<char>(f.read());
            }
            f.close();
            content.trim();
            if (content.length() > 0) {
                sirenEnabled = !(content == "0" || content.equalsIgnoreCase("false"));
            }
        }
        auto emInfoFile = FSCom.open(kEmInfoEnabledFile, FILE_O_READ);
        if (emInfoFile) {
            String content;
            while (emInfoFile.available()) {
                content += static_cast<char>(emInfoFile.read());
            }
            emInfoFile.close();
            content.trim();
            if (content.length() > 0) {
                emInfoBroadcastEnabled = !(content == "0" || content.equalsIgnoreCase("false"));
            }
        }
        auto emInfoIntervalFile = FSCom.open(kEmInfoIntervalFile, FILE_O_READ);
        if (emInfoIntervalFile) {
            String content;
            while (emInfoIntervalFile.available()) {
                content += static_cast<char>(emInfoIntervalFile.read());
            }
            emInfoIntervalFile.close();
            content.trim();
            emInfoIntervalSec = clampValue<uint32_t>(parseUintOrDefault(content, 0), 0, 600);
        }
        auto emHeartbeatIntervalFile = FSCom.open(kEmHeartbeatIntervalFile, FILE_O_READ);
        if (emHeartbeatIntervalFile) {
            String content;
            while (emHeartbeatIntervalFile.available()) {
                content += static_cast<char>(emHeartbeatIntervalFile.read());
            }
            emHeartbeatIntervalFile.close();
            content.trim();
            emHeartbeatIntervalSec =
                clampValue<uint32_t>(parseUintOrDefault(content, kDefaultEmHeartbeatIntervalSec), 0, 600);
        }
        auto emOfflineThresholdFile = FSCom.open(kEmOfflineThresholdFile, FILE_O_READ);
        if (emOfflineThresholdFile) {
            String content;
            while (emOfflineThresholdFile.available()) {
                content += static_cast<char>(emOfflineThresholdFile.read());
            }
            emOfflineThresholdFile.close();
            content.trim();
            emOfflineThresholdCount =
                clampValue<uint8_t>(static_cast<uint8_t>(parseUintOrDefault(content, kDefaultEmOfflineThresholdCount)), 1, 9);
        }
        auto emBatteryIncludedFile = FSCom.open(kEmBatteryIncludedFile, FILE_O_READ);
        if (emBatteryIncludedFile) {
            String content;
            while (emBatteryIncludedFile.available()) {
                content += static_cast<char>(emBatteryIncludedFile.read());
            }
            emBatteryIncludedFile.close();
            content.trim();
            if (content.length() > 0) {
                emBatteryIncluded = !(content == "0" || content.equalsIgnoreCase("false"));
            }
        }
    }
#endif
}

void HermesXEmUiModule::sendResetLighthouseNow()
{
    sendResetLighthouse();
}

void HermesXEmUiModule::startScream()
{
    if (!sirenEnabled) {
        HERMESX_LOG_INFO("EM UI siren disabled");
        screamActive = false;
        nextScreamAtMs = 0;
        return;
    }
    screamActive = true;
    nextScreamAtMs = 0;
}

void HermesXEmUiModule::stopScream()
{
    screamActive = false;
    nextScreamAtMs = 0;
    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->stopEmergencySiren();
    }
}

void HermesXEmUiModule::tickScream(uint32_t now)
{
    if (!sirenEnabled) {
        return;
    }
    if (!screamActive) {
        return;
    }
    if (!HermesXInterfaceModule::instance) {
        return;
    }
    if (nextScreamAtMs != 0 && static_cast<int32_t>(now - nextScreamAtMs) < 0) {
        return;
    }

    HermesXInterfaceModule::instance->startEmergencySiren(kScreamFreq, kScreamToneMs);
    nextScreamAtMs = now + kScreamToneMs + kScreamGapMs;
}

void HermesXEmUiModule::setThemeActive(bool enabled)
{
    if (!HermesXInterfaceModule::instance) {
        return;
    }

    if (enabled) {
        if (!themeSaved) {
            savedTheme = HermesXInterfaceModule::instance->getLedTheme();
            themeSaved = true;
        }
        LedTheme emTheme = savedTheme;
        emTheme.colorIdleBreathBase = 0xFF0000;
        emTheme.breathBrightnessMin = 0.2f;
        emTheme.breathBrightnessMax = 1.0f;
        HermesXInterfaceModule::instance->setLedTheme(emTheme);
    } else if (themeSaved) {
        HermesXInterfaceModule::instance->setLedTheme(savedTheme);
        themeSaved = false;
    }
}

void HermesXEmUiModule::enterEmergencyMode(const char *reason)
{
    active = true;
    uiMode = UiMode::Menu;
    selectedIndex = 0;
    listOffset = 0;
    awaitingAck = false;
    lastRequestId = 0;
    lastAckAtMs = 0;
    selectionArmed = false;
    lastNavAtMs = 0;
    lastSendAtMs = 0;
    hasSentOnce = false;
    currentEmInfoStateCode = static_cast<uint8_t>(EmInfoStateCode::Idle);
    lastEmHeartbeatSentMs = 0;
    emHeartbeatSeq = 0;
    deviceStatusOffset = 0;
    passDraft = "";
    keyRow = 0;
    keyCol = 0;
    HERMESX_LOG_INFO("EM UI enter (reason=%s)", reason ? reason : "null");
    if (reason != nullptr) {
        banner = reason;
    } else {
        banner = "";
    }

    if (cannedMessageModule) {
        cannedMessageModule->exitMenu();
    }

    setThemeActive(true);
    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->onEmergencyModeChanged(true);
    }
    startScream();
    // Fire immediately so user hears it without waiting for the next tick.
    tickScream(millis());
    if (emInfoBroadcastEnabled) {
        sendEmInfoNow();
        sendEmHeartbeatNow();
    }
    requestFocus();
    UIFrameEvent e;
    e.action = UIFrameEvent::Action::REGENERATE_FRAMESET;
    notifyObservers(&e);
}

void HermesXEmUiModule::exitEmergencyMode()
{
    if (!active) {
        return;
    }
    HERMESX_LOG_INFO("EM UI exit");
    active = false;
    uiMode = UiMode::Menu;
    awaitingAck = false;
    lastRequestId = 0;
    currentEmInfoStateCode = static_cast<uint8_t>(EmInfoStateCode::Idle);
    lastEmHeartbeatSentMs = 0;
    passDraft = "";
    stopScream();
    setThemeActive(false);
    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->onEmergencyModeChanged(false);
    }
    UIFrameEvent e;
    e.action = UIFrameEvent::Action::REGENERATE_FRAMESET;
    notifyObservers(&e);
}

void HermesXEmUiModule::tickSiren(uint32_t now)
{
    if (!active) {
        return;
    }
    tickScream(now);
}

void HermesXEmUiModule::setSirenEnabled(bool enabled)
{
    sirenEnabled = enabled;
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    FSCom.mkdir("/prefs");
    if (FSCom.exists(kEmUiBuzzerFile)) {
        FSCom.remove(kEmUiBuzzerFile);
    }
    auto f = FSCom.open(kEmUiBuzzerFile, FILE_O_WRITE);
    if (f) {
        f.print(sirenEnabled ? "1" : "0");
        f.flush();
        f.close();
    }
#endif
    if (!sirenEnabled) {
        stopScream();
    } else if (active) {
        startScream();
        tickScream(millis());
    }
}

void HermesXEmUiModule::setEmInfoBroadcastEnabled(bool enabled)
{
    emInfoBroadcastEnabled = enabled;
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    FSCom.mkdir("/prefs");
    if (FSCom.exists(kEmInfoEnabledFile)) {
        FSCom.remove(kEmInfoEnabledFile);
    }
    auto f = FSCom.open(kEmInfoEnabledFile, FILE_O_WRITE);
    if (f) {
        f.print(emInfoBroadcastEnabled ? "1" : "0");
        f.flush();
        f.close();
    }
#endif

    lastAlertMessage = emInfoBroadcastEnabled ? u8"EMINFO 廣播已開啟" : u8"EMINFO 廣播已關閉";
    if (screen) {
        screen->startHermesXAlert(lastAlertMessage.c_str());
    }
    sendLocalTextToPhone(String(u8"[LOCAL] ") + lastAlertMessage);
}

void HermesXEmUiModule::setEmInfoIntervalSec(uint32_t seconds)
{
    emInfoIntervalSec = clampValue<uint32_t>(seconds, 0, 600);
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    FSCom.mkdir("/prefs");
    if (FSCom.exists(kEmInfoIntervalFile)) {
        FSCom.remove(kEmInfoIntervalFile);
    }
    auto f = FSCom.open(kEmInfoIntervalFile, FILE_O_WRITE);
    if (f) {
        f.print(static_cast<unsigned long>(emInfoIntervalSec));
        f.flush();
        f.close();
    }
#endif
}

void HermesXEmUiModule::setEmHeartbeatIntervalSec(uint32_t seconds)
{
    emHeartbeatIntervalSec = clampValue<uint32_t>(seconds, 0, 600);
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    FSCom.mkdir("/prefs");
    if (FSCom.exists(kEmHeartbeatIntervalFile)) {
        FSCom.remove(kEmHeartbeatIntervalFile);
    }
    auto f = FSCom.open(kEmHeartbeatIntervalFile, FILE_O_WRITE);
    if (f) {
        f.print(static_cast<unsigned long>(emHeartbeatIntervalSec));
        f.flush();
        f.close();
    }
#endif
}

void HermesXEmUiModule::setEmOfflineThresholdCount(uint8_t count)
{
    emOfflineThresholdCount = clampValue<uint8_t>(count, 1, 9);
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    FSCom.mkdir("/prefs");
    if (FSCom.exists(kEmOfflineThresholdFile)) {
        FSCom.remove(kEmOfflineThresholdFile);
    }
    auto f = FSCom.open(kEmOfflineThresholdFile, FILE_O_WRITE);
    if (f) {
        f.print(static_cast<unsigned>(emOfflineThresholdCount));
        f.flush();
        f.close();
    }
#endif
}

void HermesXEmUiModule::setEmBatteryIncluded(bool enabled)
{
    emBatteryIncluded = enabled;
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    FSCom.mkdir("/prefs");
    if (FSCom.exists(kEmBatteryIncludedFile)) {
        FSCom.remove(kEmBatteryIncludedFile);
    }
    auto f = FSCom.open(kEmBatteryIncludedFile, FILE_O_WRITE);
    if (f) {
        f.print(emBatteryIncluded ? "1" : "0");
        f.flush();
        f.close();
    }
#endif
}

void HermesXEmUiModule::sendEmergencyAction(EmAction action)
{
    meshtastic_MeshPacket *p = allocDataPacket();
    if (!p) {
        HERMESX_LOG_WARN("EM UI allocDataPacket failed");
        return;
    }

    p->to = NODENUM_BROADCAST;
    p->channel = channels.getPrimaryIndex();
    p->decoded.portnum = PORTNUM_HERMESX_EMERGENCY;

    const char *payload = "";
    bool wantAck = true;
    switch (action) {
    case EmAction::Trapped:
        payload = "STATUS: TRAPPED";
        currentEmInfoStateCode = static_cast<uint8_t>(EmInfoStateCode::Trapped);
        wantAck = true;
        break;
    case EmAction::Medical:
        payload = "NEED: MEDICAL";
        currentEmInfoStateCode = static_cast<uint8_t>(EmInfoStateCode::Medical);
        wantAck = true;
        break;
    case EmAction::Supplies:
        payload = "NEED: SUPPLIES";
        currentEmInfoStateCode = static_cast<uint8_t>(EmInfoStateCode::Supplies);
        wantAck = true;
        break;
    case EmAction::OkHere:
        payload = "STATUS: OK";
        currentEmInfoStateCode = static_cast<uint8_t>(EmInfoStateCode::Safe);
        wantAck = false;
        if (lighthouseModule) {
            lighthouseModule->markEmergencySafe();
        }
        break;
    }

    p->want_ack = wantAck;
    p->decoded.payload.size = strlen(payload);
    memcpy(p->decoded.payload.bytes, payload, p->decoded.payload.size);

    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->playSendFeedback();
    }
    if (!hasSentOnce) {
        hasSentOnce = true;
    }
    stopScream();
    lastAckAtMs = 0;

    awaitingAck = wantAck;
    lastRequestId = p->id;
    HERMESX_LOG_INFO("EM UI send action=%d payload=%s want_ack=%d req_id=%u", static_cast<int>(action), payload, wantAck,
                     lastRequestId);
    service->sendToMesh(p, RX_SRC_LOCAL, false);
    sendEmInfoNow();

    if (action == EmAction::OkHere) {
        exitEmergencyMode();
    }
}

void HermesXEmUiModule::sendFiveLineReport()
{
    meshtastic_MeshPacket *p = allocDataPacket();
    if (!p) {
        HERMESX_LOG_WARN("EM UI allocDataPacket failed (5-line)");
        return;
    }

    p->to = NODENUM_BROADCAST;
    p->channel = channels.getPrimaryIndex();
    p->decoded.portnum = PORTNUM_HERMESX_EMERGENCY;

    const char *basePayload = "";
    bool wantAck = true;
    switch (pendingReportAction) {
    case EmAction::Trapped:
        basePayload = "STATUS: TRAPPED";
        currentEmInfoStateCode = static_cast<uint8_t>(EmInfoStateCode::Trapped);
        break;
    case EmAction::Medical:
        basePayload = "NEED: MEDICAL";
        currentEmInfoStateCode = static_cast<uint8_t>(EmInfoStateCode::Medical);
        break;
    case EmAction::Supplies:
        basePayload = "NEED: SUPPLIES";
        currentEmInfoStateCode = static_cast<uint8_t>(EmInfoStateCode::Supplies);
        break;
    case EmAction::OkHere:
    default:
        basePayload = "STATUS: OK";
        currentEmInfoStateCode = static_cast<uint8_t>(EmInfoStateCode::Safe);
        wantAck = false;
        break;
    }

    String payload = String(basePayload) + "\n" + u8"人:" + owner.short_name + "\n" + u8"事:" + buildReportActionZh() + "\n" +
                     u8"時:" + buildReportTimeCode() + "\n" + u8"地:" + reportPlaceDraft + "\n" + u8"物:" + reportItemDraft;
    if (payload.length() > sizeof(p->decoded.payload.bytes)) {
        payload.remove(sizeof(p->decoded.payload.bytes));
    }

    p->want_ack = wantAck;
    p->decoded.payload.size = payload.length();
    memcpy(p->decoded.payload.bytes, payload.c_str(), p->decoded.payload.size);

    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->playSendFeedback();
    }
    if (!hasSentOnce) {
        hasSentOnce = true;
    }
    stopScream();
    lastAckAtMs = 0;

    awaitingAck = wantAck;
    lastRequestId = p->id;
    HERMESX_LOG_INFO("EM UI send 5-line action=%d want_ack=%d req_id=%u", static_cast<int>(pendingReportAction), wantAck,
                     lastRequestId);
    service->sendToMesh(p, RX_SRC_LOCAL, false);
    sendEmInfoNow();
    exitFiveLineReport();
}

void HermesXEmUiModule::sendResetLighthouse()
{
    meshtastic_MeshPacket *p = allocDataPacket();
    if (!p) {
        HERMESX_LOG_WARN("EM UI allocDataPacket failed (reset)");
        return;
    }

    p->to = NODENUM_BROADCAST;
    p->channel = channels.getPrimaryIndex();
    p->decoded.portnum = PORTNUM_HERMESX_EMERGENCY;

    String payload = "RESET: EMAC";
    if (lighthouseModule != nullptr) {
        const String &pass = lighthouseModule->getEmergencyPassphrase(0).length() > 0 ? lighthouseModule->getEmergencyPassphrase(0)
                                                                                       : lighthouseModule->getEmergencyPassphrase(1);
        if (pass.length() > 0) {
            payload += " ";
            payload += pass;
        }
    }
    p->want_ack = false;
    p->decoded.payload.size = payload.length();
    memcpy(p->decoded.payload.bytes, payload.c_str(), p->decoded.payload.size);

    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->playSendFeedback();
    }
    if (!hasSentOnce) {
        hasSentOnce = true;
    }
    stopScream();
    lastAckAtMs = 0;

    HERMESX_LOG_INFO("EM UI send reset payload=%s via EM port", payload.c_str());
    service->sendToMesh(p, RX_SRC_LOCAL, false);
    if (lighthouseModule) {
        lighthouseModule->resetEmergencyState(false);
    }
    exitEmergencyMode();
}

void HermesXEmUiModule::updateListOffset()
{
    if (kEmMenuCount <= kEmVisibleRows) {
        listOffset = 0;
        return;
    }
    if (selectedIndex < listOffset) {
        listOffset = selectedIndex;
    } else if (selectedIndex >= listOffset + kEmVisibleRows) {
        listOffset = selectedIndex - kEmVisibleRows + 1;
    }
    const int maxOffset = static_cast<int>(kEmMenuCount) - static_cast<int>(kEmVisibleRows);
    if (listOffset > maxOffset) {
        listOffset = maxOffset;
    }
    if (listOffset < 0) {
        listOffset = 0;
    }
}

void HermesXEmUiModule::enterPassphraseEdit(uint8_t slot)
{
    uiMode = UiMode::PassphraseEdit;
    editingPassSlot = slot;
    passDraft = "";
    if (lighthouseModule) {
        passDraft = lighthouseModule->getEmergencyPassphrase(slot);
    }
    if (passDraft.length() > kPassMaxLen) {
        passDraft = passDraft.substring(0, kPassMaxLen);
    }
    keyRow = 0;
    keyCol = 0;
    selectionArmed = false;
    lastNavAtMs = 0;
    requestFocus();
    UIFrameEvent e;
    e.action = UIFrameEvent::Action::REDRAW_ONLY;
    notifyObservers(&e);
}

void HermesXEmUiModule::enterFiveLineReport(EmAction action)
{
    pendingReportAction = action;
    reportSelectedIndex = 0;
    reportListOffset = 0;
    selectionArmed = false;
    lastNavAtMs = 0;
    lastNavDir = 0;
    if (reportPlaceDraft.length() > kReportFieldMaxLen) {
        reportPlaceDraft = reportPlaceDraft.substring(0, kReportFieldMaxLen);
    }
    if (reportItemDraft.length() > kReportFieldMaxLen) {
        reportItemDraft = reportItemDraft.substring(0, kReportFieldMaxLen);
    }
    uiMode = UiMode::FiveLineReport;
    requestFocus();
    UIFrameEvent e;
    e.action = UIFrameEvent::Action::REDRAW_ONLY;
    notifyObservers(&e);
}

void HermesXEmUiModule::exitFiveLineReport()
{
    uiMode = UiMode::Menu;
    selectionArmed = false;
    lastNavAtMs = 0;
    lastNavDir = 0;
    requestFocus();
    UIFrameEvent e;
    e.action = UIFrameEvent::Action::REDRAW_ONLY;
    notifyObservers(&e);
}

void HermesXEmUiModule::enterReportTextEdit(ReportEditField field)
{
    reportEditField = field;
    textEditContext = (field == ReportEditField::Place) ? TextEditContext::ReportPlace : TextEditContext::ReportItem;
    textEditDraft = (field == ReportEditField::Place) ? reportPlaceDraft : reportItemDraft;
    if (textEditDraft.length() > kReportFieldMaxLen) {
        textEditDraft = textEditDraft.substring(0, kReportFieldMaxLen);
    }
    keyRow = 0;
    keyCol = 0;
    selectionArmed = false;
    lastNavAtMs = 0;
    lastNavDir = 0;
    uiMode = UiMode::TextEdit;
    requestFocus();
    UIFrameEvent e;
    e.action = UIFrameEvent::Action::REDRAW_ONLY;
    notifyObservers(&e);
}

void HermesXEmUiModule::exitTextEdit(bool save)
{
    if (save) {
        if (textEditContext == TextEditContext::ReportPlace) {
            reportPlaceDraft = textEditDraft;
        } else if (textEditContext == TextEditContext::ReportItem) {
            reportItemDraft = textEditDraft;
        }
    }

    reportEditField = ReportEditField::None;
    textEditContext = TextEditContext::None;
    uiMode = UiMode::FiveLineReport;
    requestFocus();
    UIFrameEvent e;
    e.action = UIFrameEvent::Action::REDRAW_ONLY;
    notifyObservers(&e);
}

void HermesXEmUiModule::exitPassphraseEdit(bool save)
{
    if (save && lighthouseModule) {
        const bool ok = lighthouseModule->setEmergencyPassphraseSlot(editingPassSlot, passDraft);
        notifyPassphraseSaved(editingPassSlot, passDraft, ok);
    }
    uiMode = UiMode::Menu;
    selectionArmed = false;
    lastNavAtMs = 0;
    requestFocus();
    UIFrameEvent e;
    e.action = UIFrameEvent::Action::REDRAW_ONLY;
    notifyObservers(&e);
}

void HermesXEmUiModule::sendLocalTextToPhone(const String &text)
{
    if (!service || text.length() == 0) {
        return;
    }
    meshtastic_MeshPacket *p = allocDataPacket();
    if (!p) {
        return;
    }
    p->to = NODENUM_BROADCAST;
    p->channel = channels.getPrimaryIndex();
    p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    p->want_ack = false;
    p->decoded.payload.size = text.length();
    memcpy(p->decoded.payload.bytes, text.c_str(), p->decoded.payload.size);
    service->sendToPhone(p);
}

void HermesXEmUiModule::notifyPassphraseSaved(uint8_t slot, const String &value, bool ok)
{
    const char slotChar = slot == 0 ? 'A' : 'B';
    if (ok) {
        lastAlertMessage = String(u8"EM 密碼") + slotChar + u8" 已設定: " + value;
    } else {
        lastAlertMessage = String(u8"EM 密碼") + slotChar + u8" 設定失敗";
    }
    if (screen) {
        screen->startHermesXAlert(lastAlertMessage.c_str());
    }
    sendLocalTextToPhone(String(u8"[LOCAL] ") + lastAlertMessage);
}

void HermesXEmUiModule::recordEmergencyReportPayload(const meshtastic_MeshPacket &mp)
{
    if (isFromUs(&mp)) {
        return;
    }

    char payload[96];
    size_t len = mp.decoded.payload.size;
    if (len >= sizeof(payload)) {
        len = sizeof(payload) - 1;
    }
    memcpy(payload, mp.decoded.payload.bytes, len);
    payload[len] = '\0';

    while (len > 0 && (payload[len - 1] == '\n' || payload[len - 1] == '\r' || payload[len - 1] == ' ')) {
        payload[--len] = '\0';
    }

    const char *stateZh = nullptr;
    if (strncmp(payload, "STATUS: TRAPPED", strlen("STATUS: TRAPPED")) == 0) {
        if (reportStats.trapped < UINT16_MAX) {
            ++reportStats.trapped;
        }
        stateZh = u8"受困";
    } else if (strncmp(payload, "NEED: MEDICAL", strlen("NEED: MEDICAL")) == 0) {
        if (reportStats.medical < UINT16_MAX) {
            ++reportStats.medical;
        }
        stateZh = u8"醫療";
    } else if (strncmp(payload, "NEED: SUPPLIES", strlen("NEED: SUPPLIES")) == 0) {
        if (reportStats.supplies < UINT16_MAX) {
            ++reportStats.supplies;
        }
        stateZh = u8"物資";
    } else if (strncmp(payload, "STATUS: OK", strlen("STATUS: OK")) == 0) {
        if (reportStats.safe < UINT16_MAX) {
            ++reportStats.safe;
        }
        stateZh = u8"安全";
    } else {
        return;
    }

    EmInfoNodeStatus *entry = findOrCreateNodeStatus(getFrom(&mp));
    if (entry) {
        const meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(getFrom(&mp));
        if (entry->shortName[0] == '\0') {
            if (node && node->user.short_name[0] != '\0') {
                snprintf(entry->shortName, sizeof(entry->shortName), "%s", node->user.short_name);
            }
        }
        if (node && nodeDB->hasValidPosition(node)) {
            entry->latitudeI = node->position.latitude_i;
            entry->longitudeI = node->position.longitude_i;
            entry->altitude = node->position.altitude;
        }
        snprintf(entry->state, sizeof(entry->state), "%s", stateZh);
        entry->lastSeenMs = millis();
        entry->valid = true;

        const char *line = strstr(payload, "\n");
        while (line) {
            ++line;
            if (strncmp(line, u8"人:", strlen(u8"人:")) == 0) {
                line += strlen(u8"人:");
                const char *end = strchr(line, '\n');
                const size_t copyLen = end ? static_cast<size_t>(end - line) : strlen(line);
                if (copyLen > 0) {
                    snprintf(entry->shortName, sizeof(entry->shortName), "%.*s", static_cast<int>(copyLen), line);
                }
            } else if (strncmp(line, u8"時:", strlen(u8"時:")) == 0) {
                line += strlen(u8"時:");
                const char *end = strchr(line, '\n');
                const size_t copyLen = end ? static_cast<size_t>(end - line) : strlen(line);
                snprintf(entry->timeCode, sizeof(entry->timeCode), "%.*s", static_cast<int>(copyLen), line);
            } else if (strncmp(line, u8"地:", strlen(u8"地:")) == 0) {
                line += strlen(u8"地:");
                const char *end = strchr(line, '\n');
                const size_t copyLen = end ? static_cast<size_t>(end - line) : strlen(line);
                snprintf(entry->place, sizeof(entry->place), "%.*s", static_cast<int>(copyLen), line);
            } else if (strncmp(line, u8"物:", strlen(u8"物:")) == 0) {
                line += strlen(u8"物:");
                const char *end = strchr(line, '\n');
                const size_t copyLen = end ? static_cast<size_t>(end - line) : strlen(line);
                snprintf(entry->item, sizeof(entry->item), "%.*s", static_cast<int>(copyLen), line);
            }
            line = strchr(line, '\n');
        }
    }

    HERMESX_LOG_INFO("EM UI stats trapped=%u medical=%u supplies=%u safe=%u", static_cast<unsigned>(reportStats.trapped),
                     static_cast<unsigned>(reportStats.medical), static_cast<unsigned>(reportStats.supplies),
                     static_cast<unsigned>(reportStats.safe));
}

HermesXEmUiModule::EmInfoNodeStatus *HermesXEmUiModule::findOrCreateNodeStatus(NodeNum nodeNum)
{
    int existingIndex = -1;
    for (int i = 0; i < kMaxEmInfoNodes; ++i) {
        if (emInfoNodes[i].valid && emInfoNodes[i].nodeNum == nodeNum) {
            existingIndex = i;
            break;
        }
    }

    if (existingIndex > 0) {
        EmInfoNodeStatus temp = emInfoNodes[existingIndex];
        for (int i = existingIndex; i > 0; --i) {
            emInfoNodes[i] = emInfoNodes[i - 1];
        }
        emInfoNodes[0] = temp;
        return &emInfoNodes[0];
    }
    if (existingIndex == 0) {
        return &emInfoNodes[0];
    }

    for (int i = kMaxEmInfoNodes - 1; i > 0; --i) {
        emInfoNodes[i] = emInfoNodes[i - 1];
    }
    emInfoNodes[0] = EmInfoNodeStatus{};
    emInfoNodes[0].nodeNum = nodeNum;
    emInfoNodes[0].valid = true;
    return &emInfoNodes[0];
}

void HermesXEmUiModule::recordEmInfoPayload(const meshtastic_MeshPacket &mp)
{
    if (isFromUs(&mp)) {
        return;
    }

    if (mp.decoded.payload.size < 5) {
        return;
    }

    const uint8_t *bytes = mp.decoded.payload.bytes;
    if (bytes[0] != kEmInfoSignature0 || bytes[1] != kEmInfoSignature1 || bytes[2] != kEmInfoVersion) {
        return;
    }

    const uint8_t stateByte = bytes[3];
    const uint8_t batteryPercent = bytes[4];
    char shortName[16] = {0};
    const size_t shortNameLen = std::min<size_t>(sizeof(shortName) - 1, mp.decoded.payload.size - 5);
    if (shortNameLen > 0) {
        memcpy(shortName, bytes + 5, shortNameLen);
        shortName[shortNameLen] = '\0';
    }

    EmInfoNodeStatus *entry = findOrCreateNodeStatus(getFrom(&mp));
    if (!entry) {
        return;
    }
    const meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(getFrom(&mp));
    if (node && nodeDB->hasValidPosition(node)) {
        entry->latitudeI = node->position.latitude_i;
        entry->longitudeI = node->position.longitude_i;
        entry->altitude = node->position.altitude;
    }
    snprintf(entry->shortName, sizeof(entry->shortName), "%s", shortName);
    snprintf(entry->state, sizeof(entry->state), "%s", emInfoStateCodeToZh(emInfoStateByteToCode(stateByte)));
    entry->batteryPercent = batteryPercent;
    entry->lastSeenMs = millis();
    entry->valid = true;
}

uint32_t HermesXEmUiModule::getEmInfoIntervalMs() const
{
    if (emInfoIntervalSec > 0) {
        return emInfoIntervalSec * 1000UL;
    }
    return Default::getConfiguredOrDefaultMs(config.device.node_info_broadcast_secs, default_node_info_broadcast_secs);
}

void HermesXEmUiModule::recordEmHeartbeatPayload(const meshtastic_MeshPacket &mp)
{
    if (isFromUs(&mp) || mp.decoded.payload.size < 9) {
        return;
    }

    const uint8_t *bytes = mp.decoded.payload.bytes;
    if (bytes[0] != kEmHeartbeatSignature0 || bytes[1] != kEmHeartbeatSignature1 || bytes[2] != kEmHeartbeatVersion) {
        return;
    }

    const NodeNum nodeNum = getFrom(&mp);
    const uint32_t now = millis();
    const bool activeFlag = (bytes[3] & 0x01) != 0;
    const uint8_t seq = bytes[4];
    uint32_t remoteTimestamp = static_cast<uint32_t>(bytes[5]) | (static_cast<uint32_t>(bytes[6]) << 8) |
                               (static_cast<uint32_t>(bytes[7]) << 16) | (static_cast<uint32_t>(bytes[8]) << 24);
    const bool hasBattery = mp.decoded.payload.size >= 10;
    const uint8_t batteryPercent = hasBattery ? bytes[9] : 0;

    for (int i = 0; i < kMaxEmInfoNodes; ++i) {
        if (!emInfoNodes[i].valid || emInfoNodes[i].nodeNum != nodeNum) {
            continue;
        }
        emInfoNodes[i].lastHeartbeatMs = now;
        emInfoNodes[i].remoteTimestamp = remoteTimestamp;
        emInfoNodes[i].heartbeatSeq = seq;
        emInfoNodes[i].heartbeatActive = activeFlag;
        if (hasBattery) {
            emInfoNodes[i].batteryPercent = batteryPercent;
        }
        return;
    }
}

void HermesXEmUiModule::sendEmInfoNow()
{
    if (!active || !emInfoBroadcastEnabled) {
        return;
    }

    meshtastic_MeshPacket *p = allocDataPacket();
    if (!p) {
        return;
    }

    p->to = NODENUM_BROADCAST;
    p->channel = channels.getPrimaryIndex();
    p->want_ack = false;

    char shortName[16];
    snprintf(shortName, sizeof(shortName), "%s", owner.short_name);
    const uint8_t batteryPercent = emBatteryIncluded && powerStatus ? powerStatus->getBatteryChargePercent() : 0;

    const size_t shortNameLen = strnlen(shortName, sizeof(shortName));
    p->decoded.payload.size = static_cast<uint16_t>(5 + shortNameLen);
    p->decoded.payload.bytes[0] = kEmInfoSignature0;
    p->decoded.payload.bytes[1] = kEmInfoSignature1;
    p->decoded.payload.bytes[2] = kEmInfoVersion;
    p->decoded.payload.bytes[3] = currentEmInfoStateCode;
    p->decoded.payload.bytes[4] = batteryPercent;
    if (shortNameLen > 0) {
        memcpy(p->decoded.payload.bytes + 5, shortName, shortNameLen);
    }
    service->sendToMesh(p, RX_SRC_LOCAL, false);
    lastEmInfoSentMs = millis();
    HERMESX_LOG_INFO("EMINFO broadcast state=%s battery=%u short=%s", emInfoStateByteToCode(currentEmInfoStateCode),
                     static_cast<unsigned>(batteryPercent), shortName);
}

void HermesXEmUiModule::sendEmHeartbeatNow()
{
    if (!active) {
        return;
    }
    const uint32_t intervalMs = getEmHeartbeatIntervalMs();
    if (intervalMs == 0) {
        return;
    }

    meshtastic_MeshPacket *p = allocDataPacket();
    if (!p) {
        return;
    }

    p->to = NODENUM_BROADCAST;
    p->channel = channels.getPrimaryIndex();
    p->want_ack = false;
    p->decoded.payload.bytes[0] = kEmHeartbeatSignature0;
    p->decoded.payload.bytes[1] = kEmHeartbeatSignature1;
    p->decoded.payload.bytes[2] = kEmHeartbeatVersion;
    p->decoded.payload.bytes[3] = active ? 0x01 : 0x00;
    p->decoded.payload.bytes[4] = ++emHeartbeatSeq;
    const uint32_t nowSec = static_cast<uint32_t>(getValidTime(RTCQualityFromNet));
    p->decoded.payload.bytes[5] = static_cast<uint8_t>(nowSec & 0xFF);
    p->decoded.payload.bytes[6] = static_cast<uint8_t>((nowSec >> 8) & 0xFF);
    p->decoded.payload.bytes[7] = static_cast<uint8_t>((nowSec >> 16) & 0xFF);
    p->decoded.payload.bytes[8] = static_cast<uint8_t>((nowSec >> 24) & 0xFF);
    uint16_t payloadSize = 9;
    if (emBatteryIncluded) {
        p->decoded.payload.bytes[payloadSize++] = powerStatus ? powerStatus->getBatteryChargePercent() : 0;
    }
    p->decoded.payload.size = payloadSize;
    service->sendToMesh(p, RX_SRC_LOCAL, false);
    lastEmHeartbeatSentMs = millis();
    HERMESX_LOG_INFO("EMHB broadcast seq=%u active=%d", static_cast<unsigned>(emHeartbeatSeq), active ? 1 : 0);
}

uint32_t HermesXEmUiModule::getEmHeartbeatIntervalMs() const
{
    return emHeartbeatIntervalSec * 1000UL;
}

int HermesXEmUiModule::getVisibleEmInfoNodeCount() const
{
    int count = 0;
    for (int i = 0; i < kMaxEmInfoNodes; ++i) {
        if (emInfoNodes[i].valid) {
            ++count;
        }
    }
    return count;
}

const HermesXEmUiModule::EmInfoNodeStatus *HermesXEmUiModule::getVisibleEmInfoNodeByIndex(int index) const
{
    if (index < 0) {
        return nullptr;
    }
    int current = 0;
    for (int i = 0; i < kMaxEmInfoNodes; ++i) {
        if (!emInfoNodes[i].valid) {
            continue;
        }
        if (current == index) {
            return &emInfoNodes[i];
        }
        ++current;
    }
    return nullptr;
}

const char *HermesXEmUiModule::getNodePresenceLabel(const EmInfoNodeStatus &entry) const
{
    const uint32_t intervalMs = getEmHeartbeatIntervalMs();
    const uint32_t now = millis();
    if (entry.lastHeartbeatMs == 0 || intervalMs == 0) {
        if (entry.lastSeenMs == 0) {
            return u8"未知";
        }
        return (now - entry.lastSeenMs) <= kNodeStaleFallbackMs ? u8"在線" : u8"逾時";
    }

    const uint32_t elapsedMs = now - entry.lastHeartbeatMs;
    if (elapsedMs <= intervalMs) {
        return u8"在線";
    }
    if (elapsedMs <= intervalMs * emOfflineThresholdCount) {
        return u8"延遲";
    }
    return u8"離線";
}

String HermesXEmUiModule::getNodeTimeLabel(const EmInfoNodeStatus &entry) const
{
    if (entry.timeCode[0] != '\0') {
        return String(entry.timeCode);
    }

    uint32_t refMs = entry.lastHeartbeatMs;
    if (refMs == 0) {
        refMs = entry.lastSeenMs;
    }
    if (refMs == 0) {
        return u8"--";
    }

    const uint32_t elapsedSec = (millis() - refMs) / 1000UL;
    char buf[20];
    if (elapsedSec < 60) {
        snprintf(buf, sizeof(buf), "%lus前", static_cast<unsigned long>(elapsedSec));
    } else if (elapsedSec < 3600) {
        snprintf(buf, sizeof(buf), "%lum前", static_cast<unsigned long>(elapsedSec / 60UL));
    } else {
        snprintf(buf, sizeof(buf), "%luh前", static_cast<unsigned long>(elapsedSec / 3600UL));
    }
    return String(buf);
}

String HermesXEmUiModule::getNodeRelativeHeardLabel(const EmInfoNodeStatus &entry) const
{
    uint32_t refMs = entry.lastHeartbeatMs;
    if (refMs == 0) {
        refMs = entry.lastSeenMs;
    }
    if (refMs == 0) {
        return u8"--";
    }

    const uint32_t elapsedSec = (millis() - refMs) / 1000UL;
    char buf[20];
    if (elapsedSec < 60) {
        snprintf(buf, sizeof(buf), "%lus前", static_cast<unsigned long>(elapsedSec));
    } else if (elapsedSec < 3600) {
        snprintf(buf, sizeof(buf), "%lum前", static_cast<unsigned long>(elapsedSec / 60UL));
    } else {
        snprintf(buf, sizeof(buf), "%luh前", static_cast<unsigned long>(elapsedSec / 3600UL));
    }
    return String(buf);
}

const HermesXEmUiModule::EmInfoNodeStatus *HermesXEmUiModule::getSelectedEmInfoNode() const
{
    return getVisibleEmInfoNodeByIndex(deviceStatusSelectedIndex);
}

String HermesXEmUiModule::getNodeLatitudeText(const EmInfoNodeStatus &entry) const
{
    if (entry.latitudeI == 0 && entry.longitudeI == 0) {
        return "";
    }
    char buf[24];
    snprintf(buf, sizeof(buf), "%.7f", static_cast<double>(entry.latitudeI) / 1e7);
    return String(buf);
}

String HermesXEmUiModule::getNodeLongitudeText(const EmInfoNodeStatus &entry) const
{
    if (entry.latitudeI == 0 && entry.longitudeI == 0) {
        return "";
    }
    char buf[24];
    snprintf(buf, sizeof(buf), "%.7f", static_cast<double>(entry.longitudeI) / 1e7);
    return String(buf);
}

String HermesXEmUiModule::getNodeAltitudeText(const EmInfoNodeStatus &entry) const
{
    if (entry.latitudeI == 0 && entry.longitudeI == 0) {
        return "";
    }
    char buf[24];
    snprintf(buf, sizeof(buf), "%ldm", static_cast<long>(entry.altitude));
    return String(buf);
}

int HermesXEmUiModule::buildDeviceDetailLines(const EmInfoNodeStatus &entry, String *lines, int maxLines) const
{
    if (!lines || maxLines <= 0) {
        return 0;
    }
    int count = 0;
    auto pushLine = [&](const String &line) {
        if (count < maxLines) {
            lines[count++] = line;
        }
    };

    pushLine(String(u8"返回"));
    pushLine(String(u8"人: ") + (entry.shortName[0] ? entry.shortName : "----"));
    pushLine(String(u8"事: ") + (entry.state[0] ? entry.state : u8"未知"));
    String timeText = entry.timeCode[0] ? String(entry.timeCode) : String("--");
    timeText += " (";
    timeText += getNodeRelativeHeardLabel(entry);
    timeText += ")";
    pushLine(String(u8"時: ") + timeText);
    pushLine(String(u8"地: ") + (entry.place[0] ? entry.place : ""));
    pushLine(String(u8"物: ") + (entry.item[0] ? entry.item : ""));
    pushLine(String(u8"經度: ") + getNodeLongitudeText(entry));
    pushLine(String(u8"緯度: ") + getNodeLatitudeText(entry));
    pushLine(String(u8"高度: ") + getNodeAltitudeText(entry));
    pushLine(String(u8"相對位置"));
    return count;
}

int HermesXEmUiModule::buildRelativePositionLines(const EmInfoNodeStatus &entry, String *lines, int maxLines) const
{
    if (!lines || maxLines <= 0) {
        return 0;
    }
    int count = 0;
    auto pushLine = [&](const String &line) {
        if (count < maxLines) {
            lines[count++] = line;
        }
    };

    pushLine(String(u8"返回"));
    if ((entry.latitudeI == 0 && entry.longitudeI == 0) || (localPosition.latitude_i == 0 && localPosition.longitude_i == 0)) {
        pushLine(String(u8"方位: "));
        pushLine(String(u8"距離: "));
        pushLine(String(u8"角度: "));
        return count;
    }

    const double myLat = static_cast<double>(localPosition.latitude_i) / 1e7;
    const double myLon = static_cast<double>(localPosition.longitude_i) / 1e7;
    const double dstLat = static_cast<double>(entry.latitudeI) / 1e7;
    const double dstLon = static_cast<double>(entry.longitudeI) / 1e7;
    const float distanceM = GeoCoord::latLongToMeter(myLat, myLon, dstLat, dstLon);
    const float bearingDeg = GeoCoord::bearing(myLat, myLon, dstLat, dstLon);
    const unsigned int bearingRounded = static_cast<unsigned int>(bearingDeg < 0 ? bearingDeg + 360.0f : bearingDeg);
    const char *bearingText = GeoCoord::degreesToBearing(bearingRounded);

    char distanceBuf[24];
    if (distanceM >= 1000.0f) {
        snprintf(distanceBuf, sizeof(distanceBuf), "%.1f km", distanceM / 1000.0f);
    } else {
        snprintf(distanceBuf, sizeof(distanceBuf), "%.0f m", distanceM);
    }
    char angleBuf[24];
    snprintf(angleBuf, sizeof(angleBuf), "%u°", bearingRounded % 360U);

    pushLine(String(u8"方位: ") + (bearingText ? bearingText : ""));
    pushLine(String(u8"距離: ") + distanceBuf);
    pushLine(String(u8"角度: ") + angleBuf);
    return count;
}

void HermesXEmUiModule::updateDeviceStatusOffset()
{
    const int total = getVisibleEmInfoNodeCount();
    if (total <= static_cast<int>(kDeviceStatusVisibleRows)) {
        deviceStatusOffset = 0;
        return;
    }
    if (deviceStatusSelectedIndex < deviceStatusOffset) {
        deviceStatusOffset = deviceStatusSelectedIndex;
    } else if (deviceStatusSelectedIndex >= deviceStatusOffset + kDeviceStatusVisibleRows) {
        deviceStatusOffset = deviceStatusSelectedIndex - kDeviceStatusVisibleRows + 1;
    }
    const int maxOffset = total - static_cast<int>(kDeviceStatusVisibleRows);
    deviceStatusOffset = clampValue<int>(deviceStatusOffset, 0, maxOffset);
}

int HermesXEmUiModule::handleInputEvent(const InputEvent *event)
{
    if (!active) {
        return 0;
    }

    if (event && event->source) {
        HERMESX_LOG_DEBUG("EM UI input src=%s event=%d", event->source, event->inputEvent);
    }

    if (uiMode == UiMode::PassphraseEdit) {
        return handlePassphraseInput(event);
    }
    if (uiMode == UiMode::TextEdit) {
        return handleTextEditInput(event);
    }
    if (uiMode == UiMode::FiveLineReport) {
        return handleFiveLineReportInput(event);
    }
    if (uiMode == UiMode::Stats) {
        return handleStatsInput(event);
    }
    if (uiMode == UiMode::DeviceStatus) {
        return handleDeviceStatusInput(event);
    }
    if (uiMode == UiMode::DeviceDetail || uiMode == UiMode::RelativePosition) {
        return handleDeviceStatusInput(event);
    }
    return handleMenuInput(event);
}

int HermesXEmUiModule::handleMenuInput(const InputEvent *event)
{
    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);

    const bool isUp = event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
    const bool isDown =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
    const bool isLeft =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT);
    const bool isRight =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT);
    const bool isSelect =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const bool isCancel =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL) ||
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK);

    const bool isCw = (event->inputEvent == eventCw);
    const bool isCcw = (event->inputEvent == eventCcw);
    const bool isPress = (event->inputEvent == eventPress);

    const uint32_t now = millis();
    int8_t navDir = 0;
    const bool isRotary = (event && event->source && strncmp(event->source, "rotEnc", 6) == 0);
    if (isRotary) {
        if (isCcw) {
            navDir = -1;
        } else if (isCw) {
            navDir = 1;
        } else if (eventCw == 0 && eventCcw == 0) {
            if (isUp || isLeft) {
                navDir = -1;
            } else if (isDown || isRight) {
                navDir = 1;
            }
        }
    } else {
        if (isUp || isLeft || isCcw) {
            navDir = -1;
        } else if (isDown || isRight || isCw) {
            navDir = 1;
        }
    }

    if (navDir != 0) {
        if (!isRotary) {
            if (lastNavAtMs != 0 && (now - lastNavAtMs) < kEmNavMinIntervalMs) {
                return 0;
            }
            if (lastNavDir != 0 && navDir != lastNavDir && (now - lastNavAtMs) < kEmNavFlipGuardMs) {
                return 0;
            }
        }
        if (navDir < 0) {
            if (selectedIndex > 0) {
                selectedIndex--;
            } else {
                selectedIndex = static_cast<int>(kEmMenuCount) - 1;
            }
            HERMESX_LOG_INFO("EM UI nav up -> %d (%s)", selectedIndex, kEmItems[selectedIndex]);
        } else {
            if (selectedIndex < static_cast<int>(kEmMenuCount) - 1) {
                selectedIndex++;
            } else {
                selectedIndex = 0;
            }
            HERMESX_LOG_INFO("EM UI nav down -> %d (%s)", selectedIndex, kEmItems[selectedIndex]);
        }
        HERMESX_LOG_INFO("EM UI select=%d label=%s", selectedIndex, kEmItems[selectedIndex]);
        selectionArmed = true;
        // Always record last nav time so rotary select doesn't trip nav timeout.
        lastNavAtMs = now;
        lastNavDir = navDir;
        updateListOffset();
    } else if (isSelect || isPress) {
        if (!selectionArmed) {
            HERMESX_LOG_INFO("EM UI select ignored (not armed)");
            return 0;
        }
        if (lastNavAtMs == 0 || (now - lastNavAtMs) > kEmNavArmMs) {
            selectionArmed = false;
            HERMESX_LOG_INFO("EM UI select ignored (nav timeout)");
            return 0;
        }
        selectionArmed = false;
        if (selectedIndex < static_cast<int>(kEmActionCount)) {
            if (lastSendAtMs != 0 && (now - lastSendAtMs) < kEmSendCooldownMs) {
                HERMESX_LOG_INFO("EM UI select ignored (cooldown)");
                return 0;
            }
            lastSendAtMs = now;
            HERMESX_LOG_INFO("EM UI select -> %d (%s)", selectedIndex, kEmItems[selectedIndex]);
            const EmAction action = static_cast<EmAction>(selectedIndex);
            if (action == EmAction::OkHere) {
                sendEmergencyAction(action);
            } else {
                enterFiveLineReport(action);
                return 0;
            }
        } else if (selectedIndex == static_cast<int>(kEmActionCount)) {
            HERMESX_LOG_INFO("EM UI select -> %d (%s)", selectedIndex, kEmItems[selectedIndex]);
            uiMode = UiMode::Stats;
            requestFocus();
            UIFrameEvent e;
            e.action = UIFrameEvent::Action::REDRAW_ONLY;
            notifyObservers(&e);
            return 0;
        } else if (selectedIndex == static_cast<int>(kEmActionCount) + 1) {
            HERMESX_LOG_INFO("EM UI select -> %d (%s)", selectedIndex, kEmItems[selectedIndex]);
            uiMode = UiMode::DeviceStatus;
            deviceStatusOffset = 0;
            deviceStatusSelectedIndex = 0;
            deviceDetailScrollOffset = 0;
            deviceDetailSelectedIndex = 0;
            requestFocus();
            UIFrameEvent e;
            e.action = UIFrameEvent::Action::REDRAW_ONLY;
            notifyObservers(&e);
            return 0;
        } else if (selectedIndex == static_cast<int>(kEmActionCount) + 2) {
            HERMESX_LOG_INFO("EM UI select -> %d (%s)", selectedIndex, kEmItems[selectedIndex]);
            setEmInfoBroadcastEnabled(!emInfoBroadcastEnabled);
            requestFocus();
            UIFrameEvent e;
            e.action = UIFrameEvent::Action::REDRAW_ONLY;
            notifyObservers(&e);
            return 0;
        } else if (selectedIndex == static_cast<int>(kEmActionCount) + 3 || selectedIndex == static_cast<int>(kEmActionCount) + 4) {
            const uint8_t slot = (selectedIndex == static_cast<int>(kEmActionCount) + 3) ? 0 : 1;
            HERMESX_LOG_INFO("EM UI select -> %d (%s)", selectedIndex, kEmItems[selectedIndex]);
            enterPassphraseEdit(slot);
            return 0;
        } else if (selectedIndex == static_cast<int>(kEmActionCount) + 5) {
            HERMESX_LOG_INFO("EM UI select -> %d (%s)", selectedIndex, kEmItems[selectedIndex]);
            sendResetLighthouse();
            return 0;
        }
    } else if (isCancel) {
        HERMESX_LOG_INFO("EM UI cancel");
        exitEmergencyMode();
        return 0;
    } else {
        return 0;
    }

    requestFocus();
    UIFrameEvent e;
    e.action = UIFrameEvent::Action::REDRAW_ONLY;
    notifyObservers(&e);
    return 0;
}

void HermesXEmUiModule::updateReportListOffset()
{
    if (kReportItemCount <= kReportVisibleRows) {
        reportListOffset = 0;
        return;
    }
    if (reportSelectedIndex < reportListOffset) {
        reportListOffset = reportSelectedIndex;
    } else if (reportSelectedIndex >= reportListOffset + kReportVisibleRows) {
        reportListOffset = reportSelectedIndex - kReportVisibleRows + 1;
    }
}

String HermesXEmUiModule::buildReportActionZh() const
{
    switch (pendingReportAction) {
    case EmAction::Trapped:
        return u8"受困";
    case EmAction::Medical:
        return u8"醫療";
    case EmAction::Supplies:
        return u8"物資";
    case EmAction::OkHere:
    default:
        return u8"安全";
    }
}

String HermesXEmUiModule::buildReportTimeCode() const
{
    const uint32_t rtcSec = getValidTime(RTCQuality::RTCQualityDevice, true);
    if (rtcSec == 0) {
        return "00000000-000000";
    }

    time_t raw = static_cast<time_t>(rtcSec);
    struct tm tmValue;
    localtime_r(&raw, &tmValue);
    char buf[20];
    snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d%02d%02d", tmValue.tm_year + 1900, tmValue.tm_mon + 1, tmValue.tm_mday,
             tmValue.tm_hour, tmValue.tm_min, tmValue.tm_sec);
    return String(buf);
}

String HermesXEmUiModule::getTextEditHeader() const
{
    switch (textEditContext) {
    case TextEditContext::ReportPlace:
        return u8"輸入地";
    case TextEditContext::ReportItem:
        return u8"輸入物";
    case TextEditContext::None:
    default:
        return u8"編輯";
    }
}

int HermesXEmUiModule::handleFiveLineReportInput(const InputEvent *event)
{
    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);
    const bool isUp = event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
    const bool isDown =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
    const bool isLeft =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT);
    const bool isRight =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT);
    const bool isSelect =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const bool isCancel =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL) ||
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK);
    const bool isCw = (event->inputEvent == eventCw);
    const bool isCcw = (event->inputEvent == eventCcw);
    const bool isPress = (event->inputEvent == eventPress);

    const uint32_t now = millis();
    int8_t navDir = 0;
    const bool isRotary = (event && event->source && strncmp(event->source, "rotEnc", 6) == 0);
    if (isRotary) {
        if (isCcw) {
            navDir = -1;
        } else if (isCw) {
            navDir = 1;
        } else if (eventCw == 0 && eventCcw == 0) {
            if (isUp || isLeft) {
                navDir = -1;
            } else if (isDown || isRight) {
                navDir = 1;
            }
        }
    } else {
        if (isUp || isLeft || isCcw) {
            navDir = -1;
        } else if (isDown || isRight || isCw) {
            navDir = 1;
        }
    }

    if (navDir != 0) {
        if (!isRotary) {
            if (lastNavAtMs != 0 && (now - lastNavAtMs) < kEmNavMinIntervalMs) {
                return 0;
            }
            if (lastNavDir != 0 && navDir != lastNavDir && (now - lastNavAtMs) < kEmNavFlipGuardMs) {
                return 0;
            }
        }
        reportSelectedIndex = (reportSelectedIndex + navDir + kReportItemCount) % kReportItemCount;
        selectionArmed = true;
        lastNavAtMs = now;
        lastNavDir = navDir;
        updateReportListOffset();
    } else if (isSelect || isPress) {
        if (!selectionArmed) {
            return 0;
        }
        if (lastNavAtMs == 0 || (now - lastNavAtMs) > kEmNavArmMs) {
            selectionArmed = false;
            return 0;
        }
        selectionArmed = false;
        switch (reportSelectedIndex) {
        case 0:
            exitFiveLineReport();
            return 0;
        case 4:
            enterReportTextEdit(ReportEditField::Place);
            return 0;
        case 5:
            enterReportTextEdit(ReportEditField::Item);
            return 0;
        case 6:
            if (lastSendAtMs != 0 && (now - lastSendAtMs) < kEmSendCooldownMs) {
                return 0;
            }
            lastSendAtMs = now;
            sendFiveLineReport();
            return 0;
        default:
            return 0;
        }
    } else if (isCancel) {
        exitFiveLineReport();
        return 0;
    } else {
        return 0;
    }

    requestFocus();
    UIFrameEvent e;
    e.action = UIFrameEvent::Action::REDRAW_ONLY;
    notifyObservers(&e);
    return 0;
}

int HermesXEmUiModule::handleStatsInput(const InputEvent *event)
{
    if (!event) {
        return 0;
    }

    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);
    const bool isSelect =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const bool isCancel =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL) ||
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK);
    const bool isPress = (event->inputEvent == eventPress);

    if (!(isSelect || isCancel || isPress)) {
        return 0;
    }

    uiMode = UiMode::Menu;
    requestFocus();
    UIFrameEvent e;
    e.action = UIFrameEvent::Action::REDRAW_ONLY;
    notifyObservers(&e);
    return 0;
}

int HermesXEmUiModule::handleDeviceStatusInput(const InputEvent *event)
{
    if (!event) {
        return 0;
    }

    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);
    const bool isUp = event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
    const bool isDown =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
    const bool isLeft =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT);
    const bool isRight =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT);
    const bool isSelect =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const bool isCancel =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL) ||
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK);
    const bool isPress = (event->inputEvent == eventPress);
    const bool isCw = (event->inputEvent == eventCw);
    const bool isCcw = (event->inputEvent == eventCcw);

    if (uiMode == UiMode::DeviceStatus) {
        const int total = getVisibleEmInfoNodeCount();
        if (total <= 0) {
            if (isSelect || isCancel || isPress) {
                uiMode = UiMode::Menu;
            } else {
                return 0;
            }
        } else if (isUp || isLeft || isCcw) {
            deviceStatusSelectedIndex = (deviceStatusSelectedIndex - 1 + total) % total;
            updateDeviceStatusOffset();
        } else if (isDown || isRight || isCw) {
            deviceStatusSelectedIndex = (deviceStatusSelectedIndex + 1) % total;
            updateDeviceStatusOffset();
        } else if (isSelect || isPress) {
            uiMode = UiMode::DeviceDetail;
            deviceDetailScrollOffset = 0;
            deviceDetailSelectedIndex = 0;
        } else if (isCancel) {
            uiMode = UiMode::Menu;
        } else {
            return 0;
        }
    } else {
        const EmInfoNodeStatus *entry = getSelectedEmInfoNode();
        if (!entry) {
            uiMode = UiMode::Menu;
        } else {
            String lines[12];
            const int totalLines = (uiMode == UiMode::DeviceDetail) ? buildDeviceDetailLines(*entry, lines, 12)
                                                                    : buildRelativePositionLines(*entry, lines, 12);
            const int maxOffset = std::max(0, totalLines - static_cast<int>(kDeviceDetailVisibleRows));
            if (isUp || isLeft || isCcw) {
                deviceDetailSelectedIndex = (deviceDetailSelectedIndex - 1 + totalLines) % totalLines;
            } else if (isDown || isRight || isCw) {
                deviceDetailSelectedIndex = (deviceDetailSelectedIndex + 1) % totalLines;
            } else if (isSelect || isPress) {
                const int selectedLine = deviceDetailSelectedIndex;
                if (uiMode == UiMode::DeviceDetail && selectedLine == 0) {
                    uiMode = UiMode::DeviceStatus;
                    deviceDetailScrollOffset = 0;
                    deviceDetailSelectedIndex = 0;
                } else if (uiMode == UiMode::DeviceDetail && selectedLine == totalLines - 1) {
                    uiMode = UiMode::RelativePosition;
                    deviceDetailScrollOffset = 0;
                    deviceDetailSelectedIndex = 0;
                } else if (uiMode == UiMode::RelativePosition && selectedLine == 0) {
                    uiMode = UiMode::DeviceDetail;
                    deviceDetailScrollOffset = 0;
                    deviceDetailSelectedIndex = 0;
                }
            } else if (isCancel) {
                uiMode = (uiMode == UiMode::RelativePosition) ? UiMode::DeviceDetail : UiMode::DeviceStatus;
                deviceDetailScrollOffset = 0;
                deviceDetailSelectedIndex = 0;
            } else {
                return 0;
            }
            if (deviceDetailSelectedIndex < deviceDetailScrollOffset) {
                deviceDetailScrollOffset = deviceDetailSelectedIndex;
            } else if (deviceDetailSelectedIndex >= deviceDetailScrollOffset + kDeviceDetailVisibleRows) {
                deviceDetailScrollOffset = deviceDetailSelectedIndex - kDeviceDetailVisibleRows + 1;
            }
            deviceDetailScrollOffset = clampValue<int>(deviceDetailScrollOffset, 0, maxOffset);
        }
    }

    requestFocus();
    UIFrameEvent e;
    e.action = UIFrameEvent::Action::REDRAW_ONLY;
    notifyObservers(&e);
    return 0;
}

int HermesXEmUiModule::handlePassphraseInput(const InputEvent *event)
{
    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);

    const bool isUp = event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
    const bool isDown =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
    const bool isLeft =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT);
    const bool isRight =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT);
    const bool isSelect =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const bool isCancel =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL) ||
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK);

    const bool isCw = (event->inputEvent == eventCw);
    const bool isCcw = (event->inputEvent == eventCcw);
    const bool isPress = (event->inputEvent == eventPress);

    const int rowCount = static_cast<int>(kKeyRowCount);
    const uint32_t now = millis();
    int8_t navDir = 0;
    const bool isRotary = (event && event->source && strncmp(event->source, "rotEnc", 6) == 0);
    if (isRotary) {
        if (isCcw) {
            navDir = -1;
        } else if (isCw) {
            navDir = 1;
        } else if (eventCw == 0 && eventCcw == 0) {
            if (isUp || isLeft) {
                navDir = -1;
            } else if (isDown || isRight) {
                navDir = 1;
            }
        }
    } else {
        if (isUp || isLeft || isCcw) {
            navDir = -1;
        } else if (isDown || isRight || isCw) {
            navDir = 1;
        }
    }

    if (navDir != 0) {
        if (!isRotary) {
            if (lastNavAtMs != 0 && (now - lastNavAtMs) < kEmNavMinIntervalMs) {
                return 0;
            }
            if (lastNavDir != 0 && navDir != lastNavDir && (now - lastNavAtMs) < kEmNavFlipGuardMs) {
                return 0;
            }
        }
        int totalKeys = 0;
        for (int r = 0; r < rowCount; ++r) {
            totalKeys += kKeyRowLengths[r];
        }
        int index = 0;
        for (int r = 0; r < rowCount; ++r) {
            if (r == keyRow) {
                index += keyCol;
                break;
            }
            index += kKeyRowLengths[r];
        }
        index = (index + navDir + totalKeys) % totalKeys;
        int remaining = index;
        for (int r = 0; r < rowCount; ++r) {
            const int rowLen = kKeyRowLengths[r];
            if (remaining < rowLen) {
                keyRow = r;
                keyCol = remaining;
                break;
            }
            remaining -= rowLen;
        }
        const char *label = kKeyRows[keyRow][keyCol];
        if (label) {
            HERMESX_LOG_INFO("EM UI key row=%d col=%d label=%s", keyRow, keyCol, label);
        } else {
            HERMESX_LOG_INFO("EM UI key row=%d col=%d label=?", keyRow, keyCol);
        }
        if (!isRotary) {
            lastNavAtMs = now;
            lastNavDir = navDir;
        }
    } else if (isSelect || isPress) {
        const char *label = kKeyRows[keyRow][keyCol];
        if (!label) {
            return 0;
        }
        if (strcmp(label, "OK") == 0) {
            exitPassphraseEdit(true);
            return 0;
        }
        if (strcmp(label, "DEL") == 0) {
            if (passDraft.length() > 0) {
                passDraft.remove(passDraft.length() - 1);
            }
        } else {
            if (passDraft.length() < kPassMaxLen) {
                passDraft += label;
            }
        }
    } else if (isCancel) {
        exitPassphraseEdit(false);
        return 0;
    } else {
        return 0;
    }

    requestFocus();
    UIFrameEvent e;
    e.action = UIFrameEvent::Action::REDRAW_ONLY;
    notifyObservers(&e);
    return 0;
}

int HermesXEmUiModule::handleTextEditInput(const InputEvent *event)
{
    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);

    const bool isUp = event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
    const bool isDown =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
    const bool isLeft =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT);
    const bool isRight =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT);
    const bool isSelect =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const bool isCancel =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL) ||
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK);

    const bool isCw = (event->inputEvent == eventCw);
    const bool isCcw = (event->inputEvent == eventCcw);
    const bool isPress = (event->inputEvent == eventPress);

    const int rowCount = static_cast<int>(kKeyRowCount);
    const uint32_t now = millis();
    int8_t navDir = 0;
    const bool isRotary = (event && event->source && strncmp(event->source, "rotEnc", 6) == 0);
    if (isRotary) {
        if (isCcw) {
            navDir = -1;
        } else if (isCw) {
            navDir = 1;
        } else if (eventCw == 0 && eventCcw == 0) {
            if (isUp || isLeft) {
                navDir = -1;
            } else if (isDown || isRight) {
                navDir = 1;
            }
        }
    } else {
        if (isUp || isLeft || isCcw) {
            navDir = -1;
        } else if (isDown || isRight || isCw) {
            navDir = 1;
        }
    }

    if (navDir != 0) {
        if (!isRotary) {
            if (lastNavAtMs != 0 && (now - lastNavAtMs) < kEmNavMinIntervalMs) {
                return 0;
            }
            if (lastNavDir != 0 && navDir != lastNavDir && (now - lastNavAtMs) < kEmNavFlipGuardMs) {
                return 0;
            }
        }
        int totalKeys = 0;
        for (int r = 0; r < rowCount; ++r) {
            totalKeys += kKeyRowLengths[r];
        }
        int index = 0;
        for (int r = 0; r < rowCount; ++r) {
            if (r == keyRow) {
                index += keyCol;
                break;
            }
            index += kKeyRowLengths[r];
        }
        index = (index + navDir + totalKeys) % totalKeys;
        int remaining = index;
        for (int r = 0; r < rowCount; ++r) {
            const int rowLen = kKeyRowLengths[r];
            if (remaining < rowLen) {
                keyRow = r;
                keyCol = remaining;
                break;
            }
            remaining -= rowLen;
        }
        if (!isRotary) {
            lastNavAtMs = now;
            lastNavDir = navDir;
        }
    } else if (isSelect || isPress) {
        const char *label = kKeyRows[keyRow][keyCol];
        if (!label) {
            return 0;
        }
        const size_t maxLen = kReportFieldMaxLen;
        if (strcmp(label, "OK") == 0) {
            exitTextEdit(true);
            return 0;
        }
        if (strcmp(label, "DEL") == 0) {
            if (textEditDraft.length() > 0) {
                textEditDraft.remove(textEditDraft.length() - 1);
            }
        } else if (textEditDraft.length() < maxLen) {
            textEditDraft += label;
        }
    } else if (isCancel) {
        exitTextEdit(false);
        return 0;
    } else {
        return 0;
    }

    requestFocus();
    UIFrameEvent e;
    e.action = UIFrameEvent::Action::REDRAW_ONLY;
    notifyObservers(&e);
    return 0;
}

ProcessMessage HermesXEmUiModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (mp.decoded.portnum == PORTNUM_HERMESX_EMERGENCY) {
        if (mp.decoded.payload.size >= strlen("RESET: EMAC") &&
            strncmp(reinterpret_cast<const char *>(mp.decoded.payload.bytes), "RESET: EMAC", strlen("RESET: EMAC")) == 0) {
            HERMESX_LOG_INFO("EM UI received reset payload");
            if (lighthouseModule) {
                lighthouseModule->resetEmergencyState(false);
            }
            exitEmergencyMode();
            return ProcessMessage::CONTINUE;
        }
        recordEmergencyReportPayload(mp);
        recordEmInfoPayload(mp);
        recordEmHeartbeatPayload(mp);
        return ProcessMessage::CONTINUE;
    }

    if (!active || !awaitingAck) {
        return ProcessMessage::CONTINUE;
    }
    if (mp.decoded.portnum != meshtastic_PortNum_ROUTING_APP) {
        return ProcessMessage::CONTINUE;
    }
    if (mp.decoded.request_id == 0 || mp.decoded.request_id != lastRequestId) {
        return ProcessMessage::CONTINUE;
    }

    meshtastic_Routing decoded = meshtastic_Routing_init_default;
    pb_decode_from_bytes(mp.decoded.payload.bytes, mp.decoded.payload.size, meshtastic_Routing_fields, &decoded);
    lastAckSuccess = (decoded.error_reason == meshtastic_Routing_Error_NONE);
    lastAckAtMs = millis();
    awaitingAck = false;
    lastRequestId = 0;
    HERMESX_LOG_INFO("EM UI ACK result=%s", lastAckSuccess ? "ACK" : "NACK");

    if (HermesXInterfaceModule::instance) {
        if (lastAckSuccess) {
            HermesXInterfaceModule::instance->playAckSuccess();
        } else {
            HermesXInterfaceModule::instance->playNackFail();
        }
    }

    UIFrameEvent e;
    e.action = UIFrameEvent::Action::REDRAW_ONLY;
    notifyObservers(&e);
    return ProcessMessage::CONTINUE;
}

int32_t HermesXEmUiModule::runOnce()
{
    if (active) {
        const uint32_t now = millis();
        const uint32_t emInfoIntervalMs = getEmInfoIntervalMs();
        const uint32_t emHeartbeatIntervalMs = getEmHeartbeatIntervalMs();
        if (emInfoBroadcastEnabled && (lastEmInfoSentMs == 0 || static_cast<uint32_t>(now - lastEmInfoSentMs) >= emInfoIntervalMs)) {
            sendEmInfoNow();
        }
        if (emHeartbeatIntervalMs > 0 &&
            (lastEmHeartbeatSentMs == 0 || static_cast<uint32_t>(now - lastEmHeartbeatSentMs) >= emHeartbeatIntervalMs)) {
            sendEmHeartbeatNow();
        }
        if (emInfoBroadcastEnabled && emHeartbeatIntervalMs > 0) {
            return std::min(emInfoIntervalMs, emHeartbeatIntervalMs);
        }
        if (emInfoBroadcastEnabled) {
            return emInfoIntervalMs;
        }
        if (emHeartbeatIntervalMs > 0) {
            return emHeartbeatIntervalMs;
        }
    }

    return 1000;
}

void HermesXEmUiModule::drawOverlay(OLEDDisplay *display, OLEDDisplayUiState * /*state*/)
{
    if (!active) {
        return;
    }

    tickScream(millis());

    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    if (uiMode == UiMode::PassphraseEdit) {
        display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
        display->setColor(EINK_WHITE);
#else
        display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
        display->fillRect(0, 0, width, height);

#if defined(USE_EINK)
        display->setColor(EINK_BLACK);
#else
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
        display->setFont(FONT_SMALL);
        const char *header = (editingPassSlot == 0) ? kPassHeaderA : kPassHeaderB;
        graphics::HermesX_zh::drawMixed(*display, 2, 0, header, graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
        graphics::HermesX_zh::drawMixedBounded(*display, 2, FONT_HEIGHT_SMALL + 2, width - 4, passDraft.c_str(),
                                               graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);

        const int16_t keyTop = (FONT_HEIGHT_SMALL * 2) + 6;
        const int16_t keyAreaHeight = height - keyTop;
        const int16_t rowHeight = (kKeyRowCount > 0) ? keyAreaHeight / kKeyRowCount : 0;
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        for (uint8_t row = 0; row < kKeyRowCount; ++row) {
            const int rowLen = kKeyRowLengths[row];
            if (rowLen <= 0) {
                continue;
            }
            const int16_t cellWidth = width / rowLen;
            const int16_t rowY = keyTop + row * rowHeight;
            for (int col = 0; col < rowLen; ++col) {
                const int16_t cellX = col * cellWidth;
                const char *label = kKeyRows[row][col];
                if (!label) {
                    continue;
                }
                const bool selected = (row == keyRow && col == keyCol);
                if (selected) {
#if defined(USE_EINK)
                    display->setColor(EINK_BLACK);
#else
                    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
                    display->fillRect(cellX, rowY, cellWidth, rowHeight);
#if defined(USE_EINK)
                    display->setColor(EINK_WHITE);
#else
                    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
                } else {
#if defined(USE_EINK)
                    display->setColor(EINK_BLACK);
#else
                    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
                }
                display->drawRect(cellX, rowY, cellWidth, rowHeight);
                display->drawString(cellX + cellWidth / 2, rowY + (rowHeight - FONT_HEIGHT_SMALL) / 2, label);
            }
        }
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        return;
    }

    if (uiMode == UiMode::TextEdit) {
        display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
        display->setColor(EINK_WHITE);
#else
        display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
        display->fillRect(0, 0, width, height);

#if defined(USE_EINK)
        display->setColor(EINK_BLACK);
#else
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
        display->setFont(FONT_SMALL);
        const String header = getTextEditHeader();
        graphics::HermesX_zh::drawMixed(*display, 2, 0, header.c_str(), graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL,
                                        nullptr);
        graphics::HermesX_zh::drawMixedBounded(*display, 2, FONT_HEIGHT_SMALL + 2, width - 4, textEditDraft.c_str(),
                                               graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);

        const int16_t keyTop = (FONT_HEIGHT_SMALL * 2) + 6;
        const int16_t keyAreaHeight = height - keyTop;
        const int16_t rowHeight = (kKeyRowCount > 0) ? keyAreaHeight / kKeyRowCount : 0;
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        for (uint8_t row = 0; row < kKeyRowCount; ++row) {
            const int rowLen = kKeyRowLengths[row];
            if (rowLen <= 0) {
                continue;
            }
            const int16_t cellWidth = width / rowLen;
            const int16_t rowY = keyTop + row * rowHeight;
            for (int col = 0; col < rowLen; ++col) {
                const int16_t cellX = col * cellWidth;
                const char *label = kKeyRows[row][col];
                if (!label) {
                    continue;
                }
                const bool selected = (row == keyRow && col == keyCol);
                if (selected) {
#if defined(USE_EINK)
                    display->setColor(EINK_BLACK);
#else
                    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
                    display->fillRect(cellX, rowY, cellWidth, rowHeight);
#if defined(USE_EINK)
                    display->setColor(EINK_WHITE);
#else
                    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
                } else {
#if defined(USE_EINK)
                    display->setColor(EINK_BLACK);
#else
                    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
                }
                display->drawRect(cellX, rowY, cellWidth, rowHeight);
                display->drawString(cellX + cellWidth / 2, rowY + (rowHeight - FONT_HEIGHT_SMALL) / 2, label);
            }
        }
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        return;
    }

    if (uiMode == UiMode::FiveLineReport) {
        display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
        display->setColor(EINK_WHITE);
#else
        display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
        display->fillRect(0, 0, width, height);

#if defined(USE_EINK)
        display->setColor(EINK_BLACK);
#else
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
        display->setFont(FONT_SMALL);
        display->fillRect(0, 0, width, kEmHeaderHeight);
#if defined(USE_EINK)
        display->setColor(EINK_WHITE);
#else
        display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
        graphics::HermesX_zh::drawMixed(*display, 4, 1, kReportHeader, graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL,
                                        nullptr);
#if defined(USE_EINK)
        display->setColor(EINK_BLACK);
#else
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif

        const String values[kReportItemCount] = {
            "",
            String(owner.short_name),
            buildReportActionZh(),
            buildReportTimeCode(),
            reportPlaceDraft.length() > 0 ? reportPlaceDraft : String("----"),
            reportItemDraft.length() > 0 ? reportItemDraft : String("----"),
            "",
        };
        const int16_t listTop = kEmHeaderHeight + 2;
        const int16_t scrollbarX = width - kEmScrollbarWidth;
        const int16_t bodyWidth = width - kEmScrollbarWidth - 2;
        for (int i = 0; i < kReportVisibleRows; ++i) {
            const int entryIndex = reportListOffset + i;
            if (entryIndex >= static_cast<int>(kReportItemCount)) {
                break;
            }
            const int16_t rowY = listTop + i * kEmRowHeight;
            String line = kReportItemsZh[entryIndex];
            if (entryIndex > 0 && entryIndex < 6) {
                line += ":";
                line += values[entryIndex];
            }
            const bool selected = (entryIndex == reportSelectedIndex);
            if (selected) {
#if defined(USE_EINK)
                display->fillRect(0, rowY - 1, width, kEmRowHeight);
                display->setColor(EINK_WHITE);
#else
                display->setColor(OLEDDISPLAY_COLOR::WHITE);
                display->fillRect(0, rowY - 1, width, kEmRowHeight);
                display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
            }
            graphics::HermesX_zh::drawMixedBounded(*display, 4, rowY, bodyWidth - 4, line.c_str(), graphics::HermesX_zh::GLYPH_WIDTH,
                                                   kEmRowHeight, nullptr);
            if (selected) {
#if defined(USE_EINK)
                display->setColor(EINK_BLACK);
#else
                display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
            }
        }
        drawSimpleScrollbar(display, scrollbarX, listTop, kReportVisibleRows * kEmRowHeight, kReportItemCount, kReportVisibleRows,
                            reportListOffset);
        char footer[24];
        snprintf(footer, sizeof(footer), "%d/%d", reportSelectedIndex + 1, static_cast<int>(kReportItemCount));
        graphics::HermesX_zh::drawMixedBounded(*display, 4, height - FONT_HEIGHT_SMALL, bodyWidth - 4, footer,
                                               graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
        return;
    }

    if (uiMode == UiMode::Stats) {
        display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
        display->setColor(EINK_WHITE);
#else
        display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
        display->fillRect(0, 0, width, height);

#if defined(USE_EINK)
        display->setColor(EINK_BLACK);
#else
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
        display->setFont(FONT_SMALL);
        display->fillRect(0, 0, width, kEmHeaderHeight);
#if defined(USE_EINK)
        display->setColor(EINK_WHITE);
#else
        display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
        graphics::HermesX_zh::drawMixed(*display, 4, 1, kStatsHeader, graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
#if defined(USE_EINK)
        display->setColor(EINK_BLACK);
#else
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif

        char lineBuf[48];
        const uint16_t values[] = {reportStats.trapped, reportStats.medical, reportStats.supplies, reportStats.safe};
        const int16_t startY = kEmHeaderHeight + 6;
        const int16_t rowHeight = 12;
        for (uint8_t i = 0; i < 4; ++i) {
            snprintf(lineBuf, sizeof(lineBuf), "%s: %u", kStatsLabels[i], static_cast<unsigned>(values[i]));
            graphics::HermesX_zh::drawMixedBounded(*display, 4, startY + i * rowHeight, width - 8, lineBuf,
                                                   graphics::HermesX_zh::GLYPH_WIDTH, rowHeight, nullptr);
        }

        graphics::HermesX_zh::drawMixedBounded(*display, 4, height - FONT_HEIGHT_SMALL, width - 8, u8"按下返回",
                                               graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
        return;
    }

    if (uiMode == UiMode::DeviceStatus) {
        display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
        display->setColor(EINK_WHITE);
#else
        display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
        display->fillRect(0, 0, width, height);

#if defined(USE_EINK)
        display->setColor(EINK_BLACK);
#else
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
        display->setFont(FONT_SMALL);
        display->fillRect(0, 0, width, kEmHeaderHeight);
#if defined(USE_EINK)
        display->setColor(EINK_WHITE);
#else
        display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
        graphics::HermesX_zh::drawMixed(*display, 4, 1, kDeviceStatusHeader, graphics::HermesX_zh::GLYPH_WIDTH,
                                        FONT_HEIGHT_SMALL, nullptr);
#if defined(USE_EINK)
        display->setColor(EINK_BLACK);
#else
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif

        const int total = getVisibleEmInfoNodeCount();
        const int16_t startY = kEmHeaderHeight + 4;
        const int16_t rowHeight = 12;
        for (uint8_t row = 0; row < kDeviceStatusVisibleRows; ++row) {
            const EmInfoNodeStatus *entry = getVisibleEmInfoNodeByIndex(deviceStatusOffset + row);
            if (!entry) {
                break;
            }
            char lineBuf[96];
            const char *name = entry->shortName[0] ? entry->shortName : "----";
            const String timeLabel = getNodeRelativeHeardLabel(*entry);
            snprintf(lineBuf, sizeof(lineBuf), "%s %s %s", name, entry->state[0] ? entry->state : u8"未知", timeLabel.c_str());
            const bool selected = (deviceStatusOffset + row == deviceStatusSelectedIndex);
            if (selected) {
#if defined(USE_EINK)
                display->fillRect(0, startY + row * rowHeight - 1, width, rowHeight);
                display->setColor(EINK_WHITE);
#else
                display->setColor(OLEDDISPLAY_COLOR::WHITE);
                display->fillRect(0, startY + row * rowHeight - 1, width, rowHeight);
                display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
            }
            graphics::HermesX_zh::drawMixedBounded(*display, 4, startY + row * rowHeight, width - 8, lineBuf,
                                                   graphics::HermesX_zh::GLYPH_WIDTH, rowHeight, nullptr);
            if (selected) {
#if defined(USE_EINK)
                display->setColor(EINK_BLACK);
#else
                display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
            }
        }

        drawSimpleScrollbar(display, width - kEmScrollbarWidth, startY, kDeviceStatusVisibleRows * rowHeight, total,
                            kDeviceStatusVisibleRows, deviceStatusOffset);

        char footer[32];
        snprintf(footer, sizeof(footer), "%d/%d 按下查看", deviceStatusSelectedIndex + 1, total);
        graphics::HermesX_zh::drawMixedBounded(*display, 4, height - FONT_HEIGHT_SMALL, width - 8, footer,
                                               graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
        return;
    }

    if (uiMode == UiMode::DeviceDetail || uiMode == UiMode::RelativePosition) {
        const EmInfoNodeStatus *entry = getSelectedEmInfoNode();
        display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
        display->setColor(EINK_WHITE);
#else
        display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
        display->fillRect(0, 0, width, height);

#if defined(USE_EINK)
        display->setColor(EINK_BLACK);
#else
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
        display->setFont(FONT_SMALL);
        display->fillRect(0, 0, width, kEmHeaderHeight);
#if defined(USE_EINK)
        display->setColor(EINK_WHITE);
#else
        display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
        graphics::HermesX_zh::drawMixed(*display, 4, 1, uiMode == UiMode::DeviceDetail ? kDeviceDetailHeader : kRelativePositionHeader,
                                        graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
#if defined(USE_EINK)
        display->setColor(EINK_BLACK);
#else
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
        if (!entry) {
            graphics::HermesX_zh::drawMixedBounded(*display, 4, kEmHeaderHeight + 4, width - 8, u8"無資料",
                                                   graphics::HermesX_zh::GLYPH_WIDTH, 12, nullptr);
            return;
        }

        String lines[12];
        const int totalLines = (uiMode == UiMode::DeviceDetail) ? buildDeviceDetailLines(*entry, lines, 12)
                                                                : buildRelativePositionLines(*entry, lines, 12);
        const int16_t startY = kEmHeaderHeight + 4;
        const int16_t rowHeight = 12;
        const int16_t scrollbarX = width - kEmScrollbarWidth;
        const int16_t bodyWidth = width - kEmScrollbarWidth - 2;
        for (uint8_t row = 0; row < kDeviceDetailVisibleRows; ++row) {
            const int lineIndex = deviceDetailScrollOffset + row;
            if (lineIndex >= totalLines) {
                break;
            }
            const bool selected = (lineIndex == deviceDetailSelectedIndex);
            if (selected) {
#if defined(USE_EINK)
                display->fillRect(0, startY + row * rowHeight - 1, width, rowHeight);
                display->setColor(EINK_WHITE);
#else
                display->setColor(OLEDDISPLAY_COLOR::WHITE);
                display->fillRect(0, startY + row * rowHeight - 1, width, rowHeight);
                display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
            }
            graphics::HermesX_zh::drawMixedBounded(*display, 4, startY + row * rowHeight, bodyWidth - 4, lines[lineIndex].c_str(),
                                                   graphics::HermesX_zh::GLYPH_WIDTH, rowHeight, nullptr);
            if (selected) {
#if defined(USE_EINK)
                display->setColor(EINK_BLACK);
#else
                display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
            }
        }
        drawSimpleScrollbar(display, scrollbarX, startY, kDeviceDetailVisibleRows * rowHeight, totalLines, kDeviceDetailVisibleRows,
                            deviceDetailScrollOffset);
        char footer[24];
        snprintf(footer, sizeof(footer), "%d/%d", deviceDetailSelectedIndex + 1, totalLines);
        graphics::HermesX_zh::drawMixedBounded(*display, 4, height - FONT_HEIGHT_SMALL, bodyWidth - 4, footer,
                                               graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
        return;
    }

    const int16_t rightColumnWidth = kRightColumnWidth;
    const int16_t rightColumnX = width - rightColumnWidth;
    const int16_t listRight = rightColumnX - 2;
    int32_t countdownSec = -1;
    if (lighthouseModule) {
        countdownSec = lighthouseModule->getEmergencyGraceRemainingSec();
    }
    const int16_t contentRight = listRight;
    const int16_t contentWidth = contentRight > 0 ? contentRight : width;

    display->setTextAlignment(TEXT_ALIGN_LEFT);

#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
    display->fillRect(0, 0, width, height);

#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif

    display->setFont(FONT_SMALL);
    display->fillRect(0, 0, contentWidth, kEmHeaderHeight);
#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
    drawWarningIcon(display, 2, 2);
#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
    graphics::HermesX_zh::drawMixed(*display, 14, 1, kEmHeaderZh, graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);

#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif

    const int16_t listTop = kEmHeaderHeight + 2;
    const int16_t scrollbarX = contentWidth - kEmScrollbarWidth;
    const int16_t bodyWidth = contentWidth - kEmScrollbarWidth - 2;
    for (int i = 0; i < kEmVisibleRows; ++i) {
        const int entryIndex = listOffset + i;
        if (entryIndex >= static_cast<int>(kEmMenuCount)) {
            break;
        }
        const int16_t rowY = listTop + i * kEmRowHeight;
        if (rowY + kEmRowHeight > height) {
            break;
        }
        if (entryIndex == selectedIndex) {
#if defined(USE_EINK)
            display->fillRect(0, rowY - 1, contentWidth, kEmRowHeight);
            display->setColor(EINK_WHITE);
#else
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
            display->fillRect(0, rowY - 1, contentWidth, kEmRowHeight);
            display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
            String label = kEmItemsZh[entryIndex];
            if (strcmp(kEmItems[entryIndex], "EMINFO_TOGGLE") == 0) {
                label += emInfoBroadcastEnabled ? u8" 開" : u8" 關";
            }
            graphics::HermesX_zh::drawMixedBounded(*display, 4, rowY, bodyWidth - 4, label.c_str(),
                                                   graphics::HermesX_zh::GLYPH_WIDTH, kEmRowHeight, nullptr);
#if defined(USE_EINK)
            display->setColor(EINK_BLACK);
#else
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
        } else {
            String label = kEmItemsZh[entryIndex];
            if (strcmp(kEmItems[entryIndex], "EMINFO_TOGGLE") == 0) {
                label += emInfoBroadcastEnabled ? u8" 開" : u8" 關";
            }
            graphics::HermesX_zh::drawMixedBounded(*display, 4, rowY, bodyWidth - 4, label.c_str(),
                                                   graphics::HermesX_zh::GLYPH_WIDTH, kEmRowHeight, nullptr);
        }
    }
    drawSimpleScrollbar(display, scrollbarX, listTop, kEmVisibleRows * kEmRowHeight, kEmMenuCount, kEmVisibleRows, listOffset);

    char menuFooter[24];
    snprintf(menuFooter, sizeof(menuFooter), "%d/%d", selectedIndex + 1, static_cast<int>(kEmMenuCount));
    graphics::HermesX_zh::drawMixedBounded(*display, 2, height - FONT_HEIGHT_SMALL, bodyWidth - 2, menuFooter,
                                           graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);

    if (banner.length() > 0) {
        display->setFont(FONT_SMALL);
        graphics::HermesX_zh::drawMixedBounded(*display, 28, height - FONT_HEIGHT_SMALL, bodyWidth - 28, banner.c_str(),
                                               graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
    }

    char countdownBuf[6];
    if (countdownSec < 0) {
        snprintf(countdownBuf, sizeof(countdownBuf), "--");
    } else {
        if (countdownSec > 99) {
            countdownSec = 99;
        }
        snprintf(countdownBuf, sizeof(countdownBuf), "%02ld", static_cast<long>(countdownSec));
    }
    const char *statusText = nullptr;
    if (lastAckAtMs != 0 && !lastAckSuccess) {
        statusText = "傳送失敗";
    } else if (lastAckAtMs != 0 && (millis() - lastAckAtMs) <= kAckBannerMs) {
        statusText = "傳送成功\n請原地待命";
    } else if (hasSentOnce) {
        statusText = "已傳送";
    } else {
        statusText = "等待中";
    }
    drawRightColumn(display, rightColumnX, rightColumnWidth, height, countdownBuf, statusText);
}

void HermesXEmUiModule::drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    if (!active) {
        return;
    }
    drawOverlay(display, state);
}

#endif
