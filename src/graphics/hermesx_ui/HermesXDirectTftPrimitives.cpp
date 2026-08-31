#include "HermesXDirectTftPrimitives.h"

#include "graphics/TFTDisplay.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace graphics
{
namespace
{

bool directClipContains(const DirectDrawClipRect *clip, int16_t x, int16_t y)
{
    return !clip || !clip->enabled ||
           (x >= clip->minX && x <= clip->maxX && y >= clip->minY && y <= clip->maxY);
}

void directDrawLine565Clipped(TFTDisplay *display, int16_t displayWidth, int16_t displayHeight,
                              int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color,
                              const DirectDrawClipRect *clip)
{
    if (!display) {
        return;
    }
    int16_t x = x0;
    int16_t y = y0;
    const int16_t dx = std::abs(x1 - x0);
    const int16_t sx = x0 < x1 ? 1 : -1;
    const int16_t dy = -std::abs(y1 - y0);
    const int16_t sy = y0 < y1 ? 1 : -1;
    int16_t error = dx + dy;
    while (true) {
        if (x >= 0 && x < displayWidth && y >= 0 && y < displayHeight && directClipContains(clip, x, y)) {
            display->drawPixel565(x, y, color);
        }
        if (x == x1 && y == y1) {
            break;
        }
        const int16_t doubledError = static_cast<int16_t>(2 * error);
        if (doubledError >= dy) {
            error += dy;
            x += sx;
        }
        if (doubledError <= dx) {
            error += dx;
            y += sy;
        }
    }
}

void directDrawThickLine565Clipped(TFTDisplay *display, int16_t displayWidth, int16_t displayHeight,
                                   int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                                   int16_t thickness, uint16_t color, const DirectDrawClipRect *clip)
{
    if (!display || thickness <= 0) {
        return;
    }
    if (thickness <= 1) {
        directDrawLine565Clipped(display, displayWidth, displayHeight, x0, y0, x1, y1, color, clip);
        return;
    }
    const int16_t dx = x1 - x0;
    const int16_t dy = y1 - y0;
    const int16_t steps = std::max<int16_t>(std::abs(dx), std::abs(dy));
    const int16_t dotRadius = std::max<int16_t>(1, thickness / 2);
    if (steps <= 0) {
        directFillCircle565Clipped(display, displayWidth, displayHeight, x0, y0, dotRadius, color, clip);
        return;
    }
    for (int16_t i = 0; i <= steps; ++i) {
        directFillCircle565Clipped(display, displayWidth, displayHeight,
                                   x0 + (dx * i) / steps, y0 + (dy * i) / steps,
                                   dotRadius, color, clip);
    }
}

} // namespace

bool makeDirectDrawClipRect(int16_t x, int16_t y, int16_t width, int16_t height,
                            int16_t displayWidth, int16_t displayHeight, DirectDrawClipRect &outClip)
{
    if (width <= 0 || height <= 0 || displayWidth <= 0 || displayHeight <= 0) {
        outClip.enabled = false;
        return false;
    }
    int16_t minX = x;
    int16_t minY = y;
    int16_t maxX = x + width - 1;
    int16_t maxY = y + height - 1;
    if (maxX < 0 || maxY < 0 || minX >= displayWidth || minY >= displayHeight) {
        outClip.enabled = false;
        return false;
    }
    outClip.enabled = true;
    outClip.minX = std::max<int16_t>(0, minX);
    outClip.minY = std::max<int16_t>(0, minY);
    outClip.maxX = std::min<int16_t>(displayWidth - 1, maxX);
    outClip.maxY = std::min<int16_t>(displayHeight - 1, maxY);
    return true;
}

void directFillRect565Clipped(TFTDisplay *display, int16_t displayWidth, int16_t displayHeight,
                              int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color,
                              const DirectDrawClipRect *clip)
{
    if (!display || displayWidth <= 0 || displayHeight <= 0 || width <= 0 || height <= 0) {
        return;
    }
    int16_t x0 = x;
    int16_t y0 = y;
    int16_t x1 = x + width - 1;
    int16_t y1 = y + height - 1;
    if (clip && clip->enabled) {
        x0 = std::max<int16_t>(x0, clip->minX);
        y0 = std::max<int16_t>(y0, clip->minY);
        x1 = std::min<int16_t>(x1, clip->maxX);
        y1 = std::min<int16_t>(y1, clip->maxY);
    }
    x0 = std::max<int16_t>(0, x0);
    y0 = std::max<int16_t>(0, y0);
    x1 = std::min<int16_t>(displayWidth - 1, x1);
    y1 = std::min<int16_t>(displayHeight - 1, y1);
    if (x0 <= x1 && y0 <= y1) {
        display->fillRect565(x0, y0, x1 - x0 + 1, y1 - y0 + 1, color);
    }
}

void directFillCircle565(TFTDisplay *display, int16_t displayWidth, int16_t displayHeight,
                         int16_t centerX, int16_t centerY, int16_t radius, uint16_t color)
{
    directFillCircle565Clipped(display, displayWidth, displayHeight, centerX, centerY, radius, color, nullptr);
}

void directFillCircle565Clipped(TFTDisplay *display, int16_t displayWidth, int16_t displayHeight,
                                int16_t centerX, int16_t centerY, int16_t radius, uint16_t color,
                                const DirectDrawClipRect *clip)
{
    if (!display || radius <= 0 || displayWidth <= 0 || displayHeight <= 0) {
        return;
    }
    const int32_t radiusSquared = static_cast<int32_t>(radius) * radius;
    for (int16_t yOffset = -radius; yOffset <= radius; ++yOffset) {
        const int16_t drawY = centerY + yOffset;
        if (drawY < 0 || drawY >= displayHeight ||
            (clip && clip->enabled && (drawY < clip->minY || drawY > clip->maxY))) {
            continue;
        }
        const int32_t xSquared = radiusSquared - static_cast<int32_t>(yOffset) * yOffset;
        if (xSquared < 0) {
            continue;
        }
        const int16_t xOffset = static_cast<int16_t>(sqrtf(static_cast<float>(xSquared)));
        int16_t x0 = centerX - xOffset;
        int16_t x1 = centerX + xOffset;
        if (x1 < 0 || x0 >= displayWidth) {
            continue;
        }
        x0 = std::max<int16_t>(0, x0);
        x1 = std::min<int16_t>(displayWidth - 1, x1);
        if (clip && clip->enabled) {
            if (x1 < clip->minX || x0 > clip->maxX) {
                continue;
            }
            x0 = std::max<int16_t>(x0, clip->minX);
            x1 = std::min<int16_t>(x1, clip->maxX);
        }
        if (x0 <= x1) {
            display->fillRect565(x0, drawY, x1 - x0 + 1, 1, color);
        }
    }
}

void directDrawLine565(TFTDisplay *display, int16_t displayWidth, int16_t displayHeight,
                       int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    directDrawLine565Clipped(display, displayWidth, displayHeight, x0, y0, x1, y1, color, nullptr);
}

void directDrawNeonLine565(TFTDisplay *display, int16_t displayWidth, int16_t displayHeight,
                           int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                           uint16_t coreColor, uint16_t glowNearColor, uint16_t glowFarColor,
                           int16_t coreThickness, int16_t glowNearThickness, int16_t glowFarThickness)
{
    directDrawNeonLine565Clipped(display, displayWidth, displayHeight, x0, y0, x1, y1,
                                 coreColor, glowNearColor, glowFarColor,
                                 coreThickness, glowNearThickness, glowFarThickness, nullptr);
}

void directDrawNeonLine565Clipped(TFTDisplay *display, int16_t displayWidth, int16_t displayHeight,
                                  int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                                  uint16_t coreColor, uint16_t glowNearColor, uint16_t glowFarColor,
                                  int16_t coreThickness, int16_t glowNearThickness, int16_t glowFarThickness,
                                  const DirectDrawClipRect *clip)
{
    if (!display) {
        return;
    }
    if (glowFarThickness > 0) {
        directDrawThickLine565Clipped(display, displayWidth, displayHeight, x0, y0, x1, y1,
                                      glowFarThickness, glowFarColor, clip);
    }
    if (glowNearThickness > 0) {
        directDrawThickLine565Clipped(display, displayWidth, displayHeight, x0, y0, x1, y1,
                                      glowNearThickness, glowNearColor, clip);
    }
    if (coreThickness > 0) {
        directDrawThickLine565Clipped(display, displayWidth, displayHeight, x0, y0, x1, y1,
                                      coreThickness, coreColor, clip);
    }
}

} // namespace graphics
