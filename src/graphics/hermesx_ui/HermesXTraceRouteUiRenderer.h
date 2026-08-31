#pragma once

#include <Arduino.h>
#include <OLEDDisplay.h>
#include <cstdint>

namespace graphics
{

using HermesXTraceRouteRowProvider = bool (*)(void *context, uint8_t index, String &label, String &status);

struct HermesXTraceRouteRowSource {
    HermesXTraceRouteRowSource(uint8_t countValue = 0,
                               void *contextValue = nullptr,
                               HermesXTraceRouteRowProvider rowProvider = nullptr)
        : count(countValue), context(contextValue), rowAt(rowProvider)
    {
    }

    uint8_t count;
    void *context;
    HermesXTraceRouteRowProvider rowAt;
};

class HermesXTraceRouteUiRenderer
{
  public:
    static void drawPopup(OLEDDisplay *display);
    static void drawSearchResult(OLEDDisplay *display,
                                 int16_t x,
                                 int16_t y,
                                 bool nodeAvailable,
                                 const String &shortName,
                                 const String &longName);
    static void drawMenu(OLEDDisplay *display, int16_t x, int16_t y);
    static void drawBindList(OLEDDisplay *display,
                             int16_t x,
                             int16_t y,
                             const HermesXTraceRouteRowSource &rows);
    static void drawBoundList(OLEDDisplay *display,
                              int16_t x,
                              int16_t y,
                              const HermesXTraceRouteRowSource &rows);
};

} // namespace graphics
