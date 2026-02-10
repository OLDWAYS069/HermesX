#pragma once
#include "RotaryEncoderInterruptBase.h"
#ifdef ARCH_ESP32
#include "sleep.h"
#endif

/**
 * @brief The idea behind this class to have static methods for the event handlers.
 *      Check attachInterrupt() at RotaryEncoderInteruptBase.cpp
 *      Technically you can have as many rotary encoders hardver attached
 *      to your device as you wish, but you always need to have separate event
 *      handlers, thus you need to have a RotaryEncoderInterrupt implementation.
 */
class RotaryEncoderInterruptImpl1 : public RotaryEncoderInterruptBase
{
  public:
    RotaryEncoderInterruptImpl1();
    bool init();
    static void handleIntA();
    static void handleIntB();
    static void handleIntPressed();

#ifdef ARCH_ESP32
    int beforeLightSleep(void *unused);
    int afterLightSleep(esp_sleep_wakeup_cause_t cause);
#endif

  private:
#ifdef ARCH_ESP32
    CallbackObserver<RotaryEncoderInterruptImpl1, void *> lsObserver =
        CallbackObserver<RotaryEncoderInterruptImpl1, void *>(this, &RotaryEncoderInterruptImpl1::beforeLightSleep);
    CallbackObserver<RotaryEncoderInterruptImpl1, esp_sleep_wakeup_cause_t> lsEndObserver =
        CallbackObserver<RotaryEncoderInterruptImpl1, esp_sleep_wakeup_cause_t>(this,
                                                                                &RotaryEncoderInterruptImpl1::afterLightSleep);
#endif
};

extern RotaryEncoderInterruptImpl1 *rotaryEncoderInterruptImpl1;
