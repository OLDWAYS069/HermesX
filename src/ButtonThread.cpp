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
#include "modules/CannedMessageModule.h"
#endif

#include <inttypes.h>
#include <cstdio>
#include <cstdio>

#define DEBUG_BUTTONS 0
#if DEBUG_BUTTONS
#define LOG_BUTTON(...) LOG_DEBUG(__VA_ARGS__)
#else
#define LOG_BUTTON(...)
#endif

using namespace concurrency;

// 長按／關機動畫協調（集中式 LED 管理）
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) || defined(BUTTON_PIN_ALT) ||                \
    defined(BUTTON_PIN_TOUCH)
static bool s_releaseSeen = false;
static bool s_requireReleaseBeforeLongPress = false;
static bool s_longGateArmed = false;
static bool s_longEventPending = false;
static bool s_longPressFromAlt = false;
static bool s_longPressSuppressed = false;
static uint32_t s_longPressSuppressUntil = 0;
static uint32_t s_resumeGraceUntil = 0;
static uint32_t s_lastStableChangeMs = 0;
static bool s_lastStablePressed = false;
static uint32_t s_longStartMillis = 0;
static uint32_t s_holdPressStartMs = 0;
static bool isLongPressSuppressedNow();

static bool s_shutdownAnimArmed = false;
static uint32_t s_shutdownDeadlineMs = 0;
static constexpr uint32_t kShutdownAnimDurationMs = BUTTON_LONGPRESS_MS ? BUTTON_LONGPRESS_MS : 1000;
static constexpr uint32_t kShutdownMessageMinMs = 1500;
static constexpr uint32_t kShutdownMessageEarlyMs = 4000;
static constexpr uint32_t kResumeGraceMs = 1200;
static constexpr uint32_t kReleaseDebounceMs = 80;
static constexpr uint32_t kRotaryLongPressDelayMs = 1000;
static constexpr const char *kShutdownMessage = "拜拜～下次見！";
// 罐頭訊息選單：長按約 1 秒退出
static bool s_exitCannedHold = false;
static uint32_t s_exitCannedStartMs = 0;
#if !MESHTASTIC_EXCLUDE_HERMESX
// 逐格視覺放慢倍率（需等全部轉紅才觸發關機）
static constexpr float kPowerHoldVisualStretch = 1.0f;
#endif
#if !MESHTASTIC_EXCLUDE_HERMESX
static bool s_deferShutdownUi = false;
static bool s_shutdownSequenceStarted = false;
static bool s_shutdownMessageShown = false;

static void startShutdownVisuals(uint32_t holdDurationMs)
{
    if (s_shutdownSequenceStarted)
        return;

    auto *interfaceModule = HermesXInterfaceModule::instance;

    // 先讓逐格動畫補滿並關閉，以免阻擋後續 ShutdownEffect
    if (interfaceModule) {
        interfaceModule->updatePowerHoldAnimation(holdDurationMs);
        interfaceModule->forceStopPowerHoldAnimation();
    }
    ButtonThread::clearHoldAnimationState();

    if (screen) {
        screen->startHermesXAlert(kShutdownMessage);
    }
    playBeep();
    s_shutdownMessageShown = true;

    s_shutdownAnimArmed = true;
    s_shutdownDeadlineMs = millis() + kShutdownAnimDurationMs;
    const uint32_t minDeadline = millis() + kShutdownMessageMinMs;
    if (s_shutdownDeadlineMs < minDeadline) {
        s_shutdownDeadlineMs = minDeadline;
    }
    if (interfaceModule) {
        interfaceModule->startLEDAnimation(LEDAnimation::ShutdownEffect);
    }

    s_shutdownSequenceStarted = true;
}
#endif
#endif

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
            bootHoldArmedAtMs = HermesXPowerGuard::wokeFromSleep() ? millis() : 0;
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
        bool gate = handleBootHold();
        // 只在深睡/待機時才阻塞其他流程；已開機就讓其他邏輯繼續（避免第一次長按延遲）
        if (powerFSM.getState() == &stateDARK && gate) {
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
#if !MESHTASTIC_EXCLUDE_HERMESX && (defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) ||          \
                                    defined(BUTTON_PIN_ALT) || defined(BUTTON_PIN_TOUCH))
    processWakeHoldGate();
#endif
#if !MESHTASTIC_EXCLUDE_HERMESX
    updatePowerHoldAnimation();
#endif
    // 罐頭選單退出：按鍵放開則取消計時
#if !MESHTASTIC_EXCLUDE_HERMESX
    if (s_exitCannedHold) {
        if (!isHoldButtonPressed()) {
            s_exitCannedHold = false;
        } else if (millis() - s_exitCannedStartMs >= 1000) {
            if (cannedMessageModule && cannedMessageModule->getRunState() != CANNED_MESSAGE_RUN_STATE_INACTIVE) {
                cannedMessageModule->exitMenu();
            }
            s_exitCannedHold = false;
        }
    }
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
#if !MESHTASTIC_EXCLUDE_HERMESX && (defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) ||          \
                                    defined(BUTTON_PIN_ALT) || defined(BUTTON_PIN_TOUCH))
            // 改為先進入喚醒閘門，按住達 kWakeHoldMs 才真正觸發開機
            wakeHoldActive = true;
            wakeTriggered = false;
            wakeHoldStart = millis();

            // 在集中式 LED 管理下，按下當下就啟動逐格進度（短放則在 stop 時重置）
