#include "HermesXEmergencyConfirmUiRenderer.h"

#include "graphics/ScreenFonts.h"
#include "graphics/fonts/HermesX_zh/HermesX_CN12.h"
#include <algorithm>
#include <cstdio>

namespace graphics
{

void HermesXEmergencyConfirmUiRenderer::draw(OLEDDisplay *display, const HermesXEmergencyConfirmUiState &state)
{
    if (!display || !state.visible) {
        return;
    }
    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    const int16_t boxX = 3;
    const int16_t boxY = 2;
    const int16_t boxWidth = width - 6;
    const int16_t boxHeight = height - 4;
    const int16_t titleBarHeight = 12;
    const int16_t optionHeight = 10;
    const int16_t optionY = boxY + boxHeight - optionHeight - 2;
    const int16_t optionWidth = boxWidth - 8;
    const int16_t exitX = boxX + 4;

#if defined(USE_EINK)
    const auto dialogBackground = EINK_WHITE;
    const auto dialogForeground = EINK_BLACK;
#else
    const auto dialogBackground = OLEDDISPLAY_COLOR::WHITE;
    const auto dialogForeground = OLEDDISPLAY_COLOR::BLACK;
#endif
    display->setColor(dialogBackground);
    display->fillRect(boxX, boxY, boxWidth, boxHeight);
    display->setColor(dialogForeground);
    display->drawRect(boxX, boxY, boxWidth, boxHeight);
    display->fillRect(boxX + 1, boxY + 1, boxWidth - 2, titleBarHeight);
    display->setColor(dialogBackground);

    const char *title = u8"緊急模式警告";
    const int titleWidth = HermesX_zh::stringAdvance(title, HermesX_zh::GLYPH_WIDTH, display);
    const int16_t titleX = std::max<int16_t>(boxX + 2, boxX + (boxWidth - titleWidth) / 2);
    HermesX_zh::drawMixedBounded(*display, titleX, boxY + 1, boxWidth - 4, title, HermesX_zh::GLYPH_WIDTH,
                                 FONT_HEIGHT_SMALL, nullptr);
    display->setColor(dialogForeground);

    const int advance = HermesX_zh::GLYPH_WIDTH - 1;
    const int16_t bodyX = boxX + 5;
    const int16_t bodyWidth = boxWidth - 10;
    const int16_t bodyY = boxY + titleBarHeight + 2;
    const int16_t statusY = optionY - FONT_HEIGHT_SMALL - 2;
    const int16_t bodyLineHeight =
        std::max<int16_t>(FONT_HEIGHT_SMALL - 1, std::min<int16_t>(12, (statusY - bodyY - 1) / 2));
    HermesX_zh::drawMixedBounded(*display, bodyX, bodyY, bodyWidth, u8"注意：即將進入EM模式", advance,
                                 bodyLineHeight, nullptr);
    HermesX_zh::drawMixedBounded(*display, bodyX, bodyY + bodyLineHeight, bodyWidth, u8"按下可退出取消", advance,
                                 bodyLineHeight, nullptr);

    display->drawRect(exitX, optionY, optionWidth, optionHeight);
    const char *exitLabel = u8"退出";
    const int exitLabelWidth = HermesX_zh::stringAdvance(exitLabel, HermesX_zh::GLYPH_WIDTH, display);
    const int16_t exitLabelX = std::max<int16_t>(exitX + 1, exitX + (optionWidth - exitLabelWidth) / 2);
    HermesX_zh::drawMixedBounded(*display, exitLabelX, optionY + 1, optionWidth - 2, exitLabel,
                                 HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);

    char countdownLine[48];
    snprintf(countdownLine, sizeof(countdownLine), u8"%lus 後自動進入",
             static_cast<unsigned long>(state.remainingSeconds));
    HermesX_zh::drawMixedBounded(*display, bodyX, statusY, bodyWidth, countdownLine, advance, FONT_HEIGHT_SMALL,
                                 nullptr);
    display->setColor(WHITE);
}

} // namespace graphics
