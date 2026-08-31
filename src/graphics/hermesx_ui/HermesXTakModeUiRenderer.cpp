#include "HermesXTakModeUiRenderer.h"

#include "HermesXFastSetupUiRenderer.h"
#include "graphics/ScreenFonts.h"
#include "graphics/fonts/HermesX_zh/HermesX_CN12.h"
#include <algorithm>

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

void drawMixed(OLEDDisplay *display, int16_t x, int16_t y, int16_t width, const char *text, int16_t lineHeight)
{
    HermesX_zh::drawMixedBounded(*display, x, y, width, text, HermesX_zh::GLYPH_WIDTH, lineHeight, nullptr);
}

void drawShield(OLEDDisplay *display, int16_t centerX, int16_t topY, int16_t width, int16_t height, uint8_t thickness)
{
    width = std::max<int16_t>(18, width);
    height = std::max<int16_t>(20, height);
    thickness = std::max<uint8_t>(1, thickness);
    for (uint8_t layer = 0; layer < thickness; ++layer) {
        const int16_t left = centerX - width / 2 + layer;
        const int16_t right = centerX + width / 2 - layer;
        const int16_t top = topY + layer;
        const int16_t shoulderY = top + height / 5;
        const int16_t waistY = top + (height * 2) / 3;
        const int16_t bottom = top + height - layer;
        if (right - left < 8 || bottom - top < 8) {
            break;
        }
        display->drawLine(left + 2, shoulderY, centerX, top);
        display->drawLine(centerX, top, right - 2, shoulderY);
        display->drawLine(left, shoulderY + 1, left + 4, waistY);
        display->drawLine(right, shoulderY + 1, right - 4, waistY);
        display->drawLine(left + 4, waistY, centerX, bottom);
        display->drawLine(right - 4, waistY, centerX, bottom);
    }
    const int16_t midY = topY + height / 2;
    display->drawLine(centerX, topY + height / 4, centerX, topY + height - 5);
    display->drawLine(centerX - width / 5, midY, centerX + width / 5, midY);
}

uint8_t visibleRowCount(int16_t boxY, int16_t boxHeight, int16_t listTop, int16_t rowHeight, uint8_t rowCount)
{
    uint8_t visible = static_cast<uint8_t>((boxY + boxHeight - listTop - 2) / rowHeight);
    visible = std::max<uint8_t>(1, std::min<uint8_t>(5, visible));
    return std::min<uint8_t>(visible, rowCount);
}

uint8_t resolvedOffset(uint8_t selected, uint8_t requestedOffset, uint8_t rowCount, uint8_t visibleRows)
{
    if (rowCount == 0 || visibleRows == 0) {
        return 0;
    }
    selected = std::min<uint8_t>(selected, rowCount - 1);
    uint8_t offset = std::min<uint8_t>(requestedOffset, rowCount - visibleRows);
    if (selected < offset) {
        offset = selected;
    } else if (selected >= offset + visibleRows) {
        offset = selected - visibleRows + 1;
    }
    return offset;
}

void drawRows(OLEDDisplay *display,
              int16_t boxX,
              int16_t boxWidth,
              int16_t listTop,
              int16_t rowHeight,
              uint8_t visibleRows,
              uint8_t offset,
              uint8_t selected,
              const HermesXTakModeRowSource &rows)
{
    if (!rows.rowAt) {
        return;
    }
    for (uint8_t row = 0; row < visibleRows; ++row) {
        const uint8_t index = offset + row;
        if (index >= rows.count) {
            break;
        }
        String line;
        if (!rows.rowAt(rows.context, index, line)) {
            continue;
        }
        const int16_t rowY = listTop + row * rowHeight;
        if (index == selected) {
            display->drawRect(boxX + 3, rowY - 1, boxWidth - 6, rowHeight);
        }
        drawMixed(display, boxX + 6, rowY, boxWidth - 12, line.c_str(), rowHeight);
    }
}

bool popupRowAt(const HermesXTakModeRenderView &view, uint8_t index, String &line)
{
    static const char *const rows[] = {nullptr, u8"TAKMODE設定", u8"頻道選擇", u8"GROUP設定",
                                       "EMUI", u8"尋人模組", u8"返回主選單"};
    if (index >= sizeof(rows) / sizeof(rows[0])) {
        return false;
    }
    line = index == 0 ? view.title : rows[index];
    if (index == 0) {
        line += view.active ? ": ON" : ": OFF";
    } else if (index == 4 && !view.allowEmUi) {
        line += ": OFF";
    } else if (index == 5 && !view.allowFinder) {
        line += ": OFF";
    }
    return true;
}

} // namespace

