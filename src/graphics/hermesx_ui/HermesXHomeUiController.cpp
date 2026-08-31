#include "HermesXHomeUiController.h"

namespace graphics
{

HermesXHomeUiController &HermesXHomeUiController::instance()
{
    static HermesXHomeUiController controller;
    return controller;
}

void HermesXHomeUiController::resetForWake()
{
    auto &model = HermesXHomeUiModel::instance();
    model.reset();
    model.resetDog();
    visible_ = false;
    basePainted_ = false;
    dogPose_ = 0;
    dogEntryCount_ = 0;
}

HermesXDirectHomeRuntimeDecision HermesXHomeUiController::evaluateRuntime(
    const HermesXDirectHomeRuntimeInput &input,
    uint32_t nowMs) const
{
    HermesXDirectHomeRuntimeDecision decision;
    decision.active = input.onFixedMainFrame && !input.smartPowerHomeActive && input.directOverlaySupported &&
                      !input.incomingTextPopupActive && !input.emergencyConfirmVisible && !input.emergencyUiActive &&
                      !input.lowMemoryReminderVisible;
    decision.releaseNeonBuffers = input.onFixedMainFrame && input.smartPowerHomeActive;
    decision.uiSkipBlocked = input.incomingTextPopupActive || input.paletteResetPending;
    decision.requiredFps = decision.active ? DogFrameRate : 0;
    decision.dogFrame = static_cast<uint16_t>((static_cast<uint64_t>(nowMs) * DogFrameRate) / 1000U);
    return decision;
}

HermesXDirectHomeFrameDecision HermesXHomeUiController::beginActiveFrame(
    const HermesXHomeBaseState &state,
    uint32_t nowMs,
    const HermesXDirectHomeRuntimeDecision &runtime)
{
    auto &model = HermesXHomeUiModel::instance();
    HermesXDirectHomeFrameDecision decision;
    decision.active = true;
    decision.entering = !visible_;
    decision.basePainted = basePainted_;
    decision.dogFrame = runtime.dogFrame;

    if (decision.entering) {
        model.invalidate();
        model.resetDog();
        basePainted_ = false;
        dogPose_ = static_cast<uint8_t>(dogEntryCount_ % 2U);
        ++dogEntryCount_;
        decision.basePainted = false;
        decision.forceDogRedraw = true;
    }

    decision.dogPose = dogPose_;
    decision.base = model.evaluateBase(
        state, decision.entering, basePainted_, nowMs, BaseRefreshIntervalMs);
    decision.canSkipUi = basePainted_ && !decision.base.baseDirty && !runtime.uiSkipBlocked;
    decision.baseRedrawRequested = !decision.canSkipUi;
    if (decision.baseRedrawRequested) {
        model.commitBase(state, nowMs);
        decision.forceDogRedraw = true;
    }
    visible_ = true;
    return decision;
}

HermesXDirectHomeFrameDecision HermesXHomeUiController::deactivate()
{
    auto &model = HermesXHomeUiModel::instance();
    HermesXDirectHomeFrameDecision decision;
    decision.left = visible_;
    if (decision.left) {
        decision.previousDog = model.dogCache();
    }
    model.reset();
    model.resetDog();
    visible_ = false;
    basePainted_ = false;
    return decision;
}

void HermesXHomeUiController::invalidateForPaletteReset()
{
    auto &model = HermesXHomeUiModel::instance();
    model.invalidate();
    model.invalidateDog();
    basePainted_ = false;
}

void HermesXHomeUiController::markUiFrameRendered(bool active, bool uiRendered)
{
    if (active && uiRendered) {
        basePainted_ = true;
    } else if (!active) {
        basePainted_ = false;
    }
}

bool HermesXHomeUiController::shouldRenderDog(uint16_t frame, bool forceRedraw) const
{
    return HermesXHomeUiModel::instance().shouldRenderDog(frame, dogPose_, forceRedraw);
}

void HermesXHomeUiController::commitDogFrame(uint16_t frame)
{
    HermesXHomeUiModel::instance().commitDogFrame(frame, dogPose_);
}

} // namespace graphics
