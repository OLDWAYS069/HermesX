// HermesXInterfaceModule.cpp - Refactored without TinyScheduler

#include "HermesXInterfaceModule.h"
#include "EmergencyAdaptiveModule.h"
#include "mesh/MeshService.h"
#include "mesh/NodeDB.h"
#include "mesh/Channels.h"
#include "mesh/Router.h"
#include "meshtastic/mesh.pb.h"
#include "meshtastic/emergency.pb.h"
#include "modules/CannedMessageModule.h"
#include "SinglePortModule.h"
#include "pb.h"
#include "pb_encode.h"
#include "HermesXPacketUtils.h"

#include "MusicModule.h"
#include "sleep_hooks.h"
#include "Led.h"
#include "buzz/buzz.h"
#include "graphics/Screen.h"
#include "main.h"
#ifdef ARCH_ESP32
#include <driver/rtc_io.h>
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Arduino.h>
#include "RedirectablePrint.h"
#include "DebugConfiguration.h"
#include "meshtastic/portnums.pb.h"
#include "HermesXLog.h"
#include "HermesFace.h"
#include "HermesEmergencyUi.h"
#include "PowerStatus.h"
#include "graphics/ScreenFonts.h"


#include "ReliableRouter.h"
#include "Default.h"
#include "MeshTypes.h"
#include "configuration.h"
#include "mesh-pb-constants.h"
#include "modules/NodeInfoModule.h"
#include "modules/RoutingModule.h"

#include "pb_decode.h"
#include "mesh/mesh-pb-constants.h"

#define PIN_LED 6
#define NUM_LEDS 8

#define BUZZER_PIN 17

#ifndef TFT_BACKLIGHT_ON
#define TFT_BACKLIGHT_ON HIGH
#endif

extern graphics::Screen *screen;

namespace {
constexpr uint32_t kDefaultShutdownDurationMs = 700;
constexpr uint32_t kShutdownAnimationStepMs = 20;

constexpr uint32_t kPowerHoldFadeDurationMs = 1200;
constexpr uint16_t kHermesPressIntervalMs = 500;
constexpr uint32_t kHermesSafeWindowMs = 3000;

constexpr uint32_t kPowerHoldRedColor = 0xFF0000;

struct ShutdownToneSegment {
    float freq;
    uint16_t duration;
};

constexpr ShutdownToneSegment kShutdownToneSegments[] = {
    {523.0f, 160}, // C5
    {415.0f, 150}, // G#4
    {349.0f, 180}, // F4
    {0.0f, 0}
};
constexpr uint32_t kEmergencyMenuStateHoldMs = 8000;

struct EmergencyFallbackEntry {
    const char *label;
    meshtastic_EmergencyType type;
};

constexpr EmergencyFallbackEntry kEmergencyFallbackMenu[] = {
    {"I AM TRAPPED", meshtastic_EmergencyType_SOS},
    {"NEED MEDICAL", meshtastic_EmergencyType_NEED},
    {"NEED SUPPLIES", meshtastic_EmergencyType_RESOURCE},
    {"I AM SAFE", meshtastic_EmergencyType_STATUS}
};
constexpr size_t kEmergencyFallbackCount = sizeof(kEmergencyFallbackMenu) / sizeof(kEmergencyFallbackMenu[0]);
constexpr size_t kEmergencyOverlayMaxRows = 4;
constexpr uint32_t kOverlayHideSendMs = 1200;
constexpr uint32_t kOverlayHideAckMs = 900;
constexpr uint32_t kOverlayHideNackMs = 900;

void disableVisibleOutputsCommon()
{
    ledForceOn.set(false);
    ledBlink.set(false);

#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, !TFT_BACKLIGHT_ON);
#ifdef ARCH_ESP32
    if (rtc_gpio_is_valid_gpio((gpio_num_t)TFT_BL)) {
        rtc_gpio_hold_en((gpio_num_t)TFT_BL);
    }
#endif
#endif
#ifdef ST7701_BACKLIGHT_EN
    pinMode(ST7701_BACKLIGHT_EN, OUTPUT);
    digitalWrite(ST7701_BACKLIGHT_EN, LOW);
#endif
#ifdef ILI9341_BACKLIGHT_EN
    pinMode(ILI9341_BACKLIGHT_EN, OUTPUT);
    digitalWrite(ILI9341_BACKLIGHT_EN, LOW);
#endif
#ifdef PIN_EINK_EN
    pinMode(PIN_EINK_EN, OUTPUT);
    digitalWrite(PIN_EINK_EN, LOW);
#endif
#ifdef VEXT_ENABLE
    pinMode(VEXT_ENABLE, OUTPUT);
    digitalWrite(VEXT_ENABLE, !VEXT_ON_VALUE);
#endif
    if (screen) {
        screen->setOn(false);
        screen->doDeepSleep();
    }
}

void performShutdownAnimation(uint32_t durationMs, Adafruit_NeoPixel &strip, uint32_t baseColor, MusicModule *music)
{
    if (durationMs == 0) {
        durationMs = kDefaultShutdownDurationMs;
    }

    if (music) {
        music->stopTone();
    }

    size_t toneIndex = 0;
    uint32_t toneDeadline = 0;

    auto scheduleTone = [&](size_t index) -> bool {
        const auto &segment = kShutdownToneSegments[index];
        if (segment.duration == 0) {
            return false;
        }
        toneDeadline = millis() + segment.duration;
        if (music) {
            if (segment.freq > 0.0f) {
                music->playTone(segment.freq, segment.duration);
            } else {
                music->stopTone();
            }
        }
        return true;
    };

    bool hasTone = scheduleTone(toneIndex);
    uint32_t start = millis();
    while (millis() - start < durationMs) {
        float progress = static_cast<float>(millis() - start) / static_cast<float>(durationMs);
        if (progress > 1.0f)
            progress = 1.0f;
        float brightness = 1.0f - progress;
        brightness = constrain(brightness, 0.0f, 1.0f);
        uint8_t r = ((baseColor >> 16) & 0xFF) * brightness;
        uint8_t g = ((baseColor >> 8) & 0xFF) * brightness;
        uint8_t b = (baseColor & 0xFF) * brightness;
        strip.fill(strip.Color(r, g, b));
        strip.show();

        if (hasTone && millis() >= toneDeadline) {
            if (music) {
                music->stopTone();
            }
            toneIndex++;
            hasTone = scheduleTone(toneIndex);
        }

        delay(kShutdownAnimationStepMs);
    }

    if (music) {
        music->stopTone();
    }
    strip.clear();
    strip.show();
}

void fallbackShutdownEffect(uint32_t durationMs)
{
    static Adafruit_NeoPixel fallbackStrip(NUM_LEDS, PIN_LED, NEO_GRB + NEO_KHZ800);
    static bool fallbackStripInit = false;
    if (!fallbackStripInit) {
        fallbackStrip.begin();
        fallbackStrip.setBrightness(60);
        fallbackStripInit = true;
    }

    static MusicModule fallbackMusic(BUZZER_PIN);
    static bool fallbackMusicInit = false;
    if (!fallbackMusicInit) {
        fallbackMusic.begin();
        fallbackMusicInit = true;
    }

    performShutdownAnimation(durationMs, fallbackStrip, kPowerHoldRedColor, &fallbackMusic);
    disableVisibleOutputsCommon();
}
}



