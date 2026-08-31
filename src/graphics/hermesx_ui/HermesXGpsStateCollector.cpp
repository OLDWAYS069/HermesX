#include "HermesXGpsStateCollector.h"

#include <algorithm>

namespace graphics
{

HermesXGpsStateSnapshot HermesXGpsStateCollector::collect(const HermesXGpsStateInput &input,
                                                          const HermesXGpsPosterLayers &layers)
{
    HermesXGpsStateSnapshot snapshot;
    snapshot.gpsEnabled = input.gpsEnabled;
    snapshot.poster.width = input.width;
    snapshot.poster.height = input.height;
    snapshot.poster.gpsEnabled = input.gpsEnabled;

    if (layers.decor && input.sourceAvailable && input.gpsConnected) {
        snapshot.poster.satelliteCount = static_cast<uint8_t>(std::min<uint32_t>(99, input.satelliteCount));
    }
    snapshot.decor.satelliteCount = snapshot.poster.satelliteCount;

    if (layers.coordinates) {
        snapshot.poster.gpsConnected = input.sourceAvailable && input.gpsConnected;
        snapshot.poster.gpsHasLock = input.sourceAvailable && input.gpsHasLock;
        snapshot.poster.fixedPosition = input.fixedPosition;
        snapshot.poster.hasCoordinates = input.sourceAvailable && input.gpsEnabled &&
                                         (input.fixedPosition ||
                                          (snapshot.poster.gpsConnected && snapshot.poster.gpsHasLock));
        if (snapshot.poster.hasCoordinates) {
            snapshot.poster.latitudeE7 = input.latitudeE7;
            snapshot.poster.longitudeE7 = input.longitudeE7;
            snapshot.poster.altitude = input.altitude;
            snapshot.coordinates.hasCoordinates = true;
            snapshot.coordinates.latitude = static_cast<double>(input.latitudeE7) * 1e-7;
            snapshot.coordinates.longitude = static_cast<double>(input.longitudeE7) * 1e-7;
            snapshot.coordinates.hasAltitude = input.altitude != std::numeric_limits<int32_t>::min();
            snapshot.coordinates.altitudeMeters = input.altitude;
        }
    }
    return snapshot;
}

} // namespace graphics
