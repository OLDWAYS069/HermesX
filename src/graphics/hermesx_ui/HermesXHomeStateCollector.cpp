#include "HermesXHomeStateCollector.h"

#include <algorithm>
#include <cstdio>
#include <ctime>

namespace graphics
{
HermesXHomeBaseState HermesXHomeStateSnapshot::baseState() const
{
    HermesXHomeBaseState state;
    state.stealth = stealth;
    state.hasBattery = battery.available;
    state.batteryPercent = battery.percent;
    state.satelliteCount = satelliteCount;
    state.role = role;
    state.date = date;
    return state;
}

HermesXHomeStateCollector &HermesXHomeStateCollector::instance()
{
    static HermesXHomeStateCollector collector;
    return collector;
}

HermesXHomeBatteryState HermesXHomeStateCollector::collectBattery(const HermesXHomeBatterySource &source)
{
    if (source.available && source.voltageMv > 0) {
        cachedBattery_.available = true;
        cachedBattery_.voltageMv = source.voltageMv;
        cachedBattery_.percent = std::min<uint8_t>(100, source.percent);
    } else if (!source.available) {
        cachedBattery_ = HermesXHomeBatteryState{};
    }
    return cachedBattery_;
}

void HermesXHomeStateCollector::collect(const HermesXHomeStateInput &input, HermesXHomeStateSnapshot &snapshot)
{
    snapshot = HermesXHomeStateSnapshot{};
    snapshot.hasValidTime = formatTimeDate(
        input.rtcSeconds, snapshot.time, sizeof(snapshot.time), snapshot.date, sizeof(snapshot.date));
    snapshot.battery = collectBattery(input.battery);
    snapshot.satelliteCount = input.gpsConnected
                                  ? static_cast<uint8_t>(input.satelliteCount > 99U ? 99U : input.satelliteCount)
                                  : 0;
    snapshot.stealth = input.stealth;
    snapshot.role = input.role;
}

bool HermesXHomeStateCollector::formatTimeDate(uint32_t rtcSeconds,
                                               char *timeBuffer,
                                               size_t timeBufferSize,
                                               char *dateBuffer,
                                               size_t dateBufferSize)
{
    if (!timeBuffer || timeBufferSize == 0 || !dateBuffer || dateBufferSize == 0) {
        return false;
    }
    if (rtcSeconds > 0) {
        time_t timeValue = rtcSeconds;
        tm *localTime = gmtime(&timeValue);
        if (localTime) {
            std::snprintf(timeBuffer,
                          timeBufferSize,
                          "%02d:%02d:%02d",
                          localTime->tm_hour,
                          localTime->tm_min,
                          localTime->tm_sec);
            std::snprintf(dateBuffer,
                          dateBufferSize,
                          "%04d/%02d/%02d",
                          localTime->tm_year + 1900,
                          localTime->tm_mon + 1,
                          localTime->tm_mday);
            return true;
        }
    }

    std::snprintf(timeBuffer, timeBufferSize, "--:--:--");
    std::snprintf(dateBuffer, dateBufferSize, "等待授時");
    return false;
}

} // namespace graphics
