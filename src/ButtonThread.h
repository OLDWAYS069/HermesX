#pragma once

#include "OneButton.h"
#include "concurrency/OSThread.h"
#include "configuration.h"

#ifndef BUTTON_CLICK_MS
#define BUTTON_CLICK_MS 250
#endif

#ifndef BUTTON_LONGPRESS_MS
#define BUTTON_LONGPRESS_MS 5000
#endif

#ifndef BUTTON_TOUCH_MS
#define BUTTON_TOUCH_MS 400
#endif

class ButtonThread : public concurrency::OSThread
{
  public:
    static const uint32_t c_holdOffTime = 30000; // hold off 30s after boot

    enum ButtonEventType {
        BUTTON_EVENT_NONE,
        BUTTON_EVENT_PRESSED,
        BUTTON_EVENT_PRESSED_SCREEN,
        BUTTON_EVENT_DOUBLE_PRESSED,
        BUTTON_EVENT_MULTI_PRESSED,
        BUTTON_EVENT_LONG_PRESSED,
        BUTTON_EVENT_LONG_RELEASED,
        BUTTON_EVENT_TOUCH_LONG_PRESSED,
    };

    ButtonThread();
    int32_t runOnce() override;
    void attachButtonInterrupts();
    void detachButtonInterrupts();
    void storeClickCount();
#if !MESHTASTIC_EXCLUDE_HERMESX
    void updatePowerHoldAnimation();
#if defined(HERMESX_GUARD_POWER_ANIMATIONS)
    bool handleBootHold();
    void disableBootHold();
#endif
    static void requireReleaseBeforeLongPress();
    static bool isReleaseRequiredForLongPress();
    static void clearHoldAnimationState();
    static bool isHoldButtonPressed();
#endif
    bool isBuzzing() { return buzzer_flag; }
    void setScreenFlag(bool flag) { screen_flag = flag; }
    bool getScreenFlag() { return screen_flag; }

    // Disconnect and reconnect interrupts for light sleep
#ifdef ARCH_ESP32
    int beforeLightSleep(void *unused);
    int afterLightSleep(esp_sleep_wakeup_cause_t cause);
#endif
    static void resetLongPressGates();
  private:
#if !MESHTASTIC_EXCLUDE_HERMESX && (defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) || defined(BUTTON_PIN_ALT) || defined(BUTTON_PIN_TOUCH))
    enum class HoldAnimationMode { None, PowerOn, PowerOff };
    HoldAnimationMode holdAnimationMode = HoldAnimationMode::None;
    bool holdAnimationActive = false;
    bool holdAnimationStarted = false; // 已在 LED 模組啟動
    uint32_t holdAnimationLastMs = 0;
    uint32_t holdAnimationBaseMs = 0;
    static constexpr uint32_t kWakeHoldMs = 3000; // 要按住多久才放行開機
    bool wakeAnimStarted = false;
    bool wakeHoldActive = false;
    bool wakeTriggered = false;
    uint32_t wakeHoldStart = 0;
    HoldAnimationMode resolveHoldMode() const;
#if defined(HERMESX_GUARD_POWER_ANIMATIONS)
    bool bootHoldArmed = false;
    bool bootHoldPressActive = false;
    bool bootHoldWaitingForPress = false;
    uint32_t bootHoldStartMs = 0;
    uint32_t bootHoldArmedAtMs = 0;
    bool bootHoldAnimStarted = false;
    uint8_t bootHoldDotsPhase = 0;
    static constexpr uint32_t kBootHoldIdleSleepMs = 5000;
    static constexpr uint32_t kBootHoldDotsIntervalMs = 500;
    static bool holdOffBypassed;
#endif
#endif
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN)
    static OneButton userButton; // Static - accessed from an interrupt
#endif
#ifdef BUTTON_PIN_ALT
    OneButton userButtonAlt;
#endif
#ifdef BUTTON_PIN_TOUCH
    OneButton userButtonTouch;
#endif

#ifdef ARCH_ESP32
    // Get notified when lightsleep begins and ends
    CallbackObserver<ButtonThread, void *> lsObserver =
        CallbackObserver<ButtonThread, void *>(this, &ButtonThread::beforeLightSleep);
    CallbackObserver<ButtonThread, esp_sleep_wakeup_cause_t> lsEndObserver =
        CallbackObserver<ButtonThread, esp_sleep_wakeup_cause_t>(this, &ButtonThread::afterLightSleep);
#endif

    // set during IRQ
    static volatile ButtonEventType btnEvent;
    bool buzzer_flag = false;
    bool screen_flag = true;

    // Store click count during callback, for later use
    volatile int multipressClickCount = 0;

    static void wakeOnIrq(int irq, int mode);

    static void sendAdHocPosition();
    static void switchPage();

    // IRQ callbacks
    static void userButtonPressed() { btnEvent = BUTTON_EVENT_PRESSED; }
    static void userButtonPressedScreen() { btnEvent = BUTTON_EVENT_PRESSED_SCREEN; }
    static void userButtonDoublePressed() { btnEvent = BUTTON_EVENT_DOUBLE_PRESSED; }
    static void userButtonMultiPressed(void *callerThread); // Retrieve click count from non-static Onebutton while still valid
    static void userButtonPressedLongStart();
    static void userButtonPressedLongStop();
    static void touchPressedLongStart() { btnEvent = BUTTON_EVENT_TOUCH_LONG_PRESSED; }

    void resetLongPressState();
    void updateLongGateTracking();
    void resetWakeHoldGate();
    void processWakeHoldGate();
};

extern ButtonThread *buttonThread;
