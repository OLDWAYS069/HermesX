// HermesXInterfaceModule.cpp - Refactored without TinyScheduler

#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_HERMESX

#include "HermesXInterfaceModule.h"

#include "mesh/MeshService.h"
#include "mesh/NodeDB.h"
#include "mesh/Channels.h"
#include "mesh/Router.h"
#include "meshtastic/mesh.pb.h"
#include "modules/CannedMessageModule.h"
#include "SinglePortModule.h"
#include "pb.h"
#include "pb_encode.h"
#include "HermesXPacketUtils.h"

#include "modules/LighthouseModule.h"
#include "modules/HermesEmergencyState.h"
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
#include "mesh/HermesPortnums.h"
#include "graphics/ScreenFonts.h"
#include "modules/HermesXPowerGuard.h"
#include "graphics/fonts/HermesX_zh/HermesX_CN12.h"

#include "ReliableRouter.h"
#include "Default.h"
#include "MeshTypes.h"
#include <algorithm>
#include <cstring>
#include "mesh-pb-constants.h"
#include "modules/NodeInfoModule.h"
#include "modules/RoutingModule.h"

#include "pb_decode.h"
#include "mesh/mesh-pb-constants.h"
#include "mesh/generated/meshtastic/module_config.pb.h"

#define PIN_LED 6
#define NUM_LEDS 8

#define BUZZER_PIN 17

#ifndef TFT_BACKLIGHT_ON
#define TFT_BACKLIGHT_ON HIGH
#endif

extern graphics::Screen *screen;

namespace
{
void drawMixedCentered(OLEDDisplay &display, int16_t centerX, int16_t y, const String &text, int lineHeight)
{
    // drawMixed uses current text alignment; force left so manual centering is accurate
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    const char *lineStart = text.c_str();
    int lineIndex = 0;
    while (true) {
        const char *newline = std::strchr(lineStart, '\n');
        const size_t length = newline ? static_cast<size_t>(newline - lineStart) : std::strlen(lineStart);
        String lineString(lineStart, length);
        const int width =
            graphics::HermesX_zh::stringAdvance(lineString.c_str(), graphics::HermesX_zh::GLYPH_WIDTH, &display);
        const int drawX = centerX - (width / 2);
        graphics::HermesX_zh::drawMixed(display, drawX, y + lineIndex * lineHeight, lineString.c_str(),
                                        graphics::HermesX_zh::GLYPH_WIDTH, lineHeight, nullptr);
        if (!newline)
            break;
        lineStart = newline + 1;
        ++lineIndex;
    }
}

void drawMixedCentered(OLEDDisplay &display, int16_t centerX, int16_t y, const char *text, int lineHeight)
{
    if (!text)
        return;
    drawMixedCentered(display, centerX, y, String(text), lineHeight);
}

int mixedStringWidth(OLEDDisplay &display, const char *text)
{
    return graphics::HermesX_zh::stringAdvance(text, graphics::HermesX_zh::GLYPH_WIDTH, &display);
}

__attribute__((unused)) int mixedStringWidth(OLEDDisplay &display, const String &text)
{
    return mixedStringWidth(display, text.c_str());
}

} // namespace

namespace {
constexpr uint32_t kDefaultShutdownDurationMs = 700;
constexpr uint32_t kShutdownAnimationStepMs = 20;

constexpr uint32_t kPowerHoldFadeDurationMs = 1200;
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

    fallbackStrip.fill(kPowerHoldRedColor);
    fallbackStrip.show();

    disableVisibleOutputsCommon();

    performShutdownAnimation(durationMs, fallbackStrip, kPowerHoldRedColor, &fallbackMusic);
}
}



HermesXInterfaceModule* globalHermes = nullptr;
HermesXInterfaceModule *HermesXInterfaceModule::instance = nullptr;
uint32_t safeTimeout = 5000;

HermesXFeedbackCallback hermesXCallback = nullptr;

void HermesXInterfaceModule::deferStartupVisuals()
{
#if defined(HERMESX_GUARD_POWER_ANIMATIONS)
    HermesXPowerGuard::setPowerHoldReady(false);
#endif
    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->powerHoldReady = false;
    }
}

