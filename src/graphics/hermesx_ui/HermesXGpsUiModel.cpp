#include "HermesXGpsUiModel.h"

namespace graphics
{

HermesXGpsUiModel &HermesXGpsUiModel::instance()
{
    static HermesXGpsUiModel model;
    return model;
}

void HermesXGpsUiModel::invalidate()
{
    valid_ = false;
}

void HermesXGpsUiModel::resetLifecycleForWake()
{
    invalidate();
    posterVisible_ = false;
    basePainted_ = false;
    needsFullFrameAfterSwitch_ = true;
}

void HermesXGpsUiModel::handleFrameChange(bool switchedToGpsFrame, bool isCurrentGpsFrame)
{
    if (switchedToGpsFrame) {
        invalidate();
        posterVisible_ = false;
        basePainted_ = false;
        needsFullFrameAfterSwitch_ = true;
    } else if (!isCurrentGpsFrame) {
        basePainted_ = false;
        needsFullFrameAfterSwitch_ = false;
    }
}

void HermesXGpsUiModel::markUiFrameRendered(bool onFixedGpsFrame, bool uiRendered)
{
    if (onFixedGpsFrame && uiRendered) {
        basePainted_ = true;
        needsFullFrameAfterSwitch_ = false;
    } else if (!onFixedGpsFrame) {
        basePainted_ = false;
    }
}

HermesXGpsPosterVisibilityTransition HermesXGpsUiModel::visibilityTransition(bool visible) const
{
    HermesXGpsPosterVisibilityTransition transition;
    transition.entered = visible && !posterVisible_;
    transition.left = posterVisible_ && !visible;
    return transition;
}

bool HermesXGpsUiModel::isPosterDirty(const HermesXGpsPosterState &state,
                                      const HermesXGpsPosterLayers &layers,
                                      bool forceDirty) const
{
    if (forceDirty || !valid_ || cachedState_.width != state.width || cachedState_.height != state.height) {
        return true;
    }
    if (layers.title && cachedState_.gpsEnabled != state.gpsEnabled) {
        return true;
    }
    if (layers.decor && cachedState_.satelliteCount != state.satelliteCount) {
        return true;
    }
    if (!layers.coordinates) {
        return false;
    }
    return cachedState_.gpsConnected != state.gpsConnected || cachedState_.gpsHasLock != state.gpsHasLock ||
           cachedState_.fixedPosition != state.fixedPosition ||
           cachedState_.hasCoordinates != state.hasCoordinates || cachedState_.latitudeE7 != state.latitudeE7 ||
           cachedState_.longitudeE7 != state.longitudeE7 || cachedState_.altitude != state.altitude;
}

void HermesXGpsUiModel::commitPosterState(const HermesXGpsPosterState &state)
{
    cachedState_ = state;
    valid_ = true;
}

} // namespace graphics
