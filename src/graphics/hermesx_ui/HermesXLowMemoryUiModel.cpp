#include "HermesXLowMemoryUiModel.h"

namespace graphics
{

HermesXLowMemoryUiModel &HermesXLowMemoryUiModel::instance()
{
    static HermesXLowMemoryUiModel model;
    return model;
}

void HermesXLowMemoryUiModel::show(uint32_t freeHeap, uint32_t largestBlock)
{
    state_.visible = true;
    state_.selected = 0;
    setMeasurements(freeHeap, largestBlock);
    clearStatus();
}

void HermesXLowMemoryUiModel::dismiss(uint32_t suppressUntilMs)
{
    state_.visible = false;
    state_.suppressUntilMs = suppressUntilMs;
    clearStatus();
}

void HermesXLowMemoryUiModel::setMeasurements(uint32_t freeHeap, uint32_t largestBlock)
{
    state_.triggerFree = freeHeap;
    state_.triggerLargest = largestBlock;
}

void HermesXLowMemoryUiModel::clearStatus()
{
    state_.status[0] = '\0';
}

} // namespace graphics
