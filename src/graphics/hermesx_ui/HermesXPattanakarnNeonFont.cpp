#include "HermesXPattanakarnNeonFont.h"

#include "DebugConfiguration.h"
#include "graphics/fonts/PattanakarnClock32.h"

#include <algorithm>

namespace graphics
{
namespace
{

static_assert(HermesXNeonClockGlyphCacheCount == PattanakarnClock32::kGlyphCount,
              "Neon workspace glyph cache count must match the Pattanakarn font");
static_assert(HermesXHomeUiRenderer::NeonClockGlyphHeight == PattanakarnClock32::kGlyphHeight,
              "Neon workspace glyph height must match the Pattanakarn font");

const PattanakarnClock32::GlyphInfo *findGlyph(char ch)
{
    for (size_t index = 0; index < PattanakarnClock32::kGlyphCount; ++index) {
        if (PattanakarnClock32::kGlyphs[index].ch == ch) {
            return &PattanakarnClock32::kGlyphs[index];
        }
    }
    return nullptr;
}

bool isPixelOn(const PattanakarnClock32::GlyphInfo *glyph, uint8_t row, uint8_t column)
{
    if (!glyph || row >= PattanakarnClock32::kGlyphHeight || column >= glyph->width) {
        return false;
    }
    const uint8_t byteIndex = static_cast<uint8_t>(column / 8);
    const uint8_t bitIndex = static_cast<uint8_t>(column % 8);
    const uint8_t rowBits = pgm_read_byte(glyph->bitmap + row * glyph->bytesPerRow + byteIndex);
    return (rowBits & (0x80 >> bitIndex)) != 0;
}

bool isHalfPixelOn(const PattanakarnClock32::GlyphInfo *glyph, int16_t x, int16_t y)
{
    const uint8_t sourceX = static_cast<uint8_t>(std::min<int16_t>(glyph->width - 1, x * 2));
    const uint8_t sourceY =
        static_cast<uint8_t>(std::min<int16_t>(PattanakarnClock32::kGlyphHeight - 1, y * 2));
    if (isPixelOn(glyph, sourceY, sourceX)) {
        return true;
    }
    if ((sourceX + 1) < glyph->width && isPixelOn(glyph, sourceY, static_cast<uint8_t>(sourceX + 1))) {
        return true;
    }
    if ((sourceY + 1) < PattanakarnClock32::kGlyphHeight &&
        isPixelOn(glyph, static_cast<uint8_t>(sourceY + 1), sourceX)) {
        return true;
    }
    return (sourceX + 1) < glyph->width && (sourceY + 1) < PattanakarnClock32::kGlyphHeight &&
           isPixelOn(glyph, static_cast<uint8_t>(sourceY + 1), static_cast<uint8_t>(sourceX + 1));
}

void drawGlyph(OLEDDisplay *display,
               int16_t x,
               int16_t y,
               const PattanakarnClock32::GlyphInfo *glyph,
               bool halfScale,
               bool excludeRect,
               int16_t excludeX,
               int16_t excludeY,
               int16_t excludeX2,
               int16_t excludeY2)
{
    const int16_t width = halfScale ? static_cast<int16_t>((glyph->width + 1) / 2) : glyph->width;
    const int16_t height = halfScale ? HermesXPattanakarnNeonFont::HalfHeight
                                     : HermesXPattanakarnNeonFont::GlyphHeight;
    for (int16_t row = 0; row < height; ++row) {
        for (int16_t column = 0; column < width; ++column) {
            const bool on = halfScale ? isHalfPixelOn(glyph, column, row)
                                      : isPixelOn(glyph, static_cast<uint8_t>(row), static_cast<uint8_t>(column));
            if (!on) {
                continue;
            }
            const int16_t pixelX = x + column;
            const int16_t pixelY = y + row;
            if (excludeRect && pixelX >= excludeX && pixelY >= excludeY && pixelX < excludeX2 && pixelY < excludeY2) {
                continue;
            }
            display->setPixel(pixelX, pixelY);
        }
    }
}

void draw(OLEDDisplay *display,
          int16_t x,
          int16_t y,
          const char *text,
          bool halfScale,
          bool excludeRect,
          int16_t excludeX,
          int16_t excludeY,
          int16_t excludeWidth,
          int16_t excludeHeight)
{
    if (!display || !text) {
        return;
    }
    const int16_t excludeX2 = excludeX + excludeWidth;
    const int16_t excludeY2 = excludeY + excludeHeight;
    int16_t cursorX = x;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        const auto *glyph = findGlyph(*cursor);
        if (!glyph) {
            continue;
        }
        drawGlyph(display, cursorX, y, glyph, halfScale, excludeRect, excludeX, excludeY, excludeX2, excludeY2);
        cursorX += halfScale ? static_cast<int16_t>((glyph->width + 1) / 2) : glyph->width;
    }
}

} // namespace