HermesXInterfaceModule* globalHermes = nullptr;
HermesXInterfaceModule *HermesXInterfaceModule::instance = nullptr;
uint32_t safeTimeout = 5000;

HermesXFeedbackCallback hermesXCallback = nullptr;

HermesXInterfaceModule::HermesXInterfaceModule()
  : SinglePortModule("hermesx", meshtastic_PortNum_PRIVATE_APP),
    OSThread("hermesTask", 500),
    rgb(NUM_LEDS, PIN_LED, NEO_GRB + NEO_KHZ800),
    music(BUZZER_PIN)
{
    globalHermes = this;
    HermesXInterfaceModule::instance = this;
    observe(&service->fromNumChanged);
    isPromiscuous = true;
    loopbackOk = true;
    currentTheme = defaultTheme;
    resetEmergencyMenuStates();
    initLED();
    ensureEmergencyListeners();
    const uint32_t now = millis();
    if (emergencyUiActive && !emergencyOverlayEnabled && emergencyOverlayResumeMs != 0) {
        if (static_cast<int32_t>(now - emergencyOverlayResumeMs) >= 0) {
            emergencyOverlayEnabled = true;
            emergencyOverlayResumeMs = 0;
        }
    }

    HERMESX_LOG_DEBUG("constroct");
    playStartupLEDAnimation(currentTheme.colorIdleBreathBase);
}

void HermesXInterfaceModule::setup()
{
    ensureEmergencyListeners();
    const uint32_t now = millis();
    if (emergencyUiActive && !emergencyOverlayEnabled && emergencyOverlayResumeMs != 0) {
        if (static_cast<int32_t>(now - emergencyOverlayResumeMs) >= 0) {
            emergencyOverlayEnabled = true;
            emergencyOverlayResumeMs = 0;
        }
    }
    if (emergencyModule) {
        onEmergencyModeChanged(emergencyModule->isEmergencyActive());
    }
}


void HermesXInterfaceModule::ensureEmergencyListeners()
{
    if (emergencyListenersRegistered) {
        return;
    }

    if (!emergencyModule) {
        return;
    }

    emergencyModule->addModeListener([this](bool active) { onEmergencyModeChanged(active); });
    emergencyModule->addTxResultListener([this](uint8_t type, bool ok) { onEmergencyTxResult(type, ok); });
    emergencyListenersRegistered = true;
    HERMESX_LOG_INFO("Emergency listeners attached");
    onEmergencyModeChanged(emergencyModule->isEmergencyActive());
}

void HermesXInterfaceModule::suppressEmergencyOverlay(uint32_t durationMs)
{
    emergencyOverlayEnabled = false;
    emergencyOverlayResumeMs = durationMs ? millis() + durationMs : 0;
}
void HermesXInterfaceModule::handleButtonPress()
{
    HERMESX_LOG_INFO("HermesX interface button pressed");
    playReceiveFeedback();
}


void HermesXInterfaceModule::drawFace(const char* face, uint16_t color)
{
    HermesFaceRenderContext ctx{};
    if (!HermesX_TryGetFaceRenderContext(ctx) || ctx.display == nullptr) {
        HERMESX_LOG_INFO("HermesXInterfaceModule drawFace fallback; no display ctx for face=%s", face ? face : "");
        return;
    }

    renderFaceWithContext(ctx, face, color, false);
}

void HermesXInterfaceModule::renderFaceWithContext(const HermesFaceRenderContext &ctx, const char *face, uint16_t color, bool showOverlay)
{
    OLEDDisplay *display = ctx.display;
    if (!display) {
        return;
    }

    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();

    const bool overlayVisible = showOverlay && emergencyOverlayEnabled;
    const int16_t overlayHeight = overlayVisible ? getEmergencyOverlayHeight() : 0;
    const int16_t contentHeight = height > overlayHeight ? height - overlayHeight : height;

    const char *faceText = face;
    uint16_t faceColor = color;

    if ((!faceText || !*faceText) && emergencyBannerText.length()) {
        faceText = emergencyBannerText.c_str();
        faceColor = emergencyBannerColor ? emergencyBannerColor : 0xFFFF;
    }

    if (!faceText || !*faceText) {
        faceText = "EM";
    }

    int16_t faceWidth = 0;
    int16_t faceHeight = 0;
    auto applyFaceFont = [&](const char *textPtr, int16_t &outWidth, int16_t &outHeight) {
        const int16_t availableWidth = width > 8 ? width - 8 : width;
        const int16_t availableHeight = contentHeight > 8 ? contentHeight - 8 : contentHeight;

        display->setFont(FONT_LARGE);
        outWidth = display->getStringWidth(textPtr);
        outHeight = FONT_HEIGHT_LARGE;
        if (outWidth > availableWidth || outHeight > availableHeight) {
            display->setFont(FONT_MEDIUM);
            outWidth = display->getStringWidth(textPtr);
            outHeight = FONT_HEIGHT_MEDIUM;
        }
        if (outWidth > availableWidth || outHeight > availableHeight) {
            display->setFont(FONT_SMALL);
            outWidth = display->getStringWidth(textPtr);
            outHeight = FONT_HEIGHT_SMALL;
        }
    };

    applyFaceFont(faceText, faceWidth, faceHeight);

    display->setTextAlignment(TEXT_ALIGN_CENTER);

    if (contentHeight > 0) {
        const int16_t drawX = ctx.originX + width / 2;
        const int16_t faceAreaTop = ctx.originY + overlayHeight;
        int16_t drawY = faceAreaTop + (contentHeight - faceHeight) / 2;
        if (drawY < faceAreaTop) {
            drawY = faceAreaTop;
        }

        const int16_t centerX = ctx.originX + width / 2;
        const int16_t centerY = faceAreaTop + contentHeight / 2;
        const int16_t faceMaxDimension = faceWidth > faceHeight ? faceWidth : faceHeight;
        constexpr int16_t kCirclePadding = 6;
        int16_t circleRadius = faceMaxDimension / 2 + kCirclePadding;
        const int16_t minDisplayDimension = width < contentHeight ? width : contentHeight;
        int16_t availableRadius = minDisplayDimension / 2;
        if (availableRadius > 2) {
            availableRadius -= 2;
        } else if (availableRadius < 0) {
            availableRadius = 0;
        }
        if (circleRadius > availableRadius) {
            circleRadius = availableRadius;
        }
        if (circleRadius < 0) {
            circleRadius = 0;
        }

#if defined(USE_EINK)
        display->setColor(EINK_BLACK);
#else
        display->setColor(faceColor ? OLEDDISPLAY_COLOR::WHITE : OLEDDISPLAY_COLOR::BLACK);
#endif

#if defined(USE_EINK)
        const int16_t padding = 6;
        const int16_t rectWidth = faceWidth + padding * 2;
        const int16_t rectHeight = faceHeight + padding * 2;
        const int16_t rectX = ctx.originX + (width - rectWidth) / 2;
        const int16_t rectY = faceAreaTop + (contentHeight - rectHeight) / 2;

        display->fillRect(rectX, rectY, rectWidth, rectHeight);
        display->setColor(EINK_WHITE);
#endif

        if (circleRadius > 0) {
            display->drawCircle(centerX, centerY, circleRadius);
        }

        display->drawString(drawX, drawY, faceText);
    }

    display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(faceColor ? OLEDDISPLAY_COLOR::WHITE : OLEDDISPLAY_COLOR::BLACK);
#endif

    if (overlayVisible) {
        drawEmergencyOverlay(display, ctx, getEmergencyOverlayHeight());
    }
}

