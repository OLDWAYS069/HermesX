#include "ButtonThread.h"

#include "configuration.h"
#if !MESHTASTIC_EXCLUDE_GPS
#include "GPS.h"
#endif
#include "MeshService.h"
#include "PowerFSM.h"
#include "RadioLibInterface.h"
#include "buzz.h"
#include "main.h"
#include "modules/ExternalNotificationModule.h"
#include "power.h"
#include "sleep.h"
#ifdef ARCH_PORTDUINO
#include "platform/portduino/PortduinoGlue.h"
#endif
#if !MESHTASTIC_EXCLUDE_HERMESX
#include "modules/HermesXInterfaceModule.h"
#include "modules/HermesXPowerGuard.h"
#endif

#define DEBUG_BUTTONS 0
#if DEBUG_BUTTONS
#define LOG_BUTTON(...) LOG_DEBUG(__VA_ARGS__)
#else
#define LOG_BUTTON(...)
#endif

using namespace concurrency;

ButtonThread *buttonThread; // Declared extern in header
volatile ButtonThread::ButtonEventType ButtonThread::btnEvent = ButtonThread::BUTTON_EVENT_NONE;

#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN)
OneButton ButtonThread::userButton; // Get reference to static member
#endif
#if !MESHTASTIC_EXCLUDE_HERMESX && (defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) ||           \
                                    defined(BUTTON_PIN_ALT) || defined(BUTTON_PIN_TOUCH)) && defined(HERMESX_GUARD_POWER_ANIMATIONS)
bool ButtonThread::holdOffBypassed = false;
#endif
ButtonThread::ButtonThread() : OSThread("Button")
{
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN)

#if defined(ARCH_PORTDUINO)
    if (settingsMap.count(user) != 0 && settingsMap[user] != RADIOLIB_NC) {
        this->userButton = OneButton(settingsMap[user], true, true);
        LOG_DEBUG("Use GPIO%02d for button", settingsMap[user]);
    }
#elif defined(BUTTON_PIN)
#if !defined(USERPREFS_BUTTON_PIN)
    int pin = config.device.button_gpio ? config.device.button_gpio : BUTTON_PIN;           // Resolved button pin
#endif
#ifdef USERPREFS_BUTTON_PIN
    int pin = config.device.button_gpio ? config.device.button_gpio : USERPREFS_BUTTON_PIN; // Resolved button pin
#endif
    bool activeLow = true;
    bool pullupActive = true;
#if defined(HELTEC_CAPSULE_SENSOR_V3) || defined(HELTEC_SENSOR_HUB)
    activeLow = false;
    pullupActive = false;
#elif defined(BUTTON_ACTIVE_LOW)
    activeLow = BUTTON_ACTIVE_LOW;
    pullupActive = BUTTON_ACTIVE_PULLUP;
#else
    activeLow = true;
    pullupActive = true;
#endif
    this->userButton = OneButton(pin, activeLow, pullupActive);
    LOG_DEBUG("Use GPIO%02d for button", pin);

#if !MESHTASTIC_EXCLUDE_HERMESX && defined(HERMESX_GUARD_POWER_ANIMATIONS)
    if (HermesXPowerGuard::guardEnabled()) {
        bool pressedAtBoot = activeLow ? (digitalRead(pin) == LOW) : (digitalRead(pin) == HIGH);
        if (HermesXPowerGuard::registerInitialButtonState(pressedAtBoot)) {
            bootHoldArmed = true;
            bootHoldPressActive = pressedAtBoot;
            bootHoldWaitingForPress = !pressedAtBoot;
            bootHoldStartMs = pressedAtBoot ? millis() : 0;
            holdOffBypassed = true;
            LOG_BUTTON("BootHold: armed (usb=%d, pressed=%d)", HermesXPowerGuard::usbPresentAtBoot() ? 1 : 0,
                       pressedAtBoot ? 1 : 0);
            HermesXInterfaceModule::setPowerHoldReady(false);
        }
    }
#endif
#endif

#ifdef INPUT_PULLUP_SENSE
    // Some platforms (nrf52) have a SENSE variant which allows wake from sleep - override what OneButton did
#ifdef BUTTON_SENSE_TYPE
    pinMode(pin, BUTTON_SENSE_TYPE);
#else
    pinMode(pin, INPUT_PULLUP_SENSE);
