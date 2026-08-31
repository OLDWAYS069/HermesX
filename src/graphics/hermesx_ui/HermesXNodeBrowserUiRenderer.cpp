#include "HermesXNodeBrowserUiRenderer.h"

#include "HermesXNodeBrowserUiModel.h"
#include "HermesXTextLayout.h"
#include "graphics/ScreenFonts.h"
#include "graphics/fonts/HermesX_zh/HermesX_CN12.h"
#include <algorithm>
#include <cmath>

namespace graphics
{
namespace
{

void prepareDisplay(OLEDDisplay *display, int16_t x, int16_t y, bool clearBackground)
{
    if (clearBackground) {
#if defined(USE_EINK)
        display->setColor(EINK_WHITE);
#else
        display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
        display->fillRect(x, y, display->getWidth(), display->getHeight());
#if defined(USE_EINK)
        display->setColor(EINK_BLACK);
#else
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
    }
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);
}

void drawMixed(OLEDDisplay *display, int16_t x, int16_t y, int16_t width, const char *text, int16_t lineHeight)
{
    HermesXTextLayout::drawMixedSingleLineBounded(display, x, y, width, text, lineHeight);
}

void drawStatus(OLEDDisplay *display,
                int16_t x,
                int16_t rowY,
                int16_t width,
                int16_t rowH,
                const String &label,
                const String &status,
                bool mixedStatus)
{
    const int16_t statusW = mixedStatus
                                ? HermesX_zh::stringAdvance(status.c_str(), HermesX_zh::GLYPH_WIDTH, display)
                                : display->getStringWidth(status);
    const int16_t statusX = x + width - statusW - 2;
    if (mixedStatus) {
        drawMixed(display, statusX, rowY, statusW + 2, status.c_str(), rowH);
    } else {
        display->drawString(statusX, rowY, status);
    }
    const int16_t textWidth = statusX - x - 4;
    drawMixed(display, x + 2, rowY, textWidth > 0 ? textWidth : width - 4, label.c_str(), rowH);
}

} // namespace

void HermesXNodeBrowserUiRenderer::drawMenu(OLEDDisplay *display,
                                             int16_t x,
                                             int16_t y,
                                             const char *title,
                                             const char *const *items,
                                             uint8_t itemCount,
                                             uint8_t cursor,
                                             bool clearBackground,
                                             uint8_t rowPadding,
                                             uint8_t titleGap)
{
    if (!display || !items || itemCount == 0) {
        return;
    }
    prepareDisplay(display, x, y, clearBackground);
    const int16_t width = display->getWidth();
    drawMixed(display, x, y, width - 2, title, FONT_HEIGHT_SMALL);
    const int16_t rowH = FONT_HEIGHT_SMALL + rowPadding;
    const int16_t listTop = y + FONT_HEIGHT_SMALL + titleGap;
    const uint8_t selected = std::min<uint8_t>(cursor, itemCount - 1);
    const int8_t visibleRows = std::max<int8_t>(1, (display->getHeight() - listTop - 1) / rowH);
    for (uint8_t row = 0; row < itemCount && row < static_cast<uint8_t>(visibleRows); ++row) {
        const int16_t rowY = listTop + row * rowH;
        if (row == selected) {
            display->drawRect(x, rowY - 1, width - 2, rowH);
        }
        drawMixed(display, x + 2, rowY, width - 4, items[row], rowH);
    }
}

