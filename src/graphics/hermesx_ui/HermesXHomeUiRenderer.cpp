#include "HermesXHomeUiRenderer.h"

#include "HermesXTextLayout.h"

#include "graphics/ScreenFonts.h"
#include "graphics/TFTDisplay.h"
#include "graphics/fonts/HermesX_zh/HermesX_CN12.h"
#include "graphics/fonts/PattanakarnClock32.h"
#include <algorithm>
#include <cstdio>
#include <vector>

namespace graphics
{
namespace
{

static_assert(HermesXHomeUiRenderer::NeonClockGlyphHeight == PattanakarnClock32::kGlyphHeight,
              "Home Neon clock geometry must match the Pattanakarn glyph height");

constexpr const char *HomeQuotes[] = {
    u8"別慌，有我在",
    u8"HermesX帥吧？",
    u8"狗哥爆肝中",
    u8"我其實是狗哥養的狗",
    u8"我叫小威",
    u8"你今天好嗎？",
    u8"祝你有個開心的一天",
    u8"壞心情退散！",
    u8"好玩吧？",
    u8"出發囉！！",
    u8"狗哥有點小餓",
};
constexpr uint8_t HomeQuoteCount = sizeof(HomeQuotes) / sizeof(HomeQuotes[0]);

uint8_t quoteIndex = 0;
bool quoteActive = false;

void addTftColorZone(OLEDDisplay *display,
                     int16_t x,
                     int16_t y,
                     int16_t width,
                     int16_t height,
                     uint16_t foreground,
                     uint16_t background)
{
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
    if (!display || width <= 0 || height <= 0) {
        return;
    }
    auto *tft = static_cast<TFTDisplay *>(display);
    tft->addColorPaletteZone(TFTDisplay::ColorZone{x, y, width, height, foreground, background});
#else
    (void)display;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)foreground;
    (void)background;
#endif
}

const char *currentQuote()
{
    if (!quoteActive) {
        HermesXHomeUiRenderer::startQuote();
    }
    return HomeQuotes[quoteIndex % HomeQuoteCount];
}

} // namespace

void HermesXHomeUiRenderer::resetQuote()
{
    quoteActive = false;
}

void HermesXHomeUiRenderer::startQuote()
{
    quoteIndex = static_cast<uint8_t>(random(HomeQuoteCount));
    quoteActive = true;
}

void HermesXHomeUiRenderer::drawQuote(OLEDDisplay *display,
                                      int16_t x,
                                      int16_t y,
                                      int16_t width,
                                      int16_t height)
{
    if (!display || width <= 0 || height <= 0) {
        return;
    }

    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);
    constexpr int16_t LineHeight = 13;
    const std::vector<String> lines = HermesXTextLayout::buildMixedWrappedLines(
        display, currentQuote(), width, HermesX_zh::GLYPH_WIDTH);
    if (lines.empty()) {
        return;
    }

    const size_t visibleLineCount = std::min<size_t>(2, lines.size());
    const int16_t totalHeight = static_cast<int16_t>(visibleLineCount * LineHeight);
    const int16_t firstY = y + std::max<int16_t>(0, (height - totalHeight) / 2);
    for (size_t lineIndex = 0; lineIndex < visibleLineCount; ++lineIndex) {
        const char *line = lines[lineIndex].c_str();
        const int16_t lineW = HermesX_zh::stringAdvance(line, HermesX_zh::GLYPH_WIDTH, display);
        const int16_t drawX = x + std::max<int16_t>(0, (width - lineW) / 2);
        HermesX_zh::drawMixedBounded(*display, drawX, firstY + static_cast<int16_t>(lineIndex * LineHeight),
                                     x + width - drawX, line, HermesX_zh::GLYPH_WIDTH, LineHeight, nullptr);
    }
}