#endif
#endif

#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN)
    userButton.attachClick(userButtonPressed);
    userButton.setClickMs(BUTTON_CLICK_MS);
    userButton.setPressMs(BUTTON_LONGPRESS_MS);
    userButton.setDebounceMs(1);
    userButton.attachDoubleClick(userButtonDoublePressed);
    userButton.attachMultiClick(userButtonMultiPressed, this); // Reference to instance: get click count from non-static OneButton
#if !defined(T_DECK) &&                                                                                                          \
    !defined(                                                                                                                    \
        ELECROW_ThinkNode_M2) // T-Deck immediately wakes up after shutdown, Thinknode M2 has this on the smaller ALT button
    userButton.attachLongPressStart(userButtonPressedLongStart);
    userButton.attachLongPressStop(userButtonPressedLongStop);
#endif
#endif

#ifdef BUTTON_PIN_ALT
#if defined(ELECROW_ThinkNode_M2)
    this->userButtonAlt = OneButton(BUTTON_PIN_ALT, false, false);
#else
    this->userButtonAlt = OneButton(BUTTON_PIN_ALT, true, true);
#endif
#ifdef INPUT_PULLUP_SENSE
    // Some platforms (nrf52) have a SENSE variant which allows wake from sleep - override what OneButton did
    pinMode(BUTTON_PIN_ALT, INPUT_PULLUP_SENSE);
#endif
    userButtonAlt.attachClick(userButtonPressedScreen);
    userButtonAlt.setClickMs(BUTTON_CLICK_MS);
    userButtonAlt.setPressMs(BUTTON_LONGPRESS_MS);
    userButtonAlt.setDebounceMs(1);
    userButtonAlt.attachLongPressStart(userButtonPressedLongStart);
    userButtonAlt.attachLongPressStop(userButtonPressedLongStop);
#endif

#ifdef BUTTON_PIN_TOUCH
    userButtonTouch = OneButton(BUTTON_PIN_TOUCH, true, true);
    userButtonTouch.setPressMs(BUTTON_TOUCH_MS);
    userButtonTouch.attachLongPressStart(touchPressedLongStart); // Better handling with longpress than click?
#endif

#ifdef ARCH_ESP32
    // Register callbacks for before and after lightsleep
    // Used to detach and reattach interrupts
    lsObserver.observe(&notifyLightSleep);
    lsEndObserver.observe(&notifyLightSleepEnd);
#endif

    attachButtonInterrupts();
    resetLongPressState();
#endif
}

void ButtonThread::switchPage()
{
#ifdef BUTTON_PIN
#if !defined(USERPREFS_BUTTON_PIN)
    if (((config.device.button_gpio ? config.device.button_gpio : BUTTON_PIN) !=
         moduleConfig.canned_message.inputbroker_pin_press) ||
        !(moduleConfig.canned_message.updown1_enabled || moduleConfig.canned_message.rotary1_enabled) ||
        !moduleConfig.canned_message.enabled) {
        powerFSM.trigger(EVENT_PRESS);
    }
#endif
#if defined(USERPREFS_BUTTON_PIN)
    if (((config.device.button_gpio ? config.device.button_gpio : USERPREFS_BUTTON_PIN) !=
         moduleConfig.canned_message.inputbroker_pin_press) ||
        !(moduleConfig.canned_message.updown1_enabled || moduleConfig.canned_message.rotary1_enabled) ||
        !moduleConfig.canned_message.enabled) {
        powerFSM.trigger(EVENT_PRESS);
    }
#endif

#endif
#if defined(ARCH_PORTDUINO)
    if ((settingsMap.count(user) != 0 && settingsMap[user] != RADIOLIB_NC) &&
            (settingsMap[user] != moduleConfig.canned_message.inputbroker_pin_press) ||
        !moduleConfig.canned_message.enabled) {
        powerFSM.trigger(EVENT_PRESS);
    }
#endif
}

void ButtonThread::sendAdHocPosition()
{
    service->refreshLocalMeshNode();
    auto sentPosition = service->trySendPosition(NODENUM_BROADCAST, true);
    if (screen) {
        if (sentPosition)
            screen->print("Sent ad-hoc position\n");
        else
            screen->print("Sent ad-hoc nodeinfo\n");
        screen->forceDisplay(true); // Force a new UI frame, then force an EInk update
    }
}