#if !MESHTASTIC_EXCLUDE_HERMESX
            if (HermesXInterfaceModule::instance) {
                buttonThread->holdAnimationMode = buttonThread->resolveHoldMode();
                buttonThread->holdAnimationActive = true;
                buttonThread->holdAnimationLastMs = 0;
                buttonThread->holdAnimationBaseMs = 0;
                s_holdPressStartMs = millis();
                HermesXInterfaceModule::instance->beginPowerHoldProgress();
            }
#endif
            // 罐頭訊息選單：記錄按下起點，後續長按 1 秒退出
#if !MESHTASTIC_EXCLUDE_HERMESX
            if (cannedMessageModule && cannedMessageModule->getRunState() != CANNED_MESSAGE_RUN_STATE_INACTIVE) {
                s_exitCannedHold = true;
                s_exitCannedStartMs = millis();
            }
#endif
#else
            switchPage();
#endif
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
#if !MESHTASTIC_EXCLUDE_HERMESX
            // 長按約 1 秒可退出罐頭訊息選單，不進入關機流程
            if (cannedMessageModule && cannedMessageModule->getRunState() != CANNED_MESSAGE_RUN_STATE_INACTIVE &&
                s_exitCannedHold && (millis() - s_exitCannedStartMs >= 1000)) {
                cannedMessageModule->exitMenu();
                s_exitCannedHold = false;
                break;
            }
#endif
#if !MESHTASTIC_EXCLUDE_HERMESX && (defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) ||          \
                                    defined(BUTTON_PIN_ALT) || defined(BUTTON_PIN_TOUCH))
            if (wakeTriggered) {
                break;
            }
#endif
            LOG_BUTTON("Long press!");
            powerFSM.trigger(EVENT_PRESS);
#if !MESHTASTIC_EXCLUDE_HERMESX
            // HermesX: 延後 Shutdown UI/動畫到 PowerHold 完成（逐格轉紅後）
            s_deferShutdownUi = HermesXInterfaceModule::instance;
            s_shutdownSequenceStarted = false;
            if (!s_deferShutdownUi)
#endif
            if (screen) {
                screen->startHermesXAlert(kShutdownMessage);
            }
#if !MESHTASTIC_EXCLUDE_HERMESX
            if (!s_deferShutdownUi)
#endif
            playBeep();
#if !MESHTASTIC_EXCLUDE_HERMESX
            if (!s_shutdownAnimArmed && HermesXInterfaceModule::instance && !s_deferShutdownUi) {
                // 進入關機流程時終止 powerHold 進度，讓關機動畫接管
                HermesXInterfaceModule::instance->stopPowerHoldAnimation(false);
                s_shutdownAnimArmed = true;
                s_shutdownDeadlineMs = millis() + kShutdownAnimDurationMs;
                HermesXInterfaceModule::instance->startLEDAnimation(LEDAnimation::ShutdownEffect);
            }
#endif
            break;
        }

        // Do actual shutdown when button released, otherwise the button release
        // may wake the board immediatedly.
        case BUTTON_EVENT_LONG_RELEASED: {
#if !MESHTASTIC_EXCLUDE_HERMESX && (defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) ||          \
                                    defined(BUTTON_PIN_ALT) || defined(BUTTON_PIN_TOUCH))
            if (wakeTriggered) {
                // 這次長按是喚醒流程，不要進入關機
                resetWakeHoldGate();
                break;
            }
#endif
            // 取消罐頭退出握持狀態
#if !MESHTASTIC_EXCLUDE_HERMESX
            s_exitCannedHold = false;
#endif
            LOG_INFO("Shutdown from long press");
#if !MESHTASTIC_EXCLUDE_HERMESX
            uint32_t shutdownDelayMs = kShutdownAnimDurationMs;
            auto *interfaceModule = HermesXInterfaceModule::instance;
            if (s_shutdownSequenceStarted) {
                // 視覺已啟動：只需等待已排程的 deadline，再做實際關機
                uint32_t nowMs = millis();
                if (s_shutdownAnimArmed && s_shutdownDeadlineMs > nowMs) {
                    delay(s_shutdownDeadlineMs - nowMs);
                }
            } else {
                if (s_deferShutdownUi && interfaceModule) {
                    // 放開才補啟動：補完進度、開啟關機視覺
                    const uint32_t holdDurationMs = BUTTON_LONGPRESS_MS ? BUTTON_LONGPRESS_MS : 1;
                    startShutdownVisuals(holdDurationMs);
                } else {
                    if (screen) {
                        screen->startHermesXAlert(kShutdownMessage);
                    }
                    playBeep();
                    s_shutdownAnimArmed = interfaceModule;
                    s_shutdownDeadlineMs = millis() + shutdownDelayMs;
                    if (interfaceModule) {
                        interfaceModule->startLEDAnimation(LEDAnimation::ShutdownEffect);
                    }
                    s_shutdownSequenceStarted = true;
                }
            }
#else
            if (screen) {
                screen->startHermesXAlert(kShutdownMessage);
            }
            playBeep();
#endif
            playShutdownMelody();
