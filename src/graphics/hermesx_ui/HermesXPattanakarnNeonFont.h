#pragma once

#include "HermesXNeonWorkspace.h"

#include <OLEDDisplay.h>
#include <cstddef>
#include <cstdint>

namespace graphics
{

class HermesXPattanakarnNeonFont
{
  public:
    static constexpr uint8_t GlyphHeight = 25;
    static constexpr int16_t HalfHeight = (GlyphHeight + 1) / 2;
    static constexpr size_t GlyphCount = 13;

    static bool supports(char ch);
    static uint8_t glyphWidth(char ch);
    static int16_t measureText(const char *text);
    static int16_t measureText(const char *text, bool halfScale, int16_t tracking);
    static void drawText(OLEDDisplay *display, int16_t x, int16_t y, const char *text);
    static void drawHalfText(OLEDDisplay *display, int16_t x, int16_t y, const char *text);
    static void drawTextOutsideRect(OLEDDisplay *display,
                                    int16_t x,
                                    int16_t y,
                                    const char *text,
                                    int16_t excludeX,
                                    int16_t excludeY,
                                    int16_t excludeWidth,
                                    int16_t excludeHeight);
    static HermesXNeonClockGlyphCache *glyphCache(char ch);
};

} // namespace graphics