int32_t ButtonThread::runOnce()
{
    // If the button is pressed we suppress CPU sleep until release
    canSleep = true; // Assume we should not keep the board awake

#if defined(BUTTON_PIN) || defined(USERPREFS_BUTTON_PIN)
    bool userButtonTicked = false;
#endif

#if !MESHTASTIC_EXCLUDE_HERMESX && defined(HERMESX_GUARD_POWER_ANIMATIONS) &&                                                 \
    (defined(BUTTON_PIN) || defined(USERPREFS_BUTTON_PIN))
    if (bootHoldArmed) {
        userButton.tick();
        userButtonTicked = true;
        if (handleBootHold()) {
            return 50;
        }
    }
#endif

#if defined(BUTTON_PIN) || defined(USERPREFS_BUTTON_PIN)
    if (!userButtonTicked) {
        userButton.tick();
    }
    canSleep &= userButton.isIdle();
#elif defined(ARCH_PORTDUINO)
    if (settingsMap.count(user) != 0 && settingsMap[user] != RADIOLIB_NC) {
        userButton.tick();
        canSleep &= userButton.isIdle();
    }
#endif
#ifdef BUTTON_PIN_ALT
    userButtonAlt.tick();
    canSleep &= userButtonAlt.isIdle();
#endif
#ifdef BUTTON_PIN_TOUCH
    userButtonTouch.tick();
    canSleep &= userButtonTouch.isIdle();
#endif
#if !MESHTASTIC_EXCLUDE_HERMESX
    updatePowerHoldAnimation();
#endif
    updateLongGateTracking();
    if (btnEvent != BUTTON_EVENT_NONE) {
        switch (btnEvent) {
        case BUTTON_EVENT_PRESSED: {
            LOG_BUTTON("press!");
            // If a nag notification is running, stop it and prevent other actions
            if (moduleConfig.external_notification.enabled && (externalNotificationModule->nagCycleCutoff != UINT32_MAX)) {
                externalNotificationModule->stopNow();
                break;
            }
#ifdef ELECROW_ThinkNode_M1
            sendAdHocPosition();
            break;
#endif
            switchPage();
            break;
        }

        case BUTTON_EVENT_PRESSED_SCREEN: {
            LOG_BUTTON("AltPress!");
#ifdef ELECROW_ThinkNode_M1
            // If a nag notification is running, stop it and prevent other actions
            if (moduleConfig.external_notification.enabled && (externalNotificationModule->nagCycleCutoff != UINT32_MAX)) {
                externalNotificationModule->stopNow();
                break;
            }
            switchPage();
            break;
#endif
            // Rotary short-press is handled by InputBroker (e.g. canned messages); no additional side-effects here.
            break;
        }

        case BUTTON_EVENT_DOUBLE_PRESSED: {
            LOG_BUTTON("Double press!");
#ifdef ELECROW_ThinkNode_M1
            digitalWrite(PIN_EINK_EN, digitalRead(PIN_EINK_EN) == LOW);
            break;
#endif
            sendAdHocPosition();
            break;
        }

        case BUTTON_EVENT_MULTI_PRESSED: {
            LOG_BUTTON("Mulitipress! %hux", multipressClickCount);
            switch (multipressClickCount) {
#if HAS_GPS && !defined(ELECROW_ThinkNode_M1)
            // 3 clicks: toggle GPS
            case 3:
                if (!config.device.disable_triple_click && (gps != nullptr)) {
                    gps->toggleGpsMode();
                    if (screen)
                        screen->forceDisplay(true); // Force a new UI frame, then force an EInk update
                }
                break;
#elif defined(ELECROW_ThinkNode_M1) || defined(ELECROW_ThinkNode_M2)
            case 3:
                LOG_INFO("3 clicks: toggle buzzer");
                buzzer_flag = !buzzer_flag;
                if (!buzzer_flag)
                    noTone(PIN_BUZZER);
                break;

#endif

#if defined(USE_EINK) && defined(PIN_EINK_EN) && !defined(ELECROW_ThinkNode_M1) // i.e. T-Echo
            // 4 clicks: toggle backlight
            case 4:
                digitalWrite(PIN_EINK_EN, digitalRead(PIN_EINK_EN) == LOW);
                break;
#endif
#if !MESHTASTIC_EXCLUDE_SCREEN && HAS_SCREEN
            // 5 clicks: start accelerometer/magenetometer calibration for 30 seconds
            case 5:
                if (accelerometerThread) {
                    accelerometerThread->calibrate(30);
                }
                break;
            // 6 clicks: start accelerometer/magenetometer calibration for 60 seconds
            case 6:
                if (accelerometerThread) {
                    accelerometerThread->calibrate(60);
                }
                break;
#endif
            // No valid multipress action
            default:
                break;
            } // end switch: click count

            break;
        } // end multipress event

        case BUTTON_EVENT_LONG_PRESSED: {
            LOG_BUTTON("Long press!");
            powerFSM.trigger(EVENT_PRESS);
            if (screen) {
                screen->startAlert("Shutting down...");
            }
            playBeep();
            break;
        }

        // Do actual shutdown when button released, otherwise the button release
        // may wake the board immediatedly.
        case BUTTON_EVENT_LONG_RELEASED: {
            LOG_INFO("Shutdown from long press");
#if !MESHTASTIC_EXCLUDE_HERMESX
            if (HermesXInterfaceModule::instance) {
                HermesXInterfaceModule::instance->playShutdownEffect(BUTTON_LONGPRESS_MS);
            }
#endif
            playShutdownMelody();
            delay(3000);
            power->shutdown();
            break;
        }

#ifdef BUTTON_PIN_TOUCH
        case BUTTON_EVENT_TOUCH_LONG_PRESSED: {
            LOG_BUTTON("Touch press!");
            // Ignore if: no screen
            if (!screen)
                break;

#ifdef TTGO_T_ECHO
            // Ignore if: TX in progress
            // Uncommon T-Echo hardware bug, LoRa TX triggers touch button
            if (!RadioLibInterface::instance || RadioLibInterface::instance->isSending())
                break;
#endif

            // Wake if asleep
            if (powerFSM.getState() == &stateDARK)
                powerFSM.trigger(EVENT_PRESS);

            // Update display (legacy behaviour)
            screen->forceDisplay();
            break;
        }
#endif // BUTTON_PIN_TOUCH

        default:
            break;
        }
        btnEvent = BUTTON_EVENT_NONE;
    }

    return 50;
}

