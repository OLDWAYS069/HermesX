#include "RotaryEncoderInterruptImpl1.h"
#include "InputBroker.h"
#ifdef ARCH_ESP32
#include "sleep.h"
#endif

RotaryEncoderInterruptImpl1 *rotaryEncoderInterruptImpl1;

RotaryEncoderInterruptImpl1::RotaryEncoderInterruptImpl1() : RotaryEncoderInterruptBase("rotEnc1")
{
#ifdef ARCH_ESP32
    lsObserver.observe(&notifyLightSleep);
    lsEndObserver.observe(&notifyLightSleepEnd);
#endif
}

bool RotaryEncoderInterruptImpl1::init()
{
    if (!moduleConfig.canned_message.rotary1_enabled) {
        // Input device is disabled.
        disable();
        return false;
    }

    uint8_t pinA = moduleConfig.canned_message.inputbroker_pin_a;
    uint8_t pinB = moduleConfig.canned_message.inputbroker_pin_b;
    uint8_t pinPress = moduleConfig.canned_message.inputbroker_pin_press;
    const char configuredCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char configuredCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
    const char configuredPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);
    const char eventNone = static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_NONE);
    const char eventCw =
        (configuredCw == eventNone) ? static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN)
                                    : configuredCw;
    const char eventCcw =
        (configuredCcw == eventNone) ? static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP)
                                     : configuredCcw;
    const char eventPressed =
        (configuredPress == eventNone)
            ? static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT)
            : configuredPress;

    // moduleConfig.canned_message.ext_notification_module_output
    RotaryEncoderInterruptBase::init(pinA, pinB, pinPress, eventCw, eventCcw, eventPressed,
                                     RotaryEncoderInterruptImpl1::handleIntA, RotaryEncoderInterruptImpl1::handleIntB,
                                     RotaryEncoderInterruptImpl1::handleIntPressed);
    inputBroker->registerSource(this);
    return true;
}

void RotaryEncoderInterruptImpl1::handleIntA()
{
    rotaryEncoderInterruptImpl1->intAHandler();
}
void RotaryEncoderInterruptImpl1::handleIntB()
{
    rotaryEncoderInterruptImpl1->intBHandler();
}
void RotaryEncoderInterruptImpl1::handleIntPressed()
{
    rotaryEncoderInterruptImpl1->intPressHandler();
}

#ifdef ARCH_ESP32
int RotaryEncoderInterruptImpl1::beforeLightSleep(void *unused)
{
    (void)unused;
    detachInterrupts();
    return 0;
}

int RotaryEncoderInterruptImpl1::afterLightSleep(esp_sleep_wakeup_cause_t cause)
{
    (void)cause;
    attachInterrupts();
    return 0;
}
#endif