bool HermesXInterfaceModule::wantUIFrame()
{
    return emergencyUiActive;
}

int16_t HermesXInterfaceModule::getEmergencyOverlayHeight() const
{
    const int16_t headerHeight = FONT_HEIGHT_SMALL + 4;
    const int16_t optionHeight = FONT_HEIGHT_SMALL + 2;

    size_t entryCount = kEmergencyFallbackCount;

    if (cannedMessageModule && cannedMessageModule->getActiveList() != CannedListKind::NORMAL) {
        const size_t menuCount = cannedMessageModule->getEmergencyMenuEntryCount();
        if (menuCount > 0) {
            entryCount = menuCount;
        }
    }

    if (entryCount == 0) {
        entryCount = kEmergencyFallbackCount;
    }

    return headerHeight + optionHeight * static_cast<int16_t>(entryCount) + 2;
}


void HermesXInterfaceModule::drawEmergencyOverlay(OLEDDisplay *display, const HermesFaceRenderContext &ctx, int16_t /*overlayHeight*/)
{
    if (!display) {
        return;
    }

    const int16_t width = display->getWidth();
    const int16_t originX = ctx.originX;
    const int16_t originY = ctx.originY;
    const int16_t headerHeight = FONT_HEIGHT_SMALL + 4;
    const int16_t optionHeight = FONT_HEIGHT_SMALL + 2;

    bool useDynamicMenu = false;
    size_t entryCount = kEmergencyFallbackCount;
    int selectedIndex = -1;

    if (cannedMessageModule && cannedMessageModule->getActiveList() != CannedListKind::NORMAL) {
        const size_t menuCount = cannedMessageModule->getEmergencyMenuEntryCount();
        if (menuCount > 0) {
            useDynamicMenu = true;
            entryCount = menuCount;
            selectedIndex = cannedMessageModule->getCurrentMessageIndex();
        }
    }

    if (entryCount == 0) {
        entryCount = kEmergencyFallbackCount;
    }

    const size_t visibleCount = entryCount;
    const int16_t listHeight = optionHeight * static_cast<int16_t>(visibleCount) + 2;

#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
    display->fillRect(originX, originY, width, headerHeight);
    display->setColor(EINK_WHITE);
    display->fillRect(originX, originY + headerHeight, width, listHeight);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
    display->fillRect(originX, originY, width, headerHeight);
    display->fillRect(originX, originY + headerHeight, width, listHeight);
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif

    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_CENTER);

    String header;
    if (emergencyBannerVisible && emergencyBannerText.length()) {
        header = emergencyBannerText;
    } else {
        header = "EMERGENCY";
    }
#if !MESHTASTIC_EXCLUDE_HERMESX
    if (powerStatus && powerStatus->getHasBattery()) {
        header += " ";
        header += powerStatus->getBatteryChargePercent();
        header += '%';
        if (powerStatus->getHasUSB()) {
            header += " USB";
        } else if (powerStatus->getIsCharging()) {
            header += " CHG";
        }
    }
#endif

    const int16_t headerBaseline = originY + ((headerHeight - FONT_HEIGHT_SMALL) / 2);
    display->drawString(originX + width / 2, headerBaseline, header);

    display->setTextAlignment(TEXT_ALIGN_LEFT);

    if (selectedIndex < 0 && useDynamicMenu) {
        selectedIndex = 0;
    }

    const size_t cappedVisible = visibleCount ? visibleCount : 1;
    size_t startIndex = 0;
    if (entryCount > cappedVisible) {
        size_t clampedSelection = 0;
        if (useDynamicMenu && selectedIndex >= 0) {
            size_t sel = static_cast<size_t>(selectedIndex);
            if (sel >= entryCount) {
                sel = entryCount - 1;
            }
            clampedSelection = sel;
        }
        if (cappedVisible > 1 && clampedSelection >= cappedVisible - 1) {
            startIndex = clampedSelection - (cappedVisible - 1);
        }
        if (startIndex + cappedVisible > entryCount) {
            startIndex = entryCount - cappedVisible;
        }
    }

    int16_t itemY = originY + headerHeight + 2;
    for (size_t row = 0; row < cappedVisible; ++row) {
        const size_t idx = startIndex + row;

        const char *label = nullptr;
        meshtastic_EmergencyType entryType = meshtastic_EmergencyType_SOS;

        if (useDynamicMenu && cannedMessageModule) {
            const char *menuLabel = cannedMessageModule->getMessageLabel(idx);
            const char *entryLabel = nullptr;
            meshtastic_EmergencyType resolvedType = entryType;
            if (cannedMessageModule->getEmergencyMenuEntry(idx, entryLabel, resolvedType) && entryLabel) {
                label = entryLabel;
                entryType = resolvedType;
            } else if (menuLabel) {
                label = menuLabel;
            }
        } else {
            label = kEmergencyFallbackMenu[idx % kEmergencyFallbackCount].label;
            entryType = kEmergencyFallbackMenu[idx % kEmergencyFallbackCount].type;
        }

        if (!label || !*label) {
            label = "-";
        }

        const bool isSelected = useDynamicMenu && selectedIndex >= 0 && static_cast<size_t>(selectedIndex) == idx;
        const EmergencyEntryState state = getEmergencyStateForType(entryType);

#if defined(USE_EINK)
        if (isSelected) {
            display->setColor(EINK_WHITE);
            display->fillRect(originX, itemY - 1, width, FONT_HEIGHT_SMALL + 2);
            display->setColor(EINK_BLACK);
        } else {
            display->setColor(EINK_WHITE);
        }
#else
        if (isSelected) {
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
            display->fillRect(originX, itemY - 1, width, FONT_HEIGHT_SMALL + 2);
            display->setColor(OLEDDISPLAY_COLOR::BLACK);
        } else {
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
        }
#endif

        String line;
        line.reserve(32);
        line += isSelected ? '>' : ' ';
        line += ' ';
        if (label) {
            line += label;
        }

        switch (state) {
        case EmergencyEntryState::Pending:
            line += " (SENDING)";
            break;
        case EmergencyEntryState::Success:
            line += " (OK)";
            break;
        case EmergencyEntryState::Failed:
            line += " (FAILED)";
            break;
        default:
            break;
        }

        display->drawString(originX + 4, itemY, line);

#if !defined(USE_EINK)
        if (isSelected) {
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
        }
#endif

        itemY += optionHeight;
    }
}