/*
 * Attach (or re-attach) hardware interrupts for buttons
 * Public method. Used outside class when waking from MCU sleep
 */
void ButtonThread::attachButtonInterrupts()
{
#if defined(ARCH_PORTDUINO)
    if (settingsMap.count(user) != 0 && settingsMap[user] != RADIOLIB_NC)
        wakeOnIrq(settingsMap[user], FALLING);
#elif defined(BUTTON_PIN)
    // Interrupt for user button, during normal use. Improves responsiveness.
    attachInterrupt(
#if !defined(USERPREFS_BUTTON_PIN)
        config.device.button_gpio ? config.device.button_gpio : BUTTON_PIN,
#endif
#if defined(USERPREFS_BUTTON_PIN)
        config.device.button_gpio ? config.device.button_gpio : USERPREFS_BUTTON_PIN,
#endif
        []() {
            ButtonThread::userButton.tick();
            runASAP = true;
            BaseType_t higherWake = 0;
            mainDelay.interruptFromISR(&higherWake);
        },
        CHANGE);
#endif

#ifdef BUTTON_PIN_ALT
#ifdef ELECROW_ThinkNode_M2
    wakeOnIrq(BUTTON_PIN_ALT, RISING);
#else
    wakeOnIrq(BUTTON_PIN_ALT, FALLING);
#endif
#endif

#ifdef BUTTON_PIN_TOUCH
    wakeOnIrq(BUTTON_PIN_TOUCH, FALLING);
#endif
}

/*
 * Detach the "normal" button interrupts.
 * Public method. Used before attaching a "wake-on-button" interrupt for MCU sleep
 */
