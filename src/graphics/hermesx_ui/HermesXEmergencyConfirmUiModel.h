#pragma once

#include <cstdint>

namespace graphics
{

struct HermesXEmergencyConfirmUiState {
    bool visible = false;
    bool cancelRequested = false;
    uint32_t remainingSeconds = 0;
};

class HermesXEmergencyConfirmUiModel
{
  public:
    static HermesXEmergencyConfirmUiModel &instance();

    HermesXEmergencyConfirmUiState &state() { return state_; }
    const HermesXEmergencyConfirmUiState &state() const { return state_; }

    void show(uint32_t remainingSeconds);
    void update(uint32_t remainingSeconds);
    void hide();
    void requestCancel();
    bool consumeCancelRequest();

  private:
    HermesXEmergencyConfirmUiModel() = default;

    HermesXEmergencyConfirmUiState state_;
};

} // namespace graphics