void HermesXInterfaceModule::drawFrame(OLEDDisplay *display, OLEDDisplayUiState * /*state*/, int16_t x, int16_t y)
{
    if (!emergencyUiActive || !display) {
        return;
    }

    const char *faceText = nullptr;
    uint16_t faceColor = 0;
    if (!HermesEmergencyUi::currentFace(faceText, faceColor)) {
        faceText = "EM";
        faceColor = 0xFFFF;
    }

    HermesFaceRenderContext ctx{};
    ctx.display = display;
    ctx.originX = x;
    ctx.originY = y;

    renderFaceWithContext(ctx, faceText, faceColor, true);
}



void HermesXInterfaceModule::resetEmergencyMenuStates()
{
    for (size_t i = 0; i < kEmergencyTypeCount; ++i) {
        emergencyMenuStates[i] = EmergencyEntryState::Idle;
        emergencyMenuStateExpiry[i] = 0;
    }
}

void HermesXInterfaceModule::updateEmergencyMenuState(meshtastic_EmergencyType type, EmergencyEntryState state, uint32_t now)
{
    const int8_t index = menuIndexForEmergencyType(type);
    if (index < 0) {
        return;
    }

    emergencyMenuStates[index] = state;
    if (state == EmergencyEntryState::Success || state == EmergencyEntryState::Failed) {
        emergencyMenuStateExpiry[index] = now + kEmergencyMenuStateHoldMs;
    } else if (state == EmergencyEntryState::Pending) {
        emergencyMenuStateExpiry[index] = 0;
    } else {
        emergencyMenuStateExpiry[index] = 0;
    }
}

void HermesXInterfaceModule::decayEmergencyMenuStates(uint32_t now)
{
    for (size_t i = 0; i < kEmergencyTypeCount; ++i) {
        if (emergencyMenuStateExpiry[i] == 0) {
            continue;
        }

        if (static_cast<int32_t>(now - emergencyMenuStateExpiry[i]) >= 0) {
            emergencyMenuStateExpiry[i] = 0;
            emergencyMenuStates[i] = EmergencyEntryState::Idle;
        }
    }
}

int8_t HermesXInterfaceModule::menuIndexForEmergencyType(meshtastic_EmergencyType type) const
{
    switch (type) {
    case meshtastic_EmergencyType_SOS:
        return 0;
    case meshtastic_EmergencyType_SAFE:
        return 1;
    case meshtastic_EmergencyType_NEED:
        return 2;
    case meshtastic_EmergencyType_RESOURCE:
        return 3;
    case meshtastic_EmergencyType_STATUS:
    case meshtastic_EmergencyType_HEARTBEAT:
        return 4;
    default:
        return -1;
    }
}

HermesXInterfaceModule::EmergencyEntryState HermesXInterfaceModule::getEmergencyStateForType(meshtastic_EmergencyType type) const
{
    const int8_t index = menuIndexForEmergencyType(type);
    if (index < 0) {
        return EmergencyEntryState::Idle;
    }
    return emergencyMenuStates[index];
}

void HermesXInterfaceModule::applyEmergencyTheme(bool active)
{
    const bool wasActive = emergencyUiActive;

    emergencyThemeActive = active;
    emergencyUiActive = active;

    if (active) {
        emergencyTheme = defaultTheme;
        emergencyTheme.colorAck = 0x07E0;
        emergencyTheme.colorFailed = 0xF800;
        emergencyTheme.colorIdleBreathBase = 0xF800;
        emergencyTheme.breathBrightnessMin = 0.55f;
        emergencyTheme.breathBrightnessMax = 1.0f;
        currentTheme = emergencyTheme;
    } else {
        currentTheme = defaultTheme;
        emergencyBannerVisible = false;
    }

    breathPhase = 0.0f;
    if (breathDelta < 0.0f) {
        breathDelta = -breathDelta;
    }

    if (wasActive != emergencyUiActive) {
        if (emergencyUiActive) {
            requestFocus();
        }

        UIFrameEvent evt;
        evt.action = UIFrameEvent::Action::REGENERATE_FRAMESET;
        notifyObservers(&evt);
    }
}

void HermesXInterfaceModule::initLED() {
    rgb.begin();
    rgb.setBrightness(60);
    rgb.fill(rgb.Color(0, 0, 20));
    rgb.show();
    HERMESX_LOG_INFO("LED setup\n");
}