void HermesXNodeBrowserUiRenderer::drawList(OLEDDisplay *display,
                                             int16_t x,
                                             int16_t y,
                                             const char *title,
                                             const char *backLabel,
                                             const char *emptyLabel,
                                             uint8_t cursor,
                                             const HermesXNodeBrowserRowSource &rows,
                                             uint8_t maxVisibleRows,
                                             bool clearBackground)
{
    if (!display) {
        return;
    }
    prepareDisplay(display, x, y, clearBackground);
    const int16_t width = display->getWidth();
    drawMixed(display, x, y, width - 2, title, FONT_HEIGHT_SMALL);
    const int16_t rowH = FONT_HEIGHT_SMALL + 3;
    const int16_t listTop = y + FONT_HEIGHT_SMALL + 2;
    int8_t visibleRows = std::max<int8_t>(1, (display->getHeight() - listTop - 1) / rowH);
    visibleRows = std::min<int8_t>(visibleRows, maxVisibleRows);
    const uint8_t totalRows = rows.count + 1;
    const uint8_t selected = std::min<uint8_t>(cursor, rows.count);
    const uint8_t startCursor = selected >= static_cast<uint8_t>(visibleRows)
                                    ? selected - static_cast<uint8_t>(visibleRows) + 1
                                    : 0;

    for (int8_t row = 0; row < visibleRows; ++row) {
        const uint8_t cursorIndex = startCursor + row;
        if (cursorIndex >= totalRows) {
            break;
        }
        const int16_t rowY = listTop + row * rowH;
        if (cursorIndex == selected) {
            display->drawRect(x, rowY - 1, width - 2, rowH);
        }
        if (cursorIndex == 0) {
            drawMixed(display, x + 2, rowY, width - 4, backLabel, rowH);
            continue;
        }
        String label;
        String status;
        if (rows.rowAt && rows.rowAt(rows.context, cursorIndex - 1, label, status)) {
            drawStatus(display, x, rowY, width, rowH, label, status, rows.mixedStatus);
        }
    }
    if (rows.count == 0 && visibleRows > 1) {
        drawMixed(display, x + 2, listTop + rowH, width - 4, emptyLabel, rowH);
    }
}

void HermesXNodeBrowserUiRenderer::drawDetail(OLEDDisplay *display,
                                               int16_t x,
                                               int16_t y,
                                               const char *title,
                                               const char *noDataLabel,
                                               const String *rows,
                                               uint8_t rowCount,
                                               uint8_t cursor,
                                               bool clearBackground)
{
    if (!display) {
        return;
    }
    prepareDisplay(display, x, y, clearBackground);
    const int16_t width = display->getWidth();
    drawMixed(display, x, y, width - 2, title, FONT_HEIGHT_SMALL);
    if (!rows || rowCount == 0) {
        drawMixed(display, x + 2, y + FONT_HEIGHT_SMALL + 4, width - 4, noDataLabel, FONT_HEIGHT_SMALL);
        return;
    }
    const int16_t rowH = FONT_HEIGHT_SMALL + 3;
    const int16_t listTop = y + FONT_HEIGHT_SMALL + 2;
    int8_t visibleRows = std::max<int8_t>(1, (display->getHeight() - listTop - 1) / rowH);
    visibleRows = std::min<int8_t>(visibleRows, 4);
    const uint8_t selected = std::min<uint8_t>(cursor, rowCount - 1);
    const uint8_t startCursor = selected >= static_cast<uint8_t>(visibleRows)
                                    ? selected - static_cast<uint8_t>(visibleRows) + 1
                                    : 0;
    for (int8_t row = 0; row < visibleRows; ++row) {
        const uint8_t rowIndex = startCursor + row;
        if (rowIndex >= rowCount) {
            break;
        }
        const int16_t rowY = listTop + row * rowH;
        if (rowIndex == selected) {
            display->drawRect(x, rowY - 1, width - 2, rowH);
        }
        drawMixed(display, x + 2, rowY, width - 4, rows[rowIndex].c_str(), rowH);
    }
}

