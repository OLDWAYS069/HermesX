#pragma once

#include <cstdint>

namespace graphics
{

class HermesXNeonMask
{
  public:
    static bool hasPixelInRadius(
        const uint8_t *mask, int16_t maskWidth, int16_t maskHeight, int16_t x, int16_t y, int16_t radius);
    static bool isInteriorPixel(
        const uint8_t *mask, int16_t maskWidth, int16_t maskHeight, int16_t x, int16_t y);
};

} // namespace graphics
