#include "sleep_hooks.h"
#include <Arduino.h>

#if !MESHTASTIC_EXCLUDE_HERMESX
#include "modules/HermesXInterfaceModule.h"
#include "modules/HermesXPowerGuard.h"
#endif

__attribute__((weak))

void runPreDeepSleepHook(const SleepPreHookParams &params)
{
    uint32_t ms = params.suggested_duration_ms ? params.suggested_duration_ms : 700;
#if !MESHTASTIC_EXCLUDE_HERMESX && defined(HERMESX_GUARD_POWER_ANIMATIONS)
    if (HermesXPowerGuard::consumeShutdownAnimationSuppression()) {
        return;
    }
#endif
    delay(ms);
}