void HermesXTakModeUiRenderer::draw(OLEDDisplay *display,
                                     int16_t x,
                                     int16_t y,
                                     bool overlayOnly,
                                     const HermesXTakModeUiState &state,
                                     const HermesXTakModeRenderView &view)
{
    if (!display) {
        return;
    }
    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    const bool popupView = state.page == HermesXTakModePage::Popup;
    const bool settingsView = state.page == HermesXTakModePage::Settings;
    const bool channelView = state.page == HermesXTakModePage::ChannelSelect;

    if (!overlayOnly) {
        setBackgroundColor(display);
        display->fillRect(0, 0, width, height);
    }
    setForegroundColor(display);
    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    if (!overlayOnly) {
        const int16_t titleWidth = HermesX_zh::stringAdvance(view.title, HermesX_zh::GLYPH_WIDTH, display);
        const int16_t titleX = std::max<int16_t>(x + 2, x + (width - titleWidth) / 2);
        drawMixed(display, titleX, y + 2, width - 4, view.title, FONT_HEIGHT_SMALL);

        const int16_t shieldHeight = std::max<int16_t>(24, std::min<int16_t>(height / 2, 38));
        drawShield(display, x + width / 2, y + (height - shieldHeight) / 2 - 5, shieldHeight + 8, shieldHeight, 2);

        const char *status = view.active ? "ON" : "OFF";
        const int16_t statusWidth = HermesX_zh::stringAdvance(status, HermesX_zh::GLYPH_WIDTH, display);
        drawMixed(display, x + (width - statusWidth) / 2, y + height - FONT_HEIGHT_SMALL - 3, width - 4, status,
                  FONT_HEIGHT_SMALL);
    }

    if (!overlayOnly && !popupView && !settingsView && !channelView) {
        if (view.slotLine) {
            drawMixed(display, x + 4, y + 15, width - 8, view.slotLine->c_str(), FONT_HEIGHT_SMALL);
        }
        if (view.utilizationLine) {
            drawMixed(display, x + 4, y + height - 27, width - 8, view.utilizationLine->c_str(), FONT_HEIGHT_SMALL);
        }
        if (view.suggestionLine && !view.suggestionLine->isEmpty()) {
            drawMixed(display, x + 4, y + height - 16, width - 8, view.suggestionLine->c_str(), FONT_HEIGHT_SMALL);
        }
    }

    if (!popupView && !settingsView && !channelView) {
        return;
    }

    const int16_t boxX = x + 8;
    const int16_t boxY = y + 10;
    const int16_t boxWidth = width - 16;
    const int16_t boxHeight = height - 18;
    setBackgroundColor(display);
    display->fillRect(boxX, boxY, boxWidth, boxHeight);
    setForegroundColor(display);
    display->drawRect(boxX, boxY, boxWidth, boxHeight);

    const char *modalTitle = settingsView ? u8"TAKMODE設定" : channelView ? u8"頻道選擇" : view.title;
    const int16_t modalTitleWidth = HermesX_zh::stringAdvance(modalTitle, HermesX_zh::GLYPH_WIDTH, display);
    drawMixed(display, boxX + std::max<int16_t>(2, (boxWidth - modalTitleWidth) / 2), boxY + 2, boxWidth - 4,
              modalTitle, FONT_HEIGHT_SMALL);

    const int16_t rowHeight = FONT_HEIGHT_SMALL + 1;
    const int16_t listTop = boxY + FONT_HEIGHT_SMALL + 6;
    if (popupView) {
        constexpr uint8_t PopupRowCount = 7;
        const uint8_t visibleRows = visibleRowCount(boxY, boxHeight, listTop, rowHeight, PopupRowCount);
        const uint8_t selected = std::min<uint8_t>(state.popupSelected, PopupRowCount - 1);
        const uint8_t offset = resolvedOffset(selected, state.popupOffset, PopupRowCount, visibleRows);
        for (uint8_t row = 0; row < visibleRows; ++row) {
            const uint8_t index = offset + row;
            String line;
            if (!popupRowAt(view, index, line)) {
                continue;
            }
            const int16_t rowY = listTop + row * rowHeight;
            if (index == selected) {
                display->drawRect(boxX + 3, rowY - 1, boxWidth - 6, rowHeight);
            }
            drawMixed(display, boxX + 6, rowY, boxWidth - 12, line.c_str(), rowHeight);
        }
        return;
    }

    const HermesXTakModeRowSource &rows = channelView ? view.channelRows : view.settingsRows;
    const uint8_t visibleRows = visibleRowCount(boxY, boxHeight, listTop, rowHeight, rows.count);
    const uint8_t selected = rows.count == 0 ? 0 : std::min<uint8_t>(state.settingsSelected, rows.count - 1);
    const uint8_t offset = resolvedOffset(selected, state.settingsOffset, rows.count, visibleRows);
    drawRows(display, boxX, boxWidth, listTop, rowHeight, visibleRows, offset, selected, rows);
}

void HermesXTakModeUiRenderer::drawTransition(OLEDDisplay *display, uint32_t startedAtMs, bool entering)
{
    if (!display) {
        return;
    }
    HermesXFastSetupUiRenderer::drawUpdateTransitionPage(display, display->getWidth(), display->getHeight(), startedAtMs,
                                                         entering ? u8"進入TAK模式" : u8"退出TAK模式", "");
}

} // namespace graphics
