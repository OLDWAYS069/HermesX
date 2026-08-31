#include "HermesXTraceRouteUiRenderer.h"

#include "HermesXTextLayout.h"
#include "HermesXTraceRouteUiModel.h"
#include "graphics/ScreenFonts.h"
#include "graphics/fonts/HermesX_zh/HermesX_CN12.h"
#include <algorithm>

namespace graphics
{

namespace
{

void prepareListDisplay(OLEDDisplay *display)
{
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);
}

void drawTitle(OLEDDisplay *display, int16_t x, int16_t y, int16_t width)
{
    HermesXTextLayout::drawMixedSingleLineBounded(display, x, y, width - 2, "TraceRoute", FONT_HEIGHT_SMALL);
}

void drawNodeRow(OLEDDisplay *display,
                 int16_t x,
                 int16_t rowY,
                 int16_t width,
                 int16_t rowH,
                 const String &label,
                 const String &status)
{
    const int16_t stateW = display->getStringWidth(status);
    const int16_t stateX = x + width - stateW - 2;
    display->drawString(stateX, rowY, status);
    const int16_t textWidth = stateX - x - 4;
    HermesXTextLayout::drawMixedSingleLineBounded(
        display, x + 2, rowY, textWidth > 0 ? textWidth : width - 4, label.c_str(), rowH);
}

} // namespace

void HermesXTraceRouteUiRenderer::drawPopup(OLEDDisplay *display)
{
    auto &popup = HermesXTraceRouteUiModel::instance().popupState();
    if (!display || !popup.visible) {
        return;
    }

    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    const bool compactLayout = width < 200 || height < 120;
    const int16_t marginX = compactLayout ? 5 : 12;
    const int16_t marginY = compactLayout ? 6 : 12;
    const int16_t boxX = marginX;
    const int16_t boxY = marginY;
    const int16_t boxW = std::max<int16_t>(width - marginX * 2, 40);
    const int16_t boxH = std::max<int16_t>(height - marginY * 2, 40);
    const int16_t titlePadX = compactLayout ? 4 : 6;
    const int16_t titlePadY = compactLayout ? 2 : 4;
    const int16_t titleH = compactLayout ? 16 : 18;
    const int16_t footerH = compactLayout ? 14 : 18;
    const int16_t dividerY = boxY + titleH + titlePadY * 2;
    const int16_t footerY = boxY + boxH - footerH;
    const int16_t bodyX = boxX + titlePadX;
    const int16_t bodyY = dividerY + 3;
    const int16_t scrollbarW = 4;
    const int16_t bodyW = std::max<int16_t>(boxW - titlePadX * 2 - scrollbarW - 2, 20);
    const int16_t bodyH = std::max<int16_t>(footerY - bodyY - 2, titleH);
    const int16_t lineHeight = compactLayout ? 12 : 14;

    display->setColor(BLACK);
    display->fillRect(boxX, boxY, boxW, boxH);
    display->setColor(WHITE);
    display->drawRect(boxX, boxY, boxW, boxH);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);
    HermesX_zh::drawMixedBounded(*display, bodyX, boxY + titlePadY, boxW - titlePadX * 2, popup.title.c_str(),
                                 HermesX_zh::GLYPH_WIDTH, titleH, nullptr);
    display->drawLine(boxX, dividerY, boxX + boxW - 1, dividerY);
    display->drawLine(boxX, footerY - 1, boxX + boxW - 1, footerY - 1);

    const uint16_t contentHeight =
        HermesXTextLayout::measureMixedWrappedTextHeight(display, popup.body.c_str(), bodyW, lineHeight);
    popup.maxScrollY = contentHeight > bodyH ? contentHeight - bodyH : 0;
    popup.scrollY = std::min(popup.scrollY, popup.maxScrollY);
    const std::vector<String> wrappedLines = HermesXTextLayout::buildMixedWrappedLines(
        display, popup.body.c_str(), bodyW, HermesX_zh::GLYPH_WIDTH);
    HermesXTextLayout::drawVisibleWrappedLines(display, wrappedLines, bodyX, bodyY, bodyW, bodyH, lineHeight,
                                               popup.scrollY, HermesX_zh::GLYPH_WIDTH);

    if (popup.maxScrollY > 0) {
        const int16_t trackX = boxX + boxW - scrollbarW;
        display->drawRect(trackX, bodyY, scrollbarW, bodyH);
        const int16_t thumbH =
            std::max<int16_t>(6, static_cast<int16_t>((static_cast<int32_t>(bodyH) * bodyH) / contentHeight));
        const int16_t thumbTravel = std::max<int16_t>(bodyH - thumbH - 2, 0);
        const int16_t thumbY = bodyY + 1 + static_cast<int16_t>(
                                                  (static_cast<int32_t>(thumbTravel) * popup.scrollY) / popup.maxScrollY);
        display->fillRect(trackX + 1, thumbY, scrollbarW - 2, thumbH);
    }

    const int16_t exitTextY = footerY + std::max<int16_t>(0, (footerH - FONT_HEIGHT_SMALL) / 2);
    const char *exitLabel = u8"退出";
    const int16_t exitW = display->getStringWidth(exitLabel);
    const int16_t exitX = boxX + std::max<int16_t>(2, (boxW - exitW) / 2);
    display->drawRect(exitX - 4, footerY + 1, exitW + 8, footerH - 3);
    HermesX_zh::drawMixedBounded(*display, exitX, exitTextY, exitW + 2, exitLabel, HermesX_zh::GLYPH_WIDTH, lineHeight,
                                 nullptr);
}

