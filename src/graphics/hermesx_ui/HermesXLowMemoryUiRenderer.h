#pragma once

#include "HermesXLowMemoryUiModel.h"
#include <OLEDDisplay.h>

namespace graphics
{

class HermesXLowMemoryUiRenderer
{
  public:
    static void draw(OLEDDisplay *display, const HermesXLowMemoryUiState &state, bool protectionActive);
};

} // namespace graphics