void HermesXNodeBrowserUiRenderer::drawFinderRadarIcon(OLEDDisplay *display,
                                                        int16_t centerX,
                                                        int16_t centerY,
                                                        int16_t radius,
                                                        bool compact,
                                                        int16_t sweepAngleDeg)
{
    if (!display) {
        return;
    }
    radius = std::max<int16_t>(8, radius);
    const int16_t middleRadius = (radius * 3) / 4;
    const int16_t innerRadius = radius / 2;
    const int16_t coreRadius = std::max<int16_t>(4, radius / 4);
    const int16_t crossExtension = compact ? 4 : 6;
    const int16_t dotRadius = compact ? 3 : 4;

    display->drawCircle(centerX, centerY, radius);
    display->drawCircle(centerX, centerY, middleRadius);
    display->drawCircle(centerX, centerY, innerRadius);
    display->drawCircle(centerX, centerY, coreRadius);
    display->drawLine(centerX - radius - crossExtension, centerY, centerX + radius + crossExtension, centerY);
    display->drawLine(centerX, centerY - radius - crossExtension, centerX, centerY + radius + crossExtension);
    display->drawLine(centerX - middleRadius + 1, centerY + middleRadius - 1, centerX - 2, centerY + 2);
    display->drawCircle(centerX + (radius * 4) / 5, centerY - (radius * 3) / 4, dotRadius);
    display->drawCircle(centerX - (middleRadius * 4) / 5, centerY - (innerRadius * 2) / 3, dotRadius);
    display->drawCircle(centerX + (middleRadius * 2) / 3, centerY + (radius * 2) / 3, dotRadius);

    if (sweepAngleDeg != INT16_MIN) {
        const float radians = static_cast<float>(sweepAngleDeg) * PI / 180.0f;
        const int16_t sweepX = centerX + static_cast<int16_t>(std::cos(radians) * static_cast<float>(radius - 2));
        const int16_t sweepY = centerY + static_cast<int16_t>(std::sin(radians) * static_cast<float>(radius - 2));
        display->drawLine(centerX, centerY, sweepX, sweepY);
        display->fillCircle(sweepX, sweepY, compact ? 1 : 2);
    }
}

void HermesXNodeBrowserUiRenderer::drawFinderPulseConfirm(OLEDDisplay *display, uint32_t nowMs, uint32_t armDelayMs)
{
    const auto &state = HermesXNodeBrowserUiModel::instance().finderPulseState();
    if (!display || !state.confirmVisible) {
        return;
    }
    uint32_t remainMs = 0;
    if (state.confirmShownAtMs != 0) {
        const uint32_t elapsedMs = nowMs - state.confirmShownAtMs;
        if (elapsedMs < armDelayMs) {
            remainMs = armDelayMs - elapsedMs;
        }
    }
    const uint32_t remainSec = (remainMs + 999U) / 1000U;
    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    const int16_t boxX = 3;
    const int16_t boxY = 2;
    const int16_t boxW = width - 6;
    const int16_t boxH = height - 4;
    const int16_t titleBarH = 12;
    const int16_t optionH = 10;
    const int16_t optionY = boxY + boxH - optionH - 2;
    const int16_t optionW = (boxW - 13) / 2;
    const int16_t cancelButtonX = boxX + 4;
    const int16_t confirmButtonX = cancelButtonX + optionW + 5;
#if defined(USE_EINK)
    const auto dialogBg = EINK_WHITE;
    const auto dialogFg = EINK_BLACK;
#else
    const auto dialogBg = OLEDDISPLAY_COLOR::WHITE;
    const auto dialogFg = OLEDDISPLAY_COLOR::BLACK;
#endif
    display->setColor(dialogBg);
    display->fillRect(boxX, boxY, boxW, boxH);
    display->setColor(dialogFg);
    display->drawRect(boxX, boxY, boxW, boxH);
    display->fillRect(boxX + 1, boxY + 1, boxW - 2, titleBarH);
    display->setColor(dialogBg);
    const char *title = u8"尋人模式警告";
    const int titleW = HermesX_zh::stringAdvance(title, HermesX_zh::GLYPH_WIDTH, display);
    const int16_t titleX = std::max<int16_t>(boxX + 2, boxX + (boxW - titleW) / 2);
    HermesX_zh::drawMixedBounded(*display, titleX, boxY + 1, boxW - 4, title, HermesX_zh::GLYPH_WIDTH,
                                 FONT_HEIGHT_SMALL, nullptr);
    display->setColor(dialogFg);
    const int advance = HermesX_zh::GLYPH_WIDTH - 1;
    const int16_t bodyX = boxX + 5;
    const int16_t bodyW = boxW - 10;
    const int16_t bodyY = boxY + titleBarH + 2;
    const int16_t statusY = optionY - FONT_HEIGHT_SMALL - 2;
    const int16_t lineHeight = std::max<int16_t>(FONT_HEIGHT_SMALL - 1,
                                                std::min<int16_t>(12, (statusY - bodyY - 1) / 2));
    HermesX_zh::drawMixedBounded(*display, bodyX, bodyY, bodyW, u8"注意：3秒後將強制", advance, lineHeight, nullptr);
    HermesX_zh::drawMixedBounded(*display, bodyX, bodyY + lineHeight, bodyW, u8"同頻裝置回報位置", advance,
                                 lineHeight, nullptr);
    const auto drawOption = [&](int16_t optionX, const char *label, bool selected, bool enabled) {
        selected = selected && enabled;
        display->setColor(dialogFg);
        if (selected) {
            display->fillRect(optionX, optionY, optionW, optionH);
            display->setColor(dialogBg);
        } else {
            display->drawRect(optionX, optionY, optionW, optionH);
        }
        const int textW = HermesX_zh::stringAdvance(label, HermesX_zh::GLYPH_WIDTH, display);
        const int16_t textX = std::max<int16_t>(optionX + 1, optionX + (optionW - textW) / 2);
        HermesX_zh::drawMixedBounded(*display, textX, optionY + 1, optionW - 2, label, HermesX_zh::GLYPH_WIDTH,
                                     FONT_HEIGHT_SMALL, nullptr);
        display->setColor(dialogFg);
    };
    drawOption(cancelButtonX, u8"取消", state.confirmSelected == 0, true);
    drawOption(confirmButtonX, u8"發送", state.confirmSelected != 0, remainMs == 0);
    char countdown[48];
    snprintf(countdown, sizeof(countdown), u8"%lus後自動發送尋人訊號", static_cast<unsigned long>(remainSec));
    HermesX_zh::drawMixedBounded(*display, bodyX, statusY, bodyW, countdown, advance, FONT_HEIGHT_SMALL, nullptr);
}

