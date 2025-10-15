#pragma once

#include <stdint.h>

struct SleepPreHookParams {
    uint32_t suggested_duration_ms;
};

/**
 * Provide parameters for the next deep sleep preparation effect.
 */
void setNextSleepPreHookParams(const SleepPreHookParams &params);

/**
 * Platform/application-specific hook that plays shutdown effects prior to deep sleep.
 */
void runPreDeepSleepHook(const SleepPreHookParams &params);