void HermesXInterfaceModule::updateLED() {
    uint32_t now = millis();

    if (powerHoldActive) {
        float progress = 0.0f;
        if (powerHoldDurationMs > 0) {
            progress = static_cast<float>(powerHoldElapsedMs) / static_cast<float>(powerHoldDurationMs);
        }

        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;

        rgb.clear();
        uint32_t segmentColor = currentTheme.colorIdleBreathBase;

        for (int step = 0; step < NUM_LEDS; ++step) {
            const int ledIndex = (NUM_LEDS - 1) - step;
            const float threshold = static_cast<float>(step + 1) / static_cast<float>(NUM_LEDS);
            bool segmentOn = false;

            switch (powerHoldMode) {
            case PowerHoldMode::PowerOn:
                segmentOn = progress >= threshold;
                break;
            case PowerHoldMode::PowerOff:
                segmentOn = progress < threshold;
                break;
            default:
                segmentOn = false;
                break;
            }

            if (segmentOn) {
                rgb.setPixelColor(ledIndex, segmentColor);
            }
        }

        rgb.show();

        if (progress >= 1.0f) {
            startPowerHoldFade(now);
        }
        return;
    }

    if (powerHoldFadeActive) {
        float fadeProgress = 0.0f;
        if (kPowerHoldFadeDurationMs > 0) {
            fadeProgress = static_cast<float>(now - powerHoldFadeStartMs) / static_cast<float>(kPowerHoldFadeDurationMs);
        }
        if (fadeProgress < 0.0f) fadeProgress = 0.0f;
        if (fadeProgress > 1.0f) fadeProgress = 1.0f;

        uint32_t color = scaleColor(kPowerHoldRedColor, fadeProgress);
        rgb.fill(color);
        rgb.show();

        if (fadeProgress >= 1.0f) {
            powerHoldFadeActive = false;
            powerHoldLatchedRed = true;
        }
        return;
    }

    if (powerHoldLatchedRed) {
        rgb.fill(kPowerHoldRedColor);
        rgb.show();
        return;
    }


    // === ?��???ACK/NACK ?��? ===
    if (animState == LedAnimState::ACK_FLASH || animState == LedAnimState::NACK_FLASH) {
        uint32_t color = (animState == LedAnimState::ACK_FLASH) ? currentTheme.colorAck : currentTheme.colorFailed;

        if (now - lastFlashToggle >= 120) {
            lastFlashToggle = now;
            flashOn = !flashOn;
            if (!flashOn) {
                flashCount++;
            }
        }
        if (flashCount >= 4) {
            flashCount = 0;
            flashOn = false;
            animState = LedAnimState::IDLE;
        }

        rgb.fill(flashOn ? color : 0);
        rgb.show();
        return;
    }

    // === ?�新?�吸?��? ===
    if (now - lastBreathUpdate > 30) {
        lastBreathUpdate = now;
        breathPhase += breathDelta;
        if (breathPhase >= 1.0f) {
            breathPhase = 1.0f;
            breathDelta = -breathDelta;
        } else if (breathPhase <= 0.0f) {
            breathPhase = 0.0f;
            breathDelta = -breathDelta;
        }
    }

    // === ?�吸?�景??===
    float bgScale = lerp(currentTheme.breathBrightnessMin, currentTheme.breathBrightnessMax, breathPhase);
    uint32_t bgColor = scaleColor(currentTheme.colorIdleBreathBase, bgScale);
    rgb.fill(bgColor);

    // === 事件?�畫（送出/?�收/?��?資�?�?==
    if (animState == LedAnimState::SEND_R2L || animState == LedAnimState::RECV_L2R || animState == LedAnimState::INFO2_L2R) {
        const uint16_t stepInterval = 80;
        if (now - lastAnimStep >= stepInterval) {
            lastAnimStep = now;
            int next = animPos + animDir;
            if (next < 0 || next >= NUM_LEDS) {
                // ?��??��? idle runner ?�起點、方?�設�?
                if (animState == LedAnimState::SEND_R2L) {
                    idlePos = 0;
                    idleDir = 1;
                } else {
                    idlePos = NUM_LEDS - 1;
                    idleDir = -1;
                }
                animState = LedAnimState::IDLE;
            } else {
                animPos = next;
            }
        }

        if (animState != LedAnimState::IDLE) {
            rgb.setPixelColor(animPos, eventColor);
            if (animState == LedAnimState::INFO2_L2R && animPos + 1 < NUM_LEDS) {
                rgb.setPixelColor(animPos + 1, eventColor);
            }
        }

        rgb.show();
        return;
    }

    // === 待�? runner（可變速、�??��??��?===
    if (animState == LedAnimState::IDLE && idleRunnerEnabled) {
        uint16_t interval = idleRunnerInterval;
        if (idleRunnerVarSpeed) {
            interval = (uint16_t)lerp((float)idleRunnerMaxInterval, (float)idleRunnerMinInterval, breathPhase);
        }

        bool atEdge = (idlePos == 0 && idleDir < 0) || (idlePos == NUM_LEDS - 1 && idleDir > 0);
        uint16_t wait = atEdge ? idleRunnerEdgeDwell : interval;

        if (now - lastIdleMove >= wait) {
            lastIdleMove = now;
            if (atEdge) {
                idleDir = -idleDir;
            }
            idlePos += idleDir;
        }

        uint32_t dotColor = scaleColor(currentTheme.colorIdleBreathBase, bgScale * 2.2f);
        rgb.setPixelColor(idlePos, dotColor);
    }

    rgb.show();
}


void HermesXInterfaceModule::registerRawButtonPress(HermesButtonSource source)
{
    const uint32_t now = millis();

    if (safeWindowActive) {
        if (static_cast<int32_t>(now - safeWindowDeadlineMs) > 0) {
            HERMESX_LOG_DEBUG("Safe window expired before press");
            safeWindowActive = false;
            safePressCount = 0;
            rawPressCount = 1;
            lastRawPressMs = now;
            HERMESX_LOG_DEBUG("Raw press restart count=%u source=%d", static_cast<unsigned>(rawPressCount), static_cast<int>(source));
            return;
        }

        safePressCount++;
        lastRawPressMs = now;
        HERMESX_LOG_DEBUG("Safe press %u/%u source=%d", static_cast<unsigned>(safePressCount), 2u, static_cast<int>(source));
        if (safePressCount >= 2) {
            safeWindowActive = false;
            safePressCount = 0;
            rawPressCount = 0;
            HERMESX_LOG_DEBUG("Invoking SAFE double-click");
            onDoubleClickWithin3s();
        }
        return;
    }

    if (static_cast<int32_t>(now - lastRawPressMs) > kHermesPressIntervalMs) {
        rawPressCount = 0;
    }

    rawPressCount++;
    lastRawPressMs = now;
    HERMESX_LOG_DEBUG("Raw press count=%u source=%d", static_cast<unsigned>(rawPressCount), static_cast<int>(source));

    if (rawPressCount >= 3) {
        rawPressCount = 0;
        safeWindowActive = true;
        safePressCount = 0;
        safeWindowDeadlineMs = now + kHermesSafeWindowMs;
        HERMESX_LOG_INFO("Trigger SOS triple press");
        onTripleClick();
    }
}

void HermesXInterfaceModule::onPacketSent() {
    music.playSendSound();        // ?�出?��?
    startSendAnim();              // ?�出?�畫（R?�L）�?交給 updateLED()/animState 流�???
    HERMESX_LOG_DEBUG("Sent MSG");
}


