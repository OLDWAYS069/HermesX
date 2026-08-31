#include "HermesXTextLayout.h"

#include "graphics/fonts/HermesX_zh/HermesX_CN12.h"
#include <algorithm>
#include <cstring>

namespace graphics
{
namespace HermesXTextLayout
{
namespace
{

size_t utf8SequenceLength(uint8_t firstByte)
{
    if ((firstByte & 0x80u) == 0) {
        return 1;
    }
    if ((firstByte & 0xE0u) == 0xC0u) {
        return 2;
    }
    if ((firstByte & 0xF0u) == 0xE0u) {
        return 3;
    }
    if ((firstByte & 0xF8u) == 0xF0u) {
        return 4;
    }
    return 1;
}

uint8_t readGlyphByte(const uint8_t *ptr)
{
#if defined(ARDUINO_ARCH_AVR)
    return pgm_read_byte(ptr);
#else
    return *ptr;
#endif
}

String substringAsString(const char *start, size_t length)
{
    String result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += start[i];
    }
    return result;
}

void appendWrappedMixedLines(std::vector<String> &lines, OLEDDisplay *display, const char *text, int16_t maxWidth, int advanceX)
{
    if (!display || !text || maxWidth <= 0) {
        lines.push_back(String());
        return;
    }

    String currentLine;
    int lineWidth = 0;
    auto flushLine = [&]() {
        lines.push_back(currentLine);
        currentLine = "";
        lineWidth = 0;
    };

    const char *cursor = text;
    const char *end = cursor + std::strlen(text);
    while (cursor < end) {
        const char *cpStart = cursor;
        const std::uint32_t cp = HermesX_zh::nextCodepoint(cursor, end);
        if (cp == 0) {
            break;
        }
        if (cp == '\r') {
            continue;
        }
        if (cp == '\n') {
            flushLine();
            continue;
        }

        int glyphWidth = advanceX;
        if (cp >= 0x20u && cp < 0x7Fu) {
            const String asciiChar(static_cast<char>(cp));
            glyphWidth = static_cast<int>(display->getStringWidth(asciiChar));
            if (glyphWidth <= 0) {
                glyphWidth = advanceX;
            }
        }
        if (lineWidth > 0 && lineWidth + glyphWidth > maxWidth) {
            flushLine();
        }
        currentLine += substringAsString(cpStart, static_cast<size_t>(cursor - cpStart));
        lineWidth += glyphWidth;
    }
    flushLine();
}

} // namespace

void copyUtf8Snippet(const char *src, size_t srcLen, char *out, size_t outSize, size_t maxCodepoints)
{
    if (!out || outSize == 0) {
        return;
    }
    out[0] = '\0';
    if (!src || srcLen == 0 || maxCodepoints == 0) {
        return;
    }

    size_t srcPos = 0;
    size_t dstPos = 0;
    size_t copiedCodepoints = 0;
    bool truncated = false;
    while (srcPos < srcLen && dstPos + 1 < outSize && copiedCodepoints < maxCodepoints) {
        size_t seqLen = utf8SequenceLength(static_cast<uint8_t>(src[srcPos]));
        if (srcPos + seqLen > srcLen || dstPos + seqLen >= outSize) {
            truncated = true;
            break;
        }

        bool allZero = true;
        for (size_t i = 0; i < seqLen; ++i) {
            if (src[srcPos + i] != '\0') {
                allZero = false;
                break;
            }
        }
        if (allZero) {
            break;
        }
        if (seqLen == 1 && (src[srcPos] == '\n' || src[srcPos] == '\r' || src[srcPos] == '\t')) {
            out[dstPos++] = ' ';
            ++srcPos;
            ++copiedCodepoints;
            continue;
        }
        std::memcpy(out + dstPos, src + srcPos, seqLen);
        dstPos += seqLen;
        srcPos += seqLen;
        ++copiedCodepoints;
    }

    if (srcPos < srcLen) {
        truncated = true;
    }
    if (truncated && outSize >= 4) {
        while (dstPos > 0 && (static_cast<uint8_t>(out[dstPos - 1]) & 0xC0u) == 0x80u) {
            --dstPos;
        }
        if (dstPos + 3 < outSize) {
            out[dstPos++] = '.';
            out[dstPos++] = '.';
            out[dstPos++] = '.';
        }
    }
    out[dstPos] = '\0';
}

void drawScaledHanzi(OLEDDisplay &display, int16_t x, int16_t y, int glyphIndex, uint8_t scale)
{
    const uint8_t *glyph = HermesX_zh::glyphData(glyphIndex);
    if (!glyph || scale == 0) {
        return;
    }
    const OLEDDISPLAY_COLOR color = display.getColor();
    for (uint8_t row = 0; row < HermesX_zh::GLYPH_HEIGHT; ++row) {
        for (uint8_t col = 0; col < HermesX_zh::GLYPH_WIDTH; ++col) {
            const int bitIndex = row * HermesX_zh::GLYPH_STRIDE_BITS + col;
            if (((readGlyphByte(glyph + (bitIndex >> 3)) >> (7 - (bitIndex & 7))) & 0x1u) == 0) {
                continue;
            }
            for (uint8_t dy = 0; dy < scale; ++dy) {
                for (uint8_t dx = 0; dx < scale; ++dx) {
                    display.setPixelColor(x + col * scale + dx, y + row * scale + dy, color);
                }
            }
        }
    }
}

void drawSizedHanzi(OLEDDisplay &display, int16_t x, int16_t y, int glyphIndex, uint8_t targetSize)
{
    const uint8_t *glyph = HermesX_zh::glyphData(glyphIndex);
    if (!glyph || targetSize == 0) {
        return;
    }
    const OLEDDISPLAY_COLOR color = display.getColor();
    for (uint8_t dy = 0; dy < targetSize; ++dy) {
        const uint8_t srcRow = (static_cast<uint16_t>(dy) * HermesX_zh::GLYPH_HEIGHT) / targetSize;
        for (uint8_t dx = 0; dx < targetSize; ++dx) {
            const uint8_t srcCol = (static_cast<uint16_t>(dx) * HermesX_zh::GLYPH_WIDTH) / targetSize;
            const int bitIndex = srcRow * HermesX_zh::GLYPH_STRIDE_BITS + srcCol;
            if (((readGlyphByte(glyph + (bitIndex >> 3)) >> (7 - (bitIndex & 7))) & 0x1u) != 0) {
                display.setPixelColor(x + dx, y + dy, color);
            }
        }
    }
}

void drawLargeMixedLine(OLEDDisplay &display, int16_t x, int16_t y, int16_t maxWidth, const char *text, uint8_t hanziScale)
{
    if (!text || maxWidth <= 0 || hanziScale == 0) {
        return;
    }
    const int16_t originX = x;
    const char *cursor = text;
    const char *end = cursor + std::strlen(text);
    while (cursor < end) {
        const uint32_t cp = HermesX_zh::nextCodepoint(cursor, end);
        if (cp == 0 || cp == '\n' || cp == '\r') {
            continue;
        }
        if (cp >= 0x20u && cp < 0x7Fu) {
            const String asciiChar(static_cast<char>(cp));
            const int glyphWidth = std::max<int>(1, display.getStringWidth(asciiChar));
            if (x + glyphWidth > originX + maxWidth) {
                break;
            }
            display.drawString(x, y, asciiChar);
            x += glyphWidth;
            continue;
        }
        int glyphIndex = HermesX_zh::locateCodepoint(cp);
        if (glyphIndex < 0) {
            HermesX_zh::incrementMissingGlyph();
            glyphIndex = HermesX_zh::fallbackIndex();
        }
        const int glyphWidth = HermesX_zh::GLYPH_WIDTH * hanziScale;
        if (x + glyphWidth > originX + maxWidth) {
            break;
        }
        drawScaledHanzi(display, x, y, glyphIndex, hanziScale);
        x += glyphWidth;
    }
}

void drawSizedMixedLine(OLEDDisplay &display, int16_t x, int16_t y, int16_t maxWidth, const char *text, uint8_t hanziSize)
{
    if (!text || maxWidth <= 0 || hanziSize == 0) {
        return;
    }
    const int16_t originX = x;
    const char *cursor = text;
    const char *end = cursor + std::strlen(text);
    while (cursor < end) {
        const uint32_t cp = HermesX_zh::nextCodepoint(cursor, end);
        if (cp == 0 || cp == '\n' || cp == '\r') {
            continue;
        }
        if (cp >= 0x20u && cp < 0x7Fu) {
            const String asciiChar(static_cast<char>(cp));
            const int glyphWidth = std::max<int>(1, display.getStringWidth(asciiChar));
            if (x + glyphWidth > originX + maxWidth) {
                break;
            }
            display.drawString(x, y, asciiChar);
            x += glyphWidth;
            continue;
        }
        int glyphIndex = HermesX_zh::locateCodepoint(cp);
        if (glyphIndex < 0) {
            HermesX_zh::incrementMissingGlyph();
            glyphIndex = HermesX_zh::fallbackIndex();
        }
        if (x + hanziSize > originX + maxWidth) {
            break;
        }
        drawSizedHanzi(display, x, y, glyphIndex, hanziSize);
        x += hanziSize;
    }
}

uint16_t measureMixedWrappedTextHeight(OLEDDisplay *display, const char *text, int16_t maxWidth, int lineHeight, int advanceX)
{
    if (!display || !text || maxWidth <= 0) {
        return static_cast<uint16_t>(std::max(lineHeight, 0));
    }
    int x = 0;
    int lines = 1;
    const char *cursor = text;
    const char *end = cursor + std::strlen(text);
    while (cursor < end) {
        const std::uint32_t cp = HermesX_zh::nextCodepoint(cursor, end);
        if (cp == 0) {
            break;
        }
        if (cp == '\r') {
            continue;
        }
        if (cp == '\n') {
            x = 0;
            ++lines;
            continue;
        }
        int glyphWidth = advanceX;
        if (cp >= 0x20u && cp < 0x7Fu) {
            const String asciiChar(static_cast<char>(cp));
            glyphWidth = static_cast<int>(display->getStringWidth(asciiChar));
            if (glyphWidth <= 0) {
                glyphWidth = advanceX;
            }
        }
        if (x + glyphWidth > maxWidth) {
            x = 0;
            ++lines;
        }
        x += glyphWidth;
    }
    return static_cast<uint16_t>(std::max(lines * lineHeight, lineHeight));
}

std::vector<String> buildMixedWrappedLines(OLEDDisplay *display, const char *text, int16_t maxWidth, int advanceX)
{
    std::vector<String> lines;
    appendWrappedMixedLines(lines, display, text, maxWidth, advanceX);
    if (lines.empty()) {
        lines.push_back(String());
    }
    return lines;
}

void drawMixedSingleLineBounded(OLEDDisplay *display,
                                int16_t x,
                                int16_t y,
                                int16_t maxWidth,
                                const char *text,
                                int lineHeight,
                                int advanceX)
{
    if (!display || !text || maxWidth <= 0) {
        return;
    }
    const std::vector<String> lines = buildMixedWrappedLines(display, text, maxWidth, advanceX);
    if (!lines.empty()) {
        HermesX_zh::drawMixedBounded(*display, x, y, maxWidth, lines.front().c_str(), advanceX, lineHeight, nullptr);
    }
}

void drawVisibleWrappedLines(OLEDDisplay *display,
                             const std::vector<String> &lines,
                             int16_t x,
                             int16_t y,
                             int16_t maxWidth,
                             int16_t bodyH,
                             int lineHeight,
                             uint16_t scrollY,
                             int advanceX,
                             uint8_t hanziScale,
                             uint8_t hanziTargetSize)
{
    if (!display || bodyH <= 0 || lineHeight <= 0) {
        return;
    }
    const uint16_t firstVisibleLine = static_cast<uint16_t>(scrollY / lineHeight);
    const uint16_t lineOffset = static_cast<uint16_t>(scrollY % lineHeight);
    const uint16_t maxVisibleLines = static_cast<uint16_t>(std::max<int>(1, bodyH / lineHeight) + 1);
    for (uint16_t i = 0; i < maxVisibleLines; ++i) {
        const uint16_t lineIndex = firstVisibleLine + i;
        if (lineIndex >= lines.size()) {
            break;
        }
        const int16_t drawY = y + static_cast<int16_t>(i * lineHeight) - static_cast<int16_t>(lineOffset);
        if (drawY >= y + bodyH) {
            break;
        }
        if (drawY + lineHeight <= y) {
            continue;
        }
        if (hanziTargetSize > 0) {
            drawSizedMixedLine(*display, x, drawY, maxWidth, lines[lineIndex].c_str(), hanziTargetSize);
        } else if (hanziScale > 1) {
            drawLargeMixedLine(*display, x, drawY, maxWidth, lines[lineIndex].c_str(), hanziScale);
        } else {
            HermesX_zh::drawMixedBounded(*display, x, drawY, maxWidth, lines[lineIndex].c_str(), advanceX, lineHeight, nullptr);
        }
    }
}

} // namespace HermesXTextLayout
} // namespace graphics
