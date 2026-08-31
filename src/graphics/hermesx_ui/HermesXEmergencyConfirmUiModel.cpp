#include "HermesXEmergencyConfirmUiModel.h"

namespace graphics
{

HermesXEmergencyConfirmUiModel &HermesXEmergencyConfirmUiModel::instance()
{
    static HermesXEmergencyConfirmUiModel model;
    return model;
}

void HermesXEmergencyConfirmUiModel::show(uint32_t remainingSeconds)
{
    state_.visible = true;
    state_.cancelRequested = false;
    state_.remainingSeconds = remainingSeconds;
}

void HermesXEmergencyConfirmUiModel::update(uint32_t remainingSeconds)
{
    state_.remainingSeconds = remainingSeconds;
}

void HermesXEmergencyConfirmUiModel::hide()
{
    state_.visible = false;
    state_.remainingSeconds = 0;
}

void HermesXEmergencyConfirmUiModel::requestCancel()
{
    state_.cancelRequested = true;
    hide();
}

bool HermesXEmergencyConfirmUiModel::consumeCancelRequest()
{
    const bool requested = state_.cancelRequested;
    state_.cancelRequested = false;
    return requested;
}

} // namespace graphics