void HermesXInterfaceModule::playTone(float freq, uint32_t duration_ms) {
    if (freq > 0) {
        ledcWriteTone(0, freq);
        toneStopTime = millis() + duration_ms;
    }
}

bool HermesXInterfaceModule::wantPacket(const meshtastic_MeshPacket *p)
{
    // ?��? Routing 封�???Text Message 封�??��?�?
    return p->decoded.portnum == meshtastic_PortNum_ROUTING_APP ||
       p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP ||
       p->decoded.portnum == meshtastic_PortNum_NODEINFO_APP;
}

ProcessMessage HermesXInterfaceModule::handleReceived(const meshtastic_MeshPacket &packet)
{
    if (packet.decoded.portnum == meshtastic_PortNum_ROUTING_APP) {
        HERMESX_LOG_DEBUG("Received routing: wait=%d, req_id=0x%08x, id=%u, from=%x, to=%x, len=%u",
            waitingForAck,
            packet.decoded.request_id,
            packet.id,
            packet.from,
            packet.to,
            packet.decoded.payload.size);

        meshtastic_Routing decoded = meshtastic_Routing_init_default;
        bool ok = pb_decode_from_bytes(
            packet.decoded.payload.bytes,
            packet.decoded.payload.size,
            meshtastic_Routing_fields,
            &decoded);

        if (ok) {
            if (isFromUs(&packet) && isToUs(&packet)) {
                // Local router reflection; ignore.
                return ProcessMessage::CONTINUE;
            }
            if (decoded.error_reason != meshtastic_Routing_Error_NONE) {
                playSendFailedFeedback();
                waitingForAck = false;
                if (hasPendingEmergencyType) {
                    onEmergencyTxResult(static_cast<uint8_t>(pendingEmergencyType), false);
                }
                HERMESX_LOG_WARN("Routing NAK error=%d req_id=0x%08x id=%u from=%x to=%x",
                    decoded.error_reason,
                    packet.decoded.request_id,
                    packet.id,
                    packet.from,
                    packet.to);
            } else {
                bool ours = waitingForAck ||
                    packet.decoded.request_id == lastSentRequestId ||
                    packet.id == lastSentId ||
                    isFromUs(&packet);
                if (ours) {
                    ackReceived = true;
                    waitingForAck = false;
                    pendingSuccessFeedback = true;
                    successFeedbackTime = millis() + 300;
                    HERMESX_LOG_INFO("Routing ACK req_id=0x%08x id=%u", packet.decoded.request_id, packet.id);
                }
            }
        } else {
            HERMESX_LOG_ERROR("Failed to decode Routing payload.");
        }
    }

    const bool fromUs = isFromUs(&packet);

    if (packet.decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP) {
        if (fromUs) {
            const bool isFirstLocalSend = (lastSentTime == 0);
            if (isFirstLocalSend || packet.id != lastSentId) {
                lastSentId = packet.id;
                lastSentRequestId = packet.id;
                lastSentTime = millis();
                waitingForAck = packet.want_ack;
                ackReceived = false;
                pendingSuccessFeedback = false;
                playSendFeedback();
            }
        } else {
            playReceiveFeedback();
        }
    }

    if (packet.decoded.portnum == meshtastic_PortNum_NODEINFO_APP &&
        !fromUs) {
        playNodeInfoFeedback();
    }

    return ProcessMessage::CONTINUE;
}

void hermesXFeedbackHandler(int index, bool state) {
    if (index == 0 && state) {

        HermesXInterfaceModule::instance->playReceiveFeedback();
    } else if (index == 2 && state) {

        HermesXInterfaceModule::instance->playSendFeedback();
    }
}



int32_t HermesXInterfaceModule::runOnce() {
    static bool firstTime = true;
    static bool testPlayed = false;
    uint32_t now = millis();

    // === ?��??��?段�??�執行�?�?===
    if (firstTime) {
        firstTime = false;
        music.begin();
        music.playStartupSound();
        HERMESX_LOG_INFO("first runOnce() call");

        // 建�? feedback ?�呼
        hermesXCallback = [](int index, bool state) {
            if (HermesXInterfaceModule::instance) {
                HermesXInterfaceModule::instance->handleExternalNotification(index, state);
                HERMESX_LOG_INFO("callbacksetup");
            }
        };
    }

    // === 測試?��?（�?一次�?===
    if (!testPlayed) {
        testPlayed = true;
        music.playSendSound();
    }

    ensureEmergencyListeners();
    if (emergencyUiActive && !emergencyOverlayEnabled && emergencyOverlayResumeMs != 0) {
        if (static_cast<int32_t>(now - emergencyOverlayResumeMs) >= 0) {
            emergencyOverlayEnabled = true;
            emergencyOverlayResumeMs = 0;
        }
    }

    // === ?�止 tone ?�放 ===
    if (toneStopTime && now >= toneStopTime) {
        stopTone();
        toneStopTime = 0;
    }

    // === Timeout: 等�? ACK 超�? 3 秒�?視為失�? ===
    if (waitingForAck && (now - lastSentTime > 30000)) {
        waitingForAck = false;
        ackReceived = false;
        if (hasPendingEmergencyType) {
            onEmergencyTxResult(static_cast<uint8_t>(pendingEmergencyType), false);
        }
        playSendFailedFeedback();
        HERMESX_LOG_WARN("ACK Timeout: Delivery failed");
    }

    // === ACK ?��??�畫?�音?�觸??===
    if (pendingSuccessFeedback && now >= successFeedbackTime) {
        pendingSuccessFeedback = false;
        music.playSuccessSound();
        startAckFlash();
        HERMESX_LOG_INFO("Success feedback triggered (ACK animation)");
    }

    decayEmergencyMenuStates(now);
    renderLEDs();

    return 100;  // ?��??��?�?00ms
}

void HermesXInterfaceModule::stopTone() {
    ledcWriteTone(0, 0);
}





int HermesXInterfaceModule::onNotify(uint32_t fromNum)
 {
    HERMESX_LOG_INFO("onNotify fromNum=%u", fromNum);

    return 0;
}

void HermesXInterfaceModule::handleExternalNotification(int index, bool state) {
    if (!state) {
        return;
    }

    switch (index) {
    case 0:
        playReceiveFeedback();
        break;
    case 2:
        playSendFeedback();
        break;
    case 3:
        handleAckNotification(true);
        break;
    case 4:
        handleAckNotification(false);
        break;
    default:
        HERMESX_LOG_DEBUG("Unhandled external notification index=%d state=%d", index, state);
        break;
    }
}



