#pragma once

#include <Arduino.h>
#include <OLEDDisplay.h>
#include <cstdint>

namespace graphics
{

using HermesXNodeBrowserRowProvider = bool (*)(void *context, uint8_t index, String &label, String &status);

struct HermesXNodeBrowserRowSource {
    HermesXNodeBrowserRowSource(uint8_t countValue = 0,
                                void *contextValue = nullptr,
                                HermesXNodeBrowserRowProvider provider = nullptr,
                                bool mixedStatusValue = false)
        : count(countValue), context(contextValue), rowAt(provider), mixedStatus(mixedStatusValue)
    {
    }

    uint8_t count;
    void *context;
    HermesXNodeBrowserRowProvider rowAt;
    bool mixedStatus;
};

class HermesXNodeBrowserUiRenderer
{
  public:
    static void drawMenu(OLEDDisplay *display,
                         int16_t x,
                         int16_t y,
                         const char *title,
                         const char *const *items,
                         uint8_t itemCount,
                         uint8_t cursor,
                         bool clearBackground,
                         uint8_t rowPadding,
                         uint8_t titleGap);
    static void drawList(OLEDDisplay *display,
                         int16_t x,
                         int16_t y,
                         const char *title,
                         const char *backLabel,
                         const char *emptyLabel,
                         uint8_t cursor,
                         const HermesXNodeBrowserRowSource &rows,
                         uint8_t maxVisibleRows,
                         bool clearBackground);
    static void drawDetail(OLEDDisplay *display,
                           int16_t x,
                           int16_t y,
                           const char *title,
                           const char *noDataLabel,
                           const String *rows,
                           uint8_t rowCount,
                           uint8_t cursor,
                           bool clearBackground);
    static void drawFinderRadarIcon(OLEDDisplay *display,
                                    int16_t centerX,
                                    int16_t centerY,
                                    int16_t radius,
                                    bool compact,
                                    int16_t sweepAngleDeg = INT16_MIN);
    static void drawFinderPulseConfirm(OLEDDisplay *display, uint32_t nowMs, uint32_t armDelayMs);
    static void drawFinderPulseSending(OLEDDisplay *display, uint32_t nowMs);
};

} // namespace graphics