void ButtonThread::detachButtonInterrupts()
{
#if defined(ARCH_PORTDUINO)
    if (settingsMap.count(user) != 0 && settingsMap[user] != RADIOLIB_NC)
        detachInterrupt(settingsMap[user]);
#elif defined(BUTTON_PIN)
#if !defined(USERPREFS_BUTTON_PIN)
    detachInterrupt(config.device.button_gpio ? config.device.button_gpio : BUTTON_PIN);
#endif
#if defined(USERPREFS_BUTTON_PIN)
    detachInterrupt(config.device.button_gpio ? config.device.button_gpio : USERPREFS_BUTTON_PIN);
#endif
#endif

#ifdef BUTTON_PIN_ALT
    detachInterrupt(BUTTON_PIN_ALT);
#endif

#ifdef BUTTON_PIN_TOUCH
    detachInterrupt(BUTTON_PIN_TOUCH);
#endif
}

#ifdef ARCH_ESP32

// Detach our class' interrupts before lightsleep
// Allows sleep.cpp to configure its own interrupts, which wake the device on user-button press
int ButtonThread::beforeLightSleep(void *unused)
{
    detachButtonInterrupts();
    return 0; // Indicates success
}

// Reconfigure our interrupts
// Our class' interrupts were disconnected during sleep, to allow the user button to wake the device from sleep
int ButtonThread::afterLightSleep(esp_sleep_wakeup_cause_t cause)
{
    attachButtonInterrupts();
    return 0; // Indicates success
}

#endif

/**
 * Watch a GPIO and if we get an IRQ, wake the main thread.
 * Use to add wake on button press
 */
void ButtonThread::wakeOnIrq(int irq, int mode)
{
    attachInterrupt(
        irq,
        [] {
            BaseType_t higherWake = 0;
            mainDelay.interruptFromISR(&higherWake);
            runASAP = true;
        },
        FALLING);
}

// Static callback
void ButtonThread::userButtonMultiPressed(void *callerThread)
{
    // Grab click count from non-static button, while the info is still valid
    ButtonThread *thread = (ButtonThread *)callerThread;
    thread->storeClickCount();

    // Then handle later, in the usual way
    btnEvent = BUTTON_EVENT_MULTI_PRESSED;
}

// Non-static method, runs during callback. Grabs info while still valid
void ButtonThread::storeClickCount()
{
#if defined(BUTTON_PIN) || defined(USERPREFS_BUTTON_PIN)
    multipressClickCount = userButton.getNumberClicks();
#endif
}

#if !MESHTASTIC_EXCLUDE_HERMESX

ButtonThread::HoldAnimationMode ButtonThread::resolveHoldMode() const
{
#if EXCLUDE_POWER_FSM
    return HoldAnimationMode::PowerOff;
#else
    auto *currentState = powerFSM.getState();
    if (currentState == nullptr) {
        return HoldAnimationMode::PowerOff;
    }
    if (currentState == &stateDARK) {
        return HoldAnimationMode::PowerOn;
    }
    return HoldAnimationMode::PowerOff;
#endif
}

#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) || defined(BUTTON_PIN_ALT) ||                \
    defined(BUTTON_PIN_TOUCH)
static bool s_releaseSeen = false;
static bool s_longGateArmed = false;
static bool s_longEventPending = false;
static uint32_t s_resumeGraceUntil = 0;
static uint32_t s_lastStableChangeMs = 0;
static bool s_lastStablePressed = false;
static constexpr uint32_t kResumeGraceMs = 1200;
static constexpr uint32_t kReleaseDebounceMs = 80;

void ButtonThread::resetLongPressState()
{
    s_releaseSeen = false;
    s_longGateArmed = false;
    s_longEventPending = false;
    s_resumeGraceUntil = millis() + kResumeGraceMs;
    s_lastStableChangeMs = 0;
    s_lastStablePressed = false;
}
#else
void ButtonThread::resetLongPressState() {}
#endif

#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) || defined(BUTTON_PIN_ALT) ||                \
    defined(BUTTON_PIN_TOUCH)
void ButtonThread::updateLongGateTracking()
{
    const uint32_t now = millis();
    if (s_resumeGraceUntil == 0) {
        s_resumeGraceUntil = now + kResumeGraceMs;
    }

    bool anyPressed = false;
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN)
    if (userButton.pin() >= 0 && userButton.debouncedValue()) {
        anyPressed = true;
    }
#endif
#ifdef BUTTON_PIN_ALT
    if (!anyPressed && userButtonAlt.pin() >= 0 && userButtonAlt.debouncedValue()) {
        anyPressed = true;
    }
#endif
#ifdef BUTTON_PIN_TOUCH
    if (!anyPressed && userButtonTouch.pin() >= 0 && userButtonTouch.debouncedValue()) {
        anyPressed = true;
    }
