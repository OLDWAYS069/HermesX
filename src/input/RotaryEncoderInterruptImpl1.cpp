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
    // HermesX: rotEnc1 is always interpreted as CW/CCW/Press -> Down/Up/Select.
    // Ignore configurable event remap here to avoid accidental Cancel/Back behavior.
    const char eventCw = static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
    const char eventCcw = static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
    const char eventPressed = static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);

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
