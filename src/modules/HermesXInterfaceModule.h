#pragma once
#ifndef MESHTASTIC_EXCLUDE_HERMESX
#define MESHTASTIC_EXCLUDE_HERMESX 0
#endif
#include "SinglePortModule.h"
#include "Observer.h"
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoPixel.h>
#include <stdint.h>
#include "meshtastic/mesh.pb.h"
#include "meshtastic/emergency.pb.h"
#include "MusicModule.h"
#include "concurrency/OSThread.h"

class OLEDDisplay;
class OLEDDisplayUiState;
struct HermesFaceRenderContext;
enum class HermesFaceMode : uint8_t;

typedef void (*HermesXFeedbackCallback)(int index, bool state);

extern HermesXFeedbackCallback hermesXCallback;

struct LedTheme {
    uint32_t colorSendPrimary;
    uint32_t colorSendSecondary;
    uint32_t colorReceivePrimary;
    uint32_t colorReceiveSecondary;
    uint32_t colorAck;
    uint32_t colorFailed;
    uint32_t colorIdleBreathBase;
    float breathBrightnessMin;
    float breathBrightnessMax;
};

enum class LedAnimState { IDLE, SEND_R2L, RECV_L2R, INFO2_L2R, ACK_FLASH, NACK_FLASH };

class HermesXInterfaceModule : public SinglePortModule, public Observable<const UIFrameEvent *>, public concurrency::OSThread, public Observer<uint32_t> {
public:
    enum class HermesButtonSource { Primary, Alt };
    enum class PowerHoldMode { None, PowerOn, PowerOff };

    HermesXInterfaceModule();
    static constexpr size_t kEmergencyTypeCount = 5;
    void setup();

    void handleButtonPress();

    Adafruit_NeoPixel rgb;
    MusicModule music;

    static HermesXInterfaceModule *instance;
    static void onLocalTextMessageSent();

    friend void HermesX_DrawFace(OLEDDisplay *display, int16_t x, int16_t y, HermesFaceMode mode);

    int onNotify(uint32_t fromNum) override;
    void handleExternalNotification(int index, bool state);
    void onTripleClick();
    void onDoubleClickWithin3s();
    void onEmergencyModeChanged(bool active);
    void onEmergencyTxResult(uint8_t type, bool ok);
    void registerRawButtonPress(HermesButtonSource source);
    void ensureEmergencyListeners();
    void playSOSFeedback();
    void playAckSuccess();
    void playNackFail();
    void handleAckNotification(bool success);
    void showEmergencyBanner(bool on, const __FlashStringHelper *text = nullptr, uint16_t color = 0);
    void beginEmergencyAction(meshtastic_EmergencyType type);
    void markEmergencyActionFailed(meshtastic_EmergencyType type);
    void requestEmergencyFocus();

    bool wantUIFrame() override;
    void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) override;
    Observable<const UIFrameEvent *> *getUIFrameObservable() override { return this; }

    void playSendFeedback();
    bool isEmergencyUiActive() const { return emergencyUiActive; }
    void playReceiveFeedback();
    void playSendFailedFeedback();
    void playSendSuccessFeedback();
    void playNodeInfoFeedback();

    void playStartupLEDAnimation(uint32_t color);
    void playShutdownEffect(uint32_t durationMs);   // << ?��???

    void renderLEDs();
    void updateLED();  // << ?��???

    void startSendAnim();
    void startReceiveAnim();
    void startInfoReceiveAnimTwoDots();
    void startAckFlash();
    void startNackFlash();
    void startPowerHoldAnimation(PowerHoldMode mode, uint32_t holdDurationMs);
    void updatePowerHoldAnimation(uint32_t elapsedMs);
    void stopPowerHoldAnimation(bool completed);
    void startPowerHoldFade(uint32_t now);

    void setLedTheme(const LedTheme& theme) {
        defaultTheme = theme;
        applyEmergencyTheme(emergencyThemeActive);
    }
    void onCannedMessageResult(bool ack, const String& nodeName);

    static inline uint8_t clamp8(int v) {
        return v < 0 ? 0 : (v > 255 ? 255 : v);
    }
    static inline float lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }
    static inline uint32_t scaleColor(uint32_t c, float s) {
        uint8_t r = clamp8(((c >> 16) & 0xFF) * s);
        uint8_t g = clamp8(((c >> 8) & 0xFF) * s);
        uint8_t b = clamp8((c & 0xFF) * s);
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }

private:
    void initDisplay();
    void initLED();
    void initRotary();

    void drawFace(const char* face, uint16_t color);
    void updateFace();

    enum class EmergencyEntryState : uint8_t { Idle, Pending, Success, Failed };

    void suppressEmergencyOverlay(uint32_t durationMs);
    void drawEmergencyOverlay(OLEDDisplay *display, const HermesFaceRenderContext &ctx, int16_t overlayHeight);
    int16_t getEmergencyOverlayHeight() const;
    size_t computeEmergencyVisibleCount(size_t entryCount, int selectedIndex) const;
    void renderFaceWithContext(const HermesFaceRenderContext &ctx, const char *face, uint16_t color, bool forceOverlay);
    void resetEmergencyMenuStates();
    void updateEmergencyMenuState(meshtastic_EmergencyType type, EmergencyEntryState state, uint32_t now);
    void decayEmergencyMenuStates(uint32_t now);
    int8_t menuIndexForEmergencyType(meshtastic_EmergencyType type) const;
    EmergencyEntryState getEmergencyStateForType(meshtastic_EmergencyType type) const;
    void applyEmergencyTheme(bool active);

    bool waitingForAck = false;
    bool emergencyBannerVisible = false;
    String emergencyBannerText;
    uint16_t emergencyBannerColor = 0;
    bool emergencyOverlayEnabled = true;
    uint32_t emergencyOverlayResumeMs = 0;
    EmergencyEntryState emergencyMenuStates[kEmergencyTypeCount] = {
        EmergencyEntryState::Idle, EmergencyEntryState::Idle,
        EmergencyEntryState::Idle, EmergencyEntryState::Idle,
        EmergencyEntryState::Idle
    };
    uint32_t emergencyMenuStateExpiry[kEmergencyTypeCount] = { 0, 0, 0, 0, 0 };
    bool hasPendingEmergencyType = false;
    meshtastic_EmergencyType pendingEmergencyType = meshtastic_EmergencyType_SOS;
    bool ackReceived = false;
    uint32_t waitingAckId = 0;
    uint32_t lastSentTime = 0;
    uint32_t lastSentId = 0;
    uint32_t lastSentRequestId = 0;
    uint32_t lastRawPressMs = 0;
    uint8_t rawPressCount = 0;
    bool safeWindowActive = false;
    uint8_t safePressCount = 0;
    uint32_t safeWindowDeadlineMs = 0;

    void sendText(NodeNum dest, ChannelIndex channel, const char *message, bool wantAck);
    void sendCannedMessage(const char* msg);
    void onPacketSent();
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &packet) override;

    void initBuzzer();
    void playTone(float freq, uint32_t duration_ms);
    void stopTone();
    void playStartupTone();
    void playReceiveTone();
    void playSendTone();
    void playFailedTone();

    bool wantPacket(const meshtastic_MeshPacket *p) override;
    bool pendingSuccessFeedback = false;
    uint32_t successFeedbackTime = 0;

    enum HermesFaceState { FACE_IDLE, FACE_RECEIVED, FACE_SENT, FACE_ERROR };
    HermesFaceState faceState = FACE_IDLE;
    HermesFaceState lastState = FACE_IDLE;

    // Breathing background (?��?)
    uint32_t lastBreathUpdate = 0;
    float breathPhase = 0.0f;
    float breathDelta = 0.02f;

    // LED animation state (?��?)
    LedAnimState animState = LedAnimState::IDLE;

    // Idle runner (?��?)
    uint8_t idlePos = 0;
    int8_t idleDir = 1;
    uint32_t lastIdleMove = 0;
    bool idleRunnerEnabled = true;
    uint16_t idleRunnerInterval = 160;
    uint16_t idleRunnerEdgeDwell = 260;
    bool idleRunnerVarSpeed = true;
    uint16_t idleRunnerMinInterval = 140;
    uint16_t idleRunnerMaxInterval = 240;

    // Event runner (?��?)
    uint8_t animPos = 0;
    int8_t animDir = 0;
    uint32_t lastAnimStep = 0;
    uint32_t eventColor = 0;

    // Flashing state (?��?)
    uint8_t flashCount = 0;
    bool flashOn = false;
    uint32_t lastFlashToggle = 0;

    // ?��?
    bool powerHoldActive = false;
    PowerHoldMode powerHoldMode = PowerHoldMode::None;
    uint32_t powerHoldDurationMs = 0;
    uint32_t powerHoldElapsedMs = 0;
    bool powerHoldReady = false;
    bool powerHoldFadeActive = false;
    bool powerHoldLatchedRed = false;
    uint32_t powerHoldFadeStartMs = 0;

    uint32_t toneStopTime = 0;

    LedTheme currentTheme{};
    LedTheme defaultTheme {
        .colorSendPrimary = 0xFFFFFF,
        .colorSendSecondary = 0x5050FF,
        .colorReceivePrimary = 0xFFFFFF,
        .colorReceiveSecondary = 0x5050FF,
        .colorAck = 0x00FF00,
        .colorFailed = 0xFF0000,
        .colorIdleBreathBase = 0xFF5000,
        .breathBrightnessMin = 0.5f,
        .breathBrightnessMax = 1.0f
    };
    LedTheme emergencyTheme {
        .colorSendPrimary = 0xFFFFFF,
        .colorSendSecondary = 0x5050FF,
        .colorReceivePrimary = 0xFFFFFF,
        .colorReceiveSecondary = 0x5050FF,
        .colorAck = 0x07E0,
        .colorFailed = 0xF800,
        .colorIdleBreathBase = 0xF800,
        .breathBrightnessMin = 0.55f,
        .breathBrightnessMax = 1.0f
    };
    bool emergencyThemeActive = false;
    bool emergencyUiActive = false;
    bool emergencyListenersRegistered = false;

    int32_t runOnce() override;
};

extern HermesXInterfaceModule* globalHermes;