#endif

    if (s_lastStableChangeMs == 0) {
        s_lastStableChangeMs = now;
        s_lastStablePressed = anyPressed;
    }

    if (anyPressed != s_lastStablePressed) {
        s_lastStablePressed = anyPressed;
        s_lastStableChangeMs = now;
    }

    if (!anyPressed && (now - s_lastStableChangeMs >= kReleaseDebounceMs)) {
        s_releaseSeen = true;
    }
}
#else
void ButtonThread::updateLongGateTracking() {}
#endif

void ButtonThread::resetLongPressGates()
{
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) || defined(BUTTON_PIN_ALT) ||                \
    defined(BUTTON_PIN_TOUCH)
    if (buttonThread) {
        buttonThread->resetLongPressState();
    } else {
        s_releaseSeen = false;
        s_longGateArmed = false;
        s_longEventPending = false;
        s_resumeGraceUntil = millis() + kResumeGraceMs;
        s_lastStableChangeMs = 0;
        s_lastStablePressed = false;
    }
#endif
}

void ButtonThread::updatePowerHoldAnimation()
{
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) || defined(BUTTON_PIN_ALT) || defined(BUTTON_PIN_TOUCH)
    auto *interfaceModule = HermesXInterfaceModule::instance;
    if (!interfaceModule) {
        if (holdAnimationActive) {
            holdAnimationActive = false;
            holdAnimationMode = HoldAnimationMode::None;
            holdAnimationLastMs = 0;
        }
        return;
    }

    const uint32_t holdDurationMs = BUTTON_LONGPRESS_MS ? BUTTON_LONGPRESS_MS : 1;
    bool anyPressed = false;
    uint32_t pressedMs = 0;

#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN)
    if (userButton.pin() >= 0 && userButton.debouncedValue()) {
        anyPressed = true;
        uint32_t candidate = userButton.getPressedMs();
        if (candidate > pressedMs) {
            pressedMs = candidate;
        }
    }
#endif
#ifdef BUTTON_PIN_ALT
    if (userButtonAlt.pin() >= 0 && userButtonAlt.debouncedValue()) {
        anyPressed = true;
        uint32_t candidate = userButtonAlt.getPressedMs();
        if (candidate > pressedMs) {
            pressedMs = candidate;
        }
    }
#endif
#ifdef BUTTON_PIN_TOUCH
    if (userButtonTouch.pin() >= 0 && userButtonTouch.debouncedValue()) {
        anyPressed = true;
        uint32_t candidate = userButtonTouch.getPressedMs();
        if (candidate > pressedMs) {
            pressedMs = candidate;
        }
    }
#endif

    if (anyPressed) {
        holdAnimationLastMs = pressedMs;

        if (!holdAnimationActive) {
            holdAnimationMode = resolveHoldMode();
            if (holdAnimationMode != HoldAnimationMode::None) {
                holdAnimationActive = true;
                const auto moduleMode = (holdAnimationMode == HoldAnimationMode::PowerOn)
                                            ? HermesXInterfaceModule::PowerHoldMode::PowerOn
                                            : HermesXInterfaceModule::PowerHoldMode::PowerOff;
                interfaceModule->startPowerHoldAnimation(moduleMode, holdDurationMs);
            }
        }

        if (holdAnimationActive) {
            interfaceModule->updatePowerHoldAnimation(holdAnimationLastMs);
        }

        return;
    }

    if (holdAnimationActive) {
        const bool completed = holdAnimationLastMs >= holdDurationMs;
        interfaceModule->stopPowerHoldAnimation(completed);
    }

    holdAnimationActive = false;
    holdAnimationMode = HoldAnimationMode::None;
    holdAnimationLastMs = 0;
#endif
}

#endif

#if !MESHTASTIC_EXCLUDE_HERMESX && defined(HERMESX_GUARD_POWER_ANIMATIONS) &&                                                   \
    (defined(BUTTON_PIN) || defined(USERPREFS_BUTTON_PIN))
