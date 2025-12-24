#pragma once
#ifndef MESHTASTIC_EXCLUDE_HERMESX
#define MESHTASTIC_EXCLUDE_HERMESX 0
#endif
#if !MESHTASTIC_EXCLUDE_HERMESX

#include "SinglePortModule.h"
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoPixel.h>
#include <stdint.h>
#include "meshtastic/mesh.pb.h"
#include "MusicModule.h"
#include "concurrency/OSThread.h"

class OLEDDisplay;
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

enum class LedAnimState { IDLE, SEND_L2R, RECV_R2L, INFO2_R2L, ACK_FLASH, NACK_FLASH };

// 集中式 LED 動畫列舉
enum class LEDAnimation : uint8_t {
    None,
    PowerHoldProgress,
    PowerHoldFade,
    PowerHoldLatchedRed,
    ShutdownEffect,
    StartupEffect,
    AckFlash,
    NackFlash,
    SendL2R,
    ReceiveR2L,
    InfoR2L,
    IdleBreath,
    IdleRunner
};

struct LEDState {
    LEDAnimation activeAnimation = LEDAnimation::None;
    uint32_t animStartTime = 0;
    bool isRunning = false;
};

class HermesXInterfaceModule : public SinglePortModule, public concurrency::OSThread, public Observer<uint32_t> {
public:
    enum class HermesButtonSource { Primary, Alt };
    enum class PowerHoldMode { None, PowerOn, PowerOff };

    HermesXInterfaceModule();
    void setup();

    void handleButtonPress();

    Adafruit_NeoPixel rgb;
    MusicModule music;

    // 集中式 LED 狀態管理
    LEDState ledState;
    bool useCentralLedManager = false; // 預設先關閉，遷移時可切換

    static HermesXInterfaceModule *instance;
    static void onLocalTextMessageSent();

    friend void HermesX_DrawFace(OLEDDisplay *display, int16_t x, int16_t y, HermesFaceMode mode);

    int onNotify(uint32_t fromNum) override;
    void handleExternalNotification(int index, bool state);
    void onTripleClick();
    void onDoubleClickWithin3s();
    void onEmergencyModeChanged(bool active);
    void registerRawButtonPress(HermesButtonSource source);
    void playSOSFeedback();
    void playAckSuccess();
    void playNackFail();
    bool isEmergencyUiActive() const;
    void showEmergencyBanner(bool on, const __FlashStringHelper *text = nullptr, uint16_t color = 0,
                             uint32_t durationMs = 0);

    void playSendFeedback();
    void playReceiveFeedback();
    void playSendFailedFeedback();
    void playSendSuccessFeedback();
    void playNodeInfoFeedback();

    // 集中式 LED 控制 API（後續逐步遷移）
    void startLEDAnimation(LEDAnimation anim);
    void stopLEDAnimation(LEDAnimation anim);
    void tickLEDAnimation(uint32_t now);
    LEDAnimation selectActiveAnimation() const;

    // 中央渲染 helpers（逐步遷移舊邏輯用）
    void renderPowerHoldProgress(uint32_t now);
    void renderPowerHoldFade(uint32_t now);
    void renderPowerHoldLatchedRed();
    void renderAckFlash(uint32_t now);
    void renderNackFlash(uint32_t now);
    void renderSendAnim(uint32_t now);
    void renderReceiveAnim(uint32_t now);
    void renderInfoAnim(uint32_t now);
    void renderIdleBreath(uint32_t now);
    void renderIdleRunner(uint32_t now);
    void renderStartupEffect();
    void renderShutdownEffect();

    // legacy 畫燈流程拆出 helper，集中模式調用避免遞迴
    void legacyStartupAnimation(uint32_t color);
    void legacyShutdownAnimation(uint32_t durationMs);
    void forceAllLedsOff(); // 關機前強制關閉所有 LED 狀態
    // 立即啟動 PowerHold 進度（按下當下）
    void beginPowerHoldProgress();

    void playStartupLEDAnimation(uint32_t color);
    void playShutdownEffect(uint32_t durationMs);   // << ?��???

    void renderLEDs();
    void updateLED();  // << ?��???

    void applyRoleOutputPolicy();

    void startSendAnim();
    void startReceiveAnim();
    void startInfoReceiveAnimTwoDots();
    void startAckFlash();
    void startNackFlash();
    void startPowerHoldAnimation(PowerHoldMode mode, uint32_t holdDurationMs);
    void updatePowerHoldAnimation(uint32_t elapsedMs);
    void stopPowerHoldAnimation(bool completed);
    void forceStopPowerHoldAnimation();
    LEDAnimation getCurrentAnimation() const;
    void startPowerHoldFade(uint32_t now);

    static void deferStartupVisuals();
    static void setPowerHoldReady(bool ready);
    static bool isPowerHoldReady();

    void setLedTheme(const LedTheme& theme) { currentTheme = theme; }
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

    bool waitingForAck = false;
    bool emergencyBannerVisible = false;
    String emergencyBannerText;
    uint16_t emergencyBannerColor = 0;
    uint32_t emergencyBannerHideDeadline = 0;
    bool ackReceived = false;
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

    // 中央管理旗標
    bool ackFlashActive = false;
    bool nackFlashActive = false;
    bool sendAnimActive = false;
    bool recvAnimActive = false;
    bool infoAnimActive = false;
    bool startupEffectActive = false;
    bool shutdownEffectActive = false;

    // ?��?
    bool powerHoldActive = false;
    PowerHoldMode powerHoldMode = PowerHoldMode::None;
    uint32_t powerHoldDurationMs = 0;
    uint32_t powerHoldElapsedMs = 0;
    bool powerHoldReady = false;
    bool forceStopPowerOff = false;
    bool powerHoldFadeActive = false;
    bool powerHoldLatchedRed = false;
    uint32_t powerHoldFadeStartMs = 0;

    uint32_t toneStopTime = 0;
    bool outputsDisabled = false;

    LedTheme currentTheme {
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

    int32_t runOnce() override;
};

extern HermesXInterfaceModule* globalHermes;

#else

class HermesXInterfaceModule;
extern HermesXInterfaceModule* globalHermes;

#endif // !MESHTASTIC_EXCLUDE_HERMESX
