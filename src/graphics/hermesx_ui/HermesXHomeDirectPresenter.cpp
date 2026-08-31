#include "HermesXHomeDirectPresenter.h"

#include "HermesXHomeDirectRenderer.h"
#include "HermesXHomeUiController.h"
#include "HermesXHomeUiRenderer.h"
#include "HermesXPattanakarnNeonFont.h"
#include "graphics/TFTDisplay.h"

namespace graphics
{
namespace
{

void initializeUiIfAvailable(HermesXHomeUiInitializeCallback initializeUi, void *uiContext)
{
    if (initializeUi) {
        initializeUi(uiContext);
    }
}

void clearOverlayRegion(TFTDisplay *display, int16_t displayWidth)
{
    if (!display || displayWidth <= 0) {
        return;
    }
    HermesXHomeNeonClockLayout layout;
    if (!HermesXHomeUiRenderer::makeNeonClockLayout(displayWidth, 0, layout)) {
        return;
    }
    display->fillRect565(layout.regionX,
                         layout.regionY,
                         layout.regionWidth,
                         layout.regionHeight,
                         TFTDisplay::rgb565(0x00, 0x00, 0x00));
}

} // namespace

bool HermesXHomeDirectPresenter::supportsLayout(int16_t displayWidth, int16_t displayHeight)
{
    const bool compactLayout = displayWidth > 0 && displayHeight > 0 &&
                               (displayWidth < 200 || displayHeight < 120);
    const int16_t contentWidth = displayWidth - 8;
    const int16_t clockWidth = HermesXPattanakarnNeonFont::measureText("88:88:88");
    return compactLayout && contentWidth > 0 && clockWidth > 0 && clockWidth <= contentWidth;
}

void HermesXHomeDirectPresenter::enter(TFTDisplay *display,
                                       HermesXHomeUiInitializeCallback initializeUi,
                                       void *uiContext)
{
    if (!display) {
        return;
    }
    HermesXHomeUiRenderer::startQuote();
    initializeUiIfAvailable(initializeUi, uiContext);
    display->markColorPaletteDirty();
}

void HermesXHomeDirectPresenter::leave(TFTDisplay *display,
                                       int16_t displayWidth,
                                       const HermesXHomeDogCache &previousDog,
                                       uint16_t foreground,
                                       uint16_t background,
                                       HermesXHomeUiInitializeCallback initializeUi,
                                       void *uiContext)
{
    if (!display) {
        return;
    }
    if (previousDog.width > 0 && previousDog.height > 0) {
        display->fillRect565(
            previousDog.x, previousDog.y, previousDog.width, previousDog.height, background);
        display->overlayBufferForegroundRect565(
            previousDog.x, previousDog.y, previousDog.width, previousDog.height);
    }
    clearOverlayRegion(display, displayWidth);
    display->resetColorPalette(false);
    display->setColorPaletteDefaults(foreground, background);
    initializeUiIfAvailable(initializeUi, uiContext);
    display->markColorPaletteDirty();
}

bool HermesXHomeDirectPresenter::renderDog(TFTDisplay *display,
                                           int16_t displayWidth,
                                           int16_t displayHeight,
                                           HermesXHomeUiController &controller,
                                           uint16_t dogFrame,
                                           bool forceRedraw)
{
    if (!display || !controller.shouldRenderDog(dogFrame, forceRedraw)) {
        return false;
    }
    renderDirectHomeDog(display, displayWidth, displayHeight, dogFrame, controller.dogPose());
    controller.commitDogFrame(dogFrame);
    return true;
}

} // namespace graphics
