#include "RotaryEncoderInterruptBase.h"
#include "configuration.h"

namespace
{
constexpr uint32_t kRotaryDispatchMinMs = 40;
constexpr int8_t kRotaryStepsPerDetent = 4;
const int8_t kRotaryTransitionTable[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
}

RotaryEncoderInterruptBase::RotaryEncoderInterruptBase(const char *name) : concurrency::OSThread(name)
{
    this->_originName = name;
}

void RotaryEncoderInterruptBase::init(
    uint8_t pinA, uint8_t pinB, uint8_t pinPress, char eventCw, char eventCcw, char eventPressed,
    //    std::function<void(void)> onIntA, std::function<void(void)> onIntB, std::function<void(void)> onIntPress) :
    void (*onIntA)(), void (*onIntB)(), void (*onIntPress)())
{
    this->_pinA = pinA;
    this->_pinB = pinB;
    this->_eventCw = eventCw;
    this->_eventCcw = eventCcw;
    this->_eventPressed = eventPressed;

    pinMode(pinPress, INPUT_PULLUP);
    pinMode(this->_pinA, INPUT_PULLUP);
    pinMode(this->_pinB, INPUT_PULLUP);

    //    attachInterrupt(pinPress, onIntPress, RISING);
    attachInterrupt(pinPress, onIntPress, RISING);
    attachInterrupt(this->_pinA, onIntA, CHANGE);
    attachInterrupt(this->_pinB, onIntB, CHANGE);

    this->rotaryLevelA = digitalRead(this->_pinA);
    this->rotaryLevelB = digitalRead(this->_pinB);
    this->lastRotaryState = ((this->rotaryLevelA == HIGH) ? 1 : 0) << 1 | ((this->rotaryLevelB == HIGH) ? 1 : 0);
    this->rotaryStep = 0;
    LOG_INFO("Rotary initialized (%d, %d, %d)", this->_pinA, this->_pinB, pinPress);
}

int32_t RotaryEncoderInterruptBase::runOnce()
{
    InputEvent e;
    e.inputEvent = meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_NONE;
    e.source = this->_originName;
    e.kbchar = 0x00;
    e.touchX = 0;
    e.touchY = 0;

    if ((this->action == ROTARY_ACTION_CW) || (this->action == ROTARY_ACTION_CCW)) {
        uint32_t now = millis();
        if ((this->lastRotaryDispatchMs != 0) && ((now - this->lastRotaryDispatchMs) < kRotaryDispatchMinMs)) {
            this->action = ROTARY_ACTION_NONE;
            return INT32_MAX;
        }
        this->lastRotaryDispatchMs = now;
    }

    if (this->action == ROTARY_ACTION_PRESSED) {
        LOG_DEBUG("Rotary event Press");
        e.inputEvent = this->_eventPressed;
    } else if (this->action == ROTARY_ACTION_CW) {
        LOG_DEBUG("Rotary event CW");
        e.inputEvent = this->_eventCw;
    } else if (this->action == ROTARY_ACTION_CCW) {
        LOG_DEBUG("Rotary event CCW");
        e.inputEvent = this->_eventCcw;
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
