#pragma once

#include "HermesXRotaryLockUiModel.h"
#include <OLEDDisplay.h>

namespace graphics
{

class HermesXRotaryLockUiRenderer
{
  public:
    static void draw(OLEDDisplay *display, const HermesXRotaryLockUiState &state);
};

} // namespace graphics