bool HermesXPattanakarnNeonFont::supports(char ch)
{
    return findGlyph(ch) != nullptr;
}

uint8_t HermesXPattanakarnNeonFont::glyphWidth(char ch)
{
    const auto *glyph = findGlyph(ch);
    return glyph ? glyph->width : 0;
}

int16_t HermesXPattanakarnNeonFont::measureText(const char *text)
{
    if (!text) {
        return 0;
    }
    int16_t width = 0;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        const auto *glyph = findGlyph(*cursor);
        if (!glyph) {
            return 0;
        }
        width += glyph->width;
    }
    return width;
}

int16_t HermesXPattanakarnNeonFont::measureText(const char *text, bool halfScale, int16_t tracking)
{
    return HermesXNeonWorkspace::measureTextWidth(text, halfScale, tracking, glyphWidth);
}

void HermesXPattanakarnNeonFont::drawText(OLEDDisplay *display, int16_t x, int16_t y, const char *text)
{
    draw(display, x, y, text, false, false, 0, 0, 0, 0);
}

void HermesXPattanakarnNeonFont::drawHalfText(OLEDDisplay *display, int16_t x, int16_t y, const char *text)
{
    draw(display, x, y, text, true, false, 0, 0, 0, 0);
}

void HermesXPattanakarnNeonFont::drawTextOutsideRect(OLEDDisplay *display,
                                                     int16_t x,
                                                     int16_t y,
                                                     const char *text,
                                                     int16_t excludeX,
                                                     int16_t excludeY,
                                                     int16_t excludeWidth,
                                                     int16_t excludeHeight)
{
    draw(display, x, y, text, false, true, excludeX, excludeY, excludeWidth, excludeHeight);
}

HermesXNeonClockGlyphCache *HermesXPattanakarnNeonFont::glyphCache(char ch)
{
    const auto *glyph = findGlyph(ch);
    if (!glyph) {
        return nullptr;
    }
    const size_t glyphIndex = static_cast<size_t>(glyph - &PattanakarnClock32::kGlyphs[0]);
    HermesXNeonClockGlyphSource source;
    source.ch = ch;
    source.width = glyph->width;
    source.height = PattanakarnClock32::kGlyphHeight;
    source.bytesPerRow = glyph->bytesPerRow;
    source.bitmap = glyph->bitmap;
    bool built = false;
    auto *cache = HermesXNeonWorkspace::instance().getOrBuildClockGlyph(glyphIndex, source, built);
    if (cache && built) {
        LOG_DEBUG("[DirectHome] build glyph cache ch='%c' index=%u coreW=%u tile=%ux%u bytes=%u", ch,
                  static_cast<unsigned>(glyphIndex), static_cast<unsigned>(cache->coreW),
                  static_cast<unsigned>(cache->tileW), static_cast<unsigned>(cache->tileH),
                  static_cast<unsigned>(sizeof(cache->layerMap)));
    }
    return cache;
}

} // namespace graphics
