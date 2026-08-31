#pragma once

#include <cstdint>

namespace graphics
{

struct HermesXLowMemoryUiState {
    bool visible = false;
    uint8_t selected = 0;
    uint32_t suppressUntilMs = 0;
    uint32_t triggerFree = 0;
    uint32_t triggerLargest = 0;
    char status[48] = "";
};

class HermesXLowMemoryUiModel
{
  public:
    static HermesXLowMemoryUiModel &instance();

    HermesXLowMemoryUiState &state() { return state_; }
    const HermesXLowMemoryUiState &state() const { return state_; }

    void show(uint32_t freeHeap, uint32_t largestBlock);
    void dismiss(uint32_t suppressUntilMs);
    void setMeasurements(uint32_t freeHeap, uint32_t largestBlock);
    void clearStatus();

  private:
    HermesXLowMemoryUiModel() = default;

    HermesXLowMemoryUiState state_;
};

} // namespace graphics