void HermesXTraceRouteUiRenderer::drawSearchResult(OLEDDisplay *display,
                                                    int16_t x,
                                                    int16_t y,
                                                    bool nodeAvailable,
                                                    const String &shortName,
                                                    const String &longName)
{
    if (!display) {
        return;
    }
    prepareListDisplay(display);
    const auto &search = HermesXTraceRouteUiModel::instance().searchState();
    const int16_t width = std::max<int16_t>(display->getWidth() - x, 1);
    HermesXTextLayout::drawMixedSingleLineBounded(
        display, x, y, width - 2, u8"搜尋結果", FONT_HEIGHT_SMALL);

    const int16_t contentX = x + 2;
    const int16_t contentW = std::max<int16_t>(width - 4, 1);
    const int16_t lineH = FONT_HEIGHT_SMALL + 1;
    int16_t contentY = y + FONT_HEIGHT_SMALL + 3;

    if (nodeAvailable) {
        const String shortNameLine = String("ShortName: ") + (shortName.length() > 0 ? shortName : String("--"));
        HermesXTextLayout::drawMixedSingleLineBounded(
            display, contentX, contentY, contentW, shortNameLine.c_str(), lineH);
        contentY += lineH;

        const String longNameLine = String("LongName: ") + (longName.length() > 0 ? longName : String("--"));
        const std::vector<String> longNameLines = HermesXTextLayout::buildMixedWrappedLines(
            display, longNameLine.c_str(), contentW, HermesX_zh::GLYPH_WIDTH);
        const int16_t buttonH = FONT_HEIGHT_SMALL + 4;
        const int16_t buttonY = y + display->getHeight() - buttonH - 2;
        const int16_t availableLines = std::max<int16_t>((buttonY - contentY - 2) / lineH, 1);
        const int16_t linesToDraw =
            std::min<int16_t>(static_cast<int16_t>(longNameLines.size()), availableLines);
        for (int16_t index = 0; index < linesToDraw; ++index) {
            HermesXTextLayout::drawMixedSingleLineBounded(
                display, contentX, contentY + index * lineH, contentW, longNameLines[index].c_str(), lineH);
        }

        const int16_t gap = 8;
        const int16_t buttonW = std::min<int16_t>((contentW - gap) / 2, 58);
        const int16_t buttonsW = buttonW * 2 + gap;
        const int16_t bindX = x + (width - buttonsW) / 2;
        const int16_t backX = bindX + buttonW + gap;
        display->drawRect(search.resultCursor == 0 ? bindX : backX, buttonY, buttonW, buttonH);

        const int16_t bindTextW = HermesX_zh::stringAdvance(u8"綁定", HermesX_zh::GLYPH_WIDTH, display);
        const int16_t backTextW = HermesX_zh::stringAdvance(u8"返回", HermesX_zh::GLYPH_WIDTH, display);
        HermesXTextLayout::drawMixedSingleLineBounded(
            display, bindX + (buttonW - bindTextW) / 2, buttonY + 1, bindTextW, u8"綁定", FONT_HEIGHT_SMALL + 2);
        HermesXTextLayout::drawMixedSingleLineBounded(
            display, backX + (buttonW - backTextW) / 2, buttonY + 1, backTextW, u8"返回", FONT_HEIGHT_SMALL + 2);
        return;
    }

    HermesXTextLayout::drawMixedSingleLineBounded(
        display, contentX, contentY, contentW, u8"找不到裝置", lineH);
    contentY += lineH;
    const String query = String("ShortName: ") + search.resultQuery;
    HermesXTextLayout::drawMixedSingleLineBounded(
        display, contentX, contentY, contentW, query.c_str(), lineH);

    const int16_t buttonW = 58;
    const int16_t buttonH = FONT_HEIGHT_SMALL + 4;
    const int16_t buttonX = x + (width - buttonW) / 2;
    const int16_t buttonY = y + display->getHeight() - buttonH - 2;
    display->drawRect(buttonX, buttonY, buttonW, buttonH);
    const int16_t backTextW = HermesX_zh::stringAdvance(u8"返回", HermesX_zh::GLYPH_WIDTH, display);
    HermesXTextLayout::drawMixedSingleLineBounded(
        display, buttonX + (buttonW - backTextW) / 2, buttonY + 1, backTextW, u8"返回", FONT_HEIGHT_SMALL + 2);
}

