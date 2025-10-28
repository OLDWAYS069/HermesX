#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

class TFTDisplay;
class OLEDDisplay;

namespace graphics::HermesX_zh
{

constexpr std::size_t GLYPH_COUNT = 5129;
constexpr std::uint8_t GLYPH_WIDTH = 12;
constexpr std::uint8_t GLYPH_HEIGHT = 12;
constexpr std::uint8_t GLYPH_STRIDE_BITS = 12;
constexpr std::uint8_t GLYPH_BYTES = 18;

extern const std::uint8_t HermesX_CN12_CODEPOINTS[] PROGMEM;
extern const std::uint8_t HermesX_CN12_GLYPHS[] PROGMEM;

/// Decode the next UTF-8 codepoint, advancing the cursor.
/// Returns U+FFFD on invalid sequences.
std::uint32_t nextCodepoint(const char *&cursor, const char *end);

/// Perform a binary search for the codepoint. Returns -1 if not found.
int locateCodepoint(std::uint32_t codepoint);

/// Obtain pointer to glyph bytes for the given index.
const std::uint8_t *glyphData(int index);

/// Draw a 12x12 glyph using a TFT display fast path.
/// Returns false if no fast path was used.
bool drawHanzi12(TFTDisplay &display, int16_t x, int16_t y, int glyphIndex, std::uint16_t fgColor,
                 std::uint16_t bgColor);

/// Draw fallback pixels via the generic OLEDDisplay interface.
void drawHanzi12Slow(OLEDDisplay &display, int16_t x, int16_t y, int glyphIndex);

/// Index of the fallback glyph ("□").
int fallbackIndex();

void incrementMissingGlyph();
std::uint32_t drainMissingGlyphs();

void setActiveTFT(TFTDisplay *display);
TFTDisplay *getActiveTFT();

void drawMixed(OLEDDisplay &display, int16_t x, int16_t y, const char *text, int advanceX = GLYPH_WIDTH,
               int lineHeight = 13, TFTDisplay *fastDisplay = nullptr);

void drawMixedBounded(OLEDDisplay &display, int16_t x, int16_t y, int16_t maxWidth, const char *text,
                      int advanceX = GLYPH_WIDTH, int lineHeight = 13, TFTDisplay *fastDisplay = nullptr);

int stringAdvance(const char *text, int advanceX = GLYPH_WIDTH);

} // namespace graphics::HermesX_zh
