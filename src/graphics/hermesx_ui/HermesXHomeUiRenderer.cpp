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
    u8"一切都會好起來的，對吧",
    u8"明天會更好的，是吧？",
    u8"不要放棄！",
    u8"美好的明天快來臨了",
    u8"別氣餒",
};
constexpr uint8_t HomeQuoteCount = sizeof(HomeQuotes) / sizeof(HomeQuotes[0]);

constexpr int16_t SittingDogWidth = 24;
constexpr int16_t SittingDogHeight = 24;
constexpr uint8_t SittingDogFrameCount = 4;
constexpr const char *SittingDogFrames[SittingDogFrameCount][SittingDogHeight] = {
    {
        "..............OO...OO...", "..............TS...HT...", "..............LTO.OHTO..",
        "..............LTOOOTTO..", ".............OTLHHHHHO..", ".............OTLHOOHHT..",
        ".............OTTHOOHHOOO", ".............OHHHHHHSSOO", ".............OHHHOOOSSSO",
        "............OOHHHOOSTL..", "............OOOHHHOHHT..", "............OSOHHHHOOO..",
        "...........OOOSLHHHHHO..", "..........OSSSSLLSHHHO..", "..........OSSSSTLS.HHO..",
        ".........OOOSHHHHS..H...", ".........OSSSHHHHH.HHG..", "........OSSSSTTHHTHTTG..",
        "........OSHHOSHHHTHTHG..", "TSOOO...OLHHHOOHHSSHHG..", ".OHLSS..HHHHHOOHHTSHHG..",
        "..OSTO..HHHHOOOHHOOHHG..", ".....OOOHHHHHHOHHHOHHHO.", ".........OOOOOOOOOOOOOO.",
    },
    {
        "..............OO...OO...", "..............TS...HT...", "..............LTO.OHTO..",
        "..............LTOOOTTO..", ".............OTLHHHHHO..", ".............OTLHOOHHT..",
        ".............OTTHOOHHOOO", ".............OHHHHHHSSOO", ".............OHHHOOOSSSO",
        "............OOHHHOOSTL..", "............OOOHHHOHHT..", "............OSOHHHHOOO..",
        "...........OOOSLHHHHHO..", "..........OSSSSLLSHHHO..", "..........OSSSSTLS.HHO..",
        ".........OOOSHHHHS..H...", ".........OSSSHHHHH.HHG..", "........OSSSSTTHHTHTTG..",
        "........OSHHOSHHHTHTHG..", "..TSOO..OLHHHOOHHSSHHG..", "..OHLSO.HHHHHOOHHTSHHG..",
        "...OST..HHHHOOOHHOOHHG..", ".....OOOHHHHHHOHHHOHHHO.", ".........OOOOOOOOOOOOOO.",
    },
    {
        "..............OO...OO...", "..............TS...HT...", "..............LTO.OHTO..",
        "..............LTOOOTTO..", ".............OTLHHHHHO..", ".............OTLHOOHHT..",
        ".............OTTHOOHHOOO", ".............OHHHHHHSSOO", ".............OHHHOOOSSSO",
        "............OOHHHOOSTL..", "............OOOHHHOHHT..", "............OSOHHHHOOO..",
        "...........OOOSLHHHHHO..", "..........OSSSSLLSHHHO..", "..........OSSSSTLS.HHO..",
        ".........OOOSHHHHS..H...", ".........OSSSHHHHH.HHG..", "........OSSSSTTHHTHTTG..",
        "........OSHHOSHHHTHTHG..", "....TSOOOLHHHOOHHSSHHG..", "...OHLSOHHHHHOOHHTSHHG..",
        "....OST.HHHHOOOHHOOHHG..", ".....OOOHHHHHHOHHHOHHHO.", ".........OOOOOOOOOOOOOO.",
    },
    {
        "..............OO...OO...", "..............TS...HT...", "..............LTO.OHTO..",
        "..............LTOOOTTO..", ".............OTLHHHHHO..", ".............OTLHOOHHT..",
        ".............OTTHOOHHOOO", ".............OHHHHHHSSOO", ".............OHHHOOOSSSO",
        "............OOHHHOOSTL..", "............OOOHHHOHHT..", "............OSOHHHHOOO..",
        "...........OOOSLHHHHHO..", "..........OSSSSLLSHHHO..", "..........OSSSSTLS.HHO..",
        ".........OOOSHHHHS..H...", ".........OSSSHHHHH.HHG..", "........OSSSSTTHHTHTTG..",
        "........OSHHOSHHHTHTHG..", "..TSOO..OLHHHOOHHSSHHG..", "..OHLSO.HHHHHOOHHTSHHG..",
        "...OST..HHHHOOOHHOOHHG..", ".....OOOHHHHHHOHHHOHHHO.", ".........OOOOOOOOOOOOOO.",
    },
};

