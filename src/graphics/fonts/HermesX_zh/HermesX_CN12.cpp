#include "HermesX_CN12.h"

#include "configuration.h"

#include <OLEDDisplay.h>
#include <cstring>

#include "graphics/TFTDisplay.h"

namespace graphics::HermesX_zh
{
namespace
{
constexpr std::uint32_t kReplacement = 0x25A1; // White square
constexpr std::uint32_t kInvalid = 0xFFFD;

int cachedFallback = -2;
volatile std::uint32_t missingGlyphCounter = 0;
TFTDisplay *activeTft = nullptr;

// --- HermesX Remove TFT fast-path START
bool drawHanzi12(TFTDisplay &, int16_t, int16_t, int, std::uint16_t, std::uint16_t)
{
    return false;
}
// --- HermesX Remove TFT fast-path END

std::uint32_t loadCodepoint(int index)
{
    const std::uint8_t *ptr = HermesX_CN12_CODEPOINTS + (index * 4);
#if defined(ARDUINO_ARCH_AVR)
    std::uint32_t value = pgm_read_dword(ptr);
#else
    std::uint32_t value;
    std::memcpy(&value, ptr, sizeof(value));
#endif
    return value;
}

void unpackGlyphBits(int index, std::uint16_t (&rows)[GLYPH_HEIGHT])
{
    const std::uint8_t *glyph = HermesX_CN12_GLYPHS + (index * GLYPH_BYTES);
    int bitOffset = 0;
    for (std::uint8_t row = 0; row < GLYPH_HEIGHT; ++row) {
        std::uint16_t rowBits = 0;
        for (std::uint8_t col = 0; col < GLYPH_WIDTH; ++col) {
            const int bitIndex = bitOffset + col;
            const int byteIndex = bitIndex >> 3;
            const int bitInByte = 7 - (bitIndex & 7);
#if defined(ARDUINO_ARCH_AVR)
            const std::uint8_t byteValue = pgm_read_byte(glyph + byteIndex);
#else
            const std::uint8_t byteValue = glyph[byteIndex];
#endif
            const bool on = (byteValue >> bitInByte) & 0x1;
            rowBits = static_cast<std::uint16_t>((rowBits << 1) | static_cast<std::uint16_t>(on));
        }
        rows[row] = rowBits;
        bitOffset += GLYPH_STRIDE_BITS;
    }
}

} // namespace

std::uint32_t nextCodepoint(const char *&cursor, const char *end)
{
    if (!cursor || cursor >= end)
        return 0;

    const unsigned char first = static_cast<unsigned char>(*cursor++);
    if (first < 0x80u)
        return first;

    auto continuation = [&](unsigned char &out) -> bool {
        if (cursor >= end)
            return false;
        out = static_cast<unsigned char>(*cursor++);
        return (out & 0xC0u) == 0x80u;
    };

    unsigned char c1 = 0, c2 = 0, c3 = 0;
    if ((first >> 5u) == 0x6u) {
        if (!continuation(c1))
            return kInvalid;
        return static_cast<std::uint32_t>((first & 0x1Fu) << 6 | (c1 & 0x3Fu));
    }

    if ((first >> 4u) == 0xEu) {
        if (!continuation(c1) || !continuation(c2))
            return kInvalid;
        return static_cast<std::uint32_t>((first & 0x0Fu) << 12 | (c1 & 0x3Fu) << 6 | (c2 & 0x3Fu));
    }

    if ((first >> 3u) == 0x1Eu) {
        if (!continuation(c1) || !continuation(c2) || !continuation(c3))
            return kInvalid;
        return static_cast<std::uint32_t>((first & 0x07u) << 18 | (c1 & 0x3Fu) << 12 | (c2 & 0x3Fu) << 6 |
                                          (c3 & 0x3Fu));
    }

    return kInvalid;
}

int locateCodepoint(std::uint32_t codepoint)
{
    int left = 0;
    int right = static_cast<int>(GLYPH_COUNT) - 1;
    while (left <= right) {
        const int mid = left + ((right - left) >> 1);
        const std::uint32_t midValue = loadCodepoint(mid);
        if (codepoint < midValue) {
            right = mid - 1;
        } else if (codepoint > midValue) {
            left = mid + 1;
        } else {
            return mid;
        }
    }
    return -1;
}

const std::uint8_t *glyphData(int index)
{
    if (index < 0 || index >= static_cast<int>(GLYPH_COUNT))
        return nullptr;
    return HermesX_CN12_GLYPHS + (index * GLYPH_BYTES);
}

void drawHanzi12Slow(OLEDDisplay &display, int16_t x, int16_t y, int glyphIndex)
{
    std::uint16_t rows[GLYPH_HEIGHT];
    unpackGlyphBits(glyphIndex, rows);
    const OLEDDISPLAY_COLOR color = display.getColor();
    for (std::uint8_t row = 0; row < GLYPH_HEIGHT; ++row) {
        const std::uint16_t rowMask = rows[row];
        for (std::uint8_t col = 0; col < GLYPH_WIDTH; ++col) {
            const bool on = (rowMask >> (GLYPH_WIDTH - 1 - col)) & 0x1u;
            if (on)
                display.setPixelColor(x + col, y + row, color);
        }
    }
}

int fallbackIndex()
{
    if (cachedFallback == -2) {
        cachedFallback = locateCodepoint(kReplacement);
        if (cachedFallback < 0)
            cachedFallback = 0;
    }
    return cachedFallback;
}

void incrementMissingGlyph()
{
    ++missingGlyphCounter;
}

std::uint32_t drainMissingGlyphs()
{
    const std::uint32_t value = missingGlyphCounter;
    missingGlyphCounter = 0;
    return value;
}

void setActiveTFT(TFTDisplay *display)
{
    activeTft = display;
}

TFTDisplay *getActiveTFT()
{
    return activeTft;
}

// --- HermesX Remove TFT fast-path START
void drawMixed(OLEDDisplay &display, int16_t x, int16_t y, const char *text, int advanceX, int lineHeight,
               TFTDisplay *fastDisplay)
{
    (void)fastDisplay;
    if (!text)
        return;

    const char *cursor = text;
    const char *end = cursor + std::strlen(text);
    const int originX = x;

    // --- HermesX ASCII split START
    while (cursor < end) {
        std::uint32_t cp = nextCodepoint(cursor, end);
        if (cp == 0)
            break;
        if (cp == '\r')
            continue;
        if (cp == '\n') {
            x = originX;
            y += lineHeight;
            continue;
        }
        if (cp == kInvalid)
            cp = kReplacement;
        if (cp >= 0x20u && cp < 0x7Fu) {
            ::String asciiString(static_cast<char>(cp));
            const int glyphWidth = static_cast<int>(display.getStringWidth(asciiString));
            const int asciiAdvance = glyphWidth > 0 ? glyphWidth : advanceX;
            display.drawString(x, y, asciiString);
            x += asciiAdvance;
            continue;
        }

        int glyphIndex = locateCodepoint(cp);
        if (glyphIndex < 0) {
            incrementMissingGlyph();
            glyphIndex = fallbackIndex();
        }

        drawHanzi12Slow(display, x, y, glyphIndex);
        x += advanceX;
    }
    // --- HermesX ASCII split END
}

void drawMixedBounded(OLEDDisplay &display, int16_t x, int16_t y, int16_t maxWidth, const char *text, int advanceX,
                      int lineHeight, TFTDisplay *fastDisplay)
{
    (void)fastDisplay;
    if (!text)
        return;

    const int originX = x;
    const int limit = maxWidth > 0 ? maxWidth : display.width();
    const char *cursor = text;
    const char *end = cursor + std::strlen(text);

    // --- HermesX ASCII split START
    while (cursor < end) {
        std::uint32_t cp = nextCodepoint(cursor, end);
        if (cp == 0)
            break;
        if (cp == '\r')
            continue;
        if (cp == '\n') {
            x = originX;
            y += lineHeight;
            continue;
        }
        if (cp == kInvalid)
            cp = kReplacement;
        if (cp >= 0x20u && cp < 0x7Fu) {
            ::String asciiString(static_cast<char>(cp));
            int glyphWidth = static_cast<int>(display.getStringWidth(asciiString));
            if (glyphWidth <= 0)
                glyphWidth = advanceX;
            if (x + glyphWidth > originX + limit) {
                x = originX;
                y += lineHeight;
            }
            display.drawString(x, y, asciiString);
            x += glyphWidth;
            continue;
        }

        int glyphIndex = locateCodepoint(cp);
        if (glyphIndex < 0) {
            incrementMissingGlyph();
            glyphIndex = fallbackIndex();
        }

        if (x + advanceX > originX + limit) {
            x = originX;
            y += lineHeight;
        }

        drawHanzi12Slow(display, x, y, glyphIndex);
        x += advanceX;
    }
    // --- HermesX ASCII split END
}
// --- HermesX Remove TFT fast-path END

// Return the max rendered width (not just the last line) so multiline strings align correctly (e.g. version + short_name).
// If a display is provided, ASCII width is taken from the active font metrics; otherwise falls back to advanceX.
int stringAdvance(const char *text, int advanceX, OLEDDisplay *display)
{
    if (!text)
        return 0;

    const char *cursor = text;
    const char *end = cursor + std::strlen(text);
    int width = 0;
    int maxWidth = 0;
    while (cursor < end) {
        std::uint32_t cp = nextCodepoint(cursor, end);
        if (cp == 0)
            break;
        if (cp == '\n') {
            if (width > maxWidth)
                maxWidth = width;
            width = 0;
            continue;
        }
        if (cp == '\r')
            continue;
        if (cp >= 0x20u && cp < 0x7Fu && display) {
            ::String asciiString(static_cast<char>(cp));
            int glyphWidth = static_cast<int>(display->getStringWidth(asciiString));
            if (glyphWidth <= 0) {
                glyphWidth = advanceX;
            }
            width += glyphWidth;
        } else {
            width += advanceX;
        }
    }
    if (width > maxWidth)
        maxWidth = width;
    return maxWidth;
}

} // namespace graphics::HermesX_zh

