// HermesXInterfaceModule.cpp - Refactored without TinyScheduler

#include "HermesXInterfaceModule.h"
#include "EmergencyAdaptiveModule.h"
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
    initLED();

    HERMESX_LOG_DEBUG("constroct");
    HermesEmergencyUi::setup();
    playStartupLEDAnimation(currentTheme.colorIdleBreathBase);
}

void HermesXInterfaceModule::setup()
{
    if (emergencyModule) {
        emergencyModule->addModeListener([this](bool active) { onEmergencyModeChanged(active); });
        emergencyModule->addTxResultListener([this](uint8_t /*type*/, bool ok) {
            if (ok) {
                showEmergencyBanner(true, F("SOS ACTIVE"), 0x07E0);
            } else {
                showEmergencyBanner(true, F("SOS FAILED"), 0xF800);
            }
        });
        onEmergencyModeChanged(emergencyModule->isEmergencyActive());
    }
}


void HermesXInterfaceModule::handleButtonPress()
{
    HERMESX_LOG_INFO("HermesX interface button pressed");
    playReceiveFeedback();
}


void HermesXInterfaceModule::drawFace(const char* face, uint16_t color) {
    const char *faceText = face;
    uint16_t faceColor = color;

    HermesFaceRenderContext ctx{};
    if (!HermesX_TryGetFaceRenderContext(ctx) || ctx.display == nullptr) {
        HERMESX_LOG_INFO("HermesXInterfaceModule drawFace fallback; no display ctx for face=%s", faceText ? faceText : "");
        return;
    }

    const bool showEmergencyOverlay = emergencyBannerVisible && emergencyBannerText.length() &&
        ctx.mode == HermesFaceMode::Neutral;
    const uint16_t bannerColor = emergencyBannerColor ? emergencyBannerColor : static_cast<uint16_t>(0xF800);

    if ((!faceText || !*faceText) && showEmergencyOverlay) {
        faceText = emergencyBannerText.c_str();
    }

    if (!faceText || !*faceText) {
        return;
    }

    if (showEmergencyOverlay && faceColor == 0) {
        faceColor = bannerColor;
    }

    OLEDDisplay *display = ctx.display;
    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();

    const uint8_t *faceFont = FONT_LARGE;
    auto applyFaceFont = [&](const char *text, int16_t &faceWidth, int16_t &faceHeight) {
        const int16_t availableWidth = width > 8 ? width - 8 : width;
        const int16_t availableHeight = height > 8 ? height - 8 : height;

        display->setFont(FONT_LARGE);
        faceFont = FONT_LARGE;
        faceWidth = display->getStringWidth(text);
        faceHeight = FONT_HEIGHT_LARGE;
        if (faceWidth > availableWidth || faceHeight > availableHeight) {
            display->setFont(FONT_MEDIUM);
            faceFont = FONT_MEDIUM;
            faceWidth = display->getStringWidth(text);
            faceHeight = FONT_HEIGHT_MEDIUM;
        }
        if (faceWidth > availableWidth || faceHeight > availableHeight) {
            display->setFont(FONT_SMALL);
            faceFont = FONT_SMALL;
            faceWidth = display->getStringWidth(text);
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

    display->drawString(drawX, drawY, faceText);

    if (showEmergencyOverlay) {
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->setFont(FONT_SMALL);
#if defined(USE_EINK)
        display->setColor(EINK_BLACK);
#else
        const bool highlightOverlay = bannerColor != 0;
        const OLEDDISPLAY_COLOR overlayColor = highlightOverlay ? OLEDDISPLAY_COLOR::WHITE
                                                                : (faceColor ? OLEDDISPLAY_COLOR::WHITE : OLEDDISPLAY_COLOR::BLACK);
        display->setColor(overlayColor);
#endif
        int16_t bannerY = ctx.originY + height - FONT_HEIGHT_SMALL - 2;
        if (bannerY < ctx.originY) {
            bannerY = ctx.originY;
        }
        display->drawString(centerX, bannerY, emergencyBannerText.c_str());
        display->setFont(faceFont);
        display->setTextAlignment(TEXT_ALIGN_CENTER);
    }

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
                HERMESX_LOG_WARN("Routing NAK error=%d req_id=0x%08x id=%u from=%x to=%x",
                    decoded.error_reason,
                    packet.decoded.request_id,
                    packet.id,
                    packet.from,
                    packet.to);
            } else {
                HERMESX_LOG_DEBUG("Routing ACK observed req_id=0x%08x id=%u", packet.decoded.request_id, packet.id);
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
    if (!emergencyModule) {
        playSendFailedFeedback();
        return;
    }

    if (emergencyModule->sendSOS()) {
        playSOSFeedback();
    } else {
        playSendFailedFeedback();
    }
}

void HermesXInterfaceModule::onDoubleClickWithin3s()
{
    if (!emergencyModule) {
        playSendFailedFeedback();
        return;
    }

    if (emergencyModule->sendSafe()) {
        playAckSuccess();
    } else {
        playSendFailedFeedback();
    }
}

void HermesXInterfaceModule::onEmergencyTxResult(uint8_t /*type*/, bool ok)
{
    if (ok) {
        showEmergencyBanner(true, F("SOS ACTIVE"), 0x07E0);
    } else {
        showEmergencyBanner(true, F("SOS FAILED"), 0xF800);
    }
}

void HermesXInterfaceModule::onEmergencyModeChanged(bool active)
{
    showEmergencyBanner(active);
}


void HermesXInterfaceModule::playSOSFeedback()
{
    music.playSendSound();
    startSendAnim();
    showEmergencyBanner(true);
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
    } else {
        ackReceived = false;
        pendingSuccessFeedback = false;
        HERMESX_LOG_WARN("ACK notification failed");
        playSendFailedFeedback();
    }
}

void HermesXInterfaceModule::showEmergencyBanner(bool on, const __FlashStringHelper *text, uint16_t color)
{
    emergencyBannerVisible = on;
    if (on) {
        if (text) {
            emergencyBannerText = text;
        } else {
            emergencyBannerText = F("SOS");
        }
        emergencyBannerColor = color ? color : 0xF800;
    } else {
        emergencyBannerText = "";
        emergencyBannerColor = 0;
    }
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