void HermesXNodeBrowserUiRenderer::drawFinderPulseSending(OLEDDisplay *display, uint32_t nowMs)
{
    const auto &state = HermesXNodeBrowserUiModel::instance().finderPulseState();
    if (!display || !state.sendingVisible) {
        return;
    }
    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
    const auto background = EINK_WHITE;
    const auto foreground = EINK_BLACK;
#else
    const auto background = OLEDDISPLAY_COLOR::WHITE;
    const auto foreground = OLEDDISPLAY_COLOR::BLACK;
#endif
    display->setColor(background);
    display->fillRect(0, 0, width, height);
    display->setColor(foreground);
    const uint32_t elapsedMs = nowMs - state.sendingShownAtMs;
    const int16_t radius = std::max<int16_t>(16, std::min<int16_t>(width, height) / 4);
    const int16_t centerX = width / 2;
    const int16_t centerY = height / 2 - 14;
    const int16_t sweepAngle = static_cast<int16_t>((elapsedMs / 30U) % 360U) - 90;
    drawFinderRadarIcon(display, centerX, centerY, radius, false, sweepAngle);
    const int advance = HermesX_zh::GLYPH_WIDTH - 1;
    const int16_t textY = centerY + radius + 10;
    HermesX_zh::drawMixedBounded(*display, 4, textY, width - 8, u8"正在發送尋人訊號", advance,
                                 FONT_HEIGHT_SMALL, nullptr);
    HermesX_zh::drawMixedBounded(*display, 4, textY + FONT_HEIGHT_SMALL + 4, width - 8, u8"等待位置回報...", advance,
                                 FONT_HEIGHT_SMALL, nullptr);
}

} // namespace graphics
