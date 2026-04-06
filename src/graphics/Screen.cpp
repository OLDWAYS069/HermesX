/*

SSD1306 - Screen module

Copyright (C) 2018 by Xose Perez <xose dot perez at gmail dot com>


This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/
#include "Screen.h"
#include "PowerMon.h"
#include "Throttle.h"
#include "buzz/buzz.h"
#include "configuration.h"
#include "HermesXTestFlags.h"
#if HAS_SCREEN
#ifndef HERMESX_CIV_DISABLE_EMAC
#define HERMESX_CIV_DISABLE_EMAC 0
#endif
#include <OLEDDisplay.h>

#include "DisplayFormatters.h"
#include "FSCommon.h"
#if !MESHTASTIC_EXCLUDE_GPS
#include "GPS.h"
#endif
#if !MESHTASTIC_EXCLUDE_MQTT
#include "mqtt/MQTT.h"
#endif
#include "ButtonThread.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "error.h"
#include "gps/GeoCoord.h"
#include "modules/HermesEmUiModule.h"
#include "gps/RTC.h"
#include "graphics/ScreenFonts.h"
#include "graphics/fonts/PattanakarnClock32.h"
// --- HermesX Remove TFT fast-path START
#include "graphics/fonts/HermesX_zh/HermesX_CN12.h"
#include "HeapDebug.h"
#include <cstdlib>
#include <inttypes.h>
#include <math.h>
#include <time.h>
// --- HermesX Remove TFT fast-path END
#include "graphics/images.h"
#include "input/ScanAndSelect.h"
#include "input/TouchScreenImpl1.h"
#include "Led.h"
#include "main.h"
#include "mesh-pb-constants.h"
#include "mesh/Channels.h"
#include "mesh/Default.h"
#include "mesh/RadioInterface.h"
#include "mesh/generated/meshtastic/deviceonly.pb.h"
#include "mesh/generated/meshtastic/apponly.pb.h"
#include "meshUtils.h"
#include "pb_encode.h"
#include "modules/AdminModule.h"
#include "modules/CannedMessageModule.h"
#include "modules/ExternalNotificationModule.h"
#include "modules/HermesXBatteryProtection.h"
#include "modules/LighthouseModule.h"
#include "modules/HermesXInterfaceModule.h"
#include "modules/TextMessageModule.h"
#include "modules/WaypointModule.h"
#if !MESHTASTIC_EXCLUDE_HERMESX && defined(HERMESX_GUARD_POWER_ANIMATIONS)
#include "modules/HermesXPowerGuard.h"
#endif
#include "sleep.h"
#include "sleep_hooks.h"
#include "SPILock.h"
#include "target_specific.h"

#ifdef ARDUINO_ARCH_ESP32
#include <qrcode.h>
#endif

#if HAS_WIFI && !defined(ARCH_PORTDUINO)
#include "mesh/wifi/WiFiAPClient.h"
#endif

#ifdef ARCH_ESP32
#include "esp_task_wdt.h"
#include "modules/StoreForwardModule.h"
#endif

#if ARCH_PORTDUINO
#include "modules/StoreForwardModule.h"
#include "platform/portduino/PortduinoGlue.h"
#endif

using namespace meshtastic; /** @todo remove */

namespace graphics
{

// This means the *visible* area (sh1106 can address 132, but shows 128 for example)
#define IDLE_FRAMERATE 1 // in fps
#define DIRECT_HOME_CLOCK_FRAMERATE 4 // in fps

// DEBUG
#define NUM_EXTRA_FRAMES 3 // text message and debug frame
// if defined a pixel will blink to show redraws
// #define SHOW_REDRAWS

// A text message frame + debug frame + all the node infos
FrameCallback *normalFrames;
static uint32_t targetFramerate = IDLE_FRAMERATE;
static uint32_t fastUntilMs = 0;
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
struct HermesFastSetupTftPaletteState {
    const OLEDDisplay *display = nullptr;
    int16_t width = 0;
    int16_t height = 0;
    int itemCount = -1;
    int selectedIndex = -1;
    int listOffset = -1;
    bool showToast = false;
    bool valid = false;
};
static HermesFastSetupTftPaletteState hermesFastSetupTftPaletteState;
static bool hermesFastSetupTftPaletteActive = false;
#endif

uint32_t logo_timeout = 3000; // 3 seconds for EACH logo

uint32_t hours_in_month = 730;

// This image definition is here instead of images.h because it's modified dynamically by the drawBattery function
uint8_t imgBattery[16] = {0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xE7, 0x3C};

// Threshold values for the GPS lock accuracy bar display
uint32_t dopThresholds[5] = {2000, 1000, 500, 200, 100};

// At some point, we're going to ask all of the modules if they would like to display a screen frame
// we'll need to hold onto pointers for the modules that can draw a frame.
std::vector<MeshModule *> moduleFrames;

// Stores the last 4 of our hardware ID, to make finding the device for pairing easier
static char ourId[5];

// vector where symbols (string) are displayed in bottom corner of display.
std::vector<std::string> functionSymbol;
// string displayed in bottom right corner of display. Created from elements in functionSymbol vector
std::string functionSymbolString = "";

#if HAS_GPS
// GeoCoord object for the screen
GeoCoord geoCoord;
#endif

#ifdef SHOW_REDRAWS
static bool heartbeat = false;
#endif

// Quick access to screen dimensions from static drawing functions
// DEPRECATED. To-do: move static functions inside Screen class
#define SCREEN_WIDTH display->getWidth()
#define SCREEN_HEIGHT display->getHeight()

#include "graphics/ScreenFonts.h"
#include <Throttle.h>

#define getStringCenteredX(s) ((SCREEN_WIDTH - display->getStringWidth(s)) / 2)

static bool supportsDirectTftClockRendering(OLEDDisplay *display);

// Check if the display can render a string (detect special chars; emoji)
static bool haveGlyphs(const char *str)
{
#if defined(OLED_PL) || defined(OLED_UA) || defined(OLED_RU) || defined(OLED_CS)
    // Don't want to make any assumptions about custom language support
    return true;
#endif

    // Check each character with the lookup function for the OLED library
    // We're not really meant to use this directly..
    bool have = true;
    for (uint16_t i = 0; i < strlen(str); i++) {
        uint8_t result = Screen::customFontTableLookup((uint8_t)str[i]);
        // If font doesn't support a character, it is substituted for 聶
        if (result == 191 && (uint8_t)str[i] != 191) {
            have = false;
            break;
        }
    }

    LOG_DEBUG("haveGlyphs=%d", have);
    return have;
}

static void drawMixedRightAligned(OLEDDisplay *display, int16_t rightX, int16_t y, const char *text, int lineHeight)
{
    if (!display || !text)
        return;

    display->setTextAlignment(TEXT_ALIGN_LEFT);

    const char *lineStart = text;
    int lineIndex = 0;
    while (true) {
        const char *newline = strchr(lineStart, '\n');
        const size_t length = newline ? static_cast<size_t>(newline - lineStart) : strlen(lineStart);
        String lineString(lineStart, length);
        const int lineWidth =
            HermesX_zh::stringAdvance(lineString.c_str(), HermesX_zh::GLYPH_WIDTH, display);
        const int16_t drawX = rightX - lineWidth;
        if (screen)
            screen->drawMixed(display, drawX, y + lineIndex * lineHeight, lineString.c_str(),
                              HermesX_zh::GLYPH_WIDTH, lineHeight);
        else
            HermesX_zh::drawMixed(*display, drawX, y + lineIndex * lineHeight, lineString.c_str(),
                                  HermesX_zh::GLYPH_WIDTH, lineHeight, nullptr);
        if (!newline)
            break;
        lineStart = newline + 1;
        ++lineIndex;
    }
}

/**
 * Draw the icon with extra info printed around the corners
 */
static void drawIconScreen(const char *upperMsg, OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    // draw an xbm image.
    // Please note that everything that should be transitioned
    // needs to be drawn relative to x and y

    // draw centered icon left to right and centered above the one line of app text
#if defined(M5STACK_UNITC6L)
    display->drawXbm(x + (SCREEN_WIDTH - 50) / 2, y + (SCREEN_HEIGHT - 28) / 2, icon_width, icon_height, icon_bits);
    display->setFont(FONT_MEDIUM);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);
    // Draw region in upper left (centered for this layout)
    if (upperMsg) {
        int msgWidth = display->getStringWidth(upperMsg);
        int msgX = x + (SCREEN_WIDTH - msgWidth) / 2;
        int msgY = y;
        display->drawString(msgX, msgY, upperMsg);
    }
    // Draw version and short name in bottom middle
    char buf[25];
    snprintf(buf, sizeof(buf), "%s   %s", xstr(APP_VERSION_SHORT), haveGlyphs(owner.short_name) ? owner.short_name : "");
    display->drawString(x + getStringCenteredX(buf), y + SCREEN_HEIGHT - FONT_HEIGHT_MEDIUM, buf);
    screen->forceDisplay();

    display->setTextAlignment(TEXT_ALIGN_LEFT); // Restore left align, just to be kind to any other unsuspecting code
#else
    display->drawXbm(x + (SCREEN_WIDTH - icon_width) / 2, y + (SCREEN_HEIGHT - FONT_HEIGHT_MEDIUM - icon_height) / 2 + 2,
                     icon_width, icon_height, icon_bits);

    display->setFont(FONT_MEDIUM);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    const char *title = "meshtastic.org";
    display->drawString(x + getStringCenteredX(title), y + SCREEN_HEIGHT - FONT_HEIGHT_MEDIUM, title);
    display->setFont(FONT_SMALL);

    // Draw region in upper left
    if (upperMsg)
        display->drawString(x + 0, y + 0, upperMsg);

    // Draw version and short name in upper right
    char buf[25];
    snprintf(buf, sizeof(buf), "%s\n%s", xstr(APP_VERSION_SHORT), haveGlyphs(owner.short_name) ? owner.short_name : "");

    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    display->drawString(x + SCREEN_WIDTH, y + 0, buf);
    screen->forceDisplay();

    display->setTextAlignment(TEXT_ALIGN_LEFT); // Restore left align, just to be kind to any other unsuspecting code
#endif
}

#ifdef USERPREFS_OEM_TEXT

static void drawOEMIconScreen(const char *upperMsg, OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    static const uint8_t xbm[] = USERPREFS_OEM_IMAGE_DATA;
    display->drawXbm(x + (SCREEN_WIDTH - USERPREFS_OEM_IMAGE_WIDTH) / 2,
                     y + (SCREEN_HEIGHT - FONT_HEIGHT_MEDIUM - USERPREFS_OEM_IMAGE_HEIGHT) / 2 + 2, USERPREFS_OEM_IMAGE_WIDTH,
                     USERPREFS_OEM_IMAGE_HEIGHT, xbm);

    switch (USERPREFS_OEM_FONT_SIZE) {
    case 0:
        display->setFont(FONT_SMALL);
        break;
    case 2:
        display->setFont(FONT_LARGE);
        break;
    default:
        display->setFont(FONT_MEDIUM);
        break;
    }

    display->setTextAlignment(TEXT_ALIGN_LEFT);
    const char *title = USERPREFS_OEM_TEXT;
    display->drawString(x + getStringCenteredX(title), y + SCREEN_HEIGHT - FONT_HEIGHT_MEDIUM, title);
    display->setFont(FONT_SMALL);

    // Draw region in upper left
    if (upperMsg)
        display->drawString(x + 0, y + 0, upperMsg);

    // Draw version and shortname in upper right
    char buf[25];
    snprintf(buf, sizeof(buf), "%s\n%s", xstr(APP_VERSION_SHORT), haveGlyphs(owner.short_name) ? owner.short_name : "");

    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    display->drawString(x + SCREEN_WIDTH, y + 0, buf);
    screen->forceDisplay();

    display->setTextAlignment(TEXT_ALIGN_LEFT); // Restore left align, just to be kind to any other unsuspecting code
}

static void drawOEMBootScreen(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    // Draw region in upper left
    const char *region = myRegion ? myRegion->name : NULL;
    drawOEMIconScreen(region, display, state, x, y);
}

#endif

void Screen::drawFrameText(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y, const char *message)
{
    uint16_t x_offset = display->width() / 2;
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(FONT_MEDIUM);
    display->drawString(x_offset + x, 26 + y, message);
}

// Used on boot when a certificate is being created
static void drawSSLScreen(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(FONT_SMALL);
    display->drawString(64 + x, y, "Creating SSL certificate");

#ifdef ARCH_ESP32
    yield();
    esp_task_wdt_reset();
#endif

    display->setFont(FONT_SMALL);
    if ((millis() / 1000) % 2) {
        display->drawString(64 + x, FONT_HEIGHT_SMALL + y + 2, "Please wait . . .");
    } else {
        display->drawString(64 + x, FONT_HEIGHT_SMALL + y + 2, "Please wait . .  ");
    }
}

// Used when booting without a region set
static void drawWelcomeScreen(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->drawString(64 + x, y, "//\\ E S H T /\\ S T / C");
    display->drawString(64 + x, y + FONT_HEIGHT_SMALL, getDeviceName());
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    if ((millis() / 10000) % 2) {
        display->drawString(x, y + FONT_HEIGHT_SMALL * 2 - 3, "Set the region using the");
        display->drawString(x, y + FONT_HEIGHT_SMALL * 3 - 3, "Meshtastic Android, iOS,");
        display->drawString(x, y + FONT_HEIGHT_SMALL * 4 - 3, "Web or CLI clients.");
    } else {
        display->drawString(x, y + FONT_HEIGHT_SMALL * 2 - 3, "Visit HermesX");
        display->drawString(x, y + FONT_HEIGHT_SMALL * 3 - 3, "for more information.");
        display->drawString(x, y + FONT_HEIGHT_SMALL * 4 - 3, "");
    }

#ifdef ARCH_ESP32
    yield();
    esp_task_wdt_reset();
#endif
}

// draw overlay in bottom right corner of screen to show when notifications are muted or modifier key is active
static void drawFunctionOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{
    // LOG_DEBUG("Draw function overlay");
    if (functionSymbol.begin() != functionSymbol.end()) {
        char buf[64];
        display->setFont(FONT_SMALL);
        snprintf(buf, sizeof(buf), "%s", functionSymbolString.c_str());
        // --- HermesX Remove TFT fast-path START
        const int width = HermesX_zh::stringAdvance(buf, HermesX_zh::GLYPH_WIDTH, display);
        if (screen)
            screen->drawMixed(display, SCREEN_WIDTH - width, SCREEN_HEIGHT - FONT_HEIGHT_SMALL, buf);
        else
            HermesX_zh::drawMixed(*display, SCREEN_WIDTH - width, SCREEN_HEIGHT - FONT_HEIGHT_SMALL, buf);
        // --- HermesX Remove TFT fast-path END
    }
}

static void drawHermesXEmUiOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{
#if defined(HERMESX_TEST_DISABLE_HERMES_OVERLAYS)
    (void)display;
    (void)state;
    return;
#else
    if (hermesXEmUiModule) {
        hermesXEmUiModule->drawOverlay(display, state);
    }
#endif
}

static void drawHermesXMenuFooterOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{
#if defined(HERMESX_TEST_DISABLE_HERMES_OVERLAYS)
    (void)display;
    (void)state;
    return;
#else
    if (!display || !state || !screen) {
        return;
    }
    if (supportsDirectTftClockRendering(display)) {
        // Compact direct-TFT pages (Home/GPS poster) are full-frame visuals; skip footer shortcut overlay
        // to avoid persistent top-right artifacts between transitions.
        return;
    }
    if (!screen->shouldShowHermesXMenuFooter(state->currentFrame)) {
        return;
    }

    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    const bool compactOverlay = (width < 180 || height < 100);

    if (compactOverlay) {
        // Small screens: keep the return shortcut visible but avoid covering bottom content.
        const int16_t boxW = 12;
        const int16_t boxH = 10;
        const int16_t boxX = width - boxW - 2;
        const int16_t boxY = 1;

#if defined(USE_EINK)
        display->setColor(EINK_WHITE);
        display->fillRect(boxX, boxY, boxW, boxH);
        display->setColor(EINK_BLACK);
#else
        display->setColor(OLEDDISPLAY_COLOR::BLACK);
        display->fillRect(boxX, boxY, boxW, boxH);
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
        display->drawRect(boxX, boxY, boxW, boxH);

        const int16_t cx = boxX + boxW / 2;
        const int16_t cy = boxY + boxH / 2 + 1;
        display->drawLine(cx - 3, cy, cx, cy - 2);
        display->drawLine(cx, cy - 2, cx + 3, cy);
        display->drawRect(cx - 2, cy, 4, 3);
        display->setPixel(cx, cy + 1);
        return;
    }

    const int16_t footerH = FONT_HEIGHT_SMALL + 2;
    const int16_t y = height - footerH;
    int16_t boxW = footerH + 10;
    if (boxW < 20) {
        boxW = 20;
    }
    if (boxW > width - 4) {
        boxW = width - 4;
    }
    const int16_t boxX = (width - boxW) / 2;

#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
    display->fillRect(boxX, y, boxW, footerH);
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
    display->fillRect(boxX, y, boxW, footerH);
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
    display->drawRect(boxX, y, boxW, footerH);

    // Footer shortcut uses the same "Home" symbol style as the action menu tile.
    const int16_t cx = boxX + boxW / 2;
    const int16_t cy = y + footerH / 2 + 1;
    display->drawLine(cx - 5, cy + 1, cx, cy - 3);
    display->drawLine(cx, cy - 3, cx + 5, cy + 1);
    display->drawRect(cx - 4, cy + 1, 8, 5);
    display->drawRect(cx - 1, cy + 3, 2, 3);
#endif
}

#ifdef USE_EINK
/// Used on eink displays while in deep sleep
static void drawDeepSleepScreen(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{

    // Next frame should use full-refresh, and block while running, else device will sleep before async callback
    EINK_ADD_FRAMEFLAG(display, COSMETIC);
    EINK_ADD_FRAMEFLAG(display, BLOCKING);

    LOG_DEBUG("Draw deep sleep screen");

    // Display displayStr on the screen
    drawIconScreen("Sleeping", display, state, x, y);
}

/// Used on eink displays when screen updates are paused
static void drawScreensaverOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{
    LOG_DEBUG("Draw screensaver overlay");

    EINK_ADD_FRAMEFLAG(display, COSMETIC); // Take the opportunity for a full-refresh

    // Config
    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    const char *pauseText = "Screen Paused";
    const char *idText = owner.short_name;
    const bool useId = haveGlyphs(idText); // This bool is used to hide the idText box if we can't render the short name
    constexpr uint16_t padding = 5;
    constexpr uint8_t dividerGap = 1;
    constexpr uint8_t imprecision = 5; // How far the box origins can drift from center. Combat burn-in.

    // Dimensions
    const uint16_t idTextWidth = display->getStringWidth(idText, strlen(idText), true); // "true": handle utf8 chars
    const uint16_t pauseTextWidth = display->getStringWidth(pauseText, strlen(pauseText));
    const uint16_t boxWidth = padding + (useId ? idTextWidth + padding + padding : 0) + pauseTextWidth + padding;
    const uint16_t boxHeight = padding + FONT_HEIGHT_SMALL + padding;

    // Position
    const int16_t boxLeft = (display->width() / 2) - (boxWidth / 2) + random(-imprecision, imprecision + 1);
    // const int16_t boxRight = boxLeft + boxWidth - 1;
    const int16_t boxTop = (display->height() / 2) - (boxHeight / 2 + random(-imprecision, imprecision + 1));
    const int16_t boxBottom = boxTop + boxHeight - 1;
    const int16_t idTextLeft = boxLeft + padding;
    const int16_t idTextTop = boxTop + padding;
    const int16_t pauseTextLeft = boxLeft + (useId ? padding + idTextWidth + padding : 0) + padding;
    const int16_t pauseTextTop = boxTop + padding;
    const int16_t dividerX = boxLeft + padding + idTextWidth + padding;
    const int16_t dividerTop = boxTop + 1 + dividerGap;
    const int16_t dividerBottom = boxBottom - 1 - dividerGap;

    // Draw: box
    display->setColor(EINK_WHITE);
    display->fillRect(boxLeft - 1, boxTop - 1, boxWidth + 2, boxHeight + 2); // Clear a slightly oversized area for the box
    display->setColor(EINK_BLACK);
    display->drawRect(boxLeft, boxTop, boxWidth, boxHeight);

    // Draw: Text
    if (useId)
        display->drawString(idTextLeft, idTextTop, idText);
    display->drawString(pauseTextLeft, pauseTextTop, pauseText);
    display->drawString(pauseTextLeft + 1, pauseTextTop, pauseText); // Faux bold

    // Draw: divider
    if (useId)
        display->drawLine(dividerX, dividerTop, dividerX, dividerBottom);
}
#endif

static void drawModuleFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    uint8_t module_frame;
    // there's a little but in the UI transition code
    // where it invokes the function at the correct offset
    // in the array of "drawScreen" functions; however,
    // the passed-state doesn't quite reflect the "current"
    // screen, so we have to detect it.
    if (state->frameState == IN_TRANSITION && state->transitionFrameRelationship == TransitionRelationship_INCOMING) {
        // if we're transitioning from the end of the frame list back around to the first
        // frame, then we want this to be `0`
        module_frame = state->transitionFrameTarget;
    } else {
        // otherwise, just display the module frame that's aligned with the current frame
        module_frame = state->currentFrame;
        // LOG_DEBUG("Screen is not in transition.  Frame: %d", module_frame);
    }
    // LOG_DEBUG("Draw Module Frame %d", module_frame);
    MeshModule &pi = *moduleFrames.at(module_frame);
    pi.drawFrame(display, state, x, y);
}

static void drawFrameFirmware(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(FONT_MEDIUM);
    // --- HermesX Remove TFT fast-path START
    if (screen)
        screen->drawMixed(display, 64 + x, y, "Updating");
    else
        HermesX_zh::drawMixed(*display, 64 + x, y, "Updating");
    // --- HermesX Remove TFT fast-path END
    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    // --- HermesX Remove TFT fast-path START
    if (screen)
        HermesX_zh::drawMixedBounded(*display, 0 + x, 2 + y + FONT_HEIGHT_SMALL * 2, display->getWidth(),
                                     "Please be patient and do not power off.", 12, FONT_HEIGHT_SMALL, nullptr);
    else
        HermesX_zh::drawMixedBounded(*display, 0 + x, 2 + y + FONT_HEIGHT_SMALL * 2, display->getWidth(),
                                     "Please be patient and do not power off.", 12, FONT_HEIGHT_SMALL, nullptr);
    // --- HermesX Remove TFT fast-path END
}

/// Draw the last text message we received
static void drawCriticalFaultFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_MEDIUM);

    char tempBuf[24];
    snprintf(tempBuf, sizeof(tempBuf), "Critical fault #%d", error_code);
    display->drawString(0 + x, 0 + y, tempBuf);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);
    display->drawString(0 + x, FONT_HEIGHT_MEDIUM + y, "For help, please visit \nmeshtastic.org");
}

// Ignore messages originating from phone (from the current node 0x0) unless range test or store and forward module are enabled
static bool shouldDrawMessage(const meshtastic_MeshPacket *packet)
{
    return packet->from != 0 && !moduleConfig.store_forward.enabled;
}

// Draw power bars or a charging indicator on an image of a battery, determined by battery charge voltage or percentage.
static void drawBattery(OLEDDisplay *display, int16_t x, int16_t y, uint8_t *imgBuffer, const PowerStatus *powerStatus)
{
    static const uint8_t powerBar[3] = {0x81, 0xBD, 0xBD};
    static const uint8_t lightning[8] = {0xA1, 0xA1, 0xA5, 0xAD, 0xB5, 0xA5, 0x85, 0x85};
    // Clear the bar area on the battery image
    for (int i = 1; i < 14; i++) {
        imgBuffer[i] = 0x81;
    }
    // If charging, draw a charging indicator
    if (powerStatus->getIsCharging()) {
        memcpy(imgBuffer + 3, lightning, 8);
        // If not charging, Draw power bars
    } else {
        for (int i = 0; i < 4; i++) {
            if (powerStatus->getBatteryChargePercent() >= 25 * i)
                memcpy(imgBuffer + 1 + (i * 3), powerBar, 3);
        }
    }
    display->drawFastImage(x, y, 16, 8, imgBuffer);
}

static uint8_t estimateBatteryPercentFromVoltageMv(int batteryVoltageMv)
{
    // Simple single-cell Li-ion estimate: 3.30V = empty, 4.20V = full.
    constexpr int kBatteryEmptyMv = 3300;
    constexpr int kBatteryFullMv = 4200;

    if (batteryVoltageMv <= kBatteryEmptyMv) {
        return 0;
    }
    if (batteryVoltageMv >= kBatteryFullMv) {
        return 100;
    }

    const int scaled = ((batteryVoltageMv - kBatteryEmptyMv) * 100) / (kBatteryFullMv - kBatteryEmptyMv);
    if (scaled < 0) {
        return 0;
    }
    if (scaled > 100) {
        return 100;
    }
    return static_cast<uint8_t>(scaled);
}

struct HermesXBatteryCache {
    bool valid = false;
    int voltageMv = 0;
    uint8_t percent = 0;
};
static HermesXBatteryCache gHermesXBatteryCache;
static bool supportsDirectTftClockRendering(OLEDDisplay *display);
static int16_t measurePattanakarnClockText(const char *text);
static bool isStealthModeActive();
static uint16_t mapDirectGpsWarmLayerColor(uint8_t value);
static bool shouldShowHermesXHomeFrame();
static bool shouldShowHermesXGpsFrame();
struct DirectDrawClipRect {
    bool enabled = false;
    int16_t minX = 0;
    int16_t maxX = -1;
    int16_t minY = 0;
    int16_t maxY = -1;
};
static bool makeDirectDrawClipRect(int16_t x,
                                   int16_t y,
                                   int16_t w,
                                   int16_t h,
                                   int16_t displayW,
                                   int16_t displayH,
                                   DirectDrawClipRect &outClip);
static void directFillRect565Clipped(TFTDisplay *tft,
                                     int16_t displayW,
                                     int16_t displayH,
                                     int16_t x,
                                     int16_t y,
                                     int16_t w,
                                     int16_t h,
                                     uint16_t color,
                                     const DirectDrawClipRect *clip);
static void directDrawThickLine565Clipped(TFTDisplay *tft,
                                          int16_t displayW,
                                          int16_t displayH,
                                          int16_t x0,
                                          int16_t y0,
                                          int16_t x1,
                                          int16_t y1,
                                          int16_t thickness,
                                          uint16_t color,
                                          const DirectDrawClipRect *clip);

struct HermesXDirectHomeUiCache {
    bool valid = false;
    bool stealth = false;
    bool hasBattery = false;
    uint8_t batteryPercent = 0;
    uint8_t satCount = 0;
    meshtastic_Config_DeviceConfig_Role role = meshtastic_Config_DeviceConfig_Role_CLIENT;
    char date[24] = {0};
    bool lastTimeValid = false;
    char lastTime[16] = {0};
    bool lastDogValid = false;
    uint16_t lastDogFrame = 0xFFFF;
    uint8_t lastDogPose = 0xFF;
    int16_t lastDogX = -1;
    int16_t lastDogY = -1;
    int16_t lastDogW = 0;
    int16_t lastDogH = 0;
};
static HermesXDirectHomeUiCache gHermesXDirectHomeUiCache;
static bool gDirectHomeClockWasVisible = false;
static bool gDirectHomeDogWasVisible = false;
static uint8_t gDirectHomeDogPose = 0;
static uint8_t gDirectHomeDogEntryCount = 0;
static bool gDirectGpsPosterWasVisible = false;
static bool gIncomingNodePopupWasVisible = false;
struct DirectIncomingNodePopupRenderCache {
    bool valid = false;
    uint32_t nodeNum = 0;
    int16_t width = 0;
    int16_t height = 0;
};
static DirectIncomingNodePopupRenderCache gDirectIncomingNodePopupRenderCache;
static bool gDirectHomeBasePainted = false;
static bool gDirectHomeMeshPainted = false;
static uint8_t gDirectHomeOrbLastPulsePercent = 0xFF;
static uint32_t gDirectHomeOrbLastDrawMs = 0;
static uint32_t gDirectHomeLastBaseRefreshMs = 0;
static constexpr uint32_t kDirectHomeBaseRefreshMinMs = 3000;
static constexpr uint32_t kDirectHomeOrbMinFrameIntervalMs = 90;
// Stability A/B switch: disable Home right-bottom orb animation path to isolate panic source.
static constexpr bool kEnableDirectHomeOrbAnimation = false;
// Emergency guard: keep GPS direct-TFT neon post-render disabled until boot stability is verified.
static constexpr bool kEnableDirectGpsPosterTitleNeon = true;
static constexpr bool kEnableDirectGpsPosterDecorNeon = true;
static constexpr bool kEnableDirectGpsPosterCoordinateNeon = true;

struct DirectGpsPosterRenderCache {
    bool valid = false;
    bool gpsEnabled = false;
    bool gpsConnected = false;
    bool gpsHasLock = false;
    uint8_t satCount = 0;
    bool fixedPosition = false;
    bool hasCoordinates = false;
    int32_t latitudeE7 = INT32_MIN;
    int32_t longitudeE7 = INT32_MIN;
    int32_t altitude = INT32_MIN;
    int16_t width = 0;
    int16_t height = 0;
};
static DirectGpsPosterRenderCache gDirectGpsPosterRenderCache;
static bool gDirectGpsNeedsFullFrameAfterSwitch = false;
static bool gDirectGpsBasePainted = false;
static uint8_t gDirectLastFrameIndex = 0xFF;
static bool gNormalFramesInitializedAfterBoot = false;
static constexpr uint32_t kScreenWakeInputGuardMs = 220;

static void invalidateDirectTftWakeCaches()
{
    gHermesXDirectHomeUiCache.valid = false;
    gHermesXDirectHomeUiCache.lastTimeValid = false;
    gHermesXDirectHomeUiCache.lastDogValid = false;
    gHermesXDirectHomeUiCache.lastDogFrame = 0xFFFF;
    gHermesXDirectHomeUiCache.lastDogPose = 0xFF;
    gHermesXDirectHomeUiCache.lastDogX = -1;
    gHermesXDirectHomeUiCache.lastDogY = -1;
    gHermesXDirectHomeUiCache.lastDogW = 0;
    gHermesXDirectHomeUiCache.lastDogH = 0;
    gDirectHomeClockWasVisible = false;
    gDirectHomeDogWasVisible = false;
    gDirectHomeDogPose = 0;
    gDirectHomeDogEntryCount = 0;
    gDirectGpsPosterWasVisible = false;
    gIncomingNodePopupWasVisible = false;
    gDirectIncomingNodePopupRenderCache.valid = false;
    gDirectHomeBasePainted = false;
    gDirectHomeMeshPainted = false;
    gDirectHomeOrbLastPulsePercent = 0xFF;
    gDirectHomeOrbLastDrawMs = 0;
    gDirectHomeLastBaseRefreshMs = 0;
    gDirectGpsPosterRenderCache.valid = false;
    gDirectGpsNeedsFullFrameAfterSwitch = true;
    gDirectGpsBasePainted = false;
    gDirectLastFrameIndex = 0xFF;
}

static constexpr int16_t kDirectNeonClockGlowRadiusOuter = 5;
static constexpr int16_t kDirectNeonClockMargin = kDirectNeonClockGlowRadiusOuter + 1;
static constexpr int16_t kDirectNeonClockGlyphMaxW = 21;
static constexpr int16_t kDirectNeonClockGlyphTileW = kDirectNeonClockGlyphMaxW + (kDirectNeonClockMargin * 2);
static constexpr int16_t kDirectNeonClockGlyphTileH = PattanakarnClock32::kGlyphHeight + (kDirectNeonClockMargin * 2);
static constexpr int16_t kDirectNeonClockSlotCount = 8;
static constexpr int16_t kDirectNeonClockMaxRegionW = (kDirectNeonClockGlyphMaxW * 6) + 14 + (kDirectNeonClockMargin * 2);
static constexpr int16_t kDirectNeonClockMaxRegionH = PattanakarnClock32::kGlyphHeight + (kDirectNeonClockMargin * 2);
// GPS neon text uses a smaller region than the Home clock, but both share one scratch map.
// Keep the shared map large enough for the larger Home clock region to avoid overruns.
static constexpr int16_t kDirectNeonTextMaxW = 128;
static constexpr int16_t kDirectNeonTextMaxH = 32;
static constexpr int16_t kDirectNeonSharedMaxW =
    (kDirectNeonClockMaxRegionW > kDirectNeonTextMaxW) ? kDirectNeonClockMaxRegionW : kDirectNeonTextMaxW;
static constexpr int16_t kDirectNeonSharedMaxH =
    (kDirectNeonClockMaxRegionH > kDirectNeonTextMaxH) ? kDirectNeonClockMaxRegionH : kDirectNeonTextMaxH;
static constexpr size_t kDirectNeonSharedComposedLayerMapSize =
    static_cast<size_t>(kDirectNeonSharedMaxW) * static_cast<size_t>(kDirectNeonSharedMaxH);

struct DirectNeonClockGlyphCache {
    bool valid = false;
    char ch = '\0';
    uint8_t coreW = 0;
    uint8_t tileW = 0;
    uint8_t tileH = 0;
    uint8_t layerMap[kDirectNeonClockGlyphTileW * kDirectNeonClockGlyphTileH] = {0};
};

static DirectNeonClockGlyphCache *gDirectNeonClockGlyphCache = nullptr;
static uint32_t gDirectNeonClockGlyphCacheBuildCount = 0;
// Home clock and GPS neon text never render concurrently, so they can share one scratch map.
static uint8_t *gDirectNeonSharedComposedLayerMap = nullptr;
static uint8_t *gDirectNeonClockPreviousComposedLayerMap = nullptr;
static uint8_t *gDirectNeonClockFullMask = nullptr;
static uint8_t *gDirectNeonClockCoreMask = nullptr;
static uint8_t *gDirectGpsTitleFullMask = nullptr;
static uint8_t *gDirectGpsTitleLayerMap = nullptr;
static uint32_t gDirectNeonBufferGeneration = 0;

static bool shouldKeepDirectNeonBuffers()
{
    return shouldShowHermesXHomeFrame() || shouldShowHermesXGpsFrame();
}

static void freeDirectNeonBuffers()
{
    bool freedAny = false;
    if (gDirectNeonClockGlyphCache) {
        free(gDirectNeonClockGlyphCache);
        gDirectNeonClockGlyphCache = nullptr;
        freedAny = true;
    }
    if (gDirectNeonSharedComposedLayerMap) {
        free(gDirectNeonSharedComposedLayerMap);
        gDirectNeonSharedComposedLayerMap = nullptr;
        freedAny = true;
    }
    if (gDirectNeonClockPreviousComposedLayerMap) {
        free(gDirectNeonClockPreviousComposedLayerMap);
        gDirectNeonClockPreviousComposedLayerMap = nullptr;
        freedAny = true;
    }
    if (gDirectNeonClockFullMask) {
        free(gDirectNeonClockFullMask);
        gDirectNeonClockFullMask = nullptr;
        freedAny = true;
    }
    if (gDirectNeonClockCoreMask) {
        free(gDirectNeonClockCoreMask);
        gDirectNeonClockCoreMask = nullptr;
        freedAny = true;
    }
    if (gDirectGpsTitleFullMask) {
        free(gDirectGpsTitleFullMask);
        gDirectGpsTitleFullMask = nullptr;
        freedAny = true;
    }
    if (gDirectGpsTitleLayerMap) {
        free(gDirectGpsTitleLayerMap);
        gDirectGpsTitleLayerMap = nullptr;
        freedAny = true;
    }
    gDirectNeonClockGlyphCacheBuildCount = 0;
    if (freedAny) {
        ++gDirectNeonBufferGeneration;
    }
}

static bool ensureDirectNeonBuffers()
{
    if (!shouldKeepDirectNeonBuffers()) {
        freeDirectNeonBuffers();
        return false;
    }

    const size_t glyphMaskBytes = static_cast<size_t>(kDirectNeonClockGlyphTileW) * static_cast<size_t>(kDirectNeonClockGlyphTileH);

    if (!gDirectNeonClockGlyphCache) {
        gDirectNeonClockGlyphCache = static_cast<DirectNeonClockGlyphCache *>(calloc(PattanakarnClock32::kGlyphCount,
                                                                                      sizeof(DirectNeonClockGlyphCache)));
    }
    if (!gDirectNeonSharedComposedLayerMap) {
        gDirectNeonSharedComposedLayerMap = static_cast<uint8_t *>(malloc(kDirectNeonSharedComposedLayerMapSize));
    }
    if (!gDirectNeonClockPreviousComposedLayerMap) {
        gDirectNeonClockPreviousComposedLayerMap = static_cast<uint8_t *>(malloc(kDirectNeonSharedComposedLayerMapSize));
    }
    if (!gDirectNeonClockFullMask) {
        gDirectNeonClockFullMask = static_cast<uint8_t *>(malloc(glyphMaskBytes));
    }
    if (!gDirectNeonClockCoreMask) {
        gDirectNeonClockCoreMask = static_cast<uint8_t *>(malloc(glyphMaskBytes));
    }
    if (!gDirectGpsTitleFullMask) {
        gDirectGpsTitleFullMask = static_cast<uint8_t *>(malloc(static_cast<size_t>(96 * 48)));
    }
    if (!gDirectGpsTitleLayerMap) {
        gDirectGpsTitleLayerMap = static_cast<uint8_t *>(malloc(static_cast<size_t>(96 * 48)));
    }

    if (!gDirectNeonClockGlyphCache || !gDirectNeonSharedComposedLayerMap || !gDirectNeonClockPreviousComposedLayerMap ||
        !gDirectNeonClockFullMask || !gDirectNeonClockCoreMask || !gDirectGpsTitleFullMask || !gDirectGpsTitleLayerMap) {
        LOG_WARN("[DirectHome] direct neon buffer alloc failed glyph=%u shared=%u prev=%u mask=%u gpsTitle=%u",
                 gDirectNeonClockGlyphCache ? 1 : 0, gDirectNeonSharedComposedLayerMap ? 1 : 0,
                 gDirectNeonClockPreviousComposedLayerMap ? 1 : 0, (gDirectNeonClockFullMask && gDirectNeonClockCoreMask) ? 1 : 0,
                 (gDirectGpsTitleFullMask && gDirectGpsTitleLayerMap) ? 1 : 0);
        freeDirectNeonBuffers();
        return false;
    }
    return true;
}

static void logDirectHomeNeonSummary(const char *label)
{
    if (!gDirectNeonClockGlyphCache) {
        LOG_DEBUG("[DirectHome] %s glyphCaches=0 buffers=unallocated buildCount=%lu", label,
                  (unsigned long)gDirectNeonClockGlyphCacheBuildCount);
        logHeapSnapshot(label);
        return;
    }
    uint32_t validGlyphCaches = 0;
    for (size_t i = 0; i < PattanakarnClock32::kGlyphCount; ++i) {
        if (gDirectNeonClockGlyphCache[i].valid) {
            ++validGlyphCaches;
        }
    }
    LOG_DEBUG("[DirectHome] %s glyphCaches=%lu glyphTile=%dx%d clockRegion=%dx%d buildCount=%lu", label,
              (unsigned long)validGlyphCaches, kDirectNeonClockGlyphTileW, kDirectNeonClockGlyphTileH,
              kDirectNeonClockMaxRegionW, kDirectNeonClockMaxRegionH, (unsigned long)gDirectNeonClockGlyphCacheBuildCount);
    logHeapSnapshot(label);
}

static void getHermesXHomeBatteryState(bool &hasBattery, int &batteryVoltageMv, uint8_t &batteryPercent)
{
    hasBattery = false;
    batteryVoltageMv = 0;
    batteryPercent = 0;

    if (powerStatus) {
        const int liveVoltageMv = powerStatus->getBatteryVoltageMv();
        const bool hasLiveReading = (liveVoltageMv > 0);
        if (powerStatus->getHasBattery() && hasLiveReading) {
            batteryVoltageMv = liveVoltageMv;
            batteryPercent = estimateBatteryPercentFromVoltageMv(batteryVoltageMv);
            gHermesXBatteryCache.valid = true;
            gHermesXBatteryCache.voltageMv = batteryVoltageMv;
            gHermesXBatteryCache.percent = batteryPercent;
            hasBattery = true;
        } else if (hasLiveReading) {
            batteryVoltageMv = liveVoltageMv;
            batteryPercent = estimateBatteryPercentFromVoltageMv(batteryVoltageMv);
            gHermesXBatteryCache.valid = true;
            gHermesXBatteryCache.voltageMv = batteryVoltageMv;
            gHermesXBatteryCache.percent = batteryPercent;
            hasBattery = true;
        } else if (gHermesXBatteryCache.valid) {
            batteryVoltageMv = gHermesXBatteryCache.voltageMv;
            batteryPercent = gHermesXBatteryCache.percent;
            hasBattery = true;
        }
    } else if (gHermesXBatteryCache.valid) {
        batteryVoltageMv = gHermesXBatteryCache.voltageMv;
        batteryPercent = gHermesXBatteryCache.percent;
        hasBattery = true;
    }
}

static bool formatHermesXHomeTimeDate(char *timeBuf, size_t timeBufSize, char *dateBuf, size_t dateBufSize)
{
    const uint32_t rtcSec = getValidTime(RTCQuality::RTCQualityDevice, true);
    if (rtcSec > 0) {
        time_t t = rtcSec;
        tm *localTm = gmtime(&t);
        static const char *kWeekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
        if (localTm) {
            snprintf(timeBuf, timeBufSize, "%02d:%02d:%02d", localTm->tm_hour, localTm->tm_min, localTm->tm_sec);
            const char *weekday =
                (localTm->tm_wday >= 0 && localTm->tm_wday <= 6) ? kWeekdays[localTm->tm_wday] : "---";
            snprintf(dateBuf, dateBufSize, "%04d/%02d/%02d %s", localTm->tm_year + 1900, localTm->tm_mon + 1,
                     localTm->tm_mday, weekday);
            return true;
        }
    }

    snprintf(timeBuf, timeBufSize, "--:--:--");
    snprintf(dateBuf, dateBufSize, "等待授時");
    return false;
}

static void drawHermesXHomeDog(OLEDDisplay *display, int16_t width, int16_t timeY, bool compactLayout)
{
    if (!display) {
        return;
    }

    const int16_t areaX = 10;
    const int16_t areaW = width - 20;
    if (areaW <= 24) {
        return;
    }

    const uint32_t phase = (millis() / 150U);
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

    const int16_t dogX = areaX + stride;
    const int16_t dogY = timeY + (compactLayout ? 7 : 7);
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
    const bool altStep = (gaitFrame == 0U || gaitFrame == 2U);

    for (int16_t gx = areaX; gx < (areaX + areaW); gx += 4) {
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

    const int16_t legY1 = bodyY + bodyH;
    const int16_t frontLegX = facingRight ? (bodyX + bodyW + 1) : (bodyX - 1);
    const int16_t frontLeg2X = facingRight ? (bodyX + bodyW - 2) : (bodyX + 2);
    const int16_t rearLegX = bodyX + 2;
    const int16_t rearLeg2X = bodyX + bodyW - 2;
    display->drawLine(frontLegX, legY1, frontLegX, legY1 + legH - (altStep ? 0 : 1));
    display->drawLine(frontLeg2X, legY1, frontLeg2X, legY1 + legH - (altStep ? 1 : 0));
    display->drawLine(rearLegX, legY1, rearLegX, legY1 + legH - (altStep ? 1 : 0));
    display->drawLine(rearLeg2X, legY1, rearLeg2X, legY1 + legH - (altStep ? 0 : 1));

    display->setPixel(frontLegX + (facingRight ? 1 : -1), legY1 + legH - (altStep ? 0 : 1));
    display->setPixel(rearLeg2X + 1, legY1 + legH - (altStep ? 0 : 1));
}

static bool canUseDirectHermesXHomeClock(OLEDDisplay *display)
{
    if (!supportsDirectTftClockRendering(display) || !display) {
        return false;
    }

    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    const bool compactLayout = (width < 200 || height < 120);
    const int16_t contentW = width - 8;
    const int16_t customTimeMaxW = measurePattanakarnClockText("88:88:88");
    const bool useCustomTimeFont = (contentW > 0) && (customTimeMaxW > 0) && (customTimeMaxW <= contentW);
    return compactLayout && useCustomTimeFont;
}

static void addTftColorZone(OLEDDisplay *display, int16_t x, int16_t y, int16_t width, int16_t height, uint16_t fg, uint16_t bg)
{
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
    if (!display || width <= 0 || height <= 0) {
        return;
    }
    auto *tft = static_cast<TFTDisplay *>(display);
    tft->addColorPaletteZone(TFTDisplay::ColorZone{x, y, width, height, fg, bg});
#else
    (void)display;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)fg;
    (void)bg;
#endif
}

static const PattanakarnClock32::GlyphInfo *findPattanakarnClockGlyph(char ch)
{
    for (std::size_t i = 0; i < PattanakarnClock32::kGlyphCount; ++i) {
        if (PattanakarnClock32::kGlyphs[i].ch == ch) {
            return &PattanakarnClock32::kGlyphs[i];
        }
    }
    return nullptr;
}

static int16_t measurePattanakarnClockText(const char *text)
{
    if (!text) {
        return 0;
    }

    int16_t width = 0;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        const auto *glyph = findPattanakarnClockGlyph(*cursor);
        if (!glyph) {
            return 0;
        }
        width += glyph->width;
    }
    return width;
}

static constexpr int16_t kPattanakarnClockHalfHeight = (PattanakarnClock32::kGlyphHeight + 1) / 2;

static bool isPattanakarnClockGlyphPixelOn(const PattanakarnClock32::GlyphInfo *glyph, std::uint8_t row, std::uint8_t col)
{
    if (!glyph || row >= PattanakarnClock32::kGlyphHeight || col >= glyph->width) {
        return false;
    }
    const std::uint8_t byteIndex = static_cast<std::uint8_t>(col / 8);
    const std::uint8_t bitIndex = static_cast<std::uint8_t>(col % 8);
    const std::uint8_t rowBits = pgm_read_byte(glyph->bitmap + row * glyph->bytesPerRow + byteIndex);
    return (rowBits & (0x80 >> bitIndex)) != 0;
}

static int16_t measurePattanakarnClockTextHalf(const char *text)
{
    if (!text) {
        return 0;
    }

    int16_t width = 0;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        const auto *glyph = findPattanakarnClockGlyph(*cursor);
        if (!glyph) {
            return 0;
        }
        width += static_cast<int16_t>((glyph->width + 1) / 2);
    }
    return width;
}

static void drawPattanakarnClockTextHalf(OLEDDisplay *display, int16_t x, int16_t y, const char *text)
{
    if (!display || !text) {
        return;
    }

    int16_t cursorX = x;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        const auto *glyph = findPattanakarnClockGlyph(*cursor);
        if (!glyph) {
            continue;
        }

        const int16_t halfW = static_cast<int16_t>((glyph->width + 1) / 2);
        for (int16_t dy = 0; dy < kPattanakarnClockHalfHeight; ++dy) {
            const std::uint8_t srcY = static_cast<std::uint8_t>(std::min<int16_t>(PattanakarnClock32::kGlyphHeight - 1, dy * 2));
            for (int16_t dx = 0; dx < halfW; ++dx) {
                const std::uint8_t srcX = static_cast<std::uint8_t>(std::min<int16_t>(glyph->width - 1, dx * 2));
                bool on = isPattanakarnClockGlyphPixelOn(glyph, srcY, srcX);
                if (!on && (srcX + 1) < glyph->width) {
                    on = isPattanakarnClockGlyphPixelOn(glyph, srcY, static_cast<std::uint8_t>(srcX + 1));
                }
                if (!on && (srcY + 1) < PattanakarnClock32::kGlyphHeight) {
                    on = isPattanakarnClockGlyphPixelOn(glyph, static_cast<std::uint8_t>(srcY + 1), srcX);
                }
                if (!on && (srcY + 1) < PattanakarnClock32::kGlyphHeight && (srcX + 1) < glyph->width) {
                    on = isPattanakarnClockGlyphPixelOn(glyph, static_cast<std::uint8_t>(srcY + 1), static_cast<std::uint8_t>(srcX + 1));
                }
                if (on) {
                    display->setPixel(cursorX + dx, y + dy);
                }
            }
        }

        cursorX += halfW;
    }
}

static void drawPattanakarnClockTextHalfOutsideRect(OLEDDisplay *display,
                                                    int16_t x,
                                                    int16_t y,
                                                    const char *text,
                                                    int16_t excludeX,
                                                    int16_t excludeY,
                                                    int16_t excludeW,
                                                    int16_t excludeH)
{
    if (!display || !text) {
        return;
    }

    const int16_t excludeX2 = excludeX + excludeW;
    const int16_t excludeY2 = excludeY + excludeH;

    int16_t cursorX = x;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        const auto *glyph = findPattanakarnClockGlyph(*cursor);
        if (!glyph) {
            continue;
        }

        const int16_t halfW = static_cast<int16_t>((glyph->width + 1) / 2);
        for (int16_t dy = 0; dy < kPattanakarnClockHalfHeight; ++dy) {
            const std::uint8_t srcY = static_cast<std::uint8_t>(std::min<int16_t>(PattanakarnClock32::kGlyphHeight - 1, dy * 2));
            const int16_t py = y + dy;
            for (int16_t dx = 0; dx < halfW; ++dx) {
                const std::uint8_t srcX = static_cast<std::uint8_t>(std::min<int16_t>(glyph->width - 1, dx * 2));
                bool on = isPattanakarnClockGlyphPixelOn(glyph, srcY, srcX);
                if (!on && (srcX + 1) < glyph->width) {
                    on = isPattanakarnClockGlyphPixelOn(glyph, srcY, static_cast<std::uint8_t>(srcX + 1));
                }
                if (!on && (srcY + 1) < PattanakarnClock32::kGlyphHeight) {
                    on = isPattanakarnClockGlyphPixelOn(glyph, static_cast<std::uint8_t>(srcY + 1), srcX);
                }
                if (!on && (srcY + 1) < PattanakarnClock32::kGlyphHeight && (srcX + 1) < glyph->width) {
                    on = isPattanakarnClockGlyphPixelOn(glyph, static_cast<std::uint8_t>(srcY + 1), static_cast<std::uint8_t>(srcX + 1));
                }
                if (!on) {
                    continue;
                }

                const int16_t px = cursorX + dx;
                if (px >= excludeX && py >= excludeY && px < excludeX2 && py < excludeY2) {
                    continue;
                }
                display->setPixel(px, py);
            }
        }

        cursorX += halfW;
    }
}

static void drawPattanakarnClockText(OLEDDisplay *display, int16_t x, int16_t y, const char *text)
{
    if (!display || !text) {
        return;
    }

    int16_t cursorX = x;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        const auto *glyph = findPattanakarnClockGlyph(*cursor);
        if (!glyph) {
            continue;
        }

        for (std::uint8_t row = 0; row < PattanakarnClock32::kGlyphHeight; ++row) {
            for (std::uint8_t byteIndex = 0; byteIndex < glyph->bytesPerRow; ++byteIndex) {
                const std::uint8_t rowBits =
                    pgm_read_byte(glyph->bitmap + row * glyph->bytesPerRow + byteIndex);
                for (std::uint8_t bit = 0; bit < 8; ++bit) {
                    const std::uint8_t col = static_cast<std::uint8_t>(byteIndex * 8 + bit);
                    if (col >= glyph->width) {
                        break;
                    }
                    if (rowBits & (0x80 >> bit)) {
                        display->setPixel(cursorX + col, y + row);
                    }
                }
            }
        }

        cursorX += glyph->width;
    }
}

static void drawPattanakarnClockTextOutsideRect(OLEDDisplay *display,
                                                int16_t x,
                                                int16_t y,
                                                const char *text,
                                                int16_t excludeX,
                                                int16_t excludeY,
                                                int16_t excludeW,
                                                int16_t excludeH)
{
    if (!display || !text) {
        return;
    }

    const int16_t excludeX2 = excludeX + excludeW;
    const int16_t excludeY2 = excludeY + excludeH;

    int16_t cursorX = x;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        const auto *glyph = findPattanakarnClockGlyph(*cursor);
        if (!glyph) {
            continue;
        }

        for (std::uint8_t row = 0; row < PattanakarnClock32::kGlyphHeight; ++row) {
            for (std::uint8_t byteIndex = 0; byteIndex < glyph->bytesPerRow; ++byteIndex) {
                const std::uint8_t rowBits =
                    pgm_read_byte(glyph->bitmap + row * glyph->bytesPerRow + byteIndex);
                for (std::uint8_t bit = 0; bit < 8; ++bit) {
                    const std::uint8_t col = static_cast<std::uint8_t>(byteIndex * 8 + bit);
                    if (col >= glyph->width) {
                        break;
                    }
                    if (!(rowBits & (0x80 >> bit))) {
                        continue;
                    }

                    const int16_t px = cursorX + col;
                    const int16_t py = y + row;
                    if (px >= excludeX && py >= excludeY && px < excludeX2 && py < excludeY2) {
                        continue;
                    }
                    display->setPixel(px, py);
                }
            }
        }

        cursorX += glyph->width;
    }
}

static bool supportsDirectTftClockRendering(OLEDDisplay *display)
{
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
    return display != nullptr;
#else
    (void)display;
    return false;
#endif
}

static void stampPattanakarnClockMask(uint8_t *mask,
                                      int16_t maskW,
                                      int16_t maskH,
                                      int16_t x,
                                      int16_t y,
                                      const char *text,
                                      uint8_t value)
{
    if (!mask || !text || maskW <= 0 || maskH <= 0) {
        return;
    }

    int16_t cursorX = x;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        const auto *glyph = findPattanakarnClockGlyph(*cursor);
        if (!glyph) {
            continue;
        }

        for (std::uint8_t row = 0; row < PattanakarnClock32::kGlyphHeight; ++row) {
            const int16_t py = y + static_cast<int16_t>(row);
            if (py < 0 || py >= maskH) {
                continue;
            }
            for (std::uint8_t byteIndex = 0; byteIndex < glyph->bytesPerRow; ++byteIndex) {
                const std::uint8_t rowBits =
                    pgm_read_byte(glyph->bitmap + row * glyph->bytesPerRow + byteIndex);
                for (std::uint8_t bit = 0; bit < 8; ++bit) {
                    const std::uint8_t col = static_cast<std::uint8_t>(byteIndex * 8 + bit);
                    if (col >= glyph->width) {
                        break;
                    }
                    if (!(rowBits & (0x80 >> bit))) {
                        continue;
                    }

                    const int16_t px = cursorX + static_cast<int16_t>(col);
                    if (px < 0 || px >= maskW) {
                        continue;
                    }
                    mask[py * maskW + px] = value;
                }
            }
        }

        cursorX += glyph->width;
    }
}

static bool hasMaskPixelInRadius(const uint8_t *mask, int16_t maskW, int16_t maskH, int16_t x, int16_t y, int16_t radius)
{
    if (!mask || radius < 0) {
        return false;
    }

    const int16_t minY = std::max<int16_t>(0, y - radius);
    const int16_t maxY = std::min<int16_t>(maskH - 1, y + radius);
    const int16_t minX = std::max<int16_t>(0, x - radius);
    const int16_t maxX = std::min<int16_t>(maskW - 1, x + radius);
    const int32_t radiusSq = static_cast<int32_t>(radius) * static_cast<int32_t>(radius);

    for (int16_t yy = minY; yy <= maxY; ++yy) {
        const int32_t dy = static_cast<int32_t>(yy - y);
        const int32_t dySq = dy * dy;
        const int16_t rowBase = yy * maskW;
        for (int16_t xx = minX; xx <= maxX; ++xx) {
            if (!mask[rowBase + xx]) {
                continue;
            }

            const int32_t dx = static_cast<int32_t>(xx - x);
            if ((dx * dx) + dySq <= radiusSq) {
                return true;
            }
        }
    }
    return false;
}

static bool isMaskInteriorPixel(const uint8_t *mask, int16_t maskW, int16_t maskH, int16_t x, int16_t y)
{
    if (!mask || x <= 0 || y <= 0 || x >= (maskW - 1) || y >= (maskH - 1)) {
        return false;
    }

    for (int16_t yy = y - 1; yy <= y + 1; ++yy) {
        const int16_t rowBase = yy * maskW;
        for (int16_t xx = x - 1; xx <= x + 1; ++xx) {
            if (!mask[rowBase + xx]) {
                return false;
            }
        }
    }
    return true;
}

static uint16_t mapDirectNeonClockLayerColor(uint8_t value)
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

static int16_t getDirectNeonClockSlotWidth(size_t slotIndex)
{
    return (slotIndex == 2 || slotIndex == 5) ? 7 : kDirectNeonClockGlyphMaxW;
}

static int16_t getDirectNeonClockFrameWidth()
{
    int16_t width = 0;
    for (size_t i = 0; i < kDirectNeonClockSlotCount; ++i) {
        width += getDirectNeonClockSlotWidth(i);
    }
    return width;
}

static DirectNeonClockGlyphCache *getDirectNeonClockGlyphCache(char ch)
{
    const auto *glyph = findPattanakarnClockGlyph(ch);
    if (!glyph) {
        return nullptr;
    }

    const std::size_t glyphIndex = static_cast<std::size_t>(glyph - &PattanakarnClock32::kGlyphs[0]);
    if (glyphIndex >= PattanakarnClock32::kGlyphCount) {
        return nullptr;
    }
    if (!ensureDirectNeonBuffers()) {
        return nullptr;
    }

    auto &cache = gDirectNeonClockGlyphCache[glyphIndex];
    if (cache.valid) {
        return &cache;
    }

    uint8_t *fullMask = gDirectNeonClockFullMask;
    uint8_t *coreMask = gDirectNeonClockCoreMask;
    if (!fullMask || !coreMask) {
        return nullptr;
    }
    memset(fullMask, 0, static_cast<size_t>(kDirectNeonClockGlyphTileW) * static_cast<size_t>(kDirectNeonClockGlyphTileH));
    memset(coreMask, 0, static_cast<size_t>(kDirectNeonClockGlyphTileW) * static_cast<size_t>(kDirectNeonClockGlyphTileH));
    memset(cache.layerMap, 0, sizeof(cache.layerMap));

    const char glyphText[2] = {ch, '\0'};
    stampPattanakarnClockMask(fullMask,
                              kDirectNeonClockGlyphTileW,
                              kDirectNeonClockGlyphTileH,
                              kDirectNeonClockMargin,
                              kDirectNeonClockMargin,
                              glyphText,
                              1);

    for (int16_t yy = 0; yy < kDirectNeonClockGlyphTileH; ++yy) {
        for (int16_t xx = 0; xx < kDirectNeonClockGlyphTileW; ++xx) {
            if (!fullMask[(yy * kDirectNeonClockGlyphTileW) + xx]) {
                continue;
            }
            if (isMaskInteriorPixel(fullMask, kDirectNeonClockGlyphTileW, kDirectNeonClockGlyphTileH, xx, yy)) {
                coreMask[(yy * kDirectNeonClockGlyphTileW) + xx] = 1;
            }
        }
    }

    for (int16_t yy = 0; yy < kDirectNeonClockGlyphTileH; ++yy) {
        for (int16_t xx = 0; xx < kDirectNeonClockGlyphTileW; ++xx) {
            const int16_t idx = (yy * kDirectNeonClockGlyphTileW) + xx;
            uint8_t value = 0;
            if (coreMask[idx]) {
                value = 7;
            } else if (fullMask[idx]) {
                value = 6;
            } else if (hasMaskPixelInRadius(fullMask,
                                            kDirectNeonClockGlyphTileW,
                                            kDirectNeonClockGlyphTileH,
                                            xx,
                                            yy,
                                            1)) {
                value = 5;
            } else if (hasMaskPixelInRadius(fullMask,
                                            kDirectNeonClockGlyphTileW,
                                            kDirectNeonClockGlyphTileH,
                                            xx,
                                            yy,
                                            2)) {
                value = 4;
            } else if (hasMaskPixelInRadius(fullMask,
                                            kDirectNeonClockGlyphTileW,
                                            kDirectNeonClockGlyphTileH,
                                            xx,
                                            yy,
                                            3)) {
                value = 3;
            } else if (hasMaskPixelInRadius(fullMask,
                                            kDirectNeonClockGlyphTileW,
                                            kDirectNeonClockGlyphTileH,
                                            xx,
                                            yy,
                                            4)) {
                value = 2;
            } else if (hasMaskPixelInRadius(fullMask,
                                            kDirectNeonClockGlyphTileW,
                                            kDirectNeonClockGlyphTileH,
                                            xx,
                                            yy,
                                            kDirectNeonClockGlowRadiusOuter)) {
                value = 1;
            }
            cache.layerMap[idx] = value;
        }
    }

    cache.valid = true;
    cache.ch = ch;
    cache.coreW = glyph->width;
    cache.tileW = glyph->width + (kDirectNeonClockMargin * 2);
    cache.tileH = kDirectNeonClockGlyphTileH;
    ++gDirectNeonClockGlyphCacheBuildCount;
    LOG_DEBUG("[DirectHome] build glyph cache ch='%c' index=%u coreW=%u tile=%ux%u bytes=%u", ch, (unsigned)glyphIndex,
              (unsigned)cache.coreW, (unsigned)cache.tileW, (unsigned)cache.tileH, (unsigned)sizeof(cache.layerMap));
    logDirectHomeNeonSummary("DirectHome glyph cache built");
    return &cache;
}

static void blitDirectNeonClockGlyph(TFTDisplay *tft, int16_t x, int16_t y, const DirectNeonClockGlyphCache *cache)
{
    if (!tft || !cache || !cache->valid || cache->tileW <= 0 || cache->tileH <= 0) {
        return;
    }

    const uint16_t black = TFTDisplay::rgb565(0x00, 0x00, 0x00);
    for (int16_t row = 0; row < cache->tileH; ++row) {
        int16_t runStart = -1;
        uint16_t runColor = 0;
        for (int16_t col = 0; col <= cache->tileW; ++col) {
            const bool atEnd = (col == cache->tileW);
            const uint16_t color = atEnd ? 0 : mapDirectNeonClockLayerColor(cache->layerMap[(row * kDirectNeonClockGlyphTileW) + col]);
            if (color == black) {
                if (runStart >= 0) {
                    tft->fillRect565(x + runStart, y + row, col - runStart, 1, runColor);
                    runStart = -1;
                }
                continue;
            }

            if (runStart < 0) {
                runStart = col;
                runColor = color;
                continue;
            }

            if (color != runColor) {
                tft->fillRect565(x + runStart, y + row, col - runStart, 1, runColor);
                runStart = col;
                runColor = color;
            }
        }
    }
}

static void clearDirectNeonClockRegion(TFTDisplay *tft, int16_t displayW, int16_t originY = 0)
{
    if (!tft || displayW <= 0) {
        return;
    }

    const int16_t frameW = getDirectNeonClockFrameWidth();
    const int16_t glyphH = PattanakarnClock32::kGlyphHeight;
    if (frameW <= 0 || frameW > (displayW - 4)) {
        return;
    }

    const int16_t timeY = originY + 8;
    const int16_t frameX = (displayW - frameW) / 2;
    const int16_t regionX = frameX - kDirectNeonClockMargin;
    const int16_t regionY = timeY - kDirectNeonClockMargin;
    const int16_t regionW = frameW + (kDirectNeonClockMargin * 2);
    const int16_t regionH = glyphH + (kDirectNeonClockMargin * 2);
    if (regionW <= 0 || regionH <= 0) {
        return;
    }

    tft->fillRect565(regionX, regionY, regionW, regionH, TFTDisplay::rgb565(0x00, 0x00, 0x00));
}

static bool getDirectNeonClockRegionRect(int16_t displayW,
                                         int16_t originY,
                                         int16_t &regionX,
                                         int16_t &regionY,
                                         int16_t &regionW,
                                         int16_t &regionH,
                                         int16_t &frameX)
{
    if (displayW <= 0) {
        return false;
    }

    const int16_t frameW = getDirectNeonClockFrameWidth();
    const int16_t glyphH = PattanakarnClock32::kGlyphHeight;
    if (frameW <= 0 || frameW > (displayW - 4)) {
        return false;
    }

    const int16_t timeY = originY + 8;
    frameX = (displayW - frameW) / 2;
    regionX = frameX - kDirectNeonClockMargin;
    regionY = timeY - kDirectNeonClockMargin;
    regionW = frameW + (kDirectNeonClockMargin * 2);
    regionH = glyphH + (kDirectNeonClockMargin * 2);
    if (regionW <= 0 || regionH <= 0 || regionW > kDirectNeonClockMaxRegionW || regionH > kDirectNeonClockMaxRegionH) {
        return false;
    }
    return true;
}

static void renderDirectHomeDog(TFTDisplay *tft,
                                int16_t displayW,
                                int16_t displayH,
                                int16_t originY,
                                uint16_t dogFrame,
                                uint8_t dogPose)
{
    if (!tft) {
        return;
    }

    int16_t clockRegionX = 0;
    int16_t clockRegionY = 0;
    int16_t clockRegionW = 0;
    int16_t clockRegionH = 0;
    int16_t frameX = 0;
    if (!getDirectNeonClockRegionRect(displayW, originY, clockRegionX, clockRegionY, clockRegionW, clockRegionH, frameX)) {
        return;
    }

    static constexpr int16_t kDogSpriteW = 24;
    static constexpr int16_t kDogSpriteH = 24;
    static constexpr int16_t kDogScale = 2;
    static constexpr int16_t kDogDrawW = kDogSpriteW * kDogScale;
    static constexpr int16_t kDogDrawH = kDogSpriteH * kDogScale;
    const int16_t regionW = std::min<int16_t>(displayW - 4, 120);
    const int16_t regionH = std::min<int16_t>(displayH - clockRegionY, kDogDrawH + 4);
    const int16_t regionX = 2;
    const int16_t regionY = clockRegionY;
    if (regionW <= 0 || regionH <= 0) {
        return;
    }

    DirectDrawClipRect clip{};
    if (!makeDirectDrawClipRect(regionX, regionY, regionW, regionH, displayW, displayH, clip)) {
        return;
    }

    static constexpr const char *kDogLyingFrames[2][kDogSpriteH] = {
        {
            "...............O....O...",
            "..............OTO..OHO..",
            "..............OLO..OHO..",
            "..............OLO..OHO..",
            "..............OHT..HLO..",
            "..............OHTOOHLO..",
            ".............OOLTHHHHO..",
            ".............OOLHHTHHO..",
            ".............OSLHHOHHO..",
            ".............SSHHHOHHS..",
            ".............SOHHHHHHHO.",
            "...........OSOHHHHHHSOO",
            "......SOOOOOOSSHHHOOOSOO",
            ".T....SSSSOOSSSOHHOOSSSO",
            "......OSSSSSSSSSOHHHOOOO.",
            ".....SSHHSSSLOSOHHHHHO..",
            "OO..OOHHHLTHLHHOHHHHHO..",
            "OS..OSHHHTOHLHHHH..LSO..",
            "OLO.OHHHHTOTTHHHHHHLLO..",
            ".HSOOHHHHTSTTHHHHHLLOOO.",
            ".OTLOHHHOOTTTHHHOOOOHHHS",
            "..TLOHHHTLSTSHHHHHHHOHLO",
            "..WWOHHHHHLOOHHHHHHHTHLO",
            ".....OOOOOOO.OOOOOOOOO..",
        },
        {
            "...............OO...O...",
            "...............TO..OHO..",
            "...............TS..OHO..",
            "..............OLLO.OHO..",
            "..............OSLO.LTO..",
            "..............OTLOOLTO..",
            "..............OHLHHHHO..",
            "..............OHHHHHHS..",
            "..............OLHHOHHOO.",
            ".............OOHHHOHHOO.",
            ".............OOHHHHHHHO.",
            "...........OSOHHHHHHOSO",
            "......OSSSOOOSSHHHOOSSSO",
            "......SSSSSSSSSHHHOOSSSO",
            ".....OSSSSSSOSSOHHHSSSS.",
            ".....SSHHSSSTTSOHHHLTO..",
            "....OSHHHHTHLHHSTHHHHO..",
            "....OSHHHHOTTHHHT..LHO..",
            "....OHHHHHOTLHHHH..LHO..",
            "...SOTHHHHSHLHHHHHLLHO..",
            "..OOOTHHOOHHSHHOOOOTHHO.",
            "OLTTOTHHHTLOSHHHHHHOHTH.",
            "OHHTOSHHHHLOOHHHHHTSHTL.",
            ".OOO.OOOOOOO.OOOOOOOOO..",
        },
    };
    static constexpr const char *kDogSittingFrames[2][kDogSpriteH] = {
        {
            "..............OO...OO...",
            "..............TS...HT...",
            "..............LTO.OHTO..",
            "..............LTOOOTTO..",
            ".............OTLHHHHHO..",
            ".............OTLHOOHHT..",
            ".............OTTHOOHHOOO",
            ".............OHHHHHHSSOO",
            ".............OHHHOOOSSSO",
            "............OOHHHOOSTL..",
            "............OOOHHHOHHT..",
            "............OSOHHHHOOO..",
            "...........OOOSLHHHHHO..",
            "..........OSSSSLLSHHHO..",
            "..........OSSSSTLS.HHO..",
            ".........OOOSHHHHS..H...",
            ".........OSSSHHHHH.HHG..",
            "........OSSSSTTHHTHTTG..",
            "........OSHHOSHHHTHTHG..",
            "..TSO..SOLHHHOOHHSSHHG..",
            "..OHLSOOHHHHHOOHHTSHHG..",
            "...OSTLSHHHHOOOHHOOHHG..",
            ".....OOOHHHHHHOHHHOHHHO.",
            ".........OOOOOOOOOOOOOO.",
        },
        {
            "..............OO........",
            "..............TSO..OTO..",
            "..............THO.OHLO..",
            "..............TTOOOHLO..",
            ".............OHLHHHHHO..",
            ".............OHLHSOHHO..",
            ".............OLLHSOHHOOO",
            ".............OHHHHHHTSOO",
            ".............OHHHOOOOSSO",
            "............OOHHHOOTTT..",
            "............OOOHHHOHHH..",
            "............OSOHHHHOOO..",
            "...........OOOSTHHHHHO..",
            "..........OOSSSLLSHHHO..",
            "..........OSSSOLTS.HHO..",
            ".........OOOSHHHHS..HG..",
            "........OOSSSHHHHH.THG..",
            "........OSSSSLHHHLHTTG..",
            "........SSHHOLHHHSHTHG..",
            ".......OSHHHHOLHHSSHHG..",
            ".......OTHHHHOSHHSSHHG..",
            "...OOSSOHHHHOOSHHOOHHG..",
            "OOTTTTTOHHHHHTOHHHOHHTO.",
            ".OOOO...OOOOOOOOO.OOOO..",
        },
    };

    const uint16_t outline = TFTDisplay::rgb565(0x18, 0x18, 0x1C);
    const uint16_t saddle = TFTDisplay::rgb565(0x4A, 0x49, 0x46);
    const uint16_t tan = TFTDisplay::rgb565(0xA7, 0x74, 0x55);
    const uint16_t tanHi = TFTDisplay::rgb565(0xD4, 0xA5, 0x80);
    const uint16_t chest = TFTDisplay::rgb565(0xF8, 0xF7, 0xF8);
    const uint16_t tongue = TFTDisplay::rgb565(0xBC, 0x55, 0x51);
    const uint16_t ground = TFTDisplay::rgb565(0xC6, 0xB8, 0xBB);
    const uint16_t bg = TFTDisplay::rgb565(0x00, 0x00, 0x00);

    const int16_t dogX = regionX;
    const int16_t dogY = regionY + std::max<int16_t>(0, (regionH - kDogDrawH) / 2);

    directFillRect565Clipped(tft, displayW, displayH, regionX, regionY, regionW, regionH, bg, &clip);
    const uint8_t gaitFrame = static_cast<uint8_t>(dogFrame & 0x1U);
    const char *const *sprite = (dogPose == 0) ? kDogLyingFrames[gaitFrame] : kDogSittingFrames[gaitFrame];
    auto colorFor = [&](char token) -> uint16_t {
        switch (token) {
        case 'O':
            return outline;
        case 'H':
            return tanHi;
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
            return bg;
        }
    };

    for (int16_t sy = 0; sy < kDogSpriteH; ++sy) {
        const char *row = sprite[sy];
        for (int16_t sx = 0; sx < kDogSpriteW; ++sx) {
            const char token = row[sx];
            if (token == '.') {
                continue;
            }
            directFillRect565Clipped(tft,
                                     displayW,
                                     displayH,
                                     dogX + (sx * kDogScale),
                                     dogY + (sy * kDogScale),
                                     kDogScale,
                                     kDogScale,
                                     colorFor(token),
                                     &clip);
        }
    }

    tft->setColor(OLEDDISPLAY_COLOR::WHITE);
    tft->setTextAlignment(TEXT_ALIGN_LEFT);
    tft->setFont(FONT_SMALL);
    const int16_t textX = dogX + kDogDrawW + 6;
    const int16_t textY = regionY + std::max<int16_t>(0, (regionH - FONT_HEIGHT_SMALL) / 2);
    const int16_t textW = std::max<int16_t>(0, regionW - kDogDrawW - 8);
    HermesX_zh::drawMixedBounded(*tft, textX, textY, textW, u8"請連接手機", HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
    if (textW > 0) {
        tft->overlayBufferForegroundRect565(textX, textY, textW, FONT_HEIGHT_SMALL + 2);
    }

    gHermesXDirectHomeUiCache.lastDogX = regionX;
    gHermesXDirectHomeUiCache.lastDogY = regionY;
    gHermesXDirectHomeUiCache.lastDogW = regionW;
    gHermesXDirectHomeUiCache.lastDogH = regionH;
}

static void renderDirectNeonClock(TFTDisplay *tft,
                                  int16_t width,
                                  int16_t originY,
                                  const char *timeBuf,
                                  bool forceFullRedraw = false,
                                  bool clearBackground = true)
{
    if (!tft || !timeBuf) {
        return;
    }

    int16_t regionX = 0;
    int16_t regionY = 0;
    int16_t regionW = 0;
    int16_t regionH = 0;
    int16_t frameX = 0;
    if (!getDirectNeonClockRegionRect(width, originY, regionX, regionY, regionW, regionH, frameX)) {
        return;
    }
    if (!ensureDirectNeonBuffers() || !gDirectNeonSharedComposedLayerMap || !gDirectNeonClockPreviousComposedLayerMap) {
        return;
    }
    const uint16_t black = TFTDisplay::rgb565(0x00, 0x00, 0x00);

    static char previousTimeBuf[16] = {0};
    static bool previousValid = false;
    static int16_t previousRegionX = 0;
    static int16_t previousRegionY = 0;
    static int16_t previousRegionW = 0;
    static int16_t previousRegionH = 0;
    static uint32_t previousBufferGeneration = 0;
    uint8_t *previousComposedLayerMap = gDirectNeonClockPreviousComposedLayerMap;

    if (previousBufferGeneration != gDirectNeonBufferGeneration) {
        previousValid = false;
        previousBufferGeneration = gDirectNeonBufferGeneration;
    }

    const bool geometryChanged = !previousValid || previousRegionX != regionX || previousRegionY != regionY ||
                                 previousRegionW != regionW || previousRegionH != regionH;

    if (clearBackground && geometryChanged && previousValid) {
        tft->fillRect565(previousRegionX, previousRegionY, previousRegionW, previousRegionH, black);
    }

    const bool timeChanged = !previousValid || strcmp(previousTimeBuf, timeBuf) != 0;
    if (!geometryChanged && !forceFullRedraw && !timeChanged) {
        return;
    }
    LOG_DEBUG("[DirectHome] renderDirectNeonClock geometryChanged=%d forceFullRedraw=%d timeChanged=%d region=%dx%d",
              geometryChanged ? 1 : 0, forceFullRedraw ? 1 : 0, timeChanged ? 1 : 0, regionW, regionH);

    int16_t slotStartX[kDirectNeonClockSlotCount + 1];
    slotStartX[0] = frameX;
    for (size_t slotIndex = 0; slotIndex < kDirectNeonClockSlotCount; ++slotIndex) {
        slotStartX[slotIndex + 1] = slotStartX[slotIndex] + getDirectNeonClockSlotWidth(slotIndex);
    }

    uint8_t *composedLayerMap = gDirectNeonSharedComposedLayerMap;
    memset(composedLayerMap, 0, static_cast<size_t>(regionW * regionH));

    const auto composeSlot = [&](size_t slotIndex) {
        const char ch = timeBuf[slotIndex];
        if (ch == '\0') {
            return;
        }
        if (auto *cache = getDirectNeonClockGlyphCache(ch)) {
            const int16_t slotW = getDirectNeonClockSlotWidth(slotIndex);
            const int16_t drawX = slotStartX[slotIndex] + ((slotW - cache->coreW) / 2) - kDirectNeonClockMargin;
            for (int16_t row = 0; row < cache->tileH; ++row) {
                const int16_t py = regionY + row;
                if (py < regionY || py >= (regionY + regionH)) {
                    continue;
                }
                for (int16_t col = 0; col < cache->tileW; ++col) {
                    const int16_t px = drawX + col;
                    if (px < regionX || px >= (regionX + regionW)) {
                        continue;
                    }
                    const uint8_t value = cache->layerMap[(row * kDirectNeonClockGlyphTileW) + col];
                    if (!value) {
                        continue;
                    }
                    const int16_t idx = ((py - regionY) * regionW) + (px - regionX);
                    if (value > composedLayerMap[idx]) {
                        composedLayerMap[idx] = value;
                    }
                }
            }
        }
    };

    for (size_t slotIndex = 0; slotIndex < kDirectNeonClockSlotCount; ++slotIndex) {
        if (timeBuf[slotIndex] == '\0') {
            break;
        }
        composeSlot(slotIndex);
    }

    const bool fullRepaintRequired = geometryChanged || forceFullRedraw || !previousValid;
    if (fullRepaintRequired) {
        if (clearBackground) {
            tft->fillRect565(regionX, regionY, regionW, regionH, black);
        }
        for (int16_t row = 0; row < regionH; ++row) {
            int16_t runStart = -1;
            uint8_t runValue = 0xFF;
            for (int16_t col = 0; col <= regionW; ++col) {
                const uint8_t value = (col < regionW) ? composedLayerMap[(row * regionW) + col] : 0xFF;
                if (value == runValue) {
                    continue;
                }
                if (runStart >= 0 && runValue > 0) {
                    tft->fillRect565(regionX + runStart,
                                     regionY + row,
                                     col - runStart,
                                     1,
                                     mapDirectNeonClockLayerColor(runValue));
                }
                runValue = value;
                runStart = (col < regionW) ? col : -1;
            }
        }
    } else {
        for (int16_t row = 0; row < regionH; ++row) {
            int16_t runStart = -1;
            uint8_t runValue = 0xFF;
            for (int16_t col = 0; col <= regionW; ++col) {
                bool changed = false;
                uint8_t value = 0xFF;
                if (col < regionW) {
                    const int16_t idx = (row * regionW) + col;
                    changed = previousComposedLayerMap[idx] != composedLayerMap[idx];
                    value = composedLayerMap[idx];
                }

                if (!changed || value != runValue) {
                    if (runStart >= 0) {
                        if (runValue == 0 && !clearBackground) {
                            runStart = -1;
                            continue;
                        }
                        const uint16_t color = (runValue > 0) ? mapDirectNeonClockLayerColor(runValue) : black;
                        tft->fillRect565(regionX + runStart, regionY + row, col - runStart, 1, color);
                        runStart = -1;
                    }
                }

                if (changed) {
                    if (runStart < 0) {
                        runStart = col;
                        runValue = value;
                    }
                } else {
                    runValue = 0xFF;
                }
            }
        }
    }

    strlcpy(previousTimeBuf, timeBuf, sizeof(previousTimeBuf));
    memcpy(previousComposedLayerMap, composedLayerMap, static_cast<size_t>(regionW * regionH));
    previousRegionX = regionX;
    previousRegionY = regionY;
    previousRegionW = regionW;
    previousRegionH = regionH;
    previousValid = true;
}

static int16_t measurePattanakarnClockTextTracked(const char *text, bool halfScale, int16_t tracking)
{
    if (!text) {
        return 0;
    }

    int16_t width = 0;
    bool hasGlyph = false;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        const auto *glyph = findPattanakarnClockGlyph(*cursor);
        if (!glyph) {
            continue;
        }
        if (hasGlyph) {
            width += tracking;
        }
        width += halfScale ? static_cast<int16_t>((glyph->width + 1) / 2) : static_cast<int16_t>(glyph->width);
        hasGlyph = true;
    }
    return width;
}

static bool renderDirectNeonPattanakarnText(TFTDisplay *tft,
                                            int16_t drawX,
                                            int16_t drawY,
                                            const char *text,
                                            bool halfScale,
                                            uint16_t (*layerColorFn)(uint8_t) = nullptr,
                                            bool clearBackground = true,
                                            int16_t tracking = 0,
                                            int16_t marginOverride = -1)
{
    if (!tft || !text || !*text) {
        return false;
    }
    if (!layerColorFn) {
        layerColorFn = mapDirectNeonClockLayerColor;
    }
    if (!ensureDirectNeonBuffers() || !gDirectNeonSharedComposedLayerMap) {
        return false;
    }

    const int16_t coreW = measurePattanakarnClockTextTracked(text, halfScale, tracking);
    if (coreW <= 0) {
        return false;
    }

    const int16_t coreH = halfScale ? kPattanakarnClockHalfHeight : PattanakarnClock32::kGlyphHeight;
    int16_t margin = halfScale ? static_cast<int16_t>((kDirectNeonClockMargin + 1) / 2) : kDirectNeonClockMargin;
    if (marginOverride >= 0) {
        margin = std::max<int16_t>(0, std::min<int16_t>(margin, marginOverride));
    }
    const int16_t regionX = drawX - margin;
    const int16_t regionY = drawY - margin;
    const int16_t regionW = coreW + (margin * 2);
    const int16_t regionH = coreH + (margin * 2);
    if (regionW <= 0 || regionH <= 0 || regionW > kDirectNeonTextMaxW || regionH > kDirectNeonTextMaxH) {
        return false;
    }
    LOG_DEBUG("[DirectHome] direct neon text text='%s' half=%d tracking=%d margin=%d region=%dx%d", text, halfScale ? 1 : 0,
              tracking, margin, regionW, regionH);

    uint8_t *composedLayerMap = gDirectNeonSharedComposedLayerMap;
    memset(composedLayerMap, 0, static_cast<size_t>(regionW * regionH));

    int16_t cursorX = drawX;
    bool hasGlyph = false;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        auto *cache = getDirectNeonClockGlyphCache(*cursor);
        if (!cache) {
            continue;
        }
        if (hasGlyph) {
            cursorX += tracking;
        }

        const int16_t glyphX = cursorX - margin;
        const int16_t glyphY = drawY - margin;
        for (int16_t row = 0; row < cache->tileH; ++row) {
            const int16_t py = glyphY + (halfScale ? (row / 2) : row);
            if (py < regionY || py >= (regionY + regionH)) {
                continue;
            }
            for (int16_t col = 0; col < cache->tileW; ++col) {
                const uint8_t value = cache->layerMap[(row * kDirectNeonClockGlyphTileW) + col];
                if (!value) {
                    continue;
                }

                const int16_t px = glyphX + (halfScale ? (col / 2) : col);
                if (px < regionX || px >= (regionX + regionW)) {
                    continue;
                }

                const int16_t idx = ((py - regionY) * regionW) + (px - regionX);
                if (value > composedLayerMap[idx]) {
                    composedLayerMap[idx] = value;
                }
            }
        }

        cursorX += halfScale ? static_cast<int16_t>((cache->coreW + 1) / 2) : cache->coreW;
        hasGlyph = true;
    }

    const uint16_t black = TFTDisplay::rgb565(0x00, 0x00, 0x00);
    if (clearBackground) {
        tft->fillRect565(regionX, regionY, regionW, regionH, black);
    }
    for (int16_t row = 0; row < regionH; ++row) {
        int16_t runStart = -1;
        uint8_t runValue = 0xFF;
        for (int16_t col = 0; col <= regionW; ++col) {
            const uint8_t value = (col < regionW) ? composedLayerMap[(row * regionW) + col] : 0xFF;
            if (value == runValue) {
                continue;
            }
            if (runStart >= 0 && runValue > 0) {
                tft->fillRect565(regionX + runStart, regionY + row, col - runStart, 1, layerColorFn(runValue));
            }
            runValue = value;
            runStart = (col < regionW) ? col : -1;
        }
    }

    return true;
}

struct DirectGpsPosterLayout {
    bool tiny = false;
    int16_t leftX = 0;
    int16_t leftW = 0;
    int16_t titleY = 0;
    int16_t coordY1 = 0;
    int16_t coordY2 = 0;
    int16_t altitudeY = 0;
    int16_t globeCx = 0;
    int16_t globeCy = 0;
    int16_t globeR = 0;
    int16_t mountainX = 0;
    int16_t mountainY = 0;
    int16_t mountainW = 0;
    int16_t mountainH = 0;
};

static DirectGpsPosterLayout makeDirectGpsPosterLayout(int16_t width, int16_t height)
{
    DirectGpsPosterLayout layout;
    layout.tiny = (width <= 176 || height <= 96);

    layout.globeR = (height * (layout.tiny ? 33 : 44)) / 100;
    if (layout.globeR < (layout.tiny ? 20 : 28)) {
        layout.globeR = layout.tiny ? 20 : 28;
    }
    layout.globeCx = (width * (layout.tiny ? 84 : 77)) / 100;
    const int16_t globeCxMin = layout.globeR + 2;
    const int16_t globeCxMax = width - std::max<int16_t>(2, layout.globeR / 5);
    if (layout.globeCx < globeCxMin) {
        layout.globeCx = globeCxMin;
    }
    if (layout.globeCx > globeCxMax) {
        layout.globeCx = globeCxMax;
    }

    layout.globeCy = (height * (layout.tiny ? 52 : 53)) / 100;
    const int16_t globeCyMin = layout.globeR + 2;
    const int16_t globeCyMax = height - std::max<int16_t>(2, layout.globeR / 6);
    if (layout.globeCy < globeCyMin) {
        layout.globeCy = globeCyMin;
    }
    if (layout.globeCy > globeCyMax) {
        layout.globeCy = globeCyMax;
    }

    layout.leftX = layout.tiny ? 4 : 10;
    int16_t leftRight = layout.globeCx - layout.globeR - (layout.tiny ? 2 : 10);
    if (leftRight < layout.leftX + 40) {
        leftRight = layout.leftX + 40;
    }
    if (leftRight > width - 4) {
        leftRight = width - 4;
    }
    layout.leftW = leftRight - layout.leftX;
    if (layout.leftW < 38) {
        layout.leftW = 38;
    }

    layout.titleY = (height * (layout.tiny ? 8 : 12)) / 100;
    layout.coordY1 = layout.titleY + (layout.tiny ? 19 : 26);
    layout.coordY2 = layout.coordY1 + (layout.tiny ? 18 : 28);
    layout.altitudeY = layout.coordY2 + (layout.tiny ? 18 : 22);
    const int16_t maxAltitudeY = height - (layout.tiny ? 15 : 20);
    if (layout.altitudeY > maxAltitudeY) {
        const int16_t shiftUp = layout.altitudeY - maxAltitudeY;
        layout.coordY1 -= shiftUp;
        layout.coordY2 -= shiftUp;
        layout.altitudeY -= shiftUp;
    }
    const int16_t minCoordY1 = layout.titleY + (layout.tiny ? 16 : 22);
    if (layout.coordY1 < minCoordY1) {
        const int16_t shiftDown = minCoordY1 - layout.coordY1;
        layout.coordY1 += shiftDown;
        layout.coordY2 += shiftDown;
        layout.altitudeY += shiftDown;
    }

    layout.mountainW = layout.tiny ? 30 : 46;
    layout.mountainH = layout.tiny ? 14 : 22;
    layout.mountainX = layout.leftX + (layout.tiny ? 2 : 4);
    const int16_t altitudeVisualH = kPattanakarnClockHalfHeight;
    // Vertically align the mountain icon with the altitude text row.
    layout.mountainY = layout.altitudeY + ((altitudeVisualH - layout.mountainH) / 2) + (layout.tiny ? 1 : 2);
    const int16_t minMountainY = layout.coordY2 + (layout.tiny ? 10 : 14);
    if (layout.mountainY < minMountainY) {
        layout.mountainY = minMountainY;
    }
    const int16_t maxMountainY = std::max<int16_t>(0, height - layout.mountainH - 1);
    if (layout.mountainY > maxMountainY) {
        layout.mountainY = maxMountainY;
    }

    return layout;
}

static void renderDirectGpsPosterCoordinateNeon(TFTDisplay *tft, int16_t width, int16_t height, const GPSStatus *gps)
{
    if (!tft || width <= 0 || height <= 0) {
        return;
    }

    const DirectGpsPosterLayout layout = makeDirectGpsPosterLayout(width, height);
    const bool tinyPosterLayout = layout.tiny;
    const bool gpsEnabled = config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_ENABLED;
    const bool hasGpsCoordinates =
        gps && gpsEnabled && (config.position.fixed_position || (gps->getIsConnected() && gps->getHasLock()));
    // Keep WGS84-style full decimal coordinates readable on small TFT by shrinking glyph size only.
    const int16_t coordTracking = 0;
    const int16_t altitudeTracking = tinyPosterLayout ? 1 : 1;
    const bool coordHalfScale = true;
    const bool altitudeHalfScale = true;
    const int16_t neonMargin = 0;
    const int16_t altitudeMargin = tinyPosterLayout ? 1 : 2;
    const int16_t leftRegionMinX = layout.leftX + neonMargin;
    const int16_t leftRegionMaxX = layout.leftX + layout.leftW - neonMargin;
    const int16_t leftCenterX = layout.leftX + (layout.leftW / 2);

    double lat = 0.0;
    double lon = 0.0;
    if (hasGpsCoordinates) {
        lat = static_cast<double>(gps->getLatitude()) * 1e-7;
        lon = static_cast<double>(gps->getLongitude()) * 1e-7;
    }

    char lonLine[24];
    char latLine[24];
    char altitudeLine[16];
    auto writeCoordPlaceholder = [](char *out, size_t outSize, int wholeDigits, int decimals) {
        if (!out || outSize == 0) {
            return;
        }
        size_t pos = 0;
        for (int i = 0; i < wholeDigits && pos < (outSize - 1); ++i) {
            out[pos++] = '-';
        }
        if (pos < (outSize - 1)) {
            out[pos++] = '.';
        }
        for (int i = 0; i < decimals && pos < (outSize - 1); ++i) {
            out[pos++] = '-';
        }
        out[pos] = '\0';
    };

    int16_t lonClockW = 0;
    int16_t latClockW = 0;
    const int16_t coordUsableW = std::max<int16_t>(44, layout.leftW - (neonMargin * 2));
    auto measureCoordText = [&](const char *line) { return measurePattanakarnClockTextTracked(line, coordHalfScale, coordTracking); };
    if (hasGpsCoordinates) {
        int coordDecimals = -1;
        for (int decimals = 7; decimals >= 2; --decimals) {
            snprintf(lonLine, sizeof(lonLine), "%.*f", decimals, lon);
            snprintf(latLine, sizeof(latLine), "%.*f", decimals, lat);
            lonClockW = measureCoordText(lonLine);
            latClockW = measureCoordText(latLine);
            if (lonClockW > 0 && latClockW > 0 && lonClockW <= coordUsableW && latClockW <= coordUsableW) {
                coordDecimals = decimals;
                break;
            }
        }
        if (coordDecimals < 0) {
            snprintf(lonLine, sizeof(lonLine), "%.2f", lon);
            snprintf(latLine, sizeof(latLine), "%.2f", lat);
            lonClockW = measureCoordText(lonLine);
            latClockW = measureCoordText(latLine);
        }
    } else {
        writeCoordPlaceholder(lonLine, sizeof(lonLine), 2, 2);
        writeCoordPlaceholder(latLine, sizeof(latLine), 2, 2);
        lonClockW = measureCoordText(lonLine);
        latClockW = measureCoordText(latLine);
    }

    if (lonClockW <= 0 || latClockW <= 0) {
        return;
    }

    const int32_t altitudeMeters = (hasGpsCoordinates && gps) ? gps->getAltitude() : INT32_MIN;
    if (altitudeMeters == INT32_MIN) {
        snprintf(altitudeLine, sizeof(altitudeLine), "--m");
    } else {
        snprintf(altitudeLine, sizeof(altitudeLine), "%ldm", static_cast<long>(altitudeMeters));
    }
    const int16_t altitudeW = measurePattanakarnClockTextTracked(altitudeLine, altitudeHalfScale, altitudeTracking);

    auto clampLineX = [&](int16_t wantedX, int16_t lineW) {
        const int16_t minX = leftRegionMinX;
        const int16_t maxX = leftRegionMaxX - lineW;
        if (maxX <= minX) {
            return minX;
        }
        if (wantedX < minX) {
            return minX;
        }
        if (wantedX > maxX) {
            return maxX;
        }
        return wantedX;
    };

    const int16_t lonX = clampLineX(leftCenterX - (lonClockW / 2), lonClockW);
    const int16_t latX = clampLineX(leftCenterX - (latClockW / 2), latClockW);

    int16_t altitudeX = clampLineX(layout.mountainX + layout.mountainW + (tinyPosterLayout ? 6 : 8), altitudeW);
    if (altitudeW > 0 && (altitudeX + altitudeW) > leftRegionMaxX) {
        altitudeX = clampLineX(leftCenterX - (altitudeW / 2), altitudeW);
    }

    // Decor is fully repainted before text, so keep background intact to preserve raster lines behind neon.

    renderDirectNeonPattanakarnText(
        tft, lonX, layout.coordY1, lonLine, coordHalfScale, mapDirectGpsWarmLayerColor, false, coordTracking);
    renderDirectNeonPattanakarnText(
        tft, latX, layout.coordY2, latLine, coordHalfScale, mapDirectGpsWarmLayerColor, false, coordTracking);
    if (altitudeW > 0) {
        renderDirectNeonPattanakarnText(
            tft, altitudeX, layout.altitudeY, altitudeLine, altitudeHalfScale, mapDirectGpsWarmLayerColor, false, altitudeTracking,
            altitudeMargin);
    }
}

static void directFillCircle565(TFTDisplay *tft, int16_t displayW, int16_t displayH, int16_t cx, int16_t cy, int16_t radius, uint16_t color)
{
    if (!tft || radius <= 0 || displayW <= 0 || displayH <= 0) {
        return;
    }

    const int32_t rr = static_cast<int32_t>(radius) * static_cast<int32_t>(radius);
    for (int16_t yy = -radius; yy <= radius; ++yy) {
        const int16_t py = cy + yy;
        if (py < 0 || py >= displayH) {
            continue;
        }
        const int32_t dy2 = static_cast<int32_t>(yy) * static_cast<int32_t>(yy);
        const int32_t dx2 = rr - dy2;
        if (dx2 < 0) {
            continue;
        }
        int16_t dx = static_cast<int16_t>(sqrtf(static_cast<float>(dx2)));
        int16_t x0 = cx - dx;
        int16_t x1 = cx + dx;
        if (x1 < 0 || x0 >= displayW) {
            continue;
        }
        if (x0 < 0) {
            x0 = 0;
        }
        if (x1 >= displayW) {
            x1 = displayW - 1;
        }
        const int16_t runW = x1 - x0 + 1;
        if (runW > 0) {
            tft->fillRect565(x0, py, runW, 1, color);
        }
    }
}

static void directDrawLine565(TFTDisplay *tft,
                              int16_t displayW,
                              int16_t displayH,
                              int16_t x0,
                              int16_t y0,
                              int16_t x1,
                              int16_t y1,
                              uint16_t color)
{
    if (!tft) {
        return;
    }

    int16_t x = x0;
    int16_t y = y0;
    const int16_t dx = std::abs(x1 - x0);
    const int16_t sx = (x0 < x1) ? 1 : -1;
    const int16_t dy = -std::abs(y1 - y0);
    const int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx + dy;

    while (true) {
        if (x >= 0 && x < displayW && y >= 0 && y < displayH) {
            tft->drawPixel565(x, y, color);
        }
        if (x == x1 && y == y1) {
            break;
        }
        const int16_t e2 = static_cast<int16_t>(2 * err);
        if (e2 >= dy) {
            err += dy;
            x += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y += sy;
        }
    }
}

static void directDrawThickLine565(TFTDisplay *tft,
                                   int16_t displayW,
                                   int16_t displayH,
                                   int16_t x0,
                                   int16_t y0,
                                   int16_t x1,
                                   int16_t y1,
                                   int16_t thickness,
                                   uint16_t color)
{
    if (!tft || thickness <= 0) {
        return;
    }

    if (thickness <= 1) {
        directDrawLine565(tft, displayW, displayH, x0, y0, x1, y1, color);
        return;
    }

    const int16_t dx = x1 - x0;
    const int16_t dy = y1 - y0;
    const int16_t steps = std::max<int16_t>(std::abs(dx), std::abs(dy));
    const int16_t dotR = std::max<int16_t>(1, thickness / 2);
    if (steps <= 0) {
        directFillCircle565(tft, displayW, displayH, x0, y0, dotR, color);
        return;
    }

    for (int16_t i = 0; i <= steps; ++i) {
        const int16_t px = x0 + (dx * i) / steps;
        const int16_t py = y0 + (dy * i) / steps;
        directFillCircle565(tft, displayW, displayH, px, py, dotR, color);
    }
}

static void directDrawNeonLine565(TFTDisplay *tft,
                                  int16_t displayW,
                                  int16_t displayH,
                                  int16_t x0,
                                  int16_t y0,
                                  int16_t x1,
                                  int16_t y1,
                                  uint16_t coreColor,
                                  uint16_t glowNearColor,
                                  uint16_t glowFarColor,
                                  int16_t coreThickness,
                                  int16_t glowNearThickness,
                                  int16_t glowFarThickness)
{
    if (!tft) {
        return;
    }
    if (glowFarThickness > 0) {
        directDrawThickLine565(tft, displayW, displayH, x0, y0, x1, y1, glowFarThickness, glowFarColor);
    }
    if (glowNearThickness > 0) {
        directDrawThickLine565(tft, displayW, displayH, x0, y0, x1, y1, glowNearThickness, glowNearColor);
    }
    if (coreThickness > 0) {
        directDrawThickLine565(tft, displayW, displayH, x0, y0, x1, y1, coreThickness, coreColor);
    }
}

static bool makeDirectDrawClipRect(int16_t x,
                                   int16_t y,
                                   int16_t w,
                                   int16_t h,
                                   int16_t displayW,
                                   int16_t displayH,
                                   DirectDrawClipRect &outClip)
{
    if (w <= 0 || h <= 0 || displayW <= 0 || displayH <= 0) {
        outClip.enabled = false;
        return false;
    }
    int16_t minX = x;
    int16_t minY = y;
    int16_t maxX = x + w - 1;
    int16_t maxY = y + h - 1;
    if (maxX < 0 || maxY < 0 || minX >= displayW || minY >= displayH) {
        outClip.enabled = false;
        return false;
    }
    if (minX < 0) {
        minX = 0;
    }
    if (minY < 0) {
        minY = 0;
    }
    if (maxX >= displayW) {
        maxX = displayW - 1;
    }
    if (maxY >= displayH) {
        maxY = displayH - 1;
    }
    outClip.enabled = true;
    outClip.minX = minX;
    outClip.maxX = maxX;
    outClip.minY = minY;
    outClip.maxY = maxY;
    return true;
}

static bool directClipContains(const DirectDrawClipRect *clip, int16_t x, int16_t y)
{
    if (!clip || !clip->enabled) {
        return true;
    }
    return x >= clip->minX && x <= clip->maxX && y >= clip->minY && y <= clip->maxY;
}

static void directFillCircle565Clipped(TFTDisplay *tft,
                                       int16_t displayW,
                                       int16_t displayH,
                                       int16_t cx,
                                       int16_t cy,
                                       int16_t radius,
                                       uint16_t color,
                                       const DirectDrawClipRect *clip)
{
    if (!tft || radius <= 0 || displayW <= 0 || displayH <= 0) {
        return;
    }

    const int32_t rr = static_cast<int32_t>(radius) * static_cast<int32_t>(radius);
    for (int16_t yy = -radius; yy <= radius; ++yy) {
        const int16_t py = cy + yy;
        if (py < 0 || py >= displayH) {
            continue;
        }
        if (clip && clip->enabled && (py < clip->minY || py > clip->maxY)) {
            continue;
        }

        const int32_t dy2 = static_cast<int32_t>(yy) * static_cast<int32_t>(yy);
        const int32_t dx2 = rr - dy2;
        if (dx2 < 0) {
            continue;
        }
        int16_t dx = static_cast<int16_t>(sqrtf(static_cast<float>(dx2)));
        int16_t x0 = cx - dx;
        int16_t x1 = cx + dx;
        if (x1 < 0 || x0 >= displayW) {
            continue;
        }
        if (x0 < 0) {
            x0 = 0;
        }
        if (x1 >= displayW) {
            x1 = displayW - 1;
        }
        if (clip && clip->enabled) {
            if (x1 < clip->minX || x0 > clip->maxX) {
                continue;
            }
            if (x0 < clip->minX) {
                x0 = clip->minX;
            }
            if (x1 > clip->maxX) {
                x1 = clip->maxX;
            }
        }
        const int16_t runW = x1 - x0 + 1;
        if (runW > 0) {
            tft->fillRect565(x0, py, runW, 1, color);
        }
    }
}

static void directFillRect565Clipped(TFTDisplay *tft,
                                     int16_t displayW,
                                     int16_t displayH,
                                     int16_t x,
                                     int16_t y,
                                     int16_t w,
                                     int16_t h,
                                     uint16_t color,
                                     const DirectDrawClipRect *clip)
{
    if (!tft || displayW <= 0 || displayH <= 0 || w <= 0 || h <= 0) {
        return;
    }

    int16_t x0 = x;
    int16_t y0 = y;
    int16_t x1 = x + w - 1;
    int16_t y1 = y + h - 1;
    if (clip && clip->enabled) {
        x0 = std::max<int16_t>(x0, clip->minX);
        y0 = std::max<int16_t>(y0, clip->minY);
        x1 = std::min<int16_t>(x1, clip->maxX);
        y1 = std::min<int16_t>(y1, clip->maxY);
    }
    x0 = std::max<int16_t>(0, x0);
    y0 = std::max<int16_t>(0, y0);
    x1 = std::min<int16_t>(static_cast<int16_t>(displayW - 1), x1);
    y1 = std::min<int16_t>(static_cast<int16_t>(displayH - 1), y1);
    if (x0 > x1 || y0 > y1) {
        return;
    }
    tft->fillRect565(x0, y0, x1 - x0 + 1, y1 - y0 + 1, color);
}

static void directDrawLine565Clipped(TFTDisplay *tft,
                                     int16_t displayW,
                                     int16_t displayH,
                                     int16_t x0,
                                     int16_t y0,
                                     int16_t x1,
                                     int16_t y1,
                                     uint16_t color,
                                     const DirectDrawClipRect *clip)
{
    if (!tft) {
        return;
    }

    int16_t x = x0;
    int16_t y = y0;
    const int16_t dx = std::abs(x1 - x0);
    const int16_t sx = (x0 < x1) ? 1 : -1;
    const int16_t dy = -std::abs(y1 - y0);
    const int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx + dy;

    while (true) {
        if (x >= 0 && x < displayW && y >= 0 && y < displayH && directClipContains(clip, x, y)) {
            tft->drawPixel565(x, y, color);
        }
        if (x == x1 && y == y1) {
            break;
        }
        const int16_t e2 = static_cast<int16_t>(2 * err);
        if (e2 >= dy) {
            err += dy;
            x += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y += sy;
        }
    }
}

static void directDrawThickLine565Clipped(TFTDisplay *tft,
                                          int16_t displayW,
                                          int16_t displayH,
                                          int16_t x0,
                                          int16_t y0,
                                          int16_t x1,
                                          int16_t y1,
                                          int16_t thickness,
                                          uint16_t color,
                                          const DirectDrawClipRect *clip)
{
    if (!tft || thickness <= 0) {
        return;
    }

    if (thickness <= 1) {
        directDrawLine565Clipped(tft, displayW, displayH, x0, y0, x1, y1, color, clip);
        return;
    }

    const int16_t dx = x1 - x0;
    const int16_t dy = y1 - y0;
    const int16_t steps = std::max<int16_t>(std::abs(dx), std::abs(dy));
    const int16_t dotR = std::max<int16_t>(1, thickness / 2);
    if (steps <= 0) {
        directFillCircle565Clipped(tft, displayW, displayH, x0, y0, dotR, color, clip);
        return;
    }

    for (int16_t i = 0; i <= steps; ++i) {
        const int16_t px = x0 + (dx * i) / steps;
        const int16_t py = y0 + (dy * i) / steps;
        directFillCircle565Clipped(tft, displayW, displayH, px, py, dotR, color, clip);
    }
}

static void directDrawNeonLine565Clipped(TFTDisplay *tft,
                                         int16_t displayW,
                                         int16_t displayH,
                                         int16_t x0,
                                         int16_t y0,
                                         int16_t x1,
                                         int16_t y1,
                                         uint16_t coreColor,
                                         uint16_t glowNearColor,
                                         uint16_t glowFarColor,
                                         int16_t coreThickness,
                                         int16_t glowNearThickness,
                                         int16_t glowFarThickness,
                                         const DirectDrawClipRect *clip)
{
    if (!tft) {
        return;
    }
    if (glowFarThickness > 0) {
        directDrawThickLine565Clipped(tft, displayW, displayH, x0, y0, x1, y1, glowFarThickness, glowFarColor, clip);
    }
    if (glowNearThickness > 0) {
        directDrawThickLine565Clipped(tft, displayW, displayH, x0, y0, x1, y1, glowNearThickness, glowNearColor, clip);
    }
    if (coreThickness > 0) {
        directDrawThickLine565Clipped(tft, displayW, displayH, x0, y0, x1, y1, coreThickness, coreColor, clip);
    }
}

struct DirectHomeOrbLayout {
    bool tiny = false;
    int16_t boxX = 0;
    int16_t boxY = 0;
    int16_t boxW = 0;
    int16_t boxH = 0;
    int16_t cx = 0;
    int16_t cy = 0;
    int16_t boundsX = 0;
    int16_t boundsY = 0;
    int16_t boundsW = 0;
    int16_t boundsH = 0;
};

static DirectHomeOrbLayout makeDirectHomeOrbLayout(int16_t width, int16_t height)
{
    DirectHomeOrbLayout out;
    out.tiny = (width <= 176 || height <= 96);
    // Keep the mesh in a fixed right-bottom box so it never drifts into other UI areas.
    out.boxW = out.tiny ? 54 : 108;
    out.boxH = out.tiny ? 42 : 92;
    out.boxX = width - out.boxW - (out.tiny ? 4 : 10);
    out.boxY = height - out.boxH - (out.tiny ? 4 : 10);
    if (out.boxX < 2) {
        out.boxX = 2;
    }
    if (out.boxY < 2) {
        out.boxY = 2;
    }

    out.cx = out.boxX + (out.boxW / 2);
    out.cy = out.boxY + (out.boxH / 2);

    const int16_t glowPad = out.tiny ? 5 : 7;
    const int16_t halfWPeak = static_cast<int16_t>(((static_cast<int32_t>(out.boxW) * 130) + 99) / 200);
    const int16_t halfHPeak = static_cast<int16_t>(((static_cast<int32_t>(out.boxH) * 130) + 99) / 200);
    out.boundsX = out.cx - halfWPeak - glowPad;
    out.boundsY = out.cy - halfHPeak - glowPad;
    out.boundsW = (halfWPeak + glowPad) * 2 + 1;
    out.boundsH = (halfHPeak + glowPad) * 2 + 1;
    return out;
}

static bool getDirectHomeOrbBoundsRect(int16_t width, int16_t height, int16_t &x, int16_t &y, int16_t &w, int16_t &h)
{
    if (width <= 0 || height <= 0) {
        return false;
    }
    const DirectHomeOrbLayout layout = makeDirectHomeOrbLayout(width, height);
    DirectDrawClipRect clip;
    if (!makeDirectDrawClipRect(layout.boundsX, layout.boundsY, layout.boundsW, layout.boundsH, width, height, clip)) {
        return false;
    }
    x = clip.minX;
    y = clip.minY;
    w = clip.maxX - clip.minX + 1;
    h = clip.maxY - clip.minY + 1;
    return w > 0 && h > 0;
}

static void renderDirectHomeOrangeMesh(TFTDisplay *tft,
                                       int16_t width,
                                       int16_t height,
                                       const DirectDrawClipRect *clip = nullptr,
                                       bool clearClip = false,
                                       uint8_t pulsePercent = 100)
{
    if (!tft || width <= 0 || height <= 0) {
        return;
    }

    const uint16_t black = TFTDisplay::rgb565(0x00, 0x00, 0x00);
    const DirectHomeOrbLayout layout = makeDirectHomeOrbLayout(width, height);
    if (layout.boxW <= 0 || layout.boxH <= 0) {
        return;
    }

    if (clip && clip->enabled) {
        const int16_t bx1 = layout.boundsX + layout.boundsW - 1;
        const int16_t by1 = layout.boundsY + layout.boundsH - 1;
        if (bx1 < clip->minX || layout.boundsX > clip->maxX || by1 < clip->minY || layout.boundsY > clip->maxY) {
            return;
        }
    }

    if (clearClip) {
        if (clip && clip->enabled) {
            const int16_t clipW = clip->maxX - clip->minX + 1;
            const int16_t clipH = clip->maxY - clip->minY + 1;
            if (clipW > 0 && clipH > 0) {
                tft->fillRect565(clip->minX, clip->minY, clipW, clipH, black);
            }
        } else {
            DirectDrawClipRect clearRect;
            if (makeDirectDrawClipRect(layout.boundsX, layout.boundsY, layout.boundsW, layout.boundsH, width, height, clearRect)) {
                tft->fillRect565(clearRect.minX, clearRect.minY, clearRect.maxX - clearRect.minX + 1, clearRect.maxY - clearRect.minY + 1, black);
            }
        }
    }

    if (pulsePercent < 100) {
        pulsePercent = 100;
    }
    if (pulsePercent > 130) {
        pulsePercent = 130;
    }
    const int16_t orbR = std::max<int16_t>(4, (std::min<int16_t>(layout.boxW, layout.boxH) * pulsePercent) / 280);

    static constexpr int16_t kBloomLayers = 20;
    static constexpr int16_t kBloomOuterPercent = 5;
    static constexpr int16_t kBloomInnerPercent = 34;
    const int16_t innerExtra = layout.tiny ? 1 : 2;
    const int16_t extraStep = 1;
    for (int16_t layer = kBloomLayers - 1; layer >= 0; --layer) {
        const int16_t radius = orbR + innerExtra + (layer * extraStep);
        const int16_t numerator = (kBloomInnerPercent - kBloomOuterPercent) * (kBloomLayers - 1 - layer);
        const int16_t percent =
            static_cast<int16_t>(kBloomOuterPercent + ((numerator + ((kBloomLayers - 1) / 2)) / (kBloomLayers - 1)));
        const uint8_t r = static_cast<uint8_t>((0xFFU * percent) / 100U);
        const uint8_t g = static_cast<uint8_t>((0x8AU * percent) / 100U);
        const uint8_t b = static_cast<uint8_t>((0x1FU * percent) / 100U);
        directFillCircle565Clipped(tft, width, height, layout.cx, layout.cy, radius, TFTDisplay::rgb565(r, g, b), clip);
    }

    struct OrbPoint {
        int8_t x;
        int8_t y;
    };
    // Hand-traced from the provided reference polyhedron.
    static constexpr OrbPoint kPts[] = {
        {0, 42},   // 0 left-mid
        {7, 94},   // 1 left-bottom
        {21, 15},  // 2 top-left
        {36, 24},  // 3 upper-mid
        {43, 51},  // 4 center
        {35, 62},  // 5 mid-left-lower
        {45, 86},  // 6 bottom-mid
        {78, 0},   // 7 top-right
        {63, 42},  // 8 center-right
        {78, 31},  // 9 right-upper-mid
        {88, 56},  // 10 right-mid
        {77, 67},  // 11 right-mid-lower
        {76, 100}, // 12 bottom-right
    };
    struct OrbEdge {
        uint8_t a;
        uint8_t b;
    };
    static constexpr OrbEdge kEdges[] = {
        {0, 1},  {1, 12}, {12, 7}, {7, 2},  {2, 0}, // outer shell
        {7, 9},  {9, 11}, {11, 12},                  // right spine
        {2, 3},  {3, 7},  {2, 9},  {3, 9},           // top cap
        {0, 4},  {0, 8},                              // left fan
        {2, 4},  {2, 5},                              // upper-left internals
        {3, 4},                                       // crown bridge
        {4, 5},  {5, 6},  {4, 6},                     // center pillar
        {4, 8},  {4, 9},  {4, 10}, {4, 12},           // center to right
        {5, 1},  {6, 1},                              // left-bottom ties
        {6, 12},                                      // bottom tie
        {8, 9},  {8, 10}, {8, 11},                    // right upper web
        {10, 11}, {10, 12}, {11, 12},                 // right lower web
        {3, 6},                                       // long diagonal
    };

    const uint16_t edgeCore = TFTDisplay::rgb565(0xF7, 0x8A, 0x1F);
    const uint16_t edgeGlowNear = TFTDisplay::rgb565(0xC4, 0x62, 0x16);
    const uint16_t edgeGlowFar = TFTDisplay::rgb565(0x6A, 0x2E, 0x0B);
    int16_t sx[sizeof(kPts) / sizeof(kPts[0])] = {0};
    int16_t sy[sizeof(kPts) / sizeof(kPts[0])] = {0};
    for (size_t i = 0; i < (sizeof(kPts) / sizeof(kPts[0])); ++i) {
        const int16_t px = layout.boxX + static_cast<int16_t>((static_cast<int32_t>(layout.boxW) * kPts[i].x) / 100);
        const int16_t py = layout.boxY + static_cast<int16_t>((static_cast<int32_t>(layout.boxH) * kPts[i].y) / 100);
        sx[i] = layout.cx + static_cast<int16_t>((static_cast<int32_t>(px - layout.cx) * pulsePercent) / 100);
        sy[i] = layout.cy + static_cast<int16_t>((static_cast<int32_t>(py - layout.cy) * pulsePercent) / 100);
    }
    for (size_t i = 0; i < (sizeof(kEdges) / sizeof(kEdges[0])); ++i) {
        const OrbEdge &e = kEdges[i];
        directDrawNeonLine565Clipped(tft, width, height, sx[e.a], sy[e.a], sx[e.b], sy[e.b], edgeCore, edgeGlowNear, edgeGlowFar, 1, 1, 2, clip);
        if ((i & 0x07) == 0) {
            yield();
            esp_task_wdt_reset();
        }
    }

    const uint16_t nodeCore = TFTDisplay::rgb565(0xD8, 0xFF, 0x00);
    const uint16_t nodeGlow = TFTDisplay::rgb565(0x7A, 0x8E, 0x10);
    const int16_t nodeR = layout.tiny ? 2 : 3;
    const int16_t nodeGlowR = nodeR + 1;
    for (size_t i = 0; i < (sizeof(kPts) / sizeof(kPts[0])); ++i) {
        directFillCircle565Clipped(tft, width, height, sx[i], sy[i], nodeGlowR, nodeGlow, clip);
        directFillCircle565Clipped(tft, width, height, sx[i], sy[i], nodeR, nodeCore, clip);
    }
}

static bool repaintDirectHomeClockMeshRegion(TFTDisplay *tft, int16_t width, int16_t height, int16_t originY)
{
    if (!tft || width <= 0 || height <= 0) {
        return false;
    }

    int16_t regionX = 0;
    int16_t regionY = 0;
    int16_t regionW = 0;
    int16_t regionH = 0;
    int16_t frameX = 0;
    if (!getDirectNeonClockRegionRect(width, originY, regionX, regionY, regionW, regionH, frameX)) {
        return false;
    }

    // Home mesh is constrained to the lower band; timer region on top never needs mesh repaint.
    const int16_t meshTopY = static_cast<int16_t>((height * 52) / 100);
    if ((regionY + regionH) <= meshTopY) {
        return false;
    }

    DirectDrawClipRect clip;
    if (!makeDirectDrawClipRect(regionX, regionY, regionW, regionH, width, height, clip)) {
        return false;
    }
    renderDirectHomeOrangeMesh(tft, width, height, &clip, false);
    return true;
}

static void renderDirectGpsGlobeBloom(TFTDisplay *tft,
                                      int16_t width,
                                      int16_t height,
                                      int16_t globeCx,
                                      int16_t globeCy,
                                      int16_t globeR,
                                      bool tinyLayout)
{
    if (!tft || width <= 0 || height <= 0 || globeR <= 6) {
        return;
    }

    // Dense white bloom: 40 layers with very soft falloff to 1% at the outer rim.
    static constexpr int kBloomLayers = 40;
    const int16_t innerExtra = tinyLayout ? 5 : 8;
    const int16_t extraStep = tinyLayout ? 1 : 2;

    static constexpr int16_t kBloomInnerPercent = 72;
    static constexpr int16_t kBloomOuterPercent = 1;
    for (int layer = kBloomLayers - 1; layer >= 0; --layer) { // draw outer -> inner
        const int16_t radius = globeR + innerExtra + (layer * extraStep);
        if (radius <= 0) {
            continue;
        }
        const int16_t numerator = (kBloomInnerPercent - kBloomOuterPercent) * (kBloomLayers - 1 - layer);
        const int16_t percent = static_cast<int16_t>(kBloomOuterPercent + ((numerator + ((kBloomLayers - 1) / 2)) / (kBloomLayers - 1)));
        const uint8_t v = static_cast<uint8_t>((255U * percent) / 100U);
        const uint16_t color = TFTDisplay::rgb565(v, v, v);
        directFillCircle565(tft, width, height, globeCx, globeCy, radius, color);
    }
}

static void renderDirectGpsWireGlobe(TFTDisplay *tft,
                                     int16_t width,
                                     int16_t height,
                                     int16_t globeCx,
                                     int16_t globeCy,
                                     int16_t globeR,
                                     bool tinyLayout)
{
    if (!tft || width <= 0 || height <= 0 || globeR <= 6) {
        return;
    }

    static constexpr int kMaxLonSegments = 14;
    static constexpr int kMaxLatSegments = 8;
    const int kLonSegments = tinyLayout ? 10 : kMaxLonSegments;
    const int kLatSegments = tinyLayout ? 6 : kMaxLatSegments;
    struct Point3D {
        int16_t x = 0;
        int16_t y = 0;
        float z = 0.0f;
    };
    static Point3D pts[kMaxLatSegments + 1][kMaxLonSegments];

    const float rotY = -0.55f;
    const float tiltX = 0.45f;
    const float cosY = cosf(rotY);
    const float sinY = sinf(rotY);
    const float cosX = cosf(tiltX);
    const float sinX = sinf(tiltX);

    const uint16_t lineCoreFront = TFTDisplay::rgb565(0xFF, 0xFF, 0xFF);
    const uint16_t lineCoreBack = TFTDisplay::rgb565(0xD8, 0xDE, 0xE8);
    const uint16_t nodeCore = TFTDisplay::rgb565(0xFF, 0xFF, 0xFF);

    for (int latIdx = 0; latIdx <= kLatSegments; ++latIdx) {
        const float v = static_cast<float>(latIdx) / static_cast<float>(kLatSegments);
        const float lat = (-0.5f * PI) + (v * PI);
        const float cosLat = cosf(lat);
        const float sinLat = sinf(lat);
        for (int lonIdx = 0; lonIdx < kLonSegments; ++lonIdx) {
            const float u = static_cast<float>(lonIdx) / static_cast<float>(kLonSegments);
            const float lon = u * 2.0f * PI;

            float x = cosLat * cosf(lon);
            float y = sinLat;
            float z = cosLat * sinf(lon);

            const float xr = (x * cosY) + (z * sinY);
            const float zr = (-x * sinY) + (z * cosY);
            const float yr = (y * cosX) - (zr * sinX);
            const float zz = (y * sinX) + (zr * cosX);

            pts[latIdx][lonIdx].x = globeCx + static_cast<int16_t>(lrintf(xr * globeR));
            pts[latIdx][lonIdx].y = globeCy + static_cast<int16_t>(lrintf(yr * globeR));
            pts[latIdx][lonIdx].z = zz;
        }
        yield();
    }

    auto drawEdge = [&](const Point3D &a, const Point3D &b) {
        const float zAvg = (a.z + b.z) * 0.5f;
        const bool backFace = zAvg < -0.12f;
        const uint16_t core = backFace ? lineCoreBack : lineCoreFront;
        directDrawLine565(tft, width, height, a.x, a.y, b.x, b.y, core);
    };

    for (int latIdx = 0; latIdx <= kLatSegments; ++latIdx) {
        for (int lonIdx = 0; lonIdx < kLonSegments; ++lonIdx) {
            const int nextLon = (lonIdx + 1) % kLonSegments;
            drawEdge(pts[latIdx][lonIdx], pts[latIdx][nextLon]);
            if (latIdx < kLatSegments) {
                drawEdge(pts[latIdx][lonIdx], pts[latIdx + 1][lonIdx]);
                drawEdge(pts[latIdx][lonIdx], pts[latIdx + 1][nextLon]);
            }
        }
        yield();
    }

    for (int latIdx = 0; latIdx <= kLatSegments; ++latIdx) {
        for (int lonIdx = 0; lonIdx < kLonSegments; ++lonIdx) {
            const Point3D &p = pts[latIdx][lonIdx];
            if (p.x < -2 || p.x > (width + 2) || p.y < -2 || p.y > (height + 2)) {
                continue;
            }
            tft->drawPixel565(p.x, p.y, nodeCore);
        }
        yield();
    }
}

static uint16_t mapDirectGpsWarmLayerColor(uint8_t value)
{
    switch (value) {
    case 1:
        return TFTDisplay::rgb565(0x1A, 0x07, 0x00);
    case 2:
        return TFTDisplay::rgb565(0x34, 0x10, 0x00);
    case 3:
        return TFTDisplay::rgb565(0x64, 0x1E, 0x00);
    case 4:
        return TFTDisplay::rgb565(0xA8, 0x36, 0x00);
    case 5:
        return TFTDisplay::rgb565(0xFF, 0x6A, 0x00);
    case 6:
        return TFTDisplay::rgb565(0xFF, 0x9E, 0x4A);
    case 7:
        return TFTDisplay::rgb565(0xFF, 0xF2, 0xE6);
    default:
        return TFTDisplay::rgb565(0x00, 0x00, 0x00);
    }
}

static uint16_t mapDirectGpsCoolLayerColor(uint8_t value)
{
    switch (value) {
    case 1:
        return TFTDisplay::rgb565(0x03, 0x12, 0x2D);
    case 2:
        return TFTDisplay::rgb565(0x05, 0x30, 0x6B);
    case 3:
        return TFTDisplay::rgb565(0x00, 0x5E, 0xB6);
    case 4:
        return TFTDisplay::rgb565(0x20, 0xA6, 0xFF);
    case 5:
        return TFTDisplay::rgb565(0x73, 0xE6, 0xFF);
    case 6:
        return TFTDisplay::rgb565(0xCC, 0xFB, 0xFF);
    case 7:
        return TFTDisplay::rgb565(0xF6, 0xFF, 0xFF);
    default:
        return TFTDisplay::rgb565(0x00, 0x00, 0x00);
    }
}

static uint16_t mapDirectGpsAlertLayerColor(uint8_t value)
{
    switch (value) {
    case 1:
        return TFTDisplay::rgb565(0x28, 0x09, 0x0E);
    case 2:
        return TFTDisplay::rgb565(0x4E, 0x12, 0x1F);
    case 3:
        return TFTDisplay::rgb565(0x7E, 0x1B, 0x34);
    case 4:
        return TFTDisplay::rgb565(0xBE, 0x2D, 0x50);
    case 5:
        return TFTDisplay::rgb565(0xFF, 0x5E, 0x86);
    case 6:
        return TFTDisplay::rgb565(0xFF, 0xB4, 0xC8);
    case 7:
        return TFTDisplay::rgb565(0xFF, 0xEF, 0xF3);
    default:
        return TFTDisplay::rgb565(0x00, 0x00, 0x00);
    }
}

struct DirectGpsTitleGlyphPattern {
    char ch;
    const char *rows[7];
};

static const DirectGpsTitleGlyphPattern kDirectGpsTitleGlyphs[] = {
    {'G', {" ### ", "#   #", "#    ", "# ###", "#   #", "#   #", " ### "}},
    {'P', {"#### ", "#   #", "#   #", "#### ", "#    ", "#    ", "#    "}},
    {'S', {" ####", "#    ", "#    ", " ### ", "    #", "    #", "#### "}},
    {'O', {" ### ", "#   #", "#   #", "#   #", "#   #", "#   #", " ### "}},
    {'N', {"#   #", "##  #", "# # #", "#  ##", "#   #", "#   #", "#   #"}},
    {'F', {"#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#    "}},
};

static const DirectGpsTitleGlyphPattern *findDirectGpsTitleGlyph(char ch)
{
    for (const auto &glyph : kDirectGpsTitleGlyphs) {
        if (glyph.ch == ch) {
            return &glyph;
        }
    }
    return nullptr;
}

static int16_t measureDirectGpsTitleText(const char *text, int16_t scale, int16_t spacing)
{
    if (!text || scale <= 0) {
        return 0;
    }

    int16_t width = 0;
    bool hasGlyph = false;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        if (*cursor == ' ') {
            width += scale * 3;
            continue;
        }
        const auto *glyph = findDirectGpsTitleGlyph(*cursor);
        if (!glyph) {
            continue;
        }
        if (hasGlyph) {
            width += spacing;
        }
        width += scale * 5;
        hasGlyph = true;
    }
    return width;
}

static void stampDirectGpsTitleMask(uint8_t *mask,
                                    int16_t maskW,
                                    int16_t maskH,
                                    int16_t x,
                                    int16_t y,
                                    const char *text,
                                    int16_t scale,
                                    int16_t spacing)
{
    if (!mask || !text || maskW <= 0 || maskH <= 0 || scale <= 0) {
        return;
    }

    int16_t cursorX = x;
    bool hasGlyph = false;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        const char ch = *cursor;
        if (ch == ' ') {
            cursorX += scale * 3;
            continue;
        }
        const auto *glyph = findDirectGpsTitleGlyph(ch);
        if (!glyph) {
            continue;
        }

        if (hasGlyph) {
            cursorX += spacing;
        }

        for (int16_t row = 0; row < 7; ++row) {
            const char *line = glyph->rows[row];
            if (!line) {
                continue;
            }
            for (int16_t col = 0; col < 5; ++col) {
                if (line[col] == ' ') {
                    continue;
                }
                const int16_t px0 = cursorX + (col * scale);
                const int16_t py0 = y + (row * scale);
                for (int16_t sy = 0; sy < scale; ++sy) {
                    const int16_t py = py0 + sy;
                    if (py < 0 || py >= maskH) {
                        continue;
                    }
                    const int16_t rowBase = py * maskW;
                    for (int16_t sx = 0; sx < scale; ++sx) {
                        const int16_t px = px0 + sx;
                        if (px < 0 || px >= maskW) {
                            continue;
                        }
                        mask[rowBase + px] = 1;
                    }
                }
            }
        }

        cursorX += scale * 5;
        hasGlyph = true;
    }
}

static void renderDirectGpsPosterTitleWord(TFTDisplay *tft,
                                           int16_t displayW,
                                           int16_t displayH,
                                           int16_t drawX,
                                           int16_t drawY,
                                           const char *text,
                                           int16_t scale,
                                           uint16_t (*layerColorFn)(uint8_t))
{
    if (!tft || !text || !*text || !layerColorFn || scale <= 0) {
        return;
    }

    // Title words are only "GPS"/"ON"/"OFF" with scale 2-3, so we can keep a tight mask
    // budget and avoid ~47KB of always-on static BSS.
    static constexpr int16_t kMaxW = 96;
    static constexpr int16_t kMaxH = 48;
    if (!ensureDirectNeonBuffers() || !gDirectGpsTitleFullMask || !gDirectGpsTitleLayerMap) {
        return;
    }
    uint8_t *fullMask = gDirectGpsTitleFullMask;
    uint8_t *layerMap = gDirectGpsTitleLayerMap;

    const int16_t spacing = std::max<int16_t>(1, scale / 2);
    const int16_t coreW = measureDirectGpsTitleText(text, scale, spacing);
    const int16_t coreH = scale * 7;
    const int16_t margin = std::max<int16_t>(4, scale + 1);
    const int16_t regionX = drawX - margin;
    const int16_t regionY = drawY - margin;
    const int16_t regionW = coreW + (margin * 2);
    const int16_t regionH = coreH + (margin * 2);
    if (coreW <= 0 || coreH <= 0 || regionW <= 0 || regionH <= 0 || regionW > kMaxW || regionH > kMaxH) {
        return;
    }
    LOG_DEBUG("[DirectHome] gps title neon text='%s' scale=%d spacing=%d region=%dx%d", text, scale, spacing, regionW, regionH);

    memset(fullMask, 0, static_cast<size_t>(regionW * regionH));
    memset(layerMap, 0, static_cast<size_t>(regionW * regionH));

    stampDirectGpsTitleMask(fullMask, regionW, regionH, margin, margin, text, scale, spacing);

    const int16_t outerRadius = std::max<int16_t>(3, scale + 1);
    for (int16_t yy = 0; yy < regionH; ++yy) {
        for (int16_t xx = 0; xx < regionW; ++xx) {
            const int16_t idx = (yy * regionW) + xx;
            uint8_t value = 0;
            if (fullMask[idx] && isMaskInteriorPixel(fullMask, regionW, regionH, xx, yy)) {
                value = 7;
            } else if (fullMask[idx]) {
                value = 6;
            } else if (hasMaskPixelInRadius(fullMask, regionW, regionH, xx, yy, 1)) {
                value = 5;
            } else if (hasMaskPixelInRadius(fullMask, regionW, regionH, xx, yy, 2)) {
                value = 4;
            } else if (hasMaskPixelInRadius(fullMask, regionW, regionH, xx, yy, 3)) {
                value = 3;
            } else if (hasMaskPixelInRadius(fullMask, regionW, regionH, xx, yy, 4)) {
                value = 2;
            } else if (hasMaskPixelInRadius(fullMask, regionW, regionH, xx, yy, outerRadius)) {
                value = 1;
            }
            layerMap[idx] = value;
        }
    }

    for (int16_t row = 0; row < regionH; ++row) {
        const int16_t py = regionY + row;
        if (py < 0 || py >= displayH) {
            continue;
        }
        int16_t runStart = -1;
        uint8_t runValue = 0xFF;
        for (int16_t col = 0; col <= regionW; ++col) {
            const uint8_t value = (col < regionW) ? layerMap[(row * regionW) + col] : 0xFF;
            if (value == runValue) {
                continue;
            }
            if (runStart >= 0 && runValue > 0) {
                int16_t x0 = regionX + runStart;
                int16_t x1 = regionX + col - 1;
                if (!(x1 < 0 || x0 >= displayW)) {
                    if (x0 < 0) {
                        x0 = 0;
                    }
                    if (x1 >= displayW) {
                        x1 = displayW - 1;
                    }
                    const int16_t runW = x1 - x0 + 1;
                    if (runW > 0) {
                        tft->fillRect565(x0, py, runW, 1, layerColorFn(runValue));
                    }
                }
            }
            runValue = value;
            runStart = (col < regionW) ? col : -1;
        }
    }
}

static void renderDirectGpsPosterTitleNeon(TFTDisplay *tft, int16_t width, int16_t height)
{
    if (!tft || width <= 0 || height <= 0) {
        return;
    }

    const DirectGpsPosterLayout layout = makeDirectGpsPosterLayout(width, height);
    const bool tinyPosterLayout = layout.tiny;
    const bool gpsEnabled = config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_ENABLED;
    const char *gpsWord = "GPS";
    const char *stateWord = gpsEnabled ? "ON" : "OFF";

    // Small TFT title should be more compact (about 70% of previous size).
    const int16_t scale = tinyPosterLayout ? 2 : 3;
    const int16_t spacing = std::max<int16_t>(1, scale / 2);
    const int16_t gpsW = measureDirectGpsTitleText(gpsWord, scale, spacing);
    const int16_t stateW = measureDirectGpsTitleText(stateWord, scale, spacing);
    if (gpsW <= 0 || stateW <= 0) {
        return;
    }

    int16_t titleGap = width / 30;
    if (titleGap < (tinyPosterLayout ? 4 : 6)) {
        titleGap = tinyPosterLayout ? 4 : 6;
    }
    const int16_t titleW = gpsW + titleGap + stateW;
    const int16_t leftCenterX = layout.leftX + (layout.leftW / 2);
    int16_t titleX = leftCenterX - (titleW / 2);
    if (titleX < layout.leftX) {
        titleX = layout.leftX;
    }
    const int16_t maxTitleX = std::max<int16_t>(layout.leftX, layout.leftX + layout.leftW - titleW);
    if (titleX > maxTitleX) {
        titleX = maxTitleX;
    }
    const int16_t titleY = layout.titleY;
    // Do not hard-clear title background here; keep poster raster/waves visible behind neon, including GPS OFF.
    renderDirectGpsPosterTitleWord(tft, width, height, titleX, titleY, gpsWord, scale, mapDirectGpsWarmLayerColor);
    renderDirectGpsPosterTitleWord(
        tft, width, height, titleX + gpsW + titleGap, titleY, stateWord, scale, gpsEnabled ? mapDirectGpsCoolLayerColor : mapDirectGpsAlertLayerColor);
}

static void renderDirectGpsPosterDecor(TFTDisplay *tft, int16_t width, int16_t height, const GPSStatus *gps)
{
    if (!tft || width <= 0 || height <= 0) {
        return;
    }

    const DirectGpsPosterLayout layout = makeDirectGpsPosterLayout(width, height);
    const bool tinyPosterLayout = layout.tiny;
    const uint16_t black = TFTDisplay::rgb565(0x00, 0x00, 0x00);
    const uint16_t waveCore = TFTDisplay::rgb565(0x1D, 0xD7, 0xFF);
    const uint16_t waveGlowNear = TFTDisplay::rgb565(0x08, 0x67, 0xC8);
    const uint16_t waveGlowFar = TFTDisplay::rgb565(0x03, 0x23, 0x75);
    const uint16_t mountainCore = TFTDisplay::rgb565(0xFF, 0xFF, 0xFF);
    const uint16_t mountainGlowNear = TFTDisplay::rgb565(0x88, 0xDE, 0xFF);
    const uint16_t mountainGlowFar = TFTDisplay::rgb565(0x0A, 0x2E, 0x74);

    tft->fillRect565(0, 0, width, height, black);

    // Draw bloom before raster lines so wave details stay visible on top.
    renderDirectGpsGlobeBloom(tft, width, height, layout.globeCx, layout.globeCy, layout.globeR, layout.tiny);

    // Blue wave bundle: from lower-left to the right side, converging through the globe region.
    const int16_t waveLines = tinyPosterLayout ? 8 : 10;
    const int16_t waveSegments = tinyPosterLayout ? 22 : 24;
    const int16_t waveCoreThickness = 1;
    const int16_t waveNearThickness = tinyPosterLayout ? 0 : 1;
    const int16_t waveFarThickness = tinyPosterLayout ? 0 : 2;
    const int16_t waveTargetY = layout.globeCy + layout.globeR / 4;
    for (int16_t i = 0; i < waveLines; ++i) {
        const float spread = static_cast<float>(i - (waveLines / 2));
        const int16_t startY = height - 1 - (i * (tinyPosterLayout ? 3 : 5));
        const int16_t endY = waveTargetY + static_cast<int16_t>(spread * (tinyPosterLayout ? 2.2f : 1.9f));
        int16_t prevX = 0;
        int16_t prevY = startY;
        for (int16_t seg = 1; seg <= waveSegments; ++seg) {
            const float t = static_cast<float>(seg) / static_cast<float>(waveSegments);
            const int16_t x = static_cast<int16_t>(lrintf((width - 1) * t));
            const float baseY = static_cast<float>(startY) * (1.0f - t) + static_cast<float>(endY) * t;
            const float amp = (tinyPosterLayout ? 1.4f : 3.2f) * (1.0f - t);
            const float wave = sinf((t * (tinyPosterLayout ? 2.1f : 2.5f) * PI) + (i * 0.32f)) * amp;
            const int16_t y = static_cast<int16_t>(lrintf(baseY + wave));
            directDrawNeonLine565(
                tft, width, height, prevX, prevY, x, y, waveCore, waveGlowNear, waveGlowFar, waveCoreThickness, waveNearThickness, waveFarThickness);
            prevX = x;
            prevY = y;
            if ((seg & 0x03) == 0) {
                yield();
            }
        }
        yield();
    }

    // Upper-right curved streaks.
    const int16_t topArcLines = tinyPosterLayout ? 5 : 6;
    const int16_t topArcSegments = tinyPosterLayout ? 16 : 16;
    const int16_t topArcCoreThickness = 1;
    const int16_t topArcNearThickness = tinyPosterLayout ? 0 : 1;
    const int16_t topArcFarThickness = tinyPosterLayout ? 0 : 2;
    for (int16_t i = 0; i < topArcLines; ++i) {
        const int16_t startX = width - 1;
        const int16_t startY = -4 + (i * (tinyPosterLayout ? 3 : 4));
        const int16_t endX = layout.globeCx - (layout.globeR / 3) + (i / 2);
        const int16_t endY = layout.globeCy - (layout.globeR / 2) + (i * (tinyPosterLayout ? 2 : 3));
        int16_t prevX = startX;
        int16_t prevY = startY;
        for (int16_t seg = 1; seg <= topArcSegments; ++seg) {
            const float t = static_cast<float>(seg) / static_cast<float>(topArcSegments);
            const float curve = (1.0f - t) * t;
            const int16_t x = static_cast<int16_t>(lrintf((startX * (1.0f - t)) + (endX * t) - curve * (tinyPosterLayout ? 18.0f : 28.0f)));
            const int16_t y = static_cast<int16_t>(lrintf((startY * (1.0f - t)) + (endY * t) + sinf((t + (i * 0.06f)) * PI) * 2.0f));
            directDrawNeonLine565(
                tft, width, height, prevX, prevY, x, y, waveCore, waveGlowNear, waveGlowFar, topArcCoreThickness, topArcNearThickness,
                topArcFarThickness);
            prevX = x;
            prevY = y;
            if ((seg & 0x03) == 0) {
                yield();
            }
        }
        yield();
    }

    // Mountain outline icon (white core with blue neon halo).
    const int16_t mx = layout.mountainX;
    const int16_t my = layout.mountainY;
    const int16_t mw = layout.mountainW;
    const int16_t mh = layout.mountainH;
    const int16_t mLx = mx;
    const int16_t mLy = my + mh;
    const int16_t mPx = mx + (mw * 40) / 100;
    const int16_t mPy = my;
    const int16_t mRx = mx + mw;
    const int16_t mRy = my + mh;
    const int16_t sLx = mx + (mw * 20) / 100;
    const int16_t sLy = my + mh;
    const int16_t sPx = mx + (mw * 47) / 100;
    const int16_t sPy = my + (mh * 44) / 100;
    const int16_t sRx = mx + (mw * 72) / 100;
    const int16_t sRy = my + mh;
    directDrawNeonLine565(tft, width, height, mLx, mLy, mPx, mPy, mountainCore, mountainGlowNear, mountainGlowFar, 1, 2, 3);
    directDrawNeonLine565(tft, width, height, mPx, mPy, mRx, mRy, mountainCore, mountainGlowNear, mountainGlowFar, 1, 2, 3);
    directDrawNeonLine565(tft, width, height, mLx, mLy, mRx, mRy, mountainCore, mountainGlowNear, mountainGlowFar, 1, 2, 3);
    directDrawNeonLine565(tft, width, height, sLx, sLy, sPx, sPy, mountainCore, mountainGlowNear, mountainGlowFar, 1, 2, 3);
    directDrawNeonLine565(tft, width, height, sPx, sPy, sRx, sRy, mountainCore, mountainGlowNear, mountainGlowFar, 1, 2, 3);

    // Right-side neon wireframe globe.
    renderDirectGpsWireGlobe(tft, width, height, layout.globeCx, layout.globeCy, layout.globeR, layout.tiny);

    // Center number in globe = current satellite count.
    uint8_t satCount = 0;
    if (gps && gps->getIsConnected()) {
        satCount = static_cast<uint8_t>(std::min<uint32_t>(99, gps->getNumSatellites()));
    }
    char satCountLine[4];
    snprintf(satCountLine, sizeof(satCountLine), "%u", static_cast<unsigned>(satCount));
    const bool satHalfScale = true;
    const int16_t satTracking = tinyPosterLayout ? 1 : 2;
    const int16_t satTextW = measurePattanakarnClockTextTracked(satCountLine, satHalfScale, satTracking);
    const int16_t satTextH = kPattanakarnClockHalfHeight;
    if (satTextW > 0) {
        int16_t satX = layout.globeCx - (satTextW / 2);
        int16_t satY = layout.globeCy - (satTextH / 2);
        const int16_t neonMargin = static_cast<int16_t>((kDirectNeonClockMargin + 1) / 2);
        const int16_t minX = 2 + neonMargin;
        const int16_t maxX = width - satTextW - 2 - neonMargin;
        const int16_t minY = 2 + neonMargin;
        const int16_t maxY = height - satTextH - 2 - neonMargin;
        satX = std::max<int16_t>(minX, std::min<int16_t>(satX, maxX));
        satY = std::max<int16_t>(minY, std::min<int16_t>(satY, maxY));
        renderDirectNeonPattanakarnText(
            tft, satX, satY, satCountLine, satHalfScale, mapDirectGpsWarmLayerColor, false, satTracking);
    }
}

static void drawHermesXBatteryIcon(OLEDDisplay *display, int16_t x, int16_t y, int16_t width, int16_t height, uint8_t percent)
{
    if (!display || width < 16 || height < 24) {
        return;
    }

    if (percent > 100) {
        percent = 100;
    }

    int16_t capW = width / 3;
    int16_t capH = height / 10;
    if (capW < 6) {
        capW = 6;
    }
    if (capH < 3) {
        capH = 3;
    }

    const int16_t bodyY = y + capH;
    const int16_t bodyH = height - capH;
    if (bodyH < 12) {
        return;
    }

    const int16_t capX = x + (width - capW) / 2;
    display->drawRect(x, bodyY, width, bodyH);
    if (width > 20 && bodyH > 20) {
        display->drawRect(x + 1, bodyY + 1, width - 2, bodyH - 2);
    }
    display->drawRect(capX, y, capW, capH + 1);

    const int16_t inset = (width >= 28 && bodyH >= 36) ? 3 : 2;
    const int16_t innerX = x + inset;
    const int16_t innerY = bodyY + inset;
    const int16_t innerW = width - inset * 2;
    const int16_t innerH = bodyH - inset * 2;
    if (innerW <= 0 || innerH <= 0) {
        return;
    }

    int16_t fillH = (innerH * percent) / 100;
    if (percent > 0 && fillH < 1) {
        fillH = 1;
    }
    if (fillH > 0) {
        addTftColorZone(display, innerX, innerY + innerH - fillH, innerW, fillH, TFTDisplay::rgb565(0xB5, 0xED, 0x00), 0x0000);
        display->fillRect(innerX, innerY + innerH - fillH, innerW, fillH);
    }
}

static void drawHermesXBatteryIconHorizontal(OLEDDisplay *display, int16_t x, int16_t y, int16_t width, int16_t height,
                                             uint8_t percent)
{
    if (!display || width < 20 || height < 12) {
        return;
    }

    if (percent > 100) {
        percent = 100;
    }

    int16_t capW = width / 10;
    if (capW < 3) {
        capW = 3;
    }
    int16_t capH = height / 2;
    if (capH < 4) {
        capH = 4;
    }

    const int16_t bodyW = width - capW - 1;
    if (bodyW < 12) {
        return;
    }

    const int16_t bodyY = y;
    const int16_t capX = x + bodyW;
    const int16_t capY = y + (height - capH) / 2;

    display->drawRect(x, bodyY, bodyW, height);
    if (bodyW > 16 && height > 12) {
        display->drawRect(x + 1, bodyY + 1, bodyW - 2, height - 2);
    }
    display->drawRect(capX, capY, capW, capH);

    const int16_t inset = (bodyW >= 26 && height >= 16) ? 3 : 2;
    const int16_t innerX = x + inset;
    const int16_t innerY = bodyY + inset;
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
        addTftColorZone(display, innerX, innerY, fillW, innerH, TFTDisplay::rgb565(0xB5, 0xED, 0x00), 0x0000);
        display->fillRect(innerX, innerY, fillW, innerH);
    }
}

#if defined(DISPLAY_CLOCK_FRAME)

void Screen::drawWatchFaceToggleButton(OLEDDisplay *display, int16_t x, int16_t y, bool digitalMode, float scale)
{
    uint16_t segmentWidth = SEGMENT_WIDTH * scale;
    uint16_t segmentHeight = SEGMENT_HEIGHT * scale;

    if (digitalMode) {
        uint16_t radius = (segmentWidth + (segmentHeight * 2) + 4) / 2;
        uint16_t centerX = (x + segmentHeight + 2) + (radius / 2);
        uint16_t centerY = (y + segmentHeight + 2) + (radius / 2);

        display->drawCircle(centerX, centerY, radius);
        display->drawCircle(centerX, centerY, radius + 1);
        display->drawLine(centerX, centerY, centerX, centerY - radius + 3);
        display->drawLine(centerX, centerY, centerX + radius - 3, centerY);
    } else {
        uint16_t segmentOneX = x + segmentHeight + 2;
        uint16_t segmentOneY = y;

        uint16_t segmentTwoX = segmentOneX + segmentWidth + 2;
        uint16_t segmentTwoY = segmentOneY + segmentHeight + 2;

        uint16_t segmentThreeX = segmentOneX;
        uint16_t segmentThreeY = segmentTwoY + segmentWidth + 2;

        uint16_t segmentFourX = x;
        uint16_t segmentFourY = y + segmentHeight + 2;

        drawHorizontalSegment(display, segmentOneX, segmentOneY, segmentWidth, segmentHeight);
        drawVerticalSegment(display, segmentTwoX, segmentTwoY, segmentWidth, segmentHeight);
        drawHorizontalSegment(display, segmentThreeX, segmentThreeY, segmentWidth, segmentHeight);
        drawVerticalSegment(display, segmentFourX, segmentFourY, segmentWidth, segmentHeight);
    }
}

// Draw a digital clock
void Screen::drawDigitalClockFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    drawBattery(display, x, y + 7, imgBattery, powerStatus);

    if (powerStatus->getHasBattery()) {
        String batteryPercent = String(powerStatus->getBatteryChargePercent()) + "%";

        display->setFont(FONT_SMALL);

        display->drawString(x + 20, y + 2, batteryPercent);
    }

    if (nimbleBluetooth && nimbleBluetooth->isConnected()) {
        drawBluetoothConnectedIcon(display, display->getWidth() - 18, y + 2);
    }

    drawWatchFaceToggleButton(display, display->getWidth() - 36, display->getHeight() - 36, screen->digitalWatchFace, 1);

    display->setColor(OLEDDISPLAY_COLOR::WHITE);

    uint32_t rtc_sec = getValidTime(RTCQuality::RTCQualityDevice, true); // Display local timezone
    if (rtc_sec > 0) {
        long hms = rtc_sec % SEC_PER_DAY;
        hms = (hms + SEC_PER_DAY) % SEC_PER_DAY;

        int hour = hms / SEC_PER_HOUR;
        int minute = (hms % SEC_PER_HOUR) / SEC_PER_MIN;
        int second = (hms % SEC_PER_HOUR) % SEC_PER_MIN; // or hms % SEC_PER_MIN

        hour = hour > 12 ? hour - 12 : hour;

        if (hour == 0) {
            hour = 12;
        }

        // hours string
        String hourString = String(hour);

        // minutes string
        String minuteString = minute < 10 ? "0" + String(minute) : String(minute);

        String timeString = hourString + ":" + minuteString;

        // seconds string
        String secondString = second < 10 ? "0" + String(second) : String(second);

        float scale = 1.5;

        uint16_t segmentWidth = SEGMENT_WIDTH * scale;
        uint16_t segmentHeight = SEGMENT_HEIGHT * scale;
        const uint16_t charSpacing = 5;
        const uint16_t gapBetweenTimeAndSeconds = 4;

        // calculate hours:minutes string width
        uint16_t timeStringWidth = 0;

        for (uint8_t i = 0; i < timeString.length(); i++) {
            String character = String(timeString[i]);

            if (character == ":") {
                timeStringWidth += segmentHeight + 6;
            } else {
                timeStringWidth += segmentWidth + (segmentHeight * 2) + 4;
            }

            if (i + 1u < timeString.length()) {
                timeStringWidth += charSpacing;
            }
        }

        // calculate seconds string width
        display->setFont(FONT_MEDIUM);
        uint16_t secondStringWidth = display->getStringWidth(secondString);

        // sum these to get total string width
        uint16_t totalWidth = timeStringWidth + gapBetweenTimeAndSeconds + secondStringWidth;

        uint16_t hourMinuteTextX = (display->getWidth() / 2) - (totalWidth / 2);

        uint16_t startingHourMinuteTextX = hourMinuteTextX;

        uint16_t hourMinuteTextY = (display->getHeight() / 2) - (((segmentWidth * 2) + (segmentHeight * 3) + 8) / 2);

        // iterate over characters in hours:minutes string and draw segmented characters
        for (uint8_t i = 0; i < timeString.length(); i++) {
            String character = String(timeString[i]);

            if (character == ":") {
                drawSegmentedDisplayColon(display, hourMinuteTextX, hourMinuteTextY, scale);

                hourMinuteTextX += segmentHeight + 6;
            } else {
                drawSegmentedDisplayCharacter(display, hourMinuteTextX, hourMinuteTextY, character.toInt(), scale);

                hourMinuteTextX += segmentWidth + (segmentHeight * 2) + 4;
            }

            if (i + 1u < timeString.length()) {
                hourMinuteTextX += charSpacing;
            }
        }

        // draw seconds string
        display->drawString(startingHourMinuteTextX + timeStringWidth + gapBetweenTimeAndSeconds,
                            (display->getHeight() - hourMinuteTextY) - FONT_HEIGHT_MEDIUM + 6, secondString);
    }
}

void Screen::drawSegmentedDisplayColon(OLEDDisplay *display, int x, int y, float scale)
{
    uint16_t segmentWidth = SEGMENT_WIDTH * scale;
    uint16_t segmentHeight = SEGMENT_HEIGHT * scale;

    uint16_t cellHeight = (segmentWidth * 2) + (segmentHeight * 3) + 8;

    uint16_t topAndBottomX = x + (4 * scale);

    uint16_t quarterCellHeight = cellHeight / 4;

    uint16_t topY = y + quarterCellHeight;
    uint16_t bottomY = y + (quarterCellHeight * 3);

    display->fillRect(topAndBottomX, topY, segmentHeight, segmentHeight);
    display->fillRect(topAndBottomX, bottomY, segmentHeight, segmentHeight);
}

void Screen::drawSegmentedDisplayCharacter(OLEDDisplay *display, int x, int y, uint8_t number, float scale)
{
    // the numbers 0-9, each expressed as an array of seven boolean (0|1) values encoding the on/off state of
    // segment {innerIndex + 1}
    // e.g., to display the numeral '0', segments 1-6 are on, and segment 7 is off.
    uint8_t numbers[10][7] = {
        {1, 1, 1, 1, 1, 1, 0}, // 0          Display segment key
        {0, 1, 1, 0, 0, 0, 0}, // 1                   1
        {1, 1, 0, 1, 1, 0, 1}, // 2                  ___
        {1, 1, 1, 1, 0, 0, 1}, // 3              6  |   | 2
        {0, 1, 1, 0, 0, 1, 1}, // 4                 |_7戽_|
        {1, 0, 1, 1, 0, 1, 1}, // 5              5  |   | 3
        {1, 0, 1, 1, 1, 1, 1}, // 6                 |___|
        {1, 1, 1, 0, 0, 1, 0}, // 7
        {1, 1, 1, 1, 1, 1, 1}, // 8                   4
        {1, 1, 1, 1, 0, 1, 1}, // 9
    };

    // the width and height of each segment's central rectangle:
    //             _____________________
    //           ?院  (only this part,  |??    //         ?? |   not including    |  ??    //         ?? |   the triangles    |  ??    //           ?悴    on the ends)    |??    //             ?撾撾撾撾撾撾撾撾撾撾撾撾撾撾撾撾撾撾撾撾?
    uint16_t segmentWidth = SEGMENT_WIDTH * scale;
    uint16_t segmentHeight = SEGMENT_HEIGHT * scale;

    // segment x and y coordinates
    uint16_t segmentOneX = x + segmentHeight + 2;
    uint16_t segmentOneY = y;

    uint16_t segmentTwoX = segmentOneX + segmentWidth + 2;
    uint16_t segmentTwoY = segmentOneY + segmentHeight + 2;

    uint16_t segmentThreeX = segmentTwoX;
    uint16_t segmentThreeY = segmentTwoY + segmentWidth + 2 + segmentHeight + 2;

    uint16_t segmentFourX = segmentOneX;
    uint16_t segmentFourY = segmentThreeY + segmentWidth + 2;

    uint16_t segmentFiveX = x;
    uint16_t segmentFiveY = segmentThreeY;

    uint16_t segmentSixX = x;
    uint16_t segmentSixY = segmentTwoY;

    uint16_t segmentSevenX = segmentOneX;
    uint16_t segmentSevenY = segmentTwoY + segmentWidth + 2;

    if (numbers[number][0]) {
        drawHorizontalSegment(display, segmentOneX, segmentOneY, segmentWidth, segmentHeight);
    }

    if (numbers[number][1]) {
        drawVerticalSegment(display, segmentTwoX, segmentTwoY, segmentWidth, segmentHeight);
    }

    if (numbers[number][2]) {
        drawVerticalSegment(display, segmentThreeX, segmentThreeY, segmentWidth, segmentHeight);
    }

    if (numbers[number][3]) {
        drawHorizontalSegment(display, segmentFourX, segmentFourY, segmentWidth, segmentHeight);
    }

    if (numbers[number][4]) {
        drawVerticalSegment(display, segmentFiveX, segmentFiveY, segmentWidth, segmentHeight);
    }

    if (numbers[number][5]) {
        drawVerticalSegment(display, segmentSixX, segmentSixY, segmentWidth, segmentHeight);
    }

    if (numbers[number][6]) {
        drawHorizontalSegment(display, segmentSevenX, segmentSevenY, segmentWidth, segmentHeight);
    }
}

void Screen::drawHorizontalSegment(OLEDDisplay *display, int x, int y, int width, int height)
{
    int halfHeight = height / 2;

    // draw central rectangle
    display->fillRect(x, y, width, height);

    // draw end triangles
    display->fillTriangle(x, y, x, y + height - 1, x - halfHeight, y + halfHeight);

    display->fillTriangle(x + width, y, x + width + halfHeight, y + halfHeight, x + width, y + height - 1);
}

void Screen::drawVerticalSegment(OLEDDisplay *display, int x, int y, int width, int height)
{
    int halfHeight = height / 2;

    // draw central rectangle
    display->fillRect(x, y, height, width);

    // draw end triangles
    display->fillTriangle(x + halfHeight, y - halfHeight, x + height - 1, y, x, y);

    display->fillTriangle(x, y + width, x + height - 1, y + width, x + halfHeight, y + width + halfHeight);
}

void Screen::drawBluetoothConnectedIcon(OLEDDisplay *display, int16_t x, int16_t y)
{
    display->drawFastImage(x, y, 18, 14, bluetoothConnectedIcon);
}

// Draw an analog clock
void Screen::drawAnalogClockFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    drawBattery(display, x, y + 7, imgBattery, powerStatus);

    if (powerStatus->getHasBattery()) {
        String batteryPercent = String(powerStatus->getBatteryChargePercent()) + "%";

        display->setFont(FONT_SMALL);

        display->drawString(x + 20, y + 2, batteryPercent);
    }

    if (nimbleBluetooth && nimbleBluetooth->isConnected()) {
        drawBluetoothConnectedIcon(display, display->getWidth() - 18, y + 2);
    }

    drawWatchFaceToggleButton(display, display->getWidth() - 36, display->getHeight() - 36, screen->digitalWatchFace, 1);

    // clock face center coordinates
    int16_t centerX = display->getWidth() / 2;
    int16_t centerY = display->getHeight() / 2;

    // clock face radius
    int16_t radius = (display->getWidth() / 2) * 0.8;

    // noon (0 deg) coordinates (outermost circle)
    int16_t noonX = centerX;
    int16_t noonY = centerY - radius;

    // second hand radius and y coordinate (outermost circle)
    int16_t secondHandNoonY = noonY + 1;

    // tick mark outer y coordinate; (first nested circle)
    int16_t tickMarkOuterNoonY = secondHandNoonY;

    // seconds tick mark inner y coordinate; (second nested circle)
    double secondsTickMarkInnerNoonY = (double)noonY + 8;

    // hours tick mark inner y coordinate; (third nested circle)
    double hoursTickMarkInnerNoonY = (double)noonY + 16;

    // minute hand y coordinate
    int16_t minuteHandNoonY = secondsTickMarkInnerNoonY + 4;

    // hour string y coordinate
    int16_t hourStringNoonY = minuteHandNoonY + 18;

    // hour hand radius and y coordinate
    int16_t hourHandRadius = radius * 0.55;
    int16_t hourHandNoonY = centerY - hourHandRadius;

    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    display->drawCircle(centerX, centerY, radius);

    uint32_t rtc_sec = getValidTime(RTCQuality::RTCQualityDevice, true); // Display local timezone
    if (rtc_sec > 0) {
        long hms = rtc_sec % SEC_PER_DAY;
        hms = (hms + SEC_PER_DAY) % SEC_PER_DAY;

        // Tear apart hms into h:m:s
        int hour = hms / SEC_PER_HOUR;
        int minute = (hms % SEC_PER_HOUR) / SEC_PER_MIN;
        int second = (hms % SEC_PER_HOUR) % SEC_PER_MIN; // or hms % SEC_PER_MIN

        hour = hour > 12 ? hour - 12 : hour;

        int16_t degreesPerHour = 30;
        int16_t degreesPerMinuteOrSecond = 6;

        double hourBaseAngle = hour * degreesPerHour;
        double hourAngleOffset = ((double)minute / 60) * degreesPerHour;
        double hourAngle = radians(hourBaseAngle + hourAngleOffset);

        double minuteBaseAngle = minute * degreesPerMinuteOrSecond;
        double minuteAngleOffset = ((double)second / 60) * degreesPerMinuteOrSecond;
        double minuteAngle = radians(minuteBaseAngle + minuteAngleOffset);

        double secondAngle = radians(second * degreesPerMinuteOrSecond);

        double hourX = sin(-hourAngle) * (hourHandNoonY - centerY) + noonX;
        double hourY = cos(-hourAngle) * (hourHandNoonY - centerY) + centerY;

        double minuteX = sin(-minuteAngle) * (minuteHandNoonY - centerY) + noonX;
        double minuteY = cos(-minuteAngle) * (minuteHandNoonY - centerY) + centerY;

        double secondX = sin(-secondAngle) * (secondHandNoonY - centerY) + noonX;
        double secondY = cos(-secondAngle) * (secondHandNoonY - centerY) + centerY;

        display->setFont(FONT_MEDIUM);

        // draw minute and hour tick marks and hour numbers
        for (uint16_t angle = 0; angle < 360; angle += 6) {
            double angleInRadians = radians(angle);

            double sineAngleInRadians = sin(-angleInRadians);
            double cosineAngleInRadians = cos(-angleInRadians);

            double endX = sineAngleInRadians * (tickMarkOuterNoonY - centerY) + noonX;
            double endY = cosineAngleInRadians * (tickMarkOuterNoonY - centerY) + centerY;

            if (angle % degreesPerHour == 0) {
                double startX = sineAngleInRadians * (hoursTickMarkInnerNoonY - centerY) + noonX;
                double startY = cosineAngleInRadians * (hoursTickMarkInnerNoonY - centerY) + centerY;

                // draw hour tick mark
                display->drawLine(startX, startY, endX, endY);

                static char buffer[2];

                uint8_t hourInt = (angle / 30);

                if (hourInt == 0) {
                    hourInt = 12;
                }

                // hour number x offset needs to be adjusted for some cases
                int8_t hourStringXOffset;
                int8_t hourStringYOffset = 13;

                switch (hourInt) {
                case 3:
                    hourStringXOffset = 5;
                    break;
                case 9:
                    hourStringXOffset = 7;
                    break;
                case 10:
                case 11:
                    hourStringXOffset = 8;
                    break;
                case 12:
                    hourStringXOffset = 13;
                    break;
                default:
                    hourStringXOffset = 6;
                    break;
                }

                double hourStringX = (sineAngleInRadians * (hourStringNoonY - centerY) + noonX) - hourStringXOffset;
                double hourStringY = (cosineAngleInRadians * (hourStringNoonY - centerY) + centerY) - hourStringYOffset;

                // draw hour number
                display->drawStringf(hourStringX, hourStringY, buffer, "%d", hourInt);
            }

            if (angle % degreesPerMinuteOrSecond == 0) {
                double startX = sineAngleInRadians * (secondsTickMarkInnerNoonY - centerY) + noonX;
                double startY = cosineAngleInRadians * (secondsTickMarkInnerNoonY - centerY) + centerY;

                // draw minute tick mark
                display->drawLine(startX, startY, endX, endY);
            }
        }

        // draw hour hand
        display->drawLine(centerX, centerY, hourX, hourY);

        // draw minute hand
        display->drawLine(centerX, centerY, minuteX, minuteY);

        // draw second hand
        display->drawLine(centerX, centerY, secondX, secondY);
    }
}

#endif

// Get an absolute time from "seconds ago" info. Returns false if no valid timestamp possible
bool deltaToTimestamp(uint32_t secondsAgo, uint8_t *hours, uint8_t *minutes, int32_t *daysAgo)
{
    // Cache the result - avoid frequent recalculation
    static uint8_t hoursCached = 0, minutesCached = 0;
    static uint32_t daysAgoCached = 0;
    static uint32_t secondsAgoCached = 0;
    static bool validCached = false;

    // Abort: if timezone not set
    if (strlen(config.device.tzdef) == 0) {
        validCached = false;
        return validCached;
    }

    // Abort: if invalid pointers passed
    if (hours == nullptr || minutes == nullptr || daysAgo == nullptr) {
        validCached = false;
        return validCached;
    }

    // Abort: if time seems invalid.. (> 6 months ago, probably seen before RTC set)
    if (secondsAgo > SEC_PER_DAY * 30UL * 6) {
        validCached = false;
        return validCached;
    }

    // If repeated request, don't bother recalculating
    if (secondsAgo - secondsAgoCached < 60 && secondsAgoCached != 0) {
        if (validCached) {
            *hours = hoursCached;
            *minutes = minutesCached;
            *daysAgo = daysAgoCached;
        }
        return validCached;
    }

    // Get local time
    uint32_t secondsRTC = getValidTime(RTCQuality::RTCQualityDevice, true); // Get local time

    // Abort: if RTC not set
    if (!secondsRTC) {
        validCached = false;
        return validCached;
    }

    // Get absolute time when last seen
    uint32_t secondsSeenAt = secondsRTC - secondsAgo;

    // Calculate daysAgo
    *daysAgo = (secondsRTC / SEC_PER_DAY) - (secondsSeenAt / SEC_PER_DAY); // How many "midnights" have passed

    // Get seconds since midnight
    uint32_t hms = (secondsRTC - secondsAgo) % SEC_PER_DAY;
    hms = (hms + SEC_PER_DAY) % SEC_PER_DAY;

    // Tear apart hms into hours and minutes
    *hours = hms / SEC_PER_HOUR;
    *minutes = (hms % SEC_PER_HOUR) / SEC_PER_MIN;

    // Cache the result
    daysAgoCached = *daysAgo;
    hoursCached = *hours;
    minutesCached = *minutes;
    secondsAgoCached = secondsAgo;

    validCached = true;
    return validCached;
}

static constexpr uint8_t kRecentTextMessageCapacity = 8;
static constexpr uint32_t kIncomingTextPopupMs = 5000;
static constexpr uint32_t kIncomingNodePopupMs = 3500;

struct RecentTextMessageState {
    meshtastic_MeshPacket packets[kRecentTextMessageCapacity];
    uint8_t count = 0;
    uint8_t listCursor = 0;
    uint8_t selectedIndex = 0;
    uint8_t detailIndex = 0;
    uint16_t detailScrollY = 0;
    uint16_t detailMaxScrollY = 0;
};
static RecentTextMessageState gRecentTextMessageState;

struct IncomingTextPopupState {
    meshtastic_MeshPacket packet{};
    bool pending = false;
    bool visible = false;
    uint32_t untilMs = 0;
};
static IncomingTextPopupState gIncomingTextPopupState;

struct IncomingNodePopupState {
    meshtastic_NodeInfoLite node{};
    bool pending = false;
    bool visible = false;
    uint32_t untilMs = 0;
};
static IncomingNodePopupState gIncomingNodePopupState;

static void dismissIncomingTextPopup()
{
    gIncomingTextPopupState.pending = false;
    gIncomingTextPopupState.visible = false;
    gIncomingTextPopupState.untilMs = 0;
}

static bool isIncomingTextPopupVisible()
{
    return gIncomingTextPopupState.visible && screen && screen->isHermesXMainPageActive();
}

static bool isIncomingTextPopupActive()
{
    return gIncomingTextPopupState.pending || isIncomingTextPopupVisible();
}

static void dismissIncomingNodePopup()
{
    gIncomingNodePopupState.pending = false;
    gIncomingNodePopupState.visible = false;
    gIncomingNodePopupState.untilMs = 0;
}

static bool isIncomingNodePopupVisible()
{
    return gIncomingNodePopupState.visible;
}

static bool isIncomingNodePopupActive()
{
    return gIncomingNodePopupState.pending || gIncomingNodePopupState.visible;
}

static void armIncomingTextPopup(const meshtastic_MeshPacket &packet)
{
    if (gIncomingTextPopupState.pending || gIncomingTextPopupState.visible) {
        return;
    }

    gIncomingTextPopupState.packet = packet;
    gIncomingTextPopupState.pending = true;
    gIncomingTextPopupState.visible = false;
    gIncomingTextPopupState.untilMs = 0;
}

static void armIncomingNodePopup(const meshtastic_NodeInfoLite &node)
{
    if (node.num == 0 || node.num == nodeDB->getNodeNum()) {
        return;
    }

    gIncomingNodePopupState.node = node;
    gIncomingNodePopupState.pending = true;
    gIncomingNodePopupState.visible = false;
    gIncomingNodePopupState.untilMs = 0;
    gDirectIncomingNodePopupRenderCache.valid = false;
}

void Screen::maybeArmIncomingTextPopup(const meshtastic_MeshPacket &packet)
{
    if (packet.from == 0 || config.display.screen_on_secs == 0 || isStealthModeActive()) {
        return;
    }
    if (screenOn && !isHermesXMainPageActive()) {
        return;
    }

    armIncomingTextPopup(packet);
    if (screenOn && ui) {
        setFastFramerate();
    }
}

static bool hasRecentTextMessages()
{
    return gRecentTextMessageState.count > 0;
}

static uint8_t getRecentTextMessageListEntryCount()
{
    return gRecentTextMessageState.count + 1; // Includes the leading "Back" row.
}

static void clampRecentTextMessageIndices()
{
    const uint8_t lastCursor = getRecentTextMessageListEntryCount() - 1;
    if (gRecentTextMessageState.listCursor > lastCursor) {
        gRecentTextMessageState.listCursor = lastCursor;
    }

    if (gRecentTextMessageState.count == 0) {
        gRecentTextMessageState.selectedIndex = 0;
        gRecentTextMessageState.detailIndex = 0;
        return;
    }

    const uint8_t lastIndex = gRecentTextMessageState.count - 1;
    if (gRecentTextMessageState.selectedIndex > lastIndex) {
        gRecentTextMessageState.selectedIndex = lastIndex;
    }
    if (gRecentTextMessageState.detailIndex > lastIndex) {
        gRecentTextMessageState.detailIndex = lastIndex;
    }
}

static const meshtastic_MeshPacket *getRecentTextMessageAt(uint8_t index)
{
    clampRecentTextMessageIndices();
    if (index >= gRecentTextMessageState.count) {
        return nullptr;
    }
    return &gRecentTextMessageState.packets[index];
}

static const meshtastic_MeshPacket *getActiveTextMessageForDisplay()
{
    if (hasRecentTextMessages()) {
        clampRecentTextMessageIndices();
        return &gRecentTextMessageState.packets[gRecentTextMessageState.detailIndex];
    }
    if (devicestate.has_rx_text_message) {
        return &devicestate.rx_text_message;
    }
    return nullptr;
}

static void setRecentTextMessageDetailToSelected()
{
    clampRecentTextMessageIndices();
    gRecentTextMessageState.detailIndex = gRecentTextMessageState.selectedIndex;
    gRecentTextMessageState.detailScrollY = 0;
}

static void storeRecentTextMessage(const meshtastic_MeshPacket &packet)
{
    if (packet.from == 0) {
        return;
    }

    uint8_t existingIndex = gRecentTextMessageState.count;
    for (uint8_t i = 0; i < gRecentTextMessageState.count; ++i) {
        const meshtastic_MeshPacket &candidate = gRecentTextMessageState.packets[i];
        if (candidate.from == packet.from && candidate.id == packet.id) {
            existingIndex = i;
            break;
        }
    }

    if (existingIndex == gRecentTextMessageState.count) {
        if (gRecentTextMessageState.count < kRecentTextMessageCapacity) {
            ++gRecentTextMessageState.count;
        }
        existingIndex = gRecentTextMessageState.count - 1;
    }

    for (uint8_t i = existingIndex; i > 0; --i) {
        gRecentTextMessageState.packets[i] = gRecentTextMessageState.packets[i - 1];
    }

    gRecentTextMessageState.packets[0] = packet;
    gRecentTextMessageState.listCursor = 1;
    gRecentTextMessageState.selectedIndex = 0;
    gRecentTextMessageState.detailIndex = 0;
    gRecentTextMessageState.detailScrollY = 0;
    gRecentTextMessageState.detailMaxScrollY = 0;
}

static size_t utf8SequenceLength(uint8_t firstByte)
{
    if ((firstByte & 0x80u) == 0) {
        return 1;
    }
    if ((firstByte & 0xE0u) == 0xC0u) {
        return 2;
    }
    if ((firstByte & 0xF0u) == 0xE0u) {
        return 3;
    }
    if ((firstByte & 0xF8u) == 0xF0u) {
        return 4;
    }
    return 1;
}

static void copyUtf8Snippet(const char *src, size_t srcLen, char *out, size_t outSize, size_t maxCodepoints)
{
    if (!out || outSize == 0) {
        return;
    }

    out[0] = '\0';
    if (!src || srcLen == 0 || maxCodepoints == 0) {
        return;
    }

    size_t srcPos = 0;
    size_t dstPos = 0;
    size_t copiedCodepoints = 0;
    bool truncated = false;

    while (srcPos < srcLen && dstPos + 1 < outSize && copiedCodepoints < maxCodepoints) {
        const uint8_t firstByte = static_cast<uint8_t>(src[srcPos]);
        size_t seqLen = utf8SequenceLength(firstByte);
        if (seqLen == 0) {
            seqLen = 1;
        }
        if (srcPos + seqLen > srcLen || dstPos + seqLen >= outSize) {
            truncated = true;
            break;
        }

        bool allZero = true;
        for (size_t i = 0; i < seqLen; ++i) {
            if (src[srcPos + i] != '\0') {
                allZero = false;
                break;
            }
        }
        if (allZero) {
            break;
        }

        if (seqLen == 1 && (src[srcPos] == '\n' || src[srcPos] == '\r' || src[srcPos] == '\t')) {
            out[dstPos++] = ' ';
            ++srcPos;
            ++copiedCodepoints;
            continue;
        }

        memcpy(out + dstPos, src + srcPos, seqLen);
        dstPos += seqLen;
        srcPos += seqLen;
        ++copiedCodepoints;
    }

    if (srcPos < srcLen) {
        truncated = true;
    }

    if (truncated && outSize >= 4) {
        while (dstPos > 0 && (static_cast<uint8_t>(out[dstPos - 1]) & 0xC0u) == 0x80u) {
            --dstPos;
        }
        if (dstPos + 3 < outSize) {
            out[dstPos++] = '.';
            out[dstPos++] = '.';
            out[dstPos++] = '.';
        }
    }

    out[dstPos] = '\0';
}

static void makeTextMessageSnippet(const meshtastic_MeshPacket &packet, char *out, size_t outSize, size_t maxCodepoints)
{
    copyUtf8Snippet(reinterpret_cast<const char *>(packet.decoded.payload.bytes), packet.decoded.payload.size, out, outSize, maxCodepoints);
}

static uint16_t measureMixedWrappedTextHeight(OLEDDisplay *display, const char *text, int16_t maxWidth, int lineHeight,
                                              int advanceX = graphics::HermesX_zh::GLYPH_WIDTH)
{
    if (!display || !text || maxWidth <= 0) {
        return static_cast<uint16_t>(std::max(lineHeight, 0));
    }

    int x = 0;
    int lines = 1;
    const char *cursor = text;
    const char *end = cursor + std::strlen(text);
    while (cursor < end) {
        std::uint32_t cp = graphics::HermesX_zh::nextCodepoint(cursor, end);
        if (cp == 0) {
            break;
        }
        if (cp == '\r') {
            continue;
        }
        if (cp == '\n') {
            x = 0;
            ++lines;
            continue;
        }
        if (cp >= 0x20u && cp < 0x7Fu) {
            String asciiChar(static_cast<char>(cp));
            int glyphWidth = static_cast<int>(display->getStringWidth(asciiChar));
            if (glyphWidth <= 0) {
                glyphWidth = advanceX;
            }
            if (x + glyphWidth > maxWidth) {
                x = 0;
                ++lines;
            }
            x += glyphWidth;
        } else {
            if (x + advanceX > maxWidth) {
                x = 0;
                ++lines;
            }
            x += advanceX;
        }
    }

    return static_cast<uint16_t>(std::max(lines * lineHeight, lineHeight));
}

static void formatIncomingNodePopupShortId(const meshtastic_NodeInfoLite &node, char *out, size_t outSize)
{
    if (!out || outSize == 0) {
        return;
    }

    out[0] = '\0';
    if (node.has_user && node.user.short_name[0] != '\0') {
        copyUtf8Snippet(node.user.short_name, strnlen(node.user.short_name, sizeof(node.user.short_name)), out, outSize, 4);
    }
    if (out[0] == '\0') {
        snprintf(out, outSize, "%04x", static_cast<unsigned>(node.num & 0xFFFFu));
    }
}

static void formatIncomingNodePopupName(const meshtastic_NodeInfoLite &node, char *out, size_t outSize, size_t maxCodepoints)
{
    if (!out || outSize == 0) {
        return;
    }

    out[0] = '\0';
    if (node.has_user && node.user.long_name[0] != '\0') {
        copyUtf8Snippet(node.user.long_name, strnlen(node.user.long_name, sizeof(node.user.long_name)), out, outSize, maxCodepoints);
    } else if (node.has_user && node.user.short_name[0] != '\0') {
        copyUtf8Snippet(node.user.short_name, strnlen(node.user.short_name, sizeof(node.user.short_name)), out, outSize, maxCodepoints);
    }

    if (out[0] == '\0') {
        snprintf(out, outSize, "Node %04x", static_cast<unsigned>(node.num & 0xFFFFu));
    }
}

static bool canRenderIncomingNodePopupTimerText(const char *text);
static int16_t measureIncomingNodePopupTimerText(const char *text, bool halfScale = false);
static void drawIncomingNodePopupTimerText(OLEDDisplay *display, int16_t x, int16_t y, const char *text, bool halfScale = false);
static void drawIncomingNodePopupBannerText(OLEDDisplay *display, int16_t x, int16_t y, int16_t maxWidth, const char *text);

static void drawIncomingTextPopupOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{
    (void)state;
    if (!display || !isIncomingTextPopupVisible()) {
        return;
    }

    const meshtastic_MeshPacket &packet = gIncomingTextPopupState.packet;
    if (packet.from == 0) {
        return;
    }

    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    const bool compactLayout = (width < 200 || height < 120);
    const int16_t boxX = 0;
    const int16_t boxY = 0;
    const int16_t boxW = width;
    const int16_t boxH = compactLayout ? 34 : 42;
    const int16_t textPadX = compactLayout ? 4 : 6;
    const int16_t textPadY = compactLayout ? 3 : 4;

    addTftColorZone(display, boxX, boxY, boxW, boxH, TFTDisplay::rgb565(0x1E, 0xB8, 0xE0), 0x0000);

    display->setColor(WHITE);
    display->fillRect(boxX, boxY, boxW, boxH);
    display->setColor(BLACK);

    meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(getFrom(&packet));
    const char *sender = (node && node->has_user && node->user.long_name[0] != '\0') ? node->user.long_name : "Unknown";
    char senderBuf[40];
    copyUtf8Snippet(sender, strnlen(sender, 127), senderBuf, sizeof(senderBuf), compactLayout ? 10 : 16);
    if (senderBuf[0] == '\0') {
        strlcpy(senderBuf, "Unknown", sizeof(senderBuf));
    }

    char snippetBuf[96];
    makeTextMessageSnippet(packet, snippetBuf, sizeof(snippetBuf), compactLayout ? 18 : 30);
    if (snippetBuf[0] == '\0') {
        strlcpy(snippetBuf, u8"(空白訊息)", sizeof(snippetBuf));
    }

    String headerLine = String(u8"新訊息  ");
    headerLine += senderBuf;

    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);
    const int16_t textW = boxW - textPadX * 2;
    const int16_t lineHeight = compactLayout ? 12 : 14;
    HermesX_zh::drawMixedBounded(*display, boxX + textPadX, boxY + textPadY, textW, headerLine.c_str(),
                                 HermesX_zh::GLYPH_WIDTH, lineHeight, nullptr);
    HermesX_zh::drawMixedBounded(*display, boxX + textPadX, boxY + textPadY + lineHeight + 2, textW, snippetBuf,
                                 HermesX_zh::GLYPH_WIDTH, lineHeight, nullptr);

    display->setColor(WHITE);
}

static void drawIncomingNodePopupOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{
    (void)state;
    if (!display || !isIncomingNodePopupVisible()) {
        return;
    }

#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
    if (supportsDirectTftClockRendering(display)) {
        return;
    }
#endif

    const meshtastic_NodeInfoLite &node = gIncomingNodePopupState.node;
    char shortId[12];
    char nameBuf[64];
    formatIncomingNodePopupShortId(node, shortId, sizeof(shortId));
    formatIncomingNodePopupName(node, nameBuf, sizeof(nameBuf), 14);

    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    const bool compactLayout = (width < 200 || height < 120);
    const int16_t textX = compactLayout ? 10 : 18;
    const int16_t textW = std::max<int16_t>(40, (width / 2) - textX);
    const int16_t titleY = compactLayout ? 22 : 30;
    const int16_t idY = compactLayout ? 62 : 82;
    const int16_t bannerH = compactLayout ? 18 : 22;
    const int16_t bannerY = height - bannerH;
    const int16_t nameY = bannerY + std::max<int16_t>(1, (bannerH - 14) / 2);

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
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);
    HermesX_zh::drawMixedBounded(*display, textX, titleY, textW, u8"發現新節點！", HermesX_zh::GLYPH_WIDTH, compactLayout ? 16 : 20, nullptr);

    if (canRenderIncomingNodePopupTimerText(shortId)) {
        const int16_t shortIdW = measureIncomingNodePopupTimerText(shortId);
        const int16_t shortIdX = std::max<int16_t>(textX, textX + ((textW - shortIdW) / 2));
        drawIncomingNodePopupTimerText(display, shortIdX, idY - 4, shortId);
    } else {
        display->setFont(compactLayout ? FONT_MEDIUM : FONT_LARGE);
        HermesX_zh::drawMixedBounded(*display, textX + (compactLayout ? 18 : 26), idY, textW - (compactLayout ? 14 : 22), shortId,
                                     HermesX_zh::GLYPH_WIDTH, compactLayout ? 16 : 20, nullptr);
    }

    display->drawRect(0, bannerY, width, bannerH);
    display->setFont(compactLayout ? FONT_SMALL : FONT_MEDIUM);
    drawIncomingNodePopupBannerText(display, textX, nameY, width - (textX * 2), nameBuf);

    auto drawArcApprox = [&](int16_t cx, int16_t cy, int16_t radius, float startDeg, float endDeg, uint8_t thickness) {
        if (radius <= 0) {
            return;
        }
        if (thickness == 0) {
            thickness = 1;
        }
        const int steps = (radius < 8) ? 8 : (radius * 2);
        for (uint8_t t = 0; t < thickness; ++t) {
            const int16_t rr = radius - static_cast<int16_t>(t);
            bool hasPrev = false;
            int16_t px = 0;
            int16_t py = 0;
            for (int i = 0; i <= steps; ++i) {
                const float u = static_cast<float>(i) / static_cast<float>(steps);
                const float deg = startDeg + (endDeg - startDeg) * u;
                const float a = deg * PI / 180.0f;
                const int16_t x = cx + static_cast<int16_t>(cosf(a) * rr);
                const int16_t y = cy + static_cast<int16_t>(sinf(a) * rr);
                if (hasPrev) {
                    display->drawLine(px, py, x, y);
                }
                px = x;
                py = y;
                hasPrev = true;
            }
        }
    };

    const int16_t dotR = compactLayout ? 10 : 14;
    const int16_t outerR = compactLayout ? (height * 58) / 100 : (height * 60) / 100;
    const int16_t innerR = (outerR * 2) / 3;
    const int16_t iconCx = width - outerR - (compactLayout ? 10 : 16);
    const int16_t iconCy = height - dotR - (compactLayout ? 18 : 22);
    drawArcApprox(iconCx, iconCy, outerR, -90.0f, 0.0f, compactLayout ? 10 : 14);
    drawArcApprox(iconCx, iconCy, innerR, -90.0f, 0.0f, compactLayout ? 9 : 12);
    display->fillCircle(iconCx, iconCy, dotR);
}

static void drawRecentTextMessagesFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    (void)state;
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);

    if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_INVERTED) {
        display->fillRect(x, y, x + display->getWidth(), y + FONT_HEIGHT_SMALL);
        display->setColor(BLACK);
    }

    display->drawString(x, y, "Recent Send");
    display->setColor(WHITE);

    clampRecentTextMessageIndices();
    const int16_t width = display->getWidth();
    const int16_t rowH = FONT_HEIGHT_SMALL + 3;
    const int16_t listTop = y + FONT_HEIGHT_SMALL + 2;
    int8_t visibleRows = (display->getHeight() - listTop - 1) / rowH;
    if (visibleRows < 1) {
        visibleRows = 1;
    }
    if (visibleRows > 3) {
        visibleRows = 3;
    }

    const uint8_t totalRows = getRecentTextMessageListEntryCount();
    uint8_t startCursor = 0;
    if (gRecentTextMessageState.listCursor >= static_cast<uint8_t>(visibleRows)) {
        startCursor = gRecentTextMessageState.listCursor - static_cast<uint8_t>(visibleRows) + 1;
    }

    for (int8_t row = 0; row < visibleRows; ++row) {
        const uint8_t cursorIndex = startCursor + row;
        if (cursorIndex >= totalRows) {
            break;
        }

        const int16_t rowY = listTop + row * rowH;
        const bool selected = (cursorIndex == gRecentTextMessageState.listCursor);
        if (selected) {
            display->drawRect(x, rowY - 1, width - 2, rowH);
        }

        if (cursorIndex == 0) {
            graphics::HermesX_zh::drawMixedBounded(*display, x + 2, rowY, width - 4, u8"返回",
                                                   graphics::HermesX_zh::GLYPH_WIDTH, rowH, nullptr);
            continue;
        }

        const uint8_t packetIndex = cursorIndex - 1;
        const meshtastic_MeshPacket *packet = getRecentTextMessageAt(packetIndex);
        if (!packet) {
            continue;
        }

        meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(getFrom(packet));
        char senderBuf[24];
        senderBuf[0] = '\0';
        if (node && node->has_user) {
            if (node->user.short_name[0] != '\0') {
                copyUtf8Snippet(node->user.short_name, strnlen(node->user.short_name, sizeof(node->user.short_name)), senderBuf,
                                sizeof(senderBuf), 8);
            } else if (node->user.long_name[0] != '\0') {
                copyUtf8Snippet(node->user.long_name, strnlen(node->user.long_name, sizeof(node->user.long_name)), senderBuf,
                                sizeof(senderBuf), 8);
            }
        }
        if (senderBuf[0] == '\0') {
            strlcpy(senderBuf, "???", sizeof(senderBuf));
        }

        char preview[40];
        makeTextMessageSnippet(*packet, preview, sizeof(preview), 14);

        char channelBuf[20];
        snprintf(channelBuf, sizeof(channelBuf), "#%s", channels.getName(packet->channel));
        const int16_t channelW = display->getStringWidth(channelBuf);
        const int16_t channelX = x + width - channelW - 2;
        display->drawString(channelX, rowY, channelBuf);

        char lineBuf[96];
        snprintf(lineBuf, sizeof(lineBuf), "%s: %s", senderBuf, preview);
        const int16_t textWidth = channelX - x - 4;
        HermesX_zh::drawMixedBounded(*display, x + 2, rowY, textWidth > 0 ? textWidth : width - 4, lineBuf,
                                     HermesX_zh::GLYPH_WIDTH, rowH, nullptr);
    }

    if (!hasRecentTextMessages() && visibleRows > 1) {
        display->drawString(x + 2, listTop + rowH, "No messages");
    }
}

/// Draw the last text message we received
static void drawTextMessageFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    // the max length of this buffer is much longer than we can possibly print
    static char tempBuf[237];

    const meshtastic_MeshPacket *packet = getActiveTextMessageForDisplay();
    if (!packet) {
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        display->setFont(FONT_SMALL);
        display->drawString(x, y, "No message");
        return;
    }

    const meshtastic_MeshPacket &mp = *packet;
    meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(getFrom(packet));
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);

    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    const int16_t headerLineHeight = FONT_HEIGHT_SMALL;
    const int16_t headerGap = 2;
    const int16_t dividerY = y + headerLineHeight * 2 + headerGap;
    const int16_t bodyTop = dividerY + 2;
    const int16_t scrollbarW = 4;
    const int16_t bodyW = std::max<int16_t>(width - scrollbarW - 3, 12);
    const int16_t bodyH = std::max<int16_t>(height - bodyTop - 1, headerLineHeight);
    const int16_t bodyX = x;
    const int lineHeight = FONT_HEIGHT_SMALL + 2;

    if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_INVERTED) {
        display->fillRect(x, y, width, dividerY - y);
        display->setColor(BLACK);
    }

    char senderBuf[24];
    senderBuf[0] = '\0';
    if (node && node->has_user) {
        if (node->user.short_name[0] != '\0') {
            copyUtf8Snippet(node->user.short_name, strnlen(node->user.short_name, sizeof(node->user.short_name)), senderBuf,
                            sizeof(senderBuf), 10);
        } else if (node->user.long_name[0] != '\0') {
            copyUtf8Snippet(node->user.long_name, strnlen(node->user.long_name, sizeof(node->user.long_name)), senderBuf,
                            sizeof(senderBuf), 10);
        }
    }
    if (senderBuf[0] == '\0') {
        strlcpy(senderBuf, "???", sizeof(senderBuf));
    }

    const uint32_t seconds = sinceReceived(&mp);
    const uint32_t minutes = seconds / 60;
    const uint32_t hours = minutes / 60;
    const uint32_t days = hours / 24;
    uint8_t timestampHours, timestampMinutes;
    int32_t daysAgo;
    const bool useTimestamp = deltaToTimestamp(seconds, &timestampHours, &timestampMinutes, &daysAgo);

    char headerLine1[48];
    snprintf(headerLine1, sizeof(headerLine1), "%s", senderBuf);
    char headerLine2[64];
    if (useTimestamp && minutes >= 15 && daysAgo == 0) {
        snprintf(headerLine2, sizeof(headerLine2), "At %02hu:%02hu", timestampHours, timestampMinutes);
    } else if (useTimestamp && daysAgo == 1 && width >= 200) {
        snprintf(headerLine2, sizeof(headerLine2), "Yesterday %02hu:%02hu", timestampHours, timestampMinutes);
    } else {
        snprintf(headerLine2, sizeof(headerLine2), "%s ago", screen->drawTimeDelta(days, hours, minutes, seconds).c_str());
    }

    graphics::HermesX_zh::drawMixedBounded(*display, x, y, width - 1, headerLine1, graphics::HermesX_zh::GLYPH_WIDTH,
                                           headerLineHeight, nullptr);
    graphics::HermesX_zh::drawMixedBounded(*display, x, y + headerLineHeight, width - 1, headerLine2,
                                           graphics::HermesX_zh::GLYPH_WIDTH, headerLineHeight, nullptr);
    display->drawLine(x, dividerY, x + width - 1, dividerY);
    display->setColor(WHITE);

    snprintf(tempBuf, sizeof(tempBuf), "%s", mp.decoded.payload.bytes);
    const uint16_t contentHeight = measureMixedWrappedTextHeight(display, tempBuf, bodyW, lineHeight);
    gRecentTextMessageState.detailMaxScrollY = (contentHeight > bodyH) ? (contentHeight - bodyH) : 0;
    if (gRecentTextMessageState.detailScrollY > gRecentTextMessageState.detailMaxScrollY) {
        gRecentTextMessageState.detailScrollY = gRecentTextMessageState.detailMaxScrollY;
    }

#ifndef EXCLUDE_EMOJI
    const char *msg = reinterpret_cast<const char *>(mp.decoded.payload.bytes);
    // NOTE: Emoji comparisons below use explicit UTF-8 literals; keep this block UTF-8 encoded and do not replace with '?' placeholders
    if (strcmp(msg, "\U0001F44D") == 0) {
        display->drawXbm(x + (SCREEN_WIDTH - thumbs_width) / 2,
                         y + (SCREEN_HEIGHT - FONT_HEIGHT_MEDIUM - thumbs_height) / 2 + 2 + 5, thumbs_width, thumbs_height,
                         thumbup);
    } else if (strcmp(msg, "\U0001F44E") == 0) {
        display->drawXbm(x + (SCREEN_WIDTH - thumbs_width) / 2,
                         y + (SCREEN_HEIGHT - FONT_HEIGHT_MEDIUM - thumbs_height) / 2 + 2 + 5, thumbs_width, thumbs_height,
                         thumbdown);
    } else if (strcmp(msg, "\U0001F60A") == 0 || strcmp(msg, "\U0001F600") == 0 || strcmp(msg, "\U0001F642") == 0 ||
               strcmp(msg, "\U0001F609") == 0 ||
               strcmp(msg, "\U0001F601") == 0) { // matches 5 different common smileys, so that the phone user doesn't have to
                                                 // remember which one is compatible
        display->drawXbm(x + (SCREEN_WIDTH - smiley_width) / 2,
                         y + (SCREEN_HEIGHT - FONT_HEIGHT_MEDIUM - smiley_height) / 2 + 2 + 5, smiley_width, smiley_height,
                         smiley);
    } else if (strcmp(msg, "\xE2\x80\xBC") == 0) { // U+203C DOUBLE EXCLAMATION MARK
        display->drawXbm(x + (SCREEN_WIDTH - question_width) / 2,
                         y + (SCREEN_HEIGHT - FONT_HEIGHT_MEDIUM - question_height) / 2 + 2 + 5, question_width, question_height,
                         question);
    } else if (strcmp(msg, "\xE2\x80\xBC\xEF\xB8\x8F") == 0) { // U+203C + U+FE0F (double exclamation mark VS16)
        display->drawXbm(x + (SCREEN_WIDTH - bang_width) / 2, y + (SCREEN_HEIGHT - FONT_HEIGHT_MEDIUM - bang_height) / 2 + 2 + 5,
                         bang_width, bang_height, bang);
    } else if (strcmp(msg, "\U0001F4A9") == 0) {
        display->drawXbm(x + (SCREEN_WIDTH - poo_width) / 2, y + (SCREEN_HEIGHT - FONT_HEIGHT_MEDIUM - poo_height) / 2 + 2 + 5,
                         poo_width, poo_height, poo);
    } else if (strcmp(msg, "\U0001F923") == 0) {
        display->drawXbm(x + (SCREEN_WIDTH - haha_width) / 2, y + (SCREEN_HEIGHT - FONT_HEIGHT_MEDIUM - haha_height) / 2 + 2 + 5,
                         haha_width, haha_height, haha);
    } else if (strcmp(msg, "\U0001F44B") == 0) {
        display->drawXbm(x + (SCREEN_WIDTH - wave_icon_width) / 2,
                         y + (SCREEN_HEIGHT - FONT_HEIGHT_MEDIUM - wave_icon_height) / 2 + 2 + 5, wave_icon_width,
                         wave_icon_height, wave_icon);
    } else if (strcmp(msg, "\U0001F920") == 0) {
        display->drawXbm(x + (SCREEN_WIDTH - cowboy_width) / 2,
                         y + (SCREEN_HEIGHT - FONT_HEIGHT_MEDIUM - cowboy_height) / 2 + 2 + 5, cowboy_width, cowboy_height,
                         cowboy);
    } else if (strcmp(msg, "\U0001F42D") == 0) {
        display->drawXbm(x + (SCREEN_WIDTH - deadmau5_width) / 2,
                         y + (SCREEN_HEIGHT - FONT_HEIGHT_MEDIUM - deadmau5_height) / 2 + 2 + 5, deadmau5_width, deadmau5_height,
                         deadmau5);
    } else if (strcmp(msg, "\xE2\x98\x80\xEF\xB8\x8F") == 0) {
        display->drawXbm(x + (SCREEN_WIDTH - sun_width) / 2, y + (SCREEN_HEIGHT - FONT_HEIGHT_MEDIUM - sun_height) / 2 + 2 + 5,
                         sun_width, sun_height, sun);
    } else if (strcmp(msg, "\u2614") == 0) {
        display->drawXbm(x + (SCREEN_WIDTH - rain_width) / 2, y + (SCREEN_HEIGHT - FONT_HEIGHT_MEDIUM - rain_height) / 2 + 2 + 10,
                         rain_width, rain_height, rain);
    } else if (strcmp(msg, "\u2601") == 0) {
        display->drawXbm(x + (SCREEN_WIDTH - cloud_width) / 2,
                         y + (SCREEN_HEIGHT - FONT_HEIGHT_MEDIUM - cloud_height) / 2 + 2 + 5, cloud_width, cloud_height, cloud);
    } else if (strcmp(msg, "\U0001F32B") == 0) {
        display->drawXbm(x + (SCREEN_WIDTH - fog_width) / 2, y + (SCREEN_HEIGHT - FONT_HEIGHT_MEDIUM - fog_height) / 2 + 2 + 5,
                         fog_width, fog_height, fog);
    } else if (strcmp(msg, "\U0001F608") == 0) {
        display->drawXbm(x + (SCREEN_WIDTH - devil_width) / 2,
                         y + (SCREEN_HEIGHT - FONT_HEIGHT_MEDIUM - devil_height) / 2 + 2 + 5, devil_width, devil_height, devil);
    } else if (strcmp(msg, "\xE2\x9D\xA4") == 0 || strcmp(msg, "\xE2\x9D\xA4\xEF\xB8\x8F") == 0 || strcmp(msg, "\U0001F9E1") == 0 || strcmp(msg, "\u2763") == 0 ||
               strcmp(msg, "\U00002764") == 0 || strcmp(msg, "\U0001F495") == 0 || strcmp(msg, "\U0001F496") == 0 ||
               strcmp(msg, "\U0001F497") == 0 || strcmp(msg, "\U0001F498") == 0) {
        display->drawXbm(x + (SCREEN_WIDTH - heart_width) / 2,
                         y + (SCREEN_HEIGHT - FONT_HEIGHT_MEDIUM - heart_height) / 2 + 2 + 5, heart_width, heart_height, heart);
    } else {
        HermesX_zh::drawMixedBounded(*display, bodyX, bodyTop - static_cast<int16_t>(gRecentTextMessageState.detailScrollY), bodyW,
                                     tempBuf, 12, lineHeight, nullptr);
    }
#else
    HermesX_zh::drawMixedBounded(*display, bodyX, bodyTop - static_cast<int16_t>(gRecentTextMessageState.detailScrollY), bodyW,
                                 tempBuf, 12, lineHeight, nullptr);
#endif

    if (gRecentTextMessageState.detailMaxScrollY > 0) {
        const int16_t trackX = x + width - scrollbarW;
        display->drawRect(trackX, bodyTop, scrollbarW, bodyH);
        const int16_t thumbH =
            std::max<int16_t>(6, static_cast<int16_t>((static_cast<int32_t>(bodyH) * bodyH) / contentHeight));
        const int16_t thumbTravel = std::max<int16_t>(bodyH - thumbH - 2, 0);
        const int16_t thumbY =
            bodyTop + 1 +
            static_cast<int16_t>((static_cast<int32_t>(thumbTravel) * gRecentTextMessageState.detailScrollY) /
                                 gRecentTextMessageState.detailMaxScrollY);
        if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_INVERTED) {
            display->fillRect(trackX + 1, thumbY, scrollbarW - 2, thumbH);
            display->setColor(BLACK);
            display->fillRect(trackX + 1, thumbY, scrollbarW - 2, thumbH);
            display->setColor(WHITE);
        } else {
            display->fillRect(trackX + 1, thumbY, scrollbarW - 2, thumbH);
        }
    }
}

/// Draw a series of fields in a column, wrapping to multiple columns if needed
void Screen::drawColumns(OLEDDisplay *display, int16_t x, int16_t y, const char **fields)
{
    // The coordinates define the left starting point of the text
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    const char **f = fields;
    int xo = x, yo = y;
    while (*f) {
        display->drawString(xo, yo, *f);
        if ((display->getColor() == BLACK) && config.display.heading_bold)
            display->drawString(xo + 1, yo, *f);

        display->setColor(WHITE);
        yo += FONT_HEIGHT_SMALL;
        if (yo > SCREEN_HEIGHT - FONT_HEIGHT_SMALL) {
            xo += SCREEN_WIDTH / 2;
            yo = 0;
        }
        f++;
    }
}

// Draw nodes status
static void drawNodes(OLEDDisplay *display, int16_t x, int16_t y, const NodeStatus *nodeStatus)
{
    char usersString[20];
    snprintf(usersString, sizeof(usersString), "%d/%d", nodeStatus->getNumOnline(), nodeStatus->getNumTotal());
#if (defined(USE_EINK) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7735_CS) ||      \
     defined(ST7789_CS) || defined(USE_ST7789) || defined(ILI9488_CS) || defined(HX8357_CS)) &&                                  \
    !defined(DISPLAY_FORCE_SMALL_FONTS)
    display->drawFastImage(x, y + 3, 8, 8, imgUser);
#else
    display->drawFastImage(x, y, 8, 8, imgUser);
#endif
    display->drawString(x + 10, y - 2, usersString);
    if (config.display.heading_bold)
        display->drawString(x + 11, y - 2, usersString);
}
#if HAS_GPS
// Draw GPS status summary
static void drawGPS(OLEDDisplay *display, int16_t x, int16_t y, const GPSStatus *gps)
{
    if (config.position.fixed_position) {
        // GPS coordinates are currently fixed
        display->drawString(x - 1, y - 2, "Fixed GPS");
        if (config.display.heading_bold)
            display->drawString(x, y - 2, "Fixed GPS");
        return;
    }
    if (!gps->getIsConnected()) {
        display->drawString(x, y - 2, "No GPS");
        if (config.display.heading_bold)
            display->drawString(x + 1, y - 2, "No GPS");
        return;
    }
    display->drawFastImage(x, y, 6, 8, gps->getHasLock() ? imgPositionSolid : imgPositionEmpty);
    if (!gps->getHasLock()) {
        display->drawString(x + 8, y - 2, "No sats");
        if (config.display.heading_bold)
            display->drawString(x + 9, y - 2, "No sats");
        return;
    } else {
        char satsString[3];
        uint8_t bar[2] = {0};

        // Draw DOP signal bars
        for (int i = 0; i < 5; i++) {
            if (gps->getDOP() <= dopThresholds[i])
                bar[0] = ~((1 << (5 - i)) - 1);
            else
                bar[0] = 0b10000000;
            // bar[1] = bar[0];
            display->drawFastImage(x + 9 + (i * 2), y, 2, 8, bar);
        }

        // Draw satellite image
        display->drawFastImage(x + 24, y, 8, 8, imgSatellite);

        // Draw the number of satellites
        snprintf(satsString, sizeof(satsString), "%u", gps->getNumSatellites());
        display->drawString(x + 34, y - 2, satsString);
        if (config.display.heading_bold)
            display->drawString(x + 35, y - 2, satsString);
    }
}

// Draw status when GPS is disabled or not present
static void drawGPSpowerstat(OLEDDisplay *display, int16_t x, int16_t y, const GPSStatus *gps)
{
    String displayLine;
    int pos;
    if (y < FONT_HEIGHT_SMALL) { // Line 1: use short string
        displayLine = config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_NOT_PRESENT ? "No GPS" : "GPS off";
        pos = SCREEN_WIDTH - display->getStringWidth(displayLine);
    } else {
        displayLine = config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_NOT_PRESENT ? "GPS not present"
                                                                                                       : "GPS is disabled";
        pos = (SCREEN_WIDTH - display->getStringWidth(displayLine)) / 2;
    }
    display->drawString(x + pos, y, displayLine);
}

static void drawGPSAltitude(OLEDDisplay *display, int16_t x, int16_t y, const GPSStatus *gps)
{
    String displayLine = "";
    if (!gps->getIsConnected() && !config.position.fixed_position) {
        // displayLine = "No GPS Module";
        // display->drawString(x + (SCREEN_WIDTH - (display->getStringWidth(displayLine))) / 2, y, displayLine);
    } else if (!gps->getHasLock() && !config.position.fixed_position) {
        // displayLine = "No GPS Lock";
        // display->drawString(x + (SCREEN_WIDTH - (display->getStringWidth(displayLine))) / 2, y, displayLine);
    } else {
        geoCoord.updateCoords(int32_t(gps->getLatitude()), int32_t(gps->getLongitude()), int32_t(gps->getAltitude()));
        displayLine = "Altitude: " + String(geoCoord.getAltitude()) + "m";
        if (config.display.units == meshtastic_Config_DisplayConfig_DisplayUnits_IMPERIAL)
            displayLine = "Altitude: " + String(geoCoord.getAltitude() * METERS_TO_FEET) + "ft";
        display->drawString(x + (SCREEN_WIDTH - (display->getStringWidth(displayLine))) / 2, y, displayLine);
    }
}

// Draw GPS status coordinates
static void drawGPScoordinates(OLEDDisplay *display, int16_t x, int16_t y, const GPSStatus *gps)
{
    auto gpsFormat = config.display.gps_format;
    String displayLine = "";

    if (!gps->getIsConnected() && !config.position.fixed_position) {
        displayLine = "No GPS present";
        display->drawString(x + (SCREEN_WIDTH - (display->getStringWidth(displayLine))) / 2, y, displayLine);
    } else if (!gps->getHasLock() && !config.position.fixed_position) {
        displayLine = "No GPS Lock";
        display->drawString(x + (SCREEN_WIDTH - (display->getStringWidth(displayLine))) / 2, y, displayLine);
    } else {

        geoCoord.updateCoords(int32_t(gps->getLatitude()), int32_t(gps->getLongitude()), int32_t(gps->getAltitude()));

        if (gpsFormat != meshtastic_Config_DisplayConfig_GpsCoordinateFormat_DMS) {
            char coordinateLine[22];
            if (gpsFormat == meshtastic_Config_DisplayConfig_GpsCoordinateFormat_DEC) { // Decimal Degrees
                snprintf(coordinateLine, sizeof(coordinateLine), "%f %f", geoCoord.getLatitude() * 1e-7,
                         geoCoord.getLongitude() * 1e-7);
            } else if (gpsFormat == meshtastic_Config_DisplayConfig_GpsCoordinateFormat_UTM) { // Universal Transverse Mercator
                snprintf(coordinateLine, sizeof(coordinateLine), "%2i%1c %06u %07u", geoCoord.getUTMZone(), geoCoord.getUTMBand(),
                         geoCoord.getUTMEasting(), geoCoord.getUTMNorthing());
            } else if (gpsFormat == meshtastic_Config_DisplayConfig_GpsCoordinateFormat_MGRS) { // Military Grid Reference System
                snprintf(coordinateLine, sizeof(coordinateLine), "%2i%1c %1c%1c %05u %05u", geoCoord.getMGRSZone(),
                         geoCoord.getMGRSBand(), geoCoord.getMGRSEast100k(), geoCoord.getMGRSNorth100k(),
                         geoCoord.getMGRSEasting(), geoCoord.getMGRSNorthing());
            } else if (gpsFormat == meshtastic_Config_DisplayConfig_GpsCoordinateFormat_OLC) { // Open Location Code
                geoCoord.getOLCCode(coordinateLine);
            } else if (gpsFormat == meshtastic_Config_DisplayConfig_GpsCoordinateFormat_OSGR) { // Ordnance Survey Grid Reference
                if (geoCoord.getOSGRE100k() == 'I' || geoCoord.getOSGRN100k() == 'I') // OSGR is only valid around the UK region
                    snprintf(coordinateLine, sizeof(coordinateLine), "%s", "Out of Boundary");
                else
                    snprintf(coordinateLine, sizeof(coordinateLine), "%1c%1c %05u %05u", geoCoord.getOSGRE100k(),
                             geoCoord.getOSGRN100k(), geoCoord.getOSGREasting(), geoCoord.getOSGRNorthing());
            }

            // If fixed position, display text "Fixed GPS" alternating with the coordinates.
            if (config.position.fixed_position) {
                if ((millis() / 10000) % 2) {
                    display->drawString(x + (SCREEN_WIDTH - (display->getStringWidth(coordinateLine))) / 2, y, coordinateLine);
                } else {
                    display->drawString(x + (SCREEN_WIDTH - (display->getStringWidth("Fixed GPS"))) / 2, y, "Fixed GPS");
                }
            } else {
                display->drawString(x + (SCREEN_WIDTH - (display->getStringWidth(coordinateLine))) / 2, y, coordinateLine);
            }
        } else {
            char latLine[22];
            char lonLine[22];
            snprintf(latLine, sizeof(latLine), "%2i簞 %2i' %2u\" %1c", geoCoord.getDMSLatDeg(), geoCoord.getDMSLatMin(),
                     geoCoord.getDMSLatSec(), geoCoord.getDMSLatCP());
            snprintf(lonLine, sizeof(lonLine), "%3i簞 %2i' %2u\" %1c", geoCoord.getDMSLonDeg(), geoCoord.getDMSLonMin(),
                     geoCoord.getDMSLonSec(), geoCoord.getDMSLonCP());
            display->drawString(x + (SCREEN_WIDTH - (display->getStringWidth(latLine))) / 2, y - FONT_HEIGHT_SMALL * 1, latLine);
            display->drawString(x + (SCREEN_WIDTH - (display->getStringWidth(lonLine))) / 2, y, lonLine);
        }
    }
}

static void drawThickLine(OLEDDisplay *display, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t thickness)
{
    if (thickness <= 1) {
        display->drawLine(x0, y0, x1, y1);
        return;
    }

    const int16_t half = thickness / 2;
    for (int16_t offset = -half; offset <= half; ++offset) {
        display->drawLine(x0 + offset, y0, x1 + offset, y1);
        display->drawLine(x0, y0 + offset, x1, y1 + offset);
    }
}

static void drawHermesGpsSatelliteIcon(OLEDDisplay *display, int16_t x, int16_t y, int16_t size)
{
    if (size < 24) {
        size = 24;
    }

    const int16_t cx = x + size / 2;
    const int16_t topY = y + 1;

    int16_t bodyW = size / 7;
    int16_t bodyH = size / 4;
    int16_t panelW = size / 4;
    int16_t panelH = size / 8;
    int16_t panelGap = size / 12;
    int16_t mastH = size / 5;
    int16_t arcR1 = size / 7;
    int16_t arcR2 = size / 5;

    if (bodyW < 4)
        bodyW = 4;
    if (bodyH < 8)
        bodyH = 8;
    if (panelW < 6)
        panelW = 6;
    if (panelH < 3)
        panelH = 3;
    if (panelGap < 2)
        panelGap = 2;
    if (mastH < 4)
        mastH = 4;
    if (arcR1 < 4)
        arcR1 = 4;
    if (arcR2 < 6)
        arcR2 = 6;

    const int16_t bodyX = cx - bodyW / 2;
    const int16_t bodyY = topY + size / 8;
    const int16_t panelY = bodyY + 1;
    const int16_t leftPanelX = bodyX - panelGap - panelW;
    const int16_t rightPanelX = bodyX + bodyW + panelGap;

    display->drawRect(bodyX, bodyY, bodyW, bodyH);
    display->drawRect(leftPanelX, panelY, panelW, panelH);
    display->drawRect(rightPanelX, panelY, panelW, panelH);
    display->drawLine(leftPanelX + panelW / 2, panelY, leftPanelX + panelW / 2, panelY + panelH - 1);
    display->drawLine(rightPanelX + panelW / 2, panelY, rightPanelX + panelW / 2, panelY + panelH - 1);
    display->drawLine(leftPanelX, panelY + panelH / 2, leftPanelX + panelW - 1, panelY + panelH / 2);
    display->drawLine(rightPanelX, panelY + panelH / 2, rightPanelX + panelW - 1, panelY + panelH / 2);

    const int16_t mastY1 = bodyY + bodyH;
    const int16_t mastY2 = mastY1 + mastH;
    display->drawLine(cx, mastY1, cx, mastY2);

    const int16_t dishY = mastY2 + 2;
    display->drawCircle(cx, dishY, 1);

    auto drawSignalArc = [&](int16_t radius, int16_t thickness) {
        const int16_t p1x = cx - radius;
        const int16_t p1y = dishY + 1;
        const int16_t p2x = cx - (radius * 2) / 3;
        const int16_t p2y = dishY + radius / 2;
        const int16_t p3x = cx;
        const int16_t p3y = dishY + (radius * 2) / 3;
        const int16_t p4x = cx + (radius * 2) / 3;
        const int16_t p4y = dishY + radius / 2;
        const int16_t p5x = cx + radius;
        const int16_t p5y = dishY + 1;

        for (int16_t o = 0; o < thickness; ++o) {
            const int16_t yo = o - (thickness / 2);
            display->drawLine(p1x, p1y + yo, p2x, p2y + yo);
            display->drawLine(p2x, p2y + yo, p3x, p3y + yo);
            display->drawLine(p3x, p3y + yo, p4x, p4y + yo);
            display->drawLine(p4x, p4y + yo, p5x, p5y + yo);
        }
    };

    drawSignalArc(arcR1, 1);
    drawSignalArc(arcR2, (size >= 40) ? 2 : 1);
}

static void drawHermesGpsPosterSatelliteMark(OLEDDisplay *display, int16_t cx, int16_t cy, int16_t size)
{
    if (!display) {
        return;
    }

    if (size < 30) {
        size = 30;
    }

    int16_t hubRadius = size / 6;
    if (hubRadius < 8) {
        hubRadius = 8;
    }

    int16_t armSpan = size / 2;
    if (armSpan < hubRadius + 8) {
        armSpan = hubRadius + 8;
    }

    int16_t armThickness = size / 8;
    if (armThickness < 4) {
        armThickness = 4;
    }

    drawThickLine(display, cx - armSpan, cy + armSpan, cx - hubRadius, cy + hubRadius, armThickness);
    drawThickLine(display, cx + hubRadius, cy - hubRadius, cx + armSpan, cy - armSpan, armThickness);
    display->fillCircle(cx, cy, hubRadius);

    const int16_t dishRadius = hubRadius / 2;
    const int16_t dishCx = cx + hubRadius + dishRadius + 6;
    const int16_t dishCy = cy + hubRadius - 1;
    display->fillCircle(dishCx, dishCy, dishRadius);
    display->setColor(BLACK);
    display->fillCircle(dishCx + dishRadius / 2 + 1, dishCy, dishRadius);
    display->setColor(WHITE);
}

static void drawHermesGpsFallbackCornerMapOutline(OLEDDisplay *display, int16_t x, int16_t y, int16_t width, int16_t height)
{
    if (!display || width < 150 || height < 86) {
        return;
    }

    const bool compactLayout = (width < 220 || height < 120);
    const int16_t earthR = compactLayout ? 18 : 26;
    const int16_t earthCx = x + width + (compactLayout ? earthR / 3 : earthR / 4);
    const int16_t earthCy = y + height + (compactLayout ? earthR / 5 : earthR / 7);
    const int16_t haloOuterR = earthR + (compactLayout ? 5 : 7);
    const int16_t haloInnerR = earthR + (compactLayout ? 3 : 5);
    const int16_t landR1 = std::max<int16_t>(2, earthR / 5);
    const int16_t landR2 = std::max<int16_t>(2, earthR / 6);
    const int16_t landR3 = std::max<int16_t>(2, earthR / 7);

    display->drawCircle(earthCx, earthCy, haloOuterR);
    display->drawCircle(earthCx, earthCy, haloInnerR);
    display->drawCircle(earthCx, earthCy, earthR);
    display->drawCircle(earthCx - earthR / 2, earthCy - earthR / 2, landR1);
    display->drawCircle(earthCx - earthR / 3, earthCy - earthR / 8, landR2);
    display->drawCircle(earthCx - earthR / 2, earthCy + earthR / 3, landR3);
}

void Screen::drawLowMemoryReminderOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{
    (void)state;
    if (!display || !screen || !screen->lowMemoryReminderVisible) {
        return;
    }

    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    const int16_t boxX = 4;
    const int16_t boxY = 4;
    const int16_t boxW = width - 8;
    const int16_t boxH = height - 8;
    const int16_t titleBarH = 12;
    const int16_t bodyX = boxX + 5;
    const int16_t bodyW = boxW - 10;
    const int16_t bodyY = boxY + titleBarH + 3;
    const int16_t optionH = 10;
    const int16_t optionY = boxY + boxH - optionH - 2;
    const int16_t optionW = (boxW - 13) / 2;
    const int16_t leftX = boxX + 4;
    const int16_t rightX = leftX + optionW + 5;

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
    const char *title = u8"記憶體警告";
    const int titleW = graphics::HermesX_zh::stringAdvance(title, graphics::HermesX_zh::GLYPH_WIDTH, display);
    int16_t titleX = boxX + (boxW - titleW) / 2;
    if (titleX < boxX + 2) {
        titleX = boxX + 2;
    }
    graphics::HermesX_zh::drawMixedBounded(*display, titleX, boxY + 1, boxW - 4, title, graphics::HermesX_zh::GLYPH_WIDTH,
                                           FONT_HEIGHT_SMALL, nullptr);
    display->setColor(dialogFg);

    const int advance = graphics::HermesX_zh::GLYPH_WIDTH - 1;
    const int16_t lineHeight = 11;
    graphics::HermesX_zh::drawMixedBounded(*display, bodyX, bodyY, bodyW, u8"記憶體偏低，可能導致藍牙或收發異常。", advance,
                                           lineHeight, nullptr);
    graphics::HermesX_zh::drawMixedBounded(*display, bodyX, bodyY + lineHeight, bodyW, u8"請前往 裝置管理 > 節點資料庫 清理。",
                                           advance, lineHeight, nullptr);

    char heapBuf[64];
    snprintf(heapBuf, sizeof(heapBuf), "Heap %lu/%lu", static_cast<unsigned long>(screen->lowMemoryReminderTriggerFree),
             static_cast<unsigned long>(screen->lowMemoryReminderTriggerLargest));
    graphics::HermesX_zh::drawMixedBounded(*display, bodyX, bodyY + lineHeight * 2, bodyW, heapBuf, advance, lineHeight,
                                           nullptr);

    auto drawOption = [&](int16_t optionX, const char *label, bool selected) {
        if (selected) {
            display->setColor(dialogFg);
            display->fillRect(optionX, optionY, optionW, optionH);
            display->setColor(dialogBg);
        } else {
            display->setColor(dialogFg);
            display->drawRect(optionX, optionY, optionW, optionH);
        }
        const int textW = graphics::HermesX_zh::stringAdvance(label, graphics::HermesX_zh::GLYPH_WIDTH, display);
        int16_t textX = optionX + (optionW - textW) / 2;
        if (textX < optionX + 1) {
            textX = optionX + 1;
        }
        graphics::HermesX_zh::drawMixedBounded(*display, textX, optionY + 1, optionW - 2, label,
                                               graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
        display->setColor(dialogFg);
    };

    drawOption(leftX, u8"稍後", screen->lowMemoryReminderSelected == 0);
    drawOption(rightX, u8"前往清理", screen->lowMemoryReminderSelected != 0);
}

static void drawHermesGpsNeonAsciiText(OLEDDisplay *display,
                                       int16_t drawX,
                                       int16_t drawY,
                                       const char *text,
                                       int16_t textH,
                                       uint16_t glowOuterColor,
                                       uint16_t glowInnerColor,
                                       uint16_t coreColor)
{
    if (!display || !text || !*text || textH <= 0) {
        return;
    }

    const int16_t textW = display->getStringWidth(text);
    if (textW <= 0) {
        return;
    }

    const int16_t outerRadius = (textH >= FONT_HEIGHT_LARGE) ? 3 : 2;
    const int16_t innerRadius = (outerRadius > 2) ? 2 : 1;
    addTftColorZone(display,
                    drawX - outerRadius,
                    drawY - outerRadius,
                    textW + outerRadius * 2,
                    textH + outerRadius * 2,
                    glowOuterColor,
                    0x0000);
    addTftColorZone(display,
                    drawX - innerRadius,
                    drawY - innerRadius,
                    textW + innerRadius * 2,
                    textH + innerRadius * 2,
                    glowInnerColor,
                    0x0000);
    addTftColorZone(display, drawX, drawY, textW, textH, coreColor, 0x0000);

    auto drawGlowRing = [&](int16_t radius, bool diagonal) {
        if (radius <= 0) {
            return;
        }
        display->drawString(drawX + radius, drawY, text);
        display->drawString(drawX - radius, drawY, text);
        display->drawString(drawX, drawY + radius, text);
        display->drawString(drawX, drawY - radius, text);
        if (diagonal) {
            display->drawString(drawX + radius, drawY + radius, text);
            display->drawString(drawX - radius, drawY + radius, text);
            display->drawString(drawX + radius, drawY - radius, text);
            display->drawString(drawX - radius, drawY - radius, text);
        }
    };

    if (outerRadius > 0) {
        drawGlowRing(outerRadius, true);
    }
    if (innerRadius > 0 && innerRadius != outerRadius) {
        drawGlowRing(innerRadius, true);
    }
    display->drawString(drawX, drawY, text);
}

static void drawHermesGpsNeonClockText(OLEDDisplay *display,
                                       int16_t drawX,
                                       int16_t drawY,
                                       const char *text,
                                       uint16_t glowOuterColor,
                                       uint16_t glowInnerColor,
                                       uint16_t coreColor)
{
    if (!display || !text || !*text) {
        return;
    }

    const int16_t textW = measurePattanakarnClockText(text);
    if (textW <= 0) {
        return;
    }

    const int16_t glyphH = PattanakarnClock32::kGlyphHeight;
    const int16_t coreX = drawX;
    const int16_t coreY = drawY;
    const int16_t coreW = textW;
    const int16_t coreH = glyphH;
    const int16_t outerRadius = 3;
    const int16_t innerRadius = 1;

    addTftColorZone(display,
                    drawX - outerRadius,
                    drawY - outerRadius,
                    textW + outerRadius * 2,
                    glyphH + outerRadius * 2,
                    glowOuterColor,
                    0x0000);
    addTftColorZone(display,
                    drawX - innerRadius,
                    drawY - innerRadius,
                    textW + innerRadius * 2,
                    glyphH + innerRadius * 2,
                    glowInnerColor,
                    0x0000);
    addTftColorZone(display, drawX, drawY, textW, glyphH, coreColor, 0x0000);

    auto drawGlowRing = [&](int16_t radius, bool diagonal) {
        if (radius <= 0) {
            return;
        }
        drawPattanakarnClockTextOutsideRect(display, drawX + radius, drawY, text, coreX, coreY, coreW, coreH);
        drawPattanakarnClockTextOutsideRect(display, drawX - radius, drawY, text, coreX, coreY, coreW, coreH);
        drawPattanakarnClockTextOutsideRect(display, drawX, drawY + radius, text, coreX, coreY, coreW, coreH);
        drawPattanakarnClockTextOutsideRect(display, drawX, drawY - radius, text, coreX, coreY, coreW, coreH);
        if (diagonal) {
            drawPattanakarnClockTextOutsideRect(display, drawX + radius, drawY + radius, text, coreX, coreY, coreW, coreH);
            drawPattanakarnClockTextOutsideRect(display, drawX - radius, drawY + radius, text, coreX, coreY, coreW, coreH);
            drawPattanakarnClockTextOutsideRect(display, drawX + radius, drawY - radius, text, coreX, coreY, coreW, coreH);
            drawPattanakarnClockTextOutsideRect(display, drawX - radius, drawY - radius, text, coreX, coreY, coreW, coreH);
        }
    };

    if (outerRadius > 0) {
        drawGlowRing(outerRadius, outerRadius > 1);
    }
    if (innerRadius > 0 && innerRadius != outerRadius) {
        drawGlowRing(innerRadius, false);
    }
    drawPattanakarnClockText(display, drawX, drawY, text);
}

static void drawHermesGpsNeonClockTextHalf(OLEDDisplay *display,
                                           int16_t drawX,
                                           int16_t drawY,
                                           const char *text,
                                           uint16_t glowOuterColor,
                                           uint16_t glowInnerColor,
                                           uint16_t coreColor)
{
    if (!display || !text || !*text) {
        return;
    }

    const int16_t textW = measurePattanakarnClockTextHalf(text);
    if (textW <= 0) {
        return;
    }

    const int16_t glyphH = kPattanakarnClockHalfHeight;
    const int16_t coreX = drawX;
    const int16_t coreY = drawY;
    const int16_t coreW = textW;
    const int16_t coreH = glyphH;
    const int16_t outerRadius = 2;
    const int16_t innerRadius = 1;
    addTftColorZone(display,
                    drawX - outerRadius,
                    drawY - outerRadius,
                    textW + outerRadius * 2,
                    glyphH + outerRadius * 2,
                    glowOuterColor,
                    0x0000);
    addTftColorZone(display,
                    drawX - innerRadius,
                    drawY - innerRadius,
                    textW + innerRadius * 2,
                    glyphH + innerRadius * 2,
                    glowInnerColor,
                    0x0000);
    addTftColorZone(display, drawX, drawY, textW, glyphH, coreColor, 0x0000);

    auto drawGlowRing = [&](int16_t radius, bool diagonal) {
        if (radius <= 0) {
            return;
        }
        drawPattanakarnClockTextHalfOutsideRect(display, drawX + radius, drawY, text, coreX, coreY, coreW, coreH);
        drawPattanakarnClockTextHalfOutsideRect(display, drawX - radius, drawY, text, coreX, coreY, coreW, coreH);
        drawPattanakarnClockTextHalfOutsideRect(display, drawX, drawY + radius, text, coreX, coreY, coreW, coreH);
        drawPattanakarnClockTextHalfOutsideRect(display, drawX, drawY - radius, text, coreX, coreY, coreW, coreH);
        if (diagonal) {
            drawPattanakarnClockTextHalfOutsideRect(display, drawX + radius, drawY + radius, text, coreX, coreY, coreW, coreH);
            drawPattanakarnClockTextHalfOutsideRect(display, drawX - radius, drawY + radius, text, coreX, coreY, coreW, coreH);
            drawPattanakarnClockTextHalfOutsideRect(display, drawX + radius, drawY - radius, text, coreX, coreY, coreW, coreH);
            drawPattanakarnClockTextHalfOutsideRect(display, drawX - radius, drawY - radius, text, coreX, coreY, coreW, coreH);
        }
    };

    drawGlowRing(outerRadius, true);
    drawGlowRing(innerRadius, true);
    drawPattanakarnClockTextHalf(display, drawX, drawY, text);
}

struct IncomingNodePopupAsciiStyle {
    bool halfScale = false;
    int16_t xScale = 0;
    int16_t yScale = 0;
    int16_t stroke = 0;
    int16_t letterAdvance = 0;
    int16_t narrowAdvance = 0;
    int16_t spaceAdvance = 0;
    int16_t punctuationAdvance = 0;
    int16_t spacing = 0;
    int16_t glyphHeight = 0;
};

static IncomingNodePopupAsciiStyle makeIncomingNodePopupAsciiStyle(bool halfScale)
{
    IncomingNodePopupAsciiStyle style;
    style.halfScale = halfScale;
    style.xScale = halfScale ? 2 : 4;
    style.yScale = halfScale ? 2 : 4;
    style.stroke = halfScale ? 2 : 3;
    style.letterAdvance = halfScale ? 11 : 20;
    style.narrowAdvance = halfScale ? 8 : 14;
    style.spaceAdvance = halfScale ? 4 : 8;
    style.punctuationAdvance = halfScale ? 9 : 15;
    style.spacing = halfScale ? 1 : 2;
    style.glyphHeight = halfScale ? kPattanakarnClockHalfHeight : PattanakarnClock32::kGlyphHeight;
    return style;
}

static char normalizeIncomingNodePopupAscii(char ch)
{
    if (ch >= 'a' && ch <= 'z') {
        return static_cast<char>(ch - 'a' + 'A');
    }
    return ch;
}

static bool isIncomingNodePopupAsciiLetter(char ch)
{
    const char upper = normalizeIncomingNodePopupAscii(ch);
    return upper >= 'A' && upper <= 'Z';
}

static bool isIncomingNodePopupAsciiSymbol(char ch)
{
    switch (ch) {
    case ' ':
    case '/':
    case '_':
    case '+':
        return true;
    default:
        return false;
    }
}

static bool canRenderIncomingNodePopupTimerText(const char *text)
{
    if (!text || text[0] == '\0') {
        return false;
    }

    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        if (*cursor == ' ' || findPattanakarnClockGlyph(*cursor) || isIncomingNodePopupAsciiLetter(*cursor) ||
            isIncomingNodePopupAsciiSymbol(*cursor)) {
            continue;
        }
        return false;
    }
    return true;
}

static int16_t measureIncomingNodePopupTimerGlyph(char ch, bool halfScale)
{
    const IncomingNodePopupAsciiStyle style = makeIncomingNodePopupAsciiStyle(halfScale);
    if (ch == ' ') {
        return style.spaceAdvance;
    }

    if (const auto *glyph = findPattanakarnClockGlyph(ch)) {
        return halfScale ? static_cast<int16_t>((glyph->width + 1) / 2) : glyph->width;
    }

    switch (normalizeIncomingNodePopupAscii(ch)) {
    case 'I':
        return style.narrowAdvance;
    case '/':
    case '_':
    case '+':
        return style.punctuationAdvance;
    default:
        if (isIncomingNodePopupAsciiLetter(ch)) {
            return style.letterAdvance;
        }
        return 0;
    }
}

static int16_t measureIncomingNodePopupTimerText(const char *text, bool halfScale)
{
    if (!text || text[0] == '\0') {
        return 0;
    }

    int16_t width = 0;
    bool first = true;
    const IncomingNodePopupAsciiStyle style = makeIncomingNodePopupAsciiStyle(halfScale);
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        const int16_t glyphW = measureIncomingNodePopupTimerGlyph(*cursor, halfScale);
        if (glyphW <= 0) {
            return 0;
        }

        if (!first) {
            width += style.spacing;
        }
        width += glyphW;
        first = false;
    }
    return width;
}

static void drawIncomingNodePopupStroke(OLEDDisplay *display, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t stroke)
{
    if (!display || stroke <= 0) {
        return;
    }

    const int16_t half = stroke / 2;
    for (int16_t dx = -half; dx <= half; ++dx) {
        for (int16_t dy = -half; dy <= half; ++dy) {
            display->drawLine(x0 + dx, y0 + dy, x1 + dx, y1 + dy);
        }
    }
}

static void drawIncomingNodePopupVectorGlyph(OLEDDisplay *display, int16_t x, int16_t y, char ch, bool halfScale)
{
    if (!display) {
        return;
    }

    const IncomingNodePopupAsciiStyle style = makeIncomingNodePopupAsciiStyle(halfScale);
    const char upper = normalizeIncomingNodePopupAscii(ch);
    auto sx = [&](int16_t gridX) { return x + (gridX * style.xScale); };
    auto sy = [&](int16_t gridY) { return y + (gridY * style.yScale); };
    auto segment = [&](int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
        drawIncomingNodePopupStroke(display, sx(x0), sy(y0), sx(x1), sy(y1), style.stroke);
    };

    switch (upper) {
    case 'A':
        segment(0, 6, 0, 2);
        segment(0, 2, 2, 0);
        segment(2, 0, 4, 2);
        segment(4, 2, 4, 6);
        segment(1, 3, 3, 3);
        break;
    case 'B':
        segment(0, 0, 0, 6);
        segment(0, 0, 3, 0);
        segment(0, 3, 3, 3);
        segment(0, 6, 3, 6);
        segment(4, 1, 4, 2);
        segment(4, 4, 4, 5);
        break;
    case 'C':
        segment(1, 0, 4, 0);
        segment(0, 1, 0, 5);
        segment(1, 6, 4, 6);
        break;
    case 'D':
        segment(0, 0, 0, 6);
        segment(0, 0, 3, 0);
        segment(0, 6, 3, 6);
        segment(4, 1, 4, 5);
        break;
    case 'E':
        segment(0, 0, 4, 0);
        segment(0, 0, 0, 6);
        segment(0, 3, 3, 3);
        segment(0, 6, 4, 6);
        break;
    case 'F':
        segment(0, 0, 4, 0);
        segment(0, 0, 0, 6);
        segment(0, 3, 3, 3);
        break;
    case 'G':
        segment(1, 0, 4, 0);
        segment(0, 1, 0, 5);
        segment(1, 6, 4, 6);
        segment(2, 3, 4, 3);
        segment(4, 3, 4, 5);
        break;
    case 'H':
        segment(0, 0, 0, 6);
        segment(4, 0, 4, 6);
        segment(0, 3, 4, 3);
        break;
    case 'I':
        segment(0, 0, 4, 0);
        segment(2, 0, 2, 6);
        segment(0, 6, 4, 6);
        break;
    case 'J':
        segment(0, 0, 4, 0);
        segment(4, 0, 4, 5);
        segment(1, 6, 3, 6);
        segment(0, 4, 0, 5);
        break;
    case 'K':
        segment(0, 0, 0, 6);
        segment(4, 0, 0, 3);
        segment(0, 3, 4, 6);
        break;
    case 'L':
        segment(0, 0, 0, 6);
        segment(0, 6, 4, 6);
        break;
    case 'M':
        segment(0, 6, 0, 0);
        segment(0, 0, 2, 3);
        segment(2, 3, 4, 0);
        segment(4, 0, 4, 6);
        break;
    case 'N':
        segment(0, 6, 0, 0);
        segment(0, 0, 4, 6);
        segment(4, 6, 4, 0);
        break;
    case 'O':
        segment(1, 0, 3, 0);
        segment(0, 1, 0, 5);
        segment(4, 1, 4, 5);
        segment(1, 6, 3, 6);
        break;
    case 'P':
        segment(0, 0, 0, 6);
        segment(0, 0, 3, 0);
        segment(0, 3, 3, 3);
        segment(4, 1, 4, 2);
        break;
    case 'Q':
        segment(1, 0, 3, 0);
        segment(0, 1, 0, 5);
        segment(4, 1, 4, 5);
        segment(1, 6, 3, 6);
        segment(2, 4, 4, 6);
        break;
    case 'R':
        segment(0, 0, 0, 6);
        segment(0, 0, 3, 0);
        segment(0, 3, 3, 3);
        segment(4, 1, 4, 2);
        segment(1, 3, 4, 6);
        break;
    case 'S':
        segment(1, 0, 4, 0);
        segment(0, 1, 0, 2);
        segment(1, 3, 3, 3);
        segment(4, 4, 4, 5);
        segment(0, 6, 3, 6);
        break;
    case 'T':
        segment(0, 0, 4, 0);
        segment(2, 0, 2, 6);
        break;
    case 'U':
        segment(0, 0, 0, 5);
        segment(4, 0, 4, 5);
        segment(1, 6, 3, 6);
        break;
    case 'V':
        segment(0, 0, 2, 6);
        segment(4, 0, 2, 6);
        break;
    case 'W':
        segment(0, 0, 0, 6);
        segment(0, 6, 2, 3);
        segment(2, 3, 4, 6);
        segment(4, 6, 4, 0);
        break;
    case 'X':
        segment(0, 0, 4, 6);
        segment(4, 0, 0, 6);
        break;
    case 'Y':
        segment(0, 0, 2, 3);
        segment(4, 0, 2, 3);
        segment(2, 3, 2, 6);
        break;
    case 'Z':
        segment(0, 0, 4, 0);
        segment(4, 0, 0, 6);
        segment(0, 6, 4, 6);
        break;
    case '/':
        segment(0, 6, 4, 0);
        break;
    case '_':
        segment(0, 6, 4, 6);
        break;
    case '+':
        segment(2, 1, 2, 5);
        segment(0, 3, 4, 3);
        break;
    default:
        break;
    }
}

static void drawIncomingNodePopupTimerGlyph(OLEDDisplay *display, int16_t x, int16_t y, char ch, bool halfScale)
{
    if (!display) {
        return;
    }

    if (ch == ' ') {
        return;
    }

    if (findPattanakarnClockGlyph(ch)) {
        const char glyphText[2] = {ch, '\0'};
        if (halfScale) {
            drawPattanakarnClockTextHalf(display, x, y, glyphText);
        } else {
            drawPattanakarnClockText(display, x, y, glyphText);
        }
        return;
    }

    drawIncomingNodePopupVectorGlyph(display, x, y, ch, halfScale);
}

static void drawIncomingNodePopupTimerText(OLEDDisplay *display, int16_t x, int16_t y, const char *text, bool halfScale)
{
    if (!display || !text || text[0] == '\0') {
        return;
    }

    int16_t cursorX = x;
    bool first = true;
    const IncomingNodePopupAsciiStyle style = makeIncomingNodePopupAsciiStyle(halfScale);
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        const int16_t glyphW = measureIncomingNodePopupTimerGlyph(*cursor, halfScale);
        if (glyphW <= 0) {
            continue;
        }
        if (!first) {
            cursorX += style.spacing;
        }
        drawIncomingNodePopupTimerGlyph(display, cursorX, y, *cursor, halfScale);
        cursorX += glyphW;
        first = false;
    }
}

static void drawIncomingNodePopupBannerText(OLEDDisplay *display, int16_t x, int16_t y, int16_t maxWidth, const char *text)
{
    if (!display || !text || text[0] == '\0' || maxWidth <= 0) {
        return;
    }

    const char *cursor = text;
    const char *end = cursor + std::strlen(text);
    int16_t cursorX = x;
    const int16_t maxX = x + maxWidth;
    display->setFont(FONT_SMALL);

    while (cursor < end) {
        std::uint32_t cp = HermesX_zh::nextCodepoint(cursor, end);
        if (cp == 0 || cp == '\n') {
            break;
        }
        if (cp == '\r') {
            continue;
        }

        int16_t advance = 0;
        if (cp >= 0x20u && cp < 0x7Fu) {
            const char ch = static_cast<char>(cp);
            advance = measureIncomingNodePopupTimerGlyph(ch, true);
            if (advance <= 0) {
                ::String asciiString(ch);
                advance = static_cast<int16_t>(display->getStringWidth(asciiString));
                if (advance <= 0) {
                    advance = 6;
                }
            }

            if (cursorX + advance > maxX) {
                break;
            }

            if (measureIncomingNodePopupTimerGlyph(ch, true) > 0) {
                drawIncomingNodePopupTimerGlyph(display, cursorX, y, ch, true);
            } else {
                ::String asciiString(ch);
                display->drawString(cursorX, y + 1, asciiString);
            }
            cursorX += advance;
            continue;
        }

        if (cursorX + HermesX_zh::GLYPH_WIDTH > maxX) {
            break;
        }

        int glyphIndex = HermesX_zh::locateCodepoint(cp);
        if (glyphIndex < 0) {
            HermesX_zh::incrementMissingGlyph();
            glyphIndex = HermesX_zh::fallbackIndex();
        }
        HermesX_zh::drawHanzi12Slow(*display, cursorX, y + 1, glyphIndex);
        cursorX += HermesX_zh::GLYPH_WIDTH;
    }
}

struct DirectIncomingNodePopupLayout {
    bool tiny = false;
    int16_t textX = 0;
    int16_t textW = 0;
    int16_t titleY = 0;
    int16_t idY = 0;
    int16_t nameY = 0;
    int16_t bannerY = 0;
    int16_t bannerH = 0;
    int16_t iconCx = 0;
    int16_t iconCy = 0;
    int16_t outerR = 0;
    int16_t innerR = 0;
    int16_t dotR = 0;
};

static DirectIncomingNodePopupLayout makeDirectIncomingNodePopupLayout(int16_t width, int16_t height)
{
    DirectIncomingNodePopupLayout layout;
    layout.tiny = (width < 260 || height < 160);
    layout.textX = layout.tiny ? std::max<int16_t>(10, width / 9) : std::max<int16_t>(18, width / 10);
    layout.textW = std::max<int16_t>(48, (width * 46) / 100);
    layout.titleY = layout.tiny ? std::max<int16_t>(20, height / 4) : std::max<int16_t>(26, height / 4);
    layout.idY = layout.titleY + (layout.tiny ? 34 : 44);
    layout.bannerH = layout.tiny ? 34 : 42;
    layout.bannerY = height - layout.bannerH;
    layout.nameY = layout.bannerY + std::max<int16_t>(8, (layout.bannerH - 14) / 2);
    layout.dotR = layout.tiny ? std::max<int16_t>(10, height / 14) : std::max<int16_t>(14, height / 13);
    layout.outerR = layout.tiny ? std::max<int16_t>(44, width / 3) : std::max<int16_t>(58, width / 3);
    layout.innerR = (layout.outerR * 2) / 3;
    layout.iconCx = width - layout.outerR - (layout.tiny ? 10 : 16);
    layout.iconCy = layout.bannerY - layout.dotR - (layout.tiny ? 8 : 12);
    if (layout.iconCx < (width / 2)) {
        layout.iconCx = width / 2;
    }
    if (layout.iconCy < (height / 2)) {
        layout.iconCy = height / 2;
    }
    return layout;
}

static void directDrawArcApprox565(TFTDisplay *tft,
                                   int16_t displayW,
                                   int16_t displayH,
                                   int16_t cx,
                                   int16_t cy,
                                   int16_t radius,
                                   float startDeg,
                                   float endDeg,
                                   int16_t thickness,
                                   uint16_t color)
{
    if (!tft || radius <= 0 || thickness <= 0) {
        return;
    }

    const int steps = std::max<int>(12, radius);
    for (int16_t t = 0; t < thickness; ++t) {
        const int16_t rr = radius - t;
        if (rr <= 0) {
            break;
        }

        bool hasPrev = false;
        int16_t px = 0;
        int16_t py = 0;
        for (int i = 0; i <= steps; ++i) {
            const float u = static_cast<float>(i) / static_cast<float>(steps);
            const float deg = startDeg + (endDeg - startDeg) * u;
            const float a = deg * PI / 180.0f;
            const int16_t x = cx + static_cast<int16_t>(lrintf(cosf(a) * rr));
            const int16_t y = cy + static_cast<int16_t>(lrintf(sinf(a) * rr));
            if (hasPrev) {
                directDrawLine565(tft, displayW, displayH, px, py, x, y, color);
            }
            px = x;
            py = y;
            hasPrev = true;
        }
    }
}

static void renderDirectIncomingNodePopupBackdrop(TFTDisplay *tft,
                                                  int16_t width,
                                                  int16_t height,
                                                  const DirectIncomingNodePopupLayout &layout)
{
    if (!tft || width <= 0 || height <= 0) {
        return;
    }

    const bool tiny = layout.tiny;
    const uint16_t black = TFTDisplay::rgb565(0x00, 0x00, 0x00);
    const uint16_t waveCore = TFTDisplay::rgb565(0x1D, 0xD7, 0xFF);
    const uint16_t waveGlowNear = TFTDisplay::rgb565(0x08, 0x67, 0xC8);
    const uint16_t waveGlowFar = TFTDisplay::rgb565(0x03, 0x23, 0x75);
    tft->fillRect565(0, 0, width, height, black);

    const int16_t waveLines = tiny ? 8 : 10;
    const int16_t waveSegments = tiny ? 22 : 24;
    const int16_t waveCoreThickness = 1;
    const int16_t waveNearThickness = tiny ? 0 : 1;
    const int16_t waveFarThickness = tiny ? 0 : 2;
    const int16_t waveTargetY = layout.iconCy - (layout.outerR / 6);
    for (int16_t i = 0; i < waveLines; ++i) {
        const float spread = static_cast<float>(i - (waveLines / 2));
        const int16_t startY = height - 1 - (i * (tiny ? 3 : 5));
        const int16_t endY = waveTargetY + static_cast<int16_t>(spread * (tiny ? 2.2f : 1.9f));
        int16_t prevX = 0;
        int16_t prevY = startY;
        for (int16_t seg = 1; seg <= waveSegments; ++seg) {
            const float t = static_cast<float>(seg) / static_cast<float>(waveSegments);
            const int16_t x = static_cast<int16_t>(lrintf((width - 1) * t));
            const float baseY = static_cast<float>(startY) * (1.0f - t) + static_cast<float>(endY) * t;
            const float amp = (tiny ? 1.4f : 3.2f) * (1.0f - t);
            const float wave = sinf((t * (tiny ? 2.1f : 2.5f) * PI) + (i * 0.32f)) * amp;
            const int16_t y = static_cast<int16_t>(lrintf(baseY + wave));
            directDrawNeonLine565(
                tft, width, height, prevX, prevY, x, y, waveCore, waveGlowNear, waveGlowFar, waveCoreThickness, waveNearThickness,
                waveFarThickness);
            prevX = x;
            prevY = y;
        }
    }

    const int16_t topArcLines = tiny ? 5 : 6;
    const int16_t topArcSegments = 16;
    const int16_t topArcCoreThickness = 1;
    const int16_t topArcNearThickness = tiny ? 0 : 1;
    const int16_t topArcFarThickness = tiny ? 0 : 2;
    for (int16_t i = 0; i < topArcLines; ++i) {
        const int16_t startX = width - 1;
        const int16_t startY = -4 + (i * (tiny ? 3 : 4));
        const int16_t endX = layout.iconCx - (layout.outerR / 3) + (i / 2);
        const int16_t endY = layout.iconCy - (layout.outerR / 2) + (i * (tiny ? 2 : 3));
        int16_t prevX = startX;
        int16_t prevY = startY;
        for (int16_t seg = 1; seg <= topArcSegments; ++seg) {
            const float t = static_cast<float>(seg) / static_cast<float>(topArcSegments);
            const float curve = (1.0f - t) * t;
            const int16_t x = static_cast<int16_t>(lrintf((startX * (1.0f - t)) + (endX * t) - curve * (tiny ? 18.0f : 28.0f)));
            const int16_t y = static_cast<int16_t>(lrintf((startY * (1.0f - t)) + (endY * t) + sinf((t + (i * 0.06f)) * PI) * 2.0f));
            directDrawNeonLine565(
                tft, width, height, prevX, prevY, x, y, waveCore, waveGlowNear, waveGlowFar, topArcCoreThickness, topArcNearThickness,
                topArcFarThickness);
            prevX = x;
            prevY = y;
        }
    }

    const uint16_t bannerBg = TFTDisplay::rgb565(0x0B, 0x11, 0x16);
    const uint16_t bannerEdge = TFTDisplay::rgb565(0x2A, 0x39, 0x46);
    tft->fillRect565(0, layout.bannerY, width, layout.bannerH, bannerBg);
    tft->fillRect565(0, layout.bannerY, width, 1, bannerEdge);
}

static void renderDirectIncomingNodePopupSignalIcon(TFTDisplay *tft,
                                                    int16_t width,
                                                    int16_t height,
                                                    const DirectIncomingNodePopupLayout &layout)
{
    if (!tft || width <= 0 || height <= 0) {
        return;
    }

    const uint16_t core = TFTDisplay::rgb565(0xF8, 0xAB, 0x2E);
    const uint16_t coreHi = TFTDisplay::rgb565(0xFF, 0xC8, 0x63);
    const uint16_t bloomFar = TFTDisplay::rgb565(0x41, 0x13, 0x00);
    const uint16_t bloomNear = TFTDisplay::rgb565(0x7A, 0x28, 0x00);
    const int16_t bloomOuterExtra = layout.tiny ? 6 : 10;
    const int16_t bloomInnerExtra = layout.tiny ? 3 : 5;

    auto drawSoftBloomArc = [&](int16_t baseRadius, int16_t baseThickness) {
        directDrawArcApprox565(tft,
                               width,
                               height,
                               layout.iconCx,
                               layout.iconCy,
                               baseRadius + bloomOuterExtra,
                               -90.0f,
                               0.0f,
                               baseThickness + (layout.tiny ? 4 : 6),
                               bloomFar);
        directDrawArcApprox565(tft,
                               width,
                               height,
                               layout.iconCx,
                               layout.iconCy,
                               baseRadius + bloomInnerExtra,
                               -90.0f,
                               0.0f,
                               baseThickness + (layout.tiny ? 2 : 4),
                               bloomNear);
        directDrawArcApprox565(tft,
                               width,
                               height,
                               layout.iconCx,
                               layout.iconCy,
                               baseRadius + 1,
                               -90.0f,
                               0.0f,
                               baseThickness + 1,
                               coreHi);
        directDrawArcApprox565(tft, width, height, layout.iconCx, layout.iconCy, baseRadius, -90.0f, 0.0f, baseThickness, core);
    };

    drawSoftBloomArc(layout.outerR, layout.tiny ? 7 : 10);
    drawSoftBloomArc(layout.innerR, layout.tiny ? 6 : 8);
    directFillCircle565(tft, width, height, layout.iconCx, layout.iconCy, layout.dotR + (layout.tiny ? 5 : 7), bloomFar);
    directFillCircle565(tft, width, height, layout.iconCx, layout.iconCy, layout.dotR + (layout.tiny ? 2 : 3), bloomNear);
    directFillCircle565(tft, width, height, layout.iconCx, layout.iconCy, layout.dotR + 1, coreHi);
    directFillCircle565(tft, width, height, layout.iconCx, layout.iconCy, layout.dotR, core);
}

static void renderDirectIncomingNodePopupText(TFTDisplay *tft,
                                              int16_t width,
                                              int16_t height,
                                              const DirectIncomingNodePopupLayout &layout,
                                              const meshtastic_NodeInfoLite &node)
{
    if (!tft) {
        return;
    }

    char shortName[12];
    char nameBuf[64];
    formatIncomingNodePopupShortId(node, shortName, sizeof(shortName));
    formatIncomingNodePopupName(node, nameBuf, sizeof(nameBuf), 14);
    const int16_t shortNameW = measureIncomingNodePopupTimerText(shortName);
    const int16_t shortNameX = std::max<int16_t>(layout.textX, layout.textX + ((layout.textW - shortNameW) / 2));
    const int16_t shortNameY = layout.idY - (layout.tiny ? 4 : 6);
    const bool shortNameIsTimerText = canRenderIncomingNodePopupTimerText(shortName);

    tft->clear();
    tft->clearColorPaletteZones();
    tft->setColorPaletteDefaults(0xFFFF, 0x0000);
    tft->setColor(WHITE);
    tft->setTextAlignment(TEXT_ALIGN_LEFT);
    HermesX_zh::drawMixedBounded(*tft, layout.textX, layout.titleY, layout.textW, u8"發現新節點！", HermesX_zh::GLYPH_WIDTH,
                                 layout.tiny ? 16 : 20, nullptr);

    if (shortNameIsTimerText) {
        drawIncomingNodePopupTimerText(tft, shortNameX, shortNameY, shortName);
    } else {
        tft->setFont(layout.tiny ? FONT_MEDIUM : FONT_LARGE);
        HermesX_zh::drawMixedBounded(*tft,
                                     layout.textX + (layout.tiny ? 18 : 26),
                                     layout.idY,
                                     layout.textW - (layout.tiny ? 16 : 24),
                                     shortName,
                                     HermesX_zh::GLYPH_WIDTH,
                                     layout.tiny ? 16 : 20,
                                     nullptr);
    }

    tft->setFont(layout.tiny ? FONT_SMALL : FONT_MEDIUM);
    drawIncomingNodePopupBannerText(tft, layout.textX, layout.nameY, width - (layout.textX * 2), nameBuf);
    tft->overlayBufferForeground565();
}

static void renderDirectIncomingNodePopup(TFTDisplay *tft, int16_t width, int16_t height, const meshtastic_NodeInfoLite &node)
{
    if (!tft || width <= 0 || height <= 0) {
        return;
    }

    const DirectIncomingNodePopupLayout layout = makeDirectIncomingNodePopupLayout(width, height);
    renderDirectIncomingNodePopupBackdrop(tft, width, height, layout);
    renderDirectIncomingNodePopupSignalIcon(tft, width, height, layout);
    renderDirectIncomingNodePopupText(tft, width, height, layout, node);
}

static void drawHermesGpsMonoNeonStringMaxWidth(OLEDDisplay *display,
                                                 int16_t drawX,
                                                 int16_t drawY,
                                                 int16_t maxW,
                                                 const char *text,
                                                 int16_t outerRadius,
                                                 int16_t innerRadius)
{
    if (!display || !text || !*text || maxW <= 0) {
        return;
    }

    auto drawAt = [&](int16_t dx, int16_t dy) { display->drawStringMaxWidth(drawX + dx, drawY + dy, maxW, text); };
    auto drawRing = [&](int16_t radius, bool diagonal) {
        if (radius <= 0) {
            return;
        }
        drawAt(radius, 0);
        drawAt(-radius, 0);
        drawAt(0, radius);
        drawAt(0, -radius);
        if (diagonal) {
            drawAt(radius, radius);
            drawAt(-radius, radius);
            drawAt(radius, -radius);
            drawAt(-radius, -radius);
        }
    };

    if (outerRadius > innerRadius) {
        drawRing(outerRadius, true);
    }
    if (innerRadius > 0) {
        drawRing(innerRadius, true);
    }
    drawAt(0, 0);
}

static void drawHermesGpsMonoNeonMixedBounded(OLEDDisplay *display,
                                               int16_t drawX,
                                               int16_t drawY,
                                               int16_t maxW,
                                               const char *text,
                                               int16_t advanceX,
                                               int16_t lineHeight,
                                               int16_t outerRadius,
                                               int16_t innerRadius)
{
    if (!display || !text || !*text || maxW <= 0 || advanceX <= 0 || lineHeight <= 0) {
        return;
    }

    auto drawAt = [&](int16_t dx, int16_t dy) {
        HermesX_zh::drawMixedBounded(*display, drawX + dx, drawY + dy, maxW, text, advanceX, lineHeight, nullptr);
    };
    auto drawRing = [&](int16_t radius, bool diagonal) {
        if (radius <= 0) {
            return;
        }
        drawAt(radius, 0);
        drawAt(-radius, 0);
        drawAt(0, radius);
        drawAt(0, -radius);
        if (diagonal) {
            drawAt(radius, radius);
            drawAt(-radius, radius);
            drawAt(radius, -radius);
            drawAt(-radius, -radius);
        }
    };

    if (outerRadius > innerRadius) {
        drawRing(outerRadius, true);
    }
    if (innerRadius > 0) {
        drawRing(innerRadius, true);
    }
    drawAt(0, 0);
}

static void drawHermesGpsHeroFrame(OLEDDisplay *display, int16_t x, int16_t y, const GPSStatus *gps)
{
    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    const bool posterTftLayout = supportsDirectTftClockRendering(display);

    if (posterTftLayout) {
        if (x != 0 || y != 0) {
            // During UI slide transitions, only clear our frame tile.
            // Rendering full poster decorations here can leak into adjacent tiles and look like page residue.
            display->setColor(BLACK);
            display->fillRect(x, y, width, height);
            display->setFont(FONT_SMALL);
            return;
        }
        const bool tinyPosterLayout = (width <= 176 || height <= 96);

        const uint16_t colorSatellite = TFTDisplay::rgb565(0x70, 0xB5, 0xD3);
        const uint16_t colorEarthRedGlowOuter = TFTDisplay::rgb565(0x70, 0x08, 0x1A);
        const uint16_t colorEarthRedGlowInner = TFTDisplay::rgb565(0xE4, 0x4C, 0x7A);
        const uint16_t colorEarthGlowOuter = TFTDisplay::rgb565(0x1B, 0x59, 0x73);
        const uint16_t colorEarthGlowInner = TFTDisplay::rgb565(0x67, 0xBA, 0xD7);
        const uint16_t colorEarthBase = TFTDisplay::rgb565(0xA9, 0xD0, 0xE0);
        const uint16_t colorEarthLand = TFTDisplay::rgb565(0x6D, 0xB1, 0xC9);
        const uint16_t colorGpsGlowOuter = TFTDisplay::rgb565(0x8A, 0x2A, 0x00);
        const uint16_t colorGpsGlowInner = TFTDisplay::rgb565(0xFF, 0xC5, 0x5F);
        const uint16_t colorGpsCore = TFTDisplay::rgb565(0xFF, 0xF4, 0xDB);
        const uint16_t colorOnGlowOuter = TFTDisplay::rgb565(0x00, 0x56, 0xAF);
        const uint16_t colorOnGlowInner = TFTDisplay::rgb565(0x7D, 0xE5, 0xFF);
        const uint16_t colorOnCore = TFTDisplay::rgb565(0xE4, 0xF5, 0xFF);
        const uint16_t colorOffGlowOuter = TFTDisplay::rgb565(0x1B, 0x12, 0x12);
        const uint16_t colorOffGlowInner = TFTDisplay::rgb565(0xA8, 0x58, 0x58);
        const uint16_t colorOffCore = TFTDisplay::rgb565(0xFF, 0xE8, 0xE8);
        const bool gpsEnabled = config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_ENABLED;

        display->setTextAlignment(TEXT_ALIGN_LEFT);
        display->setColor(BLACK);
        display->fillRect(x, y, width, height);

        static const uint8_t kStars[][3] = {
            {5, 4, 2},   {17, 16, 2}, {45, 9, 2},   {65, 6, 2},  {6, 50, 2},   {38, 61, 2},
            {58, 44, 2}, {27, 85, 2}, {2, 97, 2},   {42, 98, 2}, {58, 86, 2},  {49, 29, 2},
        };
        display->setColor(WHITE);
        for (const auto &star : kStars) {
            const int16_t sx = x + static_cast<int16_t>((width * star[0]) / 100);
            const int16_t sy = y + static_cast<int16_t>((height * star[1]) / 100);
            display->fillCircle(sx, sy, tinyPosterLayout ? 1 : star[2]);
        }

        int16_t satSize = (width < height) ? width : height;
        satSize = (satSize * (tinyPosterLayout ? 58 : 52)) / 100;
        if (satSize < (tinyPosterLayout ? 34 : 46)) {
            satSize = tinyPosterLayout ? 34 : 46;
        }
        const int16_t satCx = x + (width * (tinyPosterLayout ? 16 : 18)) / 100;
        const int16_t satCy = y + (height * (tinyPosterLayout ? 52 : 49)) / 100;
        const int16_t satPad = tinyPosterLayout ? 9 : 12;
        addTftColorZone(display,
                        satCx - satSize / 2 - satPad,
                        satCy - satSize / 2 - satPad,
                        satSize + satPad * 2,
                        satSize + satPad * 2,
                        colorSatellite,
                        0x0000);
        drawHermesGpsPosterSatelliteMark(display, satCx, satCy, satSize);

        int16_t earthR = (height * (tinyPosterLayout ? 52 : 64)) / 100;
        if (earthR < (tinyPosterLayout ? 28 : 40)) {
            earthR = tinyPosterLayout ? 28 : 40;
        }
        const int16_t earthCx = x + width + (tinyPosterLayout ? std::max<int16_t>(4, earthR / 5) : earthR / 3);
        const int16_t earthCy = y + height + (tinyPosterLayout ? std::max<int16_t>(4, earthR / 6) : earthR / 2);
        const int16_t earthHaloRedOuterR = earthR + (tinyPosterLayout ? 10 : 13);
        const int16_t earthHaloRedInnerR = earthR + (tinyPosterLayout ? 7 : 9);
        const int16_t earthHaloOuterR = earthR + (tinyPosterLayout ? 4 : 6);
        const int16_t earthHaloInnerR = earthR + (tinyPosterLayout ? 2 : 4);
        addTftColorZone(display,
                        earthCx - earthHaloRedOuterR,
                        earthCy - earthHaloRedOuterR,
                        earthHaloRedOuterR * 2 + 2,
                        earthHaloRedOuterR * 2 + 2,
                        colorEarthRedGlowOuter,
                        0x0000);
        addTftColorZone(display,
                        earthCx - earthHaloRedInnerR,
                        earthCy - earthHaloRedInnerR,
                        earthHaloRedInnerR * 2 + 2,
                        earthHaloRedInnerR * 2 + 2,
                        colorEarthRedGlowInner,
                        0x0000);
        addTftColorZone(display,
                        earthCx - earthHaloOuterR,
                        earthCy - earthHaloOuterR,
                        earthHaloOuterR * 2 + 2,
                        earthHaloOuterR * 2 + 2,
                        colorEarthGlowOuter,
                        0x0000);
        addTftColorZone(display,
                        earthCx - earthHaloInnerR,
                        earthCy - earthHaloInnerR,
                        earthHaloInnerR * 2 + 2,
                        earthHaloInnerR * 2 + 2,
                        colorEarthGlowInner,
                        0x0000);
        addTftColorZone(display, earthCx - earthR, earthCy - earthR, earthR * 2 + 2, earthR * 2 + 2, colorEarthBase, 0x0000);
        addTftColorZone(display,
                        earthCx - earthR + earthR / 2,
                        earthCy - earthR + earthR / 4,
                        earthR,
                        earthR + (earthR / 2),
                        colorEarthLand,
                        0x0000);
        display->setColor(WHITE);
        display->fillCircle(earthCx, earthCy, earthHaloRedOuterR);
        display->fillCircle(earthCx, earthCy, earthHaloRedInnerR);
        display->fillCircle(earthCx, earthCy, earthHaloOuterR);
        display->fillCircle(earthCx, earthCy, earthHaloInnerR);
        display->fillCircle(earthCx, earthCy, earthR);
        display->fillCircle(earthCx - earthR / 2, earthCy - earthR / 2, earthR / 4);
        display->fillCircle(earthCx - earthR / 3, earthCy - earthR / 8, earthR / 5);
        display->fillCircle(earthCx - earthR / 2, earthCy + earthR / 3, earthR / 5);
        display->fillCircle(earthCx - earthR / 5, earthCy + earthR / 6, earthR / 7);

        display->setFont(tinyPosterLayout ? FONT_MEDIUM : FONT_LARGE);
        int16_t titleFontH = tinyPosterLayout ? FONT_HEIGHT_MEDIUM : FONT_HEIGHT_LARGE;
        if (!tinyPosterLayout && display->getStringWidth("GPS ON") > ((width * 52) / 100)) {
            display->setFont(FONT_MEDIUM);
            titleFontH = FONT_HEIGHT_MEDIUM;
        }
        const char *gpsWord = "GPS";
        const char *onWord = gpsEnabled ? "ON" : "OFF";
        int16_t titleGap = width / 30;
        if (titleGap < (tinyPosterLayout ? 4 : 6)) {
            titleGap = tinyPosterLayout ? 4 : 6;
        }
        const int16_t gpsW = display->getStringWidth(gpsWord);
        const int16_t onW = display->getStringWidth(onWord);
        const int16_t titleY = y + (height * (tinyPosterLayout ? 9 : 17)) / 100;
        const int16_t titleCenterX = x + (width * (tinyPosterLayout ? 60 : 58)) / 100;
        const int16_t titleX = titleCenterX - (gpsW + titleGap + onW) / 2;
        const uint16_t stateGlowOuter = gpsEnabled ? colorOnGlowOuter : colorOffGlowOuter;
        const uint16_t stateGlowInner = gpsEnabled ? colorOnGlowInner : colorOffGlowInner;
        const uint16_t stateCore = gpsEnabled ? colorOnCore : colorOffCore;
        drawHermesGpsNeonAsciiText(
            display, titleX, titleY, gpsWord, titleFontH, colorGpsGlowOuter, colorGpsGlowInner, colorGpsCore);
        drawHermesGpsNeonAsciiText(
            display, titleX + gpsW + titleGap, titleY, onWord, titleFontH, stateGlowOuter, stateGlowInner, stateCore);

        // Coordinates are rendered after ui->update() via direct-TFT neon (same pipeline as Home timer).
        (void)gps;

        display->setFont(FONT_SMALL);
        return;
    }

    const bool largeLayout = (width >= 240 && height >= 130);
    const bool compactLayout = (width < 220 || height < 120);
    const int16_t compactLineH = _fontHeight(FONT_SMALL_LOCAL);

    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setColor(BLACK);
    display->fillRect(x, y, width, height);
    display->setColor(WHITE);
    display->setFont(compactLayout ? FONT_SMALL_LOCAL : FONT_SMALL);

    // Reserve a bottom strip to avoid colliding with HermesX menu footer overlay.
    const int16_t footerReserve = compactLayout ? 2 : (FONT_HEIGHT_SMALL + 4);
    int16_t usableHeight = height - footerReserve;
    if (usableHeight < (height / 2)) {
        usableHeight = height;
    }

    int16_t leftPaneWidth = compactLayout ? ((width * 34) / 100) : ((width * 40) / 100);
    if (leftPaneWidth < (compactLayout ? 50 : 72)) {
        leftPaneWidth = compactLayout ? 50 : 72;
    }
    if (leftPaneWidth > width - (compactLayout ? 80 : 92)) {
        leftPaneWidth = width - (compactLayout ? 80 : 92);
    }

    int16_t iconSize = leftPaneWidth - (compactLayout ? 8 : (largeLayout ? 16 : 12));
    const int16_t maxIconHeight =
        usableHeight - (compactLayout ? compactLineH : FONT_HEIGHT_SMALL) - (compactLayout ? 10 : (largeLayout ? 20 : 14));
    if (iconSize > maxIconHeight) {
        iconSize = maxIconHeight;
    }
    if (iconSize < (compactLayout ? 20 : 24)) {
        iconSize = compactLayout ? 20 : 24;
    }

    const int16_t iconX = x + (leftPaneWidth - iconSize) / 2;
    const int16_t iconY = y + (compactLayout ? 2 : (largeLayout ? 8 : 4));
    drawHermesGpsSatelliteIcon(display, iconX, iconY, iconSize);

    char satLine[48];
    snprintf(satLine, sizeof(satLine), compactLayout ? u8"衛星:%u" : u8"衛星數量：%u", gps->getNumSatellites());
    const int16_t satY = y + usableHeight - (compactLayout ? compactLineH : FONT_HEIGHT_SMALL) - 2;
    HermesX_zh::drawMixedBounded(*display, x + 4, satY, leftPaneWidth - 8, satLine,
                                 HermesX_zh::GLYPH_WIDTH, compactLayout ? compactLineH : FONT_HEIGHT_SMALL, nullptr);

    const int16_t rightGap = compactLayout ? 6 : (largeLayout ? 10 : 8);
    const int16_t rightX = x + leftPaneWidth + rightGap;
    const int16_t rightWidth = width - leftPaneWidth - rightGap - 4;
    if (rightWidth < 30) {
        return;
    }

    const bool gpsEnabled = config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_ENABLED;
    const char *gpsTitle = gpsEnabled ? "GPS ON" : "GPS OFF";
    const char *lockStatus = u8"已停用";
    const char *lockStatusShort = u8"停用";
    if (gpsEnabled) {
        if (config.position.fixed_position) {
            lockStatus = u8"固定座標";
            lockStatusShort = u8"固定";
        } else if (!gps->getIsConnected()) {
            lockStatus = u8"無 GPS";
            lockStatusShort = u8"無GPS";
        } else if (gps->getHasLock()) {
            lockStatus = u8"已鎖定";
            lockStatusShort = u8"鎖定";
        } else {
            lockStatus = u8"搜尋中";
            lockStatusShort = u8"搜尋";
        }
    }

    const int decimals = compactLayout ? 5 : (largeLayout ? 7 : 6);
    char lonLine[24] = "--";
    char latLine[24] = "--";
    const bool hasGpsCoordinates = (config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_ENABLED) &&
                                   (config.position.fixed_position || (gps->getIsConnected() && gps->getHasLock()));
    if (hasGpsCoordinates) {
        const double lat = static_cast<double>(gps->getLatitude()) * 1e-7;
        const double lon = static_cast<double>(gps->getLongitude()) * 1e-7;
        snprintf(lonLine, sizeof(lonLine), "%.*f", decimals, lon);
        snprintf(latLine, sizeof(latLine), "%.*f", decimals, lat);
    }

    char dateLine[20] = "--/--/--";
    char timeLine[20] = "--:--:--";
    char compactTimeLine[24] = "--/-- --:--";
    const uint32_t rtcSec = getValidTime(RTCQuality::RTCQualityDevice, true);
    if (rtcSec > 0) {
        time_t t = static_cast<time_t>(rtcSec);
        tm *localTm = gmtime(&t);
        if (localTm) {
            snprintf(dateLine, sizeof(dateLine), "%04d/%d/%d", localTm->tm_year + 1900, localTm->tm_mon + 1,
                     localTm->tm_mday);
            if (config.display.use_12h_clock) {
                int hour = localTm->tm_hour;
                const char *suffix = "am";
                if (hour >= 12) {
                    suffix = "pm";
                    if (hour > 12) {
                        hour -= 12;
                    }
                }
                if (hour == 0) {
                    hour = 12;
                }
                snprintf(timeLine, sizeof(timeLine), "%d:%02d:%02d%s", hour, localTm->tm_min, localTm->tm_sec, suffix);
            } else {
                snprintf(timeLine, sizeof(timeLine), "%02d:%02d:%02d", localTm->tm_hour, localTm->tm_min, localTm->tm_sec);
            }
            snprintf(compactTimeLine, sizeof(compactTimeLine), "%d/%d %02d:%02d", localTm->tm_mon + 1, localTm->tm_mday,
                     localTm->tm_hour, localTm->tm_min);
        }
    }

    if (compactLayout) {
        int16_t rowY = y + 2;

        display->setFont(FONT_SMALL_LOCAL);
        drawHermesGpsMonoNeonStringMaxWidth(display, rightX, rowY, rightWidth, gpsTitle, 1, 0);
        rowY += compactLineH + 1;

        char lockLineCompact[40];
        snprintf(lockLineCompact, sizeof(lockLineCompact), "%s%s", u8"定位:", lockStatusShort);
        drawHermesGpsMonoNeonMixedBounded(
            display, rightX, rowY, rightWidth, lockLineCompact, HermesX_zh::GLYPH_WIDTH, compactLineH, 1, 0);
        rowY += compactLineH + 1;

        const char *coordLabelCompact = u8"座標:";
        const int16_t coordLabelCompactW = HermesX_zh::stringAdvance(coordLabelCompact, HermesX_zh::GLYPH_WIDTH, display);
        const int16_t coordCompactValueX = rightX + coordLabelCompactW + 2;
        const int16_t coordCompactValueW = rightWidth - (coordCompactValueX - rightX);
        HermesX_zh::drawMixedBounded(*display, rightX, rowY, rightWidth, coordLabelCompact, HermesX_zh::GLYPH_WIDTH,
                                     compactLineH, nullptr);
        display->setFont(FONT_SMALL_LOCAL);
        drawHermesGpsMonoNeonStringMaxWidth(display, coordCompactValueX, rowY, coordCompactValueW, lonLine, 1, 0);
        rowY += compactLineH;
        drawHermesGpsMonoNeonStringMaxWidth(display, coordCompactValueX, rowY, coordCompactValueW, latLine, 1, 0);
        rowY += compactLineH + 1;

        const char *timeLabelCompact = u8"時間:";
        const int16_t timeLabelCompactW = HermesX_zh::stringAdvance(timeLabelCompact, HermesX_zh::GLYPH_WIDTH, display);
        const int16_t timeCompactValueX = rightX + timeLabelCompactW + 2;
        const int16_t timeCompactValueW = rightWidth - (timeCompactValueX - rightX);
        HermesX_zh::drawMixedBounded(*display, rightX, rowY, rightWidth, timeLabelCompact, HermesX_zh::GLYPH_WIDTH,
                                     compactLineH, nullptr);
        drawHermesGpsMonoNeonStringMaxWidth(display, timeCompactValueX, rowY, timeCompactValueW, compactTimeLine, 1, 0);
        display->setFont(FONT_SMALL_LOCAL);
        return;
    }

    const int16_t valueFontHeight = largeLayout ? FONT_HEIGHT_MEDIUM : FONT_HEIGHT_SMALL;
    const int16_t sectionGap = largeLayout ? 8 : 4;
    const int16_t valueGap = largeLayout ? 2 : 1;
    int16_t rowY = y + (largeLayout ? 8 : 6);

    display->setFont(largeLayout ? FONT_MEDIUM : FONT_SMALL);
    const int16_t titleLineH = largeLayout ? FONT_HEIGHT_MEDIUM : FONT_HEIGHT_SMALL;
    drawHermesGpsMonoNeonStringMaxWidth(display, rightX, rowY, rightWidth, gpsTitle, 2, 1);
    rowY += titleLineH + sectionGap;

    char lockLine[64];
    snprintf(lockLine, sizeof(lockLine), "%s%s", u8"衛星定位：", lockStatus);
    drawHermesGpsMonoNeonMixedBounded(display,
                                      rightX,
                                      rowY,
                                      rightWidth,
                                      lockLine,
                                      HermesX_zh::GLYPH_WIDTH,
                                      largeLayout ? FONT_HEIGHT_MEDIUM : FONT_HEIGHT_SMALL,
                                      1,
                                      0);
    rowY += (largeLayout ? FONT_HEIGHT_MEDIUM : FONT_HEIGHT_SMALL) + sectionGap;

    const char *coordLabel = u8"座標：";
    const int16_t coordLabelW = HermesX_zh::stringAdvance(coordLabel, HermesX_zh::GLYPH_WIDTH, display);
    const int16_t coordValueX = rightX + coordLabelW + (largeLayout ? 8 : 4);
    const int16_t coordValueW = rightWidth - (coordValueX - rightX);
    HermesX_zh::drawMixedBounded(*display, rightX, rowY, rightWidth, coordLabel, HermesX_zh::GLYPH_WIDTH,
                                 largeLayout ? FONT_HEIGHT_MEDIUM : FONT_HEIGHT_SMALL, nullptr);
    display->setFont(largeLayout ? FONT_MEDIUM : FONT_SMALL);
    drawHermesGpsMonoNeonStringMaxWidth(display, coordValueX, rowY, coordValueW, lonLine, 2, 1);
    rowY += valueFontHeight + valueGap;
    drawHermesGpsMonoNeonStringMaxWidth(display, coordValueX, rowY, coordValueW, latLine, 2, 1);
    rowY += valueFontHeight + sectionGap;

    const char *timeLabel = u8"時間：";
    const int16_t timeLabelW = HermesX_zh::stringAdvance(timeLabel, HermesX_zh::GLYPH_WIDTH, display);
    const int16_t timeValueX = rightX + timeLabelW + (largeLayout ? 8 : 4);
    const int16_t timeValueW = rightWidth - (timeValueX - rightX);
    display->setFont(FONT_SMALL);
    HermesX_zh::drawMixedBounded(*display, rightX, rowY, rightWidth, timeLabel, HermesX_zh::GLYPH_WIDTH,
                                 largeLayout ? FONT_HEIGHT_MEDIUM : FONT_HEIGHT_SMALL, nullptr);
    display->setFont(largeLayout ? FONT_MEDIUM : FONT_SMALL);
    display->drawStringMaxWidth(timeValueX, rowY, timeValueW, dateLine);
    rowY += valueFontHeight + valueGap;
    display->drawStringMaxWidth(timeValueX, rowY, timeValueW, timeLine);
    drawHermesGpsFallbackCornerMapOutline(display, x, y, width, usableHeight);
    display->setFont(FONT_SMALL);
}
#endif
/**
 * Given a recent lat/lon return a guess of the heading the user is walking on.
 *
 * We keep a series of "after you've gone 10 meters, what is your heading since
 * the last reference point?"
 */
float Screen::estimatedHeading(double lat, double lon)
{
    static double oldLat, oldLon;
    static float b;

    if (oldLat == 0) {
        // just prepare for next time
        oldLat = lat;
        oldLon = lon;

        return b;
    }

    float d = GeoCoord::latLongToMeter(oldLat, oldLon, lat, lon);
    if (d < 10) // haven't moved enough, just keep current bearing
        return b;

    b = GeoCoord::bearing(oldLat, oldLon, lat, lon);
    oldLat = lat;
    oldLon = lon;

    return b;
}

/// We will skip one node - the one for us, so we just blindly loop over all
/// nodes
static size_t nodeIndex;
static int8_t prevFrame = -1;

constexpr int16_t kSetupHeaderHeight = 14;
constexpr int16_t kSetupRowHeight = 12;
constexpr uint8_t kSetupVisibleRows = 4;
constexpr size_t kSetupPassMaxLen = 20;
constexpr uint32_t kSetupNavMinIntervalMs = 80;
constexpr uint32_t kSetupNavFlipGuardMs = 800;
constexpr uint8_t kMainActionVisibleSlots = 3;
constexpr uint8_t kMainActionCount = 9;
constexpr uint32_t kStealthConfirmArmMs = 3000;
constexpr uint32_t kStealthWakeMs = 1000;
constexpr uint32_t kLowMemoryReminderFreeThreshold = 6 * 1024;
constexpr uint32_t kLowMemoryReminderLargestThreshold = 4 * 1024;
constexpr uint32_t kLowMemoryReminderSuppressMs = 5 * 60 * 1000;
#if HERMESX_CIV_DISABLE_EMAC
static const char *kSetupRootItems[] = {u8"返回", u8"UI設定", u8"裝置管理", u8"罐頭訊息", u8"儲存並重新開機"};
#else
static const char *kSetupRootItems[] = {u8"返回", u8"EMAC設定", u8"UI設定", u8"裝置管理", u8"罐頭訊息",
                                        u8"儲存並重新開機"};
#endif
static const uint8_t kSetupRootCount = sizeof(kSetupRootItems) / sizeof(kSetupRootItems[0]);
static const char *kSetupEmacItems[] = {u8"返回", u8"設定密碼A", u8"設定密碼B", u8"顯示密碼", u8"EMAC解除"};
static const uint8_t kSetupEmacCount = sizeof(kSetupEmacItems) / sizeof(kSetupEmacItems[0]);
static const uint8_t kSetupNodeMenuCount = 8;
static const uint8_t kSetupPowerMenuCount = 4;
static const uint8_t kSetupUiMenuCount = 7;
static const uint8_t kSetupMqttMenuCount = 4;
static const uint8_t kSetupNodeDatabaseMenuCount = 2;
static const uint8_t kSetupNodeDatabaseResetCount = 4;
static const uint8_t kSetupMqttMapReportMenuCount = 4;
static const uint8_t kSetupChannelDetailMenuCount = 5;
static const uint8_t kSetupLoraMenuCount = 9;
static const uint8_t kSetupGpsMenuCount = 6;
static const uint8_t kSetupMaxRegionOptions = 24;

struct SetupLoraPresetOption {
    meshtastic_Config_LoRaConfig_ModemPreset preset;
    const char *label;
};

struct SetupRoleOption {
    meshtastic_Config_DeviceConfig_Role role;
    const char *label;
};

static const SetupLoraPresetOption kSetupLoraPresetOptions[] = {
    {meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST, "LongFast"},
    {meshtastic_Config_LoRaConfig_ModemPreset_LONG_SLOW, "LongSlow"},
    {meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_FAST, "MediumFast"},
    {meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_SLOW, "MediumSlow"},
    {meshtastic_Config_LoRaConfig_ModemPreset_SHORT_FAST, "ShortFast"},
    {meshtastic_Config_LoRaConfig_ModemPreset_SHORT_SLOW, "ShortSlow"},
    {meshtastic_Config_LoRaConfig_ModemPreset_LONG_MODERATE, "LongMod"},
    {meshtastic_Config_LoRaConfig_ModemPreset_SHORT_TURBO, "ShortTurbo"},
};
static const uint8_t kSetupLoraPresetOptionCount = sizeof(kSetupLoraPresetOptions) / sizeof(kSetupLoraPresetOptions[0]);

static const SetupRoleOption kSetupRoleOptions[] = {
    {meshtastic_Config_DeviceConfig_Role_CLIENT, "Client"},
    {meshtastic_Config_DeviceConfig_Role_CLIENT_MUTE, "Client Mute"},
    {meshtastic_Config_DeviceConfig_Role_CLIENT_HIDDEN, "Client Hidden"},
    {meshtastic_Config_DeviceConfig_Role_TRACKER, "Tracker"},
    {meshtastic_Config_DeviceConfig_Role_SENSOR, "Sensor"},
    {meshtastic_Config_DeviceConfig_Role_TAK, "TAK"},
    {meshtastic_Config_DeviceConfig_Role_TAK_TRACKER, "TAK Tracker"},
    {meshtastic_Config_DeviceConfig_Role_LOST_AND_FOUND, "Lost&Found"},
};
static const uint8_t kSetupRoleOptionCount = sizeof(kSetupRoleOptions) / sizeof(kSetupRoleOptions[0]);

static const uint32_t kSetupGpsUpdateOptions[] = {30, 60, 120, 300, 600, 1800};
static const uint8_t kSetupGpsUpdateCount = sizeof(kSetupGpsUpdateOptions) / sizeof(kSetupGpsUpdateOptions[0]);
static const uint32_t kSetupGpsBroadcastOptions[] = {60, 300, 600, 900, 1800, 3600};
static const uint8_t kSetupGpsBroadcastCount = sizeof(kSetupGpsBroadcastOptions) / sizeof(kSetupGpsBroadcastOptions[0]);
static const char *kSetupGpsUpdateLabels[] = {u8"30秒", u8"60秒", u8"2分鐘", u8"5分鐘", u8"10分鐘", u8"30分鐘"};
static const char *kSetupGpsBroadcastLabels[] = {u8"1分鐘", u8"5分鐘", u8"10分鐘", u8"15分鐘", u8"30分鐘", u8"60分鐘"};
static const uint32_t kSetupGpsSmartDistanceOptions[] = {10, 20, 50, 100, 250, 500, 1000, 2000};
static const uint8_t kSetupGpsSmartDistanceCount =
    sizeof(kSetupGpsSmartDistanceOptions) / sizeof(kSetupGpsSmartDistanceOptions[0]);
static const char *kSetupGpsSmartDistanceLabels[] = {u8"10公尺", u8"20公尺", u8"50公尺", u8"100公尺",
                                                     u8"250公尺", u8"500公尺", u8"1公里",  u8"2公里"};
static const uint32_t kSetupGpsSmartIntervalOptions[] = {15, 30, 60, 120, 300, 600, 1800};
static const uint8_t kSetupGpsSmartIntervalCount =
    sizeof(kSetupGpsSmartIntervalOptions) / sizeof(kSetupGpsSmartIntervalOptions[0]);
static const char *kSetupGpsSmartIntervalLabels[] = {u8"15秒", u8"30秒", u8"1分鐘", u8"2分鐘", u8"5分鐘", u8"10分鐘",
                                                     u8"30分鐘"};

static const uint32_t kSetupMqttMapPublishOptions[] = {3600, 7200, 10800, 21600, 43200, 86400};
static const uint8_t kSetupMqttMapPublishCount =
    sizeof(kSetupMqttMapPublishOptions) / sizeof(kSetupMqttMapPublishOptions[0]);
static const char *kSetupMqttMapPublishLabels[] = {u8"1小時", u8"2小時", u8"3小時", u8"6小時", u8"12小時", u8"24小時"};

struct SetupPrecisionOption {
    uint32_t value;
    const char *label;
};

static const SetupPrecisionOption kSetupMqttMapPrecisionOptions[] = {
    {15, "15"},
    {14, "14"},
    {13, "13"},
    {12, "12"},
};
static const uint8_t kSetupMqttMapPrecisionCount =
    sizeof(kSetupMqttMapPrecisionOptions) / sizeof(kSetupMqttMapPrecisionOptions[0]);

static const SetupPrecisionOption kSetupChannelPrecisionOptions[] = {
    {32, u8"最低(最精準)"},
    {19, u8"低"},
    {16, u8"中"},
    {13, u8"高"},
    {10, u8"最高(最模糊)"},
};
static const uint8_t kSetupChannelPrecisionCount =
    sizeof(kSetupChannelPrecisionOptions) / sizeof(kSetupChannelPrecisionOptions[0]);

struct SetupBrightnessOption {
    uint8_t value;
    const char *label;
};

struct SetupVoltageOption {
    uint16_t millivolts;
    const char *label;
};

static const SetupVoltageOption kSetupPowerGuardThresholdOptions[] = {
    {3000, "3.0V"},
    {3100, "3.1V"},
    {3200, "3.2V"},
    {3300, "3.3V"},
    {3400, "3.4V"},
    {3500, "3.5V"},
    {3600, "3.6V"},
    {3700, "3.7V"},
};
static const uint8_t kSetupPowerGuardThresholdCount =
    sizeof(kSetupPowerGuardThresholdOptions) / sizeof(kSetupPowerGuardThresholdOptions[0]);

static const SetupBrightnessOption kSetupBrightnessOptions[] = {
    {0, u8"關閉"},
    {30, u8"低"},
    {60, u8"中"},
    {120, u8"高"},
    {200, u8"最大"},
};
static const uint8_t kSetupBrightnessCount = sizeof(kSetupBrightnessOptions) / sizeof(kSetupBrightnessOptions[0]);

struct SetupScreenSleepOption {
    uint16_t seconds;
    const char *label;
};
static const SetupScreenSleepOption kSetupScreenSleepOptions[] = {
#if defined(USE_EINK)
    {0, u8"關閉"},
#endif
    {10, u8"10秒"},
    {30, u8"30秒"},
    {60, u8"1分鐘"},
    {120, u8"2分鐘"},
    {300, u8"5分鐘"},
    {600, u8"10分鐘"},
    {900, u8"15分鐘"},
};
static const uint8_t kSetupScreenSleepCount = sizeof(kSetupScreenSleepOptions) / sizeof(kSetupScreenSleepOptions[0]);

struct SetupTimezoneOption {
    const char *tz;
    const char *label;
};

static const SetupTimezoneOption kSetupTimezoneOptions[] = {
    {"GMT0", "UTC"},
    {"GMT+12", "GMT-12"},
    {"GMT+11", "GMT-11"},
    {"GMT+10", "GMT-10"},
    {"GMT+9", "GMT-9"},
    {"GMT+8", "GMT-8"},
    {"GMT+7", "GMT-7"},
    {"GMT+6", "GMT-6"},
    {"GMT+5", "GMT-5"},
    {"GMT+4", "GMT-4"},
    {"GMT+3", "GMT-3"},
    {"GMT+2", "GMT-2"},
    {"GMT+1", "GMT-1"},
    {"GMT-1", "GMT+1"},
    {"GMT-2", "GMT+2"},
    {"GMT-3", "GMT+3"},
    {"GMT-4", "GMT+4"},
    {"GMT-5", "GMT+5"},
    {"GMT-6", "GMT+6"},
    {"GMT-7", "GMT+7"},
    {"GMT-8", "GMT+8"},
    {"GMT-9", "GMT+9"},
    {"GMT-10", "GMT+10"},
    {"GMT-11", "GMT+11"},
    {"GMT-12", "GMT+12"},
    {"GMT-13", "GMT+13"},
    {"GMT-14", "GMT+14"},
};
static const uint8_t kSetupTimezoneCount = sizeof(kSetupTimezoneOptions) / sizeof(kSetupTimezoneOptions[0]);

static const char *kSetupKeyRows[][10] = {
    {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
    {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},
    {"A", "S", "D", "F", "G", "H", "J", "K", "L", nullptr},
    {"Z", "X", "C", "V", "B", "N", "M", "DEL", "OK", nullptr},
};
static const uint8_t kSetupKeyRowLengths[] = {10, 10, 9, 9};
static const uint8_t kSetupKeyRowCount = sizeof(kSetupKeyRowLengths) / sizeof(kSetupKeyRowLengths[0]);
static const char *kSetupNumericKeyRows[][10] = {
    {"1", "2", "3", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"4", "5", "6", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"7", "8", "9", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
    {".", "0", "DEL", "OK", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
};
static const uint8_t kSetupNumericKeyRowLengths[] = {3, 3, 3, 4};
static const uint8_t kSetupNumericKeyRowCount = sizeof(kSetupNumericKeyRowLengths) / sizeof(kSetupNumericKeyRowLengths[0]);

static const char *getSetupLoraPresetLabel(meshtastic_Config_LoRaConfig_ModemPreset preset)
{
    for (uint8_t i = 0; i < kSetupLoraPresetOptionCount; ++i) {
        if (kSetupLoraPresetOptions[i].preset == preset) {
            return kSetupLoraPresetOptions[i].label;
        }
    }
    if (preset == meshtastic_Config_LoRaConfig_ModemPreset_VERY_LONG_SLOW) {
        return "VLongSlow";
    }
    return "Custom";
}

static const char *getSetupRoleLabel(meshtastic_Config_DeviceConfig_Role role)
{
    switch (role) {
    case meshtastic_Config_DeviceConfig_Role_CLIENT:
        return "Client";
    case meshtastic_Config_DeviceConfig_Role_CLIENT_MUTE:
        return "Client Mute";
    case meshtastic_Config_DeviceConfig_Role_ROUTER:
        return "Router";
    case meshtastic_Config_DeviceConfig_Role_ROUTER_CLIENT:
        return "Router+Client";
    case meshtastic_Config_DeviceConfig_Role_REPEATER:
        return "Repeater";
    case meshtastic_Config_DeviceConfig_Role_TRACKER:
        return "Tracker";
    case meshtastic_Config_DeviceConfig_Role_SENSOR:
        return "Sensor";
    case meshtastic_Config_DeviceConfig_Role_TAK:
        return "TAK";
    case meshtastic_Config_DeviceConfig_Role_CLIENT_HIDDEN:
        return "Client Hidden";
    case meshtastic_Config_DeviceConfig_Role_LOST_AND_FOUND:
        return "Lost&Found";
    case meshtastic_Config_DeviceConfig_Role_TAK_TRACKER:
        return "TAK Tracker";
    case meshtastic_Config_DeviceConfig_Role_ROUTER_LATE:
        return "Router Late";
    default:
        return u8"未知";
    }
}

static const char *getSetupRoleOptionLabel(meshtastic_Config_DeviceConfig_Role role)
{
    for (uint8_t i = 0; i < kSetupRoleOptionCount; ++i) {
        if (kSetupRoleOptions[i].role == role) {
            return kSetupRoleOptions[i].label;
        }
    }
    return getSetupRoleLabel(role);
}

static bool shouldShowHermesXHomeFrame()
{
    return config.device.role == meshtastic_Config_DeviceConfig_Role_CLIENT ||
           config.device.role == meshtastic_Config_DeviceConfig_Role_CLIENT_MUTE ||
           config.device.role == meshtastic_Config_DeviceConfig_Role_CLIENT_HIDDEN;
}

static bool shouldShowHermesXGpsFrame()
{
    return config.device.role == meshtastic_Config_DeviceConfig_Role_CLIENT ||
           config.device.role == meshtastic_Config_DeviceConfig_Role_CLIENT_MUTE ||
           config.device.role == meshtastic_Config_DeviceConfig_Role_CLIENT_HIDDEN;
}

static const char *getSetupBrightnessLabel(uint8_t value)
{
    const SetupBrightnessOption *best = &kSetupBrightnessOptions[0];
    uint8_t bestDiff = (value > best->value) ? (value - best->value) : (best->value - value);
    for (uint8_t i = 1; i < kSetupBrightnessCount; ++i) {
        const uint8_t optionValue = kSetupBrightnessOptions[i].value;
        uint8_t diff = (value > optionValue) ? (value - optionValue) : (optionValue - value);
        if (diff < bestDiff) {
            best = &kSetupBrightnessOptions[i];
            bestDiff = diff;
        }
    }
    return best->label;
}

static String getSetupScreenSleepLabel(uint32_t seconds)
{
    for (uint8_t i = 0; i < kSetupScreenSleepCount; ++i) {
        if (kSetupScreenSleepOptions[i].seconds == seconds) {
            return String(kSetupScreenSleepOptions[i].label);
        }
    }

    if (seconds == 0) {
        return String(u8"關閉");
    }
    if ((seconds % 60U) == 0U) {
        return String(seconds / 60U) + u8"分鐘";
    }
    return String(seconds) + u8"秒";
}

static uint32_t getSetupCurrentScreenSleepSeconds()
{
#if defined(USE_EINK)
    return config.display.screen_on_secs;
#else
    return Default::getConfiguredOrDefault(config.display.screen_on_secs, default_screen_on_secs);
#endif
}

static uint8_t getSetupScreenSleepSelection(uint32_t seconds)
{
    uint8_t selected = 1;
    uint32_t bestDiff = UINT32_MAX;
    for (uint8_t i = 0; i < kSetupScreenSleepCount; ++i) {
        const uint32_t optionSeconds = kSetupScreenSleepOptions[i].seconds;
        if (optionSeconds == seconds) {
            return i + 1;
        }
        const uint32_t diff = (seconds > optionSeconds) ? (seconds - optionSeconds) : (optionSeconds - seconds);
        if (diff < bestDiff) {
            bestDiff = diff;
            selected = i + 1;
        }
    }
    return selected;
}

static String formatSetupSecondsLabel(uint32_t seconds)
{
    if (seconds == 0) {
        return String(u8"關閉");
    }
    if ((seconds % 3600U) == 0U) {
        return String(seconds / 3600U) + u8"小時";
    }
    if ((seconds % 60U) == 0U) {
        return String(seconds / 60U) + u8"分鐘";
    }
    return String(seconds) + u8"秒";
}

static String formatSetupDistanceLabel(uint32_t meters)
{
    if (meters >= 1000U && (meters % 1000U) == 0U) {
        return String(meters / 1000U) + u8"公里";
    }
    return String(meters) + u8"公尺";
}

static String formatSetupFrequencyLabel(float value)
{
    if (fabsf(value) < 0.0001f) {
        return String(u8"自動");
    }

    char buf[20];
    snprintf(buf, sizeof(buf), "%.4f", value);
    size_t len = strlen(buf);
    while (len > 0 && buf[len - 1] == '0') {
        buf[--len] = '\0';
    }
    if (len > 0 && buf[len - 1] == '.') {
        buf[--len] = '\0';
    }
    return String(buf);
}

static String formatSetupVoltageMvLabel(uint16_t millivolts)
{
    return String(millivolts / 1000U) + "." + String((millivolts % 1000U) / 100U) + "V";
}

static String getSetupCurrentVoltageLabel()
{
    if (!powerStatus) {
        return u8"未知";
    }

    const int batteryVoltageMv = powerStatus->getBatteryVoltageMv();
    if (batteryVoltageMv <= 0) {
        return u8"未知";
    }

    return formatSetupVoltageMvLabel(static_cast<uint16_t>(batteryVoltageMv));
}

static const RegionInfo *findSetupRegionInfo(meshtastic_Config_LoRaConfig_RegionCode code)
{
    for (size_t i = 0;; ++i) {
        if (regions[i].code == code) {
            return &regions[i];
        }
        if (regions[i].code == meshtastic_Config_LoRaConfig_RegionCode_UNSET) {
            break;
        }
    }
    return nullptr;
}

static uint8_t getSetupRegionOptionCount()
{
    uint8_t count = 0;
    for (;; ++count) {
        if (regions[count].code == meshtastic_Config_LoRaConfig_RegionCode_UNSET) {
            return count + 1;
        }
    }
}

static const char *getSetupRegionLabel(meshtastic_Config_LoRaConfig_RegionCode code)
{
    const RegionInfo *region = findSetupRegionInfo(code);
    return region ? region->name : "UNSET";
}

static uint32_t getSetupCurrentGpsSmartDistance()
{
    return Default::getConfiguredOrDefault(config.position.broadcast_smart_minimum_distance, 100U);
}

static uint32_t getSetupCurrentGpsSmartInterval()
{
    return Default::getConfiguredOrDefault(config.position.broadcast_smart_minimum_interval_secs, 30U);
}

static uint32_t getSetupCurrentMqttMapPrecision()
{
    uint32_t precision = Default::getConfiguredOrDefault(moduleConfig.mqtt.map_report_settings.position_precision, 14U);
    if (precision < 12U) {
        precision = 12U;
    } else if (precision > 15U) {
        precision = 15U;
    }
    return precision;
}

static uint32_t getSetupCurrentMqttMapPublishInterval()
{
    return Default::getConfiguredOrDefault(moduleConfig.mqtt.map_report_settings.publish_interval_secs,
                                           default_map_publish_interval_secs);
}

static const char *getSetupMqttMapPrecisionLabel(uint32_t value)
{
    for (uint8_t i = 0; i < kSetupMqttMapPrecisionCount; ++i) {
        if (kSetupMqttMapPrecisionOptions[i].value == value) {
            return kSetupMqttMapPrecisionOptions[i].label;
        }
    }
    return "14";
}

static uint8_t getSetupMqttMapPrecisionSelection(uint32_t value)
{
    for (uint8_t i = 0; i < kSetupMqttMapPrecisionCount; ++i) {
        if (kSetupMqttMapPrecisionOptions[i].value == value) {
            return i + 1;
        }
    }
    return 2;
}

static const char *getSetupChannelPrecisionLabel(uint32_t value)
{
    for (uint8_t i = 0; i < kSetupChannelPrecisionCount; ++i) {
        if (kSetupChannelPrecisionOptions[i].value == value) {
            return kSetupChannelPrecisionOptions[i].label;
        }
    }
    return kSetupChannelPrecisionOptions[2].label;
}

static uint8_t getSetupChannelPrecisionSelection(uint32_t value)
{
    uint8_t selected = 1;
    uint32_t bestDiff = UINT32_MAX;
    for (uint8_t i = 0; i < kSetupChannelPrecisionCount; ++i) {
        const uint32_t optionValue = kSetupChannelPrecisionOptions[i].value;
        if (optionValue == value) {
            return i + 1;
        }
        const uint32_t diff = (value > optionValue) ? (value - optionValue) : (optionValue - value);
        if (diff < bestDiff) {
            bestDiff = diff;
            selected = i + 1;
        }
    }
    return selected;
}

static uint8_t getSetupPowerGuardThresholdSelection(uint16_t millivolts)
{
    uint8_t selected = 1;
    uint16_t bestDiff = UINT16_MAX;
    for (uint8_t i = 0; i < kSetupPowerGuardThresholdCount; ++i) {
        const uint16_t optionValue = kSetupPowerGuardThresholdOptions[i].millivolts;
        if (optionValue == millivolts) {
            return i + 1;
        }
        const uint16_t diff = (millivolts > optionValue) ? (millivolts - optionValue) : (optionValue - millivolts);
        if (diff < bestDiff) {
            bestDiff = diff;
            selected = i + 1;
        }
    }
    return selected;
}

static uint32_t getSetupDefaultChannelPrecision(ChannelIndex chIndex)
{
    const auto &ch = channels.getByIndex(chIndex);
    return (ch.role == meshtastic_Channel_Role_PRIMARY) ? 13U : 32U;
}

static meshtastic_Channel getSetupChannelCopy(ChannelIndex chIndex)
{
    return channels.getByIndex(chIndex);
}

static uint32_t getSetupChannelPrecision(ChannelIndex chIndex)
{
    const auto &ch = channels.getByIndex(chIndex);
    if (ch.has_settings && ch.settings.has_module_settings) {
        return ch.settings.module_settings.position_precision;
    }
    return getSetupDefaultChannelPrecision(chIndex);
}

static bool isSetupChannelPositionSharingEnabled(ChannelIndex chIndex)
{
    return getSetupChannelPrecision(chIndex) > 0U;
}

static String getSetupChannelMenuLabel(ChannelIndex chIndex)
{
    const auto &ch = channels.getByIndex(chIndex);
    const char *name = channels.getName(chIndex);
    String label = String(u8"頻道") + String(static_cast<int>(chIndex) + 1) + ": ";
    if (name && name[0] != '\0') {
        label += name;
    } else {
        label += u8"未命名";
    }
    if (ch.role == meshtastic_Channel_Role_PRIMARY) {
        label += u8" (主)";
    }
    return label;
}

static float getSetupCurrentLoraBandwidthKhz()
{
    const RegionInfo *region = findSetupRegionInfo(config.lora.region);
    const bool useWideLora = region ? region->wideLora : (myRegion && myRegion->wideLora);

    if (config.lora.use_preset) {
        switch (config.lora.modem_preset) {
        case meshtastic_Config_LoRaConfig_ModemPreset_SHORT_TURBO:
            return useWideLora ? 1625.0f : 500.0f;
        case meshtastic_Config_LoRaConfig_ModemPreset_SHORT_FAST:
        case meshtastic_Config_LoRaConfig_ModemPreset_SHORT_SLOW:
        case meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_FAST:
        case meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_SLOW:
        default:
            return useWideLora ? 812.5f : 250.0f;
        case meshtastic_Config_LoRaConfig_ModemPreset_LONG_MODERATE:
        case meshtastic_Config_LoRaConfig_ModemPreset_LONG_SLOW:
            return useWideLora ? 406.25f : 125.0f;
        }
    }

    float bw = config.lora.bandwidth;
    if (bw == 31.0f) {
        bw = 31.25f;
    } else if (bw == 62.0f) {
        bw = 62.5f;
    } else if (bw == 200.0f) {
        bw = 203.125f;
    } else if (bw == 400.0f) {
        bw = 406.25f;
    } else if (bw == 800.0f) {
        bw = 812.5f;
    } else if (bw == 1600.0f) {
        bw = 1625.0f;
    }
    return bw > 0.0f ? bw : 250.0f;
}

static const char *getSetupTimezoneLabel(const char *tz)
{
    if (!tz || !*tz) {
        return "UTC";
    }
    for (uint8_t i = 0; i < kSetupTimezoneCount; ++i) {
        if (strcmp(kSetupTimezoneOptions[i].tz, tz) == 0) {
            return kSetupTimezoneOptions[i].label;
        }
    }
    return tz;
}

static uint8_t getSetupTimezoneSelection(const char *tz)
{
    if (!tz || !*tz) {
        return 1;
    }
    for (uint8_t i = 0; i < kSetupTimezoneCount; ++i) {
        if (strcmp(kSetupTimezoneOptions[i].tz, tz) == 0) {
            return i + 1;
        }
    }
    return 1;
}

static void applySetupTimezone(const char *tz)
{
    const char *resolved = (tz && *tz) ? tz : "GMT0";
    strlcpy(config.device.tzdef, resolved, sizeof(config.device.tzdef));
    setenv("TZ", resolved, 1);
    tzset();
}

static uint16_t getSetupLoraChannelSlotCount()
{
    const RegionInfo *region = findSetupRegionInfo(config.lora.region);
    if (!region) {
        region = findSetupRegionInfo(meshtastic_Config_LoRaConfig_RegionCode_UNSET);
    }
    if (!region) {
        return 1;
    }

    const float bandwidthKhz = getSetupCurrentLoraBandwidthKhz();
    if (bandwidthKhz <= 0.0f) {
        return 1;
    }

    const float spanMhz = region->freqEnd - region->freqStart;
    const float channelWidthMhz = region->spacing + (bandwidthKhz / 1000.0f);
    if (spanMhz <= 0.0f || channelWidthMhz <= 0.0f) {
        return 1;
    }

    const uint32_t count = static_cast<uint32_t>(floorf(spanMhz / channelWidthMhz));
    return count == 0U ? 1U : static_cast<uint16_t>(count);
}

static uint8_t buildSetupChannelList(ChannelIndex *out, uint8_t maxCount)
{
    uint8_t count = 0;
    for (unsigned int i = 0; i < channels.getNumChannels() && count < maxCount; ++i) {
        const auto role = channels.getByIndex(i).role;
        if (role == meshtastic_Channel_Role_PRIMARY || role == meshtastic_Channel_Role_SECONDARY) {
            out[count++] = static_cast<ChannelIndex>(i);
        }
    }
    return count;
}

struct StealthRuntimeState {
    bool active = false;
    bool needsRebootOnExit = false;
    bool emergencyLampEnabled = false;
    bool sirenEnabled = false;
    bool ledHeartbeatDisabled = false;
    uint8_t uiLedBrightness = 60;
    uint8_t screenBrightness = BRIGHTNESS_DEFAULT;
    meshtastic_Config_PositionConfig_GpsMode gpsMode = meshtastic_Config_PositionConfig_GpsMode_NOT_PRESENT;
    bool bluetoothEnabled = false;
    bool loraTxEnabled = true;
};
static StealthRuntimeState gStealthRuntimeState;

#ifndef RTC_NOINIT_ATTR
#define RTC_NOINIT_ATTR
#endif

static constexpr uint32_t kStealthRetainedMagic = 0x4853544CUL; // HSTL
static constexpr uint16_t kStealthRetainedVersion = 1;
static constexpr const char *kStealthStateFile = "/prefs/hermesx_stealth_state.bin";

struct StealthRetainedState {
    uint32_t magic;
    uint16_t version;
    uint16_t stateSize;
    StealthRuntimeState state;
};
RTC_NOINIT_ATTR static StealthRetainedState gStealthRetainedState;

struct TakRuntimeState {
    bool active = false;
    bool emergencyLampEnabled = false;
    bool sirenEnabled = false;
    bool ledHeartbeatDisabled = false;
    uint8_t uiLedBrightness = 60;
    meshtastic_Config_DeviceConfig_Role previousRole = meshtastic_Config_DeviceConfig_Role_CLIENT;
    uint32_t nodeInfoBroadcastSecs = 0;
    bool positionBroadcastSmartEnabled = false;
    uint32_t positionBroadcastSecs = 0;
    uint32_t positionFlags = 0;
    uint32_t telemetryDeviceUpdateInterval = 0;
};
static TakRuntimeState gTakRuntimeState;

static bool isStealthModeActive()
{
    return gStealthRuntimeState.active;
}

static bool isTakModeActive()
{
    return gTakRuntimeState.active;
}

static bool isValidStealthRetainedState(const StealthRetainedState &state)
{
    return state.magic == kStealthRetainedMagic && state.version == kStealthRetainedVersion &&
           state.stateSize == sizeof(StealthRuntimeState) && state.state.active;
}

static void syncRetainedStealthState()
{
    gStealthRetainedState.magic = kStealthRetainedMagic;
    gStealthRetainedState.version = kStealthRetainedVersion;
    gStealthRetainedState.stateSize = sizeof(StealthRuntimeState);
    gStealthRetainedState.state = gStealthRuntimeState;
}

static void clearRetainedStealthState()
{
    gStealthRetainedState.magic = 0;
    gStealthRetainedState.version = 0;
    gStealthRetainedState.stateSize = 0;
    gStealthRetainedState.state = StealthRuntimeState{};
}

static void persistStealthStateToFile()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);

    if (!FSCom.exists("/prefs")) {
        FSCom.mkdir("/prefs");
    }
    if (FSCom.exists(kStealthStateFile)) {
        FSCom.remove(kStealthStateFile);
    }

    auto f = FSCom.open(kStealthStateFile, FILE_O_WRITE);
    if (!f) {
        return;
    }

    const size_t stateLen = sizeof(gStealthRetainedState);
    const size_t written = f.write(reinterpret_cast<const uint8_t *>(&gStealthRetainedState), stateLen);
    if (written == stateLen) {
        f.flush();
    }
    f.close();
#endif
}

static void clearPersistedStealthStateFile()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    if (FSCom.exists(kStealthStateFile)) {
        FSCom.remove(kStealthStateFile);
    }
#endif
}

static bool hasPersistedStealthStateFile()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    return FSCom.exists(kStealthStateFile);
#else
    return false;
#endif
}

static void logStealthStateProbe(const char *phase)
{
    const bool hasRtcState = isValidStealthRetainedState(gStealthRetainedState);
    const bool hasFileState = hasPersistedStealthStateFile();
    LOG_INFO("[HermesX] Stealth probe (%s): runtime=%d rtc=%d file=%d bt_cfg=%d tx_cfg=%d serial_cfg=%d",
             phase ? phase : "?", gStealthRuntimeState.active, hasRtcState, hasFileState, config.bluetooth.enabled,
             config.lora.tx_enabled, config.security.serial_enabled);
}

static bool loadPersistedStealthStateFromFile()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    auto f = FSCom.open(kStealthStateFile, FILE_O_READ);
    if (!f) {
        return false;
    }

    StealthRetainedState persistedState = {};
    bool ok = false;
    if (f.available() >= static_cast<int>(sizeof(persistedState))) {
        const size_t readLen = f.read(reinterpret_cast<uint8_t *>(&persistedState), sizeof(persistedState));
        ok = (readLen == sizeof(persistedState)) && isValidStealthRetainedState(persistedState);
    }
    f.close();

    if (ok) {
        gStealthRetainedState = persistedState;
        return true;
    }

    if (FSCom.exists(kStealthStateFile)) {
        FSCom.remove(kStealthStateFile);
    }
#endif
    return false;
}

static void setHeartbeatLedDisabled(bool disabled)
{
    config.device.led_heartbeat_disabled = disabled;
    if (disabled) {
        ledBlink.set(false);
#ifdef LED_PIN
        digitalWrite(LED_PIN, HIGH ^ LED_STATE_ON);
#endif
    }
}

static bool applyStealthModeSettings()
{
    bool changed = false;

    if (HermesXInterfaceModule::instance) {
        if (HermesXInterfaceModule::instance->isEmergencyLampEnabled()) {
            HermesXInterfaceModule::instance->setEmergencyLampEnabled(false);
            changed = true;
        }
        HermesXInterfaceModule::instance->setUiLedBrightness(0);
        HermesXInterfaceModule::instance->stopEmergencySiren();
        changed = true;
    }
    if (hermesXEmUiModule) {
        if (hermesXEmUiModule->isSirenEnabled()) {
            changed = true;
        }
        hermesXEmUiModule->setSirenEnabled(false);
    }
    if (cannedMessageModule) {
        const auto runState = cannedMessageModule->getRunState();
        if (runState != CANNED_MESSAGE_RUN_STATE_DISABLED && runState != CANNED_MESSAGE_RUN_STATE_INACTIVE) {
            cannedMessageModule->exitMenu();
        }
    }

    if (screen) {
        const uint8_t current = screen->getBrightnessLevel();
        const uint8_t dimmed = static_cast<uint8_t>((static_cast<uint16_t>(current) * 50U) / 100U);
        if (dimmed != current) {
            screen->setBrightnessLevel(dimmed);
            changed = true;
        }
        screen->armStealthWakeWindow();
    }

    if (!config.device.led_heartbeat_disabled) {
        changed = true;
    }
    setHeartbeatLedDisabled(true);

#ifdef PIN_BUZZER
    noTone(PIN_BUZZER);
    changed = true;
#endif
    if (config.device.buzzer_gpio) {
        noTone(config.device.buzzer_gpio);
        changed = true;
    }
#ifdef BUZZER_EN_PIN
    pinMode(BUZZER_EN_PIN, OUTPUT);
    digitalWrite(BUZZER_EN_PIN, LOW);
    changed = true;
#endif

#if !MESHTASTIC_EXCLUDE_GPS
    if (gps) {
        gps->disable();
        changed = true;
    }
#endif
    if (config.position.gps_mode != meshtastic_Config_PositionConfig_GpsMode_DISABLED &&
        config.position.gps_mode != meshtastic_Config_PositionConfig_GpsMode_NOT_PRESENT) {
        config.position.gps_mode = meshtastic_Config_PositionConfig_GpsMode_DISABLED;
        changed = true;
    }

    setBluetoothEnable(false);
#if defined(ARCH_ESP32) && !MESHTASTIC_EXCLUDE_BLUETOOTH
    if (nimbleBluetooth) {
        nimbleBluetooth->deinit();
        gStealthRuntimeState.needsRebootOnExit = true;
        changed = true;
    }
#endif
#if defined(ARCH_NRF52) && !MESHTASTIC_EXCLUDE_BLUETOOTH
    if (nrf52Bluetooth) {
        nrf52Bluetooth->shutdown();
        changed = true;
    }
#endif
    if (rIf) {
        rIf->disable();
        changed = true;
    }

    return changed;
}

static bool restoreStealthModeAfterBoot()
{
    if (gStealthRuntimeState.active) {
        return false;
    }
    if (!isValidStealthRetainedState(gStealthRetainedState) && !loadPersistedStealthStateFromFile()) {
        return false;
    }
    if (!isValidStealthRetainedState(gStealthRetainedState)) {
        clearRetainedStealthState();
        clearPersistedStealthStateFile();
        return false;
    }

    gStealthRuntimeState = gStealthRetainedState.state;
    const bool changed = applyStealthModeSettings();
    syncRetainedStealthState();
    return changed;
}

static bool enableStealthMode()
{
    if (gStealthRuntimeState.active) {
        return false;
    }

    bool changed = false;
    gStealthRuntimeState = StealthRuntimeState{};
    gStealthRuntimeState.active = true;
    changed = true;
    gStealthRuntimeState.gpsMode = config.position.gps_mode;
    gStealthRuntimeState.bluetoothEnabled = config.bluetooth.enabled;
    gStealthRuntimeState.loraTxEnabled = config.lora.tx_enabled;
    if (screen) {
        gStealthRuntimeState.screenBrightness = screen->getBrightnessLevel();
    }
    if (HermesXInterfaceModule::instance) {
        gStealthRuntimeState.uiLedBrightness = HermesXInterfaceModule::instance->getUiLedBrightness();
        gStealthRuntimeState.emergencyLampEnabled = HermesXInterfaceModule::instance->isEmergencyLampEnabled();
    }
    if (hermesXEmUiModule) {
        gStealthRuntimeState.sirenEnabled = hermesXEmUiModule->isSirenEnabled();
    }
    gStealthRuntimeState.ledHeartbeatDisabled = config.device.led_heartbeat_disabled;

    playLowMemoryAlert();

    changed = applyStealthModeSettings() || changed;
    syncRetainedStealthState();
    persistStealthStateToFile();
    return changed;
}

static bool disableStealthMode(bool *needsReboot)
{
    if (needsReboot) {
        *needsReboot = false;
    }
    if (!gStealthRuntimeState.active) {
        return false;
    }

    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->setUiLedBrightness(gStealthRuntimeState.uiLedBrightness);
        HermesXInterfaceModule::instance->setEmergencyLampEnabled(gStealthRuntimeState.emergencyLampEnabled);
    }
    if (hermesXEmUiModule) {
        hermesXEmUiModule->setSirenEnabled(gStealthRuntimeState.sirenEnabled);
    }
    if (screen) {
        screen->setBrightnessLevel(gStealthRuntimeState.screenBrightness);
    }
    setHeartbeatLedDisabled(gStealthRuntimeState.ledHeartbeatDisabled);

#if !MESHTASTIC_EXCLUDE_GPS
    if (gps && gStealthRuntimeState.gpsMode != meshtastic_Config_PositionConfig_GpsMode_DISABLED &&
        gStealthRuntimeState.gpsMode != meshtastic_Config_PositionConfig_GpsMode_NOT_PRESENT) {
        gps->enable();
    }
#endif
    config.position.gps_mode = gStealthRuntimeState.gpsMode;
    if (rIf && gStealthRuntimeState.loraTxEnabled) {
        rIf->enable();
    }
    if (gStealthRuntimeState.bluetoothEnabled) {
        setBluetoothEnable(true);
    }

    if (needsReboot) {
        *needsReboot = gStealthRuntimeState.needsRebootOnExit;
    }
    clearPersistedStealthStateFile();
    clearRetainedStealthState();
    gStealthRuntimeState.active = false;
    gStealthRuntimeState.needsRebootOnExit = false;
    return true;
}

static bool recoverLegacyStealthCommsIfNeeded()
{
    if (gStealthRuntimeState.active) {
        return false;
    }
    if (isValidStealthRetainedState(gStealthRetainedState) || hasPersistedStealthStateFile()) {
        return false;
    }

    // Recovery path:
    // If no Stealth state exists anywhere, but communication interfaces were left disabled,
    // assume an old Stealth flow (or interrupted exit) left config stuck in a silent state.
    const bool radioBtLocked = !config.lora.tx_enabled && !config.bluetooth.enabled;
    const bool serialLocked = !config.security.serial_enabled;
    if (!radioBtLocked && !serialLocked) {
        return false;
    }

    bool changed = false;
    if (radioBtLocked) {
        LOG_WARN("Detected radio+bluetooth lock without Stealth state, restoring external comms");
        config.lora.tx_enabled = true;
        config.bluetooth.enabled = true;
        changed = true;
        if (rIf) {
            rIf->enable();
        }
        setBluetoothEnable(true);
    }
    if (serialLocked) {
        LOG_WARN("Detected serial API lock without Stealth state, restoring serial");
        config.security.serial_enabled = true;
        changed = true;
    }

    if (changed && nodeDB) {
        nodeDB->saveToDisk(SEGMENT_CONFIG);
    }
    return changed;
}

static bool enableTakMode()
{
    if (gTakRuntimeState.active) {
        return false;
    }

    gTakRuntimeState = TakRuntimeState{};
    gTakRuntimeState.active = true;
    gTakRuntimeState.previousRole = config.device.role;
    gTakRuntimeState.ledHeartbeatDisabled = config.device.led_heartbeat_disabled;
    gTakRuntimeState.nodeInfoBroadcastSecs = config.device.node_info_broadcast_secs;
    gTakRuntimeState.positionBroadcastSmartEnabled = config.position.position_broadcast_smart_enabled;
    gTakRuntimeState.positionBroadcastSecs = config.position.position_broadcast_secs;
    gTakRuntimeState.positionFlags = config.position.position_flags;
    gTakRuntimeState.telemetryDeviceUpdateInterval = moduleConfig.telemetry.device_update_interval;

    if (HermesXInterfaceModule::instance) {
        gTakRuntimeState.uiLedBrightness = HermesXInterfaceModule::instance->getUiLedBrightness();
        gTakRuntimeState.emergencyLampEnabled = HermesXInterfaceModule::instance->isEmergencyLampEnabled();
        if (gTakRuntimeState.emergencyLampEnabled) {
            HermesXInterfaceModule::instance->setEmergencyLampEnabled(false);
        }
        HermesXInterfaceModule::instance->setUiLedBrightness(0);
        HermesXInterfaceModule::instance->stopEmergencySiren();
    }

    if (hermesXEmUiModule) {
        gTakRuntimeState.sirenEnabled = hermesXEmUiModule->isSirenEnabled();
        hermesXEmUiModule->setSirenEnabled(false);
    }

#ifdef PIN_BUZZER
    noTone(PIN_BUZZER);
#endif
    if (config.device.buzzer_gpio) {
        noTone(config.device.buzzer_gpio);
    }
#ifdef BUZZER_EN_PIN
    pinMode(BUZZER_EN_PIN, OUTPUT);
    digitalWrite(BUZZER_EN_PIN, LOW);
#endif

    setHeartbeatLedDisabled(true);

    config.device.role = meshtastic_Config_DeviceConfig_Role_TAK;
    if (nodeDB) {
        nodeDB->installRoleDefaults(meshtastic_Config_DeviceConfig_Role_TAK);
    }

    return true;
}

static bool disableTakMode()
{
    if (!gTakRuntimeState.active) {
        return false;
    }

    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->setUiLedBrightness(gTakRuntimeState.uiLedBrightness);
        HermesXInterfaceModule::instance->setEmergencyLampEnabled(gTakRuntimeState.emergencyLampEnabled);
    }
    if (hermesXEmUiModule) {
        hermesXEmUiModule->setSirenEnabled(gTakRuntimeState.sirenEnabled);
    }

    config.device.role = gTakRuntimeState.previousRole;
    config.device.node_info_broadcast_secs = gTakRuntimeState.nodeInfoBroadcastSecs;
    config.position.position_broadcast_smart_enabled = gTakRuntimeState.positionBroadcastSmartEnabled;
    config.position.position_broadcast_secs = gTakRuntimeState.positionBroadcastSecs;
    config.position.position_flags = gTakRuntimeState.positionFlags;
    moduleConfig.telemetry.device_update_interval = gTakRuntimeState.telemetryDeviceUpdateInterval;
    setHeartbeatLedDisabled(gTakRuntimeState.ledHeartbeatDisabled);

    gTakRuntimeState.active = false;
    return true;
}

static String base64UrlEncode(const uint8_t *data, size_t len)
{
    static const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    String out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t chunk = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) {
            chunk |= static_cast<uint32_t>(data[i + 1]) << 8;
        }
        if (i + 2 < len) {
            chunk |= static_cast<uint32_t>(data[i + 2]);
        }
        out += kAlphabet[(chunk >> 18) & 0x3F];
        out += kAlphabet[(chunk >> 12) & 0x3F];
        if (i + 1 < len) {
            out += kAlphabet[(chunk >> 6) & 0x3F];
        }
        if (i + 2 < len) {
            out += kAlphabet[chunk & 0x3F];
        }
    }
    return out;
}

static bool buildChannelSet(meshtastic_ChannelSet &outSet, bool includeAll)
{
    memset(&outSet, 0, sizeof(outSet));
    ChannelIndex primary = channels.getPrimaryIndex();
    const auto &primaryChannel = channels.getByIndex(primary);
    if (primaryChannel.role == meshtastic_Channel_Role_PRIMARY) {
        if (outSet.settings_count < 8) {
            outSet.settings[outSet.settings_count++] = primaryChannel.settings;
        }
    }
    if (includeAll) {
        for (unsigned int i = 0; i < channels.getNumChannels() && outSet.settings_count < 8; ++i) {
            if (i == primary) {
                continue;
            }
            const auto &ch = channels.getByIndex(i);
            if (ch.role == meshtastic_Channel_Role_SECONDARY) {
                outSet.settings[outSet.settings_count++] = ch.settings;
            }
        }
    }
    if (outSet.settings_count == 0) {
        return false;
    }
    outSet.has_lora_config = true;
    outSet.lora_config = config.lora;
    return true;
}

static String buildChannelShareUrl(bool includeAll)
{
    meshtastic_ChannelSet set;
    if (!buildChannelSet(set, includeAll)) {
        return "";
    }
    uint8_t buffer[meshtastic_ChannelSet_size];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if (!pb_encode(&stream, meshtastic_ChannelSet_fields, &set)) {
        LOG_WARN("ChannelSet encode failed");
        return "";
    }
    const size_t len = stream.bytes_written;
    String encoded = base64UrlEncode(buffer, len);
    if (encoded.length() == 0) {
        return "";
    }
    String url = "https://meshtastic.org/e/#";
    url += encoded;
    return url;
}

#if defined(ARDUINO_ARCH_ESP32)
struct HermesXShareQrContext {
    OLEDDisplay *display = nullptr;
    int16_t x = 0;
    int16_t y = 0;
    int16_t width = 0;
    int16_t height = 0;
    int16_t margin = 2;
};

constexpr uint8_t kShareQrMaxVersion = 11;
constexpr uint8_t kShareQrMaxModules = 21 + 4 * (kShareQrMaxVersion - 1);
constexpr size_t kShareQrBitmapBytes = ((size_t)kShareQrMaxModules * (size_t)kShareQrMaxModules + 7u) / 8u;

struct HermesXShareQrCache {
    String url;
    uint8_t size = 0;
    bool valid = false;
    uint8_t bitmap[kShareQrBitmapBytes] = {0};
};

static HermesXShareQrContext gShareQrCtx;
static HermesXShareQrCache gShareQrCache;

static inline size_t shareQrBitIndex(uint8_t x, uint8_t y, uint8_t size)
{
    return static_cast<size_t>(y) * static_cast<size_t>(size) + static_cast<size_t>(x);
}

static inline void setShareQrBit(uint8_t *bitmap, uint8_t x, uint8_t y, uint8_t size)
{
    const size_t bit = shareQrBitIndex(x, y, size);
    bitmap[bit >> 3] |= static_cast<uint8_t>(1u << (bit & 0x7u));
}

static inline bool getShareQrBit(const uint8_t *bitmap, uint8_t x, uint8_t y, uint8_t size)
{
    const size_t bit = shareQrBitIndex(x, y, size);
    return (bitmap[bit >> 3] & static_cast<uint8_t>(1u << (bit & 0x7u))) != 0;
}

static void captureHermesXShareQr(esp_qrcode_handle_t qrcode)
{
    const int size = esp_qrcode_get_size(qrcode);
    if (size <= 0 || size > kShareQrMaxModules) {
        gShareQrCache.size = 0;
        gShareQrCache.valid = false;
        return;
    }

    memset(gShareQrCache.bitmap, 0, sizeof(gShareQrCache.bitmap));
    const uint8_t qrSize = static_cast<uint8_t>(size);
    for (uint8_t y = 0; y < qrSize; ++y) {
        for (uint8_t x = 0; x < qrSize; ++x) {
            if (esp_qrcode_get_module(qrcode, x, y)) {
                setShareQrBit(gShareQrCache.bitmap, x, y, qrSize);
            }
        }
    }
    gShareQrCache.size = qrSize;
}

static bool ensureHermesXShareQrCache(const String &url)
{
    if (gShareQrCache.valid && gShareQrCache.url.equals(url) && gShareQrCache.size > 0) {
        return true;
    }

    gShareQrCache.valid = false;
    gShareQrCache.size = 0;
    gShareQrCache.url = "";
    memset(gShareQrCache.bitmap, 0, sizeof(gShareQrCache.bitmap));

    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    cfg.display_func = captureHermesXShareQr;
    cfg.max_qrcode_version = kShareQrMaxVersion;
    cfg.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;
    if (esp_qrcode_generate(&cfg, url.c_str()) != ESP_OK || gShareQrCache.size == 0) {
        return false;
    }

    gShareQrCache.url = url;
    gShareQrCache.valid = true;
    return true;
}

static bool drawHermesXShareQrFromCache()
{
    if (!gShareQrCtx.display || !gShareQrCache.valid || gShareQrCache.size == 0) {
        return false;
    }

    OLEDDisplay *display = gShareQrCtx.display;
    const int size = gShareQrCache.size;
    const int maxScaleX = (gShareQrCtx.width - (gShareQrCtx.margin * 2)) / size;
    const int maxScaleY = (gShareQrCtx.height - (gShareQrCtx.margin * 2)) / size;
    const int scale = (maxScaleX < maxScaleY) ? maxScaleX : maxScaleY;
    if (scale < 1) {
        return false;
    }

    const int qrW = size * scale;
    const int qrH = size * scale;
    const int bgW = qrW + (gShareQrCtx.margin * 2);
    const int bgH = qrH + (gShareQrCtx.margin * 2);
    const int bgX = gShareQrCtx.x + (gShareQrCtx.width - bgW) / 2;
    const int bgY = gShareQrCtx.y + (gShareQrCtx.height - bgH) / 2;
    const int qrX = bgX + gShareQrCtx.margin;
    const int qrY = bgY + gShareQrCtx.margin;

    for (uint8_t y = 0; y < gShareQrCache.size; ++y) {
        for (uint8_t x = 0; x < gShareQrCache.size; ++x) {
            if (getShareQrBit(gShareQrCache.bitmap, x, y, gShareQrCache.size)) {
                display->fillRect(qrX + x * scale, qrY + y * scale, scale, scale);
            }
        }
    }
    return true;
}
#endif

static void drawSetupHeader(OLEDDisplay *display, int16_t width, const char *title)
{
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
    display->fillRect(0, 0, width, kSetupHeaderHeight);
#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
    display->setFont(FONT_SMALL);
    graphics::HermesX_zh::drawMixed(*display, 2, 1, title, graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
}

static void drawSetupNutIcon(OLEDDisplay *display, int16_t centerX, int16_t centerY, int16_t radius)
{
    if (!display) {
        return;
    }
    if (radius < 10) {
        radius = 10;
    }

    constexpr float kPi = 3.14159265f;
    int16_t outerX[6] = {0};
    int16_t outerY[6] = {0};
    int16_t innerX[6] = {0};
    int16_t innerY[6] = {0};
    const int16_t innerRadius = (radius * 62) / 100;

    for (uint8_t i = 0; i < 6; ++i) {
        const float angle = (kPi / 3.0f) * static_cast<float>(i) - (kPi / 2.0f);
        outerX[i] = centerX + static_cast<int16_t>(cosf(angle) * radius);
        outerY[i] = centerY + static_cast<int16_t>(sinf(angle) * radius);
        innerX[i] = centerX + static_cast<int16_t>(cosf(angle) * innerRadius);
        innerY[i] = centerY + static_cast<int16_t>(sinf(angle) * innerRadius);
    }

    for (uint8_t i = 0; i < 6; ++i) {
        const uint8_t next = (i + 1) % 6;
        display->drawLine(outerX[i], outerY[i], outerX[next], outerY[next]);
        display->drawLine(innerX[i], innerY[i], innerX[next], innerY[next]);
        display->drawLine(outerX[i], outerY[i], innerX[i], innerY[i]);
    }

    const int16_t holeRadius = radius / 3;
#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
    display->fillCircle(centerX, centerY, holeRadius);
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
    display->drawCircle(centerX, centerY, holeRadius);
}

static void drawSetupList(OLEDDisplay *display,
                          int16_t width,
                          int16_t height,
                          const char *title,
                          const char *const *items,
                          int itemCount,
                          int selectedIndex,
                          int listOffset)
{
    if (!display) {
        return;
    }

    display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
    display->fillRect(0, 0, width, height);
    drawSetupHeader(display, width, title);

#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif

    const int16_t listTop = kSetupHeaderHeight + 2;
    for (int i = 0; i < kSetupVisibleRows; ++i) {
        const int entryIndex = listOffset + i;
        if (entryIndex >= itemCount) {
            break;
        }
        const int16_t rowY = listTop + i * kSetupRowHeight;
        if (rowY + kSetupRowHeight > height) {
            break;
        }
        if (entryIndex == selectedIndex) {
#if defined(USE_EINK)
            display->fillRect(0, rowY - 1, width, kSetupRowHeight);
            display->setColor(EINK_WHITE);
#else
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
            display->fillRect(0, rowY - 1, width, kSetupRowHeight);
            display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
            graphics::HermesX_zh::drawMixedBounded(*display, 4, rowY, width - 4, items[entryIndex],
                                                   graphics::HermesX_zh::GLYPH_WIDTH, kSetupRowHeight, nullptr);
#if defined(USE_EINK)
            display->setColor(EINK_BLACK);
#else
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
        } else {
            graphics::HermesX_zh::drawMixedBounded(*display, 4, rowY, width - 4, items[entryIndex],
                                                   graphics::HermesX_zh::GLYPH_WIDTH, kSetupRowHeight, nullptr);
        }
    }
}

static void drawSetupToast(OLEDDisplay *display, int16_t width, int16_t height, String &toast, uint32_t &toastUntilMs)
{
    if (!display || toastUntilMs == 0) {
        return;
    }
    if (millis() > toastUntilMs) {
        toastUntilMs = 0;
        toast = "";
        return;
    }
    if (toast.length() == 0) {
        return;
    }
    graphics::HermesX_zh::drawMixedBounded(*display, 2, height - FONT_HEIGHT_SMALL, width - 4, toast.c_str(),
                                           graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
}

static void drawSetupKeyboardPage(OLEDDisplay *display,
                                  int16_t width,
                                  int16_t height,
                                  const char *header,
                                  const String &draft,
                                  const char *const (*rows)[10],
                                  const uint8_t *rowLengths,
                                  uint8_t rowCount,
                                  uint8_t selectedRow,
                                  uint8_t selectedCol,
                                  String &toast,
                                  uint32_t &toastUntilMs)
{
    if (!display) {
        return;
    }

    display->setTextAlignment(TEXT_ALIGN_LEFT);
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
    drawSetupHeader(display, width, header);
    display->setFont(FONT_SMALL);
    graphics::HermesX_zh::drawMixedBounded(*display, 2, kSetupHeaderHeight + 2, width - 4, draft.c_str(),
                                           graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);

    const int16_t keyTop = (FONT_HEIGHT_SMALL * 2) + 6;
    const int16_t keyAreaHeight = height - keyTop;
    const int16_t rowHeight = (rowCount > 0) ? keyAreaHeight / rowCount : 0;
    display->setTextAlignment(TEXT_ALIGN_CENTER);

    for (uint8_t row = 0; row < rowCount; ++row) {
        const int rowLen = rowLengths[row];
        if (rowLen <= 0) {
            continue;
        }
        const int16_t cellWidth = width / rowLen;
        const int16_t rowY = keyTop + row * rowHeight;
        for (int col = 0; col < rowLen; ++col) {
            const int16_t cellX = col * cellWidth;
            const char *label = rows[row][col];
            if (!label) {
                continue;
            }
            const bool selected = (row == selectedRow && col == selectedCol);
            if (selected) {
#if defined(USE_EINK)
                display->setColor(EINK_BLACK);
#else
                display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
                display->fillRect(cellX, rowY, cellWidth, rowHeight);
#if defined(USE_EINK)
                display->setColor(EINK_WHITE);
#else
                display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
            } else {
#if defined(USE_EINK)
                display->setColor(EINK_BLACK);
#else
                display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
            }
            display->drawRect(cellX, rowY, cellWidth, rowHeight);
            display->drawString(cellX + cellWidth / 2, rowY + (rowHeight - FONT_HEIGHT_SMALL) / 2, label);
        }
    }

    display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
    drawSetupToast(display, width, height, toast, toastUntilMs);
}

static void resetHermesFastSetupTftPalette(OLEDDisplay *display)
{
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
    if (display) {
        auto *tft = static_cast<TFTDisplay *>(display);
        tft->resetColorPalette(false);
    }
    hermesFastSetupTftPaletteState.valid = false;
#else
    (void)display;
#endif
}

static void applyHermesFastSetupTftPalette(OLEDDisplay *display,
                                           int16_t width,
                                           int16_t height,
                                           int itemCount,
                                           int selectedIndex,
                                           int listOffset,
                                           bool showToast,
                                           bool forceFullRedraw)
{
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
    (void)itemCount;
    (void)selectedIndex;
    (void)listOffset;
    (void)showToast;

    const bool paletteUnchanged =
        hermesFastSetupTftPaletteState.valid && hermesFastSetupTftPaletteState.display == display &&
        hermesFastSetupTftPaletteState.width == width && hermesFastSetupTftPaletteState.height == height;
    if (paletteUnchanged && !forceFullRedraw) {
        return;
    }

    // FastSetup palette reverted to monochrome: no per-zone color override.
    auto *tft = static_cast<TFTDisplay *>(display);
    tft->clearColorPaletteZones();

    hermesFastSetupTftPaletteState.display = display;
    hermesFastSetupTftPaletteState.width = width;
    hermesFastSetupTftPaletteState.height = height;
    hermesFastSetupTftPaletteState.itemCount = 0;
    hermesFastSetupTftPaletteState.selectedIndex = 0;
    hermesFastSetupTftPaletteState.listOffset = 0;
    hermesFastSetupTftPaletteState.showToast = false;
    hermesFastSetupTftPaletteState.valid = true;
    if (forceFullRedraw) {
        tft->markColorPaletteDirty();
    }
#else
    (void)display;
    (void)width;
    (void)height;
    (void)itemCount;
    (void)selectedIndex;
    (void)listOffset;
    (void)showToast;
    (void)forceFullRedraw;
#endif
}

// Draw the arrow pointing to a node's location
void Screen::drawNodeHeading(OLEDDisplay *display, int16_t compassX, int16_t compassY, uint16_t compassDiam, float headingRadian)
{
    Point tip(0.0f, 0.5f), tail(0.0f, -0.35f); // pointing up initially
    float arrowOffsetX = 0.14f, arrowOffsetY = 1.0f;
    Point leftArrow(tip.x - arrowOffsetX, tip.y - arrowOffsetY), rightArrow(tip.x + arrowOffsetX, tip.y - arrowOffsetY);

    Point *arrowPoints[] = {&tip, &tail, &leftArrow, &rightArrow};

    for (int i = 0; i < 4; i++) {
        arrowPoints[i]->rotate(headingRadian);
        arrowPoints[i]->scale(compassDiam * 0.6);
        arrowPoints[i]->translate(compassX, compassY);
    }
    /* Old arrow
    display->drawLine(tip.x, tip.y, tail.x, tail.y);
    display->drawLine(leftArrow.x, leftArrow.y, tip.x, tip.y);
    display->drawLine(rightArrow.x, rightArrow.y, tip.x, tip.y);
    display->drawLine(leftArrow.x, leftArrow.y, tail.x, tail.y);
    display->drawLine(rightArrow.x, rightArrow.y, tail.x, tail.y);
    */
#ifdef USE_EINK
    display->drawTriangle(tip.x, tip.y, rightArrow.x, rightArrow.y, tail.x, tail.y);
#else
    display->fillTriangle(tip.x, tip.y, rightArrow.x, rightArrow.y, tail.x, tail.y);
#endif
    display->drawTriangle(tip.x, tip.y, leftArrow.x, leftArrow.y, tail.x, tail.y);
}

// Get a string representation of the time passed since something happened
void Screen::getTimeAgoStr(uint32_t agoSecs, char *timeStr, uint8_t maxLength)
{
    // Use an absolute timestamp in some cases.
    // Particularly useful with E-Ink displays. Static UI, fewer refreshes.
    uint8_t timestampHours, timestampMinutes;
    int32_t daysAgo;
    bool useTimestamp = deltaToTimestamp(agoSecs, &timestampHours, &timestampMinutes, &daysAgo);

    if (agoSecs < 120) // last 2 mins?
        snprintf(timeStr, maxLength, "%u seconds ago", agoSecs);
    // -- if suitable for timestamp --
    else if (useTimestamp && agoSecs < 15 * SECONDS_IN_MINUTE) // Last 15 minutes
        snprintf(timeStr, maxLength, "%u minutes ago", agoSecs / SECONDS_IN_MINUTE);
    else if (useTimestamp && daysAgo == 0) // Today
        snprintf(timeStr, maxLength, "Last seen: %02u:%02u", (unsigned int)timestampHours, (unsigned int)timestampMinutes);
    else if (useTimestamp && daysAgo == 1) // Yesterday
        snprintf(timeStr, maxLength, "Seen yesterday");
    else if (useTimestamp && daysAgo > 1) // Last six months (capped by deltaToTimestamp method)
        snprintf(timeStr, maxLength, "%li days ago", (long)daysAgo);
    // -- if using time delta instead --
    else if (agoSecs < 120 * 60) // last 2 hrs
        snprintf(timeStr, maxLength, "%u minutes ago", agoSecs / 60);
    // Only show hours ago if it's been less than 6 months. Otherwise, we may have bad data.
    else if ((agoSecs / 60 / 60) < (hours_in_month * 6))
        snprintf(timeStr, maxLength, "%u hours ago", agoSecs / 60 / 60);
    else
        snprintf(timeStr, maxLength, "unknown age");
}

void Screen::drawCompassNorth(OLEDDisplay *display, int16_t compassX, int16_t compassY, float myHeading)
{
    // If north is supposed to be at the top of the compass we want rotation to be +0
    if (config.display.compass_north_top)
        myHeading = -0;
    /* N sign points currently not deleted*/
    Point N1(-0.04f, 0.65f), N2(0.04f, 0.65f); // N sign points (N1-N4)
    Point N3(-0.04f, 0.55f), N4(0.04f, 0.55f);
    Point NC1(0.00f, 0.50f); // north circle center point
    Point *rosePoints[] = {&N1, &N2, &N3, &N4, &NC1};

    uint16_t compassDiam = Screen::getCompassDiam(SCREEN_WIDTH, SCREEN_HEIGHT);

    for (int i = 0; i < 5; i++) {
        // North on compass will be negative of heading
        rosePoints[i]->rotate(-myHeading);
        rosePoints[i]->scale(compassDiam);
        rosePoints[i]->translate(compassX, compassY);
    }

    /* changed the N sign to a small circle on the compass circle.
    display->drawLine(N1.x, N1.y, N3.x, N3.y);
    display->drawLine(N2.x, N2.y, N4.x, N4.y);
    display->drawLine(N1.x, N1.y, N4.x, N4.y);
    */
    display->drawCircle(NC1.x, NC1.y, 4); // North sign circle, 4px radius is sufficient for all displays.
}

uint16_t Screen::getCompassDiam(uint32_t displayWidth, uint32_t displayHeight)
{
    uint16_t diam = 0;
    uint16_t offset = 0;

    if (config.display.displaymode != meshtastic_Config_DisplayConfig_DisplayMode_DEFAULT)
        offset = FONT_HEIGHT_SMALL;

    // get the smaller of the 2 dimensions and subtract 20
    if (displayWidth > (displayHeight - offset)) {
        diam = displayHeight - offset;
        // if 2/3 of the other size would be smaller, use that
        if (diam > (displayWidth * 2 / 3)) {
            diam = displayWidth * 2 / 3;
        }
    } else {
        diam = displayWidth;
        if (diam > ((displayHeight - offset) * 2 / 3)) {
            diam = (displayHeight - offset) * 2 / 3;
        }
    }

    return diam - 20;
};

[[maybe_unused]] static void drawNodeInfo(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    // We only advance our nodeIndex if the frame # has changed - because
    // drawNodeInfo will be called repeatedly while the frame is shown
    if (state->currentFrame != prevFrame) {
        prevFrame = state->currentFrame;

        nodeIndex = (nodeIndex + 1) % nodeDB->getNumMeshNodes();
        meshtastic_NodeInfoLite *n = nodeDB->getMeshNodeByIndex(nodeIndex);
        if (n->num == nodeDB->getNodeNum()) {
            // Don't show our node, just skip to next
            nodeIndex = (nodeIndex + 1) % nodeDB->getNumMeshNodes();
            n = nodeDB->getMeshNodeByIndex(nodeIndex);
        }
    }

    meshtastic_NodeInfoLite *node = nodeDB->getMeshNodeByIndex(nodeIndex);

    display->setFont(FONT_SMALL);

    // The coordinates define the left starting point of the text
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_INVERTED) {
        display->fillRect(0 + x, 0 + y, x + display->getWidth(), y + FONT_HEIGHT_SMALL);
    }

    const char *username = node->has_user ? node->user.long_name : "Unknown Name";

    static char signalStr[20];

    // section here to choose whether to display hops away rather than signal strength if more than 0 hops away.
    if (node->hops_away > 0) {
        snprintf(signalStr, sizeof(signalStr), "Hops Away: %d", node->hops_away);
    } else {
        snprintf(signalStr, sizeof(signalStr), "Signal: %d%%", clamp((int)((node->snr + 10) * 5), 0, 100));
    }

    static char lastStr[20];
    screen->getTimeAgoStr(sinceLastSeen(node), lastStr, sizeof(lastStr));

    static char distStr[20];
    if (config.display.units == meshtastic_Config_DisplayConfig_DisplayUnits_IMPERIAL) {
        strncpy(distStr, "? mi ?簞", sizeof(distStr)); // might not have location data
    } else {
        strncpy(distStr, "? km ?簞", sizeof(distStr));
    }
    meshtastic_NodeInfoLite *ourNode = nodeDB->getMeshNode(nodeDB->getNodeNum());
    const char *fields[] = {username, lastStr, signalStr, distStr, NULL};
    int16_t compassX = 0, compassY = 0;
    uint16_t compassDiam = Screen::getCompassDiam(SCREEN_WIDTH, SCREEN_HEIGHT);

    // coordinates for the center of the compass/circle
    if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_DEFAULT) {
        compassX = x + SCREEN_WIDTH - compassDiam / 2 - 5;
        compassY = y + SCREEN_HEIGHT / 2;
    } else {
        compassX = x + SCREEN_WIDTH - compassDiam / 2 - 5;
        compassY = y + FONT_HEIGHT_SMALL + (SCREEN_HEIGHT - FONT_HEIGHT_SMALL) / 2;
    }
    bool hasNodeHeading = false;

    if (ourNode && (nodeDB->hasValidPosition(ourNode) || screen->hasHeading())) {
        const meshtastic_PositionLite &op = ourNode->position;
        float myHeading;
        if (screen->hasHeading())
            myHeading = (screen->getHeading()) * PI / 180; // gotta convert compass degrees to Radians
        else
            myHeading = screen->estimatedHeading(DegD(op.latitude_i), DegD(op.longitude_i));
        screen->drawCompassNorth(display, compassX, compassY, myHeading);

        if (nodeDB->hasValidPosition(node)) {
            // display direction toward node
            hasNodeHeading = true;
            const meshtastic_PositionLite &p = node->position;
            float d =
                GeoCoord::latLongToMeter(DegD(p.latitude_i), DegD(p.longitude_i), DegD(op.latitude_i), DegD(op.longitude_i));

            float bearingToOther =
                GeoCoord::bearing(DegD(op.latitude_i), DegD(op.longitude_i), DegD(p.latitude_i), DegD(p.longitude_i));
            // If the top of the compass is a static north then bearingToOther can be drawn on the compass directly
            // If the top of the compass is not a static north we need adjust bearingToOther based on heading
            if (!config.display.compass_north_top)
                bearingToOther -= myHeading;
            screen->drawNodeHeading(display, compassX, compassY, compassDiam, bearingToOther);

            float bearingToOtherDegrees = (bearingToOther < 0) ? bearingToOther + 2 * PI : bearingToOther;
            bearingToOtherDegrees = bearingToOtherDegrees * 180 / PI;

            if (config.display.units == meshtastic_Config_DisplayConfig_DisplayUnits_IMPERIAL) {
                if (d < (2 * MILES_TO_FEET))
                    snprintf(distStr, sizeof(distStr), "%.0fft   %.0f簞", d * METERS_TO_FEET, bearingToOtherDegrees);
                else
                    snprintf(distStr, sizeof(distStr), "%.1fmi   %.0f簞", d * METERS_TO_FEET / MILES_TO_FEET,
                             bearingToOtherDegrees);
            } else {
                if (d < 2000)
                    snprintf(distStr, sizeof(distStr), "%.0fm   %.0f簞", d, bearingToOtherDegrees);
                else
                    snprintf(distStr, sizeof(distStr), "%.1fkm   %.0f簞", d / 1000, bearingToOtherDegrees);
            }
        }
    }
    if (!hasNodeHeading) {
        // direction to node is unknown so display question mark
        // Debug info for gps lock errors
        // LOG_DEBUG("ourNode %d, ourPos %d, theirPos %d", !!ourNode, ourNode && hasValidPosition(ourNode),
        // hasValidPosition(node));
        display->drawString(compassX - FONT_HEIGHT_SMALL / 4, compassY - FONT_HEIGHT_SMALL / 2, "?");
    }
    display->drawCircle(compassX, compassY, compassDiam / 2);

    if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_INVERTED) {
        display->setColor(BLACK);
    }
    // Must be after distStr is populated
    screen->drawColumns(display, x, y, fields);
}

void Screen::drawHermesFastSetupFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    (void)state;
    if (!screen) {
        return;
    }
    screen->drawHermesFastSetup(display, state, x, y);
}

void Screen::drawHermesXMainFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    (void)state;
    if (!screen) {
        return;
    }
    screen->drawHermesXMain(display, state, x, y);
}

void Screen::drawHermesXActionFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    (void)state;
    if (!screen) {
        return;
    }
    screen->drawHermesXAction(display, state, x, y);
}

void Screen::drawHermesXShareChannelFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    (void)state;
    if (!screen) {
        return;
    }
    screen->drawHermesXShareChannel(display, state, x, y);
}

void Screen::drawHermesXMain(OLEDDisplay *display, OLEDDisplayUiState * /*state*/, int16_t x, int16_t y)
{
    (void)x;
    (void)y;
    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    const int16_t originX = 0;
    const int16_t originY = 0;
    bool hasBattery = false;
    int batteryVoltageMv = 0;
    uint8_t batteryPercent = 0;
    getHermesXHomeBatteryState(hasBattery, batteryVoltageMv, batteryPercent);
    (void)batteryVoltageMv;
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

    const bool compactLayout = (width < 200 || height < 120);
    const int16_t contentX = originX + (compactLayout ? 4 : 8);
    const int16_t contentW = width - 8;
    auto drawHomeLine = [&](int16_t dx, int16_t dy, const char *text) {
        const int16_t maxW = (contentW > 0) ? contentW : (width - 4);
        display->drawStringMaxWidth(dx, dy, maxW, text);
    };
    auto drawHomeBoldLine = [&](int16_t dx, int16_t dy, const char *text) {
        const int16_t maxW = (contentW > 0) ? contentW : (width - 4);
        display->drawStringMaxWidth(dx, dy, maxW, text);
        display->drawStringMaxWidth(dx + 1, dy, maxW > 1 ? (maxW - 1) : maxW, text);
    };

    char timeBuf[16];
    char dateBuf[24];
    const bool hasValidTime = formatHermesXHomeTimeDate(timeBuf, sizeof(timeBuf), dateBuf, sizeof(dateBuf));

    const char *roleLabel = getSetupRoleLabel(config.device.role);
    char roleUpper[24];
    size_t rp = 0;
    for (; roleLabel[rp] != '\0' && rp < (sizeof(roleUpper) - 1); ++rp) {
        char c = roleLabel[rp];
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - ('a' - 'A'));
        }
        roleUpper[rp] = c;
    }
    roleUpper[rp] = '\0';

    display->setTextAlignment(TEXT_ALIGN_LEFT);

    const int16_t timeY = originY + (compactLayout ? 11 : 14);
    const int16_t customTimeMaxW = measurePattanakarnClockText("88:88:88");
    const bool useCustomTimeFont = (contentW > 0) && (customTimeMaxW > 0) && (customTimeMaxW <= contentW);
    const bool useDirectTftClock = canUseDirectHermesXHomeClock(display);
    const uint16_t neonGlowOuterFg = TFTDisplay::rgb565(0x1A, 0x3F, 0xD6);
    const uint16_t neonGlowInnerFg = TFTDisplay::rgb565(0x4C, 0xD9, 0xFF);
    const uint16_t neonCoreFg = TFTDisplay::rgb565(0xFE, 0xFF, 0xFF);
    if (!hasValidTime && !useDirectTftClock) {
        drawHermesXHomeDog(display, width, timeY, compactLayout);
    } else if (useDirectTftClock) {
        // TFT home clock is rendered after ui->update() via direct color drawing.
    } else if (useCustomTimeFont) {
        const int16_t currentTimeW = measurePattanakarnClockText(timeBuf);
        const int16_t timeFrameX = originX + ((width - customTimeMaxW) / 2);
        const int16_t drawX = timeFrameX + ((customTimeMaxW - currentTimeW) / 2);
        const int16_t coreX = drawX;
        const int16_t coreY = timeY;
        const int16_t coreW = currentTimeW;
        const int16_t coreH = PattanakarnClock32::kGlyphHeight;
        addTftColorZone(display, timeFrameX - 3, timeY - 3, customTimeMaxW + 6, PattanakarnClock32::kGlyphHeight + 6, neonGlowOuterFg,
                        0x0000);
        addTftColorZone(display, drawX - 2, timeY - 2, currentTimeW + 4, PattanakarnClock32::kGlyphHeight + 4, neonGlowInnerFg,
                        0x0000);
        addTftColorZone(display, drawX, timeY, currentTimeW, PattanakarnClock32::kGlyphHeight, neonCoreFg, 0x0000);
        drawPattanakarnClockTextOutsideRect(display, drawX + 2, timeY, timeBuf, coreX, coreY, coreW, coreH);
        drawPattanakarnClockTextOutsideRect(display, drawX - 2, timeY, timeBuf, coreX, coreY, coreW, coreH);
        drawPattanakarnClockTextOutsideRect(display, drawX, timeY + 2, timeBuf, coreX, coreY, coreW, coreH);
        drawPattanakarnClockTextOutsideRect(display, drawX, timeY - 2, timeBuf, coreX, coreY, coreW, coreH);
        drawPattanakarnClockTextOutsideRect(display, drawX + 1, timeY, timeBuf, coreX, coreY, coreW, coreH);
        drawPattanakarnClockTextOutsideRect(display, drawX - 1, timeY, timeBuf, coreX, coreY, coreW, coreH);
        drawPattanakarnClockTextOutsideRect(display, drawX, timeY + 1, timeBuf, coreX, coreY, coreW, coreH);
        drawPattanakarnClockTextOutsideRect(display, drawX, timeY - 1, timeBuf, coreX, coreY, coreW, coreH);
        drawPattanakarnClockText(display, drawX, timeY, timeBuf);
    } else {
        display->setFont(FONT_LARGE);
        if (display->getStringWidth(timeBuf) > (width - 4)) {
            display->setFont(FONT_MEDIUM);
        }
        const int16_t timeMaxWidth = display->getStringWidth("88:88:88");
        const int16_t timeWidth = display->getStringWidth(timeBuf);
        const int16_t timeFrameX = ((width - timeMaxWidth) / 2) > 2 ? ((width - timeMaxWidth) / 2) : 2;
        const int16_t drawX = timeFrameX + ((timeMaxWidth - timeWidth) / 2);
        addTftColorZone(display, timeFrameX - 3, timeY - 3, timeMaxWidth + 6, FONT_HEIGHT_LARGE + 6, neonGlowOuterFg, 0x0000);
        addTftColorZone(display, drawX - 2, timeY - 2, timeWidth + 4, FONT_HEIGHT_LARGE + 4, neonGlowInnerFg, 0x0000);
        addTftColorZone(display, drawX, timeY, timeWidth, FONT_HEIGHT_LARGE, neonCoreFg, 0x0000);
        drawHomeLine(drawX + 2, timeY, timeBuf);
        drawHomeLine(drawX - 2, timeY, timeBuf);
        drawHomeLine(drawX, timeY + 2, timeBuf);
        drawHomeLine(drawX, timeY - 2, timeBuf);
        drawHomeLine(drawX + 1, timeY, timeBuf);
        drawHomeLine(drawX - 1, timeY, timeBuf);
        drawHomeLine(drawX, timeY + 1, timeBuf);
        drawHomeLine(drawX, timeY - 1, timeBuf);
        drawHomeLine(drawX, timeY, timeBuf);
    }

    display->setFont(FONT_SMALL);
    const int16_t dateY = originY + (compactLayout ? 49 : 62);
    drawHomeBoldLine(contentX, dateY, dateBuf);

    display->setFont(compactLayout ? FONT_SMALL : FONT_MEDIUM);
    if (display->getStringWidth(roleUpper) > (width / 2)) {
        display->setFont(FONT_SMALL);
    }
    const int16_t roleY = originY + (compactLayout ? 62 : (height - FONT_HEIGHT_MEDIUM));
    drawHomeBoldLine(contentX, roleY, roleUpper);

    if (hasBattery) {
        int16_t iconW = compactLayout ? 34 : 48;
        int16_t iconH = compactLayout ? 16 : 20;
        const int16_t iconX = originX + ((width - iconW) / 2) - (compactLayout ? 6 : 12);
        const int16_t iconY = originY + height - iconH - (compactLayout ? 3 : 6);
        drawHermesXBatteryIconHorizontal(display, iconX, iconY, iconW, iconH, batteryPercent);
    }

    uint32_t satCount = 0;
    if (gpsStatus && gpsStatus->getIsConnected()) {
        satCount = gpsStatus->getNumSatellites();
    }
    if (satCount > 99) {
        satCount = 99;
    }
    const bool hasSatFix = satCount > 0;

    const int16_t badgeW = compactLayout ? 24 : 32;
    const int16_t badgeH = compactLayout ? 18 : 24;
    const int16_t badgeShiftLeft = 20;
    int16_t badgeX = width - badgeW - (compactLayout ? 12 : 18) - badgeShiftLeft;
    if (badgeX < 2) {
        badgeX = 2;
    }
    const int16_t badgeY = height - badgeH - (compactLayout ? 10 : 16);
    addTftColorZone(display, badgeX, badgeY, badgeW, badgeH,
                    hasSatFix ? TFTDisplay::rgb565(0xB5, 0xED, 0x00) : TFTDisplay::rgb565(0xD9, 0x2D, 0x20), 0x0000);
    display->setColor(BLACK);
    display->fillRect(badgeX - 1, badgeY - 1, badgeW + 2, badgeH + 2);
    display->setColor(WHITE);
    display->fillRect(badgeX, badgeY, badgeW, badgeH);
    display->setColor(BLACK);
    display->setFont(compactLayout ? FONT_SMALL : FONT_MEDIUM);
    char satBuf[4];
    snprintf(satBuf, sizeof(satBuf), "%" PRIu32, satCount);
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    const int16_t satTextY = badgeY + (compactLayout ? 1 : 2);
    display->drawString(badgeX + badgeW / 2, satTextY, satBuf);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setColor(WHITE);

}

void Screen::drawHermesXAction(OLEDDisplay *display, OLEDDisplayUiState * /*state*/, int16_t /*x*/, int16_t /*y*/)
{
    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();

    bool lampOn = false;
    if (HermesXInterfaceModule::instance) {
        lampOn = HermesXInterfaceModule::instance->isEmergencyLampEnabled();
    }
    if (hermesActionSelected < 0 || hermesActionSelected >= static_cast<int8_t>(kMainActionCount)) {
        hermesActionSelected = 0;
    }

    if (!hermesActionStealthConfirmVisible) {
        const bool stealthOn = isStealthModeActive();
        const bool takOn = isTakModeActive();
        const bool gpsPresent = config.position.gps_mode != meshtastic_Config_PositionConfig_GpsMode_NOT_PRESENT;
        const bool gpsOn = config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_ENABLED;
        const bool hasRecentMessages = hasRecentTextMessages();
        const bool recentUnread = hasRecentMessages && hasUnreadTextMessage;

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

        static const char *kTileLabelExact[kMainActionCount] = {
            u8"潛行模式", u8"緊急照明燈", "GPS", u8"TAK MODE", u8"休眠", "Home", u8"頻道", u8"設定", "MSG",
        };
        static const char *kTileLabelCompact[kMainActionCount] = {
            u8"潛行", u8"照明", "GPS", "TAK", u8"休眠", "Home", u8"頻道", u8"設定", "MSG",
        };
        const bool compactLayout = (width < 180 || height < 100);
        bool tileHasState[kMainActionCount] = {true, true, true, true, false, false, false, false, false};
        bool tileState[kMainActionCount] = {stealthOn, lampOn, gpsOn, takOn, false, false, false, false, false};
        tileHasState[8] = hasRecentMessages;
        tileState[8] = recentUnread;

        const int16_t labelLineH = compactLayout ? FONT_HEIGHT_SMALL : (FONT_HEIGHT_SMALL + 2);

#if defined(USE_EINK)
        const auto tileBgColor = EINK_WHITE;
        const auto tileFgColor = EINK_BLACK;
#else
        const auto tileBgColor = OLEDDISPLAY_COLOR::BLACK;
        const auto tileFgColor = OLEDDISPLAY_COLOR::WHITE;
#endif

        auto drawArcApprox = [&](int16_t acx, int16_t acy, int16_t radius, float startDeg, float endDeg,
                                 uint8_t thickness = 1) {
            if (radius <= 0) {
                return;
            }
            if (thickness == 0) {
                thickness = 1;
            }
            const int steps = (radius < 8) ? 8 : (radius * 2);
            for (uint8_t t = 0; t < thickness; ++t) {
                const int16_t rr = radius - static_cast<int16_t>(t);
                if (rr <= 0) {
                    break;
                }
                bool hasPrev = false;
                int16_t px = 0;
                int16_t py = 0;
                for (int i = 0; i <= steps; ++i) {
                    const float u = static_cast<float>(i) / static_cast<float>(steps);
                    const float deg = startDeg + (endDeg - startDeg) * u;
                    const float a = deg * PI / 180.0f;
                    const int16_t x = acx + static_cast<int16_t>(cosf(a) * rr);
                    const int16_t y = acy + static_cast<int16_t>(sinf(a) * rr);
                    if (hasPrev) {
                        display->drawLine(px, py, x, y);
                    }
                    px = x;
                    py = y;
                    hasPrev = true;
                }
            }
        };

        auto drawShieldOutline = [&](int16_t scx, int16_t topY, int16_t w, int16_t h, uint8_t thickness = 1) {
            if (w < 10) {
                w = 10;
            }
            if (h < 12) {
                h = 12;
            }
            if (thickness == 0) {
                thickness = 1;
            }

            for (uint8_t t = 0; t < thickness; ++t) {
                const int16_t left = scx - w / 2 + t;
                const int16_t right = scx + w / 2 - t;
                const int16_t top = topY + t;
                const int16_t shoulderY = top + h / 5;
                const int16_t waistY = top + (h * 2) / 3;
                const int16_t bottom = top + h - t;
                if (right - left < 6 || bottom - top < 6) {
                    break;
                }
                display->drawLine(left + 2, shoulderY, scx, top);
                display->drawLine(scx, top, right - 2, shoulderY);
                display->drawLine(left, shoulderY + 1, left + 4, waistY);
                display->drawLine(right, shoulderY + 1, right - 4, waistY);
                display->drawLine(left + 4, waistY, scx, bottom);
                display->drawLine(right - 4, waistY, scx, bottom);
            }
        };

        auto drawMuteSpeakerIcon = [&](int16_t scx, int16_t scy, int16_t size, bool drawWaves) {
            if (size < 14) {
                size = 14;
            }

            int16_t rectW = size / 6;
            int16_t rectH = size / 3;
            int16_t coneW = size / 4;
            int16_t waveR1 = size / 6;
            int16_t waveR2 = size / 4;
            if (rectW < 4) {
                rectW = 4;
            }
            if (rectH < 8) {
                rectH = 8;
            }
            if (coneW < 5) {
                coneW = 5;
            }
            if (waveR1 < 4) {
                waveR1 = 4;
            }
            if (waveR2 < 7) {
                waveR2 = 7;
            }

            const int16_t wavesPad = drawWaves ? (waveR2 + 4) : 2;
            const int16_t totalW = rectW + coneW + wavesPad;
            const int16_t left = scx - totalW / 2;
            const int16_t top = scy - rectH / 2;

            const int16_t bodyX = left;
            const int16_t bodyY = top + 2;
            const int16_t bodyH = rectH - 4;
            if (bodyH > 0) {
                display->drawRect(bodyX, bodyY, rectW, bodyH);
            }

            const int16_t coneBaseX = bodyX + rectW;
            const int16_t coneTipX = coneBaseX + coneW;
            const int16_t coneTopY = scy - rectH / 2;
            const int16_t coneBotY = scy + rectH / 2;
            const int16_t coneMidTopY = scy - rectH / 4;
            const int16_t coneMidBotY = scy + rectH / 4;
            display->drawLine(coneBaseX, coneMidTopY, coneTipX, coneTopY);
            display->drawLine(coneBaseX, coneMidBotY, coneTipX, coneBotY);
            display->drawLine(coneTipX, coneTopY, coneTipX, coneBotY);
            display->drawLine(coneBaseX, coneMidTopY, coneBaseX, coneMidBotY);

            if (drawWaves) {
                const int16_t wcx = coneTipX + 2;
                drawArcApprox(wcx, scy, waveR1, -35.0f, 35.0f, 1);
                drawArcApprox(wcx + 1, scy, waveR2, -35.0f, 35.0f, 1);
            }

            const int16_t slashX1 = left - 1;
            const int16_t slashY1 = scy + rectH / 2 + 2;
            const int16_t slashX2 = coneTipX + wavesPad + 1;
            const int16_t slashY2 = scy - rectH / 2 - 2;
            display->drawLine(slashX1, slashY1, slashX2, slashY2);
            display->drawLine(slashX1 + 1, slashY1, slashX2 + 1, slashY2);
        };

        auto drawGear = [&](int16_t cx, int16_t cy, int16_t r) {
            if (r < 5) {
                display->drawCircle(cx, cy, r);
                return;
            }
            display->drawCircle(cx, cy, r);
            display->drawCircle(cx, cy, r / 2);
            for (int i = 0; i < 8; ++i) {
                const float a = static_cast<float>(i) * PI / 4.0f;
                const int16_t x1 = cx + static_cast<int16_t>(cosf(a) * (r + 1));
                const int16_t y1 = cy + static_cast<int16_t>(sinf(a) * (r + 1));
                const int16_t x2 = cx + static_cast<int16_t>(cosf(a) * (r + 3));
                const int16_t y2 = cy + static_cast<int16_t>(sinf(a) * (r + 3));
                display->drawLine(x1, y1, x2, y2);
            }
        };

        auto drawActionGlyph = [&](int index, int16_t tx, int16_t ty, int16_t tw, int16_t th, bool selected) {
            const int16_t cx = tx + tw / 2;
            const int16_t iconTop = ty + (compactLayout ? 4 : 6);
            const int16_t iconBottom = ty + th - labelLineH - (compactLayout ? 6 : 10);
            const int16_t cy = iconTop + (iconBottom - iconTop) / 2;
            int16_t r = (tw < th ? tw : th) / (compactLayout ? 6 : 7);
            if (r < 4) {
                r = 4;
            }

            if (compactLayout) {
                const int16_t cr = r + 1;
                switch (index) {
                case 0: { // 潛行模式（小圖）
                    drawMuteSpeakerIcon(cx, cy + 1, cr * 3 + 4, false);
                    break;
                }
                case 1: { // 緊急照明燈（小圖）
                    const int16_t headCx = cx - 5;
                    display->drawCircle(headCx, cy, 4);
                    display->drawRect(headCx + 3, cy - 3, 12, 6);
                    display->drawLine(headCx - 6, cy, headCx - 11, cy);
                    display->drawLine(headCx - 5, cy - 3, headCx - 9, cy - 5);
                    display->drawLine(headCx - 5, cy + 3, headCx - 9, cy + 5);
                    break;
                }
                case 2: { // GPS（小圖）
                    display->drawRect(cx - 3, cy - 8, 6, 9);
                    display->drawRect(cx - 11, cy - 6, 7, 4);
                    display->drawRect(cx + 4, cy - 6, 7, 4);
                    display->drawLine(cx, cy + 1, cx, cy + 6);
                    drawArcApprox(cx, cy + 7, 5, 25.0f, 155.0f, 1);
                    drawArcApprox(cx, cy + 7, 8, 25.0f, 155.0f, 1);
                    if (!gpsPresent) {
                        display->drawLine(cx - 12, cy + 10, cx + 12, cy - 12);
                    }
                    break;
                }
                case 3: { // TAK MODE（小圖）
                    drawShieldOutline(cx, cy - 10, 20, 20, 2);
                    break;
                }
                case 4: { // 休眠（小圖）
                    display->drawCircle(cx - 3, cy + 1, 7);
                    display->setColor(tileBgColor);
                    display->fillCircle(cx, cy + 1, 6);
                    display->setColor(tileFgColor);
                    display->drawString(cx + 6, cy - 8, "z");
                    display->drawString(cx + 10, cy - 2, "z");
                    break;
                }
                case 5: { // Home（小圖）
                    display->drawLine(cx - 9, cy + 3, cx, cy - 6);
                    display->drawLine(cx, cy - 6, cx + 9, cy + 3);
                    display->drawRect(cx - 6, cy + 3, 12, 8);
                    display->drawRect(cx - 1, cy + 6, 3, 5);
                    break;
                }
                case 6: { // 頻道 / QR（小圖）
                    display->drawRect(cx - 10, cy - 9, 6, 6);
                    display->drawRect(cx + 4, cy - 9, 6, 6);
                    display->drawRect(cx - 10, cy + 4, 6, 6);
                    display->drawRect(cx - 1, cy - 1, 2, 2);
                    display->drawRect(cx + 3, cy + 2, 2, 2);
                    display->drawLine(cx + 7, cy + 1, cx + 10, cy + 1);
                    display->drawLine(cx + 8, cy + 5, cx + 10, cy + 5);
                    break;
                }
                case 7: { // 設定（小圖）
                    drawGear(cx, cy + 1, 7);
                    break;
                }
                case 8: { // MSG（小圖）
                    display->drawRect(cx - 10, cy - 8, 20, 12);
                    display->drawLine(cx - 3, cy + 4, cx - 6, cy + 8);
                    display->drawLine(cx - 3, cy + 4, cx + 1, cy + 4);
                    display->drawLine(cx - 6, cy - 4, cx + 6, cy - 4);
                    display->drawLine(cx - 6, cy - 1, cx + 4, cy - 1);
                    display->drawLine(cx - 6, cy + 2, cx + 2, cy + 2);
                    break;
                }
                default:
                    break;
                }
                return;
            }

            const int16_t padX = 8;
            const int16_t ix = tx + padX;
            int16_t iy = iconTop + 3;
            int16_t iw = tw - (padX * 2);
            int16_t ih = iconBottom - iy - 1;
            if (iw < 10) {
                iw = 10;
            }
            if (ih < 10) {
                ih = 10;
            }
            const int16_t base = (iw < ih) ? iw : ih;

            switch (index) {
            case 0: { // 潛行模式
                drawMuteSpeakerIcon(cx, iy + ih / 2, base + 16, true);
                break;
            }
            case 1: { // 緊急照明燈
                const int16_t fy = iy + ih / 2 - 2;
                int16_t headR = base / 8 + 6;
                if (headR < 7) {
                    headR = 7;
                }
                const int16_t headCx = ix + iw / 3;
                display->drawCircle(headCx, fy, headR);
                display->drawCircle(headCx, fy, headR - 2);
                display->drawLine(headCx + headR - 1, fy - headR / 2, headCx + headR + 4, fy - headR / 2);
                display->drawLine(headCx + headR - 1, fy + headR / 2, headCx + headR + 4, fy + headR / 2);

                const int16_t bodyX = headCx + headR + 4;
                const int16_t bodyW = iw - (bodyX - ix) - 8;
                const int16_t bodyH = headR;
                if (bodyW > 6) {
                    display->drawRect(bodyX, fy - bodyH / 2, bodyW, bodyH);
                    display->drawRect(bodyX + bodyW - 6, fy - bodyH / 2 + 1, 6, bodyH - 2);
                    display->drawLine(bodyX + bodyW / 2 - 3, fy, bodyX + bodyW / 2 + 3, fy);
                    display->setPixel(bodyX + bodyW / 2, fy - 2);
                    display->setPixel(bodyX + bodyW / 2 + 2, fy + 1);
                }

                display->drawLine(headCx - headR - 4, fy, headCx - headR - 16, fy - 1);
                display->drawLine(headCx - headR - 3, fy - 4, headCx - headR - 12, fy - 8);
                display->drawLine(headCx - headR - 3, fy + 4, headCx - headR - 12, fy + 8);
                break;
            }
            case 2: { // GPS
                int16_t bodyW = base / 6 + 8;
                int16_t bodyH = base / 3 + 4;
                if (bodyW < 10) {
                    bodyW = 10;
                }
                if (bodyH < 14) {
                    bodyH = 14;
                }
                const int16_t bodyX = cx - bodyW / 2;
                const int16_t bodyY = iy + 4;
                display->drawRect(bodyX, bodyY, bodyW, bodyH);

                const int16_t panelW = bodyW + 6;
                const int16_t panelH = bodyH / 2 + 3;
                const int16_t panelY = bodyY + 3;
                const int16_t leftPanelX = bodyX - panelW - 4;
                const int16_t rightPanelX = bodyX + bodyW + 4;
                display->drawRect(leftPanelX, panelY, panelW, panelH);
                display->drawRect(rightPanelX, panelY, panelW, panelH);
                display->drawLine(leftPanelX + panelW / 2, panelY, leftPanelX + panelW / 2, panelY + panelH - 1);
                display->drawLine(rightPanelX + panelW / 2, panelY, rightPanelX + panelW / 2, panelY + panelH - 1);
                display->drawLine(leftPanelX, panelY + panelH / 2, leftPanelX + panelW - 1, panelY + panelH / 2);
                display->drawLine(rightPanelX, panelY + panelH / 2, rightPanelX + panelW - 1, panelY + panelH / 2);

                const int16_t mastMidY = bodyY + bodyH + 5;
                const int16_t dishY = iy + ih - 18;
                display->drawLine(cx, bodyY + bodyH, cx, mastMidY);
                display->drawLine(cx, mastMidY, cx, dishY - 3);
                display->drawCircle(cx, dishY, 2);
                drawArcApprox(cx, dishY + 1, 9, 25.0f, 155.0f, 2);
                drawArcApprox(cx, dishY + 1, 14, 25.0f, 155.0f, 2);
                if (!gpsPresent) {
                    display->drawLine(ix + 2, iy + ih - 2, ix + iw - 2, iy + 2);
                    display->drawLine(ix + 3, iy + ih - 2, ix + iw - 1, iy + 2);
                }
                break;
            }
            case 3: { // TAK MODE
                int16_t shieldW = iw - 18;
                int16_t shieldH = ih - 10;
                if (shieldW < 28) {
                    shieldW = 28;
                }
                if (shieldH < 30) {
                    shieldH = 30;
                }
                if (shieldW > shieldH + 8) {
                    shieldW = shieldH + 8;
                }
                drawShieldOutline(cx, iy + 2, shieldW, shieldH, 2);
                break;
            }
            case 4: { // 休眠
                int16_t moonR = base / 3 + 3;
                if (moonR < 12) {
                    moonR = 12;
                }
                const int16_t moonCx = cx - iw / 7;
                const int16_t moonCy = iy + ih / 2 + 1;
                display->drawCircle(moonCx, moonCy, moonR);
                display->setColor(tileBgColor);
                display->fillCircle(moonCx + moonR / 3, moonCy - 1, moonR - 3);
                display->setColor(tileFgColor);
                drawArcApprox(moonCx, moonCy, moonR, 215.0f, 325.0f, 2);

                auto drawZigZ = [&](int16_t zx, int16_t zy, int16_t s) {
                    display->drawLine(zx, zy, zx + s, zy);
                    display->drawLine(zx + s, zy, zx + 1, zy + s);
                    display->drawLine(zx + 1, zy + s, zx + s + 1, zy + s);
                };
                drawZigZ(moonCx + moonR - 1, moonCy - moonR / 3 - 2, 5);
                drawZigZ(moonCx + moonR + 9, moonCy - moonR / 2 - 5, 4);
                drawZigZ(moonCx + moonR + 5, moonCy + 2, 5);
                break;
            }
            case 5: { // Home
                int16_t houseW = base + 8;
                int16_t houseH = base / 2 + 8;
                if (houseW < 28) {
                    houseW = 28;
                }
                if (houseH < 16) {
                    houseH = 16;
                }
                const int16_t roofY = iy + 4;
                const int16_t bodyY = roofY + houseH / 2;
                display->drawLine(cx - houseW / 2, bodyY, cx, roofY);
                display->drawLine(cx, roofY, cx + houseW / 2, bodyY);
                display->drawRect(cx - (houseW / 2 - 3), bodyY, houseW - 6, houseH);
                display->drawRect(cx - 4, bodyY + houseH / 3, 8, houseH - houseH / 3);
                break;
            }
            case 6: { // 頻道 / QR
                int16_t qr = base + 8;
                if (qr < 28) {
                    qr = 28;
                }
                const int16_t qx = cx - qr / 2;
                const int16_t qy = iy + (ih - qr) / 2;
                display->drawRect(qx, qy, qr, qr);
                const int16_t finder = qr / 4;
                display->drawRect(qx + 2, qy + 2, finder, finder);
                display->drawRect(qx + qr - finder - 2, qy + 2, finder, finder);
                display->drawRect(qx + 2, qy + qr - finder - 2, finder, finder);
                display->fillRect(qx + 4, qy + 4, finder - 4, finder - 4);
                display->fillRect(qx + qr - finder + 0, qy + 4, finder - 4, finder - 4);
                display->fillRect(qx + 4, qy + qr - finder + 0, finder - 4, finder - 4);
                display->drawRect(qx + qr / 2 - 2, qy + qr / 2 - 2, 4, 4);
                display->drawLine(qx + qr / 2 + 4, qy + qr / 2 - 1, qx + qr - 6, qy + qr / 2 - 1);
                display->drawLine(qx + qr / 2 + 2, qy + qr / 2 + 5, qx + qr - 8, qy + qr / 2 + 5);
                break;
            }
            case 7: { // 設定
                int16_t gearR = base / 4 + 5;
                if (gearR < 9) {
                    gearR = 9;
                }
                drawGear(cx, cy, gearR);
                break;
            }
            case 8: { // MSG
                int16_t bubbleW = base + 10;
                int16_t bubbleH = base / 2 + 8;
                if (bubbleW < 30) {
                    bubbleW = 30;
                }
                if (bubbleH < 18) {
                    bubbleH = 18;
                }
                const int16_t bx = cx - bubbleW / 2;
                const int16_t by = iy + (ih - bubbleH) / 2 - 2;
                display->drawRect(bx, by, bubbleW, bubbleH);
                display->drawLine(bx + 7, by + bubbleH - 1, bx + 3, by + bubbleH + 5);
                display->drawLine(bx + 7, by + bubbleH - 1, bx + 12, by + bubbleH - 1);
                display->drawLine(bx + 5, by + 5, bx + bubbleW - 6, by + 5);
                display->drawLine(bx + 5, by + 9, bx + bubbleW - 10, by + 9);
                display->drawLine(bx + 5, by + 13, bx + bubbleW - 14, by + 13);
                break;
            }
            default:
                break;
            }

            if (selected && tileHasState[index] && tileState[index]) {
                display->drawCircle(tx + tw - 10, ty + 9, 3);
                display->fillCircle(tx + tw - 10, ty + 9, 2);
            } else if (tileHasState[index] && tileState[index]) {
                display->fillCircle(tx + tw - 10, ty + 9, 2);
            }
        };

        auto drawActionLabel = [&](int index, int16_t tx, int16_t ty, int16_t tw, int16_t th, bool selected,
                                   const char *label) {
            const int labelW = graphics::HermesX_zh::stringAdvance(label, graphics::HermesX_zh::GLYPH_WIDTH, display);
            const int16_t labelY = ty + th - labelLineH - (compactLayout ? 3 : 8);
            int16_t labelX = tx + (tw - labelW) / 2;
            if (labelX < tx + 2) {
                labelX = tx + 2;
            }

            if (selected) {
                const int16_t boxPadX = compactLayout ? 2 : 4;
                const int16_t boxPadY = 1;
                int16_t boxW = labelW + boxPadX * 2;
                if (boxW > tw - 6) {
                    boxW = tw - 6;
                }
                const int16_t boxX = tx + (tw - boxW) / 2;
                display->drawRect(boxX, labelY - boxPadY, boxW, labelLineH + boxPadY * 2);
            }

            graphics::HermesX_zh::drawMixedBounded(*display, labelX, labelY, tx + tw - labelX - 2, label,
                                                   graphics::HermesX_zh::GLYPH_WIDTH, labelLineH, nullptr);

            if (!compactLayout || !tileHasState[index] || !tileState[index]) {
                return;
            }

            const int16_t dotX = tx + tw - (selected ? 9 : 6);
            const int16_t dotY = ty + (selected ? 8 : 6);
            if (selected) {
                display->drawCircle(dotX, dotY, 3);
                display->fillCircle(dotX, dotY, 2);
            } else {
                display->fillCircle(dotX, dotY, 2);
            }
        };

        auto wrapActionIndex = [&](int index) -> int {
            while (index < 0) {
                index += kMainActionCount;
            }
            return index % kMainActionCount;
        };

        const int actionSlots[kMainActionVisibleSlots] = {
            wrapActionIndex(hermesActionSelected - 1),
            hermesActionSelected,
            wrapActionIndex(hermesActionSelected + 1),
        };

        const int16_t outerPad = compactLayout ? 2 : 6;
        const int16_t gap = compactLayout ? 2 : 6;
        const int16_t minSideW = compactLayout ? 26 : 44;
        const int16_t maxSideW = compactLayout ? 34 : 60;
        const int16_t minCenterW = compactLayout ? 68 : 96;
        int16_t sideW = compactLayout ? (width / 6 + 6) : (width / 5);
        if (sideW < minSideW) {
            sideW = minSideW;
        }
        if (sideW > maxSideW) {
            sideW = maxSideW;
        }

        int16_t centerW = width - (outerPad * 2) - (sideW * 2) - (gap * 2);
        if (centerW < minCenterW) {
            sideW = (width - (outerPad * 2) - (gap * 2) - minCenterW) / 2;
            if (sideW < minSideW) {
                sideW = minSideW;
            }
            centerW = width - (outerPad * 2) - (sideW * 2) - (gap * 2);
        }
        if (centerW < 40) {
            centerW = 40;
        }

        int16_t sideH = height - (compactLayout ? 18 : 20);
        int16_t centerH = height - (compactLayout ? 4 : 8);
        if (sideH < 40) {
            sideH = height - 8;
        }
        if (centerH < 48) {
            centerH = height - 4;
        }

        const int16_t sideY = (height - sideH) / 2;
        const int16_t centerY = (height - centerH) / 2;
        const int16_t centerX = (width - centerW) / 2;
        int16_t sideDrawW = ((width - centerW) / 2) - gap - outerPad;
        if (sideDrawW > sideW) {
            sideDrawW = sideW;
        }
        if (sideDrawW < 16) {
            sideDrawW = 16;
        }
        const int16_t leftCardX = centerX - gap - sideDrawW;
        const int16_t rightCardX = centerX + centerW + gap;

        display->drawRect(centerX, centerY, centerW, centerH);
        if (!compactLayout && centerW > 8 && centerH > 8) {
            display->drawRect(centerX + 1, centerY + 1, centerW - 2, centerH - 2);
        }
        display->drawRect(leftCardX, sideY, sideDrawW, sideH);
        display->drawRect(rightCardX, sideY, sideDrawW, sideH);

        drawActionGlyph(actionSlots[0], leftCardX, sideY, sideDrawW, sideH, false);
        drawActionLabel(actionSlots[0], leftCardX, sideY, sideDrawW, sideH, false, kTileLabelCompact[actionSlots[0]]);

        drawActionGlyph(actionSlots[1], centerX, centerY, centerW, centerH, true);
        drawActionLabel(actionSlots[1], centerX, centerY, centerW, centerH, true, kTileLabelExact[actionSlots[1]]);

        drawActionGlyph(actionSlots[2], rightCardX, sideY, sideDrawW, sideH, false);
        drawActionLabel(actionSlots[2], rightCardX, sideY, sideDrawW, sideH, false, kTileLabelCompact[actionSlots[2]]);

        const int16_t midY = height / 2;
        const int16_t arrowInset = compactLayout ? 3 : 6;
        display->drawLine(centerX - gap + 1, midY, centerX - arrowInset, midY - 4);
        display->drawLine(centerX - gap + 1, midY, centerX - arrowInset, midY + 4);
        display->drawLine(centerX + centerW + gap - 1, midY, centerX + centerW + arrowInset - gap, midY - 4);
        display->drawLine(centerX + centerW + gap - 1, midY, centerX + centerW + arrowInset - gap, midY + 4);
        return;
    }

#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
    display->fillRect(0, 0, width, height);

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
    const int16_t noX = boxX + 4;
    const int16_t yesX = noX + optionW + 5;
    const uint32_t nowMs = millis();
    uint32_t remainMs = 0;
    if (hermesActionStealthConfirmShownAtMs != 0) {
        const uint32_t elapsedMs = nowMs - hermesActionStealthConfirmShownAtMs;
        if (elapsedMs < kStealthConfirmArmMs) {
            remainMs = kStealthConfirmArmMs - elapsedMs;
        }
    }
    const bool yesArmed = (remainMs == 0);

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

    const char *title = u8"潛行模式警告";
    const int titleW = graphics::HermesX_zh::stringAdvance(title, graphics::HermesX_zh::GLYPH_WIDTH, display);
    int16_t titleX = boxX + (boxW - titleW) / 2;
    if (titleX < boxX + 2) {
        titleX = boxX + 2;
    }
    const int16_t titleY = boxY + 1;
    graphics::HermesX_zh::drawMixedBounded(*display, titleX, titleY, boxW - 4, title, graphics::HermesX_zh::GLYPH_WIDTH,
                                           FONT_HEIGHT_SMALL, nullptr);
    display->setColor(dialogFg);

    const int dialogBodyAdvance = graphics::HermesX_zh::GLYPH_WIDTH - 1;
    const int16_t bodyX = boxX + 5;
    const int16_t bodyW = boxW - 10;
    const int16_t bodyY = boxY + titleBarH + 2;
    const int16_t statusY = optionY - FONT_HEIGHT_SMALL - 2;
    int16_t dialogBodyLineHeight = (statusY - bodyY - 1) / 2;
    if (dialogBodyLineHeight < (FONT_HEIGHT_SMALL - 1)) {
        dialogBodyLineHeight = FONT_HEIGHT_SMALL - 1;
    } else if (dialogBodyLineHeight > kSetupRowHeight) {
        dialogBodyLineHeight = kSetupRowHeight;
    }
    graphics::HermesX_zh::drawMixedBounded(*display, bodyX, bodyY, bodyW, u8"注意：關閉外部通訊", dialogBodyAdvance,
                                           dialogBodyLineHeight, nullptr);
    graphics::HermesX_zh::drawMixedBounded(*display, bodyX, bodyY + dialogBodyLineHeight, bodyW, u8"含藍牙與LoRa傳輸",
                                           dialogBodyAdvance, dialogBodyLineHeight, nullptr);

    auto drawOption = [&](int16_t optionX, const char *label, bool selected, bool enabled) {
        if (!enabled) {
            selected = false;
        }
        if (selected) {
            display->setColor(dialogFg);
            display->fillRect(optionX, optionY, optionW, optionH);
            display->setColor(dialogBg);
        } else {
            display->setColor(dialogFg);
            display->drawRect(optionX, optionY, optionW, optionH);
        }

        const int textW = graphics::HermesX_zh::stringAdvance(label, graphics::HermesX_zh::GLYPH_WIDTH, display);
        int16_t textX = optionX + (optionW - textW) / 2;
        if (textX < optionX + 1) {
            textX = optionX + 1;
        }
        graphics::HermesX_zh::drawMixedBounded(*display, textX, optionY + 1, optionW - 2, label,
                                               graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
        display->setColor(dialogFg);
    };

    drawOption(noX, u8"否", hermesActionStealthConfirmSelected == 0, true);
    drawOption(yesX, u8"是", hermesActionStealthConfirmSelected != 0, yesArmed);

    if (!yesArmed) {
        char lockLine[40];
        const uint32_t remainSec = (remainMs + 999) / 1000;
        snprintf(lockLine, sizeof(lockLine), u8"「是」將於%lus後可用", static_cast<unsigned long>(remainSec));
        graphics::HermesX_zh::drawMixedBounded(*display, bodyX, statusY, bodyW, lockLine, dialogBodyAdvance,
                                               FONT_HEIGHT_SMALL, nullptr);
    } else {
        graphics::HermesX_zh::drawMixedBounded(*display, bodyX, statusY, bodyW, u8"旋鈕選擇，按下確認", dialogBodyAdvance,
                                               FONT_HEIGHT_SMALL, nullptr);
    }
}

void Screen::drawHermesXShareChannel(OLEDDisplay *display, OLEDDisplayUiState * /*state*/, int16_t x, int16_t y)
{
    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();

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
    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    const int16_t gap = 2;
    const int16_t listWidth = 56;
    const int16_t qrWidth = width - listWidth - gap;
    const int16_t qrHeight = height;
    const int16_t qrX = x;
    const int16_t qrY = y;
    const int16_t listX = x + qrWidth + gap;
    const int16_t listW = width - (listX - x) - 2;

    const String url = buildChannelShareUrl(true);

    const int16_t lineHeight = FONT_HEIGHT_SMALL + 2;
    const char *titleLine1 = u8"掃描加入";
    const int16_t titleY = y + 1;
    const int16_t listTitleBottom = titleY + lineHeight;

    auto drawTitleLines = [&]() {
        graphics::HermesX_zh::drawMixedBounded(*display, listX, titleY, listW, titleLine1, graphics::HermesX_zh::GLYPH_WIDTH,
                                               FONT_HEIGHT_SMALL, nullptr);
    };

    auto drawFallback = [&](const char *line1, const char *line2) {
        drawTitleLines();
        graphics::HermesX_zh::drawMixedBounded(*display, listX, listTitleBottom, listW, line1,
                                               graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
        if (line2) {
            graphics::HermesX_zh::drawMixedBounded(*display, listX, listTitleBottom + lineHeight, listW, line2,
                                                   graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
        }
    };

    if (url.length() == 0) {
        drawFallback(u8"無法產生頻道URL", u8"請用App分享");
        return;
    }

#if defined(ARDUINO_ARCH_ESP32)
    if (!ensureHermesXShareQrCache(url)) {
        drawFallback(u8"QR 過密", u8"請用App分享");
        return;
    }
    const int size = gShareQrCache.size;
    const int scaleX = qrWidth / size;
    const int scaleY = qrHeight / size;
    const int scale = (scaleX < scaleY) ? scaleX : scaleY;
    if (scale < 1) {
        drawFallback(u8"QR 過密", u8"請用App分享");
        return;
    }

    gShareQrCtx.display = display;
    gShareQrCtx.x = qrX;
    gShareQrCtx.y = qrY;
    gShareQrCtx.width = qrWidth;
    gShareQrCtx.height = qrHeight;
    gShareQrCtx.margin = 2;

    // Draw a white QR background area for OLED to improve contrast.
#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
    display->fillRect(qrX, qrY, qrWidth, qrHeight);
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    display->fillRect(qrX, qrY, qrWidth, qrHeight);
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif

    if (!drawHermesXShareQrFromCache()) {
        // Reset text color before fallback messages
#if defined(USE_EINK)
        display->setColor(EINK_BLACK);
#else
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
        drawFallback(u8"QR 產生失敗", u8"請用App分享");
        return;
    }
#else
    drawFallback(u8"本裝置不支援QR", nullptr);
    return;
#endif

#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif

    drawTitleLines();
    int16_t textY = listTitleBottom;
    graphics::HermesX_zh::drawMixedBounded(*display, listX, textY, listW, u8"包含頻道:",
                                           graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
    textY += lineHeight;

    int enabledCount = 0;
    for (unsigned int i = 0; i < channels.getNumChannels(); ++i) {
        const auto &ch = channels.getByIndex(i);
        if (ch.role == meshtastic_Channel_Role_PRIMARY || ch.role == meshtastic_Channel_Role_SECONDARY) {
            enabledCount++;
        }
    }
    String countLine = String(u8"共 ") + String(enabledCount) + u8" 個";
    graphics::HermesX_zh::drawMixedBounded(*display, listX, textY, listW, countLine.c_str(),
                                           graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);

    const int16_t bottomLineY = y + height - FONT_HEIGHT_SMALL;
    graphics::HermesX_zh::drawMixedBounded(*display, listX, bottomLineY, listW, u8"QR: 全部",
                                           graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
}

void Screen::drawHermesFastSetup(OLEDDisplay *display, OLEDDisplayUiState * /*state*/, int16_t /*x*/, int16_t /*y*/)
{
    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    const uint32_t nowMs = millis();
    const bool showToast = (hermesSetupToastUntilMs != 0) && (nowMs <= hermesSetupToastUntilMs);
    static int lastPalettePage = -1;
    const int currentPalettePage = static_cast<int>(hermesSetupPage);
    const bool forcePaletteRedraw = (lastPalettePage != currentPalettePage);
    lastPalettePage = currentPalettePage;

    const auto applyPaletteNoSelection = [&]() {
        applyHermesFastSetupTftPalette(display, width, height, 0, 0, 0, showToast, forcePaletteRedraw);
    };
    const auto applyPaletteList = [&](int itemCount) {
        applyHermesFastSetupTftPalette(display, width, height, itemCount, hermesSetupSelected, hermesSetupOffset, showToast,
                                       forcePaletteRedraw);
    };

    if (hermesSetupPage == HermesFastSetupPage::Entry) {
        applyPaletteNoSelection();
        display->setTextAlignment(TEXT_ALIGN_LEFT);
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

        drawSetupHeader(display, width, u8"HermesFastSetup");

        const int16_t line1Y = height - (FONT_HEIGHT_SMALL * 2) - 2;
        const int16_t line2Y = height - FONT_HEIGHT_SMALL;
        const int16_t iconTop = kSetupHeaderHeight + 2;
        const int16_t iconBottom = line1Y - 4;
        int16_t iconRadius = (iconBottom > iconTop) ? ((iconBottom - iconTop) / 2) : 10;
        const int16_t maxRadiusByWidth = (width / 2) - 6;
        if (iconRadius > maxRadiusByWidth) {
            iconRadius = maxRadiusByWidth;
        }
        if (iconRadius < 10) {
            iconRadius = 10;
        }
        const int16_t iconCx = width / 2;
        const int16_t iconCy = iconTop + iconRadius;
        drawSetupNutIcon(display, iconCx, iconCy, iconRadius);

        graphics::HermesX_zh::drawMixedBounded(*display, 2, line1Y, width - 4, u8"旋轉以進入設定",
                                               graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
        graphics::HermesX_zh::drawMixedBounded(*display, 2, line2Y, width - 4, u8"短按=下一頁",
                                               graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::PassEdit) {
        applyPaletteNoSelection();
        const char *header = (hermesSetupEditingSlot == 0) ? u8"設定密碼A" : u8"設定密碼B";
        drawSetupKeyboardPage(display, width, height, header, hermesSetupPassDraft, kSetupKeyRows, kSetupKeyRowLengths,
                              kSetupKeyRowCount, hermesSetupKeyRow, hermesSetupKeyCol, hermesSetupToast,
                              hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::FrequencyEdit) {
        applyPaletteNoSelection();
        drawSetupKeyboardPage(display, width, height, u8"手動設定頻率", hermesSetupFrequencyDraft, kSetupNumericKeyRows,
                              kSetupNumericKeyRowLengths, kSetupNumericKeyRowCount, hermesSetupKeyRow, hermesSetupKeyCol,
                              hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::PassShow) {
        applyPaletteNoSelection();
        display->setTextAlignment(TEXT_ALIGN_LEFT);
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
        drawSetupHeader(display, width, u8"EMAC 密碼");
#if defined(USE_EINK)
        display->setColor(EINK_BLACK);
#else
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
        display->setFont(FONT_SMALL);
        String passA = lighthouseModule ? lighthouseModule->getEmergencyPassphrase(0) : "";
        String passB = lighthouseModule ? lighthouseModule->getEmergencyPassphrase(1) : "";
        if (passA.length() == 0) {
            passA = u8"未設定";
        }
        if (passB.length() == 0) {
            passB = u8"未設定";
        }
        const String lineA = String(u8"密碼A: ") + passA;
        const String lineB = String(u8"密碼B: ") + passB;
        graphics::HermesX_zh::drawMixedBounded(*display, 2, kSetupHeaderHeight + 2, width - 4, lineA.c_str(),
                                               graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
        graphics::HermesX_zh::drawMixedBounded(*display, 2, kSetupHeaderHeight + 2 + FONT_HEIGHT_SMALL + 2, width - 4,
                                               lineB.c_str(), graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
        graphics::HermesX_zh::drawMixedBounded(*display, 2, height - FONT_HEIGHT_SMALL, width - 4, u8"按返回離開",
                                               graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::UiMenu) {
        applyPaletteList(kSetupUiMenuCount);
        String label = u8"全域蜂鳴器: ";
        if (isBuzzerGloballyEnabled()) {
            label += u8"開";
        } else {
            label += u8"關";
        }
        const uint8_t ledBrightness =
            HermesXInterfaceModule::instance ? HermesXInterfaceModule::instance->getUiLedBrightness() : 60;
        const bool ambientEnabled = moduleConfig.has_ambient_lighting && moduleConfig.ambient_lighting.led_state;
        String brightnessLine = String(u8"Hermes狀態條: ") + getSetupBrightnessLabel(ledBrightness);
        String ambientLine = String(u8"板載RGB燈: ") + (ambientEnabled ? u8"開" : u8"關");
        String screenSleepLine = String(u8"螢幕休眠: ") + getSetupScreenSleepLabel(getSetupCurrentScreenSleepSeconds());
        String timezoneLine = String(u8"時區: ") + getSetupTimezoneLabel(config.device.tzdef);
        const bool rotarySwapped =
            moduleConfig.canned_message.inputbroker_event_cw ==
            meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP;
        String rotarySwapLine = String(u8"旋鈕對調: ") + (rotarySwapped ? u8"開" : u8"關");
        const char *items[] = {u8"返回", label.c_str(), brightnessLine.c_str(), ambientLine.c_str(), screenSleepLine.c_str(),
                               timezoneLine.c_str(), rotarySwapLine.c_str()};
        drawSetupList(display, width, height, u8"UI設定", items, kSetupUiMenuCount, hermesSetupSelected, hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::UiBrightnessSelect) {
        applyPaletteList(kSetupBrightnessCount + 1);
        const char *items[kSetupBrightnessCount + 1] = {u8"返回"};
        for (uint8_t i = 0; i < kSetupBrightnessCount; ++i) {
            items[i + 1] = kSetupBrightnessOptions[i].label;
        }
        drawSetupList(display, width, height, u8"Hermes狀態條亮度", items, kSetupBrightnessCount + 1, hermesSetupSelected,
                      hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::UiScreenSleepSelect) {
        applyPaletteList(kSetupScreenSleepCount + 1);
        const char *items[kSetupScreenSleepCount + 1] = {u8"返回"};
        for (uint8_t i = 0; i < kSetupScreenSleepCount; ++i) {
            items[i + 1] = kSetupScreenSleepOptions[i].label;
        }
        drawSetupList(display, width, height, u8"螢幕休眠時間", items, kSetupScreenSleepCount + 1, hermesSetupSelected,
                      hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::UiTimezoneSelect) {
        applyPaletteList(kSetupTimezoneCount + 1);
        const char *items[kSetupTimezoneCount + 1] = {u8"返回"};
        for (uint8_t i = 0; i < kSetupTimezoneCount; ++i) {
            items[i + 1] = kSetupTimezoneOptions[i].label;
        }
        drawSetupList(display, width, height, u8"時區設定", items, kSetupTimezoneCount + 1, hermesSetupSelected,
                      hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::UiRotarySwapSelect) {
        applyPaletteList(3);
        const char *items[] = {u8"返回", u8"關閉", u8"開啟"};
        drawSetupList(display, width, height, u8"旋鈕對調", items, 3, hermesSetupSelected, hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::EmacMenu) {
        applyPaletteList(kSetupEmacCount);
        drawSetupList(display, width, height, u8"EMAC設定", kSetupEmacItems, kSetupEmacCount, hermesSetupSelected,
                      hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::NodeMenu) {
        applyPaletteList(kSetupNodeMenuCount);
        const bool mqttEnabled = moduleConfig.mqtt.enabled;
        String btLine = String(u8"藍牙: ") + (config.bluetooth.enabled ? u8"開" : u8"關");
        String mqttLine = String("MQTT: ") + (mqttEnabled ? u8"開" : u8"關");
        const char *items[] = {u8"返回", "LoRa", u8"GPS", mqttLine.c_str(), u8"頻道設定", btLine.c_str(), u8"電源管理",
                               u8"節點資料庫"};
        drawSetupList(display, width, height, u8"裝置管理", items, kSetupNodeMenuCount, hermesSetupSelected, hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::NodeDatabaseMenu) {
        applyPaletteList(kSetupNodeDatabaseMenuCount);
        const char *items[] = {u8"返回", u8"重設資料庫"};
        drawSetupList(display, width, height, u8"節點資料庫", items, kSetupNodeDatabaseMenuCount, hermesSetupSelected,
                      hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::NodeDatabaseResetSelect) {
        applyPaletteList(kSetupNodeDatabaseResetCount);
        const char *items[] = {u8"返回", u8"清除12hr未更新", u8"清除24hr未更新", u8"清除48hr未更新"};
        drawSetupList(display, width, height, u8"重設資料庫", items, kSetupNodeDatabaseResetCount, hermesSetupSelected,
                      hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::MqttMenu) {
        applyPaletteList(kSetupMqttMenuCount);
        const bool mapEnabled = moduleConfig.mqtt.map_reporting_enabled && moduleConfig.mqtt.has_map_report_settings &&
                                moduleConfig.mqtt.map_report_settings.should_report_location;
        String mqttLine = String("MQTT: ") + (moduleConfig.mqtt.enabled ? u8"開" : u8"關");
        String proxyLine = String(u8"啟用客戶端代理: ") + (moduleConfig.mqtt.proxy_to_client_enabled ? u8"開" : u8"關");
        String mapLine = String(u8"地圖報告: ") + (mapEnabled ? u8"開" : u8"關");
        const char *items[] = {u8"返回", mqttLine.c_str(), proxyLine.c_str(), mapLine.c_str()};
        drawSetupList(display, width, height, "MQTT", items, kSetupMqttMenuCount, hermesSetupSelected, hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::MqttMapReportMenu) {
        applyPaletteList(kSetupMqttMapReportMenuCount);
        const bool mapEnabled = moduleConfig.mqtt.map_reporting_enabled && moduleConfig.mqtt.has_map_report_settings &&
                                moduleConfig.mqtt.map_report_settings.should_report_location;
        String enabledLine = String(u8"上報: ") + (mapEnabled ? u8"開" : u8"關");
        String precisionLine = String(u8"精確度: ") + getSetupMqttMapPrecisionLabel(getSetupCurrentMqttMapPrecision());
        String publishLine = String(u8"廣播間隔: ") + formatSetupSecondsLabel(getSetupCurrentMqttMapPublishInterval());
        const char *items[] = {u8"返回", enabledLine.c_str(), precisionLine.c_str(), publishLine.c_str()};
        drawSetupList(display, width, height, u8"地圖報告", items, kSetupMqttMapReportMenuCount, hermesSetupSelected,
                      hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::MqttMapPrecisionSelect) {
        const int itemCount = kSetupMqttMapPrecisionCount + 1;
        applyPaletteList(itemCount);
        const char *items[kSetupMqttMapPrecisionCount + 1];
        items[0] = u8"返回";
        for (uint8_t i = 0; i < kSetupMqttMapPrecisionCount; ++i) {
            items[i + 1] = kSetupMqttMapPrecisionOptions[i].label;
        }
        drawSetupList(display, width, height, u8"精確度", items, itemCount, hermesSetupSelected, hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::MqttMapPublishSelect) {
        const int itemCount = kSetupMqttMapPublishCount + 1;
        applyPaletteList(itemCount);
        const char *items[kSetupMqttMapPublishCount + 1];
        items[0] = u8"返回";
        for (uint8_t i = 0; i < kSetupMqttMapPublishCount; ++i) {
            items[i + 1] = kSetupMqttMapPublishLabels[i];
        }
        drawSetupList(display, width, height, u8"廣播間隔", items, itemCount, hermesSetupSelected, hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::ChannelMenu) {
        ChannelIndex channelList[MAX_NUM_CHANNELS];
        const uint8_t channelCount = buildSetupChannelList(channelList, MAX_NUM_CHANNELS);
        const int itemCount = channelCount + 1;
        applyPaletteList(itemCount);
        String itemLabels[MAX_NUM_CHANNELS + 1];
        const char *items[MAX_NUM_CHANNELS + 1];
        itemLabels[0] = u8"返回";
        items[0] = itemLabels[0].c_str();
        for (uint8_t i = 0; i < channelCount; ++i) {
            itemLabels[i + 1] = getSetupChannelMenuLabel(channelList[i]);
            items[i + 1] = itemLabels[i + 1].c_str();
        }
        drawSetupList(display, width, height, u8"頻道設定", items, itemCount, hermesSetupSelected, hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::ChannelDetailMenu) {
        applyPaletteList(kSetupChannelDetailMenuCount);
        const auto channel = getSetupChannelCopy(hermesSetupChannelIndex);
        const bool shareEnabled = isSetupChannelPositionSharingEnabled(hermesSetupChannelIndex);
        String title = getSetupChannelMenuLabel(hermesSetupChannelIndex);
        String uplinkLine = String(u8"上行: ") + (channel.settings.uplink_enabled ? u8"開" : u8"關");
        String downlinkLine = String(u8"下行: ") + (channel.settings.downlink_enabled ? u8"開" : u8"關");
        String shareLine = String(u8"位置分享: ") + (shareEnabled ? u8"開" : u8"關");
        String precisionLine =
            String(u8"精確度: ") + (shareEnabled ? getSetupChannelPrecisionLabel(getSetupChannelPrecision(hermesSetupChannelIndex))
                                                : u8"關閉");
        const char *items[] = {u8"返回", uplinkLine.c_str(), downlinkLine.c_str(), shareLine.c_str(), precisionLine.c_str()};
        drawSetupList(display, width, height, title.c_str(), items, kSetupChannelDetailMenuCount, hermesSetupSelected,
                      hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::ChannelPrecisionSelect) {
        const int itemCount = kSetupChannelPrecisionCount + 1;
        applyPaletteList(itemCount);
        const char *items[kSetupChannelPrecisionCount + 1];
        items[0] = u8"返回";
        for (uint8_t i = 0; i < kSetupChannelPrecisionCount; ++i) {
            items[i + 1] = kSetupChannelPrecisionOptions[i].label;
        }
        drawSetupList(display, width, height, u8"精確度", items, itemCount, hermesSetupSelected, hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::PowerMenu) {
        applyPaletteList(kSetupPowerMenuCount);
        String currentVoltageLine = String(u8"當前電壓: ") + getSetupCurrentVoltageLabel();
        String guardLine = String(u8"過放保護: ") + (HermesXBatteryProtection::isEnabled() ? u8"開" : u8"關");
        String thresholdLine =
            String(u8"過放門檻: ") + formatSetupVoltageMvLabel(HermesXBatteryProtection::getThresholdMv());
        const char *items[] = {u8"返回", currentVoltageLine.c_str(), guardLine.c_str(), thresholdLine.c_str()};
        drawSetupList(display, width, height, u8"電源管理", items, kSetupPowerMenuCount, hermesSetupSelected, hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::PowerGuardVoltageSelect) {
        const int itemCount = kSetupPowerGuardThresholdCount + 1;
        applyPaletteList(itemCount);
        const char *items[kSetupPowerGuardThresholdCount + 1];
        items[0] = u8"返回";
        for (uint8_t i = 0; i < kSetupPowerGuardThresholdCount; ++i) {
            items[i + 1] = kSetupPowerGuardThresholdOptions[i].label;
        }
        drawSetupList(display, width, height, u8"過放門檻", items, itemCount, hermesSetupSelected, hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::LoraMenu) {
        applyPaletteList(kSetupLoraMenuCount);
        String roleLine = String("Role: ") + getSetupRoleOptionLabel(config.device.role);
        const char *presetLabel = config.lora.use_preset ? getSetupLoraPresetLabel(config.lora.modem_preset) : "Custom";
        String presetLine = String("Preset: ") + presetLabel;
        String regionLine = String(u8"地區: ") + getSetupRegionLabel(config.lora.region);
        String ignoreMqttLine = String(u8"無視MQTT: ") + (config.lora.ignore_mqtt ? u8"開" : u8"關");
        String allowMqttLine = String(u8"允許轉發至MQTT: ") + (config.lora.config_ok_to_mqtt ? u8"開" : u8"關");
        String txLine = String(u8"啟用LoRa: ") + (config.lora.tx_enabled ? u8"開" : u8"關");
        String slotLine = String(u8"頻段槽位: ") + (config.lora.channel_num ? String(config.lora.channel_num) : String(u8"自動"));
        String freqLine = String(u8"手動頻率: ") + formatSetupFrequencyLabel(config.lora.override_frequency);
        if (fabsf(config.lora.override_frequency) >= 0.0001f) {
            freqLine += "MHz";
        }
        const char *items[] = {u8"返回",         roleLine.c_str(),    presetLine.c_str(),     regionLine.c_str(),
                               ignoreMqttLine.c_str(), allowMqttLine.c_str(), txLine.c_str(), slotLine.c_str(),
                               freqLine.c_str()};
        drawSetupList(display, width, height, "LoRa", items, kSetupLoraMenuCount, hermesSetupSelected, hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::LoraRoleSelect) {
        const int itemCount = kSetupRoleOptionCount + 1;
        applyPaletteList(itemCount);
        const char *items[kSetupRoleOptionCount + 1];
        items[0] = u8"返回";
        for (uint8_t i = 0; i < kSetupRoleOptionCount; ++i) {
            items[i + 1] = kSetupRoleOptions[i].label;
        }
        drawSetupList(display, width, height, "Role", items, itemCount, hermesSetupSelected, hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::LoraPresetSelect) {
        const int itemCount = kSetupLoraPresetOptionCount + 1;
        applyPaletteList(itemCount);
        const char *items[kSetupLoraPresetOptionCount + 1];
        items[0] = u8"返回";
        for (uint8_t i = 0; i < kSetupLoraPresetOptionCount; ++i) {
            items[i + 1] = kSetupLoraPresetOptions[i].label;
        }
        drawSetupList(display, width, height, "Preset", items, itemCount, hermesSetupSelected, hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::LoraRegionSelect) {
        const uint8_t regionCount = getSetupRegionOptionCount();
        const int itemCount = regionCount + 1;
        applyPaletteList(itemCount);
        const char *items[kSetupMaxRegionOptions + 1];
        items[0] = u8"返回";
        for (uint8_t i = 0; i < regionCount && i < kSetupMaxRegionOptions; ++i) {
            items[i + 1] = regions[i].name;
        }
        drawSetupList(display, width, height, u8"地區", items, itemCount, hermesSetupSelected, hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::LoraChannelSlotSelect) {
        const int itemCount = static_cast<int>(getSetupLoraChannelSlotCount()) + 2;
        applyPaletteList(itemCount);
        display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
        display->setColor(EINK_WHITE);
#else
        display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
        display->fillRect(0, 0, width, height);
        drawSetupHeader(display, width, u8"頻段槽位");
#if defined(USE_EINK)
        display->setColor(EINK_BLACK);
#else
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
        const int16_t listTop = kSetupHeaderHeight + 2;
        for (int i = 0; i < kSetupVisibleRows; ++i) {
            const int entryIndex = hermesSetupOffset + i;
            if (entryIndex >= itemCount) {
                break;
            }
            const int16_t rowY = listTop + i * kSetupRowHeight;
            char labelBuf[20];
            const char *label = nullptr;
            if (entryIndex == 0) {
                label = u8"返回";
            } else if (entryIndex == 1) {
                label = u8"自動";
            } else {
                snprintf(labelBuf, sizeof(labelBuf), "%d", entryIndex - 1);
                label = labelBuf;
            }
            if (entryIndex == hermesSetupSelected) {
#if defined(USE_EINK)
                display->fillRect(0, rowY - 1, width, kSetupRowHeight);
                display->setColor(EINK_WHITE);
#else
                display->setColor(OLEDDISPLAY_COLOR::WHITE);
                display->fillRect(0, rowY - 1, width, kSetupRowHeight);
                display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
                graphics::HermesX_zh::drawMixedBounded(*display, 4, rowY, width - 4, label,
                                                       graphics::HermesX_zh::GLYPH_WIDTH, kSetupRowHeight, nullptr);
#if defined(USE_EINK)
                display->setColor(EINK_BLACK);
#else
                display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
            } else {
                graphics::HermesX_zh::drawMixedBounded(*display, 4, rowY, width - 4, label,
                                                       graphics::HermesX_zh::GLYPH_WIDTH, kSetupRowHeight, nullptr);
            }
        }
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::CannedMenu) {
        applyPaletteList(2);
        String channelLine = u8"目標頻道: ";
        if (cannedMessageModule) {
            ChannelIndex chan = cannedMessageModule->getPreferredChannel();
            const char *name = channels.getName(chan);
            channelLine += name ? name : u8"未知";
        } else {
            channelLine += u8"未知";
        }
        const char *items[] = {u8"返回", channelLine.c_str()};
        drawSetupList(display, width, height, u8"罐頭訊息", items, 2, hermesSetupSelected, hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::CannedChannelSelect) {
        ChannelIndex channelList[MAX_NUM_CHANNELS];
        const uint8_t channelCount = buildSetupChannelList(channelList, MAX_NUM_CHANNELS);
        const uint8_t itemCount = channelCount + 1;
        applyPaletteList(itemCount);
        const char *items[MAX_NUM_CHANNELS + 1];
        items[0] = u8"返回";
        for (uint8_t i = 0; i < channelCount; ++i) {
            const char *name = channels.getName(channelList[i]);
            items[i + 1] = name ? name : u8"未知";
        }
        drawSetupList(display, width, height, u8"目標頻道", items, itemCount, hermesSetupSelected, hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::GpsMenu) {
        applyPaletteList(kSetupGpsMenuCount);
        const uint32_t currentUpdate =
            Default::getConfiguredOrDefault(config.position.gps_update_interval, default_gps_update_interval);
        const uint32_t currentBroadcast =
            Default::getConfiguredOrDefault(config.position.position_broadcast_secs, default_broadcast_interval_secs);
        const uint32_t currentSmartDistance = getSetupCurrentGpsSmartDistance();
        const uint32_t currentSmartInterval = getSetupCurrentGpsSmartInterval();
        const char *updateLabel = nullptr;
        for (uint8_t i = 0; i < kSetupGpsUpdateCount; ++i) {
            if (kSetupGpsUpdateOptions[i] == currentUpdate) {
                updateLabel = kSetupGpsUpdateLabels[i];
                break;
            }
        }
        const char *broadcastLabel = nullptr;
        for (uint8_t i = 0; i < kSetupGpsBroadcastCount; ++i) {
            if (kSetupGpsBroadcastOptions[i] == currentBroadcast) {
                broadcastLabel = kSetupGpsBroadcastLabels[i];
                break;
            }
        }
        String updateLine = String(u8"衛星更新: ") + (updateLabel ? updateLabel : (String(currentUpdate) + u8"秒"));
        String broadcastLine = String(u8"廣播時間: ") + (broadcastLabel ? broadcastLabel : (String(currentBroadcast) + u8"秒"));
        String smartLine = String(u8"智慧位置: ") + (config.position.position_broadcast_smart_enabled ? u8"開" : u8"關");
        String smartDistanceLine = String(u8"最小距離: ") + formatSetupDistanceLabel(currentSmartDistance);
        String smartIntervalLine = String(u8"最小間隔: ") + formatSetupSecondsLabel(currentSmartInterval);
        const char *items[] = {u8"返回", updateLine.c_str(),       broadcastLine.c_str(),
                               smartLine.c_str(), smartDistanceLine.c_str(), smartIntervalLine.c_str()};
        drawSetupList(display, width, height, u8"GPS", items, kSetupGpsMenuCount, hermesSetupSelected, hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::GpsUpdateSelect) {
        const int itemCount = kSetupGpsUpdateCount + 1;
        applyPaletteList(itemCount);
        const char *items[kSetupGpsUpdateCount + 1];
        items[0] = u8"返回";
        for (uint8_t i = 0; i < kSetupGpsUpdateCount; ++i) {
            items[i + 1] = kSetupGpsUpdateLabels[i];
        }
        drawSetupList(display, width, height, u8"衛星更新", items, itemCount, hermesSetupSelected, hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::GpsBroadcastSelect) {
        const int itemCount = kSetupGpsBroadcastCount + 1;
        applyPaletteList(itemCount);
        const char *items[kSetupGpsBroadcastCount + 1];
        items[0] = u8"返回";
        for (uint8_t i = 0; i < kSetupGpsBroadcastCount; ++i) {
            items[i + 1] = kSetupGpsBroadcastLabels[i];
        }
        drawSetupList(display, width, height, u8"廣播時間", items, itemCount, hermesSetupSelected, hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::GpsSmartDistanceSelect) {
        const int itemCount = kSetupGpsSmartDistanceCount + 1;
        applyPaletteList(itemCount);
        const char *items[kSetupGpsSmartDistanceCount + 1];
        items[0] = u8"返回";
        for (uint8_t i = 0; i < kSetupGpsSmartDistanceCount; ++i) {
            items[i + 1] = kSetupGpsSmartDistanceLabels[i];
        }
        drawSetupList(display, width, height, u8"最小距離", items, itemCount, hermesSetupSelected, hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::GpsSmartIntervalSelect) {
        const int itemCount = kSetupGpsSmartIntervalCount + 1;
        applyPaletteList(itemCount);
        const char *items[kSetupGpsSmartIntervalCount + 1];
        items[0] = u8"返回";
        for (uint8_t i = 0; i < kSetupGpsSmartIntervalCount; ++i) {
            items[i + 1] = kSetupGpsSmartIntervalLabels[i];
        }
        drawSetupList(display, width, height, u8"最小間隔", items, itemCount, hermesSetupSelected, hermesSetupOffset);
        drawSetupToast(display, width, height, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    applyPaletteList(kSetupRootCount);
    drawSetupList(display, width, height, u8"HermesFastSetup", kSetupRootItems, kSetupRootCount, hermesSetupSelected,
                  hermesSetupOffset);
    if (hermesSetupToastUntilMs != 0) {
        if (millis() > hermesSetupToastUntilMs) {
            hermesSetupToastUntilMs = 0;
            hermesSetupToast = "";
        } else if (hermesSetupToast.length() > 0) {
            graphics::HermesX_zh::drawMixedBounded(*display, 2, height - FONT_HEIGHT_SMALL, width - 4,
                                                   hermesSetupToast.c_str(), graphics::HermesX_zh::GLYPH_WIDTH,
                                                   FONT_HEIGHT_SMALL, nullptr);
        }
    }
}

#if defined(ESP_PLATFORM) && defined(USE_ST7789)
SPIClass SPI1(HSPI);
#endif

Screen::Screen(ScanI2C::DeviceAddress address, meshtastic_Config_DisplayConfig_OledType screenType, OLEDDISPLAY_GEOMETRY geometry)
    : concurrency::OSThread("Screen"), address_found(address), model(screenType), geometry(geometry), cmdQueue(32)
{
    graphics::normalFrames = new FrameCallback[MAX_NUM_NODES + NUM_EXTRA_FRAMES];
#if defined(USE_SH1106) || defined(USE_SH1107) || defined(USE_SH1107_128_64)
    dispdev = new SH1106Wire(address.address, -1, -1, geometry,
                             (address.port == ScanI2C::I2CPort::WIRE1) ? HW_I2C::I2C_TWO : HW_I2C::I2C_ONE);
#elif defined(USE_ST7789)
#ifdef ESP_PLATFORM
    dispdev = new ST7789Spi(&SPI1, ST7789_RESET, ST7789_RS, ST7789_NSS, GEOMETRY_RAWMODE, TFT_WIDTH, TFT_HEIGHT, ST7789_SDA,
                            ST7789_MISO, ST7789_SCK);
#else
    dispdev = new ST7789Spi(&SPI1, ST7789_RESET, ST7789_RS, ST7789_NSS, GEOMETRY_RAWMODE, TFT_WIDTH, TFT_HEIGHT);
#endif
#elif defined(USE_SSD1306)
    dispdev = new SSD1306Wire(address.address, -1, -1, geometry,
                              (address.port == ScanI2C::I2CPort::WIRE1) ? HW_I2C::I2C_TWO : HW_I2C::I2C_ONE);
#elif defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||    \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
    dispdev = new TFTDisplay(address.address, -1, -1, geometry,
                             (address.port == ScanI2C::I2CPort::WIRE1) ? HW_I2C::I2C_TWO : HW_I2C::I2C_ONE);
#elif defined(USE_EINK) && !defined(USE_EINK_DYNAMICDISPLAY)
    dispdev = new EInkDisplay(address.address, -1, -1, geometry,
                              (address.port == ScanI2C::I2CPort::WIRE1) ? HW_I2C::I2C_TWO : HW_I2C::I2C_ONE);
#elif defined(USE_EINK) && defined(USE_EINK_DYNAMICDISPLAY)
    dispdev = new EInkDynamicDisplay(address.address, -1, -1, geometry,
                                     (address.port == ScanI2C::I2CPort::WIRE1) ? HW_I2C::I2C_TWO : HW_I2C::I2C_ONE);
#elif defined(USE_ST7567)
    dispdev = new ST7567Wire(address.address, -1, -1, geometry,
                             (address.port == ScanI2C::I2CPort::WIRE1) ? HW_I2C::I2C_TWO : HW_I2C::I2C_ONE);
#elif ARCH_PORTDUINO && !HAS_TFT
    if (settingsMap[displayPanel] != no_screen) {
        LOG_DEBUG("Make TFTDisplay!");
        dispdev = new TFTDisplay(address.address, -1, -1, geometry,
                                 (address.port == ScanI2C::I2CPort::WIRE1) ? HW_I2C::I2C_TWO : HW_I2C::I2C_ONE);
    } else {
        dispdev = new AutoOLEDWire(address.address, -1, -1, geometry,
                                   (address.port == ScanI2C::I2CPort::WIRE1) ? HW_I2C::I2C_TWO : HW_I2C::I2C_ONE);
        isAUTOOled = true;
    }
#else
    dispdev = new AutoOLEDWire(address.address, -1, -1, geometry,
                               (address.port == ScanI2C::I2CPort::WIRE1) ? HW_I2C::I2C_TWO : HW_I2C::I2C_ONE);
    isAUTOOled = true;
#endif

    ui = new OLEDDisplayUi(dispdev);
    cmdQueue.setReader(this);
}

Screen::~Screen()
{
    delete[] graphics::normalFrames;
}

/**
 * Prepare the display for the unit going to the lowest power mode possible.  Most screens will just
 * poweroff, but eink screens will show a "I'm sleeping" graphic, possibly with a QR code
 */
void Screen::doDeepSleep()
{
#ifdef USE_EINK
    setOn(false, drawDeepSleepScreen);
#ifdef PIN_EINK_EN
    digitalWrite(PIN_EINK_EN, LOW); // power off backlight
#endif
#else
    // Without E-Ink display:
    setOn(false);
#endif
}

void Screen::handleSetOn(bool on, FrameCallback einkScreensaver)
{
    if (!useDisplay)
        return;

    if (on && !screenOn && isStealthModeActive()) {
        const uint32_t now = millis();
        if (stealthScreenWakeUntilMs == 0 || now >= stealthScreenWakeUntilMs) {
            return;
        }
    }

    if (on != screenOn) {
        if (on) {
            const uint8_t wakeRestoreFrameIndex =
                (showingNormalScreen && ui && ui->getUiState()) ? ui->getUiState()->currentFrame : 0xFF;
            bool reinitUiOnWake = false;
            LOG_INFO("Turn on screen");
            if (buttonThread) {
                buttonThread->setScreenFlag(true);
            }
            powerMon->setState(meshtastic_PowerMon_State_Screen_On);
#ifdef T_WATCH_S3
            PMU->enablePowerOutput(XPOWERS_ALDO2);
#endif
#ifdef HELTEC_TRACKER_V1_X
            uint8_t tft_vext_enabled = digitalRead(VEXT_ENABLE);
#endif
#if !ARCH_PORTDUINO
            dispdev->displayOn();
#endif

#if defined(ST7789_CS) &&                                                                                                        \
    !defined(M5STACK) // set display brightness when turning on screens. Just moved function from TFTDisplay to here.
            static_cast<TFTDisplay *>(dispdev)->setDisplayBrightness(brightness);
#endif

            dispdev->displayOn();
#ifdef HELTEC_TRACKER_V1_X
            // If the TFT VEXT power is not enabled, initialize the UI.
            if (!tft_vext_enabled) {
                ui->init();
                reinitUiOnWake = true;
            }
#endif
#ifdef USE_ST7789
            pinMode(VTFT_CTRL, OUTPUT);
            digitalWrite(VTFT_CTRL, LOW);
            ui->init();
            reinitUiOnWake = true;
#ifdef ESP_PLATFORM
            analogWrite(VTFT_LEDA, BRIGHTNESS_DEFAULT);
#else
            pinMode(VTFT_LEDA, OUTPUT);
            digitalWrite(VTFT_LEDA, TFT_BACKLIGHT_ON);
#endif
#endif
// Some TFTs (e.g. ST7735/ILI9xxx/ST77xx) lose GRAM when VEXT/backlight is cut; re-init UI on wake to avoid blank lit screen.
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||     \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
            ui->init();
            reinitUiOnWake = true;
#endif
            if (reinitUiOnWake) {
                invalidateDirectTftWakeCaches();
                if (showingNormalScreen && gNormalFramesInitializedAfterBoot) {
                    setFrames(FOCUS_DEFAULT);
                    if (wakeRestoreFrameIndex < framesetInfo.frameCount) {
                        ui->switchToFrame(wakeRestoreFrameIndex);
                    }
                    setFastFramerate();
                }
            }
            enabled = true;
            wakeInputGuardUntilMs = millis() + kScreenWakeInputGuardMs;
            setInterval(0); // Draw ASAP
            runASAP = true;
            // 重新開啟後強制更新畫面，避免黑屏
            forceDisplay(true);
        } else {
            invalidateDirectTftWakeCaches();
            wakeInputGuardUntilMs = 0;
            dismissIncomingTextPopup();
            powerMon->clearState(meshtastic_PowerMon_State_Screen_On);
#ifdef USE_EINK
            // eInkScreensaver parameter is usually NULL (default argument), default frame used instead
            setScreensaverFrames(einkScreensaver);
#endif
            LOG_INFO("Turn off screen");
            if (buttonThread) {
                buttonThread->setScreenFlag(false);
            }
#ifdef ELECROW_ThinkNode_M1
            if (digitalRead(PIN_EINK_EN) == HIGH) {
                digitalWrite(PIN_EINK_EN, LOW);
            }
#endif
            dispdev->displayOff();
#ifdef USE_ST7789
            SPI1.end();
#if defined(ARCH_ESP32)
            pinMode(VTFT_LEDA, ANALOG);
            pinMode(VTFT_CTRL, ANALOG);
            pinMode(ST7789_RESET, ANALOG);
            pinMode(ST7789_RS, ANALOG);
            pinMode(ST7789_NSS, ANALOG);
#else
            nrf_gpio_cfg_default(VTFT_LEDA);
            nrf_gpio_cfg_default(VTFT_CTRL);
            nrf_gpio_cfg_default(ST7789_RESET);
            nrf_gpio_cfg_default(ST7789_RS);
            nrf_gpio_cfg_default(ST7789_NSS);
#endif
#endif

#ifdef T_WATCH_S3
            PMU->disablePowerOutput(XPOWERS_ALDO2);
#endif
            enabled = false;
        }
        screenOn = on;
        if (!on && isStealthModeActive()) {
            stealthScreenWakeUntilMs = 0;
        }
    }
}

static FrameCallback bootScreenFrames[1];
static bool bootScreenForceLogo = false;
static bool bootScreenShowHermesWelcome = false;
static void drawHermesXBootHoldFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);

void Screen::setup()
{
    // We don't set useDisplay until setup() is called, because some boards have a declaration of this object but the device
    // is never found when probing i2c and therefore we don't call setup and never want to do (invalid) accesses to this device.
    useDisplay = true;

#ifdef AutoOLEDWire_h
    if (isAUTOOled)
        static_cast<AutoOLEDWire *>(dispdev)->setDetected(model);
#endif

#ifdef USE_SH1107_128_64
    static_cast<SH1106Wire *>(dispdev)->setSubtype(7);
#endif

#if defined(USE_ST7789) && defined(TFT_MESH)
    // Heltec T114 and T190: honor a custom text color, if defined in variant.h
    static_cast<ST7789Spi *>(dispdev)->setRGB(TFT_MESH);
#endif

    // Initialising the UI will init the display too.
    ui->init();

    displayWidth = dispdev->width();
    displayHeight = dispdev->height();

    ui->setTimePerTransition(0);

    ui->setIndicatorPosition(BOTTOM);
    // Defines where the first frame is located in the bar.
    ui->setIndicatorDirection(LEFT_RIGHT);
    ui->setFrameAnimation(SLIDE_LEFT);
    // Don't show the page swipe dots while in boot screen.
    ui->disableAllIndicators();
    // Store a pointer to Screen so we can get to it from static functions.
    ui->getUiState()->userData = this;

    // Set the utf8 conversion function
    dispdev->setFontTableLookupFunction(customFontTableLookup);

    const bool showHermesWelcome =
#if !MESHTASTIC_EXCLUDE_HERMESX && defined(HERMESX_GUARD_POWER_ANIMATIONS)
        HermesXPowerGuard::guardEnabled() && !HermesXPowerGuard::bootHoldPending() && !HermesXPowerGuard::wokeFromSleep();
#else
        false;
#endif

#ifdef USERPREFS_OEM_TEXT
    logo_timeout *= 2; // Double the time if we have a custom logo
#endif
#if !MESHTASTIC_EXCLUDE_HERMESX && defined(HERMESX_GUARD_POWER_ANIMATIONS)
    // HermesX expects a strict 3s boot logo window.
    logo_timeout = 3000;
#endif
    bootScreenShowHermesWelcome = showHermesWelcome;

    // Add frames.
    EINK_ADD_FRAMEFLAG(dispdev, DEMAND_FAST);
    bootScreenFrames[0] = [this](OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) -> void {
#if !MESHTASTIC_EXCLUDE_HERMESX && defined(HERMESX_GUARD_POWER_ANIMATIONS)
        if (bootScreenShowHermesWelcome) {
            drawHermesXBootHoldFrame(display, state, x, y);
            return;
        }
#endif
#if !MESHTASTIC_EXCLUDE_HERMESX && defined(HERMESX_GUARD_POWER_ANIMATIONS)
        if (HermesXPowerGuard::guardEnabled() && HermesXPowerGuard::bootHoldPending()) {
            static const char *const kBootHoldDots[] = {".", "..", "...", "...."};
            const uint8_t phase = static_cast<uint8_t>((millis() / 500) % 4);
            drawFrameText(display, state, x, y, kBootHoldDots[phase]);
            return;
        }
#endif
#ifdef ARCH_ESP32
        if (!bootScreenForceLogo && (wakeCause == ESP_SLEEP_WAKEUP_TIMER || wakeCause == ESP_SLEEP_WAKEUP_EXT1)) {
            drawFrameText(display, state, x, y, "Resuming...");
        } else
#endif
        {
            // Draw region in upper left
            const char *region = myRegion ? myRegion->name : NULL;
            drawIconScreen(region, display, state, x, y);
        }
    };
    alertFrames[0] = showHermesWelcome ? drawHermesXBootHoldFrame : bootScreenFrames[0];
    ui->setFrames(alertFrames, 1);
    // No overlays.
    ui->setOverlays(nullptr, 0);

    // Require presses to switch between frames.
    ui->disableAutoTransition();

    // Set up a log buffer with 3 lines, 32 chars each.
    dispdev->setLogBuffer(3, 32);

    if (showHermesWelcome) {
        startBootHoldReveal(1000);
    }

#ifdef SCREEN_MIRROR
    dispdev->mirrorScreen();
#else
    // Standard behaviour is to FLIP the screen (needed on T-Beam). If this config item is set, unflip it, and thereby logically
    // flip it. If you have a headache now, you're welcome.
    if (!config.display.flip_screen) {
#if defined(ST7701_CS) || defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) ||      \
    defined(ST7789_CS) || defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
        static_cast<TFTDisplay *>(dispdev)->flipScreenVertically();
#elif defined(USE_ST7789)
        static_cast<ST7789Spi *>(dispdev)->flipScreenVertically();
#else
        dispdev->flipScreenVertically();
#endif
    }
#endif

    // Get our hardware ID
    uint8_t dmac[6];
    getMacAddr(dmac);
    snprintf(ourId, sizeof(ourId), "%02x%02x", dmac[4], dmac[5]);
#if ARCH_PORTDUINO
    handleSetOn(false); // force clean init
#endif

    // Turn on the display.
    handleSetOn(true);

    // On some ssd1306 clones, the first draw command is discarded, so draw it
    // twice initially. Skip this for EINK Displays to save a few seconds during boot
    ui->update();
#ifndef USE_EINK
    ui->update();
#endif
    serialSinceMsec = millis();

#if ARCH_PORTDUINO && !HAS_TFT
    if (settingsMap[touchscreenModule]) {
        touchScreenImpl1 =
            new TouchScreenImpl1(dispdev->getWidth(), dispdev->getHeight(), static_cast<TFTDisplay *>(dispdev)->getTouch);
        touchScreenImpl1->init();
    }
#elif HAS_TOUCHSCREEN
    touchScreenImpl1 =
        new TouchScreenImpl1(dispdev->getWidth(), dispdev->getHeight(), static_cast<TFTDisplay *>(dispdev)->getTouch);
    touchScreenImpl1->init();
#endif

    // Subscribe to status updates
    powerStatusObserver.observe(&powerStatus->onNewStatus);
    gpsStatusObserver.observe(&gpsStatus->onNewStatus);
    nodeStatusObserver.observe(&nodeStatus->onNewStatus);
}

void Screen::attachModuleObservers()
{
    if (moduleObserversAttached)
        return;
#if !MESHTASTIC_EXCLUDE_ADMIN
    if (adminModule)
        adminMessageObserver.observe(adminModule);
#endif
    if (textMessageModule)
        textMessageObserver.observe(textMessageModule);
    if (inputBroker)
        inputObserver.observe(inputBroker);

    // Modules can notify screen about refresh
    MeshModule::observeUIEvents(&uiFrameEventObserver);

    moduleObserversAttached = true;
}

void Screen::forceDisplay(bool forceUiUpdate)
{
    // Nasty hack to force epaper updates for 'key' frames.  FIXME, cleanup.
#ifdef USE_EINK
    // If requested, make sure queued commands are run, and UI has rendered a new frame
    if (forceUiUpdate) {
        // Force a display refresh, in addition to the UI update
        // Changing the GPS status bar icon apparently doesn't register as a change in image
        // (False negative of the image hashing algorithm used to skip identical frames)
        EINK_ADD_FRAMEFLAG(dispdev, DEMAND_FAST);

        // No delay between UI frame rendering
        setFastFramerate();

        // Make sure all CMDs have run first
        while (!cmdQueue.isEmpty())
            runOnce();

        // Ensure at least one frame has drawn
        uint64_t startUpdate;
        do {
            startUpdate = millis(); // Handle impossibly unlikely corner case of a millis() overflow..
            delay(10);
            ui->update();
        } while (ui->getUiState()->lastUpdate < startUpdate);

        // Return to normal frame rate
        targetFramerate = IDLE_FRAMERATE;
        ui->setTargetFPS(targetFramerate);
    }

    // Tell EInk class to update the display
    static_cast<EInkDisplay *>(dispdev)->forceDisplay();
#endif
}

static uint32_t lastScreenTransition;
static bool pendingNormalFrames = false;
static bool hermesXBootHoldActive = false;
static bool hermesXBootHoldReveal = false;
static bool hermesXBootHoldAlertStarted = false;
static uint32_t hermesXBootHoldRevealUntilMs = 0;
static uint32_t hermesXBootHoldHeldMs = 0;
static uint32_t hermesXBootHoldLongMs = 1;
static bool hermesXBootHoldBootScreenPending = false;
static uint32_t hermesXBootHoldBootScreenAtMs = 0;

static bool showingBootScreen = true;
static uint32_t bootScreenStartMs = 0;
#ifdef USERPREFS_OEM_TEXT
static bool showingOEMBootScreen = true;
#endif

struct BootHoldPoint {
    float nx;
    float ny;
};

struct BootHoldEdge {
    uint8_t a;
    uint8_t b;
};

static float clamp01(float v)
{
    if (v < 0.0f)
        return 0.0f;
    if (v > 1.0f)
        return 1.0f;
    return v;
}

static int16_t lerpI16(int16_t a, int16_t b, float t)
{
    const float tt = clamp01(t);
    return static_cast<int16_t>(a + static_cast<float>(b - a) * tt);
}

static float edgeLength(const int16_t x0, const int16_t y0, const int16_t x1, const int16_t y1)
{
    const float dx = static_cast<float>(x1 - x0);
    const float dy = static_cast<float>(y1 - y0);
    return sqrtf(dx * dx + dy * dy);
}

static int16_t clampI16(int16_t v, int16_t lo, int16_t hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static void drawLineThick(OLEDDisplay *display, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t thickness)
{
    display->drawLine(x0, y0, x1, y1);
    if (thickness <= 1)
        return;

    const int16_t dx = (x1 >= x0) ? (x1 - x0) : (x0 - x1);
    const int16_t dy = (y1 >= y0) ? (y1 - y0) : (y0 - y1);
    const bool horizontalish = dx >= dy;

    // Draw a small perpendicular halo to simulate thicker strokes.
    const int8_t offsets[] = {1, -1};
    const uint8_t extra = thickness >= 3 ? 2 : 1;
    for (uint8_t i = 0; i < extra; ++i) {
        const int8_t off = offsets[i];
        if (horizontalish) {
            display->drawLine(x0, y0 + off, x1, y1 + off);
        } else {
            display->drawLine(x0 + off, y0, x1 + off, y1);
        }
    }
}

static void drawBootHoldPartialLine(OLEDDisplay *display, int16_t x0, int16_t y0, int16_t x1, int16_t y1, float t,
                                    uint8_t thickness, bool drawTip)
{
    const int16_t ex = lerpI16(x0, x1, t);
    const int16_t ey = lerpI16(y0, y1, t);
    drawLineThick(display, x0, y0, ex, ey, thickness);
    if (drawTip) {
        display->drawCircle(ex, ey, 2);
    }
}

static void drawHermesXBootHoldFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    (void)state;
    if (!display)
        return;

    // Normalized points (0..1) aligned to the latest reference mock.
    static const BootHoldPoint kPoints[] = {
        {1.00f, 0.38f}, // extension start (forced to right edge below)
        {0.90f, 0.38f}, // right node
        {0.50f, 0.08f}, // top node
        {0.07f, 0.40f}, // left node
        {0.20f, 0.72f}, // left-bottom node
        {0.50f, 0.80f}, // bottom node
    };
    static const BootHoldEdge kEdges[] = {
        {0, 1}, // draw from right to left
        {1, 2},
        {2, 3},
        {3, 4},
        {4, 5},
    };
    static const bool kNodeEnabled[] = {
        false, // extension has no node dot
        true,
        true,
        true,
        true,
        true,
    };

    const int16_t margin = 1;
    const int16_t drawW = display->width() - (margin * 2) - 1;
    const int16_t drawH = display->height() - (margin * 2) - 1;

    struct Pt {
        int16_t x;
        int16_t y;
    };
    Pt pts[sizeof(kPoints) / sizeof(kPoints[0])];
    for (size_t i = 0; i < (sizeof(kPoints) / sizeof(kPoints[0])); ++i) {
        pts[i].x = x + margin + static_cast<int16_t>(kPoints[i].nx * drawW);
        pts[i].y = y + margin + static_cast<int16_t>(kPoints[i].ny * drawH);
    }

    // Center the main shape (nodes 1..N) within the display, then pin the extension to the right edge.
    int16_t minX = pts[1].x;
    int16_t maxX = pts[1].x;
    int16_t minY = pts[1].y;
    int16_t maxY = pts[1].y;
    for (size_t i = 2; i < (sizeof(pts) / sizeof(pts[0])); ++i) {
        minX = pts[i].x < minX ? pts[i].x : minX;
        maxX = pts[i].x > maxX ? pts[i].x : maxX;
        minY = pts[i].y < minY ? pts[i].y : minY;
        maxY = pts[i].y > maxY ? pts[i].y : maxY;
    }
    const int16_t bboxCx = static_cast<int16_t>((minX + maxX) / 2);
    const int16_t bboxCy = static_cast<int16_t>((minY + maxY) / 2);
    const int16_t targetCx = x + (display->width() / 2);
    const int16_t targetCy = y + (display->height() / 2);
    const int16_t dx = targetCx - bboxCx;
    const int16_t dy = targetCy - bboxCy;
    const int16_t clampLoX = x + 1;
    const int16_t clampHiX = x + display->width() - 2;
    const int16_t clampLoY = y + 1;
    const int16_t clampHiY = y + display->height() - 2;
    for (size_t i = 1; i < (sizeof(pts) / sizeof(pts[0])); ++i) {
        pts[i].x = clampI16(static_cast<int16_t>(pts[i].x + dx), clampLoX, clampHiX);
        pts[i].y = clampI16(static_cast<int16_t>(pts[i].y + dy), clampLoY, clampHiY);
    }
    pts[0].x = x + display->width() - 1;
    pts[0].y = pts[1].y;

    float progress = 0.0f;
    if (hermesXBootHoldReveal) {
        progress = 1.0f;
    } else if (hermesXBootHoldLongMs > 0) {
        progress = clamp01(static_cast<float>(hermesXBootHoldHeldMs) / static_cast<float>(hermesXBootHoldLongMs));
    }

    const int edgeCount = static_cast<int>(sizeof(kEdges) / sizeof(kEdges[0]));
    static constexpr float kSegmentStepPx = 1.0f;
    static constexpr int kMaxSegments = 512;

    struct BootHoldSegment {
        int16_t x0;
        int16_t y0;
        int16_t x1;
        int16_t y1;
        uint8_t startNode;
        uint8_t endNode;
        bool endsAtNode;
    };

    BootHoldSegment segments[kMaxSegments];
    int segmentCount = 0;
    for (int i = 0; i < edgeCount && segmentCount < kMaxSegments; ++i) {
        const BootHoldEdge &e = kEdges[i];
        const Pt &a = pts[e.a];
        const Pt &b = pts[e.b];
        const float len = edgeLength(a.x, a.y, b.x, b.y);
        if (len <= 0.0f) {
            continue;
        }
        const int steps = static_cast<int>(ceilf(len / kSegmentStepPx));
        const int safeSteps = steps > 0 ? steps : 1;
        for (int s = 0; s < safeSteps && segmentCount < kMaxSegments; ++s) {
            const float t0 = static_cast<float>(s) / static_cast<float>(safeSteps);
            const float t1 = static_cast<float>(s + 1) / static_cast<float>(safeSteps);
            BootHoldSegment &seg = segments[segmentCount++];
            seg.x0 = lerpI16(a.x, b.x, t0);
            seg.y0 = lerpI16(a.y, b.y, t0);
            seg.x1 = lerpI16(a.x, b.x, t1);
            seg.y1 = lerpI16(a.y, b.y, t1);
            seg.startNode = e.a;
            seg.endNode = e.b;
            seg.endsAtNode = (s + 1) == safeSteps;
        }
    }

    bool nodeVisible[sizeof(kPoints) / sizeof(kPoints[0])] = {false};
    if (segmentCount > 0 && progress > 0.0f) {
        const float segProgress = clamp01(progress) * static_cast<float>(segmentCount);
        int fullSegments = static_cast<int>(segProgress);
        if (fullSegments > segmentCount) {
            fullSegments = segmentCount;
        }
        float partialT = segProgress - static_cast<float>(fullSegments);
        if (fullSegments >= segmentCount) {
            partialT = 0.0f;
        }

        if (fullSegments > 0) {
            nodeVisible[segments[0].startNode] = true;
        }

        for (int i = 0; i < fullSegments; ++i) {
            const BootHoldSegment &seg = segments[i];
            const uint8_t thickness = (seg.startNode == 0) ? 1 : 2;
            drawLineThick(display, seg.x0, seg.y0, seg.x1, seg.y1, thickness);
            nodeVisible[seg.startNode] = true;
            if (seg.endsAtNode) {
                nodeVisible[seg.endNode] = true;
            }
        }

        if (partialT > 0.0f && fullSegments < segmentCount) {
            const BootHoldSegment &seg = segments[fullSegments];
            nodeVisible[seg.startNode] = true;
            const uint8_t thickness = (seg.startNode == 0) ? 1 : 2;
            drawBootHoldPartialLine(display, seg.x0, seg.y0, seg.x1, seg.y1, partialT, thickness,
                                    !hermesXBootHoldReveal);
        }
    }

    for (size_t i = 0; i < (sizeof(nodeVisible) / sizeof(nodeVisible[0])); ++i) {
        if (nodeVisible[i] && kNodeEnabled[i]) {
            display->drawCircle(pts[i].x, pts[i].y, 2);
        }
    }

    // Ensure the right node dot appears once any progress is visible.
    if ((progress > 0.0f) && kNodeEnabled[1]) {
        display->drawCircle(pts[1].x, pts[1].y, 2);
    }

    if (hermesXBootHoldReveal) {
        const int16_t textX = x + (display->width() - HERMESX_WORD_WIDTH) / 2;
        const int16_t textY = y + (display->height() - HERMESX_WORD_HEIGHT) / 2;
        display->drawXbm(textX, textY, HERMESX_WORD_WIDTH, HERMESX_WORD_HEIGHT, hermesx_word_bits);

        // HermesX build tag in bottom-right corner.
        const char *kHermesXBuildTag = "HXB_0.2.9";
        display->setFont(FONT_SMALL);
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        const int16_t tagWidth = display->getStringWidth(kHermesXBuildTag);
        const int16_t tagX = x + display->width() - tagWidth - 2;
        const int16_t tagY = y + display->height() - FONT_HEIGHT_SMALL - 2;
        display->drawString(tagX, tagY, kHermesXBuildTag);
    }
}

int32_t Screen::runOnce()
{
    // --- HermesX Remove TFT fast-path START
    static bool loggedMissingGlyphs = false;
    if (!loggedMissingGlyphs) {
        const uint32_t missing = HermesX_zh::drainMissingGlyphs();
        if (missing) {
            LOG_WARN("HermesX CN12 fallback glyphs used: %" PRIu32, missing);
            loggedMissingGlyphs = true;
        }
    }
    // --- HermesX Remove TFT fast-path END

    // If we don't have a screen, don't ever spend any CPU for us.
    if (!useDisplay) {
        enabled = false;
        return RUN_SAME;
    }

    if (displayHeight == 0) {
        displayHeight = dispdev->getHeight();
    }
    if (bootScreenStartMs == 0) {
        bootScreenStartMs = serialSinceMsec;
    }

    const bool gatePending = HermesXPowerGuard::guardEnabled() && HermesXPowerGuard::bootHoldPending();
    const bool deferNormalFrames = gatePending || (nodeDB == nullptr) || hermesXBootHoldActive;

    if (!stealthRestoreChecked && !deferNormalFrames) {
        logStealthStateProbe("pre-check");
        restoreStealthModeAfterBoot();
        recoverLegacyStealthCommsIfNeeded();
        logStealthStateProbe("post-check");
        stealthRestoreChecked = true;
    }

    if (isStealthModeActive() && cannedMessageModule) {
        const auto runState = cannedMessageModule->getRunState();
        if (runState != CANNED_MESSAGE_RUN_STATE_DISABLED && runState != CANNED_MESSAGE_RUN_STATE_INACTIVE) {
            cannedMessageModule->exitMenu();
        }
    }

    if (isStealthModeActive() && screenOn && stealthScreenWakeUntilMs != 0 && millis() >= stealthScreenWakeUntilMs) {
        handleSetOn(false);
        enabled = false;
        return RUN_SAME;
    }

    // Show boot screen for first logo_timeout seconds, then switch to normal operation.
    // serialSinceMsec adjusts for additional serial wait time during nRF52 bootup
    if (!deferNormalFrames && showingBootScreen && (millis() > (logo_timeout + bootScreenStartMs))) {
        LOG_INFO("Done with boot screen");
        stopBootScreen();
        showingBootScreen = false;
        bootScreenForceLogo = false;
    }

#ifdef USERPREFS_OEM_TEXT
    if (!deferNormalFrames && showingOEMBootScreen && (millis() > ((logo_timeout / 2) + bootScreenStartMs))) {
        LOG_INFO("Switch to OEM screen...");
        // Change frames.
        static FrameCallback bootOEMFrames[] = {drawOEMBootScreen};
        static const int bootOEMFrameCount = sizeof(bootOEMFrames) / sizeof(bootOEMFrames[0]);
        ui->setFrames(bootOEMFrames, bootOEMFrameCount);
        ui->update();
#ifndef USE_EINK
        ui->update();
#endif
        showingOEMBootScreen = false;
    }
#endif

#ifndef DISABLE_WELCOME_UNSET
    if (showingNormalScreen && config.lora.region == meshtastic_Config_LoRaConfig_RegionCode_UNSET) {
        setWelcomeFrames();
    }
#endif

    // Process incoming commands.
    for (;;) {
        ScreenCmd cmd;
        if (!cmdQueue.dequeue(&cmd, 0)) {
            break;
        }
        switch (cmd.cmd) {
        case Cmd::SET_ON:
            handleSetOn(true);
            break;
        case Cmd::SET_OFF:
            handleSetOn(false);
            break;
        case Cmd::ON_PRESS:
            handleOnPress();
            break;
        case Cmd::SHOW_PREV_FRAME:
            handleShowPrevFrame();
            break;
        case Cmd::SHOW_NEXT_FRAME:
            handleShowNextFrame();
            break;
        case Cmd::START_ALERT_FRAME: {
            showingBootScreen = false; // this should avoid the edge case where an alert triggers before the boot screen goes away
            showingNormalScreen = false;
            alertFrames[0] = alertFrame;
#ifdef USE_EINK
            EINK_ADD_FRAMEFLAG(dispdev, DEMAND_FAST); // Use fast-refresh for next frame, no skip please
            EINK_ADD_FRAMEFLAG(dispdev, BLOCKING);    // Edge case: if this frame is promoted to COSMETIC, wait for update
            handleSetOn(true); // Ensure power-on to receive deep-sleep screensaver (PowerFSM should handle?)
#endif
            setFrameImmediateDraw(alertFrames);
            break;
        }
        case Cmd::START_FIRMWARE_UPDATE_SCREEN:
            handleStartFirmwareUpdateScreen();
            break;
        case Cmd::STOP_ALERT_FRAME:
        case Cmd::STOP_BOOT_SCREEN:
            EINK_ADD_FRAMEFLAG(dispdev, COSMETIC); // E-Ink: Explicitly use full-refresh for next frame
            setFrames();
            break;
        case Cmd::PRINT:
            handlePrint(cmd.print_text);
            free(cmd.print_text);
            break;
        default:
            LOG_ERROR("Invalid screen cmd");
        }
    }

    if (hermesXBootHoldBootScreenPending && hermesXBootHoldBootScreenAtMs != 0 &&
        millis() >= hermesXBootHoldBootScreenAtMs) {
        hermesXBootHoldBootScreenPending = false;
        hermesXBootHoldBootScreenAtMs = 0;
        hermesXBootHoldActive = false;
        hermesXBootHoldReveal = false;
        hermesXBootHoldRevealUntilMs = 0;
        hermesXBootHoldAlertStarted = false;
        hermesXBootHoldHeldMs = 0;
        hermesXBootHoldLongMs = 1;
        bootScreenStartMs = millis();
        showingBootScreen = true;
        bootScreenForceLogo = true;
        bootScreenShowHermesWelcome = false;
#ifdef USERPREFS_OEM_TEXT
        showingOEMBootScreen = true;
#endif
        showingNormalScreen = false;
        setFrameImmediateDraw(bootScreenFrames);
    }

    if (!deferNormalFrames && pendingNormalFrames) {
        pendingNormalFrames = false;
        setFrames(gNormalFramesInitializedAfterBoot ? FOCUS_PRESERVE : FOCUS_DEFAULT);
        gNormalFramesInitializedAfterBoot = true;
    }

    if (gIncomingTextPopupState.pending && screenOn && isHermesXMainPageActive()) {
        gIncomingTextPopupState.pending = false;
        gIncomingTextPopupState.visible = true;
        gIncomingTextPopupState.untilMs = millis() + kIncomingTextPopupMs;
        fastUntilMs = gIncomingTextPopupState.untilMs + 50;
        setFastFramerate();
    } else if (gIncomingTextPopupState.visible && gIncomingTextPopupState.untilMs != 0 &&
               millis() >= gIncomingTextPopupState.untilMs) {
        dismissIncomingTextPopup();
        setFastFramerate();
    }

    if (gIncomingNodePopupState.pending && screenOn) {
        gIncomingNodePopupState.pending = false;
        gIncomingNodePopupState.visible = true;
        gIncomingNodePopupState.untilMs = millis() + kIncomingNodePopupMs;
        fastUntilMs = gIncomingNodePopupState.untilMs + 50;
        setFastFramerate();
    } else if (gIncomingNodePopupState.visible && gIncomingNodePopupState.untilMs != 0 &&
               millis() >= gIncomingNodePopupState.untilMs) {
        dismissIncomingNodePopup();
        setFastFramerate();
    }

    if (!lowMemoryReminderVisible && showingNormalScreen && screenOn && millis() >= lowMemoryReminderSuppressUntilMs) {
        const uint32_t freeHeap = memGet.getFreeHeap();
        const uint32_t largest = memGet.getLargestFreeBlock();
        if (freeHeap < kLowMemoryReminderFreeThreshold || largest < kLowMemoryReminderLargestThreshold) {
            lowMemoryReminderVisible = true;
            lowMemoryReminderSelected = 1;
            lowMemoryReminderTriggerFree = freeHeap;
            lowMemoryReminderTriggerLargest = largest;
            playLowMemoryAlert();
            LOG_WARN("[LowMemory] reminder trigger free=%u largest=%u", freeHeap, largest);
            setFastFramerate();
        }
    }

    if (!screenOn) { // If we didn't just wake and the screen is still off, then
                     // stop updating until it is on again
        enabled = false;
        return 0;
    }

    if (hermesXBootHoldActive && hermesXBootHoldReveal && hermesXBootHoldRevealUntilMs != 0 &&
        millis() >= hermesXBootHoldRevealUntilMs) {
        hermesXBootHoldRevealUntilMs = 0;
        hermesXBootHoldBootScreenPending = true;
        hermesXBootHoldBootScreenAtMs = millis();
    }

    bool skipUiUpdate = false;

#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
    const bool directNeonBuffersReady = ensureDirectNeonBuffers();
    bool renderDirectHomeClock = false;
    bool renderDirectHomeDogOverlay = false;
    bool forceDirectHomeClockRedraw = false;
    uint16_t directHomeDogFrame = 0xFFFF;
    char directHomeTimeBuf[16];
    directHomeTimeBuf[0] = '\0';
    const uint64_t uiLastUpdateBefore = ui->getUiState()->lastUpdate;

    if (isHermesFastSetupActive()) {
        hermesFastSetupTftPaletteActive = true;
    } else if (hermesFastSetupTftPaletteActive) {
        resetHermesFastSetupTftPalette(dispdev);
        hermesFastSetupTftPaletteActive = false;
    }

    auto *tft = static_cast<TFTDisplay *>(dispdev);
    const uint16_t normalFg = TFTDisplay::rgb565(0xFF, 0xFF, 0xFF);
    const uint16_t normalBg = TFTDisplay::rgb565(0x00, 0x00, 0x00);
    const uint16_t stealthFg = TFTDisplay::rgb565(0xFF, 0x20, 0x20);
    const uint8_t currentFrameIndex = ui->getUiState()->currentFrame;
    const bool frameIndexChanged = currentFrameIndex != gDirectLastFrameIndex;
    const bool isCurrentGpsFrame = showingNormalScreen && (framesetInfo.positions.settings < framesetInfo.frameCount) &&
                                   (currentFrameIndex == framesetInfo.positions.settings);
    const bool switchedToGpsFrame = frameIndexChanged && isCurrentGpsFrame;
    if (switchedToGpsFrame) {
        // Frame just switched to GPS: require one FIXED full repaint before any skip-ui optimization.
        gDirectGpsNeedsFullFrameAfterSwitch = true;
        gDirectGpsBasePainted = false;
        gDirectGpsPosterRenderCache.valid = false;
        gDirectGpsPosterWasVisible = false;
        tft->resetColorPalette(true);
        tft->markColorPaletteDirty();
    } else if (frameIndexChanged && !isCurrentGpsFrame) {
        gDirectGpsNeedsFullFrameAfterSwitch = false;
        gDirectGpsBasePainted = false;
    }
    const bool onFixedMainFrame = showingNormalScreen && ui->getUiState()->frameState == FIXED &&
                                  framesetInfo.positions.main < framesetInfo.frameCount &&
                                  ui->getUiState()->currentFrame == framesetInfo.positions.main;
    const bool onFixedGpsFrame = showingNormalScreen && ui->getUiState()->frameState == FIXED &&
                                 framesetInfo.positions.settings < framesetInfo.frameCount &&
                                 ui->getUiState()->currentFrame == framesetInfo.positions.settings;
    const bool directGpsPosterNeonEnabled =
        kEnableDirectGpsPosterTitleNeon || kEnableDirectGpsPosterDecorNeon || kEnableDirectGpsPosterCoordinateNeon;
    const bool incomingTextPopupActive = gIncomingTextPopupState.pending || gIncomingTextPopupState.visible;
    const bool incomingNodePopupActive = isIncomingNodePopupActive();
    const bool incomingNodePopupVisible = isIncomingNodePopupVisible();
    const bool directGpsPosterVisible = directNeonBuffersReady && directGpsPosterNeonEnabled && onFixedGpsFrame &&
                                        supportsDirectTftClockRendering(dispdev) && !incomingNodePopupActive;
    bool incomingNodePopupDirty = false;
    if (incomingNodePopupVisible && supportsDirectTftClockRendering(dispdev)) {
        incomingNodePopupDirty = !gDirectIncomingNodePopupRenderCache.valid ||
                                 gDirectIncomingNodePopupRenderCache.nodeNum != gIncomingNodePopupState.node.num ||
                                 gDirectIncomingNodePopupRenderCache.width != dispdev->getWidth() ||
                                 gDirectIncomingNodePopupRenderCache.height != dispdev->getHeight();
        if (!incomingNodePopupDirty) {
            skipUiUpdate = true;
        }
    } else if (!incomingNodePopupVisible) {
        gDirectIncomingNodePopupRenderCache.valid = false;
    }
    if (directNeonBuffersReady && onFixedMainFrame && canUseDirectHermesXHomeClock(dispdev) && !incomingTextPopupActive &&
        !incomingNodePopupActive) {
        const bool enteringDirectHomeOverlay = !gDirectHomeClockWasVisible && !gDirectHomeDogWasVisible;

        char homeDateBuf[24];
        homeDateBuf[0] = '\0';
        const bool directHomeHasValidTime =
            formatHermesXHomeTimeDate(directHomeTimeBuf, sizeof(directHomeTimeBuf), homeDateBuf, sizeof(homeDateBuf));
        renderDirectHomeClock = directHomeHasValidTime;
        renderDirectHomeDogOverlay = !directHomeHasValidTime;
        directHomeDogFrame = static_cast<uint16_t>(millis() / 110U);

        bool hasBattery = false;
        int batteryVoltageMv = 0;
        uint8_t batteryPercent = 0;
        getHermesXHomeBatteryState(hasBattery, batteryVoltageMv, batteryPercent);
        (void)batteryVoltageMv;

        uint8_t satCount = 0;
        if (gpsStatus && gpsStatus->getIsConnected()) {
            satCount = static_cast<uint8_t>(std::min<uint32_t>(99, gpsStatus->getNumSatellites()));
        }

        const bool stealth = isStealthModeActive();
        const uint32_t nowMs = millis();
        const bool telemetryChanged = !gHermesXDirectHomeUiCache.valid || gHermesXDirectHomeUiCache.hasBattery != hasBattery ||
                                      gHermesXDirectHomeUiCache.batteryPercent != batteryPercent ||
                                      gHermesXDirectHomeUiCache.satCount != satCount;
        const bool telemetryRefreshDue =
            (gDirectHomeLastBaseRefreshMs == 0) || (nowMs - gDirectHomeLastBaseRefreshMs >= kDirectHomeBaseRefreshMinMs);
        const bool telemetryDirty = telemetryChanged && (!gDirectHomeBasePainted || telemetryRefreshDue);
        const bool baseDirty = enteringDirectHomeOverlay || !gHermesXDirectHomeUiCache.valid ||
                               gHermesXDirectHomeUiCache.stealth != stealth ||
                               gHermesXDirectHomeUiCache.role != config.device.role ||
                               strcmp(gHermesXDirectHomeUiCache.date, homeDateBuf) != 0 || telemetryDirty;
        if (enteringDirectHomeOverlay || telemetryDirty || baseDirty) {
            LOG_DEBUG("[DirectHome] state enter=%d telemetryChanged=%d telemetryRefreshDue=%d telemetryDirty=%d baseDirty=%d "
                      "basePainted=%d meshPainted=%d skipUi=%d",
                      enteringDirectHomeOverlay ? 1 : 0, telemetryChanged ? 1 : 0, telemetryRefreshDue ? 1 : 0,
                      telemetryDirty ? 1 : 0, baseDirty ? 1 : 0, gDirectHomeBasePainted ? 1 : 0,
                      gDirectHomeMeshPainted ? 1 : 0, skipUiUpdate ? 1 : 0);
            logDirectHomeNeonSummary("DirectHome state update");
        }

        if (targetFramerate < DIRECT_HOME_CLOCK_FRAMERATE) {
            targetFramerate = DIRECT_HOME_CLOCK_FRAMERATE;
            ui->setTargetFPS(targetFramerate);
        }

        if (enteringDirectHomeOverlay) {
            // First frame when returning to Home must force a full UI-backed repaint, otherwise direct TFT clock can
            // sit on top of stale pixels from previous frame buffers.
            gHermesXDirectHomeUiCache.valid = false;
            gHermesXDirectHomeUiCache.lastTimeValid = false;
            gHermesXDirectHomeUiCache.lastDogValid = false;
            gHermesXDirectHomeUiCache.lastDogFrame = 0xFFFF;
            gHermesXDirectHomeUiCache.lastDogPose = 0xFF;
            gHermesXDirectHomeUiCache.lastDogX = -1;
            gHermesXDirectHomeUiCache.lastDogY = -1;
            gHermesXDirectHomeUiCache.lastDogW = 0;
            gHermesXDirectHomeUiCache.lastDogH = 0;
            gDirectHomeBasePainted = false;
            gDirectHomeMeshPainted = false;
            gDirectHomeOrbLastPulsePercent = 0xFF;
            gDirectHomeOrbLastDrawMs = 0;
            forceDirectHomeClockRedraw = true;
            if (renderDirectHomeDogOverlay) {
                gDirectHomeDogPose = static_cast<uint8_t>(gDirectHomeDogEntryCount % 2U);
                ++gDirectHomeDogEntryCount;
            }
            skipUiUpdate = false;
            ui->init();
            tft->markColorPaletteDirty();
            logDirectHomeNeonSummary("DirectHome entering");
        }

        const bool canSkipUi = gDirectHomeBasePainted && !baseDirty && !gIncomingTextPopupState.pending && !gIncomingTextPopupState.visible;
        if (canSkipUi) {
            skipUiUpdate = true;
        } else {
            gHermesXDirectHomeUiCache.valid = true;
            gHermesXDirectHomeUiCache.stealth = stealth;
            gHermesXDirectHomeUiCache.hasBattery = hasBattery;
            gHermesXDirectHomeUiCache.batteryPercent = batteryPercent;
            gHermesXDirectHomeUiCache.satCount = satCount;
            gHermesXDirectHomeUiCache.role = config.device.role;
            strlcpy(gHermesXDirectHomeUiCache.date, homeDateBuf, sizeof(gHermesXDirectHomeUiCache.date));
            gDirectHomeLastBaseRefreshMs = nowMs;
            forceDirectHomeClockRedraw = true;
        }
    } else {
        gHermesXDirectHomeUiCache.valid = false;
        gHermesXDirectHomeUiCache.lastTimeValid = false;
        gHermesXDirectHomeUiCache.lastDogValid = false;
        gHermesXDirectHomeUiCache.lastDogFrame = 0xFFFF;
        gHermesXDirectHomeUiCache.lastDogPose = 0xFF;
        gHermesXDirectHomeUiCache.lastDogX = -1;
        gHermesXDirectHomeUiCache.lastDogY = -1;
        gHermesXDirectHomeUiCache.lastDogW = 0;
        gHermesXDirectHomeUiCache.lastDogH = 0;
        gDirectHomeBasePainted = false;
        gDirectHomeMeshPainted = false;
        gDirectHomeOrbLastPulsePercent = 0xFF;
        gDirectHomeOrbLastDrawMs = 0;
        gDirectHomeLastBaseRefreshMs = 0;
    }

    const bool leftDirectHomeOverlay =
        (gDirectHomeClockWasVisible && !renderDirectHomeClock) || (gDirectHomeDogWasVisible && !renderDirectHomeDogOverlay);
    if (leftDirectHomeOverlay) {
        skipUiUpdate = false;
        clearDirectNeonClockRegion(tft, dispdev->getWidth(), 0);
        gDirectHomeBasePainted = false;
        gDirectHomeMeshPainted = false;
        gHermesXDirectHomeUiCache.lastDogValid = false;
        gHermesXDirectHomeUiCache.lastDogFrame = 0xFFFF;
        gHermesXDirectHomeUiCache.lastDogPose = 0xFF;
        gHermesXDirectHomeUiCache.lastDogX = -1;
        gHermesXDirectHomeUiCache.lastDogY = -1;
        gHermesXDirectHomeUiCache.lastDogW = 0;
        gHermesXDirectHomeUiCache.lastDogH = 0;
        gDirectHomeOrbLastPulsePercent = 0xFF;
        gDirectHomeOrbLastDrawMs = 0;
        gDirectHomeLastBaseRefreshMs = 0;
        tft->resetColorPalette(false);
        tft->setColorPaletteDefaults(isStealthModeActive() ? stealthFg : normalFg, normalBg);
        ui->init();
        // Direct TFT clock bypasses the OLED diff buffers; force one full TFT repaint after leaving Home.
        tft->markColorPaletteDirty();
    }

    const bool leftIncomingNodePopup = gIncomingNodePopupWasVisible && !isIncomingNodePopupVisible();
    if (leftIncomingNodePopup) {
        skipUiUpdate = false;
        gDirectIncomingNodePopupRenderCache.valid = false;
        gHermesXDirectHomeUiCache.valid = false;
        gHermesXDirectHomeUiCache.lastTimeValid = false;
        gDirectHomeBasePainted = false;
        gDirectHomeMeshPainted = false;
        gDirectGpsPosterRenderCache.valid = false;
        gDirectGpsBasePainted = false;
        ui->init();
        tft->markColorPaletteDirty();
    }

    const bool enteredDirectGpsPoster = directGpsPosterVisible && !gDirectGpsPosterWasVisible;
    const bool leftDirectGpsPoster = gDirectGpsPosterWasVisible && !directGpsPosterVisible;
    if (enteredDirectGpsPoster || leftDirectGpsPoster) {
        // Mirror Home page behavior: only invalidate palette regions on GPS page transitions.
        tft->resetColorPalette(false);
        tft->setColorPaletteDefaults(isStealthModeActive() ? stealthFg : normalFg, normalBg);
        tft->markColorPaletteDirty();
    }
    if (enteredDirectGpsPoster) {
        // Match Home direct-TFT entry behavior: ensure one full UI-backed base redraw before any neon overlay.
        gDirectGpsBasePainted = false;
        gDirectGpsPosterRenderCache.valid = false;
        skipUiUpdate = false;
        ui->init();
        tft->fillRect565(0, 0, dispdev->getWidth(), dispdev->getHeight(), normalBg);
        tft->markColorPaletteDirty();
    }
    if (leftDirectGpsPoster) {
        gDirectGpsBasePainted = false;
        gDirectGpsPosterRenderCache.valid = false;
    }
    if (directGpsPosterVisible && !skipUiUpdate) {
        const int16_t renderW = dispdev->getWidth();
        const int16_t renderH = dispdev->getHeight();
        const bool gpsEnabled = config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_ENABLED;
        bool gpsConnected = false;
        bool gpsHasLock = false;
        uint8_t satCount = 0;
        bool fixedPosition = false;
        bool hasCoordinates = false;
        int32_t latitudeE7 = INT32_MIN;
        int32_t longitudeE7 = INT32_MIN;
        int32_t altitude = INT32_MIN;
        if (kEnableDirectGpsPosterDecorNeon && gpsStatus && gpsStatus->getIsConnected()) {
            satCount = static_cast<uint8_t>(std::min<uint32_t>(99, gpsStatus->getNumSatellites()));
        }
        if (kEnableDirectGpsPosterCoordinateNeon) {
            gpsConnected = gpsStatus && gpsStatus->getIsConnected();
            gpsHasLock = gpsStatus && gpsStatus->getHasLock();
            fixedPosition = config.position.fixed_position;
            hasCoordinates = gpsStatus && gpsEnabled && (fixedPosition || (gpsConnected && gpsHasLock));
            latitudeE7 = hasCoordinates ? gpsStatus->getLatitude() : INT32_MIN;
            longitudeE7 = hasCoordinates ? gpsStatus->getLongitude() : INT32_MIN;
            altitude = hasCoordinates ? gpsStatus->getAltitude() : INT32_MIN;
        }

        const bool gpsPosterDirty = enteredDirectGpsPoster || !gDirectGpsPosterRenderCache.valid ||
                                    gDirectGpsPosterRenderCache.width != renderW || gDirectGpsPosterRenderCache.height != renderH ||
                                    (kEnableDirectGpsPosterTitleNeon && gDirectGpsPosterRenderCache.gpsEnabled != gpsEnabled) ||
                                    (kEnableDirectGpsPosterDecorNeon && gDirectGpsPosterRenderCache.satCount != satCount) ||
                                    (kEnableDirectGpsPosterCoordinateNeon &&
                                     (gDirectGpsPosterRenderCache.gpsConnected != gpsConnected ||
                                      gDirectGpsPosterRenderCache.gpsHasLock != gpsHasLock ||
                                      gDirectGpsPosterRenderCache.fixedPosition != fixedPosition ||
                                      gDirectGpsPosterRenderCache.hasCoordinates != hasCoordinates ||
                                      gDirectGpsPosterRenderCache.latitudeE7 != latitudeE7 ||
                                      gDirectGpsPosterRenderCache.longitudeE7 != longitudeE7 ||
                                      gDirectGpsPosterRenderCache.altitude != altitude));
        const bool canSkipGpsUi = gDirectGpsBasePainted && gDirectGpsPosterRenderCache.valid && !gpsPosterDirty &&
                                  !gIncomingTextPopupState.pending && !gIncomingTextPopupState.visible &&
                                  !gDirectGpsNeedsFullFrameAfterSwitch;
        if (canSkipGpsUi) {
            skipUiUpdate = true;
        }
    }

    if (!skipUiUpdate) {
        tft->clearColorPaletteZones();
        tft->setColorPaletteDefaults(isStealthModeActive() ? stealthFg : normalFg, normalBg);
    }
#endif

    // this must be before the frameState == FIXED check, because we always
    // want to draw at least one FIXED frame before doing forceDisplay
    if (!skipUiUpdate) {
        ui->update();
    }

#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
    const bool uiRenderedThisTick = !skipUiUpdate && ui->getUiState()->lastUpdate != uiLastUpdateBefore;
    if (onFixedGpsFrame && uiRenderedThisTick) {
        gDirectGpsNeedsFullFrameAfterSwitch = false;
        gDirectGpsBasePainted = true;
    } else if (!onFixedGpsFrame) {
        gDirectGpsBasePainted = false;
    }
    if (renderDirectHomeClock && kEnableDirectHomeOrbAnimation) {
        static constexpr uint32_t kOrbPulseCycleMs = 5000U;
        const uint32_t nowMs = millis();
        const uint32_t cyclePhaseMs = nowMs % kOrbPulseCycleMs;
        uint8_t pulsePercent = 100;
        if (cyclePhaseMs <= (kOrbPulseCycleMs / 2U)) {
            pulsePercent = static_cast<uint8_t>(100U + static_cast<uint8_t>((30U * cyclePhaseMs + 1250U) / 2500U));
        } else {
            const uint32_t downPhase = cyclePhaseMs - (kOrbPulseCycleMs / 2U);
            pulsePercent = static_cast<uint8_t>(130U - static_cast<uint8_t>((30U * downPhase + 1250U) / 2500U));
        }
        // Quantize to 1% steps for smoother visible motion.
        if (pulsePercent < 100U) {
            pulsePercent = 100U;
        } else if (pulsePercent > 130U) {
            pulsePercent = 130U;
        }
        pulsePercent = static_cast<uint8_t>(100U + (pulsePercent - 100U));
        const bool repaintHomeOrbBase = !gDirectHomeMeshPainted || uiRenderedThisTick;
        const bool pulsePhaseChanged = gDirectHomeOrbLastPulsePercent != pulsePercent;
        const bool pulseFrameDue =
            (gDirectHomeOrbLastDrawMs == 0) || ((nowMs - gDirectHomeOrbLastDrawMs) >= kDirectHomeOrbMinFrameIntervalMs);
        if (repaintHomeOrbBase) {
            renderDirectHomeOrangeMesh(tft, dispdev->getWidth(), dispdev->getHeight(), nullptr, false, pulsePercent);
            // Re-stamp only the orb box so clock/date/role/battery areas are not repeatedly full-screen overdrawn.
            int16_t orbX = 0;
            int16_t orbY = 0;
            int16_t orbW = 0;
            int16_t orbH = 0;
            if (getDirectHomeOrbBoundsRect(dispdev->getWidth(), dispdev->getHeight(), orbX, orbY, orbW, orbH)) {
                tft->overlayBufferForegroundRect565(orbX, orbY, orbW, orbH);
            } else {
                tft->overlayBufferForeground565();
            }
            gDirectHomeMeshPainted = true;
            gDirectHomeOrbLastPulsePercent = pulsePercent;
            gDirectHomeOrbLastDrawMs = nowMs;
        } else if (pulsePhaseChanged && pulseFrameDue) {
            int16_t orbX = 0;
            int16_t orbY = 0;
            int16_t orbW = 0;
            int16_t orbH = 0;
            DirectDrawClipRect orbClip;
            if (getDirectHomeOrbBoundsRect(dispdev->getWidth(), dispdev->getHeight(), orbX, orbY, orbW, orbH) &&
                makeDirectDrawClipRect(orbX, orbY, orbW, orbH, dispdev->getWidth(), dispdev->getHeight(), orbClip)) {
                renderDirectHomeOrangeMesh(tft, dispdev->getWidth(), dispdev->getHeight(), &orbClip, true, pulsePercent);
                tft->overlayBufferForegroundRect565(orbX, orbY, orbW, orbH);
            }
            gDirectHomeOrbLastPulsePercent = pulsePercent;
            gDirectHomeOrbLastDrawMs = nowMs;
        }
        if (uiRenderedThisTick) {
            gDirectHomeBasePainted = true;
        }
    } else if (renderDirectHomeClock && uiRenderedThisTick) {
        gDirectHomeBasePainted = true;
    }
    if (renderDirectHomeClock) {
        if (forceDirectHomeClockRedraw || !gHermesXDirectHomeUiCache.lastTimeValid ||
            strcmp(gHermesXDirectHomeUiCache.lastTime, directHomeTimeBuf) != 0) {
            const bool repaintClockBackground = repaintDirectHomeClockMeshRegion(tft, dispdev->getWidth(), dispdev->getHeight(), 0);
            LOG_DEBUG("[DirectHome] redraw time='%s' force=%d repaintClockBackground=%d lastTimeValid=%d", directHomeTimeBuf,
                      forceDirectHomeClockRedraw ? 1 : 0, repaintClockBackground ? 1 : 0,
                      gHermesXDirectHomeUiCache.lastTimeValid ? 1 : 0);
            renderDirectNeonClock(tft,
                                  dispdev->getWidth(),
                                  0,
                                  directHomeTimeBuf,
                                  forceDirectHomeClockRedraw || repaintClockBackground,
                                  !repaintClockBackground);
            strlcpy(gHermesXDirectHomeUiCache.lastTime, directHomeTimeBuf, sizeof(gHermesXDirectHomeUiCache.lastTime));
            gHermesXDirectHomeUiCache.lastTimeValid = true;
            logDirectHomeNeonSummary("DirectHome after redraw");
        }
    } else if (renderDirectHomeDogOverlay) {
        if (forceDirectHomeClockRedraw || !gHermesXDirectHomeUiCache.lastDogValid ||
            gHermesXDirectHomeUiCache.lastDogFrame != directHomeDogFrame ||
            gHermesXDirectHomeUiCache.lastDogPose != gDirectHomeDogPose) {
            const bool repaintDogBackground = repaintDirectHomeClockMeshRegion(tft, dispdev->getWidth(), dispdev->getHeight(), 0);
            renderDirectHomeDog(tft, dispdev->getWidth(), dispdev->getHeight(), 0, directHomeDogFrame, gDirectHomeDogPose);
            gHermesXDirectHomeUiCache.lastDogValid = true;
            gHermesXDirectHomeUiCache.lastDogFrame = directHomeDogFrame;
            gHermesXDirectHomeUiCache.lastDogPose = gDirectHomeDogPose;
            gHermesXDirectHomeUiCache.lastTimeValid = false;
            if (repaintDogBackground) {
                logDirectHomeNeonSummary("DirectHome dog redraw");
            }
        }
    }
    if (directGpsPosterVisible) {
        const int16_t renderW = dispdev->getWidth();
        const int16_t renderH = dispdev->getHeight();
        const bool gpsEnabled = config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_ENABLED;
        bool gpsConnected = false;
        bool gpsHasLock = false;
        uint8_t satCount = 0;
        bool fixedPosition = false;
        bool hasCoordinates = false;
        int32_t latitudeE7 = INT32_MIN;
        int32_t longitudeE7 = INT32_MIN;
        int32_t altitude = INT32_MIN;
        if (kEnableDirectGpsPosterDecorNeon && gpsStatus && gpsStatus->getIsConnected()) {
            satCount = static_cast<uint8_t>(std::min<uint32_t>(99, gpsStatus->getNumSatellites()));
        }
        if (kEnableDirectGpsPosterCoordinateNeon) {
            gpsConnected = gpsStatus && gpsStatus->getIsConnected();
            gpsHasLock = gpsStatus && gpsStatus->getHasLock();
            fixedPosition = config.position.fixed_position;
            hasCoordinates = gpsStatus && gpsEnabled && (fixedPosition || (gpsConnected && gpsHasLock));
            latitudeE7 = hasCoordinates ? gpsStatus->getLatitude() : INT32_MIN;
            longitudeE7 = hasCoordinates ? gpsStatus->getLongitude() : INT32_MIN;
            altitude = hasCoordinates ? gpsStatus->getAltitude() : INT32_MIN;
        }

        const bool gpsPosterDirty = enteredDirectGpsPoster || !gDirectGpsPosterRenderCache.valid ||
                                    gDirectGpsPosterRenderCache.width != renderW || gDirectGpsPosterRenderCache.height != renderH ||
                                    (kEnableDirectGpsPosterTitleNeon && gDirectGpsPosterRenderCache.gpsEnabled != gpsEnabled) ||
                                    (kEnableDirectGpsPosterDecorNeon && gDirectGpsPosterRenderCache.satCount != satCount) ||
                                    (kEnableDirectGpsPosterCoordinateNeon &&
                                     (gDirectGpsPosterRenderCache.gpsConnected != gpsConnected ||
                                      gDirectGpsPosterRenderCache.gpsHasLock != gpsHasLock ||
                                      gDirectGpsPosterRenderCache.fixedPosition != fixedPosition ||
                                      gDirectGpsPosterRenderCache.hasCoordinates != hasCoordinates ||
                                      gDirectGpsPosterRenderCache.latitudeE7 != latitudeE7 ||
                                      gDirectGpsPosterRenderCache.longitudeE7 != longitudeE7 ||
                                      gDirectGpsPosterRenderCache.altitude != altitude));
        const bool gpsPosterNeedsRepaint = gDirectGpsBasePainted && (gpsPosterDirty || uiRenderedThisTick);
        if (gpsPosterNeedsRepaint) {
            // GPS base frame is still drawn by ui->update(); re-apply enabled direct layers after any UI repaint.
            if (kEnableDirectGpsPosterDecorNeon) {
                renderDirectGpsPosterDecor(tft, renderW, renderH, gpsStatus);
            }
            if (kEnableDirectGpsPosterTitleNeon) {
                renderDirectGpsPosterTitleNeon(tft, renderW, renderH);
            }
            if (kEnableDirectGpsPosterCoordinateNeon) {
                renderDirectGpsPosterCoordinateNeon(tft, renderW, renderH, gpsStatus);
            }
            gDirectGpsPosterRenderCache.valid = true;
            gDirectGpsPosterRenderCache.gpsEnabled = gpsEnabled;
            gDirectGpsPosterRenderCache.gpsConnected = gpsConnected;
            gDirectGpsPosterRenderCache.gpsHasLock = gpsHasLock;
            gDirectGpsPosterRenderCache.satCount = satCount;
            gDirectGpsPosterRenderCache.fixedPosition = fixedPosition;
            gDirectGpsPosterRenderCache.hasCoordinates = hasCoordinates;
            gDirectGpsPosterRenderCache.latitudeE7 = latitudeE7;
            gDirectGpsPosterRenderCache.longitudeE7 = longitudeE7;
            gDirectGpsPosterRenderCache.altitude = altitude;
            gDirectGpsPosterRenderCache.width = renderW;
            gDirectGpsPosterRenderCache.height = renderH;
        }
    } else {
        gDirectGpsBasePainted = false;
    }
    if (incomingNodePopupVisible && (incomingNodePopupDirty || uiRenderedThisTick)) {
        renderDirectIncomingNodePopup(tft, dispdev->getWidth(), dispdev->getHeight(), gIncomingNodePopupState.node);
        gDirectIncomingNodePopupRenderCache.valid = true;
        gDirectIncomingNodePopupRenderCache.nodeNum = gIncomingNodePopupState.node.num;
        gDirectIncomingNodePopupRenderCache.width = dispdev->getWidth();
        gDirectIncomingNodePopupRenderCache.height = dispdev->getHeight();
    }
    gDirectHomeClockWasVisible = renderDirectHomeClock;
    gDirectHomeDogWasVisible = renderDirectHomeDogOverlay;
    gDirectGpsPosterWasVisible = directGpsPosterVisible;
    gIncomingNodePopupWasVisible = incomingNodePopupVisible;
    gDirectLastFrameIndex = currentFrameIndex;
#endif

    if (showingNormalScreen && hasUnreadTextMessage && hasRecentTextMessages()) {
        const uint8_t currentFrame = ui->getUiState()->currentFrame;
        if ((framesetInfo.positions.textMessageList < framesetInfo.frameCount && currentFrame == framesetInfo.positions.textMessageList) ||
            (framesetInfo.positions.textMessage < framesetInfo.frameCount && currentFrame == framesetInfo.positions.textMessage)) {
            hasUnreadTextMessage = false;
            syncTextMessageNotification();
        }
    }

    // Switch to a low framerate (to save CPU) when we are not in transition
    // but we should only call setTargetFPS when framestate changes, because
    // otherwise that breaks animations.
    if (targetFramerate != IDLE_FRAMERATE && ui->getUiState()->frameState == FIXED && !hermesXBootHoldActive &&
        !gDirectHomeClockWasVisible) {
        if (isHermesFastSetupActive() || (hermesXEmUiModule && hermesXEmUiModule->isActive())) {
            // Keep UI responsive while interacting with HermesX screens.
            fastUntilMs = millis() + 1200;
        } else if (fastUntilMs == 0 || millis() >= fastUntilMs) {
            // oldFrameState = ui->getUiState()->frameState;
            targetFramerate = IDLE_FRAMERATE;
            ui->setTargetFPS(targetFramerate);
            forceDisplay();
        }
    }

    // While showing the bootscreen or Bluetooth pair screen all of our
    // standard screen switching is stopped.
    if (showingNormalScreen) {
        // standard screen loop handling here
        if (config.display.auto_screen_carousel_secs > 0 &&
            !Throttle::isWithinTimespanMs(lastScreenTransition, config.display.auto_screen_carousel_secs * 1000)) {

// If an E-Ink display struggles with fast refresh, force carousel to use full refresh instead
// Carousel is potentially a major source of E-Ink display wear
#if !defined(EINK_BACKGROUND_USES_FAST)
            EINK_ADD_FRAMEFLAG(dispdev, COSMETIC);
#endif

            LOG_DEBUG("LastScreenTransition exceeded %ums transition to next frame", (millis() - lastScreenTransition));
            handleOnPress();
        }
    }

    // LOG_DEBUG("want fps %d, fixed=%d", targetFramerate,
    // ui->getUiState()->frameState); If we are scrolling we need to be called
    // soon, otherwise just 1 fps (to save CPU) We also ask to be called twice
    // as fast as we really need so that any rounding errors still result with
    // the correct framerate
    return (1000 / targetFramerate);
}

void Screen::setBootHoldProgress(uint32_t heldMs, uint32_t longPressMs)
{
    hermesXBootHoldActive = true;
    hermesXBootHoldReveal = false;
    hermesXBootHoldRevealUntilMs = 0;
    hermesXBootHoldHeldMs = heldMs;
    hermesXBootHoldLongMs = longPressMs ? longPressMs : 1;
    if (!hermesXBootHoldAlertStarted) {
        hermesXBootHoldAlertStarted = true;
        startAlert(drawHermesXBootHoldFrame);
    }
    setFastFramerate();
}

void Screen::startHermesXAlert(const char *text)
{
    if (!text || !*text) {
        return;
    }
#if defined(HERMESX_TEST_DISABLE_HERMES_ALERTS)
    startAlert(text);
#else
    startAlert([text](OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) -> void {
        (void)state;
        const int lineHeight = HermesX_zh::GLYPH_HEIGHT + 1;
        const int width = HermesX_zh::stringAdvance(text, HermesX_zh::GLYPH_WIDTH, display);
        const int16_t drawX = x + (display->width() - width) / 2;
        const int16_t drawY = y + (display->height() - lineHeight) / 2;
        HermesX_zh::drawMixed(*display, drawX, drawY, text, HermesX_zh::GLYPH_WIDTH, lineHeight, nullptr);
    });
#endif
    // Ensure the alert is visible immediately (avoid 1fps idle delays).
    setFastFramerate();
}

void Screen::startBootHoldReveal(uint32_t revealMs)
{
    hermesXBootHoldActive = true;
    hermesXBootHoldReveal = true;
    hermesXBootHoldHeldMs = hermesXBootHoldLongMs;
    const uint32_t duration = revealMs ? revealMs : 1;
    hermesXBootHoldRevealUntilMs = millis() + duration;
    if (!hermesXBootHoldAlertStarted) {
        hermesXBootHoldAlertStarted = true;
        startAlert(drawHermesXBootHoldFrame);
    }
    setFastFramerate();
}

void Screen::resetBootHoldProgress()
{
    hermesXBootHoldActive = true;
    hermesXBootHoldReveal = false;
    hermesXBootHoldRevealUntilMs = 0;
    hermesXBootHoldHeldMs = 0;
    hermesXBootHoldLongMs = hermesXBootHoldLongMs ? hermesXBootHoldLongMs : 1;
    if (!hermesXBootHoldAlertStarted) {
        hermesXBootHoldAlertStarted = true;
        startAlert(drawHermesXBootHoldFrame);
    }
    setFastFramerate();
}

void Screen::drawDebugInfoTrampoline(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    Screen *screen2 = reinterpret_cast<Screen *>(state->userData);
    screen2->debugInfo.drawFrame(display, state, x, y);
}

void Screen::drawDebugInfoSettingsTrampoline(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    Screen *screen2 = reinterpret_cast<Screen *>(state->userData);
    screen2->debugInfo.drawFrameSettings(display, state, x, y);
}

void Screen::drawDebugInfoWiFiTrampoline(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    Screen *screen2 = reinterpret_cast<Screen *>(state->userData);
    screen2->debugInfo.drawFrameWiFi(display, state, x, y);
}

/* show a message that the SSL cert is being built
 * it is expected that this will be used during the boot phase */
void Screen::setSSLFrames()
{
    if (address_found.address) {
        // LOG_DEBUG("Show SSL frames");
        static FrameCallback sslFrames[] = {drawSSLScreen};
        ui->setFrames(sslFrames, 1);
        ui->update();
    }
}

/* show a message that the SSL cert is being built
 * it is expected that this will be used during the boot phase */
void Screen::setWelcomeFrames()
{
    if (address_found.address) {
        // LOG_DEBUG("Show Welcome frames");
        static FrameCallback frames[] = {drawWelcomeScreen};
        setFrameImmediateDraw(frames);
    }
}

#ifdef USE_EINK
/// Determine which screensaver frame to use, then set the FrameCallback
void Screen::setScreensaverFrames(FrameCallback einkScreensaver)
{
    // Retain specified frame / overlay callback beyond scope of this method
    static FrameCallback screensaverFrame;
    static OverlayCallback screensaverOverlay;

#if defined(HAS_EINK_ASYNCFULL) && defined(USE_EINK_DYNAMICDISPLAY)
    // Join (await) a currently running async refresh, then run the post-update code.
    // Avoid skipping of screensaver frame. Would otherwise be handled by NotifiedWorkerThread.
    EINK_JOIN_ASYNCREFRESH(dispdev);
#endif

    // If: one-off screensaver frame passed as argument. Handles doDeepSleep()
    if (einkScreensaver != NULL) {
        screensaverFrame = einkScreensaver;
        ui->setFrames(&screensaverFrame, 1);
    }

    // Else, display the usual "overlay" screensaver
    else {
        screensaverOverlay = drawScreensaverOverlay;
        ui->setOverlays(&screensaverOverlay, 1);
    }

    // Request new frame, ASAP
    setFastFramerate();
    uint64_t startUpdate;
    do {
        startUpdate = millis(); // Handle impossibly unlikely corner case of a millis() overflow..
        delay(1);
        ui->update();
    } while (ui->getUiState()->lastUpdate < startUpdate);

    // Old EInkDisplay class
#if !defined(USE_EINK_DYNAMICDISPLAY)
    static_cast<EInkDisplay *>(dispdev)->forceDisplay(0); // Screen::forceDisplay(), but override rate-limit
#endif

    // Prepare now for next frame, shown when display wakes
    ui->setOverlays(NULL, 0);  // Clear overlay
    setFrames(FOCUS_PRESERVE); // Return to normal display updates, showing same frame as before screensaver, ideally

    // Pick a refresh method, for when display wakes
#ifdef EINK_HASQUIRK_GHOSTING
    EINK_ADD_FRAMEFLAG(dispdev, COSMETIC); // Really ugly to see ghosting from "screen paused"
#else
    EINK_ADD_FRAMEFLAG(dispdev, RESPONSIVE); // Really nice to wake screen with a fast-refresh
#endif
}
#endif

// Regenerate the normal set of frames, focusing a specific frame if requested
// Called when a frame should be added / removed, or custom frames should be cleared
void Screen::setFrames(FrameFocus focus)
{
    if (HermesXPowerGuard::guardEnabled() && HermesXPowerGuard::bootHoldPending()) {
        pendingNormalFrames = true;
        return;
    }
    if (nodeDB == nullptr) {
        pendingNormalFrames = true;
        return;
    }
    if (hermesXBootHoldActive) {
        pendingNormalFrames = true;
        return;
    }

    uint8_t originalPosition = ui->getUiState()->currentFrame;
    FramesetInfo fsi; // Location of specific frames, for applying focus parameter
    fsi.positions.main = 0xFF;
    fsi.positions.mainAction = 0xFF;
    fsi.positions.setup = 0xFF;
    fsi.positions.share = 0xFF;

    LOG_DEBUG("Show standard frames");
    showingNormalScreen = true;

#ifdef USE_EINK
    // If user has disabled the screensaver, warn them after boot
    static bool warnedScreensaverDisabled = false;
    if (config.display.screen_on_secs == 0 && !warnedScreensaverDisabled) {
        screen->print("Screensaver disabled\n");
        warnedScreensaverDisabled = true;
    }
#endif

    moduleFrames = MeshModule::GetMeshModulesWithUIFrames();
    LOG_DEBUG("Show %d module frames", moduleFrames.size());
#ifdef DEBUG_PORT
    int totalFrameCount = MAX_NUM_NODES + NUM_EXTRA_FRAMES + moduleFrames.size();
    LOG_DEBUG("Total frame count: %d", totalFrameCount);
#endif

    size_t numframes = 0;

    // put all of the module frames first.
    // this is a little bit of a dirty hack; since we're going to call
    // the same drawModuleFrame handler here for all of these module frames
    // and then we'll just assume that the state->currentFrame value
    // is the same offset into the moduleFrames vector
    // so that we can invoke the module's callback
    for (auto i = moduleFrames.begin(); i != moduleFrames.end(); ++i) {
        // Draw the module frame, using the hack described above
        normalFrames[numframes] = drawModuleFrame;

        // Check if the module being drawn has requested focus
        // We will honor this request later, if setFrames was triggered by a UIFrameEvent
        MeshModule *m = *i;
        if (m->isRequestingFocus()) {
            fsi.positions.focusedModule = numframes;
        }

        // Identify the position of specific modules, if we need to know this later
        if (m == waypointModule)
            fsi.positions.waypoint = numframes;

        numframes++;
    }

    LOG_DEBUG("Added modules.  numframes: %d", numframes);

    // If we have a critical fault, show it first
    fsi.positions.fault = numframes;
    if (error_code) {
        normalFrames[numframes++] = drawCriticalFaultFrame;
        focus = FOCUS_FAULT; // Change our "focus" parameter, to ensure we show the fault frame
    }

#if defined(DISPLAY_CLOCK_FRAME)
    normalFrames[numframes++] = screen->digitalWatchFace ? &Screen::drawDigitalClockFrame : &Screen::drawAnalogClockFrame;
#endif

    // Recent received text messages list is always available from the action menu.
    fsi.positions.textMessageList = numframes;
    normalFrames[numframes++] = drawRecentTextMessagesFrame;

    // The detail page remains conditional on having at least one message.
    if (hasRecentTextMessages()) {
        fsi.positions.textMessage = numframes;
        normalFrames[numframes++] = drawTextMessageFrame;
    }

#if !defined(HERMESX_TEST_DISABLE_HERMES_PAGES)
    if (shouldShowHermesXHomeFrame()) {
        // Main screen (HermesX logo / Home)
        fsi.positions.main = numframes;
        normalFrames[numframes++] = &Screen::drawHermesXMainFrame;
    }

    // Main action menu (standalone page)
    fsi.positions.mainAction = numframes;
    normalFrames[numframes++] = &Screen::drawHermesXActionFrame;

    // HermesFastSetup replaces node info frames
    fsi.positions.setup = numframes;
    normalFrames[numframes++] = &Screen::drawHermesFastSetupFrame;

    // Share channels (QR)
    fsi.positions.share = numframes;
    normalFrames[numframes++] = &Screen::drawHermesXShareChannelFrame;
#endif

    // then the debug info
    //
    // Since frames are basic function pointers, we have to use a helper to
    // call a method on debugInfo object.
    fsi.positions.log = numframes;
    normalFrames[numframes++] = &Screen::drawDebugInfoTrampoline;

    if (shouldShowHermesXGpsFrame()) {
        // GPS hero/settings frame
        fsi.positions.settings = numframes;
        normalFrames[numframes++] = &Screen::drawDebugInfoSettingsTrampoline;
    }

    fsi.positions.wifi = numframes;
#if HAS_WIFI && !defined(ARCH_PORTDUINO)
    if (isWifiAvailable()) {
        // call a method on debugInfoScreen object (for more details)
        normalFrames[numframes++] = &Screen::drawDebugInfoWiFiTrampoline;
    }
#endif

    fsi.frameCount = numframes; // Total framecount is used to apply FOCUS_PRESERVE
    LOG_DEBUG("Finished build frames. numframes: %d", numframes);

    ui->setFrames(normalFrames, numframes);
    ui->disableAllIndicators();

    // Add function overlay here. This can show when notifications muted, modifier key is active etc
    static OverlayCallback functionOverlay[] = {
#if !defined(HERMESX_TEST_DISABLE_HERMES_OVERLAYS)
        drawHermesXEmUiOverlay,
        drawHermesXMenuFooterOverlay,
#endif
        drawFunctionOverlay,
        drawIncomingTextPopupOverlay,
        drawIncomingNodePopupOverlay,
        drawLowMemoryReminderOverlay,
    };
    static const int functionOverlayCount = sizeof(functionOverlay) / sizeof(functionOverlay[0]);
    ui->setOverlays(functionOverlay, functionOverlayCount);
    syncTextMessageNotification();

    prevFrame = -1; // Force drawNodeInfo to pick a new node (because our list
                    // just changed)

    // Focus on a specific frame, in the frame set we just created
    switch (focus) {
    case FOCUS_DEFAULT:
        if (fsi.positions.main < fsi.frameCount) {
            ui->switchToFrame(fsi.positions.main);
        } else if (fsi.positions.mainAction < fsi.frameCount) {
            ui->switchToFrame(fsi.positions.mainAction);
        } else {
            ui->switchToFrame(0); // First frame
        }
        break;
    case FOCUS_FAULT:
        ui->switchToFrame(fsi.positions.fault);
        break;
    case FOCUS_TEXTMESSAGE:
        ui->switchToFrame(fsi.positions.textMessage);
        break;
    case FOCUS_MODULE:
        // Whichever frame was marked by MeshModule::requestFocus(), if any
        // If no module requested focus, will show the first frame instead
        LOG_INFO("[Screen] setFrames focus=module target=%u textList=%u frameCount=%u",
                 static_cast<unsigned>(fsi.positions.focusedModule), static_cast<unsigned>(fsi.positions.textMessageList),
                 static_cast<unsigned>(fsi.frameCount));
        ui->switchToFrame(fsi.positions.focusedModule);
        break;

    case FOCUS_PRESERVE:
        // If we can identify which type of frame "originalPosition" was, can move directly to it in the new frameset
        const FramesetInfo &oldFsi = this->framesetInfo;
        auto canMap = [&](uint8_t oldPos, uint8_t newPos) -> bool {
            return (oldPos < oldFsi.frameCount) && (newPos < fsi.frameCount) && (originalPosition == oldPos);
        };

        if (canMap(oldFsi.positions.fault, fsi.positions.fault))
            ui->switchToFrame(fsi.positions.fault);
        else if (canMap(oldFsi.positions.textMessageList, fsi.positions.textMessageList))
            ui->switchToFrame(fsi.positions.textMessageList);
        else if (canMap(oldFsi.positions.textMessage, fsi.positions.textMessage))
            ui->switchToFrame(fsi.positions.textMessage);
        else if (canMap(oldFsi.positions.waypoint, fsi.positions.waypoint))
            ui->switchToFrame(fsi.positions.waypoint);
        else if (canMap(oldFsi.positions.main, fsi.positions.main))
            ui->switchToFrame(fsi.positions.main);
        else if (canMap(oldFsi.positions.mainAction, fsi.positions.mainAction))
            ui->switchToFrame(fsi.positions.mainAction);
        else if (canMap(oldFsi.positions.setup, fsi.positions.setup))
            ui->switchToFrame(fsi.positions.setup);
        else if (canMap(oldFsi.positions.share, fsi.positions.share))
            ui->switchToFrame(fsi.positions.share);
        else if (canMap(oldFsi.positions.log, fsi.positions.log))
            ui->switchToFrame(fsi.positions.log);
        else if (canMap(oldFsi.positions.settings, fsi.positions.settings))
            ui->switchToFrame(fsi.positions.settings);
        else if (canMap(oldFsi.positions.wifi, fsi.positions.wifi))
            ui->switchToFrame(fsi.positions.wifi);

        // If frame count has decreased
        else if (fsi.frameCount < oldFsi.frameCount) {
            uint8_t numDropped = oldFsi.frameCount - fsi.frameCount;
            // Move n frames backwards
            if (numDropped <= originalPosition)
                ui->switchToFrame(originalPosition - numDropped);
            // Unless that would put us "out of bounds" (< 0)
            else
                ui->switchToFrame(0);
        }

        // If we're not sure exactly which frame we were on, at least return to the same frame number
        // (node frames; module frames)
        else
            ui->switchToFrame(originalPosition);

        break;
    }

    // Store the info about this frameset, for future setFrames calls
    this->framesetInfo = fsi;

    setFastFramerate(); // Draw ASAP
}

void Screen::setFrameImmediateDraw(FrameCallback *drawFrames)
{
    ui->disableAllIndicators();
    ui->setFrames(drawFrames, 1);
    setFastFramerate();
}

void Screen::syncTextMessageNotification()
{
    if (!ui) {
        return;
    }

    if (notifyingTextMessageFrame != UINT8_MAX) {
        ui->removeFrameFromNotifications(notifyingTextMessageFrame);
        notifyingTextMessageFrame = UINT8_MAX;
    }

    if (!hasRecentTextMessages()) {
        hasUnreadTextMessage = false;
        return;
    }

    const bool hasVisibleListFrame = framesetInfo.positions.textMessageList < framesetInfo.frameCount;
    if (!hasVisibleListFrame || !hasUnreadTextMessage) {
        return;
    }

    const uint8_t listFrame = framesetInfo.positions.textMessageList;
    const uint8_t detailFrame = framesetInfo.positions.textMessage;
    const bool isCurrentMessageFrame = showingNormalScreen && ui->getUiState() &&
                                       ((ui->getUiState()->currentFrame == listFrame) ||
                                        (detailFrame < framesetInfo.frameCount && ui->getUiState()->currentFrame == detailFrame));
    if (isCurrentMessageFrame) {
        hasUnreadTextMessage = false;
        return;
    }

    // Do not force-switch to the notified frame; only blink the frame indicator dot.
    if (ui->addFrameToNotifications(listFrame, false)) {
        notifyingTextMessageFrame = listFrame;
    }
}

// Dismisses the currently displayed screen frame, if possible
// Relevant for text message, waypoint, others in future?
// Triggered with a CardKB keycombo
void Screen::dismissCurrentFrame()
{
    uint8_t currentFrame = ui->getUiState()->currentFrame;
    bool dismissed = false;

    if (currentFrame == framesetInfo.positions.textMessage && hasRecentTextMessages()) {
        hasUnreadTextMessage = false;
        syncTextMessageNotification();
        if (framesetInfo.positions.textMessageList < framesetInfo.frameCount) {
            ui->switchToFrame(framesetInfo.positions.textMessageList);
            setFastFramerate();
        }
        return;
    }

    if (currentFrame == framesetInfo.positions.textMessageList && hasRecentTextMessages()) {
        hasUnreadTextMessage = false;
        syncTextMessageNotification();
        return;
    }

    else if (currentFrame == framesetInfo.positions.waypoint && devicestate.has_rx_waypoint) {
        LOG_DEBUG("Dismiss Waypoint");
        devicestate.has_rx_waypoint = false;
        dismissed = true;
    }

    // If we did make changes to dismiss, we now need to regenerate the frameset
    if (dismissed)
        setFrames();
}

void Screen::handleStartFirmwareUpdateScreen()
{
    LOG_DEBUG("Show firmware screen");
    showingNormalScreen = false;
    EINK_ADD_FRAMEFLAG(dispdev, DEMAND_FAST); // E-Ink: Explicitly use fast-refresh for next frame

    static FrameCallback frames[] = {drawFrameFirmware};
    setFrameImmediateDraw(frames);
}

void Screen::blink()
{
    setFastFramerate();
    uint8_t count = 10;
    dispdev->setBrightness(254);
    while (count > 0) {
        dispdev->fillRect(0, 0, dispdev->getWidth(), dispdev->getHeight());
        dispdev->display();
        delay(50);
        dispdev->clear();
        dispdev->display();
        delay(50);
        count = count - 1;
    }
    // The dispdev->setBrightness does not work for t-deck display, it seems to run the setBrightness function in OLEDDisplay.
    dispdev->setBrightness(brightness);
}

void Screen::increaseBrightness()
{
    const uint8_t next = ((brightness + 62) > 254) ? brightness : static_cast<uint8_t>(brightness + 62);
    setBrightnessLevel(next);

    /* TO DO: add little popup in center of screen saying what brightness level it is set to*/
}

void Screen::decreaseBrightness()
{
    const uint8_t next = (brightness < 70) ? brightness : static_cast<uint8_t>(brightness - 62);
    setBrightnessLevel(next);

    /* TO DO: add little popup in center of screen saying what brightness level it is set to*/
}

void Screen::setBrightnessLevel(uint8_t value)
{
    brightness = (value > 254) ? 254 : value;

#if defined(ST7789_CS)
    // run the setDisplayBrightness function. This works on t-decks
    static_cast<TFTDisplay *>(dispdev)->setDisplayBrightness(brightness);
#else
    dispdev->setBrightness(brightness);
#endif
}

void Screen::setFunctionSymbol(std::string sym)
{
    if (std::find(functionSymbol.begin(), functionSymbol.end(), sym) == functionSymbol.end()) {
        functionSymbol.push_back(sym);
        functionSymbolString = "";
        for (auto symbol : functionSymbol) {
            functionSymbolString = symbol + " " + functionSymbolString;
        }
        setFastFramerate();
    }
}

void Screen::removeFunctionSymbol(std::string sym)
{
    functionSymbol.erase(std::remove(functionSymbol.begin(), functionSymbol.end(), sym), functionSymbol.end());
    functionSymbolString = "";
    for (auto symbol : functionSymbol) {
        functionSymbolString = symbol + " " + functionSymbolString;
    }
    setFastFramerate();
}

// --- HermesX Remove TFT fast-path START
void Screen::drawMixed(OLEDDisplay *display, int16_t x, int16_t y, const char *text, int advanceX, int lineHeight)
{
    if (!display || !text)
        return;

    HermesX_zh::drawMixed(*display, x, y, text, advanceX, lineHeight, nullptr);
}
// --- HermesX Remove TFT fast-path END

std::string Screen::drawTimeDelta(uint32_t days, uint32_t hours, uint32_t minutes, uint32_t seconds)
{
    std::string uptime;

    if (days > (hours_in_month * 6))
        uptime = "?";
    else if (days >= 2)
        uptime = std::to_string(days) + "d";
    else if (hours >= 2)
        uptime = std::to_string(hours) + "h";
    else if (minutes >= 1)
        uptime = std::to_string(minutes) + "m";
    else
        uptime = std::to_string(seconds) + "s";
    return uptime;
}

void Screen::handlePrint(const char *text)
{
    // the string passed into us probably has a newline, but that would confuse the logging system
    // so strip it
    LOG_DEBUG("Screen: %.*s", strlen(text) - 1, text);
    if (!useDisplay || !showingNormalScreen)
        return;

    dispdev->print(text);
}

void Screen::handleOnPress()
{
    if (wakeInputGuardUntilMs != 0) {
        const uint32_t now = millis();
        if (now < wakeInputGuardUntilMs) {
            return;
        }
        wakeInputGuardUntilMs = 0;
    }

    // FastSetup page locks frame switching unless Exit is pressed.
    if (isHermesFastSetupActive() || isHermesXActionPageActive()) {
        return;
    }
    // If Canned Messages is using the "Scan and Select" input, dismiss the canned message frame when user button is pressed
    // Minimize impact as a courtesy, as "scan and select" may be used as default config for some boards
    if (scanAndSelectInput != nullptr && scanAndSelectInput->dismissCannedMessageFrame())
        return;

    // If screen was off, wake it first. This matters for Stealth, where the PowerFSM
    // state can stay ON while the screen itself has been forcibly blanked.
    if (!screenOn) {
        handleSetOn(true);
        enabled = true;
        setFastFramerate();
        return;
    }

    if (isRecentTextMessagesPageActive()) {
        if (showTextMessageDetailPage()) {
            return;
        }
    }

    // Otherwise advance to next frame. If we are in a transition, the press must have bounced, drop it.
    if (ui->getUiState()->frameState == FIXED) {
        ui->nextFrame();
        lastScreenTransition = millis();
        setFastFramerate();
    }
}

void Screen::handleShowPrevFrame()
{
    // If screen was off, just wake it, otherwise go back to previous frame
    // If we are in a transition, the press must have bounced, drop it.
    if (ui->getUiState()->frameState == FIXED) {
        ui->previousFrame();
        lastScreenTransition = millis();
        setFastFramerate();
    }
}

void Screen::handleShowNextFrame()
{
    // If screen was off, just wake it, otherwise advance to next frame
    // If we are in a transition, the press must have bounced, drop it.
    if (ui->getUiState()->frameState == FIXED) {
        ui->nextFrame();
        lastScreenTransition = millis();
        setFastFramerate();
    }
}

#ifndef SCREEN_TRANSITION_FRAMERATE
#define SCREEN_TRANSITION_FRAMERATE 30 // fps
#endif

void Screen::setFastFramerate()
{
    // We are about to start a transition so speed up fps
    targetFramerate = SCREEN_TRANSITION_FRAMERATE;
    fastUntilMs = millis() + 1200;

    ui->setTargetFPS(targetFramerate);
    // OLEDDisplayUi::update() enforces its own frame budget using lastUpdate.
    // For fixed-frame menu interactions, forcing the next tick avoids the
    // visual lag where selection state advances but the redraw is deferred.
    if (ui && ui->getUiState() && ui->getUiState()->frameState == FIXED) {
        ui->getUiState()->lastUpdate = 0;
    }
    setInterval(0); // redraw ASAP
    runASAP = true;
}

void DebugInfo::drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->setFont(FONT_SMALL);

    // The coordinates define the left starting point of the text
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_INVERTED) {
        display->fillRect(0 + x, 0 + y, x + display->getWidth(), y + FONT_HEIGHT_SMALL);
        display->setColor(BLACK);
    }

    char channelStr[20];
    {
        concurrency::LockGuard guard(&lock);
        snprintf(channelStr, sizeof(channelStr), "#%s", channels.getName(channels.getPrimaryIndex()));
    }

    // Display power status
    if (powerStatus->getHasBattery()) {
        if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_DEFAULT) {
            drawBattery(display, x, y + 2, imgBattery, powerStatus);
        } else {
            drawBattery(display, x + 1, y + 3, imgBattery, powerStatus);
        }
    } else if (powerStatus->knowsUSB()) {
        if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_DEFAULT) {
            display->drawFastImage(x, y + 2, 16, 8, powerStatus->getHasUSB() ? imgUSB : imgPower);
        } else {
            display->drawFastImage(x + 1, y + 3, 16, 8, powerStatus->getHasUSB() ? imgUSB : imgPower);
        }
    }
    // Display nodes status
    if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_DEFAULT) {
        drawNodes(display, x + (SCREEN_WIDTH * 0.25), y + 2, nodeStatus);
    } else {
        drawNodes(display, x + (SCREEN_WIDTH * 0.25), y + 3, nodeStatus);
    }
#if HAS_GPS
    // Display GPS status
    if (config.position.gps_mode != meshtastic_Config_PositionConfig_GpsMode_ENABLED) {
        drawGPSpowerstat(display, x, y + 2, gpsStatus);
    } else {
        if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_DEFAULT) {
            drawGPS(display, x + (SCREEN_WIDTH * 0.63), y + 2, gpsStatus);
        } else {
            drawGPS(display, x + (SCREEN_WIDTH * 0.63), y + 3, gpsStatus);
        }
    }
#endif
    display->setColor(WHITE);
    // --- HermesX Remove TFT fast-path START
    const int16_t screenWidth = display->width();
    const int idWidth = display->getStringWidth(ourId);
    // Draw the channel name
    if (screen)
        screen->drawMixed(display, x, y + FONT_HEIGHT_SMALL, channelStr);
    else
        HermesX_zh::drawMixed(*display, x, y + FONT_HEIGHT_SMALL, channelStr);
    // Draw our hardware ID to assist with bluetooth pairing. Either prefix with Info or S&F Logo
    if (moduleConfig.store_forward.enabled) {
#ifdef ARCH_ESP32
        if (!Throttle::isWithinTimespanMs(storeForwardModule->lastHeartbeat,
                                          (storeForwardModule->heartbeatInterval * 1200))) { // no heartbeat, overlap a bit
#if (defined(USE_EINK) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7735_CS) ||      \
     defined(ST7789_CS) || defined(USE_ST7789) || defined(HX8357_CS) || defined(ILI9488_CS) || ARCH_PORTDUINO) &&                \
    !defined(DISPLAY_FORCE_SMALL_FONTS)
            display->drawFastImage(x + screenWidth - 14 - idWidth, y + 3 + FONT_HEIGHT_SMALL, 12, 8,
                                   imgQuestionL1);
            display->drawFastImage(x + screenWidth - 14 - idWidth, y + 11 + FONT_HEIGHT_SMALL, 12, 8,
                                   imgQuestionL2);
#else
            display->drawFastImage(x + screenWidth - 10 - idWidth, y + 2 + FONT_HEIGHT_SMALL, 8, 8,
                                   imgQuestion);
#endif
        } else {
#if (defined(USE_EINK) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7735_CS) ||      \
     defined(ST7789_CS) || defined(USE_ST7789) || defined(ILI9488_CS) || defined(HX8357_CS)) &&                                  \
    !defined(DISPLAY_FORCE_SMALL_FONTS)
            display->drawFastImage(x + screenWidth - 18 - idWidth, y + 3 + FONT_HEIGHT_SMALL, 16, 8,
                                   imgSFL1);
            display->drawFastImage(x + screenWidth - 18 - idWidth, y + 11 + FONT_HEIGHT_SMALL, 16, 8,
                                   imgSFL2);
#else
            display->drawFastImage(x + screenWidth - 13 - idWidth, y + 2 + FONT_HEIGHT_SMALL, 11, 8,
                                   imgSF);
#endif
        }
#endif
    } else {
        // TODO: Raspberry Pi supports more than just the one screen size
#if (defined(USE_EINK) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7735_CS) ||      \
     defined(ST7789_CS) || defined(USE_ST7789) || defined(HX8357_CS) || defined(ILI9488_CS) || ARCH_PORTDUINO) &&                \
    !defined(DISPLAY_FORCE_SMALL_FONTS)
        display->drawFastImage(x + screenWidth - 14 - idWidth, y + 3 + FONT_HEIGHT_SMALL, 12, 8,
                               imgInfoL1);
        display->drawFastImage(x + screenWidth - 14 - idWidth, y + 11 + FONT_HEIGHT_SMALL, 12, 8,
                               imgInfoL2);
#else
        display->drawFastImage(x + screenWidth - 10 - idWidth, y + 2 + FONT_HEIGHT_SMALL, 8, 8, imgInfo);
#endif
    }

    display->drawString(x + screenWidth - idWidth, y + FONT_HEIGHT_SMALL, ourId);
    // --- HermesX Remove TFT fast-path END

    // Draw any log messages
    display->drawLogBuffer(x, y + (FONT_HEIGHT_SMALL * 2));

    /* Display a heartbeat pixel that blinks every time the frame is redrawn */
#ifdef SHOW_REDRAWS
    if (heartbeat)
        display->setPixel(0, 0);
    heartbeat = !heartbeat;
#endif
}

// Jm
void DebugInfo::drawFrameWiFi(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
#if HAS_WIFI && !defined(ARCH_PORTDUINO)
    const char *wifiName = config.network.wifi_ssid;

    display->setFont(FONT_SMALL);

    // The coordinates define the left starting point of the text
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_INVERTED) {
        display->fillRect(0 + x, 0 + y, x + display->getWidth(), y + FONT_HEIGHT_SMALL);
        display->setColor(BLACK);
    }

    if (WiFi.status() != WL_CONNECTED) {
        display->drawString(x, y, String("WiFi: Not Connected"));
        if (config.display.heading_bold)
            display->drawString(x + 1, y, String("WiFi: Not Connected"));
    } else {
        display->drawString(x, y, String("WiFi: Connected"));
        if (config.display.heading_bold)
            display->drawString(x + 1, y, String("WiFi: Connected"));

        display->drawString(x + SCREEN_WIDTH - display->getStringWidth("RSSI " + String(WiFi.RSSI())), y,
                            "RSSI " + String(WiFi.RSSI()));
        if (config.display.heading_bold) {
            display->drawString(x + SCREEN_WIDTH - display->getStringWidth("RSSI " + String(WiFi.RSSI())) - 1, y,
                                "RSSI " + String(WiFi.RSSI()));
        }
    }

    display->setColor(WHITE);

    /*
    - WL_CONNECTED: assigned when connected to a WiFi network;
    - WL_NO_SSID_AVAIL: assigned when no SSID are available;
    - WL_CONNECT_FAILED: assigned when the connection fails for all the attempts;
    - WL_CONNECTION_LOST: assigned when the connection is lost;
    - WL_DISCONNECTED: assigned when disconnected from a network;
    - WL_IDLE_STATUS: it is a temporary status assigned when WiFi.begin() is called and remains active until the number of
    attempts expires (resulting in WL_CONNECT_FAILED) or a connection is established (resulting in WL_CONNECTED);
    - WL_SCAN_COMPLETED: assigned when the scan networks is completed;
    - WL_NO_SHIELD: assigned when no WiFi shield is present;

    */
    if (WiFi.status() == WL_CONNECTED) {
        display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "IP: " + String(WiFi.localIP().toString().c_str()));
    } else if (WiFi.status() == WL_NO_SSID_AVAIL) {
        display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "SSID Not Found");
    } else if (WiFi.status() == WL_CONNECTION_LOST) {
        display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "Connection Lost");
    } else if (WiFi.status() == WL_CONNECT_FAILED) {
        display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "Connection Failed");
    } else if (WiFi.status() == WL_IDLE_STATUS) {
        display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "Idle ... Reconnecting");
    }
#ifdef ARCH_ESP32
    else {
        // Codes:
        // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html#wi-fi-reason-code
        display->drawString(x, y + FONT_HEIGHT_SMALL * 1,
                            WiFi.disconnectReasonName(static_cast<wifi_err_reason_t>(getWifiDisconnectReason())));
    }
#else
    else {
        display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "Unkown status: " + String(WiFi.status()));
    }
#endif

    display->drawString(x, y + FONT_HEIGHT_SMALL * 2, "SSID: " + String(wifiName));

    display->drawString(x, y + FONT_HEIGHT_SMALL * 3, "http://meshtastic.local");

    /* Display a heartbeat pixel that blinks every time the frame is redrawn */
#ifdef SHOW_REDRAWS
    if (heartbeat)
        display->setPixel(0, 0);
    heartbeat = !heartbeat;
#endif
#endif
}

void DebugInfo::drawFrameSettings(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->setFont(FONT_SMALL);

    // The coordinates define the left starting point of the text
    display->setTextAlignment(TEXT_ALIGN_LEFT);

#if HAS_GPS
    {
        drawHermesGpsHeroFrame(display, x, y, gpsStatus);
#ifdef SHOW_REDRAWS
        if (heartbeat)
            display->setPixel(0, 0);
        heartbeat = !heartbeat;
#endif
        return;
    }
#endif

    if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_INVERTED) {
        display->fillRect(0 + x, 0 + y, x + display->getWidth(), y + FONT_HEIGHT_SMALL);
        display->setColor(BLACK);
    }

    char batStr[20];
    if (powerStatus->getHasBattery()) {
        int batV = powerStatus->getBatteryVoltageMv() / 1000;
        int batCv = (powerStatus->getBatteryVoltageMv() % 1000) / 10;

        snprintf(batStr, sizeof(batStr), "B %01d.%02dV %3d%% %c%c", batV, batCv, powerStatus->getBatteryChargePercent(),
                 powerStatus->getIsCharging() ? '+' : ' ', powerStatus->getHasUSB() ? 'U' : ' ');

        // Line 1
        display->drawString(x, y, batStr);
        if (config.display.heading_bold)
            display->drawString(x + 1, y, batStr);
    } else {
        // Line 1
        display->drawString(x, y, String("USB"));
        if (config.display.heading_bold)
            display->drawString(x + 1, y, String("USB"));
    }

    //    auto mode = DisplayFormatters::getModemPresetDisplayName(config.lora.modem_preset, true);

    //    display->drawString(x + SCREEN_WIDTH - display->getStringWidth(mode), y, mode);
    //    if (config.display.heading_bold)
    //        display->drawString(x + SCREEN_WIDTH - display->getStringWidth(mode) - 1, y, mode);

    uint32_t currentMillis = millis();
    uint32_t seconds = currentMillis / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    uint32_t days = hours / 24;
    // currentMillis %= 1000;
    // seconds %= 60;
    // minutes %= 60;
    // hours %= 24;

    // Show uptime as days, hours, minutes OR seconds
    std::string uptime = screen->drawTimeDelta(days, hours, minutes, seconds);

    // Line 1 (Still)
    display->drawString(x + SCREEN_WIDTH - display->getStringWidth(uptime.c_str()), y, uptime.c_str());
    if (config.display.heading_bold)
        display->drawString(x - 1 + SCREEN_WIDTH - display->getStringWidth(uptime.c_str()), y, uptime.c_str());

    display->setColor(WHITE);

    // Setup string to assemble analogClock string
    std::string analogClock = "";

    uint32_t rtc_sec = getValidTime(RTCQuality::RTCQualityDevice, true); // Display local timezone
    if (rtc_sec > 0) {
        long hms = rtc_sec % SEC_PER_DAY;
        // hms += tz.tz_dsttime * SEC_PER_HOUR;
        // hms -= tz.tz_minuteswest * SEC_PER_MIN;
        // mod `hms` to ensure in positive range of [0...SEC_PER_DAY)
        hms = (hms + SEC_PER_DAY) % SEC_PER_DAY;

        // Tear apart hms into h:m:s
        int hour = hms / SEC_PER_HOUR;
        int min = (hms % SEC_PER_HOUR) / SEC_PER_MIN;
        int sec = (hms % SEC_PER_HOUR) % SEC_PER_MIN; // or hms % SEC_PER_MIN

        char timebuf[12];

        if (config.display.use_12h_clock) {
            std::string meridiem = "am";
            if (hour >= 12) {
                if (hour > 12)
                    hour -= 12;
                meridiem = "pm";
            }
            if (hour == 00) {
                hour = 12;
            }
            snprintf(timebuf, sizeof(timebuf), "%d:%02d:%02d%s", hour, min, sec, meridiem.c_str());
        } else {
            snprintf(timebuf, sizeof(timebuf), "%02d:%02d:%02d", hour, min, sec);
        }
        analogClock += timebuf;
    }

    // Line 2
    display->drawString(x, y + FONT_HEIGHT_SMALL * 1, analogClock.c_str());

    // Display Channel Utilization
    char chUtil[13];
    snprintf(chUtil, sizeof(chUtil), "ChUtil %2.0f%%", airTime->channelUtilizationPercent());
    display->drawString(x + SCREEN_WIDTH - display->getStringWidth(chUtil), y + FONT_HEIGHT_SMALL * 1, chUtil);

#if HAS_GPS
    if (config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_ENABLED) {
        // Line 3
        if (config.display.gps_format !=
            meshtastic_Config_DisplayConfig_GpsCoordinateFormat_DMS) // if DMS then don't draw altitude
            drawGPSAltitude(display, x, y + FONT_HEIGHT_SMALL * 2, gpsStatus);

        // Line 4
        drawGPScoordinates(display, x, y + FONT_HEIGHT_SMALL * 3, gpsStatus);
    } else {
        drawGPSpowerstat(display, x, y + FONT_HEIGHT_SMALL * 2, gpsStatus);
    }
#endif
/* Display a heartbeat pixel that blinks every time the frame is redrawn */
#ifdef SHOW_REDRAWS
    if (heartbeat)
        display->setPixel(0, 0);
    heartbeat = !heartbeat;
#endif
}

int Screen::handleStatusUpdate(const meshtastic::Status *arg)
{
    // LOG_DEBUG("Screen got status update %d", arg->getStatusType());
    switch (arg->getStatusType()) {
    case STATUS_TYPE_NODE: {
        const bool nodeCountChanged = nodeStatus->getLastNumTotal() != nodeStatus->getNumTotal();

        if (showingNormalScreen && nodeCountChanged) {
            setFrames(FOCUS_PRESERVE); // Regen the list of screen frames (returning to same frame, if possible)
        }
        nodeDB->updateGUI = false;
        break;
    }
    }

    return 0;
}

int Screen::handleTextMessage(const meshtastic_MeshPacket *packet)
{
    if (packet && shouldDrawMessage(packet)) {
        storeRecentTextMessage(*packet);
        maybeArmIncomingTextPopup(*packet);
    }

    if (showingNormalScreen) {
        // Outgoing message
        if (!packet || packet->from == 0) {
            setFrames(FOCUS_PRESERVE); // Return to same frame (quietly hiding the rx text message frame)
        } else {
            const bool onTextFrameNow = ui && ui->getUiState() &&
                                        ((framesetInfo.positions.textMessageList < framesetInfo.frameCount &&
                                          ui->getUiState()->currentFrame == framesetInfo.positions.textMessageList) ||
                                         (framesetInfo.positions.textMessage < framesetInfo.frameCount &&
                                          ui->getUiState()->currentFrame == framesetInfo.positions.textMessage));
            hasUnreadTextMessage = !onTextFrameNow;
            // Keep current screen context; use frame notification dot instead of forced focus switch.
            setFrames(FOCUS_PRESERVE);
        }
    }

    return 0;
}

// Triggered by MeshModules
int Screen::handleUIFrameEvent(const UIFrameEvent *event)
{
    if (showingNormalScreen) {
        const uint8_t beforeFrame = (ui && ui->getUiState()) ? ui->getUiState()->currentFrame : 0xFF;
        LOG_INFO("[Screen] UIFrameEvent action=%u before=%u", static_cast<unsigned>(event->action),
                 static_cast<unsigned>(beforeFrame));

        // Regenerate the frameset, potentially honoring a module's internal requestFocus() call
        if (event->action == UIFrameEvent::Action::REGENERATE_FRAMESET)
            setFrames(FOCUS_MODULE);

        // Regenerate the frameset, while Attempt to maintain focus on the current frame
        else if (event->action == UIFrameEvent::Action::REGENERATE_FRAMESET_BACKGROUND)
            setFrames(FOCUS_PRESERVE);

        // Don't regenerate the frameset, just re-draw whatever is on screen ASAP
        else if (event->action == UIFrameEvent::Action::REDRAW_ONLY)
            setFastFramerate();

        const uint8_t afterFrame = (ui && ui->getUiState()) ? ui->getUiState()->currentFrame : 0xFF;
        LOG_INFO("[Screen] UIFrameEvent done action=%u after=%u", static_cast<unsigned>(event->action),
                 static_cast<unsigned>(afterFrame));
    }

    return 0;
}

bool Screen::handleHermesXActionInput(const InputEvent *event)
{
    if (!event) {
        return false;
    }

    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);

    const bool isUp = event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
    const bool isDown =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
    const bool isLeft =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT);
    const bool isRight =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT);
    const bool isSelect =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const bool isCancel =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL) ||
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK);

    const bool isCw = (eventCw != 0) && (event->inputEvent == eventCw);
    const bool isCcw = (eventCcw != 0) && (event->inputEvent == eventCcw);
    const bool isPress = (eventPress != 0) && (event->inputEvent == eventPress);
    const uint32_t now = millis();

    int8_t navDir = 0;
    const bool isRotary = (event->source && strncmp(event->source, "rotEnc", 6) == 0);
    if (isRotary) {
        if (isCcw) {
            navDir = -1;
        } else if (isCw) {
            navDir = 1;
        } else if (eventCw == 0 && eventCcw == 0) {
            if (isUp || isLeft) {
                navDir = -1;
            } else if (isDown || isRight) {
                navDir = 1;
            }
        }
    } else {
        if (isLeft || isCcw) {
            navDir = -1;
        } else if (isRight || isCw) {
            navDir = 1;
        }
    }
    auto allowNav = [&](int8_t dir) -> bool {
        if (dir == 0) {
            return false;
        }
        if (!isRotary) {
            if (hermesActionLastNavAtMs != 0 && (now - hermesActionLastNavAtMs) < kSetupNavMinIntervalMs) {
                return false;
            }
            if (hermesActionLastNavDir != 0 && dir != hermesActionLastNavDir &&
                (now - hermesActionLastNavAtMs) < kSetupNavFlipGuardMs) {
                return false;
            }
        }
        hermesActionLastNavAtMs = now;
        hermesActionLastNavDir = dir;
        return true;
    };

    auto openHermesFastSetupPage = [&](HermesFastSetupPage page) {
        hermesSetupPage = page;
        hermesSetupSelected = 0;
        hermesSetupOffset = 0;
        hermesSetupLastNavAtMs = 0;
        hermesSetupLastNavDir = 0;
        if (ui) {
            ui->switchToFrame(framesetInfo.positions.setup);
        }
    };

    auto openHermesFastSetupRoot = [&]() { openHermesFastSetupPage(HermesFastSetupPage::Root); };
    auto openHermesMainFrame = [&]() {
        if (ui && framesetInfo.positions.main < framesetInfo.frameCount) {
            ui->switchToFrame(framesetInfo.positions.main);
        } else if (screen) {
            screen->print("Home page unavailable\n");
        }
    };
    auto openHermesShareFrame = [&]() {
        if (ui && framesetInfo.positions.share < framesetInfo.frameCount) {
            ui->switchToFrame(framesetInfo.positions.share);
        }
    };
    auto openRecentTextMessageList = [&]() {
        if (!ui || framesetInfo.positions.textMessageList >= framesetInfo.frameCount) {
            return false;
        }
        if (cannedMessageModule) {
            const auto runState = cannedMessageModule->getRunState();
            if (runState != CANNED_MESSAGE_RUN_STATE_DISABLED && runState != CANNED_MESSAGE_RUN_STATE_INACTIVE) {
                cannedMessageModule->exitMenu();
            }
        }
        if (hasRecentTextMessages()) {
            hasUnreadTextMessage = false;
            syncTextMessageNotification();
        }
        LOG_INFO("[Screen] Open Recent Send list (count=%u, unread=%d)", gRecentTextMessageState.count,
                 hasUnreadTextMessage ? 1 : 0);
        ui->switchToFrame(framesetInfo.positions.textMessageList);
        return true;
    };

    if (hermesActionStealthConfirmVisible) {
        bool yesArmed = true;
        if (hermesActionStealthConfirmShownAtMs != 0) {
            const uint32_t elapsedMs = now - hermesActionStealthConfirmShownAtMs;
            yesArmed = (elapsedMs >= kStealthConfirmArmMs);
        }

        if (isCancel) {
            hermesActionStealthConfirmVisible = false;
            hermesActionStealthConfirmSelected = 0;
            hermesActionStealthConfirmShownAtMs = 0;
            setFastFramerate();
            return true;
        }

        if (navDir != 0 && allowNav(navDir)) {
            if (!yesArmed || navDir < 0) {
                hermesActionStealthConfirmSelected = 0;
            } else {
                hermesActionStealthConfirmSelected = 1;
            }
            setFastFramerate();
            return true;
        }

        if (isSelect || isPress) {
            const bool confirmEnable = yesArmed && (hermesActionStealthConfirmSelected != 0);
            hermesActionStealthConfirmVisible = false;
            hermesActionStealthConfirmSelected = 0;
            hermesActionStealthConfirmShownAtMs = 0;
            if (confirmEnable) {
                if (enableStealthMode() && screen) {
                    screen->print("Stealth ON (brightness 50%)\n");
                }
            }
            setFastFramerate();
            return true;
        }

        return true;
    }

    if (isCancel) {
        showNextFrame();
        setFastFramerate();
        return true;
    }

    if (navDir != 0 && allowNav(navDir)) {
        int selected = hermesActionSelected;
        selected += navDir;
        while (selected < 0) {
            selected += kMainActionCount;
        }
        selected %= kMainActionCount;
        hermesActionSelected = selected;
        setFastFramerate();
        return true;
    }

    if (!isSelect && !isPress) {
        return false;
    }

    if (hermesActionSelected == 0) {
        bool needsReboot = false;
        if (!isStealthModeActive()) {
            if (isTakModeActive()) {
                if (screen) {
                    screen->print("Disable TAK MODE first\n");
                }
            } else {
                hermesActionStealthConfirmVisible = true;
                hermesActionStealthConfirmSelected = 0;
                hermesActionStealthConfirmShownAtMs = millis();
            }
        } else if (disableStealthMode(&needsReboot)) {
            if (screen) {
                screen->print(needsReboot ? "Stealth OFF, rebooting...\n" : "Stealth OFF\n");
            }
            if (needsReboot) {
                rebootAtMsec = millis() + 1500;
            }
        }
    } else if (hermesActionSelected == 1) {
        if (HermesXInterfaceModule::instance) {
            const bool next = !HermesXInterfaceModule::instance->isEmergencyLampEnabled();
            HermesXInterfaceModule::instance->setEmergencyLampEnabled(next);
        }
    } else if (hermesActionSelected == 2) {
        if (ui && framesetInfo.positions.settings < framesetInfo.frameCount) {
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
            // Entering GPS from Action must force one full repaint, otherwise
            // direct-GPS cache can occasionally leave only the title neon on top
            // of the previous page.
            gDirectGpsPosterRenderCache.valid = false;
            gDirectGpsPosterWasVisible = false;
            if (supportsDirectTftClockRendering(dispdev)) {
                auto *tft = static_cast<TFTDisplay *>(dispdev);
                tft->resetColorPalette(true);
                tft->markColorPaletteDirty();
            }
#endif
            ui->switchToFrame(framesetInfo.positions.settings);
        } else if (screen) {
            screen->print("GPS page unavailable\n");
        }
    } else if (hermesActionSelected == 3) {
        if (!isTakModeActive()) {
            if (isStealthModeActive()) {
                if (screen) {
                    screen->print("Disable Stealth first\n");
                }
            } else if (enableTakMode()) {
                if (screen) {
                    screen->print("TAK MODE ON (role=TAK)\n");
                }
            }
        } else if (disableTakMode()) {
            if (screen) {
                screen->print("TAK MODE OFF\n");
            }
        }
    } else if (hermesActionSelected == 4) {
        if (screen) {
            screen->print("Sleeping...\n");
        }
        runPreDeepSleepHook(SleepPreHookParams{BUTTON_LONGPRESS_MS});
        ::doDeepSleep(Default::getConfiguredOrDefaultMs(config.power.sds_secs), false, false);
    } else if (hermesActionSelected == 5) {
        openHermesMainFrame();
    } else if (hermesActionSelected == 6) {
        openHermesShareFrame();
    } else if (hermesActionSelected == 7) {
        openHermesFastSetupRoot();
    } else if (hermesActionSelected == 8) {
        openRecentTextMessageList();
    }

    setFastFramerate();
    return true;
}

bool Screen::handleLowMemoryReminderInput(const InputEvent *event)
{
    if (!event || !lowMemoryReminderVisible) {
        return false;
    }

    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);
    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
    const bool isLeft =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT);
    const bool isRight =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT);
    const bool isUp = event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
    const bool isDown =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
    const bool isSelect =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const bool isCancel =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL) ||
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK);
    const bool isPress = (eventPress != 0) && (event->inputEvent == eventPress);
    const bool isCw = (eventCw != 0) && (event->inputEvent == eventCw);
    const bool isCcw = (eventCcw != 0) && (event->inputEvent == eventCcw);

    if (isLeft || isUp || isCcw) {
        lowMemoryReminderSelected = 0;
        setFastFramerate();
        return true;
    }
    if (isRight || isDown || isCw) {
        lowMemoryReminderSelected = 1;
        setFastFramerate();
        return true;
    }
    if (isCancel) {
        lowMemoryReminderVisible = false;
        lowMemoryReminderSuppressUntilMs = millis() + kLowMemoryReminderSuppressMs;
        setFastFramerate();
        return true;
    }
    if (!(isSelect || isPress)) {
        return true;
    }

    lowMemoryReminderVisible = false;
    if (lowMemoryReminderSelected == 0) {
        lowMemoryReminderSuppressUntilMs = millis() + kLowMemoryReminderSuppressMs;
        setFastFramerate();
        return true;
    }

    hermesSetupPage = HermesFastSetupPage::NodeDatabaseMenu;
    hermesSetupSelected = 1;
    hermesSetupOffset = 0;
    hermesSetupLastNavAtMs = 0;
    hermesSetupLastNavDir = 0;
    if (ui && framesetInfo.positions.setup < framesetInfo.frameCount) {
        ui->switchToFrame(framesetInfo.positions.setup);
    }
    setFastFramerate();
    return true;
}

bool Screen::handleHermesFastSetupInput(const InputEvent *event)
{
    if (!event) {
        return false;
    }

    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);

    const bool isUp = event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
    const bool isDown =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
    const bool isLeft =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT);
    const bool isRight =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT);
    const bool isSelect =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const bool isCancel =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL) ||
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK);

    const bool isCw = (event->inputEvent == eventCw);
    const bool isCcw = (event->inputEvent == eventCcw);
    const bool isPress = (event->inputEvent == eventPress);
    const uint32_t now = millis();

    int8_t navDir = 0;
    const bool isRotary = (event && event->source && strncmp(event->source, "rotEnc", 6) == 0);
    if (isRotary) {
        if (isCcw) {
            navDir = -1;
        } else if (isCw) {
            navDir = 1;
        } else if (eventCw == 0 && eventCcw == 0) {
            if (isUp || isLeft) {
                navDir = -1;
            } else if (isDown || isRight) {
                navDir = 1;
            }
        }
    } else {
        if (isUp || isLeft || isCcw) {
            navDir = -1;
        } else if (isDown || isRight || isCw) {
            navDir = 1;
        }
    }

    auto resetMenu = [&](HermesFastSetupPage page) {
        hermesSetupPage = page;
        hermesSetupSelected = 0;
        hermesSetupOffset = 0;
        hermesSetupLastNavAtMs = 0;
        hermesSetupLastNavDir = 0;
    };

    auto updateOffset = [&](int count) {
        if (count <= static_cast<int>(kSetupVisibleRows)) {
            hermesSetupOffset = 0;
            return;
        }
        if (hermesSetupSelected < hermesSetupOffset) {
            hermesSetupOffset = hermesSetupSelected;
        } else if (hermesSetupSelected >= hermesSetupOffset + static_cast<int>(kSetupVisibleRows)) {
            hermesSetupOffset = hermesSetupSelected - static_cast<int>(kSetupVisibleRows) + 1;
        }
        const int maxOffset = count - static_cast<int>(kSetupVisibleRows);
        if (hermesSetupOffset > maxOffset) {
            hermesSetupOffset = maxOffset;
        }
        if (hermesSetupOffset < 0) {
            hermesSetupOffset = 0;
        }
    };

    auto enterMenu = [&](HermesFastSetupPage page, int count, int selected) {
        hermesSetupPage = page;
        if (count <= 0) {
            hermesSetupSelected = 0;
        } else if (selected < 0) {
            hermesSetupSelected = 0;
        } else if (selected >= count) {
            hermesSetupSelected = count - 1;
        } else {
            hermesSetupSelected = selected;
        }
        hermesSetupOffset = 0;
        hermesSetupLastNavAtMs = 0;
        hermesSetupLastNavDir = 0;
        updateOffset(count);
    };

    auto allowNav = [&](int8_t dir) -> bool {
        if (dir == 0) {
            return false;
        }
        if (!isRotary) {
            if (hermesSetupLastNavAtMs != 0 && (now - hermesSetupLastNavAtMs) < kSetupNavMinIntervalMs) {
                return false;
            }
            if (hermesSetupLastNavDir != 0 && dir != hermesSetupLastNavDir &&
                (now - hermesSetupLastNavAtMs) < kSetupNavFlipGuardMs) {
                return false;
            }
            hermesSetupLastNavAtMs = now;
            hermesSetupLastNavDir = dir;
        }
        return true;
    };

    auto saveSetupSegments = [&](int saveWhat) {
        if (nodeDB) {
            nodeDB->saveToDisk(saveWhat);
        }
    };

    auto ensureMqttMapSettings = [&]() {
        moduleConfig.mqtt.has_map_report_settings = true;
        if (moduleConfig.mqtt.map_report_settings.publish_interval_secs < default_map_publish_interval_secs) {
            moduleConfig.mqtt.map_report_settings.publish_interval_secs = default_map_publish_interval_secs;
        }
        const uint32_t precision = getSetupCurrentMqttMapPrecision();
        moduleConfig.mqtt.map_report_settings.position_precision = precision;
    };

    auto saveSetupChannel = [&](meshtastic_Channel &channel) {
        channels.setChannel(channel);
        channels.onConfigChanged();
        saveSetupSegments(SEGMENT_CHANNELS);
        hermesSetupChannelIndex = channel.index;
        if (mqtt) {
            mqtt->start();
        }
    };

    auto enterChannelMenu = [&]() {
        ChannelIndex channelList[MAX_NUM_CHANNELS];
        const uint8_t channelCount = buildSetupChannelList(channelList, MAX_NUM_CHANNELS);
        int selected = 0;
        for (uint8_t i = 0; i < channelCount; ++i) {
            if (channelList[i] == hermesSetupChannelIndex) {
                selected = i + 1;
                break;
            }
        }
        if (selected == 0 && channelCount > 0) {
            selected = 1;
            hermesSetupChannelIndex = channelList[0];
        }
        enterMenu(HermesFastSetupPage::ChannelMenu, channelCount + 1, selected);
    };
    auto exitFastSetupToActionPage = [&]() {
        resetMenu(HermesFastSetupPage::Entry);
        if (!showHermesXActionPage()) {
            showNextFrame();
        } else {
            setFastFramerate();
        }
    };

    if (hermesSetupPage == HermesFastSetupPage::Entry) {
        if (navDir != 0 && allowNav(navDir)) {
            resetMenu(HermesFastSetupPage::Root);
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress || isCancel) {
            resetMenu(HermesFastSetupPage::Entry);
            showNextFrame();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::PassEdit) {
        const int rowCount = static_cast<int>(kSetupKeyRowCount);
        if (navDir != 0 && allowNav(navDir)) {
            int totalKeys = 0;
            for (int r = 0; r < rowCount; ++r) {
                totalKeys += kSetupKeyRowLengths[r];
            }
            int index = 0;
            for (int r = 0; r < rowCount; ++r) {
                if (r == hermesSetupKeyRow) {
                    index += hermesSetupKeyCol;
                    break;
                }
                index += kSetupKeyRowLengths[r];
            }
            index = (index + navDir + totalKeys) % totalKeys;
            int remaining = index;
            for (int r = 0; r < rowCount; ++r) {
                const int rowLen = kSetupKeyRowLengths[r];
                if (remaining < rowLen) {
                    hermesSetupKeyRow = r;
                    hermesSetupKeyCol = remaining;
                    break;
                }
                remaining -= rowLen;
            }
            const char *label = kSetupKeyRows[hermesSetupKeyRow][hermesSetupKeyCol];
            if (label) {
                LOG_INFO("[HermesFastSetup] key row=%u col=%u label=%s", hermesSetupKeyRow, hermesSetupKeyCol, label);
            } else {
                LOG_INFO("[HermesFastSetup] key row=%u col=%u label=?", hermesSetupKeyRow, hermesSetupKeyCol);
            }
        } else if (isSelect || isPress) {
            const char *label = kSetupKeyRows[hermesSetupKeyRow][hermesSetupKeyCol];
            if (label) {
                if (strcmp(label, "OK") == 0) {
                    bool ok = false;
                    if (lighthouseModule) {
                        ok = lighthouseModule->setEmergencyPassphraseSlot(hermesSetupEditingSlot, hermesSetupPassDraft);
                    }
                    const char slotChar = hermesSetupEditingSlot == 0 ? 'A' : 'B';
                    String msg = ok ? String(u8"密碼") + slotChar + u8" 已設定: " + hermesSetupPassDraft
                                    : String(u8"密碼") + slotChar + u8" 設定失敗";
                    hermesSetupToast = msg;
                    hermesSetupToastUntilMs = millis() + 1500;
                    resetMenu(HermesFastSetupPage::EmacMenu);
                } else if (strcmp(label, "DEL") == 0) {
                    if (hermesSetupPassDraft.length() > 0) {
                        hermesSetupPassDraft.remove(hermesSetupPassDraft.length() - 1);
                    }
                } else if (hermesSetupPassDraft.length() < kSetupPassMaxLen) {
                    hermesSetupPassDraft += label;
                }
            }
        } else if (isCancel) {
            resetMenu(HermesFastSetupPage::EmacMenu);
        } else {
            return false;
        }

        setFastFramerate();
        return true;
    }

    if (hermesSetupPage == HermesFastSetupPage::FrequencyEdit) {
        const int rowCount = static_cast<int>(kSetupNumericKeyRowCount);
        if (navDir != 0 && allowNav(navDir)) {
            int totalKeys = 0;
            for (int r = 0; r < rowCount; ++r) {
                totalKeys += kSetupNumericKeyRowLengths[r];
            }
            int index = 0;
            for (int r = 0; r < rowCount; ++r) {
                if (r == hermesSetupKeyRow) {
                    index += hermesSetupKeyCol;
                    break;
                }
                index += kSetupNumericKeyRowLengths[r];
            }
            index = (index + navDir + totalKeys) % totalKeys;
            int remaining = index;
            for (int r = 0; r < rowCount; ++r) {
                const int rowLen = kSetupNumericKeyRowLengths[r];
                if (remaining < rowLen) {
                    hermesSetupKeyRow = r;
                    hermesSetupKeyCol = remaining;
                    break;
                }
                remaining -= rowLen;
            }
        } else if (isSelect || isPress) {
            const char *label = kSetupNumericKeyRows[hermesSetupKeyRow][hermesSetupKeyCol];
            if (label) {
                if (strcmp(label, "OK") == 0) {
                    if (hermesSetupFrequencyDraft.length() == 0) {
                        config.lora.override_frequency = 0.0f;
                        saveSetupSegments(SEGMENT_CONFIG);
                        hermesSetupToast = u8"手動頻率已清除";
                        hermesSetupToastUntilMs = millis() + 1500;
                        resetMenu(HermesFastSetupPage::LoraMenu);
                    } else {
                        const float frequency = hermesSetupFrequencyDraft.toFloat();
                        if (frequency >= 100.0f && frequency <= 3000.0f) {
                            config.lora.override_frequency = frequency;
                            saveSetupSegments(SEGMENT_CONFIG);
                            hermesSetupToast = String(u8"手動頻率: ") + formatSetupFrequencyLabel(frequency) + "MHz";
                            hermesSetupToastUntilMs = millis() + 1500;
                            resetMenu(HermesFastSetupPage::LoraMenu);
                        } else {
                            hermesSetupToast = u8"頻率格式錯誤";
                            hermesSetupToastUntilMs = millis() + 1500;
                        }
                    }
                } else if (strcmp(label, "DEL") == 0) {
                    if (hermesSetupFrequencyDraft.length() > 0) {
                        hermesSetupFrequencyDraft.remove(hermesSetupFrequencyDraft.length() - 1);
                    }
                } else if (strcmp(label, ".") == 0) {
                    if (hermesSetupFrequencyDraft.indexOf('.') < 0 && hermesSetupFrequencyDraft.length() > 0) {
                        hermesSetupFrequencyDraft += label;
                    }
                } else if (hermesSetupFrequencyDraft.length() < 12) {
                    hermesSetupFrequencyDraft += label;
                }
            }
        } else if (isCancel) {
            resetMenu(HermesFastSetupPage::LoraMenu);
        } else {
            return false;
        }

        setFastFramerate();
        return true;
    }

    if (hermesSetupPage == HermesFastSetupPage::PassShow) {
        if (isCancel || isSelect || isPress) {
            resetMenu(HermesFastSetupPage::EmacMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    auto handleMenuNav = [&](int count) -> bool {
        if (navDir == 0) {
            return false;
        }
        if (!allowNav(navDir)) {
            return true;
        }
        if (navDir < 0) {
            if (hermesSetupSelected > 0) {
                hermesSetupSelected--;
            } else {
                hermesSetupSelected = count - 1;
            }
            updateOffset(count);
        } else {
            if (hermesSetupSelected < count - 1) {
                hermesSetupSelected++;
            } else {
                hermesSetupSelected = 0;
            }
            updateOffset(count);
        }
        return true;
    };

    if (hermesSetupPage == HermesFastSetupPage::Root) {
        if (isCancel) {
            exitFastSetupToActionPage();
            return true;
        }
        if (handleMenuNav(kSetupRootCount)) {
            LOG_INFO("[HermesFastSetup] select=%d item=%s", hermesSetupSelected, kSetupRootItems[hermesSetupSelected]);
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                exitFastSetupToActionPage();
            }
#if HERMESX_CIV_DISABLE_EMAC
            else if (hermesSetupSelected == 1) {
                resetMenu(HermesFastSetupPage::UiMenu);
            } else if (hermesSetupSelected == 2) {
                resetMenu(HermesFastSetupPage::NodeMenu);
            } else if (hermesSetupSelected == 3) {
                resetMenu(HermesFastSetupPage::CannedMenu);
            } else {
                nodeDB->saveToDisk(SEGMENT_CONFIG | SEGMENT_MODULECONFIG | SEGMENT_CHANNELS | SEGMENT_DEVICESTATE);
                hermesSetupToast = u8"即將重新開機";
                hermesSetupToastUntilMs = millis() + 1500;
                rebootAtMsec = millis() + 2000;
            }
#else
            else if (hermesSetupSelected == 1) {
                resetMenu(HermesFastSetupPage::EmacMenu);
            } else if (hermesSetupSelected == 2) {
                resetMenu(HermesFastSetupPage::UiMenu);
            } else if (hermesSetupSelected == 3) {
                resetMenu(HermesFastSetupPage::NodeMenu);
            } else if (hermesSetupSelected == 4) {
                resetMenu(HermesFastSetupPage::CannedMenu);
            } else {
                nodeDB->saveToDisk(SEGMENT_CONFIG | SEGMENT_MODULECONFIG | SEGMENT_CHANNELS | SEGMENT_DEVICESTATE);
                hermesSetupToast = u8"即將重新開機";
                hermesSetupToastUntilMs = millis() + 1500;
                rebootAtMsec = millis() + 2000;
            }
#endif
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::NodeMenu) {
        if (handleMenuNav(kSetupNodeMenuCount)) {
            const char *itemName = "未知";
            if (hermesSetupSelected == 0) {
                itemName = "返回";
            } else if (hermesSetupSelected == 1) {
                itemName = "LoRa";
            } else if (hermesSetupSelected == 2) {
                itemName = "GPS";
            } else if (hermesSetupSelected == 3) {
                itemName = "MQTT";
            } else if (hermesSetupSelected == 4) {
                itemName = "Channel";
            } else if (hermesSetupSelected == 5) {
                itemName = "Bluetooth";
            } else if (hermesSetupSelected == 6) {
                itemName = "Power";
            } else if (hermesSetupSelected == 7) {
                itemName = "NodeDB";
            }
            LOG_INFO("[HermesFastSetup] select=%d item=%s", hermesSetupSelected, itemName);
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::Root);
            } else if (hermesSetupSelected == 1) {
                resetMenu(HermesFastSetupPage::LoraMenu);
            } else if (hermesSetupSelected == 2) {
                resetMenu(HermesFastSetupPage::GpsMenu);
            } else if (hermesSetupSelected == 3) {
                resetMenu(HermesFastSetupPage::MqttMenu);
            } else if (hermesSetupSelected == 4) {
                enterChannelMenu();
            } else if (hermesSetupSelected == 5) {
                if (config.bluetooth.enabled) {
                    config.bluetooth.enabled = false;
                    saveSetupSegments(SEGMENT_CONFIG);
                    disableBluetooth();
                    hermesSetupToast = u8"藍牙已關閉";
                    hermesSetupToastUntilMs = millis() + 1500;
                } else {
                    config.bluetooth.enabled = true;
                    saveSetupSegments(SEGMENT_CONFIG);
                    hermesSetupToast = u8"藍牙已開啟，重開中";
                    hermesSetupToastUntilMs = millis() + 1500;
                    rebootAtMsec = millis() + 1500;
                }
            } else if (hermesSetupSelected == 6) {
                resetMenu(HermesFastSetupPage::PowerMenu);
            } else if (hermesSetupSelected == 7) {
                resetMenu(HermesFastSetupPage::NodeDatabaseMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::Root);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::NodeDatabaseMenu) {
        if (handleMenuNav(kSetupNodeDatabaseMenuCount)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::NodeMenu);
            } else if (hermesSetupSelected == 1) {
                resetMenu(HermesFastSetupPage::NodeDatabaseResetSelect);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::NodeMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::NodeDatabaseResetSelect) {
        if (handleMenuNav(kSetupNodeDatabaseResetCount)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::NodeDatabaseMenu);
            } else {
                uint32_t ageSeconds = 0;
                if (hermesSetupSelected == 1) {
                    ageSeconds = 12U * 60U * 60U;
                } else if (hermesSetupSelected == 2) {
                    ageSeconds = 24U * 60U * 60U;
                } else if (hermesSetupSelected == 3) {
                    ageSeconds = 48U * 60U * 60U;
                }
                if (ageSeconds > 0 && nodeDB) {
                    hermesSetupNodeCleanupAgeSeconds = ageSeconds;
                    const int removed = nodeDB->cleanupNodesOlderThan(ageSeconds, true);
                    char toastBuf[48];
                    snprintf(toastBuf, sizeof(toastBuf), u8"已清除 %d 筆節點", removed);
                    hermesSetupToast = toastBuf;
                    hermesSetupToastUntilMs = millis() + 1800;
                }
                resetMenu(HermesFastSetupPage::NodeDatabaseMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::NodeDatabaseMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::MqttMenu) {
        if (handleMenuNav(kSetupMqttMenuCount)) {
            const char *itemName = "未知";
            if (hermesSetupSelected == 0) {
                itemName = "返回";
            } else if (hermesSetupSelected == 1) {
                itemName = "MQTT";
            } else if (hermesSetupSelected == 2) {
                itemName = "客戶端代理";
            } else if (hermesSetupSelected == 3) {
                itemName = "地圖報告";
            }
            LOG_INFO("[HermesFastSetup] select=%d item=%s", hermesSetupSelected, itemName);
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::NodeMenu);
            } else if (hermesSetupSelected == 1) {
                moduleConfig.mqtt.enabled = !moduleConfig.mqtt.enabled;
                saveSetupSegments(SEGMENT_MODULECONFIG);
                if (mqtt) {
                    mqtt->start();
                }
                hermesSetupToast = moduleConfig.mqtt.enabled ? u8"MQTT 已啟用" : u8"MQTT 已停用";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (hermesSetupSelected == 2) {
                moduleConfig.mqtt.proxy_to_client_enabled = !moduleConfig.mqtt.proxy_to_client_enabled;
                saveSetupSegments(SEGMENT_MODULECONFIG);
                if (mqtt) {
                    mqtt->start();
                }
                hermesSetupToast = moduleConfig.mqtt.proxy_to_client_enabled ? u8"客戶端代理已啟用" : u8"客戶端代理已停用";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (hermesSetupSelected == 3) {
                enterMenu(HermesFastSetupPage::MqttMapReportMenu, kSetupMqttMapReportMenuCount, 1);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::NodeMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::MqttMapReportMenu) {
        if (handleMenuNav(kSetupMqttMapReportMenuCount)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::MqttMenu);
            } else if (hermesSetupSelected == 1) {
                ensureMqttMapSettings();
                const bool next =
                    !(moduleConfig.mqtt.map_reporting_enabled && moduleConfig.mqtt.map_report_settings.should_report_location);
                moduleConfig.mqtt.map_reporting_enabled = next;
                moduleConfig.mqtt.map_report_settings.should_report_location = next;
                saveSetupSegments(SEGMENT_MODULECONFIG);
                if (mqtt) {
                    mqtt->start();
                }
                hermesSetupToast = next ? u8"地圖報告已啟用" : u8"地圖報告已停用";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (hermesSetupSelected == 2) {
                ensureMqttMapSettings();
                const uint8_t selected = getSetupMqttMapPrecisionSelection(getSetupCurrentMqttMapPrecision());
                enterMenu(HermesFastSetupPage::MqttMapPrecisionSelect, kSetupMqttMapPrecisionCount + 1, selected);
            } else if (hermesSetupSelected == 3) {
                ensureMqttMapSettings();
                uint8_t selected = 1;
                const uint32_t current = getSetupCurrentMqttMapPublishInterval();
                for (uint8_t i = 0; i < kSetupMqttMapPublishCount; ++i) {
                    if (kSetupMqttMapPublishOptions[i] == current) {
                        selected = i + 1;
                        break;
                    }
                }
                enterMenu(HermesFastSetupPage::MqttMapPublishSelect, kSetupMqttMapPublishCount + 1, selected);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::MqttMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::MqttMapPrecisionSelect) {
        const int count = kSetupMqttMapPrecisionCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::MqttMapReportMenu);
            } else {
                const uint8_t index = hermesSetupSelected - 1;
                if (index < kSetupMqttMapPrecisionCount) {
                    ensureMqttMapSettings();
                    moduleConfig.mqtt.map_report_settings.position_precision = kSetupMqttMapPrecisionOptions[index].value;
                    saveSetupSegments(SEGMENT_MODULECONFIG);
                    hermesSetupToast = String(u8"地圖精確度: ") + kSetupMqttMapPrecisionOptions[index].label;
                    hermesSetupToastUntilMs = millis() + 1500;
                }
                resetMenu(HermesFastSetupPage::MqttMapReportMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::MqttMapReportMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::MqttMapPublishSelect) {
        const int count = kSetupMqttMapPublishCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::MqttMapReportMenu);
            } else {
                const uint8_t index = hermesSetupSelected - 1;
                if (index < kSetupMqttMapPublishCount) {
                    ensureMqttMapSettings();
                    moduleConfig.mqtt.map_report_settings.publish_interval_secs = kSetupMqttMapPublishOptions[index];
                    saveSetupSegments(SEGMENT_MODULECONFIG);
                    hermesSetupToast = String(u8"地圖間隔: ") + kSetupMqttMapPublishLabels[index];
                    hermesSetupToastUntilMs = millis() + 1500;
                }
                resetMenu(HermesFastSetupPage::MqttMapReportMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::MqttMapReportMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::ChannelMenu) {
        ChannelIndex channelList[MAX_NUM_CHANNELS];
        const uint8_t channelCount = buildSetupChannelList(channelList, MAX_NUM_CHANNELS);
        const int count = channelCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::NodeMenu);
            } else {
                const uint8_t index = hermesSetupSelected - 1;
                if (index < channelCount) {
                    hermesSetupChannelIndex = channelList[index];
                    resetMenu(HermesFastSetupPage::ChannelDetailMenu);
                }
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::NodeMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::ChannelDetailMenu) {
        if (handleMenuNav(kSetupChannelDetailMenuCount)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            auto channel = getSetupChannelCopy(hermesSetupChannelIndex);
            if (hermesSetupSelected == 0) {
                enterChannelMenu();
            } else if (hermesSetupSelected == 1) {
                channel.settings.uplink_enabled = !channel.settings.uplink_enabled;
                saveSetupChannel(channel);
                hermesSetupToast = channel.settings.uplink_enabled ? u8"上行已啟用" : u8"上行已停用";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (hermesSetupSelected == 2) {
                channel.settings.downlink_enabled = !channel.settings.downlink_enabled;
                saveSetupChannel(channel);
                hermesSetupToast = channel.settings.downlink_enabled ? u8"下行已啟用" : u8"下行已停用";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (hermesSetupSelected == 3) {
                channel.settings.has_module_settings = true;
                const bool next = !isSetupChannelPositionSharingEnabled(hermesSetupChannelIndex);
                channel.settings.module_settings.position_precision = next ? getSetupDefaultChannelPrecision(hermesSetupChannelIndex) : 0U;
                saveSetupChannel(channel);
                hermesSetupToast = next ? u8"位置分享已啟用" : u8"位置分享已停用";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (hermesSetupSelected == 4) {
                const uint32_t current = getSetupChannelPrecision(hermesSetupChannelIndex);
                const uint8_t selected = getSetupChannelPrecisionSelection(
                    current == 0U ? getSetupDefaultChannelPrecision(hermesSetupChannelIndex) : current);
                enterMenu(HermesFastSetupPage::ChannelPrecisionSelect, kSetupChannelPrecisionCount + 1, selected);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            enterChannelMenu();
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::ChannelPrecisionSelect) {
        const int count = kSetupChannelPrecisionCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::ChannelDetailMenu);
            } else {
                const uint8_t index = hermesSetupSelected - 1;
                if (index < kSetupChannelPrecisionCount) {
                    auto channel = getSetupChannelCopy(hermesSetupChannelIndex);
                    channel.settings.has_module_settings = true;
                    channel.settings.module_settings.position_precision = kSetupChannelPrecisionOptions[index].value;
                    saveSetupChannel(channel);
                    hermesSetupToast = String(u8"頻道精確度: ") + kSetupChannelPrecisionOptions[index].label;
                    hermesSetupToastUntilMs = millis() + 1500;
                }
                resetMenu(HermesFastSetupPage::ChannelDetailMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::ChannelDetailMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::PowerMenu) {
        if (handleMenuNav(kSetupPowerMenuCount)) {
            const char *itemName = "未知";
            if (hermesSetupSelected == 0) {
                itemName = "返回";
            } else if (hermesSetupSelected == 1) {
                itemName = "當前電壓";
            } else if (hermesSetupSelected == 2) {
                itemName = "過放保護";
            } else if (hermesSetupSelected == 3) {
                itemName = "過放門檻";
            }
            LOG_INFO("[HermesFastSetup] select=%d item=%s", hermesSetupSelected, itemName);
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::NodeMenu);
            } else if (hermesSetupSelected == 1) {
                // Read-only live metric.
            } else if (hermesSetupSelected == 2) {
                const bool next = !HermesXBatteryProtection::isEnabled();
                HermesXBatteryProtection::setEnabled(next);
                hermesSetupToast = next ? u8"過放保護已開啟" : u8"過放保護已關閉";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (hermesSetupSelected == 3) {
                const uint8_t selected =
                    getSetupPowerGuardThresholdSelection(HermesXBatteryProtection::getThresholdMv());
                enterMenu(HermesFastSetupPage::PowerGuardVoltageSelect, kSetupPowerGuardThresholdCount + 1, selected);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::NodeMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::PowerGuardVoltageSelect) {
        const int count = kSetupPowerGuardThresholdCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::PowerMenu);
            } else {
                const uint8_t index = hermesSetupSelected - 1;
                if (index < kSetupPowerGuardThresholdCount) {
                    HermesXBatteryProtection::setThresholdMv(kSetupPowerGuardThresholdOptions[index].millivolts);
                    hermesSetupToast = String(u8"過放門檻: ") + kSetupPowerGuardThresholdOptions[index].label;
                    hermesSetupToastUntilMs = millis() + 1500;
                }
                resetMenu(HermesFastSetupPage::PowerMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::PowerMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::EmacMenu) {
        if (handleMenuNav(kSetupEmacCount)) {
            LOG_INFO("[HermesFastSetup] select=%d item=%s", hermesSetupSelected, kSetupEmacItems[hermesSetupSelected]);
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 1 || hermesSetupSelected == 2) {
                hermesSetupEditingSlot = (hermesSetupSelected == 1) ? 0 : 1;
                hermesSetupPassDraft = lighthouseModule ? lighthouseModule->getEmergencyPassphrase(hermesSetupEditingSlot) : "";
                if (hermesSetupPassDraft.length() > kSetupPassMaxLen) {
                    hermesSetupPassDraft = hermesSetupPassDraft.substring(0, kSetupPassMaxLen);
                }
                hermesSetupKeyRow = 0;
                hermesSetupKeyCol = 0;
                hermesSetupPage = HermesFastSetupPage::PassEdit;
            } else if (hermesSetupSelected == 3) {
                hermesSetupPage = HermesFastSetupPage::PassShow;
            } else if (hermesSetupSelected == 4) {
                if (hermesXEmUiModule) {
                    hermesXEmUiModule->sendResetLighthouseNow();
                    hermesSetupToast = u8"EMAC解除已送出";
                } else {
                    hermesSetupToast = u8"EMAC解除失敗";
                }
                hermesSetupToastUntilMs = millis() + 1500;
                setFastFramerate();
                return true;
            } else {
                resetMenu(HermesFastSetupPage::Root);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::Root);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::UiMenu) {
        if (handleMenuNav(kSetupUiMenuCount)) {
            const char *itemName = "未知";
            if (hermesSetupSelected == 0) {
                itemName = "返回";
            } else if (hermesSetupSelected == 1) {
                itemName = "全域蜂鳴器";
            } else if (hermesSetupSelected == 2) {
                itemName = "Hermes狀態條";
            } else if (hermesSetupSelected == 3) {
                itemName = "板載RGB燈";
            } else if (hermesSetupSelected == 4) {
                itemName = "螢幕休眠時間";
            } else if (hermesSetupSelected == 5) {
                itemName = "時區設定";
            } else if (hermesSetupSelected == 6) {
                itemName = "旋鈕對調";
            }
            LOG_INFO("[HermesFastSetup] select=%d item=%s", hermesSetupSelected, itemName);
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 1) {
                const bool next = !isBuzzerGloballyEnabled();
                setGlobalBuzzerEnabled(next);
                if (hermesXEmUiModule) {
                    hermesXEmUiModule->setSirenEnabled(next);
                }
                if (!next && HermesXInterfaceModule::instance) {
                    HermesXInterfaceModule::instance->stopEmergencySiren();
                }
                hermesSetupToast = next ? u8"全域蜂鳴器已啟用" : u8"全域蜂鳴器已停用";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (hermesSetupSelected == 2) {
                int selected = 1;
                const uint8_t ledBrightness =
                    HermesXInterfaceModule::instance ? HermesXInterfaceModule::instance->getUiLedBrightness() : 60;
                for (uint8_t i = 0; i < kSetupBrightnessCount; ++i) {
                    if (kSetupBrightnessOptions[i].value == ledBrightness) {
                        selected = i + 1;
                        break;
                    }
                }
                enterMenu(HermesFastSetupPage::UiBrightnessSelect, kSetupBrightnessCount + 1, selected);
            } else if (hermesSetupSelected == 3) {
                moduleConfig.has_ambient_lighting = true;
                if (moduleConfig.ambient_lighting.current == 0) {
                    moduleConfig.ambient_lighting.current = 10;
                }
                moduleConfig.ambient_lighting.led_state = !moduleConfig.ambient_lighting.led_state;
                saveSetupSegments(SEGMENT_MODULECONFIG);
                hermesSetupToast = moduleConfig.ambient_lighting.led_state ? u8"狀態燈已開啟 (重開生效)"
                                                                           : u8"狀態燈已關閉 (重開生效)";
                hermesSetupToastUntilMs = millis() + 1800;
            } else if (hermesSetupSelected == 4) {
                const uint8_t selected = getSetupScreenSleepSelection(getSetupCurrentScreenSleepSeconds());
                enterMenu(HermesFastSetupPage::UiScreenSleepSelect, kSetupScreenSleepCount + 1, selected);
            } else if (hermesSetupSelected == 5) {
                const uint8_t selected = getSetupTimezoneSelection(config.device.tzdef);
                enterMenu(HermesFastSetupPage::UiTimezoneSelect, kSetupTimezoneCount + 1, selected);
            } else if (hermesSetupSelected == 6) {
                const bool rotarySwapped =
                    moduleConfig.canned_message.inputbroker_event_cw ==
                    meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP;
                enterMenu(HermesFastSetupPage::UiRotarySwapSelect, 3, rotarySwapped ? 2 : 1);
            } else if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::Root);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::Root);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::UiRotarySwapSelect) {
        if (handleMenuNav(3)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::UiMenu);
            } else {
                const bool swapEnabled = (hermesSetupSelected == 2);
                moduleConfig.canned_message.inputbroker_event_cw =
                    swapEnabled ? meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP
                                : meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN;
                moduleConfig.canned_message.inputbroker_event_ccw =
                    swapEnabled ? meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN
                                : meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP;
                saveSetupSegments(SEGMENT_MODULECONFIG);
                hermesSetupToast = swapEnabled ? u8"旋鈕對調已啟用" : u8"旋鈕對調已關閉";
                hermesSetupToastUntilMs = millis() + 1500;
                resetMenu(HermesFastSetupPage::UiMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::UiMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::UiBrightnessSelect) {
        if (handleMenuNav(kSetupBrightnessCount + 1)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::UiMenu);
            } else {
                const uint8_t idx = static_cast<uint8_t>(hermesSetupSelected - 1);
                if (idx < kSetupBrightnessCount) {
                    if (HermesXInterfaceModule::instance) {
                        HermesXInterfaceModule::instance->setUiLedBrightnessPreference(kSetupBrightnessOptions[idx].value);
                    }
                    hermesSetupToast = String(u8"Hermes狀態條: ") + kSetupBrightnessOptions[idx].label;
                    hermesSetupToastUntilMs = millis() + 1500;
                }
                resetMenu(HermesFastSetupPage::UiMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::UiMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::UiScreenSleepSelect) {
        if (handleMenuNav(kSetupScreenSleepCount + 1)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::UiMenu);
            } else {
                const uint8_t idx = static_cast<uint8_t>(hermesSetupSelected - 1);
                if (idx < kSetupScreenSleepCount) {
                    config.display.screen_on_secs = kSetupScreenSleepOptions[idx].seconds;
                    nodeDB->saveToDisk(SEGMENT_CONFIG);
                    hermesSetupToast = String(u8"休眠: ") + kSetupScreenSleepOptions[idx].label + u8" (重開生效)";
                    hermesSetupToastUntilMs = millis() + 1800;
                }
                resetMenu(HermesFastSetupPage::UiMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::UiMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::UiTimezoneSelect) {
        if (handleMenuNav(kSetupTimezoneCount + 1)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::UiMenu);
            } else {
                const uint8_t idx = static_cast<uint8_t>(hermesSetupSelected - 1);
                if (idx < kSetupTimezoneCount) {
                    applySetupTimezone(kSetupTimezoneOptions[idx].tz);
                    saveSetupSegments(SEGMENT_CONFIG);
                    hermesSetupToast = String(u8"時區: ") + kSetupTimezoneOptions[idx].label;
                    hermesSetupToastUntilMs = millis() + 1500;
                }
                resetMenu(HermesFastSetupPage::UiMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::UiMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::LoraMenu) {
        if (handleMenuNav(kSetupLoraMenuCount)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::NodeMenu);
            } else if (hermesSetupSelected == 1) {
                int selected = 1;
                for (uint8_t i = 0; i < kSetupRoleOptionCount; ++i) {
                    if (kSetupRoleOptions[i].role == config.device.role) {
                        selected = i + 1;
                        break;
                    }
                }
                enterMenu(HermesFastSetupPage::LoraRoleSelect, kSetupRoleOptionCount + 1, selected);
            } else if (hermesSetupSelected == 2) {
                int selected = 1;
                for (uint8_t i = 0; i < kSetupLoraPresetOptionCount; ++i) {
                    if (kSetupLoraPresetOptions[i].preset == config.lora.modem_preset) {
                        selected = i + 1;
                        break;
                    }
                }
                enterMenu(HermesFastSetupPage::LoraPresetSelect, kSetupLoraPresetOptionCount + 1, selected);
            } else if (hermesSetupSelected == 3) {
                int selected = 1;
                const uint8_t regionCount = getSetupRegionOptionCount();
                for (uint8_t i = 0; i < regionCount; ++i) {
                    if (regions[i].code == config.lora.region) {
                        selected = i + 1;
                        break;
                    }
                }
                enterMenu(HermesFastSetupPage::LoraRegionSelect, regionCount + 1, selected);
            } else if (hermesSetupSelected == 4) {
                config.lora.ignore_mqtt = !config.lora.ignore_mqtt;
                saveSetupSegments(SEGMENT_CONFIG);
                hermesSetupToast = config.lora.ignore_mqtt ? u8"已改為忽視 MQTT" : u8"已接收 MQTT";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (hermesSetupSelected == 5) {
                config.lora.config_ok_to_mqtt = !config.lora.config_ok_to_mqtt;
                saveSetupSegments(SEGMENT_CONFIG);
                hermesSetupToast = config.lora.config_ok_to_mqtt ? u8"允許轉發至 MQTT" : u8"禁止轉發至 MQTT";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (hermesSetupSelected == 6) {
                config.lora.tx_enabled = !config.lora.tx_enabled;
                saveSetupSegments(SEGMENT_CONFIG);
                hermesSetupToast = config.lora.tx_enabled ? u8"LoRa 已啟用" : u8"LoRa 已停用";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (hermesSetupSelected == 7) {
                const int selected = config.lora.channel_num ? (config.lora.channel_num + 1) : 1;
                enterMenu(HermesFastSetupPage::LoraChannelSlotSelect, getSetupLoraChannelSlotCount() + 2, selected);
            } else if (hermesSetupSelected == 8) {
                hermesSetupFrequencyDraft =
                    (fabsf(config.lora.override_frequency) < 0.0001f) ? String("") : formatSetupFrequencyLabel(config.lora.override_frequency);
                hermesSetupKeyRow = 0;
                hermesSetupKeyCol = 0;
                hermesSetupPage = HermesFastSetupPage::FrequencyEdit;
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::NodeMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::LoraRoleSelect) {
        const int count = kSetupRoleOptionCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::LoraMenu);
            } else {
                const uint8_t index = hermesSetupSelected - 1;
                if (index < kSetupRoleOptionCount) {
                    const auto nextRole = kSetupRoleOptions[index].role;
                    if (config.device.role != nextRole) {
                        config.device.role = nextRole;
                        if (nodeDB) {
                            nodeDB->installRoleDefaults(nextRole);
                        }
                        saveSetupSegments(SEGMENT_CONFIG | SEGMENT_NODEDATABASE | SEGMENT_DEVICESTATE);
                        setFrames(FOCUS_DEFAULT);
                        hermesSetupToast = String("Role: ") + kSetupRoleOptions[index].label + u8" (重開生效)";
                        hermesSetupToastUntilMs = millis() + 1800;
                    }
                }
                resetMenu(HermesFastSetupPage::LoraMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::LoraMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::LoraPresetSelect) {
        const int count = kSetupLoraPresetOptionCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::LoraMenu);
            } else {
                const uint8_t index = hermesSetupSelected - 1;
                if (index < kSetupLoraPresetOptionCount) {
                    config.lora.use_preset = true;
                    config.lora.modem_preset = kSetupLoraPresetOptions[index].preset;
                    saveSetupSegments(SEGMENT_CONFIG);
                    hermesSetupToast = String(u8"Preset: ") + kSetupLoraPresetOptions[index].label + u8" (重開生效)";
                    hermesSetupToastUntilMs = millis() + 1800;
                }
                resetMenu(HermesFastSetupPage::LoraMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::LoraMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::LoraRegionSelect) {
        const int count = getSetupRegionOptionCount() + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::LoraMenu);
            } else {
                const uint8_t index = hermesSetupSelected - 1;
                if (index < getSetupRegionOptionCount()) {
                    config.lora.region = regions[index].code;
                    saveSetupSegments(SEGMENT_CONFIG);
                    hermesSetupToast = String(u8"地區: ") + regions[index].name + u8" (重開生效)";
                    hermesSetupToastUntilMs = millis() + 1800;
                }
                resetMenu(HermesFastSetupPage::LoraMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::LoraMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::LoraChannelSlotSelect) {
        const int count = getSetupLoraChannelSlotCount() + 2;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::LoraMenu);
            } else if (hermesSetupSelected == 1) {
                config.lora.channel_num = 0;
                saveSetupSegments(SEGMENT_CONFIG);
                hermesSetupToast = u8"頻段槽位: 自動";
                hermesSetupToastUntilMs = millis() + 1500;
                resetMenu(HermesFastSetupPage::LoraMenu);
            } else {
                config.lora.channel_num = hermesSetupSelected - 1;
                saveSetupSegments(SEGMENT_CONFIG);
                hermesSetupToast = String(u8"頻段槽位: ") + String(config.lora.channel_num);
                hermesSetupToastUntilMs = millis() + 1500;
                resetMenu(HermesFastSetupPage::LoraMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::LoraMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::CannedMenu) {
        if (handleMenuNav(2)) {
            LOG_INFO("[HermesFastSetup] select=%d item=%s", hermesSetupSelected, (hermesSetupSelected == 0) ? "返回" : "目標頻道");
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::Root);
            } else {
                ChannelIndex channelList[MAX_NUM_CHANNELS];
                const uint8_t channelCount = buildSetupChannelList(channelList, MAX_NUM_CHANNELS);
                int selected = 0;
                ChannelIndex current = cannedMessageModule ? cannedMessageModule->getPreferredChannel() : 0;
                for (uint8_t i = 0; i < channelCount; ++i) {
                    if (channelList[i] == current) {
                        selected = i + 1;
                        break;
                    }
                }
                enterMenu(HermesFastSetupPage::CannedChannelSelect, channelCount + 1, selected);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::Root);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::CannedChannelSelect) {
        ChannelIndex channelList[MAX_NUM_CHANNELS];
        const uint8_t channelCount = buildSetupChannelList(channelList, MAX_NUM_CHANNELS);
        const int count = channelCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::CannedMenu);
            } else {
                const uint8_t index = hermesSetupSelected - 1;
                if (index < channelCount && cannedMessageModule) {
                    const ChannelIndex chan = channelList[index];
                    cannedMessageModule->setPreferredChannel(chan);
                    const char *name = channels.getName(chan);
                    hermesSetupToast = String(u8"頻道已設定: ") + (name ? name : u8"未知");
                    hermesSetupToastUntilMs = millis() + 1500;
                }
                resetMenu(HermesFastSetupPage::CannedMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::CannedMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::GpsMenu) {
        if (handleMenuNav(kSetupGpsMenuCount)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::NodeMenu);
            } else if (hermesSetupSelected == 1) {
                const uint32_t current =
                    Default::getConfiguredOrDefault(config.position.gps_update_interval, default_gps_update_interval);
                int selected = 1;
                for (uint8_t i = 0; i < kSetupGpsUpdateCount; ++i) {
                    if (kSetupGpsUpdateOptions[i] == current) {
                        selected = i + 1;
                        break;
                    }
                }
                enterMenu(HermesFastSetupPage::GpsUpdateSelect, kSetupGpsUpdateCount + 1, selected);
            } else if (hermesSetupSelected == 2) {
                const uint32_t current =
                    Default::getConfiguredOrDefault(config.position.position_broadcast_secs, default_broadcast_interval_secs);
                int selected = 1;
                for (uint8_t i = 0; i < kSetupGpsBroadcastCount; ++i) {
                    if (kSetupGpsBroadcastOptions[i] == current) {
                        selected = i + 1;
                        break;
                    }
                }
                enterMenu(HermesFastSetupPage::GpsBroadcastSelect, kSetupGpsBroadcastCount + 1, selected);
            } else if (hermesSetupSelected == 3) {
                config.position.position_broadcast_smart_enabled = !config.position.position_broadcast_smart_enabled;
                saveSetupSegments(SEGMENT_CONFIG);
                hermesSetupToast = config.position.position_broadcast_smart_enabled ? u8"智慧位置已啟用"
                                                                                   : u8"智慧位置已停用";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (hermesSetupSelected == 4) {
                int selected = 1;
                const uint32_t current = getSetupCurrentGpsSmartDistance();
                for (uint8_t i = 0; i < kSetupGpsSmartDistanceCount; ++i) {
                    if (kSetupGpsSmartDistanceOptions[i] == current) {
                        selected = i + 1;
                        break;
                    }
                }
                enterMenu(HermesFastSetupPage::GpsSmartDistanceSelect, kSetupGpsSmartDistanceCount + 1, selected);
            } else if (hermesSetupSelected == 5) {
                int selected = 1;
                const uint32_t current = getSetupCurrentGpsSmartInterval();
                for (uint8_t i = 0; i < kSetupGpsSmartIntervalCount; ++i) {
                    if (kSetupGpsSmartIntervalOptions[i] == current) {
                        selected = i + 1;
                        break;
                    }
                }
                enterMenu(HermesFastSetupPage::GpsSmartIntervalSelect, kSetupGpsSmartIntervalCount + 1, selected);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::NodeMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::GpsUpdateSelect) {
        const int count = kSetupGpsUpdateCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::GpsMenu);
            } else {
                const uint8_t index = hermesSetupSelected - 1;
                if (index < kSetupGpsUpdateCount) {
                    config.position.gps_update_interval = kSetupGpsUpdateOptions[index];
                    saveSetupSegments(SEGMENT_CONFIG);
                    hermesSetupToast = String(u8"衛星更新: ") + kSetupGpsUpdateLabels[index];
                    hermesSetupToastUntilMs = millis() + 1500;
                }
                resetMenu(HermesFastSetupPage::GpsMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::GpsMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::GpsBroadcastSelect) {
        const int count = kSetupGpsBroadcastCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::GpsMenu);
            } else {
                const uint8_t index = hermesSetupSelected - 1;
                if (index < kSetupGpsBroadcastCount) {
                    config.position.position_broadcast_secs = kSetupGpsBroadcastOptions[index];
                    saveSetupSegments(SEGMENT_CONFIG);
                    hermesSetupToast = String(u8"廣播時間: ") + kSetupGpsBroadcastLabels[index];
                    hermesSetupToastUntilMs = millis() + 1500;
                }
                resetMenu(HermesFastSetupPage::GpsMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::GpsMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::GpsSmartDistanceSelect) {
        const int count = kSetupGpsSmartDistanceCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::GpsMenu);
            } else {
                const uint8_t index = hermesSetupSelected - 1;
                if (index < kSetupGpsSmartDistanceCount) {
                    config.position.broadcast_smart_minimum_distance = kSetupGpsSmartDistanceOptions[index];
                    saveSetupSegments(SEGMENT_CONFIG);
                    hermesSetupToast = String(u8"最小距離: ") + kSetupGpsSmartDistanceLabels[index];
                    hermesSetupToastUntilMs = millis() + 1500;
                }
                resetMenu(HermesFastSetupPage::GpsMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::GpsMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::GpsSmartIntervalSelect) {
        const int count = kSetupGpsSmartIntervalCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::GpsMenu);
            } else {
                const uint8_t index = hermesSetupSelected - 1;
                if (index < kSetupGpsSmartIntervalCount) {
                    config.position.broadcast_smart_minimum_interval_secs = kSetupGpsSmartIntervalOptions[index];
                    saveSetupSegments(SEGMENT_CONFIG);
                    hermesSetupToast = String(u8"最小間隔: ") + kSetupGpsSmartIntervalLabels[index];
                    hermesSetupToastUntilMs = millis() + 1500;
                }
                resetMenu(HermesFastSetupPage::GpsMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::GpsMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    return false;
}

bool Screen::handleIncomingNodePopupInput(const InputEvent *event)
{
    if (!event || !isIncomingNodePopupVisible()) {
        return false;
    }

    dismissIncomingNodePopup();
    setFastFramerate();
    return true;
}

bool Screen::handleTextMessagePopupInput(const InputEvent *event)
{
    if (!event || !isIncomingTextPopupActive()) {
        return false;
    }

    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);
    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);

    const bool isSelect =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const bool isConfiguredPress = (eventPress != 0) && (event->inputEvent == eventPress);
    const bool wantsOpen = isConfiguredPress || (eventPress == 0 && isSelect);

    const bool isLeft =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT);
    const bool isRight =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT);
    const bool isUp = event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
    const bool isDown =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
    const bool isBack =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK);
    const bool isCancel =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL);
    const bool isCw = (eventCw != 0) && (event->inputEvent == eventCw);
    const bool isCcw = (eventCcw != 0) && (event->inputEvent == eventCcw);
    const bool isRotary = (event->source && strncmp(event->source, "rotEnc", 6) == 0);
    const bool hasRotaryFallback = isRotary && (eventCw == 0 && eventCcw == 0) && (isLeft || isRight || isUp || isDown);
    const bool wantsDismiss = isLeft || isRight || isUp || isDown || isBack || isCancel || isCw || isCcw || hasRotaryFallback;

    if (wantsOpen) {
        dismissIncomingTextPopup();
        if (hasRecentTextMessages()) {
            gRecentTextMessageState.listCursor = 1;
            gRecentTextMessageState.selectedIndex = 0;
            gRecentTextMessageState.detailIndex = 0;
            showTextMessageDetailPage();
        } else {
            setFastFramerate();
        }
        return true;
    }

    if (wantsDismiss) {
        dismissIncomingTextPopup();
        setFastFramerate();
        return true;
    }

    return true; // While popup is visible, swallow other input so it doesn't leak through.
}

bool Screen::handleRecentTextMessageListInput(const InputEvent *event)
{
    if (!event) {
        return false;
    }

    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);

    const bool isUp = event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
    const bool isDown =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
    const bool isLeft =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT);
    const bool isRight =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT);
    const bool isSelect =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const bool isCancel =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL) ||
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK);

    const bool isCw = (eventCw != 0) && (event->inputEvent == eventCw);
    const bool isCcw = (eventCcw != 0) && (event->inputEvent == eventCcw);
    const bool isPress = (eventPress != 0) && (event->inputEvent == eventPress);

    int8_t navDir = 0;
    const bool isRotary = (event->source && strncmp(event->source, "rotEnc", 6) == 0);
    if (isRotary) {
        if (isCcw) {
            navDir = -1;
        } else if (isCw) {
            navDir = 1;
        } else if (eventCw == 0 && eventCcw == 0) {
            if (isUp || isLeft) {
                navDir = -1;
            } else if (isDown || isRight) {
                navDir = 1;
            }
        }
    } else {
        if (isCcw || isUp || isLeft) {
            navDir = -1;
        } else if (isCw || isDown || isRight) {
            navDir = 1;
        }
    }

    if (navDir != 0) {
        clampRecentTextMessageIndices();
        const int totalEntries = static_cast<int>(getRecentTextMessageListEntryCount());
        int nextCursor = static_cast<int>(gRecentTextMessageState.listCursor) + navDir;
        if (nextCursor < 0) {
            nextCursor = 0;
        } else if (nextCursor >= totalEntries) {
            nextCursor = totalEntries - 1;
        }

        if (nextCursor != gRecentTextMessageState.listCursor) {
            gRecentTextMessageState.listCursor = static_cast<uint8_t>(nextCursor);
            if (nextCursor > 0) {
                gRecentTextMessageState.selectedIndex = static_cast<uint8_t>(nextCursor - 1);
            }
            LOG_INFO("[Screen] Recent Send cursor=%u selected=%u total=%d", gRecentTextMessageState.listCursor,
                     gRecentTextMessageState.selectedIndex, totalEntries);
            setFastFramerate();
        }
        return true;
    }

    if (isSelect || isPress) {
        clampRecentTextMessageIndices();
        if (gRecentTextMessageState.listCursor == 0) {
            LOG_INFO("[Screen] Recent Send select -> back to action page");
            showHermesXActionPage();
            setFastFramerate();
        } else {
            gRecentTextMessageState.selectedIndex = gRecentTextMessageState.listCursor - 1;
            LOG_INFO("[Screen] Recent Send open detail index=%u", gRecentTextMessageState.selectedIndex);
            showTextMessageDetailPage();
        }
        return true;
    }

    if (isCancel) {
        LOG_INFO("[Screen] Recent Send cancel -> back to action page");
        showHermesXActionPage();
        setFastFramerate();
        return true;
    }

    return false;
}

bool Screen::handleRecentTextMessageDetailInput(const InputEvent *event)
{
    if (!event || !showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.textMessageList >= framesetInfo.frameCount) {
        return false;
    }

    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);

    const bool isUp = event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
    const bool isDown =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
    const bool isLeft =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT);
    const bool isRight =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT);
    const bool isCancel =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL) ||
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK);

    const bool isCw = (eventCw != 0) && (event->inputEvent == eventCw);
    const bool isCcw = (eventCcw != 0) && (event->inputEvent == eventCcw);
    const bool isRotary = (event->source && strncmp(event->source, "rotEnc", 6) == 0);
    const bool hasRotaryFallback = isRotary && (eventCw == 0 && eventCcw == 0) && (isUp || isDown || isLeft || isRight);

    int8_t navDir = 0;
    if (isRotary) {
        if (isCcw) {
            navDir = -1;
        } else if (isCw) {
            navDir = 1;
        } else if (eventCw == 0 && eventCcw == 0) {
            if (isUp) {
                navDir = -1;
            } else if (isDown) {
                navDir = 1;
            }
        }
    } else {
        if (isUp) {
            navDir = -1;
        } else if (isDown) {
            navDir = 1;
        }
    }

    if (navDir != 0 && gRecentTextMessageState.detailMaxScrollY > 0) {
        const uint16_t kDetailScrollStep = FONT_HEIGHT_SMALL + 2;
        const int nextScroll =
            clamp<int>(static_cast<int>(gRecentTextMessageState.detailScrollY) + navDir * kDetailScrollStep, 0,
                       static_cast<int>(gRecentTextMessageState.detailMaxScrollY));
        if (nextScroll != static_cast<int>(gRecentTextMessageState.detailScrollY)) {
            gRecentTextMessageState.detailScrollY = static_cast<uint16_t>(nextScroll);
            setFastFramerate();
        }
        return true;
    }

    const bool wantsBack = isCancel || isLeft || isRight || hasRotaryFallback;

    if (!wantsBack) {
        return false;
    }

    if (cannedMessageModule) {
        const auto runState = cannedMessageModule->getRunState();
        if (runState != CANNED_MESSAGE_RUN_STATE_DISABLED && runState != CANNED_MESSAGE_RUN_STATE_INACTIVE) {
            return false;
        }
    }

    ui->switchToFrame(framesetInfo.positions.textMessageList);
    setFastFramerate();
    return true;
}

int Screen::handleInputEvent(const InputEvent *event)
{
    if (!event) {
        return 0;
    }

    if (wakeInputGuardUntilMs != 0) {
        const uint32_t now = millis();
        if (now < wakeInputGuardUntilMs) {
            return 0;
        }
        wakeInputGuardUntilMs = 0;
    }

    if (isStealthModeActive() && screenOn) {
        armStealthWakeWindow();
    }

#if defined(DISPLAY_CLOCK_FRAME)
    // For the T-Watch, intercept touches to the 'toggle digital/analog watch face' button
    uint8_t watchFaceFrame = error_code ? 1 : 0;

    if (this->ui->getUiState()->currentFrame == watchFaceFrame && event->touchX >= 204 && event->touchX <= 240 &&
        event->touchY >= 204 && event->touchY <= 240) {
        screen->digitalWatchFace = !screen->digitalWatchFace;

        setFrames();

        return 0;
    }
#endif

    // Use left or right input from a keyboard to move between frames,
    // so long as a mesh module isn't using these events for some other purpose
    if (showingNormalScreen) {
        const uint8_t currentFrame = this->ui->getUiState()->currentFrame;
        const bool hasMenuFooter = shouldShowHermesXMenuFooter(currentFrame);

        if (handleLowMemoryReminderInput(event)) {
            return 0;
        }

        if (handleIncomingNodePopupInput(event)) {
            return 0;
        }

        if (handleTextMessagePopupInput(event)) {
            return 0;
        }

        if (currentFrame == framesetInfo.positions.mainAction) {
            if (handleHermesXActionInput(event)) {
                return 0;
            }
            // Action page handles navigation internally; keep frame switching locked.
            return 0;
        }

        if (currentFrame == framesetInfo.positions.setup) {
            if (handleHermesFastSetupInput(event)) {
                return 0;
            }
            // FastSetup page locks frame switching unless Exit is pressed.
            return 0;
        }

        if (isRecentTextMessagesPageActive()) {
            handleRecentTextMessageListInput(event);
            // Recent message list handles its own navigation and open-detail flow.
            return 0;
        }

        if (isRecentTextMessageDetailPageActive()) {
            if (handleRecentTextMessageDetailInput(event)) {
                return 0;
            }
        }

        const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);
        const bool isSelect = event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
        const bool isConfiguredPress = (eventPress != 0) && (event->inputEvent == eventPress);
        const bool isFooterShortcutPress = isConfiguredPress || (eventPress == 0 && isSelect);

        // Ask any MeshModules if they're handling keyboard input right now.
        bool inputIntercepted = false;
        for (MeshModule *module : moduleFrames) {
            if (module->interceptingKeyboardInput())
                inputIntercepted = true;
        }

        if (!inputIntercepted && isFooterShortcutPress && hasMenuFooter) {
            if (showHermesXActionPage()) {
                return 0;
            }
        }

        // If no modules are using the input, move between frames
        if (!inputIntercepted) {
            if (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT))
                showPrevFrame();
            else if (event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT))
                showNextFrame();
        }
    }

    return 0;
}

bool Screen::isStealthModeConstrained() const
{
    return isStealthModeActive();
}

void Screen::armStealthWakeWindow()
{
    if (!isStealthModeActive()) {
        return;
    }
    stealthScreenWakeUntilMs = millis() + kStealthWakeMs;
}

bool Screen::isHermesXMainPageActive() const
{
    if (!showingNormalScreen || !ui || !ui->getUiState()) {
        return false;
    }
    if (framesetInfo.positions.main >= framesetInfo.frameCount) {
        return false;
    }
    return ui->getUiState()->frameState == FIXED && ui->getUiState()->currentFrame == framesetInfo.positions.main;
}

bool Screen::isHermesFastSetupActive() const
{
#if defined(HERMESX_TEST_DISABLE_HERMES_PAGES)
    return false;
#else
    if (!showingNormalScreen || !ui) {
        return false;
    }
    return ui->getUiState()->currentFrame == framesetInfo.positions.setup;
#endif
}

bool Screen::isHermesXActionPageActive() const
{
#if defined(HERMESX_TEST_DISABLE_HERMES_PAGES)
    return false;
#else
    if (!showingNormalScreen || !ui) {
        return false;
    }
    return ui->getUiState()->currentFrame == framesetInfo.positions.mainAction;
#endif
}

bool Screen::isRecentTextMessagesPageActive() const
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.textMessageList >= framesetInfo.frameCount) {
        return false;
    }
    return ui->getUiState()->currentFrame == framesetInfo.positions.textMessageList;
}

bool Screen::isRecentTextMessageDetailPageActive() const
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.textMessage >= framesetInfo.frameCount) {
        return false;
    }
    return ui->getUiState()->currentFrame == framesetInfo.positions.textMessage;
}

uint8_t Screen::getCurrentFrameIndexForDebug() const
{
    if (!ui || !ui->getUiState()) {
        return 0xFF;
    }
    return ui->getUiState()->currentFrame;
}

uint8_t Screen::getRecentListFrameIndexForDebug() const
{
    return framesetInfo.positions.textMessageList;
}

uint8_t Screen::getRecentDetailFrameIndexForDebug() const
{
    return framesetInfo.positions.textMessage;
}

uint8_t Screen::getFrameCountForDebug() const
{
    return framesetInfo.frameCount;
}

bool Screen::shouldShowHermesXMenuFooter(uint8_t frameIndex) const
{
#if defined(HERMESX_TEST_DISABLE_HERMES_PAGES)
    (void)frameIndex;
    return false;
#else
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (cannedMessageModule) {
        const auto runState = cannedMessageModule->getRunState();
        if (runState != CANNED_MESSAGE_RUN_STATE_DISABLED && runState != CANNED_MESSAGE_RUN_STATE_INACTIVE) {
            return false; // When canned menu is active, do not show/consume Home footer shortcut.
        }
    }
    if (frameIndex >= framesetInfo.frameCount) {
        return false;
    }
    if (frameIndex == framesetInfo.positions.main) { // Keep logo page free for canned message UX.
        return false;
    }
    if ((framesetInfo.positions.textMessageList < framesetInfo.frameCount) &&
        (frameIndex == framesetInfo.positions.textMessageList)) { // Press is repurposed here to open the selected message.
        return false;
    }
    if (frameIndex == framesetInfo.positions.mainAction) { // Already the menu page.
        return false;
    }
    if (frameIndex == framesetInfo.positions.setup) { // FastSetup already has its own navigation model.
        return false;
    }
    if (frameIndex == framesetInfo.positions.settings) { // GPS hero poster reserves top-right corner visuals.
        return false;
    }
    return true;
#endif
}

bool Screen::showHermesXActionPage()
{
#if defined(HERMESX_TEST_DISABLE_HERMES_PAGES)
    return false;
#else
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.mainAction >= framesetInfo.frameCount) {
        return false;
    }
    ui->switchToFrame(framesetInfo.positions.mainAction);
    return true;
#endif
}

bool Screen::showFrameByIndex(uint8_t frameIndex)
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (frameIndex >= framesetInfo.frameCount) {
        return false;
    }

    ui->switchToFrame(frameIndex);
    setFastFramerate();
    return true;
}

bool Screen::showHermesXMainPage()
{
#if defined(HERMESX_TEST_DISABLE_HERMES_PAGES)
    return false;
#else
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.main >= framesetInfo.frameCount) {
        return false;
    }

    ui->switchToFrame(framesetInfo.positions.main);
    setFastFramerate();
    return true;
#endif
}

bool Screen::showRecentTextMessageListPage()
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.textMessageList >= framesetInfo.frameCount) {
        return false;
    }

    ui->switchToFrame(framesetInfo.positions.textMessageList);
    setFastFramerate();
    return true;
}

bool Screen::showTextMessageDetailPage()
{
    if (!showingNormalScreen || !ui || !hasRecentTextMessages()) {
        return false;
    }
    if (framesetInfo.positions.textMessage >= framesetInfo.frameCount) {
        return false;
    }

    setRecentTextMessageDetailToSelected();
    hasUnreadTextMessage = false;
    syncTextMessageNotification();
    ui->switchToFrame(framesetInfo.positions.textMessage);
    setFastFramerate();
    return true;
}

int Screen::handleAdminMessage(const meshtastic_AdminMessage *arg)
{
    switch (arg->which_payload_variant) {
    // Node removed manually (i.e. via app)
    case meshtastic_AdminMessage_remove_by_nodenum_tag:
        setFrames(FOCUS_PRESERVE);
        break;

    // Default no-op, in case the admin message observable gets used by other classes in future
    default:
        break;
    }
    return 0;
}

} // namespace graphics
#else
graphics::Screen::Screen(ScanI2C::DeviceAddress, meshtastic_Config_DisplayConfig_OledType, OLEDDISPLAY_GEOMETRY) {}
#endif // HAS_SCREEN
