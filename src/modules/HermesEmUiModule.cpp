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
#include "modules/LighthouseModule.h"
#include "modules/RoutingModule.h"
#include <OLEDDisplay.h>
#include <OLEDDisplayUi.h>

HermesXEmUiModule *hermesXEmUiModule = nullptr;

namespace {
constexpr uint32_t kAckBannerMs = 2000;
constexpr uint32_t kEmHeaderHeight = 14;
constexpr uint32_t kEmRowHeight = 12;
constexpr uint32_t kEmSendCooldownMs = 1500;
constexpr uint32_t kEmNavArmMs = 5000;
constexpr const char *kEmItems[] = {"TRAPPED", "MEDICAL", "SUPPLIES", "SAFE"};
constexpr const char *kEmItemsZh[] = {"受困", "醫療", "物資", "安全"};
constexpr const char *kEmHeaderZh = "人員尋回模式啟動";
constexpr const int16_t kRightColumnWidth = 42;
constexpr float kScreamFreq = 2600.0f;
constexpr uint32_t kScreamToneMs = 600;
constexpr uint32_t kScreamGapMs = 200;

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
} // namespace

HermesXEmUiModule::HermesXEmUiModule() : SinglePortModule("HermesXEmUi", PORTNUM_HERMESX_EMERGENCY)
{
    hermesXEmUiModule = this;
    if (inputBroker) {
        inputObserver.observe(inputBroker);
    }
}

void HermesXEmUiModule::startScream()
{
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
    selectedIndex = 0;
    awaitingAck = false;
    lastRequestId = 0;
    lastAckAtMs = 0;
    selectionArmed = false;
    lastNavAtMs = 0;
    lastSendAtMs = 0;
    hasSentOnce = false;
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
    startScream();
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
    awaitingAck = false;
    lastRequestId = 0;
    stopScream();
    setThemeActive(false);
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
        wantAck = true;
        break;
    case EmAction::Medical:
        payload = "NEED: MEDICAL";
        wantAck = true;
        break;
    case EmAction::Supplies:
        payload = "NEED: SUPPLIES";
        wantAck = true;
        break;
    case EmAction::OkHere:
        payload = "STATUS: OK";
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

    if (action == EmAction::OkHere) {
        exitEmergencyMode();
    }
}

int HermesXEmUiModule::handleInputEvent(const InputEvent *event)
{
    if (!active) {
        return 0;
    }

    if (event && event->source) {
        HERMESX_LOG_DEBUG("EM UI input src=%s event=%d", event->source, event->inputEvent);
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

    const bool isCw = (event->inputEvent == eventCw);
    const bool isCcw = (event->inputEvent == eventCcw);
    const bool isPress = (event->inputEvent == eventPress);

    if (isUp || isLeft || isCcw) {
        if (selectedIndex > 0) {
            selectedIndex--;
        } else {
            selectedIndex = 3;
        }
        selectionArmed = true;
        lastNavAtMs = millis();
        HERMESX_LOG_INFO("EM UI nav up -> %d (%s)", selectedIndex, kEmItems[selectedIndex]);
    } else if (isDown || isRight || isCw) {
        if (selectedIndex < 3) {
            selectedIndex++;
        } else {
            selectedIndex = 0;
        }
        selectionArmed = true;
        lastNavAtMs = millis();
        HERMESX_LOG_INFO("EM UI nav down -> %d (%s)", selectedIndex, kEmItems[selectedIndex]);
    } else if (isSelect || isPress) {
        if (!selectionArmed) {
            HERMESX_LOG_INFO("EM UI select ignored (not armed)");
            return 0;
        }
        const uint32_t now = millis();
        if (lastNavAtMs == 0 || (now - lastNavAtMs) > kEmNavArmMs) {
            selectionArmed = false;
            HERMESX_LOG_INFO("EM UI select ignored (nav timeout)");
            return 0;
        }
        if (lastSendAtMs != 0 && (now - lastSendAtMs) < kEmSendCooldownMs) {
            HERMESX_LOG_INFO("EM UI select ignored (cooldown)");
            return 0;
        }
        lastSendAtMs = now;
        selectionArmed = false;
        HERMESX_LOG_INFO("EM UI select -> %d (%s)", selectedIndex, kEmItems[selectedIndex]);
        sendEmergencyAction(static_cast<EmAction>(selectedIndex));
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

ProcessMessage HermesXEmUiModule::handleReceived(const meshtastic_MeshPacket &mp)
{
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

void HermesXEmUiModule::drawOverlay(OLEDDisplay *display, OLEDDisplayUiState * /*state*/)
{
    if (!active) {
        return;
    }

    tickScream(millis());

    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
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
    for (int i = 0; i < 4; ++i) {
        const int16_t rowY = listTop + i * kEmRowHeight;
        if (rowY + kEmRowHeight > height) {
            break;
        }
        if (i == selectedIndex) {
#if defined(USE_EINK)
            display->fillRect(0, rowY - 1, contentWidth, kEmRowHeight);
            display->setColor(EINK_WHITE);
#else
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
            display->fillRect(0, rowY - 1, contentWidth, kEmRowHeight);
            display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
            graphics::HermesX_zh::drawMixedBounded(*display, 4, rowY, contentWidth - 4, kEmItemsZh[i],
                                                   graphics::HermesX_zh::GLYPH_WIDTH, kEmRowHeight, nullptr);
#if defined(USE_EINK)
            display->setColor(EINK_BLACK);
#else
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
        } else {
            graphics::HermesX_zh::drawMixedBounded(*display, 4, rowY, contentWidth - 4, kEmItemsZh[i],
                                                   graphics::HermesX_zh::GLYPH_WIDTH, kEmRowHeight, nullptr);
        }
    }

    if (banner.length() > 0) {
        display->setFont(FONT_SMALL);
        graphics::HermesX_zh::drawMixedBounded(*display, 2, height - FONT_HEIGHT_SMALL, contentWidth - 2, banner.c_str(),
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
