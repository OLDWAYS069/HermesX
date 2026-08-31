#pragma once

#include <OLEDDisplay.h>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace graphics
{
namespace HermesXTextLayout
{

void copyUtf8Snippet(const char *src, size_t srcLen, char *out, size_t outSize, size_t maxCodepoints);
void drawScaledHanzi(OLEDDisplay &display, int16_t x, int16_t y, int glyphIndex, uint8_t scale);
void drawSizedHanzi(OLEDDisplay &display, int16_t x, int16_t y, int glyphIndex, uint8_t targetSize);
void drawLargeMixedLine(OLEDDisplay &display,
                        int16_t x,
                        int16_t y,
                        int16_t maxWidth,
                        const char *text,
                        uint8_t hanziScale);
void drawSizedMixedLine(OLEDDisplay &display,
                        int16_t x,
                        int16_t y,
                        int16_t maxWidth,
                        const char *text,
                        uint8_t hanziSize);
uint16_t measureMixedWrappedTextHeight(OLEDDisplay *display,
                                       const char *text,
                                       int16_t maxWidth,
                                       int lineHeight,
                                       int advanceX = 12);
std::vector<String> buildMixedWrappedLines(OLEDDisplay *display, const char *text, int16_t maxWidth, int advanceX = 12);
void drawMixedSingleLineBounded(OLEDDisplay *display,
                                int16_t x,
                                int16_t y,
                                int16_t maxWidth,
                                const char *text,
                                int lineHeight,
                                int advanceX = 12);
void drawVisibleWrappedLines(OLEDDisplay *display,
                             const std::vector<String> &lines,
                             int16_t x,
                             int16_t y,
                             int16_t maxWidth,
                             int16_t bodyH,
                             int lineHeight,
                             uint16_t scrollY,
                             int advanceX = 12,
                             uint8_t hanziScale = 1,
                             uint8_t hanziTargetSize = 0);

} // namespace HermesXTextLayout
} // namespace graphics
