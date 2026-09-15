#pragma once

#include "HermesXTextLayout.h"
#include "graphics/ScreenFonts.h"
#include "graphics/fonts/HermesX_zh/HermesX_CN12.h"
#include <algorithm>

namespace graphics
{
// 128x64 dialogs reserve separate bands for the title, two full-size mixed
// text lines and buttons. drawMixedBounded wraps; single-line regions must
// use drawMixedSingleLineBounded instead.
namespace HermesXCompactUi
{
constexpr int16_t LineHeight = 14;
constexpr int16_t BodyTop = 17;
constexpr int16_t ButtonHeight = 16;

inline bool applies(OLEDDisplay *display)
{
    return display && display->getWidth() <= 128 && display->getHeight() <= 64;
}

inline void line(OLEDDisplay *display, int16_t x, int16_t y, int16_t width, const char *text)
{
    HermesXTextLayout::drawMixedSingleLineBounded(display, x, y, width, text, LineHeight);
}

inline void begin(OLEDDisplay *display, const char *title)
{
    display->setFont(FONT_SMALL_LOCAL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setColor(BLACK);
    display->fillRect(0, 0, display->getWidth(), display->getHeight());
    display->setColor(WHITE);
    line(display, 3, 0, display->getWidth() - 6, title);
    display->drawHorizontalLine(0, 14, display->getWidth());
}

inline void button(OLEDDisplay *display, int16_t x, int16_t width, const char *label, bool selected)
{
    const int16_t y = display->getHeight() - ButtonHeight;
    display->setColor(WHITE);
    if (selected) {
        display->fillRect(x, y, width, ButtonHeight);
        display->setColor(BLACK);
    } else {
        display->drawRect(x, y, width, ButtonHeight);
    }
    const int16_t labelWidth = HermesX_zh::stringAdvance(label, HermesX_zh::GLYPH_WIDTH, display);
    const int16_t labelX = x + std::max<int16_t>(2, (width - labelWidth) / 2);
    line(display, labelX, y + 1, x + width - 2 - labelX, label);
    display->setColor(WHITE);
}

inline void options(OLEDDisplay *display, const char *left, const char *right, uint8_t selected)
{
    const int16_t width = display->getWidth();
    if (!right) {
        button(display, 2, width - 4, left, true);
    } else {
        const int16_t optionWidth = (width - 7) / 2;
        button(display, 2, optionWidth, left, selected == 0);
        button(display, optionWidth + 5, optionWidth, right, selected != 0);
    }
}

inline void body(OLEDDisplay *display, const char *text, uint32_t nowMs)
{
    const auto lines = HermesXTextLayout::buildMixedWrappedLines(display, text, display->getWidth() - 6);
    const size_t pages = (lines.size() + 1) / 2;
    const size_t first = pages > 1 ? ((nowMs / 2500U) % pages) * 2 : 0;
    for (size_t row = 0; row < 2 && first + row < lines.size(); ++row)
        line(display, 3, BodyTop + row * LineHeight, display->getWidth() - 6, lines[first + row].c_str());
}

inline void scrollable(OLEDDisplay *display, const char *title, const char *body,
                       uint16_t &scrollY, uint16_t &maxScrollY)
{
    begin(display, title);
    const int16_t width = display->getWidth();
    const auto lines = HermesXTextLayout::buildMixedWrappedLines(display, body, width - 11);
    const size_t visible = std::max<int16_t>(1, (display->getHeight() - ButtonHeight - BodyTop) / LineHeight);
    maxScrollY = lines.size() > visible ? (lines.size() - visible) * LineHeight : 0;
    // Input controllers use pixel steps; snap to full lines so no glyph can
    // cross the title divider or the action band, including at the last page.
    scrollY = std::min<uint16_t>(maxScrollY, ((scrollY + LineHeight / 2) / LineHeight) * LineHeight);
    const size_t first = scrollY / LineHeight;
    for (size_t row = 0; row < visible && first + row < lines.size(); ++row)
        line(display, 3, BodyTop + row * LineHeight, width - 11, lines[first + row].c_str());
    if (maxScrollY) {
        const int16_t trackHeight = visible * LineHeight;
        display->drawVerticalLine(width - 4, BodyTop, trackHeight);
        const int16_t thumbY = BodyTop + (trackHeight - 4) * scrollY / maxScrollY;
        display->fillRect(width - 5, thumbY, 3, 4);
    }
    options(display, u8"退出", nullptr, 0);
}
} // namespace HermesXCompactUi
} // namespace graphics
