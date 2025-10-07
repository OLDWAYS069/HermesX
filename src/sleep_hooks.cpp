// src/sleep_hooks_impl.cpp
#include "sleep_hooks.h"
#include <Arduino.h>

// 如果你會用到 HermesX 的關機動畫，就把它的標頭引進來；沒用也不會出錯
#include "modules/HermesXInterfaceModule.h"

__attribute__((weak))

void runPreDeepSleepHook(const SleepPreHookParams &params)
{
    // 沒有 HermesX 的環境：至少等一下，不讓系統立刻睡
    uint32_t ms = params.suggested_duration_ms ? params.suggested_duration_ms : 700;
    delay(ms);
}