bool ButtonThread::handleBootHold()
{
    if (!bootHoldArmed)
        return false;

    bool currentlyPressed = userButton.debouncedValue();

    if (bootHoldWaitingForPress) {
        if (!currentlyPressed)
            return true;

        bootHoldWaitingForPress = false;
        bootHoldPressActive = true;
        HermesXPowerGuard::markPressDetected();
        bootHoldStartMs = millis();
        LOG_BUTTON("BootHold: press detected");
    }

    if (!bootHoldPressActive && currentlyPressed) {
        bootHoldPressActive = true;
        bootHoldStartMs = millis();
    }

    if (!currentlyPressed) {
        uint32_t pressedMs = userButton.getPressedMs();
        if (!bootHoldPressActive || pressedMs < BUTTON_LONGPRESS_MS) {
            HermesXPowerGuard::markBootHoldAborted();
            HermesXPowerGuard::requestShutdownAnimationSuppression();
            LOG_BUTTON("BootHold: aborted (sleep)");
            bootHoldArmed = false;
            bootHoldPressActive = false;
            bootHoldWaitingForPress = false;
            holdOffBypassed = false;
            HermesXInterfaceModule::setPowerHoldReady(false);
            power->shutdown();
            return true;
        }
        bootHoldArmed = false;
        bootHoldPressActive = false;
        bootHoldWaitingForPress = false;
        holdOffBypassed = false;
        return false;
    }

    uint32_t pressedMs = userButton.getPressedMs();

    if (pressedMs >= BUTTON_LONGPRESS_MS) {
        HermesXPowerGuard::markBootHoldCommitted();
        LOG_BUTTON("BootHold: committed (ready)");
        bootHoldArmed = false;
        bootHoldPressActive = false;
        bootHoldWaitingForPress = false;
        holdOffBypassed = false;
        HermesXInterfaceModule::setPowerHoldReady(true);
        return false;
    }

    HermesXPowerGuard::setPowerHoldReady(false);
    return true;
}
#endif

void ButtonThread::userButtonPressedLongStart()
{
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) || defined(BUTTON_PIN_ALT)
    uint32_t pressedMs = 0;
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN)
    pressedMs = userButton.getPressedMs();
#endif
#ifdef BUTTON_PIN_ALT
    uint32_t altPressed = userButtonAlt.getPressedMs();
    if (altPressed > pressedMs)
        pressedMs = altPressed;
#endif
    if (pressedMs + 20 < BUTTON_LONGPRESS_MS)
        return;

    const uint32_t now = millis();
#if !MESHTASTIC_EXCLUDE_HERMESX && defined(HERMESX_GUARD_POWER_ANIMATIONS) &&                                                   \
    (defined(BUTTON_PIN) || defined(USERPREFS_BUTTON_PIN) || defined(ARCH_PORTDUINO))
    bool holdAllowed = holdOffBypassed || (now > c_holdOffTime);
#else
    bool holdAllowed = (now > c_holdOffTime);
#endif

    if (!holdAllowed)
        return;

    if (!s_releaseSeen)
        return;

    if (s_resumeGraceUntil && (now < s_resumeGraceUntil))
        return;

    if (s_longGateArmed)
        return;

    s_longGateArmed = true;
    s_longEventPending = true;
    s_releaseSeen = false;
    btnEvent = BUTTON_EVENT_LONG_PRESSED;
#endif
}

void ButtonThread::userButtonPressedLongStop()
{
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) || defined(BUTTON_PIN_ALT)
    const uint32_t now = millis();
    bool holdAllowed =
#if !MESHTASTIC_EXCLUDE_HERMESX && defined(HERMESX_GUARD_POWER_ANIMATIONS) &&                                                   \
    (defined(BUTTON_PIN) || defined(USERPREFS_BUTTON_PIN) || defined(ARCH_PORTDUINO))
        (holdOffBypassed || (now > c_holdOffTime));
#else
        (now > c_holdOffTime);
#endif

    if (!s_longGateArmed) {
        s_longEventPending = false;
        return;
    }

    s_longGateArmed = false;

    if (!holdAllowed) {
        s_longEventPending = false;
        return;
    }

    if (s_resumeGraceUntil && (now < s_resumeGraceUntil)) {
        s_longEventPending = false;
        return;
    }

    s_longEventPending = false;
    btnEvent = BUTTON_EVENT_LONG_RELEASED;
#else
    bool holdAllowed = (millis() > c_holdOffTime);
    if (!holdAllowed)
        return;

    btnEvent = BUTTON_EVENT_LONG_RELEASED;
#endif
}