void HermesXInterfaceModule::handleAckNotification(bool success)
{
    if (!waitingForAck) {
        HERMESX_LOG_DEBUG("Ignoring ACK notification; no pending send");
        return;
    }

    waitingForAck = false;

    if (success) {
        ackReceived = true;
        pendingSuccessFeedback = true;
        successFeedbackTime = millis() + 300;
        HERMESX_LOG_INFO("ACK notification received");
        if (hasPendingEmergencyType) {
            onEmergencyTxResult(static_cast<uint8_t>(pendingEmergencyType), true);
        }
    } else {
        ackReceived = false;
        pendingSuccessFeedback = false;
        HERMESX_LOG_WARN("ACK notification failed");
        if (hasPendingEmergencyType) {
            onEmergencyTxResult(static_cast<uint8_t>(pendingEmergencyType), false);
        }
        playSendFailedFeedback();
    }
}



void HermesXInterfaceModule::playSendFeedback()
{
    if (emergencyUiActive) {
        suppressEmergencyOverlay(kOverlayHideSendMs);
        HermesEmergencyUi::onSend();
    }
    music.playSendSound();
    startSendAnim();
    HERMESX_LOG_INFO("Send feedback triggered");
}

void HermesXInterfaceModule::playReceiveFeedback() {
    music.playReceiveSound();
    startReceiveAnim();
    if (emergencyUiActive) {
        HermesEmergencyUi::onReceive();
    }
    HERMESX_LOG_INFO("Receive feedback triggered");
}

void HermesXInterfaceModule::playSendSuccessFeedback() {
    pendingSuccessFeedback = true;
    successFeedbackTime = millis() + 1000;
    HERMESX_LOG_INFO("Success feedback scheduled");
}

void HermesXInterfaceModule::playNodeInfoFeedback() {
    music.playNodeInfoSound();
    startInfoReceiveAnimTwoDots();
    HERMESX_LOG_INFO("NodeInfo feedback triggered");
}

void HermesXInterfaceModule::playSendFailedFeedback() {
    music.playFailedSound();   
    startNackFlash();
    if (emergencyUiActive) {
        HermesEmergencyUi::onAck(false);
    }
    HERMESX_LOG_INFO("Failed feedback triggered");
}

void HermesXInterfaceModule::onTripleClick()
{
    const meshtastic_EmergencyType type = meshtastic_EmergencyType_SOS;
    if (!emergencyModule) {
        markEmergencyActionFailed(type);
        playSendFailedFeedback();
        return;
    }

    beginEmergencyAction(type);
    if (emergencyModule->sendSOS()) {
        playSOSFeedback();
    } else {
        markEmergencyActionFailed(type);
        playSendFailedFeedback();
    }
}

void HermesXInterfaceModule::onDoubleClickWithin3s()
{
    const meshtastic_EmergencyType type = meshtastic_EmergencyType_SAFE;
    if (!emergencyModule) {
        markEmergencyActionFailed(type);
        playSendFailedFeedback();
        return;
    }

    beginEmergencyAction(type);
    if (emergencyModule->sendSafe()) {
        showEmergencyBanner(true, F("SAFE SENDING"), 0x07E0);
        playAckSuccess();
    } else {
        markEmergencyActionFailed(type);
        playSendFailedFeedback();
    }
}

void HermesXInterfaceModule::onEmergencyTxResult(uint8_t type, bool ok)
{
    const meshtastic_EmergencyType emergencyType = static_cast<meshtastic_EmergencyType>(type);
    if (hasPendingEmergencyType && emergencyType == pendingEmergencyType) {
        hasPendingEmergencyType = false;
    }

    if (emergencyUiActive) {
        HermesEmergencyUi::onAck(ok);
    }

    updateEmergencyMenuState(emergencyType, ok ? EmergencyEntryState::Success : EmergencyEntryState::Failed, millis());

    const uint16_t color = ok ? 0x07E0 : 0xF800;
    const __FlashStringHelper *text = nullptr;

    switch (emergencyType) {
    case meshtastic_EmergencyType_SOS:
        text = ok ? F("SOS ACTIVE") : F("SOS FAILED");
        break;
    case meshtastic_EmergencyType_SAFE:
        if (ok) {
            showEmergencyBanner(false);
            return;
        }
        text = F("SAFE FAILED");
        break;
    case meshtastic_EmergencyType_NEED:
        text = ok ? F("NEED SENT") : F("NEED FAILED");
        break;
    case meshtastic_EmergencyType_RESOURCE:
        text = ok ? F("RESOURCE SENT") : F("RESOURCE FAILED");
        break;
    case meshtastic_EmergencyType_STATUS:
        text = ok ? F("STATUS SENT") : F("STATUS FAILED");
        break;
    case meshtastic_EmergencyType_HEARTBEAT:
        if (!ok) {
            text = F("HEARTBEAT FAIL");
            break;
        }
        return;
    default:
        text = ok ? F("EM SENT") : F("EM FAILED");
        break;
    }

    showEmergencyBanner(true, text, color);
}

void HermesXInterfaceModule::onEmergencyModeChanged(bool active)
{
    HermesEmergencyUi::onEmergencyModeChanged(active, false);
    applyEmergencyTheme(active);
#if !MESHTASTIC_EXCLUDE_HERMESX
    if (cannedMessageModule) {
        CannedListKind target = active ?
            ((moduleConfig.emergency.mode == meshtastic_ModuleConfig_EmergencyConfig_Mode_DRILL)
                 ? CannedListKind::DRILL
                 : CannedListKind::EMERGENCY)
            : CannedListKind::NORMAL;
        cannedMessageModule->setActiveList(target);
    }
#endif
    resetEmergencyMenuStates();
    hasPendingEmergencyType = false;

    if (active) {
        showEmergencyBanner(true, F("EM MODE"), 0xF800);
    } else {
        showEmergencyBanner(false);
    }
}


void HermesXInterfaceModule::playSOSFeedback()
{
    playSendFeedback();
    showEmergencyBanner(true, F("EM SENDING"), 0xF800);
}

void HermesXInterfaceModule::playAckSuccess()
{
    music.playSuccessSound();
    startAckFlash();
    if (emergencyUiActive) {
        suppressEmergencyOverlay(kOverlayHideAckMs);
        HermesEmergencyUi::onAck(true);
    }
}

void HermesXInterfaceModule::playNackFail()
{
    music.playFailedSound();
    startNackFlash();
    if (emergencyUiActive) {
        suppressEmergencyOverlay(kOverlayHideNackMs);
        HermesEmergencyUi::onAck(false);
    }
}