#if !MESHTASTIC_EXCLUDE_HERMESX
            if (!s_shutdownAnimArmed && HermesXInterfaceModule::instance) {
                s_shutdownAnimArmed = true;
                s_shutdownDeadlineMs = millis() + shutdownDelayMs;
                HermesXInterfaceModule::instance->startLEDAnimation(LEDAnimation::ShutdownEffect);
            }
#endif
            s_shutdownAnimArmed = true;
            const uint32_t minDeadline = millis() + kShutdownMessageMinMs;
            if (s_shutdownDeadlineMs < minDeadline) {
                s_shutdownDeadlineMs = minDeadline;
            }
#if !MESHTASTIC_EXCLUDE_HERMESX
            s_deferShutdownUi = false;
            s_shutdownSequenceStarted = false;
#endif
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

#if !MESHTASTIC_EXCLUDE_HERMESX
    // 若已經啟動關機動畫，但遲遲沒有收到長按放開事件，逾時後強制收尾並進入關機
    if (s_shutdownAnimArmed && s_shutdownDeadlineMs > 0) {
        uint32_t now = millis();
        if (now >= s_shutdownDeadlineMs) {
            if (HermesXInterfaceModule::instance) {
                HermesXInterfaceModule::instance->forceAllLedsOff();
            }
            power->shutdown();
            s_shutdownAnimArmed = false;
            s_shutdownDeadlineMs = 0;
            s_shutdownSequenceStarted = false;
            s_deferShutdownUi = false;
        }
    }
#endif

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
void ButtonThread::resetLongPressState()
{
    s_releaseSeen = false;
    s_longGateArmed = false;
    s_longEventPending = false;
    s_longPressFromAlt = false;
    s_longPressSuppressed = false;
    s_longPressSuppressUntil = 0;
    s_resumeGraceUntil = millis() + kResumeGraceMs;
    s_lastStableChangeMs = 0;
    s_lastStablePressed = false;
    s_longStartMillis = 0;
    s_holdPressStartMs = 0;
#if !MESHTASTIC_EXCLUDE_HERMESX
    s_deferShutdownUi = false;
    s_shutdownSequenceStarted = false;
#endif
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) || defined(BUTTON_PIN_ALT) ||                \
    defined(BUTTON_PIN_TOUCH)
    holdAnimationStarted = false;
#endif
}

void ButtonThread::resetWakeHoldGate()
{
    bool hadWakeAnim = wakeAnimStarted;
    wakeHoldActive = false;
    wakeTriggered = false;
    wakeHoldStart = 0;
    wakeAnimStarted = false;
#if !MESHTASTIC_EXCLUDE_HERMESX
    if (HermesXInterfaceModule::instance && hadWakeAnim) {
        HermesXInterfaceModule::instance->stopPowerHoldAnimation(false);
    }
#endif
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) || defined(BUTTON_PIN_ALT) || defined(BUTTON_PIN_TOUCH)
    holdAnimationStarted = false;
#endif
}

void ButtonThread::processWakeHoldGate()
{
    // Wake hold 只在待機/深睡狀態使用；進入系統後不介入（也不重置動畫/閘門）
    if (powerFSM.getState() != &stateDARK)
        return;

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

    if (!wakeHoldActive && anyPressed) {
        // 按住後在事件送達前就進入閘門，避免開機時抓不到 PRESS 事件
        wakeHoldActive = true;
        wakeTriggered = false;
        wakeAnimStarted = false;
        wakeHoldStart = millis();
    }

    if (!anyPressed) {
        resetWakeHoldGate();
        return;
    }

    auto *interfaceModule = HermesXInterfaceModule::instance;

    if (!wakeAnimStarted && interfaceModule) {
        interfaceModule->startPowerHoldAnimation(HermesXInterfaceModule::PowerHoldMode::PowerOn, kWakeHoldMs);
        wakeAnimStarted = true;
    }

    if (wakeAnimStarted && interfaceModule) {
        uint32_t clamped = pressedMs;
        if (clamped > kWakeHoldMs) {
            clamped = kWakeHoldMs;
        }
        interfaceModule->updatePowerHoldAnimation(clamped);
    }

    if (!wakeTriggered && pressedMs >= kWakeHoldMs) {
        wakeTriggered = true;
        // 只有在 Dark 狀態才觸發開機，避免已開機狀態下按住又切回關機
        auto *currentState = powerFSM.getState();
        if (currentState == &stateDARK) {
            powerFSM.trigger(EVENT_PRESS);
        }
#if !MESHTASTIC_EXCLUDE_HERMESX
        if (HermesXInterfaceModule::instance) {
            // 使用預設待機色播放開機動畫
            HermesXInterfaceModule::instance->playStartupLEDAnimation(0xFF5000);
        }
#endif
    }
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
        if (s_requireReleaseBeforeLongPress) {
            s_requireReleaseBeforeLongPress = false;
        }
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
        s_longPressFromAlt = false;
        s_longPressSuppressed = false;
        s_longPressSuppressUntil = 0;
        s_resumeGraceUntil = millis() + kResumeGraceMs;
        s_lastStableChangeMs = 0;
        s_lastStablePressed = false;
        s_longStartMillis = 0;
    }
#endif
}

