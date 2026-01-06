#pragma once

#include <OLEDDisplay.h>
#include <string>

#include "graphics/ScreenFonts.h" // for FONT_HEIGHT_* macros
#include "graphics/fonts/HermesX_zh/HermesX_CN12.h"

namespace graphics
{

// Track last alignment set through helper
extern OLEDDISPLAY_TEXT_ALIGNMENT lastTextAlignment;

// =======================
// Shared UI Helpers
// =======================

#define textZeroLine 0
// Consistent Line Spacing - this is standard for all display and the fall-back spacing
#define textFirstLine (FONT_HEIGHT_SMALL - 1)
#define textSecondLine (textFirstLine + (FONT_HEIGHT_SMALL - 5))
#define textThirdLine (textSecondLine + (FONT_HEIGHT_SMALL - 5))
#define textFourthLine (textThirdLine + (FONT_HEIGHT_SMALL - 5))
#define textFifthLine (textFourthLine + (FONT_HEIGHT_SMALL - 5))
#define textSixthLine (textFifthLine + (FONT_HEIGHT_SMALL - 5))

// Consistent Line Spacing for devices like T114 and TEcho/ThinkNode M1 of devices
#define textFirstLine_medium (FONT_HEIGHT_SMALL + 1)
#define textSecondLine_medium (textFirstLine_medium + FONT_HEIGHT_SMALL)
#define textThirdLine_medium (textSecondLine_medium + FONT_HEIGHT_SMALL)
#define textFourthLine_medium (textThirdLine_medium + FONT_HEIGHT_SMALL)
#define textFifthLine_medium (textFourthLine_medium + FONT_HEIGHT_SMALL)
#define textSixthLine_medium (textFifthLine_medium + FONT_HEIGHT_SMALL)

// Consistent Line Spacing for devices like VisionMaster T190
#define textFirstLine_large (FONT_HEIGHT_SMALL + 1)
#define textSecondLine_large (textFirstLine_large + (FONT_HEIGHT_SMALL + 5))
#define textThirdLine_large (textSecondLine_large + (FONT_HEIGHT_SMALL + 5))
#define textFourthLine_large (textThirdLine_large + (FONT_HEIGHT_SMALL + 5))
#define textFifthLine_large (textFourthLine_large + (FONT_HEIGHT_SMALL + 5))
#define textSixthLine_large (textFifthLine_large + (FONT_HEIGHT_SMALL + 5))

// Quick screen access
#define SCREEN_WIDTH display->getWidth()
#define SCREEN_HEIGHT display->getHeight()

// Shared state (declare inside namespace)
extern bool hasUnreadMessage;
extern bool isMuted;
extern bool isHighResolution;
void determineResolution(int16_t screenheight, int16_t screenwidth);

// Rounded highlight (used for inverted headers)
void drawRoundedHighlight(OLEDDisplay *display, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r);

// Shared battery/time/mail header
void drawCommonHeader(OLEDDisplay *display, int16_t x, int16_t y, const char *titleStr = "", bool force_no_invert = false,
                      bool show_date = false);

// Shared battery/time/mail header
void drawCommonFooter(OLEDDisplay *display, int16_t x, int16_t y);

const int *getTextPositions(OLEDDisplay *display);

bool isAllowedPunctuation(char c);

std::string sanitizeString(const std::string &input);

// Translation table (zh-TW)
const char *translateZh(const char *text);

// HermesX: centralized mixed-text helpers (CN12 for Hanzi, current font for ASCII)
int stringWidthMixed(OLEDDisplay *display, const char *text, int advanceX = HermesX_zh::GLYPH_WIDTH);
void drawStringMixed(OLEDDisplay *display, int16_t x, int16_t y, const char *text, int lineHeight = FONT_HEIGHT_SMALL);
void drawStringMixedCentered(OLEDDisplay *display, int16_t centerX, int16_t y, const char *text,
                             int lineHeight = FONT_HEIGHT_SMALL);
void drawStringMixedBounded(OLEDDisplay *display, int16_t x, int16_t y, int16_t maxWidth, const char *text,
                            int lineHeight = FONT_HEIGHT_SMALL);

} // namespace graphics
