#pragma once

#include "HermesXGpsUiModel.h"

#include <cstdint>

namespace graphics
{

using HermesXGpsRuntimeWorkspaceEnsure = bool (*)();
using HermesXGpsRuntimeWorkspaceRelease = void (*)();

struct HermesXGpsRuntimeInput {
    bool showingNormalScreen = false;
    bool frameFixed = false;
    uint8_t currentFrameIndex = 0xFF;
    uint8_t gpsFrameIndex = 0xFF;
    uint8_t frameCount = 0;
    bool directOverlaySupported = false;
    bool releaseWorkspaceWhenInactive = false;
    HermesXGpsPosterLayers layers;
};

struct HermesXGpsRuntimeDecision {
    bool frameChanged = false;
    bool switchedToGpsFrame = false;
    bool isCurrentGpsFrame = false;
    bool onFixedGpsFrame = false;
    bool resetPaletteForFrameSwitch = false;
    bool candidate = false;
    bool workspaceReady = false;
    bool visible = false;
    HermesXGpsPosterLayers layers;
};

class HermesXGpsUiController
{
  public:
    static HermesXGpsUiController &instance();

    void resetForWake();
    HermesXGpsRuntimeDecision evaluateRuntime(const HermesXGpsRuntimeInput &input,
                                              HermesXGpsRuntimeWorkspaceEnsure ensureWorkspace,
                                              HermesXGpsRuntimeWorkspaceRelease releaseWorkspace);
    void commitFrame(uint8_t frameIndex) { lastFrameIndex_ = frameIndex; }

  private:
    HermesXGpsUiController() = default;

    uint8_t lastFrameIndex_ = 0xFF;
};

} // namespace graphics