void ButtonThread::updatePowerHoldAnimation()
{
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) || defined(BUTTON_PIN_ALT) || defined(BUTTON_PIN_TOUCH)
    auto *interfaceModule = HermesXInterfaceModule::instance;
    bool interfaceReady = interfaceModule != nullptr;

    if (isLongPressSuppressedNow()) {
        if (holdAnimationActive || holdAnimationStarted) {
            holdAnimationActive = false;
            holdAnimationStarted = false;
            holdAnimationLastMs = 0;
            s_holdPressStartMs = 0;
            if (interfaceReady) {
                interfaceModule->stopPowerHoldAnimation(false);
            }
        }
        return;
    }

    // 已經進入關機序列時，不再繼續進度動畫，避免再次啟動或重跑
    if (s_shutdownSequenceStarted) {
        // 進度動畫停掉，但不要重置計時；等待關機
        if (holdAnimationActive && holdAnimationStarted) {
            if (auto *interfaceModule = HermesXInterfaceModule::instance) {
                interfaceModule->stopPowerHoldAnimation(false);
            }
            holdAnimationActive = false;
            holdAnimationStarted = false;
        }
        return;
    }

    uint32_t holdDurationMs = BUTTON_LONGPRESS_MS ? BUTTON_LONGPRESS_MS : 1;
    bool anyPressed = false;
    uint32_t pressedMs = 0;
    uint32_t userPressedMs = 0;
    uint32_t altPressedMs = 0;
    uint32_t touchPressedMs = 0;

#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN)
    if (userButton.pin() >= 0 && userButton.debouncedValue()) {
        anyPressed = true;
        uint32_t candidate = userButton.getPressedMs();
        userPressedMs = candidate;
        if (candidate > pressedMs) {
            pressedMs = candidate;
        }
    }
#endif
#ifdef BUTTON_PIN_ALT
    if (userButtonAlt.pin() >= 0 && userButtonAlt.debouncedValue()) {
        anyPressed = true;
        uint32_t candidate = userButtonAlt.getPressedMs();
        altPressedMs = candidate;
        if (candidate > pressedMs) {
            pressedMs = candidate;
        }
    }
#endif
#ifdef BUTTON_PIN_TOUCH
    if (userButtonTouch.pin() >= 0 && userButtonTouch.debouncedValue()) {
        anyPressed = true;
        uint32_t candidate = userButtonTouch.getPressedMs();
        touchPressedMs = candidate;
        if (candidate > pressedMs) {
            pressedMs = candidate;
        }
    }
#endif

    // 如果剛按下，OneButton 的 pressedMs 可能仍為 0；仍然要啟動動畫
    if (anyPressed && pressedMs == 0) {
        pressedMs = 1;
    }

#if !MESHTASTIC_EXCLUDE_HERMESX
    if (altPressedMs >= pressedMs && altPressedMs > 0) {
        holdDurationMs += kRotaryLongPressDelayMs;
    }
