#pragma once

#include <Arduino.h>
#include "mesh/generated/meshtastic/deviceonly.pb.h"
#include <cstdint>

namespace graphics
{

class HermesXNodeBrowserDataSource
{
  public:
    static HermesXNodeBrowserDataSource &instance();

    uint8_t refreshOnline();
    uint8_t refreshFinder();
    uint8_t refreshTraceRoute();

    const meshtastic_NodeInfoLite *onlineNodeAt(uint8_t index) const;
    const meshtastic_NodeInfoLite *finderNodeAt(uint8_t index) const;
    const meshtastic_NodeInfoLite *traceRouteNodeAt(uint8_t index) const;
    const meshtastic_NodeInfoLite *nodeByNum(uint32_t nodeNum) const;
    const meshtastic_NodeInfoLite *selectedOnlineNode();
    const meshtastic_NodeInfoLite *selectedFinderNode();
    uint8_t traceRouteCount() const { return traceRouteCount_; }
    uint32_t findTraceRouteNodeByShortName(const String &shortName) const;

  private:
    HermesXNodeBrowserDataSource() = default;

    static constexpr uint16_t TraceRouteCapacity = 250;
    static bool isOnlineCandidate(const meshtastic_NodeInfoLite &node);
    static bool isFinderCandidate(const meshtastic_NodeInfoLite &node);
    static float finderDistanceScore(const meshtastic_NodeInfoLite &node);

    uint16_t traceRouteOrder_[TraceRouteCapacity]{};
    uint8_t traceRouteCount_ = 0;
};

} // namespace graphics
