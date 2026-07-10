#include "RotaryEncoderInterruptBase.h"
#include "configuration.h"
#include "graphics/Screen.h"

extern graphics::Screen *screen;

namespace
{
constexpr uint32_t kRotaryDispatchMinMs = 40;
constexpr uint32_t kRotaryPressDebounceMs = 200;
constexpr uint32_t kRotaryLockHoldMs = 3000;
constexpr int8_t kRotaryStepsPerDetent = 4;
const int8_t kRotaryTransitionTable[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

bool isHermesRotaryLocked()
{
    return screen && screen->isRotaryLocked();
}

bool isHermesRotaryLockPopupVisible()
{
    return screen && screen->isRotaryLockPopupVisible();
}

bool shouldAllowHermesRotaryLockLongPress()
{
    return screen && screen->shouldAllowRotaryLockLongPress();
}

bool shouldSuppressHermesRotaryShortPressAfterHold(uint32_t heldMs)
{
    return screen && screen->shouldSuppressRotaryShortPressAfterHold(heldMs);
}

void setHermesRotaryLocked(bool locked)
{
    if (screen) {
        screen->setRotaryLockState(locked);
    }
}
}

RotaryEncoderInterruptBase::RotaryEncoderInterruptBase(const char *name) : concurrency::OSThread(name)
{
    this->_originName = name;
}

void RotaryEncoderInterruptBase::init(
    uint8_t pinA, uint8_t pinB, uint8_t pinPress, char eventCw, char eventCcw, char eventPressed,
    //    std::function<void(void)> onIntA, std::function<void(void)> onIntB, std::function<void(void)> onIntPress) :
    void (*onIntA)(), void (*onIntB)(), void (*onIntPress)(), bool directionSwapped)
{
    this->_pinA = pinA;
    this->_pinB = pinB;
    this->_pinPress = pinPress;
    this->_onIntA = onIntA;
    this->_onIntB = onIntB;
    this->_onIntPress = onIntPress;
    setEventMapping(eventCw, eventCcw, eventPressed, directionSwapped);

    pinMode(pinPress, INPUT_PULLUP);
    pinMode(this->_pinA, INPUT_PULLUP);
    pinMode(this->_pinB, INPUT_PULLUP);

    attachInterrupts();

    this->rotaryLevelA = digitalRead(this->_pinA);
    this->rotaryLevelB = digitalRead(this->_pinB);
    this->lastRotaryState = ((this->rotaryLevelA == HIGH) ? 1 : 0) << 1 | ((this->rotaryLevelB == HIGH) ? 1 : 0);
    this->rotaryStep = 0;
    LOG_INFO("Rotary initialized (%d, %d, %d) swapped=%d", this->_pinA, this->_pinB, pinPress,
             this->_directionSwapped ? 1 : 0);
}

void RotaryEncoderInterruptBase::setEventMapping(char eventCw, char eventCcw, char eventPressed, bool directionSwapped)
{
    this->_eventCw = eventCw;
    this->_eventCcw = eventCcw;
    this->_eventPressed = eventPressed;
    this->_directionSwapped = directionSwapped;
    this->rotaryStep = 0;
}

void RotaryEncoderInterruptBase::attachInterrupts()
{
    if (!_pinA || !_pinB || !_pinPress || !_onIntA || !_onIntB || !_onIntPress) {
        return;
    }
    attachInterrupt(_pinPress, _onIntPress, CHANGE);
    attachInterrupt(_pinA, _onIntA, CHANGE);
    attachInterrupt(_pinB, _onIntB, CHANGE);
}

void RotaryEncoderInterruptBase::detachInterrupts()
{
    if (_pinPress) {
        detachInterrupt(_pinPress);
    }
    if (_pinA) {
        detachInterrupt(_pinA);
    }
    if (_pinB) {
        detachInterrupt(_pinB);
    }
}

int32_t RotaryEncoderInterruptBase::runOnce()
{
    InputEvent e;
    e.inputEvent = meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_NONE;
    e.source = this->_originName;
    e.kbchar = 0x00;
    e.touchX = 0;
    e.touchY = 0;

    const uint32_t now = millis();
    const bool pressLow = this->_pinPress != 0 && digitalRead(this->_pinPress) == LOW;

    if (this->action == ROTARY_ACTION_PRESSED || this->pressTracking) {
        if (pressLow) {
            if (!this->pressTracking) {
                this->pressTracking = true;
                this->pressLongFired = false;
                this->pressDownSinceMs = now;
            }
            if (!this->pressLongFired && (now - this->pressDownSinceMs) >= kRotaryLockHoldMs) {
                this->pressLongFired = true;
                if (!isHermesRotaryLockPopupVisible() && shouldAllowHermesRotaryLockLongPress()) {
                    setHermesRotaryLocked(!isHermesRotaryLocked());
                }
                this->action = ROTARY_ACTION_NONE;
            }
            setIntervalFromNow(50);
            return INT32_MAX;
        }

        const uint32_t heldMs = (this->pressTracking && this->pressDownSinceMs != 0) ? (now - this->pressDownSinceMs) : 0;
        const bool reachedLongPress = heldMs >= kRotaryLockHoldMs;
        const bool suppressShortPress = shouldSuppressHermesRotaryShortPressAfterHold(heldMs);
        const bool shouldSendShortPress = this->pressTracking && !this->pressLongFired && !reachedLongPress &&
                                          !suppressShortPress && !isHermesRotaryLocked();
        this->pressTracking = false;
        this->pressLongFired = false;
        this->pressDownSinceMs = 0;
        this->action = ROTARY_ACTION_NONE;

        if (shouldSendShortPress) {
            if ((this->lastPressDispatchMs != 0) && ((now - this->lastPressDispatchMs) < kRotaryPressDebounceMs)) {
                return INT32_MAX;
            }
            this->lastPressDispatchMs = now;
            LOG_DEBUG("Rotary event Press");
            e.inputEvent = this->_eventPressed;
        }
    }

    if (isHermesRotaryLocked() && ((this->action == ROTARY_ACTION_CW) || (this->action == ROTARY_ACTION_CCW))) {
        this->action = ROTARY_ACTION_NONE;
        this->rotaryStep = 0;
        return INT32_MAX;
    }

    if ((this->action == ROTARY_ACTION_CW) || (this->action == ROTARY_ACTION_CCW)) {
        if ((this->lastRotaryDispatchMs != 0) && ((now - this->lastRotaryDispatchMs) < kRotaryDispatchMinMs)) {
            this->action = ROTARY_ACTION_NONE;
            return INT32_MAX;
        }
        this->lastRotaryDispatchMs = now;
    }

    if (this->action == ROTARY_ACTION_CW) {
        LOG_DEBUG("Rotary event CW");
        e.inputEvent = this->_directionSwapped ? this->_eventCcw : this->_eventCw;
    } else if (this->action == ROTARY_ACTION_CCW) {
        LOG_DEBUG("Rotary event CCW");
        e.inputEvent = this->_directionSwapped ? this->_eventCw : this->_eventCcw;
    }

    if (e.inputEvent != meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_NONE) {
        this->notifyObservers(&e);
    }

    this->action = ROTARY_ACTION_NONE;

    return INT32_MAX;
}

void RotaryEncoderInterruptBase::intPressHandler()
{
    this->action = ROTARY_ACTION_PRESSED;
    setIntervalFromNow(20); // TODO: this modifies a non-volatile variable!
}

void RotaryEncoderInterruptBase::intAHandler()
{
    // CW rotation (at least on most common rotary encoders)
    int currentLevelA = digitalRead(this->_pinA);
    if (this->rotaryLevelA == currentLevelA) {
        return;
    }
    this->rotaryLevelA = currentLevelA;
    this->rotaryStateCCW = intHandler(currentLevelA == HIGH, this->rotaryLevelB, ROTARY_ACTION_CCW, this->rotaryStateCCW);
}

void RotaryEncoderInterruptBase::intBHandler()
{
    // CW rotation (at least on most common rotary encoders)
    int currentLevelB = digitalRead(this->_pinB);
    if (this->rotaryLevelB == currentLevelB) {
        return;
    }
    this->rotaryLevelB = currentLevelB;
    this->rotaryStateCW = intHandler(currentLevelB == HIGH, this->rotaryLevelA, ROTARY_ACTION_CW, this->rotaryStateCW);
}

/**
 * @brief Rotary action implementation.
 *   We assume, the following pin setup:
 *    A   --||
 *    GND --||]========
 *    B   --||
 *
 * @return The new state for rotary pin.
 */
RotaryEncoderInterruptBaseStateType RotaryEncoderInterruptBase::intHandler(bool actualPinRaising, int otherPinLevel,
                                                                           RotaryEncoderInterruptBaseActionType action,
                                                                           RotaryEncoderInterruptBaseStateType state)
{
    (void)actualPinRaising;
    (void)otherPinLevel;
    (void)action;

    uint8_t currentState = ((this->rotaryLevelA == HIGH) ? 1 : 0) << 1 | ((this->rotaryLevelB == HIGH) ? 1 : 0);
    uint8_t transition = (this->lastRotaryState << 2) | currentState;
    int8_t movement = kRotaryTransitionTable[transition & 0x0f];

    if (this->action != ROTARY_ACTION_PRESSED) {
        if (movement != 0) {
            this->rotaryStep += movement;
            if (this->rotaryStep <= -kRotaryStepsPerDetent) {
                if (this->action == ROTARY_ACTION_NONE) {
                    this->action = ROTARY_ACTION_CW;
                    setIntervalFromNow(50); // TODO: this modifies a non-volatile variable!
                }
                this->rotaryStep = 0;
            } else if (this->rotaryStep >= kRotaryStepsPerDetent) {
                if (this->action == ROTARY_ACTION_NONE) {
                    this->action = ROTARY_ACTION_CCW;
                    setIntervalFromNow(50); // TODO: this modifies a non-volatile variable!
                }
                this->rotaryStep = 0;
            }
        } else if (currentState != this->lastRotaryState) {
            this->rotaryStep = 0;
        }
    } else {
        this->rotaryStep = 0;
    }

    this->lastRotaryState = currentState;
    return state;
}