void HermesXInterfaceModule::setPowerHoldReady(bool ready)
{
#if defined(HERMESX_GUARD_POWER_ANIMATIONS)
    HermesXPowerGuard::setPowerHoldReady(ready);
#endif
    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->powerHoldReady = ready;
    }
}

bool HermesXInterfaceModule::isPowerHoldReady()
{
#if defined(HERMESX_GUARD_POWER_ANIMATIONS)
    return HermesXPowerGuard::isPowerHoldReady();
#else
    return true;
#endif
}

HermesXInterfaceModule::HermesXInterfaceModule()
  : SinglePortModule("hermesx", meshtastic_PortNum_PRIVATE_APP),
    OSThread("hermesTask", 500),
    rgb(NUM_LEDS, PIN_LED, NEO_GRB + NEO_KHZ800),
    music(BUZZER_PIN)
{
    globalHermes = this;
    HermesXInterfaceModule::instance = this;
#if defined(HERMESX_GUARD_POWER_ANIMATIONS)
    powerHoldReady = HermesXPowerGuard::isPowerHoldReady();
#endif
    observe(&service->fromNumChanged);
    isPromiscuous = true;
    loopbackOk = true;
    initLED();

    HERMESX_LOG_DEBUG("constroct");
#if defined(HERMESX_GUARD_POWER_ANIMATIONS)
    if (HermesXPowerGuard::startupVisualsAllowed()) {
        playStartupLEDAnimation(currentTheme.colorIdleBreathBase);
    }
#else
    playStartupLEDAnimation(currentTheme.colorIdleBreathBase);
#endif
}

void HermesXInterfaceModule::setup()
{
    // Setup code here
}


void HermesXInterfaceModule::handleButtonPress()
{

}


void HermesXInterfaceModule::registerRawButtonPress(HermesButtonSource /*source*/)//目前任何輸入都可
{
    const uint32_t now = millis();

    // Expire the SAFE double-press window if the timeout has elapsed.
    if (safeWindowActive && now > safeWindowDeadlineMs) {
        safeWindowActive = false;
        safePressCount = 0;
        safeWindowDeadlineMs = 0;
    }

    constexpr uint32_t kMultiPressWindowMs = 1200;
    if (lastRawPressMs == 0 || now - lastRawPressMs > kMultiPressWindowMs) {
        rawPressCount = 0;
    }

    lastRawPressMs = now;
    rawPressCount++;

    if (rawPressCount >= 3) {
        rawPressCount = 0;
        safeWindowActive = false;
        safePressCount = 0;
        safeWindowDeadlineMs = 0;
        onTripleClick();
        return;
    }

    constexpr uint32_t kSafeWindowMs = 3000;
    if (!safeWindowActive) {
        safeWindowActive = true;
        safePressCount = 1;
        safeWindowDeadlineMs = now + kSafeWindowMs;
    } else {
        safePressCount++;
        if (safePressCount >= 2 && now <= safeWindowDeadlineMs) {
            safeWindowActive = false;
            safePressCount = 0;
            safeWindowDeadlineMs = 0;
            onDoubleClickWithin3s();
        }
    }
}


