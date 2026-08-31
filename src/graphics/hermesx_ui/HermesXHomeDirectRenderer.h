#pragma once

#include <cstdint>

class TFTDisplay;

namespace graphics
{

void renderDirectHomeDog(TFTDisplay *display,
                         int16_t displayWidth,
                         int16_t displayHeight,
                         uint16_t dogFrame,
                         uint8_t dogPose);

} // namespace graphics
