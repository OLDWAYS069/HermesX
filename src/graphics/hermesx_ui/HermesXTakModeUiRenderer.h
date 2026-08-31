#pragma once

#include "HermesXTakModeUiModel.h"
#include <Arduino.h>
#include <OLEDDisplay.h>
#include <cstdint>

namespace graphics
{

using HermesXTakModeRowProvider = bool (*)(void *context, uint8_t index, String &label);

struct HermesXTakModeRowSource {
    HermesXTakModeRowSource(uint8_t countValue = 0,
                            void *contextValue = nullptr,
                            HermesXTakModeRowProvider providerValue = nullptr)
        : count(countValue), context(contextValue), rowAt(providerValue)
    {
    }

    uint8_t count;
    void *context;
    HermesXTakModeRowProvider rowAt;
};

struct HermesXTakModeRenderView {
    const char *title = "TAK MODE";
    bool active = false;
    bool allowEmUi = true;
    bool allowFinder = true;
    const String *slotLine = nullptr;
    const String *utilizationLine = nullptr;
    const String *suggestionLine = nullptr;
    HermesXTakModeRowSource settingsRows;
    HermesXTakModeRowSource channelRows;
};

class HermesXTakModeUiRenderer
{
  public:
    static void draw(OLEDDisplay *display,
                     int16_t x,
                     int16_t y,
                     bool overlayOnly,
                     const HermesXTakModeUiState &state,
                     const HermesXTakModeRenderView &view);
    static void drawTransition(OLEDDisplay *display, uint32_t startedAtMs, bool entering);
};

} // namespace graphics
