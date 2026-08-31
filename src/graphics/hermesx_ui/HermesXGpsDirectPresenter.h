#pragma once

#include "HermesXGpsUiModel.h"

#include <cstdint>

class TFTDisplay;

namespace graphics
{

using HermesXGpsUiInitializeCallback = void (*)(void *context);

class HermesXGpsDirectPresenter
{
  public:
    static HermesXGpsPosterVisibilityTransition updateVisibility(
        TFTDisplay *display,
        int16_t displayWidth,
        int16_t displayHeight,
        HermesXGpsUiModel &model,
        bool visible,
        uint16_t foreground,
        uint16_t background,
        HermesXGpsUiInitializeCallback initializeUi,
        void *uiContext);
    static bool shouldSkipUi(const HermesXGpsUiModel &model,
                             const HermesXGpsPosterState &state,
                             const HermesXGpsPosterLayers &layers,
                             bool forceDirty,
                             bool uiSkipBlocked);
};

} // namespace graphics