void HermesXTraceRouteUiRenderer::drawMenu(OLEDDisplay *display, int16_t x, int16_t y)
{
    if (!display) {
        return;
    }
    prepareListDisplay(display);
    const int16_t width = std::max<int16_t>(display->getWidth() - x, 1);
    drawTitle(display, x, y, width);

    const auto &navigation = HermesXTraceRouteUiModel::instance().navigationState();
    const char *items[] = {u8"返回", u8"綁定節點", "TraceRoute"};
    const int16_t rowH = FONT_HEIGHT_SMALL + 4;
    const int16_t listTop = y + FONT_HEIGHT_SMALL + 6;
    for (uint8_t index = 0; index < 3; ++index) {
        const int16_t rowY = listTop + index * rowH;
        if (navigation.menuCursor == index) {
            display->drawRect(x, rowY - 1, width - 2, rowH);
        }
        HermesXTextLayout::drawMixedSingleLineBounded(
            display, x + 2, rowY, width - 4, items[index], rowH);
    }
}

void HermesXTraceRouteUiRenderer::drawBindList(OLEDDisplay *display,
                                                int16_t x,
                                                int16_t y,
                                                const HermesXTraceRouteRowSource &rows)
{
    if (!display) {
        return;
    }
    prepareListDisplay(display);
    const int16_t width = std::max<int16_t>(display->getWidth() - x, 1);
    drawTitle(display, x, y, width);

    const auto &navigation = HermesXTraceRouteUiModel::instance().navigationState();
    const int16_t rowH = FONT_HEIGHT_SMALL + 3;
    const int16_t listTop = y + FONT_HEIGHT_SMALL + 2;
    int8_t visibleRows = (display->getHeight() - listTop - 1) / rowH;
    visibleRows = std::max<int8_t>(1, std::min<int8_t>(visibleRows, 3));
    const uint8_t totalRows = rows.count + 2;
    const uint8_t startCursor = navigation.bindCursor >= static_cast<uint8_t>(visibleRows)
                                    ? navigation.bindCursor - static_cast<uint8_t>(visibleRows) + 1
                                    : 0;

    for (int8_t row = 0; row < visibleRows; ++row) {
        const uint8_t cursorIndex = startCursor + row;
        if (cursorIndex >= totalRows) {
            break;
        }
        const int16_t rowY = listTop + row * rowH;
        if (cursorIndex == navigation.bindCursor) {
            display->drawRect(x, rowY - 1, width - 2, rowH);
        }
        if (cursorIndex == 0 || cursorIndex == 1) {
            HermesXTextLayout::drawMixedSingleLineBounded(
                display, x + 2, rowY, width - 4, cursorIndex == 0 ? u8"搜尋裝置" : u8"返回", rowH);
            continue;
        }
        String label;
        String status;
        if (rows.rowAt && rows.rowAt(rows.context, cursorIndex - 2, label, status)) {
            drawNodeRow(display, x, rowY, width, rowH, label, status);
        }
    }

    if (rows.count == 0 && visibleRows > 2) {
        HermesXTextLayout::drawMixedSingleLineBounded(
            display, x + 2, listTop + rowH * 2, width - 4, u8"沒有在線節點", rowH);
    }

    if (!navigation.confirmVisible) {
        return;
    }
#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
    const int16_t boxW = std::min<int16_t>(width - 12, 116);
    const int16_t boxH = 42;
    const int16_t boxX = x + (width - boxW) / 2;
    const int16_t boxY = y + (display->getHeight() - boxH) / 2;
    display->fillRect(boxX, boxY, boxW, boxH);
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
    display->drawRect(boxX, boxY, boxW, boxH);
    HermesXTextLayout::drawMixedSingleLineBounded(
        display, boxX + 6, boxY + 5, boxW - 12, u8"是否綁定？", FONT_HEIGHT_SMALL + 2);
    const int16_t optionY = boxY + 24;
    display->drawRect(navigation.confirmSelected == 0 ? boxX + 10 : boxX + boxW - 46,
                      optionY - 1, 36, FONT_HEIGHT_SMALL + 4);
    HermesXTextLayout::drawMixedSingleLineBounded(
        display, boxX + 20, optionY, 22, u8"是", FONT_HEIGHT_SMALL + 2);
    HermesXTextLayout::drawMixedSingleLineBounded(
        display, boxX + boxW - 36, optionY, 22, u8"否", FONT_HEIGHT_SMALL + 2);
}