void HermesXHomeUiRenderer::drawDog(OLEDDisplay *display,
                                    int16_t width,
                                    int16_t timeY,
                                    bool compactLayout,
                                    uint32_t nowMs)
{
    if (!display) {
        return;
    }

    constexpr int16_t AreaX = 10;
    const int16_t areaW = width - 20;
    if (areaW <= 24) {
        return;
    }

    const uint32_t phase = nowMs / 150U;
    const int16_t dogW = compactLayout ? 36 : 46;
    const int16_t maxTravel = std::max<int16_t>(0, areaW - dogW);
    int16_t stride = 0;
    bool facingRight = true;
    if (maxTravel > 0) {
        const int16_t cycle = maxTravel * 2;
        const int16_t offset = static_cast<int16_t>(phase % static_cast<uint32_t>(cycle > 0 ? cycle : 1));
        if (offset >= maxTravel) {
            stride = cycle - offset;
            facingRight = false;
        } else {
            stride = offset;
        }
    }

    const int16_t dogX = AreaX + stride;
    const int16_t dogY = timeY + 7;
    const int16_t bodyW = compactLayout ? 16 : 22;
    const int16_t bodyH = compactLayout ? 6 : 8;
    const int16_t neckW = compactLayout ? 4 : 6;
    const int16_t headW = compactLayout ? 10 : 12;
    const int16_t headH = compactLayout ? 7 : 9;
    const int16_t legH = compactLayout ? 7 : 9;
    const int16_t bodyY = dogY + 4;
    const int16_t bodyX = facingRight ? dogX + 8 : dogX + 12;
    const int16_t chestX = facingRight ? bodyX + bodyW - 1 : bodyX - neckW + 1;
    const int16_t chestY = bodyY + 1;
    const int16_t headX = facingRight ? bodyX + bodyW + neckW - 1 : dogX;
    const int16_t headY = dogY;
    const uint8_t gaitFrame = static_cast<uint8_t>(phase % 4U);
    const bool altStep = gaitFrame == 0U || gaitFrame == 2U;

    for (int16_t gx = AreaX; gx < (AreaX + areaW); gx += 4) {
        display->setPixel(gx, bodyY + bodyH + legH + 1);
    }

    display->fillRect(bodyX, bodyY, bodyW, bodyH);
    if (facingRight) {
        display->drawLine(bodyX, bodyY + 1, bodyX - 3, bodyY);
        display->drawLine(bodyX - 3, bodyY, bodyX - 6, bodyY - 2);
        display->drawLine(bodyX - 6, bodyY - 2, bodyX - 7, bodyY - 5);
    } else {
        const int16_t tailX = bodyX + bodyW - 1;
        display->drawLine(tailX, bodyY + 1, tailX + 3, bodyY);
        display->drawLine(tailX + 3, bodyY, tailX + 6, bodyY - 2);
        display->drawLine(tailX + 6, bodyY - 2, tailX + 7, bodyY - 5);
    }

    display->drawLine(bodyX + bodyW - 1, bodyY, chestX, chestY);
    display->drawLine(bodyX + bodyW - 1, bodyY + bodyH - 1, chestX, chestY + bodyH);
    display->fillRect(facingRight ? bodyX + bodyW - 1 : bodyX - neckW + 1, chestY, neckW, bodyH);
    display->fillRect(headX, headY, headW, headH);

    if (facingRight) {
        display->drawLine(headX + 2, headY, headX + 2, headY - 3);
        display->drawLine(headX + 5, headY, headX + 6, headY - 4);
        display->drawLine(headX + headW - 1, headY + 3, headX + headW + 3, headY + 4);
        display->setPixel(headX + 5, headY + 3);
    } else {
        display->drawLine(headX + headW - 3, headY, headX + headW - 3, headY - 3);
        display->drawLine(headX + headW - 6, headY, headX + headW - 7, headY - 4);
        display->drawLine(headX, headY + 3, headX - 4, headY + 4);
        display->setPixel(headX + 2, headY + 3);
    }

    const int16_t legY = bodyY + bodyH;
    const int16_t frontLegX = facingRight ? bodyX + bodyW + 1 : bodyX - 1;
    const int16_t frontLeg2X = facingRight ? bodyX + bodyW - 2 : bodyX + 2;
    const int16_t rearLegX = bodyX + 2;
    const int16_t rearLeg2X = bodyX + bodyW - 2;
    display->drawLine(frontLegX, legY, frontLegX, legY + legH - (altStep ? 0 : 1));
    display->drawLine(frontLeg2X, legY, frontLeg2X, legY + legH - (altStep ? 1 : 0));
    display->drawLine(rearLegX, legY, rearLegX, legY + legH - (altStep ? 1 : 0));
    display->drawLine(rearLeg2X, legY, rearLeg2X, legY + legH - (altStep ? 0 : 1));

    display->setPixel(frontLegX + (facingRight ? 1 : -1), legY + legH - (altStep ? 0 : 1));
    display->setPixel(rearLeg2X + 1, legY + legH - (altStep ? 0 : 1));
}