void HermesXInterfaceModule::showEmergencyBanner(bool on, const __FlashStringHelper *text, uint16_t color)
{
    emergencyBannerVisible = on;
    if (on) {
        if (text) {
            emergencyBannerText = text;
        } else {
            emergencyBannerText = F("EM");
        }
        emergencyBannerColor = color ? color : 0xF800;
    } else {
        emergencyBannerText = "";
        emergencyBannerColor = 0;
    }
}

void HermesXInterfaceModule::beginEmergencyAction(meshtastic_EmergencyType type)
{
    pendingEmergencyType = type;
    hasPendingEmergencyType = true;
    updateEmergencyMenuState(type, EmergencyEntryState::Pending, millis());
}

void HermesXInterfaceModule::markEmergencyActionFailed(meshtastic_EmergencyType type)
{
    if (hasPendingEmergencyType && pendingEmergencyType == type) {
        hasPendingEmergencyType = false;
    }
    pendingEmergencyType = type;
    updateEmergencyMenuState(type, EmergencyEntryState::Failed, millis());
}

void HermesXInterfaceModule::requestEmergencyFocus()
{
    requestFocus();
}

void HermesXInterfaceModule::startSendAnim() {
    animState = LedAnimState::SEND_R2L;
    animPos = NUM_LEDS - 1;
    animDir = -1;
    eventColor = currentTheme.colorSendPrimary;
    lastAnimStep = millis();
}

void HermesXInterfaceModule::startReceiveAnim() {
    animState = LedAnimState::RECV_L2R;
    animPos = 0;
    animDir = 1;
    eventColor = currentTheme.colorReceivePrimary;
    lastAnimStep = millis();
}

void HermesXInterfaceModule::startInfoReceiveAnimTwoDots() {
    animState = LedAnimState::INFO2_L2R;
    animPos = 0;
    animDir = 1;
    eventColor = currentTheme.colorAck;
    lastAnimStep = millis();
}

void HermesXInterfaceModule::startAckFlash() {
    animState = LedAnimState::ACK_FLASH;
    flashCount = 0;
    flashOn = true;
    lastFlashToggle = millis();
}

void HermesXInterfaceModule::startNackFlash() {
    animState = LedAnimState::NACK_FLASH;
    flashCount = 0;
    flashOn = true;
    lastFlashToggle = millis();
}

void HermesXInterfaceModule::startPowerHoldAnimation(PowerHoldMode mode, uint32_t holdDurationMs) {
    if (mode == PowerHoldMode::None || holdDurationMs == 0) {
        stopPowerHoldAnimation(false);
        return;
    }

    powerHoldActive = true;
    powerHoldMode = mode;
    powerHoldDurationMs = holdDurationMs;
    powerHoldElapsedMs = 0;
    powerHoldReady = false;
    powerHoldFadeActive = false;
    powerHoldLatchedRed = false;
    powerHoldFadeStartMs = 0;

    // Reset other LED effects while the hold animation runs
    animState = LedAnimState::IDLE;
    flashOn = false;
    flashCount = 0;
}

void HermesXInterfaceModule::updatePowerHoldAnimation(uint32_t elapsedMs) {
    if (powerHoldMode == PowerHoldMode::None) {
        return;
    }
    powerHoldElapsedMs = elapsedMs;
    if (powerHoldDurationMs > 0 && powerHoldElapsedMs > powerHoldDurationMs) {
        powerHoldElapsedMs = powerHoldDurationMs;
    }

    if (powerHoldActive && powerHoldDurationMs > 0 && powerHoldElapsedMs >= powerHoldDurationMs) {
        startPowerHoldFade(millis());
    }
}

void HermesXInterfaceModule::stopPowerHoldAnimation(bool completed) {
    if (completed) {
        startPowerHoldFade(millis());
        return;
    }

    powerHoldActive = false;
    powerHoldReady = false;
    powerHoldFadeActive = false;
    powerHoldLatchedRed = false;
    powerHoldMode = PowerHoldMode::None;
    powerHoldDurationMs = 0;
    powerHoldElapsedMs = 0;
}

void HermesXInterfaceModule::startPowerHoldFade(uint32_t now) {
    if (powerHoldFadeActive || powerHoldLatchedRed) {
        return;
    }

    powerHoldActive = false;
    powerHoldReady = true;
    powerHoldFadeActive = true;
    powerHoldLatchedRed = false;
    powerHoldFadeStartMs = now;
    if (powerHoldDurationMs > 0 && powerHoldElapsedMs < powerHoldDurationMs) {
        powerHoldElapsedMs = powerHoldDurationMs;
    }

    animState = LedAnimState::IDLE;
    flashOn = false;
    flashCount = 0;
}

void HermesXInterfaceModule::playStartupLEDAnimation(uint32_t color) {
    rgb.clear();
    rgb.show();
    delay(100);

    int center = NUM_LEDS / 2;

    for (int i = 0; i <= center; ++i) {
        if (center - i >= 0) rgb.setPixelColor(center - i, color);
        if (center + i < NUM_LEDS) rgb.setPixelColor(center + i, color);
        rgb.show();
        delay(80);
    }

    delay(300);

    for (int i = 0; i <= center; ++i) {
        if (center - i >= 0) rgb.setPixelColor(center - i, 0);
        if (center + i < NUM_LEDS) rgb.setPixelColor(center + i, 0);
        rgb.show();
        delay(60);
    }
}

void HermesXInterfaceModule::playShutdownEffect(uint32_t durationMs)
{
    const uint32_t effectiveDuration = durationMs ? durationMs : 700;

    pendingSuccessFeedback = false;
    animState = LedAnimState::IDLE;
    flashOn = false;
    flashCount = 0;

    powerHoldActive = false;
    powerHoldReady = false;
    powerHoldFadeActive = false;
    powerHoldLatchedRed = false;
    powerHoldMode = PowerHoldMode::None;
    powerHoldDurationMs = 0;
    powerHoldElapsedMs = 0;

    rgb.setBrightness(80);
    rgb.clear();
    rgb.show();

    music.stopTone();
    stopTone();

    performShutdownAnimation(effectiveDuration, rgb, kPowerHoldRedColor, &music);

    rgb.clear();
    rgb.show();

    disableVisibleOutputsCommon();
}

void HermesXInterfaceModule::renderLEDs()
{
    // 保�?你現?��?對�?介面；內?�由 updateLED() 實�?（已?��? animState 流�?
    updateLED();
}

void runPreDeepSleepHook(const SleepPreHookParams &params)
{
    uint32_t ms = params.suggested_duration_ms ? params.suggested_duration_ms : 700;
    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->playShutdownEffect(ms);
    } else {
        // 沒�? HermesX ?�環境�??��?底方�?
        fallbackShutdownEffect(ms);
    }
}


