constexpr int16_t HermesOneMascotWidth = 16;
constexpr int16_t HermesOneMascotHeight = 16;
constexpr const char *HermesOneMascot[HermesOneMascotHeight] = {
    "................",
    "..##........##..",
    ".#..#......#..#.",
    ".#...######...#.",
    "..##........##..",
    "...#........#...",
    "...#..#..#..#...",
    "...#....#...#...",
    "...#...##...#...",
    "....#......#....",
    "....########....",
    "....#......#....",
    ".##.#......#....",
    "#..##......#....",
    ".##.#.##.###....",
    "....##..##......",
};

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

inline __attribute__((always_inline)) void drawNotificationActions(OLEDDisplay *display,
                             int16_t x,
                             int16_t y,
                             int16_t width,
                             int16_t height,
                             uint8_t selectedAction)
{
    constexpr int16_t gap = 3;
    const int16_t actionW = (width - gap) / 2;
    const char *labels[] = {u8"查看", u8"略過"};
    for (uint8_t action = 0; action < 2U; ++action) {
        const int16_t actionX = x + action * (actionW + gap);
        if (selectedAction == action) {
            display->fillRect(actionX, y, actionW, height);
#if defined(USE_EINK)
            display->setColor(EINK_WHITE);
#else
            display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
        } else {
            display->drawRect(actionX, y, actionW, height);
        }
        const int16_t labelW = HermesX_zh::stringAdvance(labels[action], HermesX_zh::GLYPH_WIDTH, display);
        HermesX_zh::drawMixed(*display, actionX + std::max<int16_t>(1, (actionW - labelW) / 2), y + 1,
                              labels[action], HermesX_zh::GLYPH_WIDTH, 13, nullptr);
#if defined(USE_EINK)
        display->setColor(EINK_BLACK);
#else
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
    }
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

void HermesXHomeUiRenderer::drawMonochromeSittingDog(
    OLEDDisplay *display, int16_t x, int16_t y, uint8_t scale, uint8_t frameIndex, bool outlineOnly, bool mirror)
{
    if (!display || scale == 0) {
        return;
    }

    if (outlineOnly) {
        // Dedicated chunky 1-bit mascot for HermesOne. It is intentionally a
        // low-resolution 16x16 drawing enlarged to 32x32, matching the strong
        // pixel language of classic virtual-pet LCDs.
        const uint8_t phase = frameIndex % 60U;
        const int16_t bobY = (phase >= 15U && phase < 45U) ? -1 : 0;
        const bool blink = phase >= 54U;
        for (int16_t sy = 0; sy < HermesOneMascotHeight; ++sy) {
            for (int16_t sx = 0; sx < HermesOneMascotWidth; ++sx) {
                bool visible = HermesOneMascot[sy][sx] == '#';
                const bool eye = sy == 6 && (sx == 6 || sx == 9);
                if (blink && eye) {
                    visible = false;
                }
                if (visible) {
                    const int16_t drawX = mirror ? HermesOneMascotWidth - 1 - sx : sx;
                    display->fillRect(x + drawX * scale, y + bobY + sy * scale, scale, scale);
                }
            }
        }
        if (blink) {
            // The body deliberately uses chunky 2x pixels, but eyelids need
            // native OLED resolution. A one-physical-pixel stroke reads as a
            // closed eye instead of another enlarged block.
            const int16_t eyelidY = y + bobY + 6 * scale + scale / 2;
            const int16_t leftEyeCenterX = x + 6 * scale + scale / 2;
            const int16_t rightEyeCenterX = x + 9 * scale + scale / 2;
            display->drawHorizontalLine(leftEyeCenterX - 1, eyelidY, 3);
            display->drawHorizontalLine(rightEyeCenterX - 1, eyelidY, 3);
        }
        return;
    }

    const char *const *sprite = SittingDogFrames[frameIndex % SittingDogFrameCount];
    for (int16_t sy = 0; sy < SittingDogHeight; ++sy) {
        for (int16_t sx = 0; sx < SittingDogWidth; ++sx) {
            const char token = sprite[sy][sx];
            if (token == '.') {
                continue;
            }
            bool visible = token == 'O' || token == 'L' || token == 'W';
            if (token == 'H' || token == 'T') {
                visible = ((sx + sy) % 3) != 0;
            } else if (token == 'S') {
                visible = ((sx + sy) & 1) == 0;
            } else if (token == 'G') {
                visible = (sx & 1) == 0;
            }
            if (visible) {
                display->fillRect(x + sx * scale, y + sy * scale, scale, scale);
            }
        }
    }
}

void HermesXHomeUiRenderer::drawHermesOneThoughtBubble(
    OLEDDisplay *display, int16_t x, int16_t y, const char *symbol, bool stomachIcon)
{
    if (!display) {
        return;
    }

    constexpr int16_t bubbleW = 24;
    constexpr int16_t bubbleH = 16;

    // Soft, uneven cloud silhouette.  Deliberately avoid drawRect(): on a
    // 1-bit display a regular box reads like a status label, not a thought.
    display->drawHorizontalLine(x + 7, y, 5);
    display->drawLine(x + 6, y + 1, x + 4, y + 3);
    display->drawLine(x + 4, y + 3, x + 2, y + 3);
    display->drawLine(x + 2, y + 3, x, y + 6);
    display->drawVerticalLine(x, y + 6, 4);
    display->drawLine(x, y + 10, x + 3, y + 13);
    display->drawHorizontalLine(x + 3, y + 13, 4);
    display->drawLine(x + 7, y + 13, x + 9, y + 15);
    display->drawHorizontalLine(x + 9, y + 15, 5);
    display->drawLine(x + 14, y + 15, x + 16, y + 13);
    display->drawHorizontalLine(x + 16, y + 13, 4);
    display->drawLine(x + 20, y + 13, x + 23, y + 10);
    display->drawVerticalLine(x + 23, y + 6, 4);
    display->drawLine(x + 23, y + 6, x + 21, y + 4);
    display->drawLine(x + 21, y + 4, x + 20, y + 2);
    display->drawLine(x + 20, y + 2, x + 17, y + 1);
    display->drawLine(x + 17, y + 1, x + 15, y + 2);
    display->drawLine(x + 15, y + 2, x + 12, y);

    // Two round thought beads lead down-left from the head-right cloud toward
    // Xiaowei's cheek.
    display->drawCircle(x - 2, y + bubbleH - 1, 1);
    display->setPixel(x - 5, y + bubbleH + 2);

    if (stomachIcon) {
        const int16_t cx = x + bubbleW / 2;
        const int16_t cy = y + bubbleH / 2;
        // Compact chibi stomach: narrow oesophagus, round belly, curled outlet.
        display->drawLine(cx - 1, cy - 5, cx - 1, cy - 2);
        display->drawLine(cx - 1, cy - 2, cx - 4, cy);
        display->drawLine(cx - 4, cy, cx - 3, cy + 4);
        display->drawLine(cx - 3, cy + 4, cx + 2, cy + 5);
        display->drawLine(cx + 2, cy + 5, cx + 5, cy + 2);
        display->drawLine(cx + 5, cy + 2, cx + 3, cy - 1);
        display->drawLine(cx + 3, cy - 1, cx + 1, cy - 2);
        display->setPixel(cx + 4, cy + 4);
        return;
    }

    display->setFont(FONT_SMALL_LOCAL);
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->drawString(x + bubbleW / 2, y + 1, symbol ? symbol : "");
    display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void HermesXHomeUiRenderer::drawHermesOneTombstone(OLEDDisplay *display, int16_t x, int16_t y)
{
    if (!display) {
        return;
    }

    // A rounded-at-the-top 1-bit gravestone occupying the mascot's 32x32 area.
    display->drawHorizontalLine(x + 7, y, 16);
    display->drawLine(x + 7, y, x + 3, y + 5);
    display->drawLine(x + 23, y, x + 27, y + 5);
    display->drawVerticalLine(x + 3, y + 5, 22);
    display->drawVerticalLine(x + 27, y + 5, 22);
    display->drawHorizontalLine(x + 3, y + 26, 25);
    display->drawHorizontalLine(x, y + 29, 31);
    display->drawHorizontalLine(x + 5, y + 31, 21);
    display->setFont(FONT_SMALL_LOCAL);
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->drawString(x + 15, y + 8, "RIP");
    display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void HermesXHomeUiRenderer::drawHermesOnePetDialog(
    OLEDDisplay *display, int16_t width, int16_t height, bool revive, uint8_t selectedAction)
{
    if (!display || width <= 0 || height <= 0) {
        return;
    }

    display->setColor(OLEDDISPLAY_COLOR::BLACK);
    display->fillRect(0, 0, width, height);
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    display->drawRect(1, 1, width - 2, height - 2);

    if (revive) {
        drawHermesOneTombstone(display, 5, 8);
    } else {
        const uint8_t frame = static_cast<uint8_t>((static_cast<uint64_t>(millis()) * 60U / 1000U) % 60U);
        drawMonochromeSittingDog(display, 3, 9, 2, frame, true);
    }

    const int16_t textX = 39;
    const int16_t textW = width - textX - 3;
    display->setFont(FONT_SMALL_LOCAL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    const char *question = revive ? u8"是否將她復活？" : u8"餵她吃飯？";
    const std::vector<String> lines =
        HermesXTextLayout::buildMixedWrappedLines(display, question, textW, HermesX_zh::GLYPH_WIDTH);
    const size_t lineCount = std::min<size_t>(2, lines.size());
    for (size_t i = 0; i < lineCount; ++i) {
        HermesX_zh::drawMixedBounded(*display, textX, 5 + static_cast<int16_t>(i * 13), textW,
                                     lines[i].c_str(), HermesX_zh::GLYPH_WIDTH, 13, nullptr);
    }

    const uint8_t actionCount = revive ? 1 : 2;
    const int16_t gap = 3;
    const int16_t actionW = (textW - gap * (actionCount - 1)) / actionCount;
    const int16_t actionY = height - 18;
    const char *labels[] = {u8"是", u8"否"};
    for (uint8_t action = 0; action < actionCount; ++action) {
        const int16_t actionX = textX + action * (actionW + gap);
        if (selectedAction == action) {
            display->fillRect(actionX, actionY, actionW, 15);
            display->setColor(OLEDDISPLAY_COLOR::BLACK);
        } else {
            display->drawRect(actionX, actionY, actionW, 15);
        }
        const int16_t labelW = HermesX_zh::stringAdvance(labels[action], HermesX_zh::GLYPH_WIDTH, display);
        HermesX_zh::drawMixed(*display, actionX + std::max<int16_t>(1, (actionW - labelW) / 2), actionY + 1,
                              labels[action], HermesX_zh::GLYPH_WIDTH, 13, nullptr);
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
    }
    display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void HermesXHomeUiRenderer::drawSittingDogNotification(OLEDDisplay *display,
                                                       int16_t width,
                                                       int16_t height,
                                                       const char *message,
                                                       uint32_t nowMs,
                                                       bool showActions,
                                                       uint8_t selectedAction)
{
    if (!display || !message || width <= 0 || height <= 0) {
        return;
    }
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
    auto *tft = static_cast<TFTDisplay *>(display);
    tft->clearColorPaletteZones();
    tft->setColorPaletteDefaults(0xFFFF, 0x0000);
#endif
#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
    display->fillRect(0, 0, width, height);
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif

#if defined(HERMESX_NRF52840_PROMICRO)
    const int16_t scale = 2;
    const bool useHermesOneMascot = true;
    const int16_t mascotWidth = HermesOneMascotWidth;
    const int16_t mascotHeight = HermesOneMascotHeight;
#else
    const int16_t scale = (height >= 72 && width >= 120) ? 2 : 1;
    const bool useHermesOneMascot = false;
    const int16_t mascotWidth = SittingDogWidth;
    const int16_t mascotHeight = SittingDogHeight;
#endif
    const int16_t dogW = mascotWidth * scale;
    const int16_t dogH = mascotHeight * scale;
    const int16_t dogX = std::max<int16_t>(2, (width / 3 - dogW) / 2);
    const int16_t dogY = std::max<int16_t>(1, (height - dogH) / 2);
    const uint8_t frameIndex = useHermesOneMascot
                                   ? static_cast<uint8_t>((static_cast<uint64_t>(nowMs) * 60U / 1000U) % 60U)
                                   : static_cast<uint8_t>((nowMs / 160U) % SittingDogFrameCount);
    drawMonochromeSittingDog(
        display, dogX, dogY, static_cast<uint8_t>(scale), frameIndex, useHermesOneMascot);

    const int16_t textX = std::max<int16_t>(dogX + dogW + 4, width / 3);
    const int16_t textW = std::max<int16_t>(1, width - textX - 3);
    display->setFont(FONT_SMALL_LOCAL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    constexpr int16_t lineHeight = 13;
    const std::vector<String> lines = HermesXTextLayout::buildMixedWrappedLines(
        display, message, textW, HermesX_zh::GLYPH_WIDTH);
    const size_t visibleLines = std::min<size_t>(2, lines.size());
    const int16_t actionsHeight = showActions ? 17 : 0;
    const int16_t messageAreaHeight = height - actionsHeight;
    const int16_t textY = std::max<int16_t>(1, (messageAreaHeight - static_cast<int16_t>(visibleLines * lineHeight)) / 2);
    for (size_t lineIndex = 0; lineIndex < visibleLines; ++lineIndex) {
        const int16_t lineW = HermesX_zh::stringAdvance(lines[lineIndex].c_str(), HermesX_zh::GLYPH_WIDTH, display);
        const int16_t lineX = textX + std::max<int16_t>(0, (textW - lineW) / 2);
        HermesX_zh::drawMixedBounded(*display, lineX, textY + static_cast<int16_t>(lineIndex * lineHeight),
                                     textX + textW - lineX, lines[lineIndex].c_str(), HermesX_zh::GLYPH_WIDTH,
                                     lineHeight, nullptr);
    }

    if (showActions) {
        drawNotificationActions(display, textX, height - 16, textW, 15, selectedAction);
    }
}

void HermesXHomeUiRenderer::drawHermesOneParcelNotification(OLEDDisplay *display,
                                                             int16_t width,
                                                             int16_t height,
                                                             HermesXParcelAnimationMode mode,
                                                             uint32_t elapsedMs,
                                                             bool showActions,
                                                             uint8_t selectedAction,
                                                             const char *message)
{
    if (!display || width <= 0 || height <= 0) {
        return;
    }

    display->setColor(OLEDDISPLAY_COLOR::BLACK);
    display->fillRect(0, 0, width, height);
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    const int16_t groundY = showActions ? height - 17 : height - 5;
    display->drawHorizontalLine(2, groundY, width - 4);

    auto drawBox = [&](int16_t x, int16_t y) {
        display->drawRect(x, y, 18, 14);
        display->drawHorizontalLine(x + 2, y + 4, 14);
        display->drawVerticalLine(x + 8, y, 14);
    };
    auto drawTippedBox = [&](int16_t x, int16_t y) {
        display->drawLine(x, y + 8, x + 14, y);
        display->drawLine(x + 14, y, x + 21, y + 12);
        display->drawLine(x + 21, y + 12, x + 7, y + 20);
        display->drawLine(x + 7, y + 20, x, y + 8);
        display->drawLine(x + 7, y + 4, x + 14, y + 16);
        display->drawLine(x + 14, y, x + 18, y - 6);
    };
    auto drawFallenDog = [&](int16_t x, int16_t y) {
        for (int16_t sy = 0; sy < HermesOneMascotHeight; ++sy) {
            for (int16_t sx = 0; sx < HermesOneMascotWidth; ++sx) {
                if (HermesOneMascot[sy][sx] != '#') {
                    continue;
                }
                const int16_t rotatedX = sy;
                const int16_t rotatedY = HermesOneMascotWidth - 1 - sx;
                display->fillRect(x + rotatedX * 2, y + rotatedY * 2, 2, 2);
            }
        }
    };

    const uint32_t travelMs = std::min<uint32_t>(1800U, elapsedMs);
    const uint8_t dogFrame = static_cast<uint8_t>((elapsedMs * 60U / 1000U) % 60U);
    const int16_t stepY = elapsedMs < 1800U && ((elapsedMs / 120U) & 1U) ? -1 : 0;
    if (mode == HermesXParcelAnimationMode::Outgoing) {
        const int16_t dogX = 2 + static_cast<int16_t>((70U * travelMs) / 1800U);
        const int16_t dogY = groundY - 33 + stepY;
        const int16_t boxX = dogX + 29;
        const int16_t boxY = dogY + 14;
        drawMonochromeSittingDog(display, dogX, dogY, 2, dogFrame, true);
        drawBox(boxX, boxY);
    } else if (mode == HermesXParcelAnimationMode::Incoming) {
        const int16_t groupX = 76 - static_cast<int16_t>((74U * travelMs) / 1800U);
        const int16_t dogX = groupX + 18;
        const int16_t dogY = groundY - 33 + stepY;
        const int16_t boxY = dogY + 14;
        drawBox(groupX, boxY);
        drawMonochromeSittingDog(display, dogX, dogY, 2, dogFrame, true, true);
        if (elapsedMs >= 1800U && message && message[0]) {
            display->setFont(FONT_SMALL_LOCAL);
            display->drawRect(54, 13, width - 55, 18);
            HermesXTextLayout::drawMixedSingleLineBounded(display, 57, 16, width - 61, message, 13);
        }
    } else if (elapsedMs < 500U) {
        const int16_t dogX = 27;
        const int16_t dogY = groundY - 33 + stepY;
        const int16_t boxX = dogX + 29;
        const int16_t boxY = dogY + 14;
        drawMonochromeSittingDog(display, dogX, dogY, 2, dogFrame, true);
        drawBox(boxX, boxY);
    } else {
        drawTippedBox(57, groundY - 23);
        if (elapsedMs >= 850U) {
            drawFallenDog(16, groundY - 32);
        } else {
            drawMonochromeSittingDog(display, 25, groundY - 33, 2, dogFrame, true);
        }

        static constexpr int8_t spillDx[] = {10, 18, 26, 34, 42, 48, 53, 56};
        static constexpr int8_t spillDy[] = {3, 14, 7, 19, 11, 22, 16, 20};
        const uint8_t spillStep =
            elapsedMs <= 650U ? 0U : static_cast<uint8_t>(std::min<uint32_t>(4U, (elapsedMs - 650U) / 225U));
        for (size_t i = 0; i < sizeof(spillDx); ++i) {
            const int16_t px = 68 + static_cast<int16_t>((spillDx[i] * spillStep) / 4U);
            const int16_t py = groundY - 24 + static_cast<int16_t>((spillDy[i] * spillStep) / 4U);
            const int16_t size = (i % 3U) == 0U ? 2 : 1;
            display->fillRect(px, py, size, size);
        }
    }

    if (showActions) {
        display->setFont(FONT_SMALL_LOCAL);
        drawNotificationActions(display, 2, height - 15, width - 4, 14, selectedAction);
    }
    display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void HermesXHomeUiRenderer::renderDirectSittingDogNotification(TFTDisplay *display,
                                                               int16_t width,
                                                               int16_t height,
                                                               uint32_t nowMs)
{
    if (!display || width <= 0 || height <= 0) {
        return;
    }

    const int16_t scale = (height >= 72 && width >= 120) ? 2 : 1;
    const int16_t dogW = SittingDogWidth * scale;
    const int16_t dogH = SittingDogHeight * scale;
    if (dogW > width || dogH > height) {
        return;
    }
    const int16_t dogX = std::max<int16_t>(2, (width / 3 - dogW) / 2);
    const int16_t dogY = std::max<int16_t>(1, (height - dogH) / 2);
    const uint16_t background = TFTDisplay::rgb565(0x00, 0x00, 0x00);
    const uint16_t outline = TFTDisplay::rgb565(0x18, 0x18, 0x1C);
    const uint16_t saddle = TFTDisplay::rgb565(0x4A, 0x49, 0x46);
    const uint16_t tan = TFTDisplay::rgb565(0xA7, 0x74, 0x55);
    const uint16_t tanHighlight = TFTDisplay::rgb565(0xD4, 0xA5, 0x80);
    const uint16_t chest = TFTDisplay::rgb565(0xF8, 0xF7, 0xF8);
    const uint16_t tongue = TFTDisplay::rgb565(0xBC, 0x55, 0x51);
    const uint16_t ground = TFTDisplay::rgb565(0xC6, 0xB8, 0xBB);
    auto colorFor = [&](char token) -> uint16_t {
        switch (token) {
        case 'O':
            return outline;
        case 'H':
            return tanHighlight;
        case 'S':
            return saddle;
        case 'T':
            return tan;
        case 'L':
            return tongue;
        case 'W':
            return chest;
        case 'G':
            return ground;
        default:
            return background;
        }
    };

    display->fillRect565(dogX, dogY, dogW, dogH, background);
    const uint8_t frameIndex = static_cast<uint8_t>((nowMs / 160U) % SittingDogFrameCount);
    const char *const *sprite = SittingDogFrames[frameIndex];
    for (int16_t sy = 0; sy < SittingDogHeight; ++sy) {
        for (int16_t sx = 0; sx < SittingDogWidth; ++sx) {
            const char token = sprite[sy][sx];
            if (token == '.') {
                continue;
            }
            display->fillRect565(
                dogX + sx * scale, dogY + sy * scale, scale, scale, colorFor(token));
        }
    }
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