#endif

    static bool s_prevAnyPressed = false;
    static uint32_t s_lastPressTimestamp = 0;
    if (anyPressed && !s_prevAnyPressed) {
        LOG_INFO("PowerHold: press detected (user=%" PRIu32 " alt=%" PRIu32 " touch=%" PRIu32 ")", userPressedMs, altPressedMs,
                 touchPressedMs);
    }
    if (anyPressed) {
        s_lastPressTimestamp = millis();
    }
    s_prevAnyPressed = anyPressed;

    if (anyPressed) {
        const uint32_t nowMs = millis();
        if (s_holdPressStartMs == 0) {
            s_holdPressStartMs = nowMs;
        }
        uint32_t elapsedMs = nowMs - s_holdPressStartMs;
        holdAnimationLastMs = elapsedMs;
        const uint32_t visualGateMs =
#if !MESHTASTIC_EXCLUDE_HERMESX
            static_cast<uint32_t>(static_cast<float>(holdDurationMs) * kPowerHoldVisualStretch + 0.5f);
#else
            holdDurationMs;
#endif
        const uint32_t shutdownGateMs = visualGateMs ? visualGateMs : holdDurationMs;

        if (!holdAnimationActive) {
            holdAnimationMode = resolveHoldMode();
            if (holdAnimationMode != HoldAnimationMode::None) {
                holdAnimationActive = true;
                holdAnimationStarted = false;
                holdAnimationLastMs = 0;          // 動畫進度從 0 開始
                holdAnimationBaseMs = 0;           // 改用累計時間
                const auto moduleMode = (holdAnimationMode == HoldAnimationMode::PowerOn)
                                            ? HermesXInterfaceModule::PowerHoldMode::PowerOn
                                            : HermesXInterfaceModule::PowerHoldMode::PowerOff;
                LOG_INFO("PowerHold: auto-start mode=%s ms=%" PRIu32,
                         moduleMode == HermesXInterfaceModule::PowerHoldMode::PowerOn ? "on" : "off", pressedMs);
                if (interfaceReady) {
                    interfaceModule->startPowerHoldAnimation(moduleMode, holdDurationMs);
                    holdAnimationStarted = true;
                } else {
                    LOG_INFO("PowerHold: interface not ready, defer start");
                }
            }
        } else if (pressedMs >= holdDurationMs && holdAnimationLastMs == 0) {
            // 進入時已經累積很久，記錄遲到偵測
            LOG_INFO("PowerHold: late detection, pressedMs=%" PRIu32 " holdDuration=%" PRIu32, pressedMs, holdDurationMs);
        }

        if (holdAnimationActive) {
            // 若發現 LED 管理的當前動畫已被其他事件蓋掉，立即恢復 PowerHoldProgress
            if (interfaceReady && holdAnimationStarted &&
                interfaceModule->getCurrentAnimation() != LEDAnimation::PowerHoldProgress) {
                LOG_INFO("PowerHold: animation lost, restoring");
                interfaceModule->startLEDAnimation(LEDAnimation::PowerHoldProgress);
                interfaceModule->updatePowerHoldAnimation(holdAnimationLastMs);
            }

            // 先顯示關機頁面（提前到 4s），但不進入關機流程
            if (holdAnimationMode == HoldAnimationMode::PowerOff && !s_shutdownMessageShown &&
                holdAnimationLastMs >= kShutdownMessageEarlyMs) {
                if (screen) {
                    screen->startHermesXAlert(kShutdownMessage);
                }
                s_shutdownMessageShown = true;
            }

            // 達標即啟動關機視覺，避免重跑或提前被 stop
            if (holdAnimationMode == HoldAnimationMode::PowerOff && holdAnimationLastMs >= shutdownGateMs) {
                startShutdownVisuals(shutdownGateMs);
                return;
            }

            // 關機長按已經進行且累積達標後，不再接受後續 long-start 重置
            if (holdAnimationMode == HoldAnimationMode::PowerOff && holdAnimationLastMs >= shutdownGateMs) {
                holdAnimationActive = false; // 交由關機流程收尾
                return;
            }

            // 若尚未在 LED 模組啟動且接口已就緒，立即啟動並套用累計時間
            if (!holdAnimationStarted && interfaceReady) {
                const auto moduleMode = (holdAnimationMode == HoldAnimationMode::PowerOn)
                                            ? HermesXInterfaceModule::PowerHoldMode::PowerOn
                                            : HermesXInterfaceModule::PowerHoldMode::PowerOff;
                interfaceModule->startPowerHoldAnimation(moduleMode, holdDurationMs);
                holdAnimationStarted = true;
                interfaceModule->updatePowerHoldAnimation(holdAnimationLastMs);
            }

            holdAnimationLastMs = elapsedMs;
            LOG_INFO("PowerHold: update ms=%" PRIu32, holdAnimationLastMs);
            if (holdAnimationStarted && interfaceReady) {
                interfaceModule->updatePowerHoldAnimation(holdAnimationLastMs);
            }

            // 達標即進入關機視覺，避免進度重跑第二輪
            if (holdAnimationMode == HoldAnimationMode::PowerOff && holdAnimationLastMs >= holdDurationMs) {
                startShutdownVisuals(holdDurationMs);
                return;
            }

            // 關機長按：逐格轉紅達標就啟動關機視覺（不必等放開）
            if (s_deferShutdownUi && holdAnimationMode == HoldAnimationMode::PowerOff &&
                holdAnimationLastMs >= holdDurationMs) {
                startShutdownVisuals(holdDurationMs);
            }
        }

        return;
    }

    // 釋放後立即停止逐格動畫，釋放資源給其他動畫使用（留一點容錯避免短暫抖動）
    const uint32_t nowMs = millis();
    const uint32_t releaseGraceMs = 180;
    if (holdAnimationActive && s_lastPressTimestamp && (nowMs - s_lastPressTimestamp) < releaseGraceMs) {
        return;
    }

    if (holdAnimationActive) {
        holdAnimationActive = false;
        holdAnimationMode = HoldAnimationMode::None;
        holdAnimationLastMs = 0;
        holdAnimationStarted = false;
        s_holdPressStartMs = 0;
        s_shutdownMessageShown = false;
        if (auto *interfaceModule = HermesXInterfaceModule::instance) {
            interfaceModule->stopPowerHoldAnimation(false);
        }
    } else {
        s_holdPressStartMs = 0;
    }

    // 若已經進入關機序列，確保長按閘門狀態清掉，避免提前發生 shutdown
#if !MESHTASTIC_EXCLUDE_HERMESX
    if (s_shutdownSequenceStarted || s_shutdownAnimArmed) {
        s_longGateArmed = false;
        s_longEventPending = false;
        s_longStartMillis = 0;
    }
#endif
#endif
}

#endif

#if !MESHTASTIC_EXCLUDE_HERMESX && defined(HERMESX_GUARD_POWER_ANIMATIONS)
void ButtonThread::disableBootHold()
{
#if defined(BUTTON_PIN) || defined(USERPREFS_BUTTON_PIN)
    bootHoldArmed = false;
    bootHoldPressActive = false;
    bootHoldWaitingForPress = false;
    bootHoldStartMs = 0;
    bootHoldArmedAtMs = 0;
    bootHoldAnimStarted = false;
    bootHoldDotsPhase = 0;
    if (screen) {
        screen->endAlert();
    }
#else
    (void)screen;
#endif
}

void ButtonThread::requireReleaseBeforeLongPress()
{
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) || defined(BUTTON_PIN_ALT) ||                \
    defined(BUTTON_PIN_TOUCH)
    s_requireReleaseBeforeLongPress = true;
    s_releaseSeen = false;
#endif
}

