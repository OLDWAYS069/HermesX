#include "HermesXRotaryLockUiModel.h"

namespace graphics
{

HermesXRotaryLockUiModel &HermesXRotaryLockUiModel::instance()
{
    static HermesXRotaryLockUiModel model;
    return model;
}

void HermesXRotaryLockUiModel::show(bool locked, uint32_t nowMs)
{
    state_.locked = locked;
    state_.visible = true;
    state_.selectedLocked = locked;
    state_.shownAtMs = nowMs;
}

void HermesXRotaryLockUiModel::select(bool locked, uint32_t nowMs)
{
    state_.selectedLocked = locked;
    state_.shownAtMs = nowMs;
}

void HermesXRotaryLockUiModel::confirm()
{
    state_.locked = state_.selectedLocked;
    state_.visible = false;
    state_.shownAtMs = 0;
}

void HermesXRotaryLockUiModel::cancel()
{
    state_.visible = false;
    state_.selectedLocked = state_.locked;
    state_.shownAtMs = 0;
}

bool HermesXRotaryLockUiModel::expire(uint32_t nowMs, uint32_t timeoutMs)
{
    if (!state_.visible || state_.shownAtMs == 0 || (nowMs - state_.shownAtMs) < timeoutMs) {
        return false;
    }
    state_.visible = false;
    return true;
}

} // namespace graphics