void HermesXHomeUiRenderer::drawHorizontalBattery(
    OLEDDisplay *display, int16_t x, int16_t y, int16_t width, int16_t height, uint8_t percent)
{
    if (!display || width < 20 || height < 12) {
        return;
    }

    percent = std::min<uint8_t>(100, percent);
    const int16_t capW = std::max<int16_t>(3, width / 10);
    const int16_t capH = std::max<int16_t>(4, height / 2);
    const int16_t bodyW = width - capW - 1;
    if (bodyW < 12) {
        return;
    }

    const int16_t capX = x + bodyW;
    const int16_t capY = y + (height - capH) / 2;
    display->drawRect(x, y, bodyW, height);
    if (bodyW > 16 && height > 12) {
        display->drawRect(x + 1, y + 1, bodyW - 2, height - 2);
    }
    display->drawRect(capX, capY, capW, capH);

    const int16_t inset = (bodyW >= 26 && height >= 16) ? 3 : 2;
    const int16_t innerX = x + inset;
    const int16_t innerY = y + inset;
    const int16_t innerW = bodyW - inset * 2;
    const int16_t innerH = height - inset * 2;
    if (innerW <= 0 || innerH <= 0) {
        return;
    }

    int16_t fillW = (innerW * percent) / 100;
    if (percent > 0 && fillW < 1) {
        fillW = 1;
    }
    if (fillW > 0) {
        addTftColorZone(
            display, innerX, innerY, fillW, innerH, TFTDisplay::rgb565(0xB5, 0xED, 0x00), 0x0000);
        display->fillRect(innerX, innerY, fillW, innerH);
    }
}

