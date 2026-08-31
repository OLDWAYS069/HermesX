#pragma once

#include <cstdint>
#include <limits>

namespace graphics
{

struct HermesXGpsPosterLayers {
    bool title = false;
    bool decor = false;
    bool coordinates = false;
};

struct HermesXGpsPosterState {
    bool gpsEnabled = false;
    bool gpsConnected = false;
    bool gpsHasLock = false;
    uint8_t satelliteCount = 0;
    bool fixedPosition = false;
    bool hasCoordinates = false;
    int32_t latitudeE7 = std::numeric_limits<int32_t>::min();
    int32_t longitudeE7 = std::numeric_limits<int32_t>::min();
    int32_t altitude = std::numeric_limits<int32_t>::min();
    int16_t width = 0;
    int16_t height = 0;
};

struct HermesXGpsPosterVisibilityTransition {
    bool entered = false;
    bool left = false;
};

class HermesXGpsUiModel
{
  public:
    static HermesXGpsUiModel &instance();

    void invalidate();
    void invalidatePoster()
    {
        invalidate();
        basePainted_ = false;
    }
    void resetLifecycleForWake();
    void handleFrameChange(bool switchedToGpsFrame, bool isCurrentGpsFrame);
    void invalidateBase() { basePainted_ = false; }
    void markUiFrameRendered(bool onFixedGpsFrame, bool uiRendered);
    bool isBasePainted() const { return basePainted_; }
    bool needsFullFrameAfterSwitch() const { return needsFullFrameAfterSwitch_; }
    HermesXGpsPosterVisibilityTransition visibilityTransition(bool visible) const;
    void commitVisibility(bool visible) { posterVisible_ = visible; }
    void clearVisibility() { posterVisible_ = false; }
    bool isValid() const { return valid_; }
    bool isPosterDirty(const HermesXGpsPosterState &state,
                       const HermesXGpsPosterLayers &layers,
                       bool forceDirty = false) const;
    void commitPosterState(const HermesXGpsPosterState &state);

  private:
    HermesXGpsUiModel() = default;

    bool valid_ = false;
    bool posterVisible_ = false;
    bool basePainted_ = false;
    bool needsFullFrameAfterSwitch_ = false;
    HermesXGpsPosterState cachedState_;
};

} // namespace graphics
