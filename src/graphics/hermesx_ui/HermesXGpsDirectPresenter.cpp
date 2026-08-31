#include "HermesXGpsDirectPresenter.h"

#include "graphics/TFTDisplay.h"

namespace graphics
{

HermesXGpsPosterVisibilityTransition HermesXGpsDirectPresenter::updateVisibility(
    TFTDisplay *display,
    int16_t displayWidth,
    int16_t displayHeight,
    HermesXGpsUiModel &model,
    bool visible,
    uint16_t foreground,
    uint16_t background,
    HermesXGpsUiInitializeCallback initializeUi,
    void *uiContext)
{
    const HermesXGpsPosterVisibilityTransition transition = model.visibilityTransition(visible);
    if (!display) {
        return transition;
    }
    if (transition.entered || transition.left) {
        display->resetColorPalette(false);
        display->setColorPaletteDefaults(foreground, background);
        display->markColorPaletteDirty();
    }
    if (transition.entered) {
        model.invalidatePoster();
        if (initializeUi) {
            initializeUi(uiContext);
        }
        if (displayWidth > 0 && displayHeight > 0) {
            display->fillRect565(0, 0, displayWidth, displayHeight, background);
        }
        display->markColorPaletteDirty();
    } else if (transition.left) {
        model.invalidatePoster();
    }
    return transition;
}

bool HermesXGpsDirectPresenter::shouldSkipUi(const HermesXGpsUiModel &model,
                                             const HermesXGpsPosterState &state,
                                             const HermesXGpsPosterLayers &layers,
                                             bool forceDirty,
                                             bool uiSkipBlocked)
{
    const bool posterDirty = model.isPosterDirty(state, layers, forceDirty);
    return model.isBasePainted() && model.isValid() && !posterDirty && !uiSkipBlocked &&
           !model.needsFullFrameAfterSwitch();
}

} // namespace graphics