void HermesXInterfaceModule::drawFace(const char* face, uint16_t color) {
    const char *faceText = face;
    uint16_t faceColor = color;
    if (emergencyBannerVisible) {
        faceText = emergencyBannerText.length() ? emergencyBannerText.c_str() : "SOS";
        if (emergencyBannerColor) {
            faceColor = emergencyBannerColor;
        }
    }

    if (!faceText || !*faceText) {
        return;
    }

    HermesFaceRenderContext ctx{};
    if (!HermesX_TryGetFaceRenderContext(ctx) || ctx.display == nullptr) {
        HERMESX_LOG_INFO("HermesXInterfaceModule drawFace fallback; no display ctx for face=%s", faceText);
        return;
    }

    OLEDDisplay *display = ctx.display;
    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();

    auto applyFaceFont = [&](const char *text, int16_t &faceWidth, int16_t &faceHeight) {
        const int16_t availableWidth = width > 8 ? width - 8 : width;
        const int16_t availableHeight = height > 8 ? height - 8 : height;

        display->setFont(FONT_LARGE);
        faceWidth = mixedStringWidth(*display, text);
        faceHeight = FONT_HEIGHT_LARGE;
        if (faceWidth > availableWidth || faceHeight > availableHeight) {
            display->setFont(FONT_MEDIUM);
            faceWidth = mixedStringWidth(*display, text);
            faceHeight = FONT_HEIGHT_MEDIUM;
        }
        if (faceWidth > availableWidth || faceHeight > availableHeight) {
            display->setFont(FONT_SMALL);
            faceWidth = mixedStringWidth(*display, text);
            faceHeight = FONT_HEIGHT_SMALL;
        }
    };

    int16_t faceWidth = 0;
    int16_t faceHeight = 0;
    applyFaceFont(faceText, faceWidth, faceHeight);

    display->setTextAlignment(TEXT_ALIGN_CENTER);

    const int16_t drawX = ctx.originX + width / 2;
    int16_t drawY = ctx.originY + (height - faceHeight) / 2;
    if (drawY < ctx.originY) {
        drawY = ctx.originY;
    }

    const int16_t centerX = ctx.originX + width / 2;
    const int16_t centerY = ctx.originY + height / 2;
    const int16_t faceMaxDimension = faceWidth > faceHeight ? faceWidth : faceHeight;
    constexpr int16_t kCirclePadding = 6;
    int16_t circleRadius = faceMaxDimension / 2 + kCirclePadding;
    const int16_t minDisplayDimension = width < height ? width : height;
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
    const int16_t rectY = ctx.originY + (height - rectHeight) / 2;

    display->fillRect(rectX, rectY, rectWidth, rectHeight);
    display->setColor(EINK_WHITE);
#endif

    if (circleRadius > 0) {
        display->drawCircle(centerX, centerY, circleRadius);
    }

    drawMixedCentered(*display, drawX, drawY, faceText, faceHeight);

    display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(faceColor ? OLEDDISPLAY_COLOR::WHITE : OLEDDISPLAY_COLOR::BLACK);
#endif
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
        const bool awaitingSafe = HermesIsEmergencyAwaitingSafe();
        const uint32_t activeColor = awaitingSafe ? currentTheme.colorAck : kPowerHoldRedColor;
        const uint32_t idleColor = awaitingSafe ? 0x000000 : currentTheme.colorIdleBreathBase;

        for (int step = 0; step < NUM_LEDS; ++step) {
            // power-off: red top->bottom; SAFE: green bottom->top
            const int ledIndex = awaitingSafe ? step : (NUM_LEDS - 1 - step);
            const float threshold = static_cast<float>(step + 1) / static_cast<float>(NUM_LEDS);

            bool segmentLatched = false;
            switch (powerHoldMode) {
            case PowerHoldMode::PowerOn:
                segmentLatched = progress >= threshold;
                break;
            case PowerHoldMode::PowerOff:
                segmentLatched = progress >= threshold;
                break;
            default:
                segmentLatched = false;
                break;
            }

            const uint32_t color = segmentLatched ? activeColor : idleColor;
            rgb.setPixelColor(ledIndex, color);
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
        const bool awaitingSafe = HermesIsEmergencyAwaitingSafe();
        rgb.fill(awaitingSafe ? currentTheme.colorAck : kPowerHoldRedColor);
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
    const bool awaitingSafe = HermesIsEmergencyAwaitingSafe();
    uint32_t bgColor = scaleColor(awaitingSafe ? kPowerHoldRedColor : currentTheme.colorIdleBreathBase, bgScale);
    rgb.fill(bgColor);

    // === 事件?�畫（送出/?�收/?��?資�?�?==
    if (animState == LedAnimState::SEND_L2R || animState == LedAnimState::RECV_R2L || animState == LedAnimState::INFO2_R2L) {
        const uint16_t stepInterval = 80;
        if (now - lastAnimStep >= stepInterval) {
            lastAnimStep = now;
            int next = animPos + animDir;
            if (next < 0 || next >= NUM_LEDS) {
                // ?��??��? idle runner ?�起點、方?�設�?
                if (animState == LedAnimState::SEND_L2R) {
                    idlePos = NUM_LEDS - 1;
                    idleDir = -1;
                } else {
                    idlePos = 0;
                    idleDir = 1;
                }
                animState = LedAnimState::IDLE;
            } else {
                animPos = next;
            }
        }

        if (animState != LedAnimState::IDLE) {
            rgb.setPixelColor(animPos, eventColor);
            if (animState == LedAnimState::INFO2_R2L) {
                int second = animPos + animDir;
                if (second >= 0 && second < NUM_LEDS) {
                    rgb.setPixelColor(second, eventColor);
                }
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
            if (decoded.error_reason != meshtastic_Routing_Error_NONE) {
                playSendFailedFeedback();
                waitingForAck = false;
                HERMESX_LOG_WARN("Routing NAK error=%d req_id=0x%08x id=%u from=%x to=%x",
                    decoded.error_reason,
                    packet.decoded.request_id,
                    packet.id,
                    packet.from,
                    packet.to);
                if (safeSendPending && packet.decoded.request_id == lastSafeRequestId) {
                    HERMESX_LOG_WARN("SAFE NAK req_id=0x%08x keep EM awaiting", packet.decoded.request_id);
                    safeSendPending = false;
                }
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
#if !MESHTASTIC_EXCLUDE_LIGHTHOUSE
                    if (safeSendPending && packet.decoded.request_id == lastSafeRequestId) {
                        safeSendPending = false;
                        HermesClearEmergencyAwaitingSafe();
                        if (lighthouseModule) {
                            lighthouseModule->exitEmergencyMode();
                        }
                    }
#endif
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

    // === ?�止 tone ?�放 ===
    if (toneStopTime && now >= toneStopTime) {
        stopTone();
        toneStopTime = 0;
    }

    // === Timeout: 等�? ACK 超�? 3 秒�?視為失�? ===
    if (waitingForAck && (now - lastSentTime > 30000)) {
        waitingForAck = false;
        ackReceived = false;
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

    if (emergencyBannerVisible && emergencyBannerHideDeadline && now >= emergencyBannerHideDeadline) {
        showEmergencyBanner(false);
    }

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

bool HermesXInterfaceModule::isEmergencyUiActive() const
{
    return HermesIsEmergencyAwaitingSafe() || emergencyBannerVisible;
}

void HermesXInterfaceModule::handleExternalNotification(int index, bool state) {
    if (index == 0 && state) playReceiveFeedback();
    if (index == 2 && state) playSendFeedback();
    if (index == 3 && state) playAckSuccess();
    if (index == 4 && state) playNackFail();
    HERMESX_LOG_DEBUG("handleExternalNotification!");
}



void HermesXInterfaceModule::playSendFeedback() {
    music.playSendSound();
    startSendAnim();
    HERMESX_LOG_INFO("Send feedback triggered");
}

void HermesXInterfaceModule::playReceiveFeedback() {
    music.playReceiveSound();
    startReceiveAnim();
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
    HERMESX_LOG_INFO("Failed feedback triggered");
}

void HermesXInterfaceModule::onTripleClick()
{
    ///come back on beta_0.3.0
}

void HermesXInterfaceModule::onDoubleClickWithin3s()
{
    // reserved
}

void HermesXInterfaceModule::onEmergencyModeChanged(bool active)
{
    ///will comeback on beta_0.3.0
}


void HermesXInterfaceModule::playSOSFeedback()
{
    music.playSendSound();
    startSendAnim();
}

void HermesXInterfaceModule::playAckSuccess()
{
    music.playSuccessSound();
    startAckFlash();
}

void HermesXInterfaceModule::playNackFail()
{
    music.playFailedSound();
    startNackFlash();
}

void HermesXInterfaceModule::showEmergencyBanner(bool on, const __FlashStringHelper *text, uint16_t color,
                                                 uint32_t durationMs)
{
    if (on) {
        emergencyBannerVisible = true;
        emergencyBannerText = text ? String(text) : String();
        emergencyBannerColor = color;
        emergencyBannerHideDeadline = durationMs ? millis() + durationMs : 0;
    } else {
        emergencyBannerVisible = false;
        emergencyBannerText = "";
        emergencyBannerColor = 0;
        emergencyBannerHideDeadline = 0;
    }
}

void HermesXInterfaceModule::startSendAnim() {
    animState = LedAnimState::SEND_L2R;
    animPos = 0;
    animDir = 1;
    eventColor = currentTheme.colorSendPrimary;
    lastAnimStep = millis();
}

void HermesXInterfaceModule::startReceiveAnim() {
    animState = LedAnimState::RECV_R2L;
    animPos = NUM_LEDS - 1;
    animDir = -1;
    eventColor = currentTheme.colorReceivePrimary;
    lastAnimStep = millis();
}

void HermesXInterfaceModule::startInfoReceiveAnimTwoDots() {
    animState = LedAnimState::INFO2_R2L;
    animPos = NUM_LEDS - 1;
    animDir = -1;
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
    HermesXInterfaceModule::setPowerHoldReady(false);
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
        // EM SAFE long-press path: trigger SAFE when hold completes and we are awaiting SAFE.
        if (HermesIsEmergencyAwaitingSafe()) {
            meshtastic_MeshPacket *p = allocDataPacket();
            if (p) {
                const char *payload = "SAFE";
                p->to = NODENUM_BROADCAST;
                p->channel = 0;
                p->want_ack = true;
                p->decoded.portnum = PORTNUM_HERMESX_EMERGENCY;
                p->decoded.payload.size = strlen(payload);
                memcpy(p->decoded.payload.bytes, payload, p->decoded.payload.size);
                service->sendToMesh(p, RX_SRC_LOCAL, true);
                HERMESX_LOG_INFO("Send EM SAFE on long-press");
                startSendAnim();
                eventColor = currentTheme.colorAck;
                lastSafeRequestId = p->id;
                safeSendPending = true;
            } else {
                HERMESX_LOG_WARN("Failed to alloc SAFE packet");
            }
        }
        return;
    }

    powerHoldActive = false;
    HermesXInterfaceModule::setPowerHoldReady(false);
    powerHoldFadeActive = false;
    powerHoldLatchedRed = false;
    powerHoldMode = PowerHoldMode::None;
    powerHoldDurationMs = 0;
    powerHoldElapsedMs = 0;
}

void HermesXInterfaceModule::startPowerHoldFade(uint32_t now) {
#if defined(HERMESX_GUARD_POWER_ANIMATIONS)
    if (!HermesXPowerGuard::isPowerHoldReady()) {
        return;
    }
#endif
    if (powerHoldFadeActive || powerHoldLatchedRed) {
        return;
    }

    powerHoldActive = false;
    HermesXInterfaceModule::setPowerHoldReady(true);
    powerHoldFadeActive = false;
    powerHoldLatchedRed = true;
    powerHoldFadeStartMs = now;
    if (powerHoldDurationMs > 0 && powerHoldElapsedMs < powerHoldDurationMs) {
        powerHoldElapsedMs = powerHoldDurationMs;
    }

    animState = LedAnimState::IDLE;
    flashOn = false;
    flashCount = 0;

    rgb.fill(kPowerHoldRedColor);
    rgb.show();
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

#if defined(HERMESX_GUARD_POWER_ANIMATIONS)
    if (HermesXPowerGuard::consumeShutdownAnimationSuppression()) {
        HermesXPowerGuard::logBootHoldEvent("BootHold: shutdown animation suppressed");
        pendingSuccessFeedback = false;
        animState = LedAnimState::IDLE;
        flashOn = false;
        flashCount = 0;

        powerHoldActive = false;
        HermesXInterfaceModule::setPowerHoldReady(false);
        powerHoldFadeActive = false;
        powerHoldLatchedRed = false;
        powerHoldMode = PowerHoldMode::None;
        powerHoldDurationMs = 0;
        powerHoldElapsedMs = 0;

        rgb.clear();
        rgb.show();

        music.stopTone();
        stopTone();

        disableVisibleOutputsCommon();
        return;
    }
#endif

    pendingSuccessFeedback = false;
    animState = LedAnimState::IDLE;
    flashOn = false;
    flashCount = 0;

    powerHoldActive = false;
    HermesXInterfaceModule::setPowerHoldReady(false);
    powerHoldFadeActive = false;
    powerHoldLatchedRed = false;
    powerHoldMode = PowerHoldMode::None;
    powerHoldDurationMs = 0;
    powerHoldElapsedMs = 0;

    rgb.setBrightness(80);
    rgb.fill(kPowerHoldRedColor);
    rgb.show();

    music.stopTone();
    stopTone();

    disableVisibleOutputsCommon();

    performShutdownAnimation(effectiveDuration, rgb, kPowerHoldRedColor, &music);
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

#endif // !MESHTASTIC_EXCLUDE_HERMESX
