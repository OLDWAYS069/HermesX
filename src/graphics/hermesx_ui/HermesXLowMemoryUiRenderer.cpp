#include "HermesXLowMemoryUiRenderer.h"

#include "graphics/ScreenFonts.h"
#include "graphics/fonts/HermesX_zh/HermesX_CN12.h"
#include <cstdio>

namespace graphics
{
namespace
{

void setBackgroundColor(OLEDDisplay *display)
{
#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
}

void setForegroundColor(OLEDDisplay *display)
{
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
}

void drawOption(OLEDDisplay *display, int16_t x, int16_t y, int16_t width, const char *label, bool selected)
{
    if (selected) {
        setForegroundColor(display);
        display->fillRect(x, y, width, 11);
        setBackgroundColor(display);
    } else {
        setForegroundColor(display);
        display->drawRect(x, y, width, 11);
    }
    const int16_t labelWidth = HermesX_zh::stringAdvance(label, HermesX_zh::GLYPH_WIDTH, display);
    int16_t labelX = x + (width - labelWidth) / 2;
    if (labelX < x + 1) {
        labelX = x + 1;
    }
    HermesX_zh::drawMixedBounded(*display, labelX, y + 1, width - 2, label, HermesX_zh::GLYPH_WIDTH,
                                 FONT_HEIGHT_SMALL, nullptr);
    setForegroundColor(display);
}

} // namespace

void HermesXLowMemoryUiRenderer::draw(OLEDDisplay *display, const HermesXLowMemoryUiState &state, bool protectionActive)
{
    if (!display || !state.visible) {
        return;
    }
    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    setBackgroundColor(display);
    display->fillRect(0, 0, width, height);
    setForegroundColor(display);

    const int advance = HermesX_zh::GLYPH_WIDTH - 1;
    const int16_t margin = 5;
    const int16_t bodyWidth = width - margin * 2;
    HermesX_zh::drawMixedBounded(*display, margin, 2, bodyWidth, u8"HEAP 保護模式", advance, 12, nullptr);
    const char *modeLine = protectionActive ? u8"已停用HOME/GPS特效與選單" : u8"Heap 已恢復，可退出";
    HermesX_zh::drawMixedBounded(*display, margin, 16, bodyWidth, modeLine, advance, 11, nullptr);

    char heapBuffer[48];
    snprintf(heapBuffer, sizeof(heapBuffer), "Heap %lu/%lu", static_cast<unsigned long>(state.triggerFree),
             static_cast<unsigned long>(state.triggerLargest));
    HermesX_zh::drawMixedBounded(*display, margin, 29, bodyWidth, heapBuffer, advance, 11, nullptr);
    const char *status = state.status[0] ? state.status : u8"可清除節點資料庫釋放空間";
    HermesX_zh::drawMixedBounded(*display, margin, 42, bodyWidth, status, advance, 11, nullptr);

    const int16_t optionY = height - 13;
    const int16_t optionWidth = (width - 14) / 2;
    drawOption(display, 4, optionY, optionWidth, u8"退出", state.selected == 0);
    drawOption(display, 10 + optionWidth, optionY, optionWidth, u8"清除節點", state.selected != 0);
}

} // namespace graphics
