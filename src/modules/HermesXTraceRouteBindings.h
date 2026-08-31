#pragma once

#include <cstdint>

class HermesXTraceRouteBindings
{
  public:
    static constexpr uint8_t Capacity = 16;

    static HermesXTraceRouteBindings &instance();

    uint8_t count();
    uint32_t nodeAt(uint8_t index);
    bool contains(uint32_t nodeNum);
    bool bind(uint32_t nodeNum);
    bool unbindAt(uint8_t index);

  private:
    HermesXTraceRouteBindings() = default;

    void ensureLoaded();
    bool save();

    uint32_t nodes_[Capacity]{};
    uint8_t count_ = 0;
    bool loaded_ = false;
};
