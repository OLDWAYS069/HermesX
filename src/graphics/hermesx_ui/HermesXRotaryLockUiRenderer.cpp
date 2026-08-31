#include "HermesXRotaryLockUiRenderer.h"

#include "graphics/ScreenFonts.h"
#include "graphics/fonts/HermesX_zh/HermesX_CN12.h"
#include <algorithm>

namespace graphics
{

void HermesXRotaryLockUiRenderer::draw(OLEDDisplay *display, const HermesXRotaryLockUiState &state)
{
    if (!display || !state.visible) {
        return;
    }

    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    const int16_t boxX = 7;
    const int16_t boxY = 10;
    const int16_t boxWidth = width - 14;
    const int16_t boxHeight = height - 20;
    const int16_t titleBarHeight = 13;
    const int16_t optionHeight = 13;
    const int16_t optionY = boxY + boxHeight - optionHeight - 4;
    const int16_t optionWidth = (boxWidth - 15) / 2;
    const int16_t unlockX = boxX + 5;
    const int16_t lockX = unlockX + optionWidth + 5;

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

    const char *title = u8"旋鈕鎖定";
    const int titleWidth = HermesX_zh::stringAdvance(title, HermesX_zh::GLYPH_WIDTH, display);
    const int16_t titleX = std::max<int16_t>(boxX + 2, boxX + (boxWidth - titleWidth) / 2);
    HermesX_zh::drawMixedBounded(*display, titleX, boxY + 2, boxWidth - 4, title, HermesX_zh::GLYPH_WIDTH,
                                 FONT_HEIGHT_SMALL, nullptr);
    display->setColor(dialogForeground);

    const auto drawOption = [&](int16_t optionX, const char *label, bool selected) {
        if (selected) {
            display->setColor(dialogForeground);
            display->fillRect(optionX, optionY, optionWidth, optionHeight);
            display->setColor(dialogBackground);
        } else {
            display->setColor(dialogForeground);
            display->drawRect(optionX, optionY, optionWidth, optionHeight);
        }

        const int textWidth = HermesX_zh::stringAdvance(label, HermesX_zh::GLYPH_WIDTH, display);
        const int16_t textX = std::max<int16_t>(optionX + 1, optionX + (optionWidth - textWidth) / 2);
        HermesX_zh::drawMixedBounded(*display, textX, optionY + 2, optionWidth - 2, label,
                                     HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
        display->setColor(dialogForeground);
    };

    drawOption(unlockX, u8"解鎖", !state.selectedLocked);
    drawOption(lockX, u8"鎖定", state.selectedLocked);
    display->setColor(WHITE);
}

} // namespace graphics
