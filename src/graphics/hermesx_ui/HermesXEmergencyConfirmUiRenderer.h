#pragma once

#include "HermesXEmergencyConfirmUiModel.h"
#include <OLEDDisplay.h>

namespace graphics
{

class HermesXEmergencyConfirmUiRenderer
{
  public:
    static void draw(OLEDDisplay *display, const HermesXEmergencyConfirmUiState &state);
};

} // namespace graphics
