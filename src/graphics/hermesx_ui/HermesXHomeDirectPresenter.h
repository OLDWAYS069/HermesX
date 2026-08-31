#pragma once

#include "HermesXHomeUiModel.h"

#include <cstdint>

class TFTDisplay;

namespace graphics
{

using HermesXHomeUiInitializeCallback = void (*)(void *context);

class HermesXHomeUiController;

class HermesXHomeDirectPresenter
{
  public:
    static bool supportsLayout(int16_t displayWidth, int16_t displayHeight);
    static void enter(TFTDisplay *display,
                      HermesXHomeUiInitializeCallback initializeUi,
                      void *uiContext);
    static void leave(TFTDisplay *display,
                      int16_t displayWidth,
                      const HermesXHomeDogCache &previousDog,
                      uint16_t foreground,
                      uint16_t background,
                      HermesXHomeUiInitializeCallback initializeUi,
                      void *uiContext);
    static bool renderDog(TFTDisplay *display,
                          int16_t displayWidth,
                          int16_t displayHeight,
                          HermesXHomeUiController &controller,
                          uint16_t dogFrame,
                          bool forceRedraw);
};

} // namespace graphics
