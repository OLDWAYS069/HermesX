#pragma once

#include "HermesXHomeUiModel.h"

#include <cstddef>
#include <cstdint>

namespace graphics
{

struct HermesXHomeBatterySource {
    bool available = false;
    int voltageMv = 0;
};

struct HermesXHomeBatteryState {
    bool available = false;
    int voltageMv = 0;
    uint8_t percent = 0;
};

struct HermesXHomeStateInput {
    uint32_t rtcSeconds = 0;
    HermesXHomeBatterySource battery;
    bool gpsConnected = false;
    uint32_t satelliteCount = 0;
    bool stealth = false;
    int32_t role = 0;
};

struct HermesXHomeStateSnapshot {
    bool hasValidTime = false;
    HermesXHomeBatteryState battery;
    uint8_t satelliteCount = 0;
    bool stealth = false;
    int32_t role = 0;
    char time[16]{};
    char date[24]{};

    HermesXHomeBaseState baseState() const;
};

class HermesXHomeStateCollector
{
  public:
    static HermesXHomeStateCollector &instance();

    HermesXHomeBatteryState collectBattery(const HermesXHomeBatterySource &source);
    void collect(const HermesXHomeStateInput &input, HermesXHomeStateSnapshot &snapshot);
    static bool formatTimeDate(uint32_t rtcSeconds,
                               char *timeBuffer,
                               size_t timeBufferSize,
                               char *dateBuffer,
                               size_t dateBufferSize);

  private:
    HermesXHomeStateCollector() = default;

    HermesXHomeBatteryState cachedBattery_;
};

} // namespace graphics
