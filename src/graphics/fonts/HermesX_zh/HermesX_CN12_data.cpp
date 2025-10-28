#include <Arduino.h>

#include "HermesX_CN12.h"

namespace graphics::HermesX_zh
{

const std::uint8_t HermesX_CN12_CODEPOINTS[] PROGMEM = {
#include "HermesX_CN12.codepoints.inc"
};

const std::uint8_t HermesX_CN12_GLYPHS[] PROGMEM = {
#include "HermesX_CN12.glyphs.inc"
};

} // namespace graphics::HermesX_zh