static bool isLongPressSuppressedNow()
{
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) || defined(BUTTON_PIN_ALT) ||                \
    defined(BUTTON_PIN_TOUCH)
    if (s_longPressSuppressUntil && millis() > s_longPressSuppressUntil) {
        s_longPressSuppressUntil = 0;
        s_longPressSuppressed = false;
    }
    return s_longPressSuppressed;
#else
    return false;
#endif
}

void ButtonThread::extendLongPressGrace(uint32_t graceMs)
{
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) || defined(BUTTON_PIN_ALT) ||                \
    defined(BUTTON_PIN_TOUCH)
    if (graceMs == 0) {
        return;
    }
    const uint32_t now = millis();
    const uint32_t candidate = now + graceMs;
    if (candidate > s_resumeGraceUntil) {
        s_resumeGraceUntil = candidate;
    }
#endif
}

void ButtonThread::suppressLongPress(bool active)
{
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) || defined(BUTTON_PIN_ALT) ||                \
    defined(BUTTON_PIN_TOUCH)
    s_longPressSuppressed = active;
    if (!active) {
        s_longPressSuppressUntil = 0;
    }
#endif
}

void ButtonThread::suppressLongPressFor(uint32_t graceMs)
{
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) || defined(BUTTON_PIN_ALT) ||                \
    defined(BUTTON_PIN_TOUCH)
    if (graceMs == 0) {
        s_longPressSuppressed = false;
        s_longPressSuppressUntil = 0;
        return;
    }
    s_longPressSuppressed = true;
    s_longPressSuppressUntil = millis() + graceMs;
#endif
}

bool ButtonThread::isLongPressSuppressed()
{
    return isLongPressSuppressedNow();
}

bool ButtonThread::isReleaseRequiredForLongPress()
{
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) || defined(BUTTON_PIN_ALT) ||                \
    defined(BUTTON_PIN_TOUCH)
    return s_requireReleaseBeforeLongPress;
#else
    return false;
#endif
}