void HermesXHomeUiRenderer::drawStatus(OLEDDisplay *display, const HermesXHomeStatusView &view)
{
    if (!display) {
        return;
    }

    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    const bool compactLayout = width < 200 || height < 120;
    const int16_t contentX = compactLayout ? 4 : 8;
    const int16_t contentW = width - 8;
    const auto drawBoldLine = [&](int16_t x, int16_t y, const char *text) {
        const int16_t maxWidth = contentW > 0 ? contentW : width - 4;
        display->drawStringMaxWidth(x, y, maxWidth, text ? text : "");
        display->drawStringMaxWidth(x + 1, y, maxWidth > 1 ? maxWidth - 1 : maxWidth, text ? text : "");
    };

    display->setFont(FONT_SMALL);
    drawBoldLine(contentX, compactLayout ? 49 : 62, view.date);

    display->setFont(compactLayout ? FONT_SMALL : FONT_MEDIUM);
    if (display->getStringWidth(view.role ? view.role : "") > width / 2) {
        display->setFont(FONT_SMALL);
    }
    drawBoldLine(contentX, compactLayout ? 62 : height - FONT_HEIGHT_MEDIUM, view.role);

    if (view.hasBattery) {
        const int16_t iconW = compactLayout ? 34 : 48;
        const int16_t iconH = compactLayout ? 16 : 20;
        const int16_t iconX = ((width - iconW) / 2) - (compactLayout ? 6 : 12);
        const int16_t iconY = height - iconH - (compactLayout ? 3 : 6);
        drawHorizontalBattery(display, iconX, iconY, iconW, iconH, view.batteryPercent);
    }

    const bool hasSatelliteFix = view.satelliteCount > 0;
    const int16_t badgeW = compactLayout ? 24 : 32;
    const int16_t badgeH = compactLayout ? 18 : 24;
    const int16_t badgeX = std::max<int16_t>(2, width - badgeW - (compactLayout ? 12 : 18) - 20);
    const int16_t badgeY = height - badgeH - (compactLayout ? 10 : 16);
    addTftColorZone(display, badgeX, badgeY, badgeW, badgeH,
                    hasSatelliteFix ? TFTDisplay::rgb565(0xB5, 0xED, 0x00)
                                    : TFTDisplay::rgb565(0xD9, 0x2D, 0x20),
                    0x0000);
    display->setColor(BLACK);
    display->fillRect(badgeX - 1, badgeY - 1, badgeW + 2, badgeH + 2);
    display->setColor(WHITE);
    display->fillRect(badgeX, badgeY, badgeW, badgeH);
    display->setColor(BLACK);
    display->setFont(compactLayout ? FONT_SMALL : FONT_MEDIUM);
    char satelliteText[4];
    snprintf(satelliteText, sizeof(satelliteText), "%u", static_cast<unsigned>(view.satelliteCount));
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->drawString(badgeX + badgeW / 2, badgeY + (compactLayout ? 1 : 2), satelliteText);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setColor(WHITE);
}

int16_t HermesXHomeUiRenderer::neonClockSlotWidth(size_t slotIndex)
{
    return (slotIndex == 2 || slotIndex == 5) ? 7 : NeonClockGlyphMaxWidth;
}

int16_t HermesXHomeUiRenderer::neonClockFrameWidth()
{
    int16_t width = 0;
    for (size_t slotIndex = 0; slotIndex < NeonClockSlotCount; ++slotIndex) {
        width += neonClockSlotWidth(slotIndex);
    }
    return width;
}

bool HermesXHomeUiRenderer::makeNeonClockLayout(int16_t displayWidth,
                                                int16_t originY,
                                                HermesXHomeNeonClockLayout &layout)
{
    layout = HermesXHomeNeonClockLayout{};
    if (displayWidth <= 0) {
        return false;
    }

    const int16_t frameWidth = neonClockFrameWidth();
    if (frameWidth <= 0 || frameWidth > (displayWidth - 4)) {
        return false;
    }

    const int16_t timeY = originY + 8;
    layout.frameX = (displayWidth - frameWidth) / 2;
    layout.regionX = layout.frameX - NeonClockMargin;
    layout.regionY = timeY - NeonClockMargin;
    layout.regionWidth = frameWidth + (NeonClockMargin * 2);
    layout.regionHeight = NeonClockGlyphHeight + (NeonClockMargin * 2);
    return layout.regionWidth > 0 && layout.regionHeight > 0 && layout.regionWidth <= NeonClockMaxRegionWidth &&
           layout.regionHeight <= NeonClockMaxRegionHeight;
}

uint16_t HermesXHomeUiRenderer::neonClockLayerColor(uint8_t value)
{
    switch (value) {
    case 1:
        return TFTDisplay::rgb565(0x03, 0x09, 0x1F);
    case 2:
        return TFTDisplay::rgb565(0x04, 0x12, 0x3D);
    case 3:
        return TFTDisplay::rgb565(0x06, 0x24, 0x72);
    case 4:
        return TFTDisplay::rgb565(0x0C, 0x54, 0xC8);
    case 5:
        return TFTDisplay::rgb565(0x36, 0xC5, 0xFF);
    case 6:
        return TFTDisplay::rgb565(0xAC, 0xF6, 0xFF);
    case 7:
        return TFTDisplay::rgb565(0xFF, 0xFF, 0xFF);
    default:
        return TFTDisplay::rgb565(0x00, 0x00, 0x00);
    }
}

} // namespace graphics
