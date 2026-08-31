#pragma once

#include "HermesXGpsUiModel.h"
#include "HermesXGpsUiRenderer.h"

#include <cstdint>
#include <limits>

namespace graphics
{

struct HermesXGpsStateInput {
    int16_t width = 0;
    int16_t height = 0;
    bool sourceAvailable = false;
    bool gpsEnabled = false;
    bool gpsConnected = false;
    bool gpsHasLock = false;
    bool fixedPosition = false;
    uint32_t satelliteCount = 0;
    int32_t latitudeE7 = std::numeric_limits<int32_t>::min();
    int32_t longitudeE7 = std::numeric_limits<int32_t>::min();
    int32_t altitude = std::numeric_limits<int32_t>::min();
};

struct HermesXGpsStateSnapshot {
    bool gpsEnabled = false;
    HermesXGpsPosterState poster;
    HermesXGpsPosterCoordinateView coordinates;
    HermesXGpsPosterDecorView decor;
};

class HermesXGpsStateCollector
{
  public:
    static HermesXGpsStateSnapshot collect(const HermesXGpsStateInput &input,
                                           const HermesXGpsPosterLayers &layers);
};

} // namespace graphics
