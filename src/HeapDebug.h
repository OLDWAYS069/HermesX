#pragma once

#include "configuration.h"
#include "memGet.h"

inline void logHeapSnapshot(const char *label)
{
#ifdef ARCH_ESP32
    LOG_DEBUG("[Heap] %s free=%u min=%u largest=%u psram=%u", label, memGet.getFreeHeap(), memGet.getMinFreeHeap(),
              memGet.getLargestFreeBlock(), memGet.getFreePsram());
#else
    (void)label;
#endif
}

inline bool hasHeapHeadroomForCriticalAlloc(const char *label,
                                            uint32_t freeThreshold = 12 * 1024,
                                            uint32_t largestThreshold = 4 * 1024)
{
#ifdef ARCH_ESP32
    const uint32_t freeHeap = memGet.getFreeHeap();
    const uint32_t largest = memGet.getLargestFreeBlock();
    if (freeHeap < freeThreshold || largest < largestThreshold) {
        LOG_WARN("[Heap] skip %s free=%u min=%u largest=%u psram=%u", label, freeHeap, memGet.getMinFreeHeap(), largest,
                 memGet.getFreePsram());
        return false;
    }
#else
    (void)label;
    (void)freeThreshold;
    (void)largestThreshold;
#endif
    return true;
}
