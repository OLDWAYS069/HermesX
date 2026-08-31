#pragma once

#include "HermesXHomeUiModel.h"

#include <cstdint>

namespace graphics
{

struct HermesXDirectHomeFrameDecision {
    bool active = false;
    bool entering = false;
    bool left = false;
    bool basePainted = false;
    bool canSkipUi = false;
    bool baseRedrawRequested = false;
    bool forceDogRedraw = false;
    uint16_t dogFrame = 0xFFFF;
    uint8_t dogPose = 0;
    HermesXHomeBaseDecision base;
    HermesXHomeDogCache previousDog;
};

struct HermesXDirectHomeRuntimeInput {
    bool onFixedMainFrame = false;
    bool smartPowerHomeActive = false;
    bool directOverlaySupported = false;
    bool incomingTextPopupActive = false;
    bool emergencyConfirmVisible = false;
    bool emergencyUiActive = false;
    bool lowMemoryReminderVisible = false;
    bool paletteResetPending = false;
};

struct HermesXDirectHomeRuntimeDecision {
    bool active = false;
    bool releaseNeonBuffers = false;
    bool uiSkipBlocked = false;
    uint8_t requiredFps = 0;
    uint16_t dogFrame = 0xFFFF;
};

class HermesXHomeUiController
{
  public:
    static constexpr uint8_t DogFrameRate = 15;
    static constexpr uint32_t BaseRefreshIntervalMs = 3000;

    static HermesXHomeUiController &instance();

    void resetForWake();
    HermesXDirectHomeRuntimeDecision evaluateRuntime(const HermesXDirectHomeRuntimeInput &input,
                                                      uint32_t nowMs) const;
    HermesXDirectHomeFrameDecision beginActiveFrame(const HermesXHomeBaseState &state,
                                                    uint32_t nowMs,
                                                    const HermesXDirectHomeRuntimeDecision &runtime);
    HermesXDirectHomeFrameDecision deactivate();
    void invalidateForPaletteReset();
    void markUiFrameRendered(bool active, bool uiRendered);
    bool shouldRenderDog(uint16_t frame, bool forceRedraw) const;
    void commitDogFrame(uint16_t frame);

    bool isVisible() const { return visible_; }
    uint8_t dogPose() const { return dogPose_; }

  private:
    HermesXHomeUiController() = default;

    bool visible_ = false;
    bool basePainted_ = false;
    uint8_t dogPose_ = 0;
    uint8_t dogEntryCount_ = 0;
};

} // namespace graphics
