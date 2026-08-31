#include "HermesXGpsUiController.h"

namespace graphics
{

HermesXGpsUiController &HermesXGpsUiController::instance()
{
    static HermesXGpsUiController controller;
    return controller;
}

void HermesXGpsUiController::resetForWake()
{
    lastFrameIndex_ = 0xFF;
    HermesXGpsUiModel::instance().resetLifecycleForWake();
}

HermesXGpsRuntimeDecision HermesXGpsUiController::evaluateRuntime(
    const HermesXGpsRuntimeInput &input,
    HermesXGpsRuntimeWorkspaceEnsure ensureWorkspace,
    HermesXGpsRuntimeWorkspaceRelease releaseWorkspace)
{
    auto &model = HermesXGpsUiModel::instance();
    HermesXGpsRuntimeDecision decision;
    decision.layers = input.layers;
    decision.frameChanged = input.currentFrameIndex != lastFrameIndex_;
    decision.isCurrentGpsFrame = input.showingNormalScreen && input.gpsFrameIndex < input.frameCount &&
                                 input.currentFrameIndex == input.gpsFrameIndex;
    decision.switchedToGpsFrame = decision.frameChanged && decision.isCurrentGpsFrame;
    decision.onFixedGpsFrame = decision.isCurrentGpsFrame && input.frameFixed;
    decision.resetPaletteForFrameSwitch = decision.switchedToGpsFrame;

    if (decision.switchedToGpsFrame) {
        model.handleFrameChange(true, true);
    } else if (decision.frameChanged && !decision.isCurrentGpsFrame) {
        model.handleFrameChange(false, false);
    }

    const bool anyLayerEnabled = input.layers.title || input.layers.decor || input.layers.coordinates;
    decision.candidate = anyLayerEnabled && decision.onFixedGpsFrame && input.directOverlaySupported;
    if (decision.candidate && ensureWorkspace) {
        decision.workspaceReady = ensureWorkspace();
    } else if (input.releaseWorkspaceWhenInactive && releaseWorkspace) {
        releaseWorkspace();
    }
    decision.visible = decision.candidate && decision.workspaceReady;
    return decision;
}

} // namespace graphics
