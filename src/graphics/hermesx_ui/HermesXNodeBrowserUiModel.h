#pragma once

#include <cstdint>

namespace graphics
{

struct HermesXNodeBrowserState {
    static constexpr uint16_t Capacity = 250;

    uint16_t order[Capacity]{};
    uint8_t count = 0;
    uint8_t listCursor = 0;
    uint8_t selectedIndex = 0;
    uint8_t detailCursor = 0;
};

struct HermesXGroupBrowserState {
    uint8_t menuCursor = 0;
    bool nodeListVisible = false;
    uint8_t listCursor = 0;
    uint8_t selectedIndex = 0;
    uint8_t detailCursor = 0;
};

struct HermesXFinderPulseState {
    bool confirmVisible = false;
    uint8_t confirmSelected = 0;
    uint32_t confirmShownAtMs = 0;
    bool dispatched = false;
    bool sendingVisible = false;
    uint32_t sendingShownAtMs = 0;
    uint32_t lastNavAtMs = 0;
    int8_t lastNavDir = 0;
};

class HermesXNodeBrowserUiModel
{
  public:
    static HermesXNodeBrowserUiModel &instance();

    HermesXNodeBrowserState &onlineState() { return online_; }
    HermesXNodeBrowserState &finderState() { return finder_; }
    HermesXGroupBrowserState &groupState() { return group_; }
    HermesXFinderPulseState &finderPulseState() { return finderPulse_; }
    const HermesXFinderPulseState &finderPulseState() const { return finderPulse_; }

    void reset(HermesXNodeBrowserState &state);
    void clamp(HermesXNodeBrowserState &state);
    void resetGroupList();
    void clampGroup(uint8_t nodeCount);

  private:
    HermesXNodeBrowserUiModel() = default;

    HermesXNodeBrowserState online_;
    HermesXNodeBrowserState finder_;
    HermesXGroupBrowserState group_;
    HermesXFinderPulseState finderPulse_;
};

} // namespace graphics