void HermesXTraceRouteUiRenderer::drawBoundList(OLEDDisplay *display,
                                                 int16_t x,
                                                 int16_t y,
                                                 const HermesXTraceRouteRowSource &rows)
{
    if (!display) {
        return;
    }
    prepareListDisplay(display);
    const int16_t width = std::max<int16_t>(display->getWidth() - x, 1);
    drawTitle(display, x, y, width);

    const auto &navigation = HermesXTraceRouteUiModel::instance().navigationState();
    const int16_t rowH = FONT_HEIGHT_SMALL + 3;
    const int16_t listTop = y + FONT_HEIGHT_SMALL + 2;
    int8_t visibleRows = (display->getHeight() - listTop - 1) / rowH;
    visibleRows = std::max<int8_t>(1, std::min<int8_t>(visibleRows, 4));
    const uint8_t totalRows = rows.count + 1;
    const uint8_t startCursor = navigation.boundCursor >= static_cast<uint8_t>(visibleRows)
                                    ? navigation.boundCursor - static_cast<uint8_t>(visibleRows) + 1
                                    : 0;

    for (int8_t row = 0; row < visibleRows; ++row) {
        const uint8_t rowIndex = startCursor + row;
        if (rowIndex >= totalRows) {
            break;
        }
        const int16_t rowY = listTop + row * rowH;
        if (rowIndex == navigation.boundCursor) {
            display->drawRect(x, rowY - 1, width - 2, rowH);
        }
        if (rowIndex == 0) {
            HermesXTextLayout::drawMixedSingleLineBounded(
                display, x + 2, rowY, width - 4, u8"返回", rowH);
            continue;
        }
        String label;
        String status;
        if (rows.rowAt && rows.rowAt(rows.context, rowIndex - 1, label, status)) {
            drawNodeRow(display, x, rowY, width, rowH, label, status);
        }
    }

    if (rows.count == 0 && visibleRows > 1) {
        HermesXTextLayout::drawMixedSingleLineBounded(
            display, x + 2, listTop + rowH, width - 4, u8"尚未綁定節點", rowH);
    }
}

} // namespace graphics
