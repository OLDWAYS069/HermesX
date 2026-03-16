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