bool ButtonThread::handleBootHold()
{
    if (!bootHoldArmed)
        return false;

    static const char *const kBootHoldDots[] = {".", "..", "...", "...."};
    // BootHold runs after a deep-sleep wake (it does not wake the device) and enforces a long press to resume.
    bool currentlyPressed = userButton.debouncedValue();
    const uint32_t now = millis();
    const bool allowTimeout = HermesXPowerGuard::wokeFromSleep();

    if (bootHoldWaitingForPress) {
        if (!currentlyPressed) {
            if (allowTimeout && bootHoldArmedAtMs != 0 && (now - bootHoldArmedAtMs) >= kBootHoldIdleSleepMs) {
                LOG_INFO("BootHold idle timeout, return to deep sleep");
                if (bootHoldAnimStarted && HermesXInterfaceModule::instance) {
                    HermesXInterfaceModule::instance->stopPowerHoldAnimation(false);
                }
                bootHoldAnimStarted = false;
                bootHoldDotsPhase = 0;
                if (screen) {
                    screen->endAlert();
                }
                HermesXPowerGuard::markBootHoldAborted();
                bootHoldArmed = false;
                bootHoldPressActive = false;
                bootHoldWaitingForPress = false;
                bootHoldStartMs = 0;
                bootHoldArmedAtMs = 0;
                HermesXInterfaceModule::setPowerHoldReady(false);
                power->shutdown();
            }
            return true;
        }

        bootHoldWaitingForPress = false;
        bootHoldPressActive = true;
        bootHoldAnimStarted = false;
        bootHoldDotsPhase = 0xFF;
        HermesXPowerGuard::markPressDetected();
        bootHoldStartMs = now;
        LOG_BUTTON("BootHold: press detected");
    }

    if (!bootHoldPressActive && currentlyPressed) {
        bootHoldPressActive = true;
        bootHoldStartMs = now;
        bootHoldAnimStarted = false;
        bootHoldDotsPhase = 0xFF;
    }

    if (bootHoldPressActive && currentlyPressed) {
        uint32_t pressedMs = userButton.getPressedMs();
        if (pressedMs == 0) {
            pressedMs = 1;
        }
        if (auto *interfaceModule = HermesXInterfaceModule::instance) {
            if (!bootHoldAnimStarted) {
                interfaceModule->startPowerHoldAnimation(HermesXInterfaceModule::PowerHoldMode::PowerOn, BUTTON_LONGPRESS_MS);
                bootHoldAnimStarted = true;
            }
            interfaceModule->updatePowerHoldAnimation(pressedMs);
        }
        if (screen) {
            const uint32_t elapsedMs = now - bootHoldStartMs;
            const uint8_t newPhase = static_cast<uint8_t>((elapsedMs / kBootHoldDotsIntervalMs) % 4);
            if (newPhase != bootHoldDotsPhase) {
                bootHoldDotsPhase = newPhase;
                screen->startAlert(kBootHoldDots[newPhase]);
            }
        }
    }

    if (!currentlyPressed) {
        uint32_t pressedMs = userButton.getPressedMs();
        if (!bootHoldPressActive) {
            return true;
        }
        if (pressedMs < BUTTON_LONGPRESS_MS) {
            // 短按喚醒後進入等待期：在視窗內再次長按才放行開機
            LOG_INFO("BootHold short press, waiting for long press (pressed=%" PRIu32 ")", pressedMs);
            if (bootHoldAnimStarted && HermesXInterfaceModule::instance) {
                HermesXInterfaceModule::instance->stopPowerHoldAnimation(false);
            }
            bootHoldAnimStarted = false;
            bootHoldDotsPhase = 0;
            if (screen) {
                screen->endAlert();
            }
            bootHoldArmed = true;
            bootHoldPressActive = false;
            bootHoldWaitingForPress = true;
            bootHoldStartMs = 0;
            bootHoldArmedAtMs = allowTimeout ? now : 0;
            holdOffBypassed = false;
            HermesXInterfaceModule::setPowerHoldReady(false);
            return true;
        }
        bootHoldArmed = false;
        bootHoldPressActive = false;
        bootHoldWaitingForPress = false;
        bootHoldArmedAtMs = 0;
        bootHoldAnimStarted = false;
        bootHoldDotsPhase = 0;
        if (screen) {
            screen->endAlert();
        }
        holdOffBypassed = true; // Boot 已經完成，允許立即使用長按關機
        LOG_BUTTON("BootHold: passed (pressed=%" PRIu32 ")", pressedMs);
        return false;
    }

    uint32_t pressedMs = userButton.getPressedMs();

    if (pressedMs >= BUTTON_LONGPRESS_MS) {
        HermesXPowerGuard::markBootHoldCommitted();
        LOG_BUTTON("BootHold: committed (ready)");
        bootHoldArmed = false;
        bootHoldPressActive = false;
        bootHoldWaitingForPress = false;
        bootHoldArmedAtMs = 0;
        bootHoldAnimStarted = false;
        bootHoldDotsPhase = 0;
        if (screen) {
            screen->endAlert();
        }
        holdOffBypassed = true; // Boot 已經完成，允許立即使用長按關機
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
    if (isLongPressSuppressedNow()) {
        return;
    }
    // 已啟動關機序列時忽略後續長按開始，避免中途再次觸發導致提前關機
#if !MESHTASTIC_EXCLUDE_HERMESX
    if (s_shutdownSequenceStarted || s_shutdownAnimArmed) {
        return;
    }
    // 已有任何逐燈動畫在跑（開/關機皆然），避免 OneButton 重複觸發 long start 造成重置
    if (buttonThread && buttonThread->holdAnimationActive) {
        return;
    }
#endif
    uint32_t pressedMs = 0;
    bool useAltPress = false;
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN)
    pressedMs = userButton.getPressedMs();
#endif
#ifdef BUTTON_PIN_ALT
    uint32_t altPressed = userButtonAlt.getPressedMs();
    if (altPressed > pressedMs) {
        pressedMs = altPressed;
        useAltPress = true;
    }
#endif
    uint32_t longPressThreshold = BUTTON_LONGPRESS_MS;
#if !MESHTASTIC_EXCLUDE_HERMESX
    if (useAltPress) {
        longPressThreshold += kRotaryLongPressDelayMs;
    }
#endif
    if (pressedMs + 20 < longPressThreshold)
        return;

    const uint32_t now = millis();
#if !MESHTASTIC_EXCLUDE_HERMESX && defined(HERMESX_GUARD_POWER_ANIMATIONS) &&                                                   \
    (defined(BUTTON_PIN) || defined(USERPREFS_BUTTON_PIN) || defined(ARCH_PORTDUINO))
    // 已進入系統（非深睡喚醒）時，允許立即長按關機；否則維持原 hold-off 規則
    bool bootHoldActive = buttonThread ? buttonThread->bootHoldArmed : false;
    bool holdAllowed = (!bootHoldActive && powerFSM.getState() != &stateDARK) ? true : (holdOffBypassed || (now > c_holdOffTime));
#else
    bool holdAllowed = true;
#endif

    if (s_requireReleaseBeforeLongPress)
        return;

    if (s_shutdownSequenceStarted || s_shutdownAnimArmed) {
        return;
    }

    if (!holdAllowed)
        return;

    // 冷開機後第一次按壓，若尚未記錄過「釋放」，允許按住達 debounce 時即視為可進行長按
    if (!s_releaseSeen) {
        if (pressedMs >= kReleaseDebounceMs) {
            s_releaseSeen = true;
        } else {
            return;
        }
    }

    if (s_resumeGraceUntil && (now < s_resumeGraceUntil))
        return;

    if (s_longGateArmed)
        return;

    s_longStartMillis = now - pressedMs;
    s_longGateArmed = true;
    s_longEventPending = true;
    s_longPressFromAlt = useAltPress;
    s_releaseSeen = false;
    btnEvent = BUTTON_EVENT_LONG_PRESSED;

#if !MESHTASTIC_EXCLUDE_HERMESX
    // 確保長按立即啟動 PowerHold 逐格動畫（開機/關機各自對應）
    if (auto *interfaceModule = HermesXInterfaceModule::instance) {
        const auto mode = (buttonThread && buttonThread->resolveHoldMode() == HoldAnimationMode::PowerOn)
                              ? HermesXInterfaceModule::PowerHoldMode::PowerOn
                              : HermesXInterfaceModule::PowerHoldMode::PowerOff;
        if (buttonThread) {
            buttonThread->holdAnimationMode =
                (mode == HermesXInterfaceModule::PowerHoldMode::PowerOn) ? HoldAnimationMode::PowerOn : HoldAnimationMode::PowerOff;
            buttonThread->holdAnimationActive = true;
            buttonThread->holdAnimationLastMs = 0; // 動畫從 0 開始跑
            buttonThread->holdAnimationBaseMs = 0;
            s_holdPressStartMs = millis();
        }
        LOG_INFO("PowerHold: manual start mode=%s ms=%" PRIu32,
                 mode == HermesXInterfaceModule::PowerHoldMode::PowerOn ? "on" : "off", pressedMs);
        interfaceModule->startPowerHoldAnimation(mode, longPressThreshold ? longPressThreshold : 1);
    }
#endif
#endif
}

