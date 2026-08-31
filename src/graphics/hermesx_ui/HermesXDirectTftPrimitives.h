#pragma once

#include <cstdint>

class TFTDisplay;

namespace graphics
{

struct DirectDrawClipRect
{
    bool enabled = false;
    int16_t minX = 0;
    int16_t maxX = -1;
    int16_t minY = 0;
    int16_t maxY = -1;
};

bool makeDirectDrawClipRect(int16_t x, int16_t y, int16_t width, int16_t height,
                            int16_t displayWidth, int16_t displayHeight, DirectDrawClipRect &outClip);
void directFillRect565Clipped(TFTDisplay *display, int16_t displayWidth, int16_t displayHeight,
                              int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color,
                              const DirectDrawClipRect *clip);
void directFillCircle565(TFTDisplay *display, int16_t displayWidth, int16_t displayHeight,
                         int16_t centerX, int16_t centerY, int16_t radius, uint16_t color);
void directFillCircle565Clipped(TFTDisplay *display, int16_t displayWidth, int16_t displayHeight,
                                int16_t centerX, int16_t centerY, int16_t radius, uint16_t color,
                                const DirectDrawClipRect *clip);
void directDrawLine565(TFTDisplay *display, int16_t displayWidth, int16_t displayHeight,
                       int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void directDrawNeonLine565(TFTDisplay *display, int16_t displayWidth, int16_t displayHeight,
                           int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                           uint16_t coreColor, uint16_t glowNearColor, uint16_t glowFarColor,
                           int16_t coreThickness, int16_t glowNearThickness, int16_t glowFarThickness);
void directDrawNeonLine565Clipped(TFTDisplay *display, int16_t displayWidth, int16_t displayHeight,
                                  int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                                  uint16_t coreColor, uint16_t glowNearColor, uint16_t glowFarColor,
                                  int16_t coreThickness, int16_t glowNearThickness, int16_t glowFarThickness,
                                  const DirectDrawClipRect *clip);

} // namespace graphics
