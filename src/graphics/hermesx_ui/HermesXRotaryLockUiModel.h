#pragma once

#include <cstdint>

namespace graphics
{

struct HermesXRotaryLockUiState {
    bool locked = false;
    bool visible = false;
    bool selectedLocked = false;
    uint32_t shownAtMs = 0;
};

class HermesXRotaryLockUiModel
{
  public:
    static HermesXRotaryLockUiModel &instance();

    HermesXRotaryLockUiState &state() { return state_; }
    const HermesXRotaryLockUiState &state() const { return state_; }

    void show(bool locked, uint32_t nowMs);
    void select(bool locked, uint32_t nowMs);
    void confirm();
    void cancel();
    bool expire(uint32_t nowMs, uint32_t timeoutMs);

  private:
    HermesXRotaryLockUiModel() = default;

    HermesXRotaryLockUiState state_;
};

} // namespace graphics