void ButtonThread::userButtonPressedLongStop()
{
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) || defined(BUTTON_PIN_ALT)
    // 已啟動關機序列時忽略，避免提前 shutdown
#if !MESHTASTIC_EXCLUDE_HERMESX
    if (s_shutdownSequenceStarted || s_shutdownAnimArmed) {
        s_longGateArmed = false;
        s_longEventPending = false;
        s_longStartMillis = 0;
        s_longPressFromAlt = false;
        return;
    }
#endif
    if (isLongPressSuppressedNow()) {
        s_longGateArmed = false;
        s_longEventPending = false;
        s_longStartMillis = 0;
        s_longPressFromAlt = false;
        return;
    }
    const uint32_t now = millis();
    bool holdAllowed =
#if !MESHTASTIC_EXCLUDE_HERMESX && defined(HERMESX_GUARD_POWER_ANIMATIONS) &&                                                   \
    (defined(BUTTON_PIN) || defined(USERPREFS_BUTTON_PIN) || defined(ARCH_PORTDUINO))
        []() {
            bool bootHoldActive = buttonThread ? buttonThread->bootHoldArmed : false;
            return (!bootHoldActive && powerFSM.getState() != &stateDARK) ? true : (holdOffBypassed || (millis() > c_holdOffTime));
        }();
#else
        true;
#endif

    if (!s_longGateArmed) {
        s_longEventPending = false;
        s_longStartMillis = 0;
        s_longPressFromAlt = false;
        return;
    }

    s_longGateArmed = false;

    if (!holdAllowed) {
        s_longEventPending = false;
        s_longStartMillis = 0;
        s_longPressFromAlt = false;
        return;
    }

    if (s_resumeGraceUntil && (now < s_resumeGraceUntil)) {
        s_longEventPending = false;
        s_longStartMillis = 0;
        s_longPressFromAlt = false;
        return;
    }

    uint32_t duration = s_longStartMillis ? (now - s_longStartMillis) : 0;
    s_longStartMillis = 0;
    s_longEventPending = false;
    const uint32_t longPressThreshold =
#if !MESHTASTIC_EXCLUDE_HERMESX
        BUTTON_LONGPRESS_MS + (s_longPressFromAlt ? kRotaryLongPressDelayMs : 0);
#else
        BUTTON_LONGPRESS_MS;
#endif
    s_longPressFromAlt = false;

#if !MESHTASTIC_EXCLUDE_HERMESX
    // 收尾時再更新一次動畫進度，確保達標時會進入完成態
    if (auto *interfaceModule = HermesXInterfaceModule::instance) {
        interfaceModule->updatePowerHoldAnimation(duration);
        const bool deferPowerHoldCompletion = s_deferShutdownUi;
        if (!deferPowerHoldCompletion) {
            interfaceModule->stopPowerHoldAnimation(duration + 20 >= BUTTON_LONGPRESS_MS);
        }
    }
#endif

    if (duration + 20 < longPressThreshold) {
        return;
    }

    btnEvent = BUTTON_EVENT_LONG_RELEASED;
#else
    bool holdAllowed = (millis() > c_holdOffTime);
    if (!holdAllowed)
        return;

    btnEvent = BUTTON_EVENT_LONG_RELEASED;
#endif
}
#if !MESHTASTIC_EXCLUDE_HERMESX
void ButtonThread::clearHoldAnimationState()
{
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN) || defined(BUTTON_PIN_ALT) || defined(BUTTON_PIN_TOUCH)
    if (buttonThread) {
        buttonThread->holdAnimationActive = false;
        buttonThread->holdAnimationMode = HoldAnimationMode::None;
        buttonThread->holdAnimationStarted = false;
        buttonThread->holdAnimationLastMs = 0;
    }
    s_holdPressStartMs = 0;
    s_shutdownMessageShown = false;
#endif
}

bool ButtonThread::isHoldButtonPressed()
{
#if defined(BUTTON_PIN) || defined(ARCH_PORTDUINO) || defined(USERPREFS_BUTTON_PIN)
    if (userButton.pin() >= 0 && userButton.debouncedValue())
        return true;
#endif
#ifdef BUTTON_PIN_ALT
    if (buttonThread && buttonThread->userButtonAlt.pin() >= 0 && buttonThread->userButtonAlt.debouncedValue())
        return true;
#endif
#ifdef BUTTON_PIN_TOUCH
    if (buttonThread && buttonThread->userButtonTouch.pin() >= 0 && buttonThread->userButtonTouch.debouncedValue())
        return true;
#endif
    return false;
}
#endif
