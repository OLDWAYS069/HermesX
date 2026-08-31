#include "HermesXNeonMask.h"

#include <algorithm>

namespace graphics
{

bool HermesXNeonMask::hasPixelInRadius(
    const uint8_t *mask, int16_t maskWidth, int16_t maskHeight, int16_t x, int16_t y, int16_t radius)
{
    if (!mask || maskWidth <= 0 || maskHeight <= 0 || radius < 0) {
        return false;
    }

    const int16_t minY = std::max<int16_t>(0, y - radius);
    const int16_t maxY = std::min<int16_t>(maskHeight - 1, y + radius);
    const int16_t minX = std::max<int16_t>(0, x - radius);
    const int16_t maxX = std::min<int16_t>(maskWidth - 1, x + radius);
    const int32_t radiusSquared = static_cast<int32_t>(radius) * static_cast<int32_t>(radius);

    for (int16_t targetY = minY; targetY <= maxY; ++targetY) {
        const int32_t deltaY = static_cast<int32_t>(targetY - y);
        const int32_t deltaYSquared = deltaY * deltaY;
        const int16_t rowBase = targetY * maskWidth;
        for (int16_t targetX = minX; targetX <= maxX; ++targetX) {
            if (!mask[rowBase + targetX]) {
                continue;
            }

            const int32_t deltaX = static_cast<int32_t>(targetX - x);
            if ((deltaX * deltaX) + deltaYSquared <= radiusSquared) {
                return true;
            }
        }
    }
    return false;
}

bool HermesXNeonMask::isInteriorPixel(
    const uint8_t *mask, int16_t maskWidth, int16_t maskHeight, int16_t x, int16_t y)
{
    if (!mask || maskWidth <= 0 || maskHeight <= 0 || x <= 0 || y <= 0 || x >= (maskWidth - 1) ||
        y >= (maskHeight - 1)) {
        return false;
    }

    for (int16_t targetY = y - 1; targetY <= y + 1; ++targetY) {
        const int16_t rowBase = targetY * maskWidth;
        for (int16_t targetX = x - 1; targetX <= x + 1; ++targetX) {
            if (!mask[rowBase + targetX]) {
                return false;
            }
        }
    }
    return true;
}

} // namespace graphics
