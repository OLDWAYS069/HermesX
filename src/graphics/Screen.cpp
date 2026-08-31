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
#include "modules/HermesXTraceRouteBindings.h"
#include "gps/RTC.h"
#include "graphics/ScreenFonts.h"
#include "graphics/hermesx_ui/HermesXDirectMessageComposer.h"
#include "graphics/hermesx_ui/HermesXDetailPopupController.h"
#include "graphics/hermesx_ui/HermesXDetailPopupModel.h"
#include "graphics/hermesx_ui/HermesXEmergencyConfirmUiController.h"
#include "graphics/hermesx_ui/HermesXEmergencyConfirmUiModel.h"
#include "graphics/hermesx_ui/HermesXEmergencyConfirmUiRenderer.h"
#include "graphics/hermesx_ui/HermesXFastSetupUiController.h"
#include "graphics/hermesx_ui/HermesXFastSetupUiModel.h"
#include "graphics/hermesx_ui/HermesXFastSetupUiRenderer.h"
#include "graphics/hermesx_ui/HermesXHomeUiController.h"
#include "graphics/hermesx_ui/HermesXHomeDirectPresenter.h"
#include "graphics/hermesx_ui/HermesXHomeUiModel.h"
#include "graphics/hermesx_ui/HermesXHomeStateCollector.h"
#include "graphics/hermesx_ui/HermesXHomeUiRenderer.h"
#include "graphics/hermesx_ui/HermesXGpsDirectPresenter.h"
#include "graphics/hermesx_ui/HermesXGpsDirectRenderer.h"
#include "graphics/hermesx_ui/HermesXGpsStateCollector.h"
#include "graphics/hermesx_ui/HermesXGpsUiController.h"
#include "graphics/hermesx_ui/HermesXGpsUiModel.h"
#include "graphics/hermesx_ui/HermesXGpsUiRenderer.h"
#include "graphics/hermesx_ui/HermesXMessageUiController.h"
#include "graphics/hermesx_ui/HermesXMessageUiModel.h"
#include "graphics/hermesx_ui/HermesXMessageUiRenderer.h"
#include "graphics/hermesx_ui/HermesXLowMemoryUiController.h"
#include "graphics/hermesx_ui/HermesXLowMemoryUiModel.h"
#include "graphics/hermesx_ui/HermesXLowMemoryUiRenderer.h"
#include "graphics/hermesx_ui/HermesXRotaryLockUiController.h"
#include "graphics/hermesx_ui/HermesXRotaryLockUiModel.h"
#include "graphics/hermesx_ui/HermesXRotaryLockUiRenderer.h"
#include "graphics/hermesx_ui/HermesXNeonWorkspace.h"
#include "graphics/hermesx_ui/HermesXPattanakarnNeonFont.h"
#include "graphics/hermesx_ui/HermesXNodeBrowserDataSource.h"
#include "graphics/hermesx_ui/HermesXNodeBrowserUiController.h"
#include "graphics/hermesx_ui/HermesXNodeBrowserUiModel.h"
#include "graphics/hermesx_ui/HermesXNodeBrowserUiRenderer.h"
#include "graphics/hermesx_ui/HermesXTakModeUiController.h"
#include "graphics/hermesx_ui/HermesXTakModeUiModel.h"
#include "graphics/hermesx_ui/HermesXTakModeUiRenderer.h"
#include "graphics/hermesx_ui/HermesXTextLayout.h"
#include "graphics/hermesx_ui/HermesXTraceRouteUiController.h"
#include "graphics/hermesx_ui/HermesXTraceRouteUiModel.h"
#include "graphics/hermesx_ui/HermesXTraceRouteUiRenderer.h"
#include "graphics/hermesx_ui/HermesXUiInputRouter.h"
// --- HermesX Remove TFT fast-path START
#include "graphics/fonts/HermesX_zh/HermesX_CN12.h"
#include "HeapDebug.h"
#include <cstdlib>
#include <inttypes.h>
#include <math.h>
#include <time.h>
// --- HermesX Remove TFT fast-path END
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
#include "graphics/hermes_photo_boot_anim_160x80_color.h"
#endif
#include "graphics/images.h"
#include "input/RotaryEncoderInterruptImpl1.h"
#include "input/ScanAndSelect.h"
#include "input/TouchScreenImpl1.h"
#include "Led.h"
#include "main.h"
#include "mesh-pb-constants.h"
#include "mesh/Channels.h"
#include "mesh/Default.h"
#include "mesh/RadioInterface.h"
#include "mesh/TypeConversions.h"
#include "mesh/generated/meshtastic/deviceonly.pb.h"
#include "mesh/generated/meshtastic/apponly.pb.h"
#include "mesh/wifi/WiFiAPClient.h"
#include "meshUtils.h"
#include "pb_encode.h"
#include "modules/AdminModule.h"
#include "modules/CannedMessageModule.h"
#include "modules/ExternalNotificationModule.h"
#include "modules/HermesXBatteryProtection.h"
#include "modules/LighthouseModule.h"
#include "modules/HermesXInterfaceModule.h"
#include "modules/HermesXPreferences.h"
#include "modules/NodeInfoModule.h"
#include "modules/HermesXUpdateManager.h"
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
#if !MESHTASTIC_EXCLUDE_WEBSERVER
#include "mesh/http/WebServer.h"
#endif
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

using HermesXTextLayout::copyUtf8Snippet;
using HermesXTextLayout::drawLargeMixedLine;
using HermesXTextLayout::drawMixedSingleLineBounded;
using HermesXTextLayout::drawSizedMixedLine;
using HermesXTextLayout::drawVisibleWrappedLines;
using HermesXTextLayout::measureMixedWrappedTextHeight;

static auto &gHermesFastSetupNavigation = HermesXFastSetupUiModel::instance().navigation();

// This means the *visible* area (sh1106 can address 132, but shows 128 for example)
#define IDLE_FRAMERATE 1 // in fps

// DEBUG
#define NUM_EXTRA_FRAMES 21 // HermesX action pages + text/debug/system frames
// if defined a pixel will blink to show redraws
// #define SHOW_REDRAWS

// Static HermesX/system frames + all the module frames
FrameCallback *normalFrames;
static uint32_t targetFramerate = IDLE_FRAMERATE;
static uint32_t fastUntilMs = 0;
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
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

static bool supportsDirectTftOverlayRendering(OLEDDisplay *display);

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
    if (supportsDirectTftOverlayRendering(display)) {
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

static bool supportsDirectTftOverlayRendering(OLEDDisplay *display);
static bool isStealthModeActive();
static bool shouldShowHermesXHomeFrame();
static bool shouldShowHermesXGpsFrame();
// Emergency guard: keep GPS direct-TFT neon post-render disabled until boot stability is verified.
static constexpr bool kEnableDirectGpsPosterTitleNeon = true;
static constexpr bool kEnableDirectGpsPosterDecorNeon = true;
static constexpr bool kEnableDirectGpsPosterCoordinateNeon = true;

static bool gNormalFramesInitializedAfterBoot = false;
static constexpr uint32_t kScreenWakeInputGuardMs = 220;

static void invalidateDirectTftWakeCaches()
{
    HermesXHomeUiController::instance().resetForWake();
    HermesXHomeUiRenderer::resetQuote();
    HermesXGpsUiController::instance().resetForWake();
}

static constexpr int16_t kDirectNeonClockGlyphTileW = HermesXNeonClockGlyphTileWidth;
static constexpr int16_t kDirectNeonClockGlyphTileH = HermesXNeonClockGlyphTileHeight;
static constexpr int16_t kDirectNeonClockMaxRegionW = HermesXHomeUiRenderer::NeonClockMaxRegionWidth;
static constexpr int16_t kDirectNeonClockMaxRegionH = HermesXHomeUiRenderer::NeonClockMaxRegionHeight;

static bool gLowMemoryProtectionActive = false;
static auto &gEmergencyConfirmUiModel = graphics::HermesXEmergencyConfirmUiModel::instance();
static auto &gEmergencyConfirmUiState = gEmergencyConfirmUiModel.state();
static auto &gLowMemoryUiModel = graphics::HermesXLowMemoryUiModel::instance();
static auto &gLowMemoryUiState = gLowMemoryUiModel.state();
static auto &gRotaryLockUiModel = graphics::HermesXRotaryLockUiModel::instance();
static auto &gRotaryLockUiState = gRotaryLockUiModel.state();

constexpr uint32_t kLowMemoryReminderFreeThreshold = 6 * 1024;
constexpr uint32_t kLowMemoryReminderLargestThreshold = 4 * 1024;
constexpr uint32_t kLowMemoryProtectionReleaseFreeThreshold = 12 * 1024;
constexpr uint32_t kLowMemoryProtectionReleaseLargestThreshold = 8 * 1024;
constexpr uint32_t kLowMemoryDangerConfirmMs = 3000;
static uint32_t gLowMemoryDangerSinceMs = 0;

static bool shouldKeepDirectNeonBuffers()
{
    if (gLowMemoryProtectionActive) {
        return false;
    }
    return shouldShowHermesXHomeFrame() || shouldShowHermesXGpsFrame();
}

static void freeDirectNeonBuffers()
{
    HermesXNeonWorkspace::instance().release();
}

static void activateLowMemoryProtection(uint32_t freeHeap, uint32_t largestBlock)
{
    if (gLowMemoryProtectionActive) {
        return;
    }
    gLowMemoryProtectionActive = true;
    freeDirectNeonBuffers();
    LOG_WARN("[LowMemory] protection active free=%u largest=%u", freeHeap, largestBlock);
}

static bool confirmLowMemoryDanger(bool lowMemoryDanger, uint32_t now)
{
    static uint32_t lastPendingLogMs = 0;

    if (!lowMemoryDanger) {
        gLowMemoryDangerSinceMs = 0;
        lastPendingLogMs = 0;
        return false;
    }
    if (gLowMemoryProtectionActive) {
        return true;
    }
    if (gLowMemoryDangerSinceMs == 0) {
        gLowMemoryDangerSinceMs = now;
        lastPendingLogMs = now;
        LOG_DEBUG("[LowMemory] danger pending");
        return false;
    }
    if (now - lastPendingLogMs >= 1000U) {
        lastPendingLogMs = now;
        LOG_DEBUG("[LowMemory] danger pending %lums", static_cast<unsigned long>(now - gLowMemoryDangerSinceMs));
    }
    return now - gLowMemoryDangerSinceMs >= kLowMemoryDangerConfirmMs;
}

static bool releaseLowMemoryProtectionIfRecovered(uint32_t freeHeap, uint32_t largestBlock)
{
    if (!gLowMemoryProtectionActive) {
        return false;
    }
    if (freeHeap < kLowMemoryProtectionReleaseFreeThreshold || largestBlock < kLowMemoryProtectionReleaseLargestThreshold) {
        return false;
    }
    gLowMemoryProtectionActive = false;
    gLowMemoryDangerSinceMs = 0;
    invalidateDirectTftWakeCaches();
    LOG_INFO("[LowMemory] protection released free=%u largest=%u", freeHeap, largestBlock);
    return true;
}

static bool ensureDirectNeonBuffers()
{
    static uint32_t lastAllocFailureLogMs = 0;
    auto &workspace = HermesXNeonWorkspace::instance();

    if (!shouldKeepDirectNeonBuffers()) {
        workspace.release();
        return false;
    }

    if (!workspace.ensureAllocated()) {
        const HermesXNeonWorkspaceStatus status = workspace.status();
        const uint32_t now = millis();
        if (lastAllocFailureLogMs == 0 || now - lastAllocFailureLogMs >= 10000U) {
            lastAllocFailureLogMs = now;
            LOG_WARN("[DirectHome] direct neon buffer alloc failed glyph=%u shared=%u mask=%u gpsTitle=%u",
                     status.glyphCache ? 1 : 0, status.sharedMap ? 1 : 0, status.clockMasks ? 1 : 0,
                     status.gpsTitleMasks ? 1 : 0);
        }
        workspace.release();
        return false;
    }
    lastAllocFailureLogMs = 0;
    return true;
}

static void logDirectHomeNeonSummary(const char *label)
{
    auto &workspace = HermesXNeonWorkspace::instance();
    auto *glyphCaches = workspace.glyphCaches();
    if (!glyphCaches) {
        LOG_DEBUG("[DirectHome] %s glyphCaches=0 buffers=unallocated buildCount=%lu", label,
                  (unsigned long)workspace.glyphCacheBuildCount());
        logHeapSnapshot(label);
        return;
    }
    uint32_t validGlyphCaches = 0;
    for (size_t i = 0; i < HermesXPattanakarnNeonFont::GlyphCount; ++i) {
        if (glyphCaches[i].valid) {
            ++validGlyphCaches;
        }
    }
    LOG_DEBUG("[DirectHome] %s glyphCaches=%lu glyphTile=%dx%d clockRegion=%dx%d buildCount=%lu", label,
              (unsigned long)validGlyphCaches, kDirectNeonClockGlyphTileW, kDirectNeonClockGlyphTileH,
              kDirectNeonClockMaxRegionW, kDirectNeonClockMaxRegionH,
              (unsigned long)workspace.glyphCacheBuildCount());
    logHeapSnapshot(label);
}

static HermesXHomeBatterySource readHermesXHomeBatterySource()
{
    HermesXHomeBatterySource source;
    source.available = powerStatus != nullptr;
    source.voltageMv = powerStatus ? powerStatus->getBatteryVoltageMv() : 0;
    return source;
}

static bool isDirectHomePresentationAvailable(OLEDDisplay *display)
{
    return display && !gLowMemoryProtectionActive && supportsDirectTftOverlayRendering(display) &&
           HermesXHomeDirectPresenter::supportsLayout(display->getWidth(), display->getHeight());
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

static bool supportsDirectTftOverlayRendering(OLEDDisplay *display)
{
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
    return display != nullptr;
#else
    (void)display;
    return false;
#endif
}

static void initializeDisplayUi(void *context)
{
    auto *displayUi = static_cast<OLEDDisplayUi *>(context);
    if (displayUi) {
        displayUi->init();
    }
}


static HermesXGpsPosterLayers getDirectGpsPosterLayers()
{
    HermesXGpsPosterLayers layers;
    layers.title = kEnableDirectGpsPosterTitleNeon;
    layers.decor = kEnableDirectGpsPosterDecorNeon;
    layers.coordinates = kEnableDirectGpsPosterCoordinateNeon;
    return layers;
}

static HermesXGpsStateSnapshot collectDirectGpsState(int16_t width, int16_t height, const GPSStatus *gps)
{
    HermesXGpsStateInput input;
    input.width = width;
    input.height = height;
    input.sourceAvailable = gps != nullptr;
    input.gpsEnabled = config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_ENABLED;
    input.fixedPosition = config.position.fixed_position;
    if (gps) {
        input.gpsConnected = gps->getIsConnected();
        input.gpsHasLock = gps->getHasLock();
        input.satelliteCount = gps->getNumSatellites();
        input.latitudeE7 = gps->getLatitude();
        input.longitudeE7 = gps->getLongitude();
        input.altitude = gps->getAltitude();
    }
    return HermesXGpsStateCollector::collect(input, getDirectGpsPosterLayers());
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

static constexpr uint32_t kIncomingTextPopupMs = 3000;
static constexpr const char *kIncomingTextPopupEnabledFile = "/prefs/hermesx_msg_popup_enabled.txt";
static auto &gRecentTextMessageState = HermesXMessageUiModel::instance().recentState();

static auto &gNodeBrowserUiModel = HermesXNodeBrowserUiModel::instance();
static auto &gNodeBrowserDataSource = HermesXNodeBrowserDataSource::instance();
static auto &gOnlineNodeState = gNodeBrowserUiModel.onlineState();
static auto &gTraceRouteBindings = HermesXTraceRouteBindings::instance();

static auto &gTraceRouteSearchState = HermesXTraceRouteUiModel::instance().searchState();
static auto &gTraceRouteNavigationState = HermesXTraceRouteUiModel::instance().navigationState();
static constexpr uint8_t kTraceRouteBindSearchRow = 0;
static constexpr uint8_t kTraceRouteBindBackRow = 1;
static constexpr uint8_t kTraceRouteBindFirstNodeRow = 2;
static const char *getSetupRoleLabel(meshtastic_Config_DeviceConfig_Role role);
static String formatOnlineNodeSeenAgo(const meshtastic_NodeInfoLite &node);

static auto &gFinderNodeState = gNodeBrowserUiModel.finderState();
static auto &gGroupNodeState = gNodeBrowserUiModel.groupState();
static auto &gFinderPulseState = gNodeBrowserUiModel.finderPulseState();

static auto &gDirectMessageComposer = HermesXDirectMessageComposer::instance();
static auto &gIncomingTextPopupState = HermesXMessageUiModel::instance().popupState();
static bool gIncomingTextPopupEnabled = true;
static bool gIncomingTextPopupEnabledLoaded = false;

static auto &gTraceRoutePopupState = HermesXTraceRouteUiModel::instance().popupState();
static constexpr uint32_t kTraceRoutePopupDismissGuardMs = 250;

static auto &gSetupDetailPopupState = HermesXDetailPopupModel::instance().state();

static auto &gTraceRouteRequestState = HermesXTraceRouteUiModel::instance().requestState();
static constexpr uint32_t kTraceRouteResultTimeoutMs = 30000;

static void dismissIncomingTextPopup()
{
    HermesXMessageUiController::instance().dismissPopup();
}

static bool loadIncomingTextPopupEnabledPreference()
{
    return HermesXPreferences::loadBool(kIncomingTextPopupEnabledFile, true);
}

static bool isIncomingTextPopupEnabled()
{
    if (!gIncomingTextPopupEnabledLoaded) {
        gIncomingTextPopupEnabled = loadIncomingTextPopupEnabledPreference();
        gIncomingTextPopupEnabledLoaded = true;
    }
    return gIncomingTextPopupEnabled;
}

static void saveIncomingTextPopupEnabledPreference(bool enabled)
{
    if (!HermesXPreferences::saveBool(kIncomingTextPopupEnabledFile, enabled)) {
        LOG_WARN("[Screen] Failed to save incoming text popup preference");
    }
}

static void setIncomingTextPopupEnabled(bool enabled)
{
    gIncomingTextPopupEnabled = enabled;
    gIncomingTextPopupEnabledLoaded = true;
    saveIncomingTextPopupEnabledPreference(enabled);
    if (!enabled) {
        dismissIncomingTextPopup();
    }
}

static bool isIncomingTextPopupVisible()
{
    return gIncomingTextPopupState.visible && screen && screen->isHermesXMainPageActive();
}

static bool isIncomingTextPopupActive()
{
    return gIncomingTextPopupState.pending || isIncomingTextPopupVisible();
}

static void dismissTraceRoutePopup()
{
    HermesXTraceRouteUiModel::instance().dismissPopup();
}

static bool isTraceRoutePopupVisible()
{
    return gTraceRoutePopupState.visible;
}

static bool isSetupDetailPopupVisible()
{
    return gSetupDetailPopupState.visible;
}

static void showTraceRoutePopup(const char *title, const char *body)
{
    HermesXTraceRouteUiModel::instance().showPopup(title, body, millis());
    fastUntilMs = millis() + 1200;
}

static void showSetupDetailPopup(const char *title, const String &body)
{
    HermesXDetailPopupModel::instance().show(title, body, millis());
    fastUntilMs = millis() + 1200;
}

static void drawTraceRoutePopupOverlay(OLEDDisplay *display, OLEDDisplayUiState *state);
static void drawSetupDetailPopupOverlay(OLEDDisplay *display, OLEDDisplayUiState *state);

static void startTraceRoutePending(NodeNum destNode, uint32_t requestId)
{
    HermesXTraceRouteUiModel::instance().startRequest(destNode, requestId, millis());
}

static void clearTraceRoutePending()
{
    HermesXTraceRouteUiModel::instance().clearRequest();
}

static bool isExpectedTraceRouteResult(NodeNum fromNode, uint32_t requestId)
{
    return HermesXTraceRouteUiModel::instance().isExpectedResult(fromNode, requestId);
}

static void armIncomingTextPopup(const meshtastic_MeshPacket &packet)
{
    gIncomingTextPopupState.packet = packet;
    gIncomingTextPopupState.selectedOption = 0;
    if (gIncomingTextPopupState.visible) {
        gIncomingTextPopupState.untilMs = millis() + kIncomingTextPopupMs;
    } else {
        gIncomingTextPopupState.pending = true;
        gIncomingTextPopupState.visible = false;
        gIncomingTextPopupState.untilMs = 0;
    }
}

void Screen::maybeArmIncomingTextPopup(const meshtastic_MeshPacket &packet)
{
    if (packet.from == 0 || config.display.screen_on_secs == 0 || isStealthModeActive() ||
        !isIncomingTextPopupEnabled()) {
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

bool Screen::showTraceRouteResultPopup(NodeNum fromNode, uint32_t requestId, const char *title, const char *body)
{
    if (!isExpectedTraceRouteResult(fromNode, requestId)) {
        LOG_DEBUG("[Screen] Ignore TraceRoute result from=%08lx req=%08lx pending=%u dest=%08lx pending_req=%08lx",
                  static_cast<unsigned long>(fromNode), static_cast<unsigned long>(requestId),
                  gTraceRouteRequestState.pending ? 1 : 0, static_cast<unsigned long>(gTraceRouteRequestState.destination),
                  static_cast<unsigned long>(gTraceRouteRequestState.requestId));
        return false;
    }
    clearTraceRoutePending();
    showTraceRoutePopup(title, body);
    return true;
}

static bool hasRecentTextMessages()
{
    return HermesXMessageUiModel::instance().hasRecentMessages();
}

static bool isTraceRouteBindNodeRow(uint8_t cursor)
{
    return cursor >= kTraceRouteBindFirstNodeRow;
}

static uint8_t traceRouteBindNodeIndex(uint8_t cursor)
{
    return cursor - kTraceRouteBindFirstNodeRow;
}

static bool isTraceRouteNodeBound(NodeNum nodeNum)
{
    return gTraceRouteBindings.contains(nodeNum);
}

static bool bindTraceRouteNode(NodeNum nodeNum)
{
    return gTraceRouteBindings.bind(nodeNum);
}

static bool unbindTraceRouteNodeAt(uint8_t index)
{
    const bool removed = gTraceRouteBindings.unbindAt(index);
    HermesXTraceRouteUiModel::instance().clampBoundSelection(gTraceRouteBindings.count());
    return removed;
}

static void syncTraceRouteBoundSelection()
{
    HermesXTraceRouteUiModel::instance().clampBoundSelection(gTraceRouteBindings.count());
}

static const meshtastic_NodeInfoLite *getBoundTraceRouteNodeAt(uint8_t index)
{
    syncTraceRouteBoundSelection();
    if (index >= gTraceRouteBindings.count()) {
        return nullptr;
    }
    return gNodeBrowserDataSource.nodeByNum(gTraceRouteBindings.nodeAt(index));
}

static const meshtastic_NodeInfoLite *getSelectedTraceRouteNode()
{
    if (gTraceRouteNavigationState.mode == HermesXTraceRouteUiMode::BoundRoutes) {
        if (gTraceRouteNavigationState.boundSelectedIndex >= gTraceRouteBindings.count()) {
            return nullptr;
        }
        return getBoundTraceRouteNodeAt(gTraceRouteNavigationState.boundSelectedIndex);
    }
    gNodeBrowserDataSource.refreshTraceRoute();
    if (gTraceRouteNavigationState.bindSelectedIndex >= gNodeBrowserDataSource.traceRouteCount()) {
        return nullptr;
    }
    return gNodeBrowserDataSource.traceRouteNodeAt(gTraceRouteNavigationState.bindSelectedIndex);
}

static String buildTraceRouteNodeInfoBody(const meshtastic_NodeInfoLite &node)
{
    String body = String("LongName: ") + (node.user.long_name[0] ? String(node.user.long_name) : String("--"));
    body += "\nrole: ";
    body += (node.has_user ? getSetupRoleLabel(node.user.role) : "--");
    body += "\n";
    body += String(u8"最近一次聽到: ") + formatOnlineNodeSeenAgo(node);
    return body;
}

static void cancelDeferredTraceRouteBindShortPress()
{
    HermesXTraceRouteUiModel::instance().cancelDeferredBindShortPress();
}

static void cancelDeferredTraceRouteBoundShortPress()
{
    HermesXTraceRouteUiModel::instance().cancelDeferredRouteShortPress();
}

static bool showTraceRouteBindInfoForSelectedNode()
{
    if (!isTraceRouteBindNodeRow(gTraceRouteNavigationState.bindCursor)) {
        return false;
    }
    gTraceRouteNavigationState.bindSelectedIndex = traceRouteBindNodeIndex(gTraceRouteNavigationState.bindCursor);
    const meshtastic_NodeInfoLite *node = getSelectedTraceRouteNode();
    if (!node) {
        LOG_WARN("[Screen] TraceRoute bind short info missing node cursor=%u count=%u",
                 static_cast<unsigned>(gTraceRouteNavigationState.bindCursor),
                 static_cast<unsigned>(gNodeBrowserDataSource.traceRouteCount()));
        return false;
    }

    LOG_INFO("[Screen] TraceRoute bind short info node=%08lx cursor=%u",
             static_cast<unsigned long>(node->num), static_cast<unsigned>(gTraceRouteNavigationState.bindCursor));
    showSetupDetailPopup("TraceRoute", buildTraceRouteNodeInfoBody(*node));
    return true;
}

static void deferTraceRouteBindShortPressForSelectedNode()
{
    if (gTraceRouteNavigationState.mode != HermesXTraceRouteUiMode::BindOnline ||
        !isTraceRouteBindNodeRow(gTraceRouteNavigationState.bindCursor)) {
        cancelDeferredTraceRouteBindShortPress();
        return;
    }

    gTraceRouteNavigationState.bindSelectedIndex = traceRouteBindNodeIndex(gTraceRouteNavigationState.bindCursor);
    const meshtastic_NodeInfoLite *node = getSelectedTraceRouteNode();
    if (!node) {
        cancelDeferredTraceRouteBindShortPress();
        LOG_WARN("[Screen] TraceRoute bind deferred short missing node cursor=%u count=%u",
                 static_cast<unsigned>(gTraceRouteNavigationState.bindCursor),
                 static_cast<unsigned>(gNodeBrowserDataSource.traceRouteCount()));
        return;
    }

    HermesXTraceRouteUiModel::instance().deferBindShortPress(gTraceRouteNavigationState.bindCursor, node->num);
    LOG_INFO("[Screen] TraceRoute bind defer short node=%08lx cursor=%u",
             static_cast<unsigned long>(node->num), static_cast<unsigned>(gTraceRouteNavigationState.bindCursor));
}

static void openTraceRouteBindConfirmForSelectedNode()
{
    cancelDeferredTraceRouteBindShortPress();
    if (gTraceRouteNavigationState.mode != HermesXTraceRouteUiMode::BindOnline ||
        !isTraceRouteBindNodeRow(gTraceRouteNavigationState.bindCursor)) {
        return;
    }

    gTraceRouteNavigationState.bindSelectedIndex = traceRouteBindNodeIndex(gTraceRouteNavigationState.bindCursor);
    const meshtastic_NodeInfoLite *node = getSelectedTraceRouteNode();
    if (!node) {
        LOG_WARN("[Screen] TraceRoute bind confirm missing node cursor=%u count=%u",
                 static_cast<unsigned>(gTraceRouteNavigationState.bindCursor),
                 static_cast<unsigned>(gNodeBrowserDataSource.traceRouteCount()));
        return;
    }

    HermesXTraceRouteUiModel::instance().showBindConfirm(node->num);
    LOG_INFO("[Screen] TraceRoute bind confirm node=%08lx cursor=%u",
             static_cast<unsigned long>(node->num), static_cast<unsigned>(gTraceRouteNavigationState.bindCursor));
}

static int getGroupNodeCount()
{
    return hermesXEmUiModule ? hermesXEmUiModule->getVisibleEmInfoNodeCount() : 0;
}

static void clampGroupNodeState()
{
    gNodeBrowserUiModel.clampGroup(static_cast<uint8_t>(std::max(0, getGroupNodeCount())));
}

static const HermesXEmUiModule::EmInfoNodeStatus *getGroupNodeAt(uint8_t index)
{
    if (!hermesXEmUiModule) {
        return nullptr;
    }
    clampGroupNodeState();
    return hermesXEmUiModule->getVisibleEmInfoNodeByIndex(index);
}

static const HermesXEmUiModule::EmInfoNodeStatus *getSelectedGroupNode()
{
    return getGroupNodeAt(gGroupNodeState.selectedIndex);
}

static const meshtastic_NodeInfoLite *getGroupMeshNode(const HermesXEmUiModule::EmInfoNodeStatus &entry)
{
    return nodeDB ? nodeDB->getMeshNode(entry.nodeNum) : nullptr;
}

static String getGroupNodeDisplayName(const HermesXEmUiModule::EmInfoNodeStatus &entry)
{
    if (entry.shortName[0] != '\0') {
        return String(entry.shortName);
    }
    const meshtastic_NodeInfoLite *node = getGroupMeshNode(entry);
    if (node && node->has_user) {
        if (node->user.short_name[0] != '\0') {
            return String(node->user.short_name);
        }
        if (node->user.long_name[0] != '\0') {
            return String(node->user.long_name);
        }
    }
    char buf[12];
    snprintf(buf, sizeof(buf), "%04lx", static_cast<unsigned long>(entry.nodeNum & 0xFFFFu));
    return String(buf);
}

static constexpr uint8_t kGroupDetailMessageRow = 1;
static constexpr uint8_t kGroupDetailTraceRouteRow = 2;

static uint8_t buildGroupNodeDetailRows(const HermesXEmUiModule::EmInfoNodeStatus &entry, String *rows, uint8_t maxRows)
{
    uint8_t rowCount = 0;
    auto appendRow = [&](const String &row) {
        if (rows && rowCount < maxRows) {
            rows[rowCount] = row;
        }
        ++rowCount;
    };

    const meshtastic_NodeInfoLite *node = getGroupMeshNode(entry);
    appendRow(u8"返回");
    appendRow("MSG");
    appendRow((node && node->via_mqtt) ? "TraceRoute: --" : "TraceRoute");
    appendRow(String(u8"EM狀態: ") + (entry.state[0] ? entry.state : u8"未知"));
    appendRow(String(u8"在線: ") + (hermesXEmUiModule ? hermesXEmUiModule->getNodePresenceLabel(entry) : u8"未知"));
    appendRow(String("LastHB: ") + (hermesXEmUiModule ? hermesXEmUiModule->getNodeRelativeHeardLabel(entry) : String("--")));
    appendRow(String(u8"電量: ") + (entry.batteryPercent > 0 ? String(entry.batteryPercent) + "%" : String("--")));
    appendRow(String("LongName: ") +
              ((node && node->has_user && node->user.long_name[0]) ? String(node->user.long_name) : String("--")));
    if (entry.place[0]) {
        appendRow(String(u8"地: ") + entry.place);
    }
    if (entry.item[0]) {
        appendRow(String(u8"物: ") + entry.item);
    }
    if (entry.latitudeI != 0 || entry.longitudeI != 0) {
        appendRow(String(u8"經: ") + (hermesXEmUiModule ? hermesXEmUiModule->getNodeLongitudeText(entry) : String("")));
        appendRow(String(u8"緯: ") + (hermesXEmUiModule ? hermesXEmUiModule->getNodeLatitudeText(entry) : String("")));
        appendRow(String(u8"高度: ") + (hermesXEmUiModule ? hermesXEmUiModule->getNodeAltitudeText(entry) : String("")));
    } else {
        appendRow(String(u8"經: --"));
        appendRow(String(u8"緯: --"));
        appendRow(String(u8"高度: --"));
    }
    appendRow(String("Node: !") + String(entry.nodeNum, HEX));
    return rowCount;
}

static bool sendOnlineNodeTraceRoute(NodeNum nodeNum)
{
    if (!service || !router || nodeNum == 0) {
        if (HermesXInterfaceModule::instance) {
            HermesXInterfaceModule::instance->playNackFail();
        }
        showTraceRoutePopup("TraceRoute", "SEND FAIL");
        return false;
    }

    meshtastic_MeshPacket *p = router->allocForSending();
    if (!p) {
        if (HermesXInterfaceModule::instance) {
            HermesXInterfaceModule::instance->playNackFail();
        }
        showTraceRoutePopup("TraceRoute", "SEND FAIL");
        return false;
    }

    p->to = nodeNum;
    p->channel = channels.getPrimaryIndex();
    p->decoded.portnum = meshtastic_PortNum_TRACEROUTE_APP;
    p->want_ack = false;
    p->decoded.want_response = true;

    meshtastic_RouteDiscovery route = meshtastic_RouteDiscovery_init_default;
    p->decoded.payload.size =
        pb_encode_to_bytes(p->decoded.payload.bytes, sizeof(p->decoded.payload.bytes), &meshtastic_RouteDiscovery_msg, &route);
    const uint32_t requestId = p->id;
    service->sendToMesh(p, RX_SRC_LOCAL, true);
    startTraceRoutePending(nodeNum, requestId);

    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->playSendFeedback();
    }
    showTraceRoutePopup("TraceRoute", "SEND\n\n等待回應...");
    return true;
}

static bool sendTraceRouteForSelectedBoundNode()
{
    if (gTraceRouteNavigationState.mode != HermesXTraceRouteUiMode::BoundRoutes || gTraceRouteNavigationState.boundCursor == 0) {
        return false;
    }
    syncTraceRouteBoundSelection();
    if (gTraceRouteNavigationState.boundCursor == 0 || gTraceRouteNavigationState.boundCursor > gTraceRouteBindings.count()) {
        return false;
    }

    gTraceRouteNavigationState.boundSelectedIndex = gTraceRouteNavigationState.boundCursor - 1;
    const meshtastic_NodeInfoLite *node = getSelectedTraceRouteNode();
    if (!node) {
        LOG_WARN("[Screen] TraceRoute bound short missing node cursor=%u count=%u",
                 static_cast<unsigned>(gTraceRouteNavigationState.boundCursor), static_cast<unsigned>(gTraceRouteBindings.count()));
        return false;
    }
    if (node->via_mqtt) {
        if (HermesXInterfaceModule::instance) {
            HermesXInterfaceModule::instance->playNackFail();
        }
        showTraceRoutePopup("TraceRoute", "LORA ONLY");
    } else {
        sendOnlineNodeTraceRoute(node->num);
    }
    return true;
}

static void deferTraceRouteBoundShortPressForSelectedNode()
{
    if (gTraceRouteNavigationState.mode != HermesXTraceRouteUiMode::BoundRoutes || gTraceRouteNavigationState.boundCursor == 0) {
        cancelDeferredTraceRouteBoundShortPress();
        return;
    }
    syncTraceRouteBoundSelection();
    if (gTraceRouteNavigationState.boundCursor == 0 || gTraceRouteNavigationState.boundCursor > gTraceRouteBindings.count()) {
        cancelDeferredTraceRouteBoundShortPress();
        return;
    }

    gTraceRouteNavigationState.boundSelectedIndex = gTraceRouteNavigationState.boundCursor - 1;
    const meshtastic_NodeInfoLite *node = getSelectedTraceRouteNode();
    if (!node) {
        cancelDeferredTraceRouteBoundShortPress();
        LOG_WARN("[Screen] TraceRoute bound deferred short missing node cursor=%u count=%u",
                 static_cast<unsigned>(gTraceRouteNavigationState.boundCursor), static_cast<unsigned>(gTraceRouteBindings.count()));
        return;
    }

    HermesXTraceRouteUiModel::instance().deferRouteShortPress(gTraceRouteNavigationState.boundCursor, node->num);
    LOG_INFO("[Screen] TraceRoute bound defer short node=%08lx cursor=%u",
             static_cast<unsigned long>(node->num), static_cast<unsigned>(gTraceRouteNavigationState.boundCursor));
}

static bool unbindSelectedTraceRouteBoundNode()
{
    cancelDeferredTraceRouteBoundShortPress();
    if (gTraceRouteNavigationState.mode != HermesXTraceRouteUiMode::BoundRoutes || gTraceRouteNavigationState.boundCursor == 0) {
        return false;
    }
    syncTraceRouteBoundSelection();
    if (gTraceRouteNavigationState.boundCursor == 0 || gTraceRouteNavigationState.boundCursor > gTraceRouteBindings.count()) {
        return false;
    }

    const uint8_t removeIndex = gTraceRouteNavigationState.boundCursor - 1;
    const NodeNum nodeNum = gTraceRouteBindings.nodeAt(removeIndex);
    if (!unbindTraceRouteNodeAt(removeIndex)) {
        return false;
    }
    LOG_INFO("[Screen] TraceRoute bound unbind node=%08lx cursor=%u",
             static_cast<unsigned long>(nodeNum), static_cast<unsigned>(gTraceRouteNavigationState.boundCursor));
    showTraceRoutePopup("TraceRoute", u8"已解除綁定");
    return true;
}

static String getOnlineNodeDisplayName(const meshtastic_NodeInfoLite &node)
{
    if (node.user.short_name[0] != '\0') {
        return String(node.user.short_name);
    }
    if (node.user.long_name[0] != '\0') {
        return String(node.user.long_name);
    }
    char buf[12];
    snprintf(buf, sizeof(buf), "%04x", static_cast<unsigned>(node.num & 0xFFFFu));
    return String(buf);
}

static String getOnlineNodeShortId(const meshtastic_NodeInfoLite &node)
{
    char buf[20];
    snprintf(buf, sizeof(buf), "!%08lx", static_cast<unsigned long>(node.num));
    return String(buf);
}

static bool getOnlineBrowserRenderRow(void *, uint8_t index, String &label, String &status)
{
    const meshtastic_NodeInfoLite *node = gNodeBrowserDataSource.onlineNodeAt(index);
    if (!node) {
        return false;
    }
    label = getOnlineNodeDisplayName(*node) + " " + getOnlineNodeShortId(*node);
    status = formatOnlineNodeSeenAgo(*node);
    return true;
}

static bool getFinderBrowserRenderRow(void *, uint8_t index, String &label, String &status)
{
    const meshtastic_NodeInfoLite *node = gNodeBrowserDataSource.finderNodeAt(index);
    if (!node) {
        return false;
    }
    label = getOnlineNodeDisplayName(*node) + " " + getOnlineNodeShortId(*node);
    status = formatOnlineNodeSeenAgo(*node);
    return true;
}

static bool getGroupBrowserRenderRow(void *, uint8_t index, String &label, String &status)
{
    const auto *entry = getGroupNodeAt(index);
    if (!entry) {
        return false;
    }
    label = getGroupNodeDisplayName(*entry);
    if (entry->state[0]) {
        label += " ";
        label += entry->state;
    }
    status = hermesXEmUiModule ? String(hermesXEmUiModule->getNodePresenceLabel(*entry)) : String(u8"未知");
    return true;
}

static bool getTraceRouteBindRenderRow(void *, uint8_t index, String &label, String &status)
{
    const meshtastic_NodeInfoLite *node = gNodeBrowserDataSource.traceRouteNodeAt(index);
    if (!node) {
        return false;
    }
    label = getOnlineNodeDisplayName(*node) + " " + getOnlineNodeShortId(*node);
    status = isTraceRouteNodeBound(node->num) ? String(u8"已綁") : String("");
    return true;
}

static bool getTraceRouteBoundRenderRow(void *, uint8_t index, String &label, String &status)
{
    const meshtastic_NodeInfoLite *node = getBoundTraceRouteNodeAt(index);
    if (!node) {
        return false;
    }
    label = getOnlineNodeDisplayName(*node) + " " + getOnlineNodeShortId(*node);
    status = node->via_mqtt ? "--" : "TR";
    return true;
}

static String getDirectMessageTargetName(NodeNum nodeNum)
{
    if (nodeDB) {
        const meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(nodeNum);
        if (node && node->has_user) {
            if (node->user.short_name[0] != '\0') {
                return String(node->user.short_name);
            }
            if (node->user.long_name[0] != '\0') {
                return String(node->user.long_name);
            }
        }
    }
    char buf[12];
    snprintf(buf, sizeof(buf), "%04lx", static_cast<unsigned long>(nodeNum & 0xFFFFu));
    return String(buf);
}

static void startDirectMessageComposer(NodeNum destNode, bool fromGroupDetail)
{
    if (destNode == 0 || destNode == NODENUM_BROADCAST) {
        return;
    }
    gDirectMessageComposer.start(destNode, fromGroupDetail);
}

static void stopDirectMessageComposer()
{
    gDirectMessageComposer.stop();
}

static bool sendDirectTextMessage(NodeNum destNode, const String &message)
{
    if (!service || !router || destNode == 0 || destNode == NODENUM_BROADCAST || message.length() == 0) {
        return false;
    }

    meshtastic_MeshPacket *p = router->allocForSending();
    if (!p) {
        return false;
    }

    p->to = destNode;
    p->channel = nodeDB ? nodeDB->getMeshNodeChannel(destNode) : channels.getPrimaryIndex();
    p->want_ack = true;
    p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    const size_t payloadLen = std::min<size_t>(message.length(), meshtastic_Constants_DATA_PAYLOAD_LEN);
    p->decoded.payload.size = payloadLen;
    memcpy(p->decoded.payload.bytes, message.c_str(), payloadLen);
    service->sendToMesh(p, RX_SRC_LOCAL, true);
    return true;
}

static void drawDirectMessageComposerFrame(OLEDDisplay *display, int16_t x, int16_t y)
{
    if (!display) {
        return;
    }

    (void)x;
    (void)y;
    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    if (gDirectMessageComposer.candidateMode()) {
        HermesXMessageUiRenderer::drawComposerCandidates(
            display, width, height, gDirectMessageComposer.previewText(), gDirectMessageComposer.candidates(),
            gDirectMessageComposer.candidateCursor());
        return;
    }
    String header = String("MSG ") + getDirectMessageTargetName(gDirectMessageComposer.destination());
    const String draft = gDirectMessageComposer.previewText();
    HermesXFastSetupUiRenderer::drawKeyboardPage(
        display, width, height, header.c_str(), draft, gDirectMessageComposer.keyRows(),
        gDirectMessageComposer.keyRowLengths(), gDirectMessageComposer.keyRowCount(), gDirectMessageComposer.keyRow(),
        gDirectMessageComposer.keyCol(), gDirectMessageComposer.toast(), gDirectMessageComposer.toastUntilMs());
}

static String formatOnlineNodeSeenAgo(const meshtastic_NodeInfoLite &node)
{
    const uint32_t seconds = sinceLastSeen(&node);
    char buf[20];
    if (seconds < 60) {
        snprintf(buf, sizeof(buf), "%lus前", static_cast<unsigned long>(seconds));
    } else if (seconds < 3600) {
        snprintf(buf, sizeof(buf), "%lum前", static_cast<unsigned long>(seconds / 60U));
    } else {
        snprintf(buf, sizeof(buf), "%luh前", static_cast<unsigned long>(seconds / 3600U));
    }
    return String(buf);
}

static constexpr uint8_t kOnlineDetailMessageRow = 1;
static constexpr uint8_t kOnlineDetailTraceRouteRow = 2;

static uint8_t buildOnlineNodeDetailRows(const meshtastic_NodeInfoLite &node, bool finderMode, String *rows, uint8_t maxRows)
{
    uint8_t rowCount = 0;
    auto appendRow = [&](const String &row) {
        if (rows && rowCount < maxRows) {
            rows[rowCount] = row;
        }
        ++rowCount;
    };

    appendRow(finderMode ? u8"離開" : u8"返回");
    if (!finderMode) {
        appendRow("MSG");
        appendRow(node.via_mqtt ? "TraceRoute: --" : "TraceRoute");
    }
    appendRow(String("LongName: ") + (node.user.long_name[0] ? String(node.user.long_name) : String("--")));
    appendRow(String("LastHeard: ") + formatOnlineNodeSeenAgo(node));
    if (!finderMode) {
        appendRow(String("Link: ") + (node.via_mqtt ? String("MQTT") : String("LORA")));
    }
    appendRow(String("location"));

    if (node.position.latitude_i != 0 || node.position.longitude_i != 0) {
        char latBuf[24];
        char lonBuf[24];
        char altBuf[24];
        snprintf(latBuf, sizeof(latBuf), "%.7f", static_cast<double>(node.position.latitude_i) / 1e7);
        snprintf(lonBuf, sizeof(lonBuf), "%.7f", static_cast<double>(node.position.longitude_i) / 1e7);
        snprintf(altBuf, sizeof(altBuf), "%ldm", static_cast<long>(node.position.altitude));
        appendRow(String(u8"經: ") + lonBuf);
        appendRow(String(u8"緯: ") + latBuf);
        appendRow(String(u8"高度: ") + altBuf);

        meshtastic_NodeInfoLite *ourNode = nodeDB ? nodeDB->getMeshNode(nodeDB->getNodeNum()) : nullptr;
        if (ourNode && nodeDB->hasValidPosition(ourNode) && nodeDB->hasValidPosition(const_cast<meshtastic_NodeInfoLite *>(&node))) {
            const float ourLat = ourNode->position.latitude_i * 1e-7f;
            const float ourLon = ourNode->position.longitude_i * 1e-7f;
            const float theirLat = node.position.latitude_i * 1e-7f;
            const float theirLon = node.position.longitude_i * 1e-7f;
            const float distanceM = GeoCoord::latLongToMeter(ourLat, ourLon, theirLat, theirLon);
            const float bearingDeg = GeoCoord::bearing(ourLat, ourLon, theirLat, theirLon);
            const unsigned int bearingRounded = static_cast<unsigned int>(bearingDeg < 0 ? bearingDeg + 360.0f : bearingDeg) % 360U;
            char distBuf[24];
            char relativeBuf[48];
            if (distanceM >= 1000.0f) {
                snprintf(distBuf, sizeof(distBuf), "%.1fkm", distanceM / 1000.0f);
            } else {
                snprintf(distBuf, sizeof(distBuf), "%.0fm", distanceM);
            }
            snprintf(relativeBuf, sizeof(relativeBuf), "%s %s %u deg", distBuf, GeoCoord::degreesToBearing(bearingRounded),
                     bearingRounded);
            appendRow(String(u8"相對位置: ") + relativeBuf);
        } else {
            appendRow(String(u8"相對位置: --"));
        }
    } else {
        appendRow(String(u8"經: --"));
        appendRow(String(u8"緯: --"));
        appendRow(String(u8"高度: --"));
        appendRow(String(u8"相對位置: --"));
    }

    return rowCount;
}

static bool isHermesXRecentMessagePageActive();

static void clampRecentTextMessageIndices()
{
    HermesXMessageUiModel::instance().clampRecentIndices();
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
    HermesXMessageUiModel::instance().selectDetailFromList();
}

static void storeRecentTextMessage(const meshtastic_MeshPacket &packet)
{
    HermesXMessageUiModel::instance().storeRecentMessage(packet, isHermesXRecentMessagePageActive());
}

static bool isHermesXRecentMessagePageActive()
{
    return screen && (screen->isRecentTextMessagesPageActive() || screen->isRecentTextMessageDetailPageActive());
}

static meshtastic_NodeInfoLite *resolveMessageNode(const meshtastic_MeshPacket &packet);

static void drawIncomingTextPopupOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{
    if (!display || !isIncomingTextPopupVisible()) {
        return;
    }
    HermesXMessageRenderContext context;
    context.resolveNode = resolveMessageNode;
    HermesXMessageUiRenderer::drawIncomingPopup(display, state, context);
}

static meshtastic_NodeInfoLite *resolveMessageNode(const meshtastic_MeshPacket &packet)
{
    return nodeDB->getMeshNode(getFrom(&packet));
}

static const char *resolveMessageChannelName(uint8_t channel)
{
    return channels.getName(channel);
}

static bool drawMessageEmoji(OLEDDisplay *display,
                             int16_t x,
                             int16_t y,
                             int16_t width,
                             int16_t height,
                             const char *payload)
{
#ifndef EXCLUDE_EMOJI
    if (!display || !payload) {
        return false;
    }
    auto drawIcon = [&](const uint8_t *icon, int16_t iconWidth, int16_t iconHeight, int16_t extraY = 5) {
        display->drawXbm(x + (width - iconWidth) / 2,
                         y + (height - FONT_HEIGHT_MEDIUM - iconHeight) / 2 + 2 + extraY, iconWidth, iconHeight, icon);
    };
    if (strcmp(payload, "\U0001F44D") == 0) {
        drawIcon(thumbup, thumbs_width, thumbs_height);
    } else if (strcmp(payload, "\U0001F44E") == 0) {
        drawIcon(thumbdown, thumbs_width, thumbs_height);
    } else if (strcmp(payload, "\U0001F60A") == 0 || strcmp(payload, "\U0001F600") == 0 ||
               strcmp(payload, "\U0001F642") == 0 || strcmp(payload, "\U0001F609") == 0 ||
               strcmp(payload, "\U0001F601") == 0) {
        drawIcon(smiley, smiley_width, smiley_height);
    } else if (strcmp(payload, "\xE2\x80\xBC") == 0) {
        drawIcon(question, question_width, question_height);
    } else if (strcmp(payload, "\xE2\x80\xBC\xEF\xB8\x8F") == 0) {
        drawIcon(bang, bang_width, bang_height);
    } else if (strcmp(payload, "\U0001F4A9") == 0) {
        drawIcon(poo, poo_width, poo_height);
    } else if (strcmp(payload, "\U0001F923") == 0) {
        drawIcon(haha, haha_width, haha_height);
    } else if (strcmp(payload, "\U0001F44B") == 0) {
        drawIcon(wave_icon, wave_icon_width, wave_icon_height);
    } else if (strcmp(payload, "\U0001F920") == 0) {
        drawIcon(cowboy, cowboy_width, cowboy_height);
    } else if (strcmp(payload, "\U0001F42D") == 0) {
        drawIcon(deadmau5, deadmau5_width, deadmau5_height);
    } else if (strcmp(payload, "\xE2\x98\x80\xEF\xB8\x8F") == 0) {
        drawIcon(sun, sun_width, sun_height);
    } else if (strcmp(payload, "\u2614") == 0) {
        drawIcon(rain, rain_width, rain_height, 10);
    } else if (strcmp(payload, "\u2601") == 0) {
        drawIcon(cloud, cloud_width, cloud_height);
    } else if (strcmp(payload, "\U0001F32B") == 0) {
        drawIcon(fog, fog_width, fog_height);
    } else if (strcmp(payload, "\U0001F608") == 0) {
        drawIcon(devil, devil_width, devil_height);
    } else if (strcmp(payload, "\xE2\x9D\xA4") == 0 || strcmp(payload, "\xE2\x9D\xA4\xEF\xB8\x8F") == 0 ||
               strcmp(payload, "\U0001F9E1") == 0 || strcmp(payload, "\u2763") == 0 ||
               strcmp(payload, "\U00002764") == 0 || strcmp(payload, "\U0001F495") == 0 ||
               strcmp(payload, "\U0001F496") == 0 || strcmp(payload, "\U0001F497") == 0 ||
               strcmp(payload, "\U0001F498") == 0) {
        drawIcon(heart, heart_width, heart_height);
    } else {
        return false;
    }
    return true;
#else
    (void)display;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)payload;
    return false;
#endif
}

static void drawRecentTextMessagesFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    HermesXMessageRenderContext context;
    context.inverted = config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_INVERTED;
    context.resolveNode = resolveMessageNode;
    context.channelName = resolveMessageChannelName;
    HermesXMessageUiRenderer::drawRecentList(display, state, x, y, context);
}

void Screen::drawOnlineNodeListFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    (void)state;
    gNodeBrowserDataSource.refreshOnline();
    const HermesXNodeBrowserRowSource rows(gOnlineNodeState.count, nullptr, getOnlineBrowserRenderRow);
    HermesXNodeBrowserUiRenderer::drawList(
        display, x, y, "ONLINE", u8"返回", u8"沒有在線節點", gOnlineNodeState.listCursor, rows, 3, false);
}

void Screen::drawFinderNodeListFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    (void)state;
    if (screen && screen->hermesFinderUiMode == Screen::HermesFinderUiMode::Menu) {
        static const char *items[] = {u8"離開", u8"發送尋人訊號", u8"節點列表"};
        HermesXNodeBrowserUiRenderer::drawMenu(
            display, x, y, u8"尋人模式", items, 3, screen->hermesFinderMenuSelected, true, 4, 6);
        return;
    }

    gNodeBrowserDataSource.refreshFinder();
    const HermesXNodeBrowserRowSource rows(gFinderNodeState.count, nullptr, getFinderBrowserRenderRow);
    HermesXNodeBrowserUiRenderer::drawList(
        display, x, y, u8"尋人清單", u8"離開", u8"沒有節點", gFinderNodeState.listCursor, rows, 3, true);
}

void Screen::drawOnlineNodeDetailFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    (void)state;
    if (gDirectMessageComposer.active() && !gDirectMessageComposer.fromGroupDetail()) {
        drawDirectMessageComposerFrame(display, x, y);
        return;
    }
    const meshtastic_NodeInfoLite *node = gNodeBrowserDataSource.selectedOnlineNode();
    if (!node) {
        HermesXNodeBrowserUiRenderer::drawDetail(
            display, x, y, "ONLINE DETAIL", "No node", nullptr, 0, gOnlineNodeState.detailCursor, false);
        return;
    }

    String rows[12];
    const uint8_t rowCount = buildOnlineNodeDetailRows(*node, false, rows, sizeof(rows) / sizeof(rows[0]));
    if (rowCount > 0 && gOnlineNodeState.detailCursor >= rowCount) {
        gOnlineNodeState.detailCursor = rowCount - 1;
    }
    HermesXNodeBrowserUiRenderer::drawDetail(
        display, x, y, "ONLINE DETAIL", "No node", rows, rowCount, gOnlineNodeState.detailCursor, false);
}

void Screen::drawTraceRouteNodeListFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    (void)state;
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);

    if (gTraceRouteSearchState.active) {
        const String preview = gTraceRouteSearchState.draft.length() == 0
                                   ? String("_")
                                   : gTraceRouteSearchState.draft + "_";
        HermesXFastSetupUiRenderer::drawKeyboardPage(
            display, display->getWidth(), display->getHeight(), u8"搜尋裝置 ShortName", preview,
            HermesXTraceRouteUiController::instance().searchKeyRows(),
            HermesXTraceRouteUiController::instance().searchKeyRowLengths(),
            HermesXTraceRouteUiController::instance().searchKeyRowCount(), gTraceRouteSearchState.keyRow,
            gTraceRouteSearchState.keyCol, gTraceRouteSearchState.toast, gTraceRouteSearchState.toastUntilMs);
        return;
    }

    if (gTraceRouteSearchState.resultVisible) {
        const meshtastic_NodeInfoLite *resultNode =
            gTraceRouteSearchState.resultFound ? gNodeBrowserDataSource.nodeByNum(gTraceRouteSearchState.resultNode) : nullptr;
        const String shortName = resultNode ? String(resultNode->user.short_name) : String();
        const String longName = resultNode ? String(resultNode->user.long_name) : String();
        HermesXTraceRouteUiRenderer::drawSearchResult(display, x, y, resultNode != nullptr, shortName, longName);
        return;
    }

    if (gTraceRouteNavigationState.mode == HermesXTraceRouteUiMode::Menu) {
        HermesXTraceRouteUiRenderer::drawMenu(display, x, y);
        drawSetupDetailPopupOverlay(display, state);
        drawTraceRoutePopupOverlay(display, state);
        return;
    }

    gNodeBrowserDataSource.refreshTraceRoute();
    const HermesXTraceRouteRowSource rows{gNodeBrowserDataSource.traceRouteCount(), nullptr, getTraceRouteBindRenderRow};
    HermesXTraceRouteUiRenderer::drawBindList(display, x, y, rows);
    drawSetupDetailPopupOverlay(display, state);
    drawTraceRoutePopupOverlay(display, state);
}

void Screen::drawTraceRouteNodeDetailFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    syncTraceRouteBoundSelection();
    const HermesXTraceRouteRowSource rows{gTraceRouteBindings.count(), nullptr, getTraceRouteBoundRenderRow};
    HermesXTraceRouteUiRenderer::drawBoundList(display, x, y, rows);
    drawSetupDetailPopupOverlay(display, state);
    drawTraceRoutePopupOverlay(display, state);
}

void Screen::drawFinderNodeDetailFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    (void)state;
    const meshtastic_NodeInfoLite *node = gNodeBrowserDataSource.selectedFinderNode();
    if (!node) {
        HermesXNodeBrowserUiRenderer::drawDetail(
            display, x, y, u8"尋人清單", "No node", nullptr, 0, gFinderNodeState.detailCursor, true);
        return;
    }

    String rows[12];
    const uint8_t rowCount = buildOnlineNodeDetailRows(*node, true, rows, sizeof(rows) / sizeof(rows[0]));
    if (rowCount > 0 && gFinderNodeState.detailCursor >= rowCount) {
        gFinderNodeState.detailCursor = rowCount - 1;
    }
    HermesXNodeBrowserUiRenderer::drawDetail(
        display, x, y, u8"尋人清單", "No node", rows, rowCount, gFinderNodeState.detailCursor, true);
}

void Screen::drawGroupNodeListFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    (void)state;
    if (!gGroupNodeState.nodeListVisible) {
        static const char *kGroupMenuItems[] = {u8"返回", u8"GROUP設定", u8"節點列表"};
        HermesXNodeBrowserUiRenderer::drawMenu(
            display, x, y, "GROUP", kGroupMenuItems, 3, gGroupNodeState.menuCursor, false, 3, 2);
        return;
    }

    clampGroupNodeState();
    const uint8_t count = static_cast<uint8_t>(std::max(0, getGroupNodeCount()));
    const HermesXNodeBrowserRowSource rows(count, nullptr, getGroupBrowserRenderRow, true);
    HermesXNodeBrowserUiRenderer::drawList(
        display, x, y, "GROUP", u8"返回", u8"沒有已配對節點", gGroupNodeState.listCursor, rows, 4, false);
}

void Screen::drawGroupNodeDetailFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    (void)state;
    if (gDirectMessageComposer.active() && gDirectMessageComposer.fromGroupDetail()) {
        drawDirectMessageComposerFrame(display, x, y);
        return;
    }
    const auto *entry = getSelectedGroupNode();
    if (!entry) {
        HermesXNodeBrowserUiRenderer::drawDetail(
            display, x, y, "GROUP DETAIL", u8"無節點資料", nullptr, 0, gGroupNodeState.detailCursor, false);
        return;
    }

    String rows[16];
    const uint8_t rowCount = buildGroupNodeDetailRows(*entry, rows, sizeof(rows) / sizeof(rows[0]));
    if (gGroupNodeState.detailCursor >= rowCount) {
        gGroupNodeState.detailCursor = rowCount > 0 ? rowCount - 1 : 0;
    }

    HermesXNodeBrowserUiRenderer::drawDetail(
        display, x, y, "GROUP DETAIL", u8"無節點資料", rows, rowCount, gGroupNodeState.detailCursor, false);
}

/// Draw the last text message we received
static void drawTextMessageFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    const meshtastic_MeshPacket *packet = getActiveTextMessageForDisplay();
    char timeLabel[64] = {0};
    if (packet) {
        const uint32_t seconds = sinceReceived(packet);
        const uint32_t minutes = seconds / 60;
        const uint32_t hours = minutes / 60;
        const uint32_t days = hours / 24;
        uint8_t timestampHours = 0;
        uint8_t timestampMinutes = 0;
        int32_t daysAgo = 0;
        const bool useTimestamp = deltaToTimestamp(seconds, &timestampHours, &timestampMinutes, &daysAgo);
        if (useTimestamp && minutes >= 15 && daysAgo == 0) {
            snprintf(timeLabel, sizeof(timeLabel), "At %02hu:%02hu", timestampHours, timestampMinutes);
        } else if (useTimestamp && daysAgo == 1 && display && display->getWidth() >= 200) {
            snprintf(timeLabel, sizeof(timeLabel), "Yesterday %02hu:%02hu", timestampHours, timestampMinutes);
        } else {
            snprintf(timeLabel, sizeof(timeLabel), "%s ago", screen->drawTimeDelta(days, hours, minutes, seconds).c_str());
        }
    }

    HermesXMessageRenderContext context;
    context.inverted = config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_INVERTED;
    context.resolveNode = resolveMessageNode;
    context.drawEmoji = drawMessageEmoji;
    HermesXMessageUiRenderer::drawDetail(display, state, x, y, packet, timeLabel, context);
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

static void drawTraceRoutePopupOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{
    (void)state;
    HermesXTraceRouteUiRenderer::drawPopup(display);
}

static void drawSetupDetailPopupOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{
    (void)state;
    if (!display || !isSetupDetailPopupVisible()) {
        return;
    }
    HermesXFastSetupUiRenderer::drawDetailPopup(display, gSetupDetailPopupState.title, gSetupDetailPopupState.body,
                                                gSetupDetailPopupState.scrollY, gSetupDetailPopupState.maxScrollY);
}
void Screen::drawLowMemoryReminderOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{
    (void)state;
    graphics::HermesXLowMemoryUiRenderer::draw(display, gLowMemoryUiState, gLowMemoryProtectionActive);
}


static void drawHermesGpsHeroFrame(OLEDDisplay *display, int16_t x, int16_t y, const GPSStatus *gps)
{
    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    const bool posterTftLayout = supportsDirectTftOverlayRendering(display);

    if (posterTftLayout) {
        const bool gpsEnabled = config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_ENABLED;
        HermesXGpsUiRenderer::drawPosterBase(display, x, y, width, height, gpsEnabled, addTftColorZone);

        // Coordinates are rendered after ui->update() via direct-TFT neon (same pipeline as Home timer).
        (void)gps;

        return;
    }

    const bool largeLayout = (width >= 240 && height >= 130);
    const bool compactLayout = (width < 220 || height < 120);
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

    const HermesXGpsStatusView view(gps->getNumSatellites(),
                                    gpsTitle,
                                    lockStatus,
                                    lockStatusShort,
                                    lonLine,
                                    latLine,
                                    dateLine,
                                    timeLine,
                                    compactTimeLine);
    HermesXGpsUiRenderer::drawStatusFrame(display, x, y, view);
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

constexpr int16_t kSetupRowHeight = 12;
constexpr uint8_t kSetupVisibleRows = 4;
constexpr size_t kSetupPassMaxLen = 20;
constexpr size_t kSetupWifiSsidMaxLen = 32;
constexpr size_t kSetupWifiPasswordMaxLen = 64;
constexpr uint32_t kSetupNavMinIntervalMs = 80;
constexpr uint32_t kSetupNavFlipGuardMs = 800;
constexpr uint8_t kMainActionVisibleSlots = 3;
constexpr uint8_t kMainActionIdCount = 15;
constexpr uint8_t kMainActionPrimaryCount = 8;
constexpr uint8_t kMainActionFeatureCount = 7;
constexpr uint8_t kMainActionHomeIndex = 5;
constexpr uint8_t kMainActionFeatureEntryIndex = 3;
constexpr uint8_t kMainActionFeatureId = 13;
constexpr uint8_t kMainActionFeatureExitId = 14;
static const uint8_t kMainActionPrimaryOrder[kMainActionPrimaryCount] = {0, 1, 2, kMainActionFeatureId, 4, 5, 6, 7};
static const uint8_t kMainActionFeatureOrder[kMainActionFeatureCount] = {3, 8, 9, 10, 11, 12, kMainActionFeatureExitId};
constexpr uint32_t kStealthConfirmArmMs = 3000;
constexpr uint32_t kStealthWakeMs = 1000;
constexpr uint32_t kLowMemoryReminderSuppressMs = 5 * 60 * 1000;
static const char *kSetupRootItems[] = {u8"返回", u8"GROUP設定", u8"UI設定", u8"裝置管理", u8"罐頭訊息",
                                        u8"儲存並重新開機"};
static const uint8_t kSetupRootCount = sizeof(kSetupRootItems) / sizeof(kSetupRootItems[0]);
#if HERMESX_CIV_DISABLE_EMAC
static const char *kSetupEmacItems[] = {u8"返回", u8"EMINFO設定", "GROUP PIN A", "GROUP PIN B", u8"查看GROUP PIN"};
#else
static const char *kSetupEmacItems[] = {u8"返回", u8"EMINFO設定", "GROUP PIN A", "GROUP PIN B", u8"查看GROUP PIN",
                                        u8"解除EMAC"};
#endif
static const uint8_t kSetupEmacCount = sizeof(kSetupEmacItems) / sizeof(kSetupEmacItems[0]);
static const char *kSetupEmInfoItems[] = {u8"返回", u8"EMINFO廣播", u8"EMINFO週期", u8"Heartbeat週期", u8"離線門檻",
                                          u8"附帶電量"};
static const uint8_t kSetupEmInfoCount = sizeof(kSetupEmInfoItems) / sizeof(kSetupEmInfoItems[0]);
static const uint32_t kSetupEmInfoIntervalOptions[] = {0, 5, 10, 30, 60, 120, 300};
static const char *kSetupEmInfoIntervalLabels[] = {u8"沿用系統", "5s", "10s", "30s", "60s", "120s", "300s"};
static const uint8_t kSetupEmInfoIntervalCount = sizeof(kSetupEmInfoIntervalOptions) / sizeof(kSetupEmInfoIntervalOptions[0]);
static const uint32_t kSetupHeartbeatIntervalOptions[] = {0, 5, 10, 15, 30, 60};
static const char *kSetupHeartbeatIntervalLabels[] = {u8"關閉", "5s", "10s", "15s", "30s", "60s"};
static const uint8_t kSetupHeartbeatIntervalCount =
    sizeof(kSetupHeartbeatIntervalOptions) / sizeof(kSetupHeartbeatIntervalOptions[0]);
static const uint8_t kSetupOfflineThresholdOptions[] = {2, 3, 4, 5, 6};
static const char *kSetupOfflineThresholdLabels[] = {"2x", "3x", "4x", "5x", "6x"};
static const uint8_t kSetupOfflineThresholdCount =
    sizeof(kSetupOfflineThresholdOptions) / sizeof(kSetupOfflineThresholdOptions[0]);
static const uint8_t kSetupNodeMenuCount = 10;
static const uint8_t kSetupDeviceInfoMenuCount = 6;
static const uint8_t kSetupUpdateEntryMenuCount = 2;
static const uint8_t kSetupUpdateMenuCount = 5;
static constexpr uint32_t kSetupUpdateIntroMs = 900;
static constexpr uint32_t kSetupUpdateExitRebootMs = 1200;
static constexpr uint32_t kTakModeTransitionRebootMs = 1500;
static const uint8_t kSetupUpdateCheckMenuCount = 8;
static const uint8_t kSetupUpdateCheckFlowCount = 2;
static const uint8_t kSetupUpdateRuntimeMenuCount = 3;
static const uint8_t kSetupUpdateWifiConfigMenuCount = 6;
static const uint8_t kSetupUpdateWifiMenuCount = 4;
static const uint8_t kSetupUpdateUploadMenuCount = 4;
static const uint8_t kSetupUpdateApplyMenuCount = 2;
static const uint8_t kSetupPowerMenuCount = 4;
static const uint8_t kSetupUiMenuCount = 8;
static const uint8_t kSetupMqttMenuCount = 4;
static const uint8_t kSetupNodeDatabaseMenuCount = 2;
static const uint8_t kSetupNodeDatabaseResetCount = 5;
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
static const uint32_t kSetupNodeInfoBroadcastOptions[] = {3600, 3 * 3600, 6 * 3600, 12 * 3600, ONE_DAY};
static const char *kSetupNodeInfoBroadcastLabels[] = {"1h", "3h", "6h", "12h", "24h"};
static const uint8_t kSetupNodeInfoBroadcastCount =
    sizeof(kSetupNodeInfoBroadcastOptions) / sizeof(kSetupNodeInfoBroadcastOptions[0]);
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

static const uint32_t kTakNodeInfoOptions[] = {ONE_DAY, 3600, 1800, 600, 300, 60};
static const char *kTakNodeInfoLabels[] = {"24h", "1h", "30m", "10m", "5m", "1m"};
static const uint8_t kTakNodeInfoCount = sizeof(kTakNodeInfoOptions) / sizeof(kTakNodeInfoOptions[0]);
static const uint32_t kTakGpsUpdateOptions[] = {5, 15, 30, 60, 120};
static const char *kTakGpsUpdateLabels[] = {"5s", "15s", "30s", "60s", "120s"};
static const uint8_t kTakGpsUpdateCount = sizeof(kTakGpsUpdateOptions) / sizeof(kTakGpsUpdateOptions[0]);
static const uint32_t kTakPositionBroadcastOptions[] = {20, 30, 60, 120, 300};
static const char *kTakPositionBroadcastLabels[] = {"20s", "30s", "60s", "120s", "300s"};
static const uint8_t kTakPositionBroadcastCount =
    sizeof(kTakPositionBroadcastOptions) / sizeof(kTakPositionBroadcastOptions[0]);
static const uint32_t kTakSmartDistanceOptions[] = {10, 20, 50, 100};
static const char *kTakSmartDistanceLabels[] = {"10m", "20m", "50m", "100m"};
static const uint8_t kTakSmartDistanceCount = sizeof(kTakSmartDistanceOptions) / sizeof(kTakSmartDistanceOptions[0]);
static const uint32_t kTakSmartIntervalOptions[] = {15, 30, 60, 120};
static const char *kTakSmartIntervalLabels[] = {"15s", "30s", "60s", "120s"};
static const uint8_t kTakSmartIntervalCount = sizeof(kTakSmartIntervalOptions) / sizeof(kTakSmartIntervalOptions[0]);
static const uint32_t kTakMissionSlotOptions[] = {0, 5, 9, 17, 19, 3};
static const char *kTakMissionSlotLabels[] = {u8"自動", "TAK A", "TAK B", "TAK C", "TAK D", "TAK E"};
static const uint8_t kTakMissionSlotCount = sizeof(kTakMissionSlotOptions) / sizeof(kTakMissionSlotOptions[0]);
static constexpr uint8_t kTakPopupRowCount = 7;
static constexpr uint8_t kTakSettingsRowCount = 11;

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
static const char *kSetupWifiKeyRowsUpper[][10] = {
    {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
    {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},
    {"A", "S", "D", "F", "G", "H", "J", "K", "L", "Aa"},
    {"Z", "X", "C", "V", "B", "N", "M", ".", "-", "_"},
    {"@", "/", "!", "?", "SP", "DEL", "OK", nullptr, nullptr, nullptr},
};
static const char *kSetupWifiKeyRowsLower[][10] = {
    {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
    {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"},
    {"a", "s", "d", "f", "g", "h", "j", "k", "l", "Aa"},
    {"z", "x", "c", "v", "b", "n", "m", ".", "-", "_"},
    {"@", "/", "!", "?", "SP", "DEL", "OK", nullptr, nullptr, nullptr},
};
static const uint8_t kSetupWifiKeyRowLengths[] = {10, 10, 10, 10, 7};
static const uint8_t kSetupWifiKeyRowCount = sizeof(kSetupWifiKeyRowLengths) / sizeof(kSetupWifiKeyRowLengths[0]);
static const char *kSetupNumericKeyRows[][10] = {
    {"1", "2", "3", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"4", "5", "6", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"7", "8", "9", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
    {".", "0", "DEL", "OK", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
};
static const uint8_t kSetupNumericKeyRowLengths[] = {3, 3, 3, 4};
static const uint8_t kSetupNumericKeyRowCount = sizeof(kSetupNumericKeyRowLengths) / sizeof(kSetupNumericKeyRowLengths[0]);

static String maskSetupSecret(const char *value, size_t visibleTail = 0)
{
    if (!value || value[0] == '\0') {
        return u8"未設定";
    }
    const String raw(value);
    if (visibleTail > raw.length()) {
        visibleTail = raw.length();
    }
    String masked;
    const size_t hidden = raw.length() - visibleTail;
    for (size_t i = 0; i < hidden; ++i) {
        masked += '*';
    }
    if (visibleTail > 0) {
        masked += raw.substring(raw.length() - visibleTail);
    }
    return masked;
}

static const char *const (*getSetupWifiKeyRows(bool lowercase))[10]
{
    return lowercase ? kSetupWifiKeyRowsLower : kSetupWifiKeyRowsUpper;
}

static String getSetupWifiIpLabel()
{
#if HAS_WIFI && !defined(ARCH_PORTDUINO)
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString().c_str();
    }
#endif
    return u8"-";
}

static String getSetupUsbStatusLabel()
{
    return u8"等待電腦傳送";
}

static String makeSetupDraftPreview(const String &draft)
{
    String preview = draft;
    preview += '|';
    return preview;
}

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

static const char *getTakOptionLabel(uint32_t value, const uint32_t *options, const char *const *labels, uint8_t count)
{
    for (uint8_t i = 0; i < count; ++i) {
        if (options[i] == value) {
            return labels[i];
        }
    }
    return u8"自訂";
}

static void cycleTakOption(uint32_t &value, const uint32_t *options, uint8_t count)
{
    if (!options || count == 0) {
        return;
    }
    uint8_t index = 0;
    for (uint8_t i = 0; i < count; ++i) {
        if (options[i] == value) {
            index = i;
            break;
        }
    }
    value = options[(index + 1) % count];
}

static int8_t getTakSmartPowerMinDbm()
{
    return HermesXInterfaceModule::instance ? HermesXInterfaceModule::instance->getSmartPowerMinDbm() : 14;
}

static int8_t getTakSmartPowerMaxDbm()
{
    return HermesXInterfaceModule::instance ? HermesXInterfaceModule::instance->getSmartPowerMaxDbm() : 22;
}

static bool isTakModeActive();
static bool isTakExperienceActive();

static bool isSmartPowerHomeActive()
{
    return isTakExperienceActive() ||
           (HermesXInterfaceModule::instance && HermesXInterfaceModule::instance->isSmartPowerActive());
}

static bool shouldShowHermesXHomeFrame()
{
    return config.device.role == meshtastic_Config_DeviceConfig_Role_CLIENT ||
           config.device.role == meshtastic_Config_DeviceConfig_Role_CLIENT_MUTE ||
           config.device.role == meshtastic_Config_DeviceConfig_Role_CLIENT_HIDDEN ||
           isSmartPowerHomeActive();
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

static String getSetupNodeNumLabel()
{
    if (!nodeDB) {
        return u8"未知";
    }
    char buf[12];
    snprintf(buf, sizeof(buf), "!%08x", nodeDB->getNodeNum());
    return String(buf);
}

static uint32_t getSetupCurrentNodeInfoBroadcast()
{
    return Default::getConfiguredOrDefault(config.device.node_info_broadcast_secs, default_node_info_broadcast_secs);
}

static const char *getSetupNodeInfoBroadcastLabel(uint32_t seconds)
{
    for (uint8_t i = 0; i < kSetupNodeInfoBroadcastCount; ++i) {
        if (kSetupNodeInfoBroadcastOptions[i] == seconds) {
            return kSetupNodeInfoBroadcastLabels[i];
        }
    }
    return nullptr;
}

static uint8_t getSetupNodeInfoBroadcastSelection(uint32_t seconds)
{
    for (uint8_t i = 0; i < kSetupNodeInfoBroadcastCount; ++i) {
        if (kSetupNodeInfoBroadcastOptions[i] == seconds) {
            return i + 1;
        }
    }
    return 1;
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

static uint8_t getSetupEmInfoIntervalSelection(uint32_t value)
{
    for (uint8_t i = 0; i < kSetupEmInfoIntervalCount; ++i) {
        if (kSetupEmInfoIntervalOptions[i] == value) {
            return i + 1;
        }
    }
    return 1;
}

static uint8_t getSetupHeartbeatIntervalSelection(uint32_t value)
{
    for (uint8_t i = 0; i < kSetupHeartbeatIntervalCount; ++i) {
        if (kSetupHeartbeatIntervalOptions[i] == value) {
            return i + 1;
        }
    }
    return 1;
}

static uint8_t getSetupOfflineThresholdSelection(uint8_t value)
{
    for (uint8_t i = 0; i < kSetupOfflineThresholdCount; ++i) {
        if (kSetupOfflineThresholdOptions[i] == value) {
            return i + 1;
        }
    }
    return 2;
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

static constexpr const char *kUpdateBootFlagPath = "/prefs/hermesx_update_boot.bin";
static constexpr uint32_t kUpdateBootFlagMagic = 0x48585542; // HXUB

struct UpdateBootFlagState {
    uint32_t magic = kUpdateBootFlagMagic;
};

static bool gUpdateBootRequested = false;
static bool gUpdateBootHandled = false;

struct UpdateLowLoadRuntimeState {
    bool active = false;
    bool gpsWasEnabled = false;
    bool bluetoothWasEnabled = false;
    bool radioWasEnabled = false;
    bool nodeInfoWasActive = false;
    bool serialWasEnabled = false;
};
static UpdateLowLoadRuntimeState gUpdateLowLoadRuntimeState;

static bool loadUpdateBootFlag()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    auto f = FSCom.open(kUpdateBootFlagPath, FILE_O_READ);
    if (!f) {
        return false;
    }
    UpdateBootFlagState state = {};
    const size_t readLen = f.read(reinterpret_cast<uint8_t *>(&state), sizeof(state));
    f.close();
    return readLen == sizeof(state) && state.magic == kUpdateBootFlagMagic;
#else
    return false;
#endif
}

static bool saveUpdateBootFlag()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    if (!FSCom.exists("/prefs")) {
        FSCom.mkdir("/prefs");
    }
    auto f = FSCom.open(kUpdateBootFlagPath, FILE_O_WRITE);
    if (!f) {
        return false;
    }
    UpdateBootFlagState state = {};
    const size_t written = f.write(reinterpret_cast<const uint8_t *>(&state), sizeof(state));
    f.close();
    return written == sizeof(state);
#else
    return false;
#endif
}

static void clearUpdateBootFlag()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    if (FSCom.exists(kUpdateBootFlagPath)) {
        FSCom.remove(kUpdateBootFlagPath);
    }
#endif
}

#ifndef RTC_NOINIT_ATTR
#define RTC_NOINIT_ATTR
#endif

static constexpr uint32_t kStealthRetainedMagic = 0x4853544CUL; // HSTL
static constexpr uint16_t kStealthRetainedVersion = 1;
static constexpr const char *kStealthStateFile = "/prefs/hermesx_stealth_state.bin";
static constexpr uint32_t kTakRetainedMagic = 0x4854414BUL; // HTAK
static constexpr uint16_t kTakRetainedVersion = 4;
static constexpr const char *kTakStateFile = "/prefs/hermesx_tak_state.bin";
static constexpr uint32_t kTakProfileMagic = 0x48545046UL; // HTPF
static constexpr uint16_t kTakProfileVersion = 2;
static constexpr const char *kTakProfileFile = "/prefs/hermesx_tak_profile.bin";

struct TakModeProfile {
    uint32_t magic = kTakProfileMagic;
    uint16_t version = kTakProfileVersion;
    uint16_t stateSize = sizeof(TakModeProfile);
    uint32_t nodeInfoBroadcastSecs = ONE_DAY;
    uint32_t gpsUpdateIntervalSecs = 5;
    uint32_t positionBroadcastSecs = 20;
    uint32_t smartMinimumDistanceMeters = 10;
    uint32_t smartMinimumIntervalSecs = 15;
    uint32_t missionSlot = 5;
    bool quietOutputs = true;
    bool allowEmUi = true;
    bool allowFinder = true;
};
static TakModeProfile gTakModeProfile;
static bool gTakModeProfileLoaded = false;

static auto &gTakModeUiModel = graphics::HermesXTakModeUiModel::instance();
static auto &gTakModeUiState = gTakModeUiModel.state();

struct StealthRetainedState {
    uint32_t magic;
    uint16_t version;
    uint16_t stateSize;
    StealthRuntimeState state;
};
RTC_NOINIT_ATTR static StealthRetainedState gStealthRetainedState;

struct TakRuntimeState {
    bool active = false;
    bool quietOutputsApplied = false;
    bool emergencyLampEnabled = false;
    bool sirenEnabled = false;
    bool ledHeartbeatDisabled = false;
    uint8_t uiLedBrightness = 60;
    meshtastic_Config_DeviceConfig_Role previousRole = meshtastic_Config_DeviceConfig_Role_CLIENT;
    uint32_t nodeInfoBroadcastSecs = 0;
    uint32_t gpsUpdateIntervalSecs = 0;
    bool positionBroadcastSmartEnabled = false;
    uint32_t positionBroadcastSecs = 0;
    uint32_t smartMinimumDistanceMeters = 0;
    uint32_t smartMinimumIntervalSecs = 0;
    uint32_t positionFlags = 0;
    uint32_t telemetryDeviceUpdateInterval = 0;
    bool loraUsePreset = true;
    meshtastic_Config_LoRaConfig_ModemPreset loraModemPreset = meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST;
    uint16_t loraBandwidth = 0;
    uint32_t loraSpreadFactor = 0;
    uint8_t loraCodingRate = 0;
    uint32_t loraChannelNum = 0;
    float loraOverrideFrequency = 0.0f;
};
static TakRuntimeState gTakRuntimeState;

struct TakRetainedState {
    uint32_t magic;
    uint16_t version;
    uint16_t stateSize;
    TakRuntimeState state;
};
RTC_NOINIT_ATTR static TakRetainedState gTakRetainedState;

static bool isStealthModeActive()
{
    return gStealthRuntimeState.active;
}

static bool isTakModeActive()
{
    return gTakRuntimeState.active;
}

static bool isTakExperienceActive()
{
    return isTakModeActive() || config.device.role == meshtastic_Config_DeviceConfig_Role_TAK ||
           config.device.role == meshtastic_Config_DeviceConfig_Role_TAK_TRACKER;
}

static const char *getTakExperienceTitle()
{
    return config.device.role == meshtastic_Config_DeviceConfig_Role_TAK_TRACKER ? "TAK TRACKER" : "TAK MODE";
}

static const char *getTakMissionSlotLabel(uint32_t slot)
{
    for (uint8_t i = 0; i < kTakMissionSlotCount; ++i) {
        if (kTakMissionSlotOptions[i] == slot) {
            return kTakMissionSlotLabels[i];
        }
    }
    return "Slot";
}

static uint32_t getTakEffectiveMissionSlot()
{
    const uint16_t slotCount = getSetupLoraChannelSlotCount();
    if (gTakModeProfile.missionSlot == 0 || slotCount == 0) {
        return 0;
    }
    if (gTakModeProfile.missionSlot > slotCount) {
        return slotCount;
    }
    return gTakModeProfile.missionSlot;
}

static float getSetupLoraSlotFrequencyMhz(uint32_t slot)
{
    const RegionInfo *region = findSetupRegionInfo(config.lora.region);
    if (!region) {
        region = findSetupRegionInfo(meshtastic_Config_LoRaConfig_RegionCode_UNSET);
    }
    if (!region || slot == 0) {
        return 0.0f;
    }

    const float bandwidthKhz = getSetupCurrentLoraBandwidthKhz();
    if (bandwidthKhz <= 0.0f) {
        return 0.0f;
    }

    return region->freqStart + (bandwidthKhz / 2000.0f) + ((slot - 1) * (bandwidthKhz / 1000.0f));
}

static String getTakMissionSlotDisplayLabel(uint32_t slot, bool includeFrequency)
{
    String label = getTakMissionSlotLabel(slot);
    const uint32_t effectiveSlot = slot == 0 ? config.lora.channel_num : slot;
    if (effectiveSlot != 0) {
        label += " S";
        label += String(effectiveSlot);
    }
    if (includeFrequency && effectiveSlot != 0) {
        const float freq = getSetupLoraSlotFrequencyMhz(effectiveSlot);
        if (freq > 0.0f) {
            label += " ";
            label += formatSetupFrequencyLabel(freq);
        }
    }
    return label;
}

static const char *getTakAirtimeStatusLabel(float channelUtil)
{
    if (channelUtil >= 70.0f) {
        return u8"嚴重擁塞";
    }
    if (channelUtil >= 50.0f) {
        return u8"擁塞";
    }
    if (channelUtil >= 20.0f) {
        return u8"偏忙";
    }
    return u8"正常";
}

static uint32_t getTakSuggestedMissionSlot()
{
    const uint16_t slotCount = getSetupLoraChannelSlotCount();
    if (slotCount == 0) {
        return 0;
    }

    const uint32_t current = config.lora.channel_num ? config.lora.channel_num : getTakEffectiveMissionSlot();
    for (uint8_t i = 1; i < kTakMissionSlotCount; ++i) {
        const uint32_t candidate = kTakMissionSlotOptions[i];
        if (candidate != 0 && candidate <= slotCount && candidate != current) {
            return candidate;
        }
    }
    return 0;
}

static bool isTakDeviceRole(meshtastic_Config_DeviceConfig_Role role)
{
    return role == meshtastic_Config_DeviceConfig_Role_TAK || role == meshtastic_Config_DeviceConfig_Role_TAK_TRACKER;
}

static meshtastic_Config_DeviceConfig_Role sanitizeTakPreviousRole(meshtastic_Config_DeviceConfig_Role role)
{
    return isTakDeviceRole(role) ? meshtastic_Config_DeviceConfig_Role_CLIENT : role;
}

static bool isValidStealthRetainedState(const StealthRetainedState &state)
{
    return state.magic == kStealthRetainedMagic && state.version == kStealthRetainedVersion &&
           state.stateSize == sizeof(StealthRuntimeState) && state.state.active;
}

static bool isValidTakRetainedState(const TakRetainedState &state)
{
    return state.magic == kTakRetainedMagic && state.version == kTakRetainedVersion &&
           state.stateSize == sizeof(TakRuntimeState) && state.state.active;
}

static bool isValidTakModeProfile(const TakModeProfile &profile)
{
    return profile.magic == kTakProfileMagic && profile.version == kTakProfileVersion &&
           profile.stateSize == sizeof(TakModeProfile);
}

static void normalizeTakModeProfile(TakModeProfile &profile)
{
    profile.magic = kTakProfileMagic;
    profile.version = kTakProfileVersion;
    profile.stateSize = sizeof(TakModeProfile);
    if (profile.nodeInfoBroadcastSecs == 0) {
        profile.nodeInfoBroadcastSecs = ONE_DAY;
    }
    if (profile.gpsUpdateIntervalSecs == 0) {
        profile.gpsUpdateIntervalSecs = 5;
    }
    if (profile.positionBroadcastSecs == 0) {
        profile.positionBroadcastSecs = 20;
    }
    if (profile.smartMinimumDistanceMeters == 0) {
        profile.smartMinimumDistanceMeters = 10;
    }
    if (profile.smartMinimumIntervalSecs == 0) {
        profile.smartMinimumIntervalSecs = 15;
    }
    bool validMissionSlot = false;
    for (uint8_t i = 0; i < kTakMissionSlotCount; ++i) {
        if (profile.missionSlot == kTakMissionSlotOptions[i]) {
            validMissionSlot = true;
            break;
        }
    }
    if (!validMissionSlot) {
        profile.missionSlot = 5;
    }
}

static void syncRetainedStealthState()
{
    gStealthRetainedState.magic = kStealthRetainedMagic;
    gStealthRetainedState.version = kStealthRetainedVersion;
    gStealthRetainedState.stateSize = sizeof(StealthRuntimeState);
    gStealthRetainedState.state = gStealthRuntimeState;
}

static void syncRetainedTakState()
{
    gTakRetainedState.magic = kTakRetainedMagic;
    gTakRetainedState.version = kTakRetainedVersion;
    gTakRetainedState.stateSize = sizeof(TakRuntimeState);
    gTakRetainedState.state = gTakRuntimeState;
}

static void clearRetainedStealthState()
{
    gStealthRetainedState.magic = 0;
    gStealthRetainedState.version = 0;
    gStealthRetainedState.stateSize = 0;
    gStealthRetainedState.state = StealthRuntimeState{};
}

static void clearRetainedTakState()
{
    gTakRetainedState.magic = 0;
    gTakRetainedState.version = 0;
    gTakRetainedState.stateSize = 0;
    gTakRetainedState.state = TakRuntimeState{};
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

static void persistTakStateToFile()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);

    if (!FSCom.exists("/prefs")) {
        FSCom.mkdir("/prefs");
    }
    if (FSCom.exists(kTakStateFile)) {
        FSCom.remove(kTakStateFile);
    }

    auto f = FSCom.open(kTakStateFile, FILE_O_WRITE);
    if (!f) {
        return;
    }

    const size_t stateLen = sizeof(gTakRetainedState);
    const size_t written = f.write(reinterpret_cast<const uint8_t *>(&gTakRetainedState), stateLen);
    if (written == stateLen) {
        f.flush();
    }
    f.close();
#endif
}

static void persistTakModeProfileToFile()
{
#ifdef FSCom
    normalizeTakModeProfile(gTakModeProfile);
    concurrency::LockGuard g(spiLock);

    if (!FSCom.exists("/prefs")) {
        FSCom.mkdir("/prefs");
    }
    if (FSCom.exists(kTakProfileFile)) {
        FSCom.remove(kTakProfileFile);
    }

    auto f = FSCom.open(kTakProfileFile, FILE_O_WRITE);
    if (!f) {
        return;
    }

    const size_t written = f.write(reinterpret_cast<const uint8_t *>(&gTakModeProfile), sizeof(gTakModeProfile));
    if (written == sizeof(gTakModeProfile)) {
        f.flush();
    }
    f.close();
#else
    normalizeTakModeProfile(gTakModeProfile);
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

static void clearPersistedTakStateFile()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    if (FSCom.exists(kTakStateFile)) {
        FSCom.remove(kTakStateFile);
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

static bool loadPersistedTakStateFromFile()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    auto f = FSCom.open(kTakStateFile, FILE_O_READ);
    if (!f) {
        return false;
    }

    TakRetainedState persistedState = {};
    bool ok = false;
    if (f.available() >= static_cast<int>(sizeof(persistedState))) {
        const size_t readLen = f.read(reinterpret_cast<uint8_t *>(&persistedState), sizeof(persistedState));
        ok = (readLen == sizeof(persistedState)) && isValidTakRetainedState(persistedState);
    }
    f.close();

    if (ok) {
        gTakRetainedState = persistedState;
        return true;
    }

    if (FSCom.exists(kTakStateFile)) {
        FSCom.remove(kTakStateFile);
    }
#endif
    return false;
}

static void loadTakModeProfileIfNeeded()
{
    if (gTakModeProfileLoaded) {
        return;
    }
    gTakModeProfileLoaded = true;

#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    auto f = FSCom.open(kTakProfileFile, FILE_O_READ);
    if (f) {
        TakModeProfile persisted = {};
        bool ok = false;
        if (f.available() >= static_cast<int>(sizeof(persisted))) {
            const size_t readLen = f.read(reinterpret_cast<uint8_t *>(&persisted), sizeof(persisted));
            ok = (readLen == sizeof(persisted)) && isValidTakModeProfile(persisted);
        }
        f.close();
        if (ok) {
            gTakModeProfile = persisted;
        } else if (FSCom.exists(kTakProfileFile)) {
            FSCom.remove(kTakProfileFile);
        }
    }
#endif
    normalizeTakModeProfile(gTakModeProfile);
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
        hermesXEmUiModule->setSirenRuntimeEnabled(false);
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
        if (nimbleBluetooth->isActive()) {
            nimbleBluetooth->deinit();
            changed = true;
        } else {
            LOG_INFO("[HermesX] Skip BT deinit during stealth apply: NimBLE not active yet");
        }
        gStealthRuntimeState.needsRebootOnExit = true;
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
    if (isTakExperienceActive()) {
        LOG_INFO("[HermesX] Skip stealth restore during boot because TAK MODE is active");
        clearRetainedStealthState();
        clearPersistedStealthStateFile();
        return false;
    }
    if (lighthouseModule != nullptr && lighthouseModule->isEmergencyModeActive()) {
        LOG_INFO("[HermesX] Skip stealth restore during boot because EMAC is active");
        clearRetainedStealthState();
        clearPersistedStealthStateFile();
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
        hermesXEmUiModule->setSirenRuntimeEnabled(gStealthRuntimeState.sirenEnabled);
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

static bool enableUpdateLowLoadMode()
{
    if (gUpdateLowLoadRuntimeState.active) {
        return false;
    }

    gUpdateLowLoadRuntimeState = UpdateLowLoadRuntimeState{};
    gUpdateLowLoadRuntimeState.active = true;
    gUpdateLowLoadRuntimeState.gpsWasEnabled =
        (config.position.gps_mode != meshtastic_Config_PositionConfig_GpsMode_DISABLED &&
         config.position.gps_mode != meshtastic_Config_PositionConfig_GpsMode_NOT_PRESENT);
    gUpdateLowLoadRuntimeState.bluetoothWasEnabled = config.bluetooth.enabled;
    gUpdateLowLoadRuntimeState.radioWasEnabled = config.lora.tx_enabled;
    gUpdateLowLoadRuntimeState.nodeInfoWasActive = (nodeInfoModule != nullptr);
    gUpdateLowLoadRuntimeState.serialWasEnabled = config.security.serial_enabled;

    LOG_INFO("[UpdateLowLoad] enter gps=%d bt=%d radio=%d nodeinfo=%d free=%u largest=%u",
             gUpdateLowLoadRuntimeState.gpsWasEnabled ? 1 : 0, gUpdateLowLoadRuntimeState.bluetoothWasEnabled ? 1 : 0,
             gUpdateLowLoadRuntimeState.radioWasEnabled ? 1 : 0, gUpdateLowLoadRuntimeState.nodeInfoWasActive ? 1 : 0,
             ESP.getFreeHeap(), ESP.getMaxAllocHeap());

#if !MESHTASTIC_EXCLUDE_GPS
    if (gps) {
        gps->disable();
    }
#endif
    if (nodeInfoModule) {
        nodeInfoModule->pauseForUpdateMode();
    }
    if (rIf) {
        rIf->disable();
    }
    if (config.bluetooth.enabled) {
        setBluetoothEnable(false);
    }
    if (!config.security.serial_enabled) {
        config.security.serial_enabled = true;
    }

    LOG_INFO("[UpdateLowLoad] entered serial=%d free=%u largest=%u", config.security.serial_enabled ? 1 : 0,
             ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    return true;
}

static bool disableUpdateLowLoadMode()
{
    if (!gUpdateLowLoadRuntimeState.active) {
        return false;
    }

    LOG_INFO("[UpdateLowLoad] exit begin free=%u largest=%u", ESP.getFreeHeap(), ESP.getMaxAllocHeap());

    if (rIf && gUpdateLowLoadRuntimeState.radioWasEnabled) {
        rIf->enable();
    }
    if (nodeInfoModule && gUpdateLowLoadRuntimeState.nodeInfoWasActive) {
        nodeInfoModule->resumeFromUpdateMode();
    }
#if !MESHTASTIC_EXCLUDE_GPS
    if (gps && gUpdateLowLoadRuntimeState.gpsWasEnabled) {
        gps->enable();
    }
#endif
    if (gUpdateLowLoadRuntimeState.bluetoothWasEnabled) {
        setBluetoothEnable(true);
    }
    config.security.serial_enabled = gUpdateLowLoadRuntimeState.serialWasEnabled;

    gUpdateLowLoadRuntimeState = UpdateLowLoadRuntimeState{};
    LOG_INFO("[UpdateLowLoad] exit done free=%u largest=%u", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    return true;
}

static bool applyTakModeSettings()
{
    loadTakModeProfileIfNeeded();
    bool changed = false;

    if (gTakModeProfile.quietOutputs && HermesXInterfaceModule::instance) {
        if (HermesXInterfaceModule::instance->isEmergencyLampEnabled()) {
            HermesXInterfaceModule::instance->setEmergencyLampEnabled(false);
            changed = true;
        }
        HermesXInterfaceModule::instance->setUiLedBrightness(0);
        HermesXInterfaceModule::instance->stopEmergencySiren();
        changed = true;
    }

    if (gTakModeProfile.quietOutputs && hermesXEmUiModule) {
        hermesXEmUiModule->setSirenRuntimeEnabled(false);
        changed = true;
    }

    if (gTakModeProfile.quietOutputs) {
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

        if (!config.device.led_heartbeat_disabled) {
            changed = true;
        }
        setHeartbeatLedDisabled(true);
        gTakRuntimeState.quietOutputsApplied = true;
    } else if (gTakRuntimeState.quietOutputsApplied) {
        if (HermesXInterfaceModule::instance) {
            HermesXInterfaceModule::instance->setUiLedBrightness(gTakRuntimeState.uiLedBrightness);
            HermesXInterfaceModule::instance->setEmergencyLampEnabled(gTakRuntimeState.emergencyLampEnabled);
            HermesXInterfaceModule::instance->restoreBuzzerOutput();
        }
        if (hermesXEmUiModule) {
            hermesXEmUiModule->setSirenRuntimeEnabled(gTakRuntimeState.sirenEnabled);
        }
        setHeartbeatLedDisabled(gTakRuntimeState.ledHeartbeatDisabled);
#ifdef BUZZER_EN_PIN
        pinMode(BUZZER_EN_PIN, OUTPUT);
        digitalWrite(BUZZER_EN_PIN, HIGH);
#endif
        gTakRuntimeState.quietOutputsApplied = false;
        changed = true;
    }

    if (cannedMessageModule) {
        const auto runState = cannedMessageModule->getRunState();
        if (runState != CANNED_MESSAGE_RUN_STATE_DISABLED && runState != CANNED_MESSAGE_RUN_STATE_INACTIVE) {
            cannedMessageModule->exitMenu();
        }
    }

    const auto activeRole = isTakDeviceRole(config.device.role) ? config.device.role
                                                                : meshtastic_Config_DeviceConfig_Role_TAK;
    if (config.device.role != activeRole) {
        changed = true;
    }
    config.device.role = activeRole;
    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->applyRoleOutputPolicy();
    }
    if (nodeDB) {
        nodeDB->installRoleDefaults(activeRole);
        changed = true;
    }
    config.device.node_info_broadcast_secs = gTakModeProfile.nodeInfoBroadcastSecs;
    config.position.gps_update_interval = gTakModeProfile.gpsUpdateIntervalSecs;
    config.position.position_broadcast_smart_enabled = true;
    config.position.position_broadcast_secs = gTakModeProfile.positionBroadcastSecs;
    config.position.broadcast_smart_minimum_distance = gTakModeProfile.smartMinimumDistanceMeters;
    config.position.broadcast_smart_minimum_interval_secs = gTakModeProfile.smartMinimumIntervalSecs;

    if (!config.lora.use_preset || config.lora.modem_preset != meshtastic_Config_LoRaConfig_ModemPreset_SHORT_FAST) {
        config.lora.use_preset = true;
        config.lora.modem_preset = meshtastic_Config_LoRaConfig_ModemPreset_SHORT_FAST;
        changed = true;
        if (service) {
            service->configChanged.notifyObservers(NULL);
        }
    }

    const uint32_t takMissionSlot = getTakEffectiveMissionSlot();
    const uint32_t nextChannelNum = takMissionSlot == 0 ? gTakRuntimeState.loraChannelNum : takMissionSlot;
    const float nextOverrideFrequency = takMissionSlot == 0 ? gTakRuntimeState.loraOverrideFrequency : 0.0f;
    if (config.lora.channel_num != nextChannelNum || fabsf(config.lora.override_frequency - nextOverrideFrequency) >= 0.0001f) {
        config.lora.channel_num = nextChannelNum;
        config.lora.override_frequency = nextOverrideFrequency;
        changed = true;
        if (service) {
            service->configChanged.notifyObservers(NULL);
        }
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
    gTakRuntimeState.previousRole = sanitizeTakPreviousRole(config.device.role);
    gTakRuntimeState.ledHeartbeatDisabled = config.device.led_heartbeat_disabled;
    gTakRuntimeState.nodeInfoBroadcastSecs = config.device.node_info_broadcast_secs;
    gTakRuntimeState.gpsUpdateIntervalSecs = config.position.gps_update_interval;
    gTakRuntimeState.positionBroadcastSmartEnabled = config.position.position_broadcast_smart_enabled;
    gTakRuntimeState.positionBroadcastSecs = config.position.position_broadcast_secs;
    gTakRuntimeState.smartMinimumDistanceMeters = config.position.broadcast_smart_minimum_distance;
    gTakRuntimeState.smartMinimumIntervalSecs = config.position.broadcast_smart_minimum_interval_secs;
    gTakRuntimeState.positionFlags = config.position.position_flags;
    gTakRuntimeState.telemetryDeviceUpdateInterval = moduleConfig.telemetry.device_update_interval;
    gTakRuntimeState.loraUsePreset = config.lora.use_preset;
    gTakRuntimeState.loraModemPreset = config.lora.modem_preset;
    gTakRuntimeState.loraBandwidth = config.lora.bandwidth;
    gTakRuntimeState.loraSpreadFactor = config.lora.spread_factor;
    gTakRuntimeState.loraCodingRate = config.lora.coding_rate;
    gTakRuntimeState.loraChannelNum = config.lora.channel_num;
    gTakRuntimeState.loraOverrideFrequency = config.lora.override_frequency;

    if (HermesXInterfaceModule::instance) {
        gTakRuntimeState.uiLedBrightness = HermesXInterfaceModule::instance->getUiLedBrightness();
        gTakRuntimeState.emergencyLampEnabled = HermesXInterfaceModule::instance->isEmergencyLampEnabled();
    }

    if (hermesXEmUiModule) {
        gTakRuntimeState.sirenEnabled = hermesXEmUiModule->isSirenEnabled();
    }

    applyTakModeSettings();
    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->syncSmartPowerRoleNow();
    }
    syncRetainedTakState();
    persistTakStateToFile();
    return true;
}

static bool restoreTakModeAfterBoot()
{
    if (gTakRuntimeState.active) {
        return false;
    }
    if (!isValidTakRetainedState(gTakRetainedState) && !loadPersistedTakStateFromFile()) {
        if (isTakDeviceRole(config.device.role)) {
            LOG_INFO("[HermesX] Adopt configured TAK role=%d into TAK UI runtime", static_cast<int>(config.device.role));
            return enableTakMode();
        }
        return false;
    }
    if (!isValidTakRetainedState(gTakRetainedState)) {
        clearRetainedTakState();
        clearPersistedTakStateFile();
        if (isTakDeviceRole(config.device.role)) {
            LOG_INFO("[HermesX] Rebuild TAK UI runtime for configured role=%d", static_cast<int>(config.device.role));
            return enableTakMode();
        }
        return false;
    }

    gTakRuntimeState = gTakRetainedState.state;
    gTakRuntimeState.active = true;
    gTakRuntimeState.previousRole = sanitizeTakPreviousRole(gTakRuntimeState.previousRole);
    const bool changed = applyTakModeSettings();
    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->syncSmartPowerRoleNow();
    }
    syncRetainedTakState();
    persistTakStateToFile();
    return changed;
}

static bool disableTakMode()
{
    if (!gTakRuntimeState.active) {
        return false;
    }

    if (gTakRuntimeState.quietOutputsApplied) {
        if (HermesXInterfaceModule::instance) {
            HermesXInterfaceModule::instance->setUiLedBrightness(gTakRuntimeState.uiLedBrightness);
            HermesXInterfaceModule::instance->setEmergencyLampEnabled(gTakRuntimeState.emergencyLampEnabled);
            HermesXInterfaceModule::instance->restoreBuzzerOutput();
        }
        if (hermesXEmUiModule) {
            hermesXEmUiModule->setSirenRuntimeEnabled(gTakRuntimeState.sirenEnabled);
        }
        setHeartbeatLedDisabled(gTakRuntimeState.ledHeartbeatDisabled);
#ifdef BUZZER_EN_PIN
        pinMode(BUZZER_EN_PIN, OUTPUT);
        digitalWrite(BUZZER_EN_PIN, HIGH);
#endif
        gTakRuntimeState.quietOutputsApplied = false;
    }

    const auto restoredRole = sanitizeTakPreviousRole(gTakRuntimeState.previousRole);
    config.device.role = restoredRole;
    config.device.node_info_broadcast_secs = gTakRuntimeState.nodeInfoBroadcastSecs;
    config.position.gps_update_interval = gTakRuntimeState.gpsUpdateIntervalSecs;
    config.position.position_broadcast_smart_enabled = gTakRuntimeState.positionBroadcastSmartEnabled;
    config.position.position_broadcast_secs = gTakRuntimeState.positionBroadcastSecs;
    config.position.broadcast_smart_minimum_distance = gTakRuntimeState.smartMinimumDistanceMeters;
    config.position.broadcast_smart_minimum_interval_secs = gTakRuntimeState.smartMinimumIntervalSecs;
    config.position.position_flags = gTakRuntimeState.positionFlags;
    moduleConfig.telemetry.device_update_interval = gTakRuntimeState.telemetryDeviceUpdateInterval;
    config.lora.use_preset = gTakRuntimeState.loraUsePreset;
    config.lora.modem_preset = gTakRuntimeState.loraModemPreset;
    config.lora.bandwidth = gTakRuntimeState.loraBandwidth;
    config.lora.spread_factor = gTakRuntimeState.loraSpreadFactor;
    config.lora.coding_rate = gTakRuntimeState.loraCodingRate;
    config.lora.channel_num = gTakRuntimeState.loraChannelNum;
    config.lora.override_frequency = gTakRuntimeState.loraOverrideFrequency;
    setHeartbeatLedDisabled(gTakRuntimeState.ledHeartbeatDisabled);
    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->applyRoleOutputPolicy();
        HermesXInterfaceModule::instance->restoreBuzzerOutput();
    }
    if (nodeDB) {
        nodeDB->installRoleDefaults(restoredRole);
        nodeDB->saveToDisk(SEGMENT_CONFIG | SEGMENT_NODEDATABASE | SEGMENT_DEVICESTATE);
    }
    if (service) {
        service->configChanged.notifyObservers(NULL);
    }
    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->syncSmartPowerRoleNow();
    }
    LOG_INFO("[HermesX] TAK MODE restored role=%d", static_cast<int>(restoredRole));

    clearPersistedTakStateFile();
    clearRetainedTakState();
    gTakRuntimeState.active = false;
    gTakModeUiModel.showMain();
    return true;
}

static void startTakModeTransition(bool entering)
{
    gTakModeUiModel.startTransition(entering, millis());
    if (screen) {
        screen->showHermesXMainPage();
        screen->requestImmediateRedraw();
    }
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

static void resetHermesFastSetupTftPalette(OLEDDisplay *display)
{
    HermesXFastSetupUiRenderer::resetTftPalette(display);
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
    if (!screen) {
        return;
    }
    screen->drawHermesFastSetup(display, state, x, y);
    drawSetupDetailPopupOverlay(display, state);
    drawTraceRoutePopupOverlay(display, state);
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

static bool getTakSettingsRenderRow(void *, uint8_t index, String &label)
{
    switch (index) {
    case 0:
        label = u8"返回";
        return true;
    case 1:
        label = String(u8"裝置資訊: ") +
                getTakOptionLabel(gTakModeProfile.nodeInfoBroadcastSecs, kTakNodeInfoOptions, kTakNodeInfoLabels,
                                  kTakNodeInfoCount);
        return true;
    case 2:
        label = String(u8"GPS刷新: ") +
                getTakOptionLabel(gTakModeProfile.gpsUpdateIntervalSecs, kTakGpsUpdateOptions, kTakGpsUpdateLabels,
                                  kTakGpsUpdateCount);
        return true;
    case 3:
        label = String(u8"位置廣播: ") +
                getTakOptionLabel(gTakModeProfile.positionBroadcastSecs, kTakPositionBroadcastOptions,
                                  kTakPositionBroadcastLabels, kTakPositionBroadcastCount);
        return true;
    case 4:
        label = String(u8"智慧距離: ") +
                getTakOptionLabel(gTakModeProfile.smartMinimumDistanceMeters, kTakSmartDistanceOptions,
                                  kTakSmartDistanceLabels, kTakSmartDistanceCount);
        return true;
    case 5:
        label = String(u8"智慧間隔: ") +
                getTakOptionLabel(gTakModeProfile.smartMinimumIntervalSecs, kTakSmartIntervalOptions,
                                  kTakSmartIntervalLabels, kTakSmartIntervalCount);
        return true;
    case 6:
        label = String(u8"智慧功率低: ") + String(static_cast<int>(getTakSmartPowerMinDbm())) + "dBm";
        return true;
    case 7:
        label = String(u8"智慧功率高: ") + String(static_cast<int>(getTakSmartPowerMaxDbm())) + "dBm";
        return true;
    case 8:
        label = String(u8"聲光靜默: ") + (gTakModeProfile.quietOutputs ? "ON" : "OFF");
        return true;
    case 9:
        label = String("EMUI: ") + (gTakModeProfile.allowEmUi ? "ON" : "OFF");
        return true;
    case 10:
        label = String(u8"尋人模組: ") + (gTakModeProfile.allowFinder ? "ON" : "OFF");
        return true;
    default:
        return false;
    }
}

static bool getTakChannelRenderRow(void *, uint8_t index, String &label)
{
    if (index == 0) {
        label = u8"返回";
        return true;
    }
    const uint8_t slotIndex = index - 1;
    if (slotIndex >= kTakMissionSlotCount) {
        return false;
    }
    label = getTakMissionSlotDisplayLabel(kTakMissionSlotOptions[slotIndex], true);
    if (kTakMissionSlotOptions[slotIndex] == gTakModeProfile.missionSlot) {
        label += " *";
    }
    return true;
}

void Screen::drawTakModeFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    if (!display) {
        return;
    }

    loadTakModeProfileIfNeeded();
    graphics::HermesXTakModeRenderView view;
    view.title = getTakExperienceTitle();
    view.active = isTakExperienceActive();
    view.allowEmUi = gTakModeProfile.allowEmUi;
    view.allowFinder = gTakModeProfile.allowFinder;
    view.settingsRows = {kTakSettingsRowCount, nullptr, getTakSettingsRenderRow};
    view.channelRows = {static_cast<uint8_t>(kTakMissionSlotCount + 1), nullptr, getTakChannelRenderRow};

    String slotLine;
    String utilizationLine;
    String suggestionLine;
    if (gTakModeUiState.page == graphics::HermesXTakModePage::Main) {
        const float channelUtilization = airTime ? airTime->channelUtilizationPercent() : 0.0f;
        slotLine =
            getTakMissionSlotDisplayLabel(config.lora.channel_num ? config.lora.channel_num : getTakEffectiveMissionSlot(), false);
        if (fabsf(config.lora.override_frequency) >= 0.0001f) {
            slotLine = String(u8"手動 ") + formatSetupFrequencyLabel(config.lora.override_frequency);
        }
        char utilizationBuffer[32];
        snprintf(utilizationBuffer, sizeof(utilizationBuffer), "ChUtil %.0f%%", channelUtilization);
        utilizationLine = String(utilizationBuffer) + " " + getTakAirtimeStatusLabel(channelUtilization);
        if (channelUtilization >= 50.0f) {
            const uint32_t suggestedSlot = getTakSuggestedMissionSlot();
            if (suggestedSlot != 0) {
                suggestionLine = String(u8"建議 ") + getTakMissionSlotDisplayLabel(suggestedSlot, false);
            }
        }
        view.slotLine = &slotLine;
        view.utilizationLine = &utilizationLine;
        view.suggestionLine = &suggestionLine;
    }

    const bool modalPage = gTakModeUiState.page == graphics::HermesXTakModePage::Popup ||
                           gTakModeUiState.page == graphics::HermesXTakModePage::Settings ||
                           gTakModeUiState.page == graphics::HermesXTakModePage::ChannelSelect;
    graphics::HermesXTakModeUiRenderer::draw(display, x, y, state == nullptr && modalPage, gTakModeUiState, view);
}

static void drawSmartPowerHomeFrame(OLEDDisplay *display, int16_t x, int16_t y)
{
    if (!display) {
        return;
    }

    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    const bool portraitLayout = width < height && width <= 160;
    const bool compactLayout = (width < 200 || height < 120);

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

    int8_t currentDbm = config.lora.tx_power;
    float snr = 0.0f;
    int32_t rssi = 0;
    uint32_t signalAgeMs = 0;
    bool hasSignal = false;
    if (HermesXInterfaceModule::instance) {
        currentDbm = HermesXInterfaceModule::instance->getSmartPowerCurrentDbm();
        hasSignal = HermesXInterfaceModule::instance->getSmartPowerLastSignal(snr, rssi, signalAgeMs);
    }
    const int groupCount = std::max<int>(0, getGroupNodeCount());
    const HermesXHomeBatteryState battery =
        HermesXHomeStateCollector::instance().collectBattery(readHermesXHomeBatterySource());
    const bool hasBattery = battery.available;
    const uint8_t batteryPercent = battery.percent;

    const int16_t pad = portraitLayout ? 6 : (compactLayout ? 4 : 8);
    const int16_t titleY = y + (portraitLayout ? 2 : (compactLayout ? 1 : 3));
    const int16_t titleH = compactLayout ? 12 : 16;
    graphics::HermesX_zh::drawMixedBounded(*display, x + pad, titleY, width - (pad * 2), u8"智慧功率",
                                           graphics::HermesX_zh::GLYPH_WIDTH, titleH, nullptr);
    if (hasBattery) {
        const int16_t iconW = portraitLayout ? 24 : 20;
        const int16_t iconH = 12;
        const int16_t iconX = width - pad - iconW;
        const int16_t iconY = titleY;
        const int16_t titleTextW = graphics::HermesX_zh::stringAdvance(u8"智慧功率", graphics::HermesX_zh::GLYPH_WIDTH, display);
        const int16_t headerLeft = x + pad + titleTextW + 4;
        const int16_t headerRight = iconX - 3;
        const uint32_t currentSlot = config.lora.channel_num ? config.lora.channel_num : getTakEffectiveMissionSlot();
        const char *channelLabel = nullptr;
        if (fabsf(config.lora.override_frequency) >= 0.0001f) {
            channelLabel = u8"手動";
        } else if (currentSlot != 0) {
            channelLabel = getTakMissionSlotLabel(currentSlot);
        } else {
            channelLabel = u8"自動";
        }
        display->setFont(FONT_SMALL);
        const int16_t channelW = display->getStringWidth(channelLabel);
        const int16_t shortNameW = owner.short_name[0] != '\0' ? display->getStringWidth(owner.short_name) : 0;
        const int16_t gap = 3;
        const int16_t headerW = headerRight - headerLeft;
        if (headerW >= channelW) {
            display->setTextAlignment(TEXT_ALIGN_LEFT);
            display->drawString(headerLeft, titleY, channelLabel);
            if (owner.short_name[0] != '\0' && headerW >= channelW + gap + shortNameW) {
                display->setTextAlignment(TEXT_ALIGN_RIGHT);
                display->drawString(headerRight, titleY, owner.short_name);
            }
        }
        HermesXHomeUiRenderer::drawHorizontalBattery(display, iconX, iconY, iconW, iconH, batteryPercent);
    }

    const int16_t groupW = portraitLayout ? (width - pad * 2) : (compactLayout ? 48 : 66);
    const int16_t groupH = portraitLayout ? 64 : std::min<int16_t>(height - (compactLayout ? 30 : 42), compactLayout ? 68 : 82);
    const int16_t groupX = portraitLayout ? (x + pad) : (width - groupW - pad);
    const int16_t groupY = portraitLayout ? (height - groupH - pad) : ((height - groupH) / 2 + (compactLayout ? 8 : 10));
    display->drawRect(groupX, groupY, groupW, groupH);
    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->drawString(groupX + groupW / 2, groupY + 4, "GROUP");
    display->setFont((!compactLayout && groupCount < 100) ? FONT_LARGE : FONT_MEDIUM);
    char groupBuf[8];
    snprintf(groupBuf, sizeof(groupBuf), "%d", groupCount);
    const int16_t groupNumberH = compactLayout ? FONT_HEIGHT_MEDIUM : FONT_HEIGHT_LARGE;
    const int16_t groupHeaderH = 18;
    const int16_t groupNumberY = groupY + groupHeaderH + std::max<int16_t>(0, (groupH - groupHeaderH - groupNumberH) / 2);
    display->drawString(groupX + groupW / 2, groupNumberY, groupBuf);

    display->setTextAlignment(TEXT_ALIGN_LEFT);
    const int16_t leftX = x + pad;
    const int16_t leftW = portraitLayout ? (width - pad * 2) : std::max<int16_t>(40, groupX - leftX - pad);
    display->setFont(FONT_SMALL);
    char snrBuf[24];
    char powerBuf[24];
    char rssiBuf[24];
    char ageBuf[24];
    snprintf(powerBuf, sizeof(powerBuf), "dBm %d", static_cast<int>(currentDbm));
    if (hasSignal) {
        snprintf(snrBuf, sizeof(snrBuf), "SNR %.1f", static_cast<double>(snr));
        if (rssi != 0) {
            snprintf(rssiBuf, sizeof(rssiBuf), "RSSI %" PRId32, rssi);
        } else {
            snprintf(rssiBuf, sizeof(rssiBuf), "RSSI --");
        }
        if (signalAgeMs < 1000U) {
            snprintf(ageBuf, sizeof(ageBuf), "RX now");
        } else {
            snprintf(ageBuf, sizeof(ageBuf), "RX %lus", static_cast<unsigned long>(signalAgeMs / 1000U));
        }
    } else {
        snprintf(snrBuf, sizeof(snrBuf), "SNR --");
        snprintf(rssiBuf, sizeof(rssiBuf), "RSSI --");
        snprintf(ageBuf, sizeof(ageBuf), "RX wait");
    }

    const int16_t metricsTop = titleY + titleH + (portraitLayout ? 8 : (compactLayout ? 4 : 8));
    const int16_t metricsBottom = portraitLayout ? (groupY - pad) : (height - pad);
    const int16_t metricsH = std::max<int16_t>(FONT_HEIGHT_SMALL * 4, metricsBottom - metricsTop);
    const int16_t rowStep = std::max<int16_t>(FONT_HEIGHT_SMALL, metricsH / 4);
    display->drawStringMaxWidth(leftX, metricsTop, leftW, snrBuf);
    display->drawStringMaxWidth(leftX, metricsTop + rowStep, leftW, powerBuf);
    display->drawStringMaxWidth(leftX, metricsTop + rowStep * 2, leftW, rssiBuf);
    display->drawStringMaxWidth(leftX, metricsTop + rowStep * 3, leftW, ageBuf);

    display->setColor(WHITE);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void Screen::drawHermesXMain(OLEDDisplay *display, OLEDDisplayUiState * /*state*/, int16_t x, int16_t y)
{
    if (gTakModeUiState.page == graphics::HermesXTakModePage::TransitionEnter ||
        gTakModeUiState.page == graphics::HermesXTakModePage::TransitionExit) {
        graphics::HermesXTakModeUiRenderer::drawTransition(
            display, gTakModeUiState.transitionStartedAtMs,
            gTakModeUiState.page == graphics::HermesXTakModePage::TransitionEnter);
        return;
    }
    if (gLowMemoryUiState.visible) {
        graphics::HermesXLowMemoryUiRenderer::draw(display, gLowMemoryUiState, gLowMemoryProtectionActive);
        return;
    }
    if (isSmartPowerHomeActive()) {
        drawSmartPowerHomeFrame(display, x, y);
        if (gTakModeUiState.page != graphics::HermesXTakModePage::Main) {
            drawTakModeFrame(display, nullptr, x, y);
        }
        return;
    }

    (void)x;
    (void)y;
    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    const int16_t originX = 0;
    const int16_t originY = 0;
    const HermesXHomeBatteryState battery =
        HermesXHomeStateCollector::instance().collectBattery(readHermesXHomeBatterySource());
    const bool hasBattery = battery.available;
    const uint8_t batteryPercent = battery.percent;
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
    const int16_t contentW = width - 8;
    auto drawHomeLine = [&](int16_t dx, int16_t dy, const char *text) {
        const int16_t maxW = (contentW > 0) ? contentW : (width - 4);
        display->drawStringMaxWidth(dx, dy, maxW, text);
    };
    char timeBuf[16];
    char dateBuf[24];
    const bool hasValidTime = HermesXHomeStateCollector::formatTimeDate(
        getValidTime(RTCQuality::RTCQualityNTP, true), timeBuf, sizeof(timeBuf), dateBuf, sizeof(dateBuf));

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
    const int16_t customTimeMaxW = HermesXPattanakarnNeonFont::measureText("88:88:88");
    const bool useCustomTimeFont =
        !gLowMemoryProtectionActive && (contentW > 0) && (customTimeMaxW > 0) && (customTimeMaxW <= contentW);
    const bool useDirectHomeOverlay = isDirectHomePresentationAvailable(display);
    const uint16_t neonGlowOuterFg = TFTDisplay::rgb565(0x1A, 0x3F, 0xD6);
    const uint16_t neonGlowInnerFg = TFTDisplay::rgb565(0x4C, 0xD9, 0xFF);
    const uint16_t neonCoreFg = TFTDisplay::rgb565(0xFE, 0xFF, 0xFF);
    const bool showHomeQuote = useDirectHomeOverlay;
    if (showHomeQuote) {
        const int16_t quoteX = compactLayout ? 56 : 64;
        const int16_t quoteY = originY + (compactLayout ? 16 : 20);
        const int16_t quoteW = width - quoteX - (compactLayout ? 4 : 8);
        const int16_t quoteH = compactLayout ? 28 : 34;
        HermesXHomeUiRenderer::drawQuote(display, quoteX, quoteY, quoteW, quoteH);
    }
    if (!hasValidTime && !useDirectHomeOverlay && !gLowMemoryProtectionActive) {
        HermesXHomeUiRenderer::drawDog(display, width, timeY, compactLayout, millis());
    } else if (useDirectHomeOverlay) {
        // The direct-TFT Home dog is rendered after ui->update().
    } else if (useCustomTimeFont) {
        const int16_t currentTimeW = HermesXPattanakarnNeonFont::measureText(timeBuf);
        const int16_t timeFrameX = originX + ((width - customTimeMaxW) / 2);
        const int16_t drawX = timeFrameX + ((customTimeMaxW - currentTimeW) / 2);
        const int16_t coreX = drawX;
        const int16_t coreY = timeY;
        const int16_t coreW = currentTimeW;
        const int16_t coreH = HermesXPattanakarnNeonFont::GlyphHeight;
        addTftColorZone(display, timeFrameX - 3, timeY - 3, customTimeMaxW + 6,
                        HermesXPattanakarnNeonFont::GlyphHeight + 6, neonGlowOuterFg,
                        0x0000);
        addTftColorZone(display, drawX - 2, timeY - 2, currentTimeW + 4,
                        HermesXPattanakarnNeonFont::GlyphHeight + 4, neonGlowInnerFg,
                        0x0000);
        addTftColorZone(
            display, drawX, timeY, currentTimeW, HermesXPattanakarnNeonFont::GlyphHeight, neonCoreFg, 0x0000);
        HermesXPattanakarnNeonFont::drawTextOutsideRect(display, drawX + 2, timeY, timeBuf, coreX, coreY, coreW, coreH);
        HermesXPattanakarnNeonFont::drawTextOutsideRect(display, drawX - 2, timeY, timeBuf, coreX, coreY, coreW, coreH);
        HermesXPattanakarnNeonFont::drawTextOutsideRect(display, drawX, timeY + 2, timeBuf, coreX, coreY, coreW, coreH);
        HermesXPattanakarnNeonFont::drawTextOutsideRect(display, drawX, timeY - 2, timeBuf, coreX, coreY, coreW, coreH);
        HermesXPattanakarnNeonFont::drawTextOutsideRect(display, drawX + 1, timeY, timeBuf, coreX, coreY, coreW, coreH);
        HermesXPattanakarnNeonFont::drawTextOutsideRect(display, drawX - 1, timeY, timeBuf, coreX, coreY, coreW, coreH);
        HermesXPattanakarnNeonFont::drawTextOutsideRect(display, drawX, timeY + 1, timeBuf, coreX, coreY, coreW, coreH);
        HermesXPattanakarnNeonFont::drawTextOutsideRect(display, drawX, timeY - 1, timeBuf, coreX, coreY, coreW, coreH);
        HermesXPattanakarnNeonFont::drawText(display, drawX, timeY, timeBuf);
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

    uint32_t satCount = 0;
    if (gpsStatus && gpsStatus->getIsConnected()) {
        satCount = gpsStatus->getNumSatellites();
    }
    if (satCount > 99) {
        satCount = 99;
    }
    const HermesXHomeStatusView statusView{
        dateBuf, roleUpper, hasBattery, batteryPercent, static_cast<uint8_t>(satCount)};
    HermesXHomeUiRenderer::drawStatus(display, statusView);

}

void Screen::drawEmergencyConfirmOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{
    (void)state;
    graphics::HermesXEmergencyConfirmUiRenderer::draw(display, gEmergencyConfirmUiState);
}

void Screen::drawRotaryLockOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{
    (void)state;
    constexpr uint32_t kRotaryLockPopupMs = 5000;
    if (gRotaryLockUiModel.expire(millis(), kRotaryLockPopupMs)) {
        if (screen) {
            screen->setFastFramerate();
        }
        return;
    }
    if (!display) {
        return;
    }
    graphics::HermesXRotaryLockUiRenderer::draw(display, gRotaryLockUiState);
}

static void drawFinderPulseConfirmOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{
    (void)state;
    HermesXNodeBrowserUiRenderer::drawFinderPulseConfirm(display, millis(), kStealthConfirmArmMs);
}

static void drawFinderPulseSendingOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{
    (void)state;
    HermesXNodeBrowserUiRenderer::drawFinderPulseSending(display, millis());
}
void Screen::drawHermesXAction(OLEDDisplay *display, OLEDDisplayUiState * /*state*/, int16_t /*x*/, int16_t /*y*/)
{
    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();

    bool lampOn = false;
    if (HermesXInterfaceModule::instance) {
        lampOn = HermesXInterfaceModule::instance->isEmergencyLampEnabled();
    }
    const uint8_t currentActionCount = hermesActionFeatureMenuActive ? kMainActionFeatureCount : kMainActionPrimaryCount;
    const uint8_t *currentActionOrder = hermesActionFeatureMenuActive ? kMainActionFeatureOrder : kMainActionPrimaryOrder;
    if (hermesActionSelected < 0 || hermesActionSelected >= static_cast<int8_t>(currentActionCount)) {
        hermesActionSelected = 0;
    }

    if (!hermesActionStealthConfirmVisible) {
        gNodeBrowserDataSource.refreshOnline();
        const bool stealthOn = isStealthModeActive();
        const bool takOn = isTakExperienceActive();
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

        static const char *kTileLabelExact[kMainActionIdCount] = {
            u8"潛行模式", u8"緊急照明燈", "GPS", u8"TAK MODE", u8"休眠", "Home", u8"頻道", u8"設定", "MSG", "ONLINE",
            "TRACE", "GROUP", u8"尋人模組", u8"功能", u8"退出",
        };
        static const char *kTileLabelCompact[kMainActionIdCount] = {
            u8"潛行", u8"照明", "GPS", "TAK", u8"休眠", "Home", u8"頻道", u8"設定", "MSG", "ON", "TR", "GRP", u8"尋人",
            u8"功能", u8"退出",
        };
        const bool compactLayout = (width < 180 || height < 100);
        bool tileHasState[kMainActionIdCount] = {true, true, true, true, false, false, false, false, false, false, false, false, false, false, false};
        bool tileState[kMainActionIdCount] = {stealthOn, lampOn, gpsOn, takOn, false, false, false, false, false, false, false, false, false, false, false};
        tileHasState[8] = hasRecentMessages;
        tileState[8] = recentUnread;
        tileHasState[9] = (gOnlineNodeState.count > 0);
        tileState[9] = (gOnlineNodeState.count > 0);
        tileHasState[10] = (gOnlineNodeState.count > 0);
        tileState[10] = (gOnlineNodeState.count > 0);
        tileHasState[11] = getGroupNodeCount() > 0;
        tileState[11] = getGroupNodeCount() > 0;

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
                case 9: { // ONLINE（小圖）
                    const int16_t leftHeadR = 4;
                    const int16_t rightHeadR = 3;
                    const int16_t leftHeadX = cx - 5;
                    const int16_t leftHeadY = cy - 2;
                    const int16_t rightHeadX = cx + 5;
                    const int16_t rightHeadY = cy;
                    display->drawCircle(leftHeadX, leftHeadY, leftHeadR);
                    display->drawCircle(rightHeadX, rightHeadY, rightHeadR);
                    display->drawLine(leftHeadX - 6, cy + 7, leftHeadX + 6, cy + 7);
                    display->drawLine(leftHeadX - 3, cy + 3, leftHeadX + 3, cy + 3);
                    display->drawLine(rightHeadX - 4, cy + 5, rightHeadX + 4, cy + 5);
                    break;
                }
                case 10: { // TraceRoute（小圖）
                    display->drawCircle(cx - 8, cy + 5, 3);
                    display->drawCircle(cx, cy - 6, 3);
                    display->drawCircle(cx + 8, cy + 5, 3);
                    display->drawLine(cx - 5, cy + 3, cx - 2, cy - 4);
                    display->drawLine(cx + 2, cy - 4, cx + 5, cy + 3);
                    display->drawLine(cx - 9, cy - 8, cx + 9, cy - 8);
                    display->drawLine(cx + 9, cy - 8, cx + 6, cy - 11);
                    display->drawLine(cx + 9, cy - 8, cx + 6, cy - 5);
                    break;
                }
                case 11: { // GROUP（小圖）
                    display->drawLine(cx - 7, cy - 3, cx + 7, cy - 3);
                    display->drawLine(cx - 7, cy - 3, cx, cy + 7);
                    display->drawLine(cx + 7, cy - 3, cx, cy + 7);
                    display->drawCircle(cx - 7, cy - 3, 3);
                    display->drawCircle(cx + 7, cy - 3, 3);
                    display->drawCircle(cx, cy + 7, 3);
                    break;
                }
                case 12: { // 尋人模組（小圖）
                    HermesXNodeBrowserUiRenderer::drawFinderRadarIcon(display, cx, cy + 1, 10, true);
                    break;
                }
                case 13: { // 功能（小圖）
                    display->drawRect(cx - 10, cy - 9, 7, 7);
                    display->drawRect(cx + 3, cy - 9, 7, 7);
                    display->drawRect(cx - 10, cy + 4, 7, 7);
                    display->drawRect(cx + 3, cy + 4, 7, 7);
                    break;
                }
                case 14: { // 退出（小圖）
                    display->drawLine(cx - 9, cy, cx + 8, cy);
                    display->drawLine(cx - 9, cy, cx - 2, cy - 7);
                    display->drawLine(cx - 9, cy, cx - 2, cy + 7);
                    display->drawLine(cx + 3, cy - 9, cx + 9, cy - 9);
                    display->drawLine(cx + 9, cy - 9, cx + 9, cy + 9);
                    display->drawLine(cx + 9, cy + 9, cx + 3, cy + 9);
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
            case 9: { // ONLINE
                int16_t leftHeadR = base / 5;
                int16_t rightHeadR = leftHeadR - 1;
                if (leftHeadR < 5) {
                    leftHeadR = 5;
                }
                if (rightHeadR < 4) {
                    rightHeadR = 4;
                }

                const int16_t headBandTop = iy + 3;
                const int16_t headBandBottom = iy + ih - 10;
                const int16_t headCenterY = headBandTop + (headBandBottom - headBandTop) / 2 - 3;
                const int16_t leftHeadX = cx - (iw / 5);
                const int16_t rightHeadX = cx + (iw / 6);
                const int16_t leftHeadY = headCenterY;
                const int16_t rightHeadY = headCenterY + 2;

                display->drawCircle(leftHeadX, leftHeadY, leftHeadR);
                display->drawCircle(rightHeadX, rightHeadY, rightHeadR);

                const int16_t leftBodyY = leftHeadY + leftHeadR + 4;
                const int16_t rightBodyY = rightHeadY + rightHeadR + 4;
                display->drawLine(leftHeadX - leftHeadR - 4, leftBodyY, leftHeadX + leftHeadR + 4, leftBodyY);
                display->drawLine(leftHeadX - leftHeadR + 1, leftBodyY - 5, leftHeadX + leftHeadR - 1, leftBodyY - 5);
                display->drawLine(rightHeadX - rightHeadR - 3, rightBodyY, rightHeadX + rightHeadR + 3, rightBodyY);
                display->drawLine(rightHeadX - rightHeadR + 1, rightBodyY - 4, rightHeadX + rightHeadR - 1, rightBodyY - 4);
                break;
            }
            case 10: { // TraceRoute
                int16_t nodeR = base / 10 + 4;
                if (nodeR < 5) {
                    nodeR = 5;
                }
                const int16_t leftX = cx - base / 3;
                const int16_t midX = cx;
                const int16_t rightX = cx + base / 3;
                const int16_t topY = iy + ih / 2 - base / 4;
                const int16_t bottomY = iy + ih / 2 + base / 4;
                display->drawCircle(leftX, bottomY, nodeR);
                display->drawCircle(midX, topY, nodeR);
                display->drawCircle(rightX, bottomY, nodeR);
                display->drawLine(leftX + nodeR, bottomY - 1, midX - nodeR, topY + 1);
                display->drawLine(midX + nodeR, topY + 1, rightX - nodeR, bottomY - 1);
                display->drawLine(leftX - nodeR, topY - 4, rightX + nodeR, topY - 4);
                display->drawLine(rightX + nodeR, topY - 4, rightX + nodeR - 5, topY - 9);
                display->drawLine(rightX + nodeR, topY - 4, rightX + nodeR - 5, topY + 1);
                break;
            }
            case 11: { // GROUP
                const int16_t topY = iy + ih / 2 - base / 4;
                const int16_t botY = iy + ih / 2 + base / 4;
                const int16_t leftX = cx - base / 3;
                const int16_t rightX = cx + base / 3;
                int16_t nodeR = base / 9 + 3;
                if (nodeR < 5) {
                    nodeR = 5;
                }
                display->drawLine(leftX, topY, rightX, topY);
                display->drawLine(leftX, topY, cx, botY);
                display->drawLine(rightX, topY, cx, botY);
                display->drawCircle(leftX, topY, nodeR);
                display->drawCircle(rightX, topY, nodeR);
                display->drawCircle(cx, botY, nodeR);
                break;
            }
            case 12: { // 尋人模組
                int16_t radarR = base / 2;
                if (radarR < 16) {
                    radarR = 16;
                }
                HermesXNodeBrowserUiRenderer::drawFinderRadarIcon(display, cx, iy + ih / 2, radarR, false);
                break;
            }
            case 13: { // 功能
                const int16_t cell = std::max<int16_t>(10, std::min<int16_t>(base / 3 + 6, 20));
                const int16_t gap = std::max<int16_t>(4, cell / 3);
                const int16_t gridW = cell * 2 + gap;
                const int16_t gridH = cell * 2 + gap;
                const int16_t gx = cx - gridW / 2;
                const int16_t gy = iy + (ih - gridH) / 2;
                display->drawRect(gx, gy, cell, cell);
                display->drawRect(gx + cell + gap, gy, cell, cell);
                display->drawRect(gx, gy + cell + gap, cell, cell);
                display->drawRect(gx + cell + gap, gy + cell + gap, cell, cell);
                display->drawLine(gx + cell / 2, gy + cell / 2, gx + cell + gap + cell / 2, gy + cell / 2);
                display->drawLine(gx + cell / 2, gy + cell / 2, gx + cell / 2, gy + cell + gap + cell / 2);
                break;
            }
            case 14: { // 退出
                const int16_t arrowW = std::max<int16_t>(24, base / 2 + 16);
                const int16_t arrowX = cx - arrowW / 3;
                const int16_t arrowY = iy + ih / 2;
                display->drawLine(arrowX - arrowW / 2, arrowY, arrowX + arrowW / 2, arrowY);
                display->drawLine(arrowX - arrowW / 2, arrowY, arrowX - arrowW / 2 + 9, arrowY - 9);
                display->drawLine(arrowX - arrowW / 2, arrowY, arrowX - arrowW / 2 + 9, arrowY + 9);
                const int16_t doorX = arrowX + arrowW / 3;
                const int16_t doorY = arrowY - std::max<int16_t>(14, base / 3);
                const int16_t doorW = std::max<int16_t>(16, base / 3);
                const int16_t doorH = std::max<int16_t>(28, base / 2 + 8);
                display->drawRect(doorX, doorY, doorW, doorH);
                display->drawLine(doorX + doorW - 4, arrowY, doorX + doorW - 2, arrowY);
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
            display->setTextAlignment(TEXT_ALIGN_LEFT);
            display->setFont(FONT_SMALL);
            bool asciiOnly = true;
            for (const char *p = label; p && *p; ++p) {
                const uint8_t ch = static_cast<uint8_t>(*p);
                if (ch < 0x20 || ch >= 0x7F) {
                    asciiOnly = false;
                    break;
                }
            }
            const char *drawLabel = label;
            int labelW = graphics::HermesX_zh::stringAdvance(drawLabel, graphics::HermesX_zh::GLYPH_WIDTH, display);
            const int16_t labelY = ty + th - labelLineH - (compactLayout ? 3 : 8);
            int16_t labelX = tx + (tw - labelW) / 2;
            if (labelX < tx + (asciiOnly ? 1 : 2)) {
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

            if (asciiOnly) {
                display->drawString(labelX, labelY, drawLabel);
            } else {
                graphics::HermesX_zh::drawMixedBounded(*display, labelX, labelY, tx + tw - labelX - 2, drawLabel,
                                                       graphics::HermesX_zh::GLYPH_WIDTH, labelLineH, nullptr);
            }

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
                index += currentActionCount;
            }
            return index % currentActionCount;
        };

        const int actionSlots[kMainActionVisibleSlots] = {
            wrapActionIndex(hermesActionSelected - 1),
            hermesActionSelected,
            wrapActionIndex(hermesActionSelected + 1),
        };
        const uint8_t actionIds[kMainActionVisibleSlots] = {
            currentActionOrder[actionSlots[0]],
            currentActionOrder[actionSlots[1]],
            currentActionOrder[actionSlots[2]],
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

        drawActionGlyph(actionIds[0], leftCardX, sideY, sideDrawW, sideH, false);
        drawActionLabel(actionIds[0], leftCardX, sideY, sideDrawW, sideH, false, kTileLabelCompact[actionIds[0]]);

        drawActionGlyph(actionIds[1], centerX, centerY, centerW, centerH, true);
        drawActionLabel(actionIds[1], centerX, centerY, centerW, centerH, true, kTileLabelExact[actionIds[1]]);

        drawActionGlyph(actionIds[2], rightCardX, sideY, sideDrawW, sideH, false);
        drawActionLabel(actionIds[2], rightCardX, sideY, sideDrawW, sideH, false, kTileLabelCompact[actionIds[2]]);

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
    static int lastPalettePage = -1;
    const int currentPalettePage = static_cast<int>(gHermesFastSetupNavigation.page);
    const bool forcePaletteRedraw = (lastPalettePage != currentPalettePage);
    lastPalettePage = currentPalettePage;

    const auto applyPaletteNoSelection = [&]() {
        HermesXFastSetupUiRenderer::applyTftPalette(display, width, height, forcePaletteRedraw);
    };
    const HermesXFastSetupListContext listContext(display, width, height, gHermesFastSetupNavigation.selected,
                                                  gHermesFastSetupNavigation.offset, forcePaletteRedraw,
                                                  hermesSetupToast, hermesSetupToastUntilMs);

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateDetailPopup && isSetupDetailPopupVisible()) {
        if (gHermesFastSetupNavigation.returnPage != HermesFastSetupPage::UpdateDetailPopup) {
            const HermesFastSetupPage popupPage = gHermesFastSetupNavigation.page;
            gHermesFastSetupNavigation.page = gHermesFastSetupNavigation.returnPage;
            drawHermesFastSetup(display, nullptr, 0, 0);
            gHermesFastSetupNavigation.page = popupPage;
        } else {
            applyPaletteNoSelection();
        }
        drawSetupDetailPopupOverlay(display, nullptr);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::Entry) {
        applyPaletteNoSelection();
        HermesXFastSetupUiRenderer::drawEntryPage(display, width, height);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::PassEdit) {
        applyPaletteNoSelection();
        const char *header = (hermesSetupEditingSlot == 0) ? "GROUP PIN A" : "GROUP PIN B";
        HermesXFastSetupUiRenderer::drawKeyboardPage(
            display, width, height, header, hermesSetupPassDraft, kSetupKeyRows, kSetupKeyRowLengths,
            kSetupKeyRowCount, hermesSetupKeyRow, hermesSetupKeyCol, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::FrequencyEdit) {
        applyPaletteNoSelection();
        HermesXFastSetupUiRenderer::drawKeyboardPage(
            display, width, height, u8"手動設定頻率", hermesSetupFrequencyDraft, kSetupNumericKeyRows,
            kSetupNumericKeyRowLengths, kSetupNumericKeyRowCount, hermesSetupKeyRow, hermesSetupKeyCol,
            hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::PassShow) {
        const String passA = lighthouseModule ? lighthouseModule->getEmergencyGroupPin(0) : "";
        const String passB = lighthouseModule ? lighthouseModule->getEmergencyGroupPin(1) : "";
        HermesXFastSetupUiRenderer::drawGroupPins(listContext, passA.c_str(), passB.c_str());
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UiMenu) {
        const uint8_t ledBrightness =
            HermesXInterfaceModule::instance ? HermesXInterfaceModule::instance->getUiLedBrightness() : 60;
        const bool ambientEnabled = moduleConfig.has_ambient_lighting && moduleConfig.ambient_lighting.led_state;
        const bool rotarySwapped =
            moduleConfig.canned_message.inputbroker_event_cw ==
            meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP;
        const String brightnessLabel = getSetupBrightnessLabel(ledBrightness);
        const String sleepLabel = getSetupScreenSleepLabel(getSetupCurrentScreenSleepSeconds());
        HermesXFastSetupUiRenderer::drawUiMenu(
            listContext, isBuzzerGloballyEnabled(), brightnessLabel.c_str(), ambientEnabled, sleepLabel.c_str(),
            getSetupTimezoneLabel(config.device.tzdef), rotarySwapped, isIncomingTextPopupEnabled());
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UiBrightnessSelect) {
        const char *labels[kSetupBrightnessCount];
        for (uint8_t i = 0; i < kSetupBrightnessCount; ++i) {
            labels[i] = kSetupBrightnessOptions[i].label;
        }
        HermesXFastSetupUiRenderer::drawSelectionMenu(
            listContext, u8"Hermes狀態條亮度", labels, kSetupBrightnessCount);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UiScreenSleepSelect) {
        const char *labels[kSetupScreenSleepCount];
        for (uint8_t i = 0; i < kSetupScreenSleepCount; ++i) {
            labels[i] = kSetupScreenSleepOptions[i].label;
        }
        HermesXFastSetupUiRenderer::drawSelectionMenu(
            listContext, u8"螢幕休眠時間", labels, kSetupScreenSleepCount);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UiTimezoneSelect) {
        const char *labels[kSetupTimezoneCount];
        for (uint8_t i = 0; i < kSetupTimezoneCount; ++i) {
            labels[i] = kSetupTimezoneOptions[i].label;
        }
        HermesXFastSetupUiRenderer::drawSelectionMenu(listContext, u8"時區設定", labels, kSetupTimezoneCount);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UiRotarySwapSelect) {
        HermesXFastSetupUiRenderer::drawToggleSelectionMenu(listContext, u8"旋鈕對調");
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::EmacMenu) {
        HermesXFastSetupUiRenderer::drawMenu(listContext, u8"GROUP設定", kSetupEmacItems, kSetupEmacCount);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::EmacEmInfoMenu) {
        const String emInfoLabel =
            formatSetupSecondsLabel(hermesXEmUiModule ? hermesXEmUiModule->getEmInfoIntervalSec() : 0);
        const String heartbeatLabel =
            formatSetupSecondsLabel(hermesXEmUiModule ? hermesXEmUiModule->getEmHeartbeatIntervalSec() : 0);
        HermesXFastSetupUiRenderer::drawEmInfoMenu(
            listContext, hermesXEmUiModule && hermesXEmUiModule->isEmInfoBroadcastEnabled(), emInfoLabel.c_str(),
            heartbeatLabel.c_str(), hermesXEmUiModule ? hermesXEmUiModule->getEmOfflineThresholdCount() : 3,
            hermesXEmUiModule && hermesXEmUiModule->isEmBatteryIncluded());
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::EmacEmInfoIntervalSelect) {
        HermesXFastSetupUiRenderer::drawSelectionMenu(
            listContext, u8"EMINFO週期", kSetupEmInfoIntervalLabels, kSetupEmInfoIntervalCount);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::EmacHeartbeatIntervalSelect) {
        HermesXFastSetupUiRenderer::drawSelectionMenu(
            listContext, u8"Heartbeat週期", kSetupHeartbeatIntervalLabels, kSetupHeartbeatIntervalCount);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::EmacOfflineThresholdSelect) {
        HermesXFastSetupUiRenderer::drawSelectionMenu(
            listContext, u8"離線門檻", kSetupOfflineThresholdLabels, kSetupOfflineThresholdCount);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::EmacBatteryIncludeSelect) {
        HermesXFastSetupUiRenderer::drawToggleSelectionMenu(listContext, u8"附帶電量");
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::NodeMenu) {
        HermesXFastSetupUiRenderer::drawNodeMenu(listContext, moduleConfig.mqtt.enabled, config.bluetooth.enabled);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::DeviceInfoMenu) {
        const uint32_t currentBroadcast = getSetupCurrentNodeInfoBroadcast();
        const char *broadcastLabel = getSetupNodeInfoBroadcastLabel(currentBroadcast);
        const String nodeId = getSetupNodeNumLabel();
        const String broadcastValue =
            broadcastLabel ? String(broadcastLabel) : formatSetupSecondsLabel(currentBroadcast);
        HermesXFastSetupUiRenderer::drawDeviceInfoMenu(
            listContext, owner.short_name, owner.long_name, nodeId.c_str(), broadcastValue.c_str());
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::DeviceInfoShortNameEdit ||
        gHermesFastSetupNavigation.page == HermesFastSetupPage::DeviceInfoLongNameEdit) {
        applyPaletteNoSelection();
        const String preview = makeSetupDraftPreview(hermesSetupDeviceInfoDraft);
        const char *title =
            (gHermesFastSetupNavigation.page == HermesFastSetupPage::DeviceInfoShortNameEdit) ? u8"裝置ID" : u8"裝置名稱";
        HermesXFastSetupUiRenderer::drawKeyboardPage(
            display, width, height, title, preview, getSetupWifiKeyRows(hermesSetupDeviceInfoLowercase),
            kSetupWifiKeyRowLengths, kSetupWifiKeyRowCount, hermesSetupKeyRow, hermesSetupKeyCol, hermesSetupToast,
            hermesSetupToastUntilMs);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::DeviceInfoBroadcastSelect) {
        HermesXFastSetupUiRenderer::drawSelectionMenu(
            listContext, u8"廣播時間", kSetupNodeInfoBroadcastLabels, kSetupNodeInfoBroadcastCount);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateMenu) {
        auto &updateManager = HermesXUpdateManager::instance();
        HermesXFastSetupUiRenderer::drawUpdateMenu(
            listContext, gUpdateBootRequested, updateManager.getCurrentVersion().c_str());
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateIntro) {
        applyPaletteNoSelection();
        HermesXFastSetupUiRenderer::drawUpdateIntroPage(display, width, height, hermesUpdateIntroStartedAtMs);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateExitPending) {
        applyPaletteNoSelection();
        const char *pendingTitle = (hermesSetupToast == u8"重開中") ? u8"重開中" : u8"退出更新模式中";
        HermesXFastSetupUiRenderer::drawUpdateTransitionPage(
            display, width, height, hermesUpdateIntroStartedAtMs, pendingTitle, "");
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateCheckMenu) {
        auto &updateManager = HermesXUpdateManager::instance();
        HermesXFastSetupUiRenderer::drawUpdateCheckMenu(
            listContext, updateManager.getCurrentVersion().c_str(), updateManager.getRemoteUpdateUrl(),
            updateManager.getSourceStatus().c_str(), updateManager.getCandidateVersion().c_str(),
            updateManager.getProgressPercent(), updateManager.getLastError().c_str(), updateManager.hasCandidate(),
            updateManager.canApply());
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateCheckFlowPage) {
        applyPaletteNoSelection();
        auto &updateManager = HermesXUpdateManager::instance();
        const char *actionLabel = updateManager.canApply()
                                      ? u8"套用更新"
                                      : (updateManager.hasCandidate() ? u8"開始下載" : u8"開始檢查");
        HermesXFastSetupUiRenderer::drawUrlUpdateFlowPage(
            display, width, height, updateManager.getSourceStatus(), updateManager.getCandidateVersion(),
            updateManager.getProgressPercent(), updateManager.getLastError(), actionLabel,
            gHermesFastSetupNavigation.selected, hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateDetailPopup) {
        LOG_INFO("[UpdateDetailPopup] render title=%s bodyLen=%u", gSetupDetailPopupState.title.c_str(),
                 static_cast<unsigned>(gSetupDetailPopupState.body.length()));
        if (gHermesFastSetupNavigation.returnPage != HermesFastSetupPage::UpdateDetailPopup) {
            const HermesFastSetupPage popupPage = gHermesFastSetupNavigation.page;
            gHermesFastSetupNavigation.page = gHermesFastSetupNavigation.returnPage;
            drawHermesFastSetup(display, nullptr, 0, 0);
            gHermesFastSetupNavigation.page = popupPage;
        } else {
            applyPaletteNoSelection();
        }
        drawSetupDetailPopupOverlay(display, nullptr);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateRuntimeMenu) {
        HermesXFastSetupUiRenderer::drawUpdateRuntimeMenu(listContext);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateWifiConfigMenu) {
        const String maskedPassword = maskSetupSecret(hermesSetupWifiPasswordDraft.c_str(), 0);
        const String ipLabel = getSetupWifiIpLabel();
        HermesXFastSetupUiRenderer::drawUpdateWifiConfigMenu(
            listContext, hermesSetupWifiEnabledDraft, hermesSetupWifiSsidDraft.c_str(), maskedPassword.c_str(),
            hermesSetupWifiDirty, ipLabel.c_str());
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateWifiMenu) {
        auto &updateManager = HermesXUpdateManager::instance();
        const String connectionStatus = getSetupWifiIpLabel();
        HermesXFastSetupUiRenderer::drawUpdateTransportMenu(
            listContext, true, updateManager.getCurrentVersion().c_str(), connectionStatus.c_str());
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateUploadMenu) {
        auto &updateManager = HermesXUpdateManager::instance();
        const String connectionStatus = getSetupUsbStatusLabel();
        HermesXFastSetupUiRenderer::drawUpdateTransportMenu(
            listContext, false, updateManager.getCurrentVersion().c_str(), connectionStatus.c_str());
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateApplyMenu) {
        applyPaletteNoSelection();
        auto &updateManager = HermesXUpdateManager::instance();
        const bool wifiFlow = (hermesManualUpdateFlow == Screen::HermesManualUpdateFlow::Wifi);
        const bool usbFlow = (hermesManualUpdateFlow == Screen::HermesManualUpdateFlow::Usb);
        if (wifiFlow && WiFi.isConnected() && !isMiniUpdateUploadServerRunning()) {
            startMiniUpdateUploadServer();
        }
        String guideLine;
        if (wifiFlow) {
            if (updateManager.canApply()) {
                guideLine = u8"更新檔已接收完成";
            } else if (updateManager.getLastError().length() > 0) {
                guideLine = u8"更新失敗，請重新傳送";
            } else if (updateManager.getSourceStatus().indexOf(u8"接收更新中") >= 0) {
                guideLine = u8"正在接收韌體";
            } else if (isMiniUpdateUploadServerRunning()) {
                guideLine = u8"請在電腦開始傳送韌體";
            } else if (WiFi.isConnected()) {
                guideLine = u8"正在啟動上傳服務";
            } else {
                guideLine = u8"正在連線 WiFi";
            }
        } else if (usbFlow) {
            if (updateManager.canApply()) {
                guideLine = u8"更新檔已接收完成";
            } else if (updateManager.getLastError().length() > 0) {
                guideLine = u8"更新失敗，請重新傳送";
            } else if (updateManager.getSourceStatus().indexOf(u8"接收更新中") >= 0) {
                guideLine = u8"正在接收韌體";
            } else {
                guideLine = u8"請用 USB-C 從電腦傳送韌體";
            }
        } else {
            guideLine = u8"請先回上一頁按開始更新";
        }
        String targetLine = updateManager.getCandidateVersion().isEmpty() ? u8"待更新版本: 無"
                                                                          : String(u8"待更新版本: ") + updateManager.getCandidateVersion();
        String statusLine = String(u8"狀態: ") + updateManager.getSourceStatus();
        String errorLine = updateManager.getLastError().isEmpty() ? String()
                                                                  : String(u8"錯誤: ") + updateManager.getLastError();
        HermesXFastSetupUiRenderer::drawUpdateProgressPage(
            display, width, height, wifiFlow ? u8"WiFi更新" : u8"USB更新", guideLine,
            updateManager.getProgressPercent(), statusLine, targetLine, errorLine, updateManager.canApply(),
            updateManager.canApply() ? 2 : 1, u8"返回", u8"套用更新", gHermesFastSetupNavigation.selected,
            hermesSetupToast, hermesSetupToastUntilMs);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateWifiSsidEdit) {
        applyPaletteNoSelection();
        const String preview = makeSetupDraftPreview(hermesSetupWifiSsidDraft);
        HermesXFastSetupUiRenderer::drawKeyboardPage(
            display, width, height, "WiFi SSID", preview, getSetupWifiKeyRows(hermesSetupWifiLowercase),
            kSetupWifiKeyRowLengths, kSetupWifiKeyRowCount, hermesSetupKeyRow, hermesSetupKeyCol, hermesSetupToast,
            hermesSetupToastUntilMs);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateWifiPasswordEdit) {
        applyPaletteNoSelection();
        const String preview = makeSetupDraftPreview(hermesSetupWifiPasswordDraft);
        HermesXFastSetupUiRenderer::drawKeyboardPage(
            display, width, height, u8"WiFi密碼", preview, getSetupWifiKeyRows(hermesSetupWifiLowercase),
            kSetupWifiKeyRowLengths, kSetupWifiKeyRowCount, hermesSetupKeyRow, hermesSetupKeyCol, hermesSetupToast,
            hermesSetupToastUntilMs);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::NodeDatabaseMenu) {
        HermesXFastSetupUiRenderer::drawNodeDatabaseMenu(listContext, false);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::NodeDatabaseResetSelect) {
        HermesXFastSetupUiRenderer::drawNodeDatabaseMenu(listContext, true);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::MqttMenu) {
        const bool mapEnabled = moduleConfig.mqtt.map_reporting_enabled && moduleConfig.mqtt.has_map_report_settings &&
                                moduleConfig.mqtt.map_report_settings.should_report_location;
        HermesXFastSetupUiRenderer::drawMqttMenu(
            listContext, moduleConfig.mqtt.enabled, moduleConfig.mqtt.proxy_to_client_enabled, mapEnabled);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::MqttMapReportMenu) {
        const bool mapEnabled = moduleConfig.mqtt.map_reporting_enabled && moduleConfig.mqtt.has_map_report_settings &&
                                moduleConfig.mqtt.map_report_settings.should_report_location;
        const String precisionLabel = getSetupMqttMapPrecisionLabel(getSetupCurrentMqttMapPrecision());
        const String publishLabel = formatSetupSecondsLabel(getSetupCurrentMqttMapPublishInterval());
        HermesXFastSetupUiRenderer::drawMqttMapReportMenu(
            listContext, mapEnabled, precisionLabel.c_str(), publishLabel.c_str());
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::MqttMapPrecisionSelect) {
        const char *labels[kSetupMqttMapPrecisionCount];
        for (uint8_t i = 0; i < kSetupMqttMapPrecisionCount; ++i) {
            labels[i] = kSetupMqttMapPrecisionOptions[i].label;
        }
        HermesXFastSetupUiRenderer::drawSelectionMenu(listContext, u8"精確度", labels, kSetupMqttMapPrecisionCount);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::MqttMapPublishSelect) {
        HermesXFastSetupUiRenderer::drawSelectionMenu(
            listContext, u8"廣播間隔", kSetupMqttMapPublishLabels, kSetupMqttMapPublishCount);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::ChannelMenu) {
        ChannelIndex channelList[MAX_NUM_CHANNELS];
        const uint8_t channelCount = buildSetupChannelList(channelList, MAX_NUM_CHANNELS);
        String itemLabels[MAX_NUM_CHANNELS];
        const char *labels[MAX_NUM_CHANNELS];
        for (uint8_t i = 0; i < channelCount; ++i) {
            itemLabels[i] = getSetupChannelMenuLabel(channelList[i]);
            labels[i] = itemLabels[i].c_str();
        }
        HermesXFastSetupUiRenderer::drawChannelMenu(listContext, labels, channelCount);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::ChannelDetailMenu) {
        const auto channel = getSetupChannelCopy(hermesSetupChannelIndex);
        const bool shareEnabled = isSetupChannelPositionSharingEnabled(hermesSetupChannelIndex);
        const String title = getSetupChannelMenuLabel(hermesSetupChannelIndex);
        const String precisionLabel = getSetupChannelPrecisionLabel(getSetupChannelPrecision(hermesSetupChannelIndex));
        HermesXFastSetupUiRenderer::drawChannelDetailMenu(
            listContext, title.c_str(), channel.settings.uplink_enabled, channel.settings.downlink_enabled,
            shareEnabled, precisionLabel.c_str());
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::ChannelPrecisionSelect) {
        const char *labels[kSetupChannelPrecisionCount];
        for (uint8_t i = 0; i < kSetupChannelPrecisionCount; ++i) {
            labels[i] = kSetupChannelPrecisionOptions[i].label;
        }
        HermesXFastSetupUiRenderer::drawSelectionMenu(listContext, u8"精確度", labels, kSetupChannelPrecisionCount);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::PowerMenu) {
        const String voltageLabel = getSetupCurrentVoltageLabel();
        const String thresholdLabel = formatSetupVoltageMvLabel(HermesXBatteryProtection::getThresholdMv());
        HermesXFastSetupUiRenderer::drawPowerMenu(
            listContext, voltageLabel.c_str(), HermesXBatteryProtection::isEnabled(), thresholdLabel.c_str());
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::PowerGuardVoltageSelect) {
        const char *labels[kSetupPowerGuardThresholdCount];
        for (uint8_t i = 0; i < kSetupPowerGuardThresholdCount; ++i) {
            labels[i] = kSetupPowerGuardThresholdOptions[i].label;
        }
        HermesXFastSetupUiRenderer::drawSelectionMenu(listContext, u8"過放門檻", labels, kSetupPowerGuardThresholdCount);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::LoraMenu) {
        const char *roleLabel = getSetupRoleOptionLabel(config.device.role);
        const char *presetLabel = config.lora.use_preset ? getSetupLoraPresetLabel(config.lora.modem_preset) : "Custom";
        const char *regionLabel = getSetupRegionLabel(config.lora.region);
        const String slotLabel = config.lora.channel_num ? String(config.lora.channel_num) : String(u8"自動");
        String frequencyLabel = formatSetupFrequencyLabel(config.lora.override_frequency);
        if (fabsf(config.lora.override_frequency) >= 0.0001f) {
            frequencyLabel += "MHz";
        }
        HermesXFastSetupUiRenderer::drawLoraMenu(
            listContext, roleLabel, presetLabel, regionLabel, config.lora.ignore_mqtt,
            config.lora.config_ok_to_mqtt, config.lora.tx_enabled, slotLabel.c_str(), frequencyLabel.c_str());
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::LoraRoleSelect) {
        const char *labels[kSetupRoleOptionCount];
        for (uint8_t i = 0; i < kSetupRoleOptionCount; ++i) {
            labels[i] = kSetupRoleOptions[i].label;
        }
        HermesXFastSetupUiRenderer::drawSelectionMenu(listContext, "Role", labels, kSetupRoleOptionCount);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::LoraPresetSelect) {
        const char *labels[kSetupLoraPresetOptionCount];
        for (uint8_t i = 0; i < kSetupLoraPresetOptionCount; ++i) {
            labels[i] = kSetupLoraPresetOptions[i].label;
        }
        HermesXFastSetupUiRenderer::drawSelectionMenu(listContext, "Preset", labels, kSetupLoraPresetOptionCount);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::LoraRegionSelect) {
        const uint8_t regionCount = getSetupRegionOptionCount();
        const char *labels[kSetupMaxRegionOptions];
        for (uint8_t i = 0; i < regionCount && i < kSetupMaxRegionOptions; ++i) {
            labels[i] = regions[i].name;
        }
        HermesXFastSetupUiRenderer::drawSelectionMenu(listContext, u8"地區", labels, regionCount);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::LoraChannelSlotSelect) {
        HermesXFastSetupUiRenderer::drawLoraChannelSlotMenu(listContext, getSetupLoraChannelSlotCount());
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::CannedMenu) {
        const char *channelName = nullptr;
        if (cannedMessageModule) {
            channelName = channels.getName(cannedMessageModule->getPreferredChannel());
        }
        HermesXFastSetupUiRenderer::drawCannedMenu(listContext, channelName);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::CannedChannelSelect) {
        ChannelIndex channelList[MAX_NUM_CHANNELS];
        const uint8_t channelCount = buildSetupChannelList(channelList, MAX_NUM_CHANNELS);
        const char *labels[MAX_NUM_CHANNELS];
        for (uint8_t i = 0; i < channelCount; ++i) {
            const char *name = channels.getName(channelList[i]);
            labels[i] = name ? name : u8"未知";
        }
        HermesXFastSetupUiRenderer::drawSelectionMenu(listContext, u8"目標頻道", labels, channelCount);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::GpsMenu) {
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
        const String distanceLabel = formatSetupDistanceLabel(currentSmartDistance);
        const String intervalLabel = formatSetupSecondsLabel(currentSmartInterval);
        HermesXFastSetupUiRenderer::drawGpsMenu(
            listContext, currentUpdate, updateLabel, currentBroadcast, broadcastLabel,
            config.position.position_broadcast_smart_enabled, distanceLabel.c_str(), intervalLabel.c_str());
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::GpsUpdateSelect) {
        HermesXFastSetupUiRenderer::drawSelectionMenu(
            listContext, u8"衛星更新", kSetupGpsUpdateLabels, kSetupGpsUpdateCount);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::GpsBroadcastSelect) {
        HermesXFastSetupUiRenderer::drawSelectionMenu(
            listContext, u8"廣播時間", kSetupGpsBroadcastLabels, kSetupGpsBroadcastCount);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::GpsSmartDistanceSelect) {
        HermesXFastSetupUiRenderer::drawSelectionMenu(
            listContext, u8"最小距離", kSetupGpsSmartDistanceLabels, kSetupGpsSmartDistanceCount);
        return;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::GpsSmartIntervalSelect) {
        HermesXFastSetupUiRenderer::drawSelectionMenu(
            listContext, u8"最小間隔", kSetupGpsSmartIntervalLabels, kSetupGpsSmartIntervalCount);
        return;
    }

    HermesXFastSetupUiRenderer::drawMenu(listContext, u8"HermesFastSetup", kSetupRootItems, kSetupRootCount);
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
static bool showingBootScreen = true;
static uint32_t bootScreenStartMs = 0;
static bool hermesXBootWelcomeActive = false;
static uint32_t hermesXBootWelcomeStartedAtMs = 0;
static uint32_t hermesXBootWelcomeUntilMs = 0;
static bool hermesXBootWelcomeDirectLogged = false;
static bool hermesXBootHoldDirectLogged = false;
static bool hermesXBootFrameWelcomeLogged = false;
static bool hermesXBootFramePendingLogged = false;
static bool hermesXBootHoldProgressLogged = false;
static bool hermesXBootHoldRevealCompleteLogged = false;
static int8_t hermesXBootHoldProgressLogBucket = -1;
static constexpr uint32_t kHermesXBootWelcomeAnimMs = 3000;
static constexpr uint32_t kHermesXBootWelcomeFinalHoldMs = 1200;
static constexpr uint32_t kHermesXBootWelcomeTotalMs = kHermesXBootWelcomeAnimMs + kHermesXBootWelcomeFinalHoldMs;
static constexpr uint32_t kHermesXBootWelcomeMaxStepMs = 50;
static uint32_t hermesXBootWelcomeAnimElapsedMs = 0;
static uint32_t hermesXBootWelcomeLastRenderAtMs = 0;
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
static constexpr bool kHermesXBootAnimTftDirect = true;
#else
static constexpr bool kHermesXBootAnimTftDirect = false;
#endif
static void drawHermesXBootHoldFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
static void renderDirectHermesPhotoBootFrame(TFTDisplay *tft, int16_t width, int16_t height, uint32_t elapsedMs);
static void playDirectHermesPhotoBootWelcomeBlocking(TFTDisplay *tft, int16_t width, int16_t height);
#endif

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

#if !MESHTASTIC_EXCLUDE_HERMESX && defined(HERMESX_GUARD_POWER_ANIMATIONS)
    const bool hermesGuardEnabled = HermesXPowerGuard::guardEnabled();
    const bool hermesBootHoldPending = HermesXPowerGuard::bootHoldPending();
    const bool hermesWokeFromSleep = HermesXPowerGuard::wokeFromSleep();
#else
    const bool hermesGuardEnabled = false;
    const bool hermesBootHoldPending = false;
    const bool hermesWokeFromSleep = false;
#endif
    const bool showHermesWelcome = hermesGuardEnabled && !hermesBootHoldPending;

#ifdef USERPREFS_OEM_TEXT
    logo_timeout *= 2; // Double the time if we have a custom logo
#endif
#if !MESHTASTIC_EXCLUDE_HERMESX && defined(HERMESX_GUARD_POWER_ANIMATIONS)
    // HermesX expects a strict 3s boot logo window.
    logo_timeout = 3000;
#endif
    bootScreenShowHermesWelcome = showHermesWelcome;
    hermesXBootWelcomeActive = showHermesWelcome;
    hermesXBootWelcomeStartedAtMs = 0;
    hermesXBootWelcomeUntilMs = 0;
    hermesXBootWelcomeAnimElapsedMs = 0;
    hermesXBootWelcomeLastRenderAtMs = 0;
    hermesXBootWelcomeDirectLogged = false;
    hermesXBootHoldDirectLogged = false;
    hermesXBootFrameWelcomeLogged = false;
    hermesXBootFramePendingLogged = false;
    hermesXBootHoldProgressLogged = false;
    hermesXBootHoldRevealCompleteLogged = false;
    hermesXBootHoldProgressLogBucket = -1;
    LOG_INFO("[HermesBootAnim] setup guard=%d pending=%d woke=%d show=%d display=%dx%d timer_deferred=%d",
             hermesGuardEnabled ? 1 : 0, hermesBootHoldPending ? 1 : 0, hermesWokeFromSleep ? 1 : 0,
             showHermesWelcome ? 1 : 0, displayWidth, displayHeight, showHermesWelcome ? 1 : 0);

    // Add frames.
    EINK_ADD_FRAMEFLAG(dispdev, DEMAND_FAST);
    bootScreenFrames[0] = [this](OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) -> void {
#if !MESHTASTIC_EXCLUDE_HERMESX && defined(HERMESX_GUARD_POWER_ANIMATIONS)
        if (bootScreenShowHermesWelcome) {
            if (!hermesXBootFrameWelcomeLogged) {
                hermesXBootFrameWelcomeLogged = true;
                LOG_INFO("[HermesBootAnim] boot callback welcome show=1 tft=%d", kHermesXBootAnimTftDirect ? 1 : 0);
            }
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
            display->setColor(OLEDDISPLAY_COLOR::BLACK);
            display->fillRect(x, y, display->width(), display->height());
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
#else
            drawHermesXBootHoldFrame(display, state, x, y);
#endif
            return;
        }
#endif
#if !MESHTASTIC_EXCLUDE_HERMESX && defined(HERMESX_GUARD_POWER_ANIMATIONS)
        if (HermesXPowerGuard::guardEnabled() && HermesXPowerGuard::bootHoldPending()) {
            if (!hermesXBootFramePendingLogged) {
                hermesXBootFramePendingLogged = true;
                LOG_INFO("[HermesBootAnim] boot callback pending hold dots");
            }
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
    if (showHermesWelcome) {
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
        alertFrames[0] = bootScreenFrames[0];
#else
        alertFrames[0] = drawHermesXBootHoldFrame;
#endif
    } else {
        alertFrames[0] = bootScreenFrames[0];
    }
    ui->setFrames(alertFrames, 1);
    // No overlays.
    ui->setOverlays(nullptr, 0);

    // Require presses to switch between frames.
    ui->disableAutoTransition();

    // Set up a log buffer with 3 lines, 32 chars each.
    dispdev->setLogBuffer(3, 32);

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

#if !MESHTASTIC_EXCLUDE_HERMESX && defined(HERMESX_GUARD_POWER_ANIMATIONS)
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
    bool hermesWelcomePlayedBlocking = false;
    if (bootScreenShowHermesWelcome) {
        auto *tft = static_cast<TFTDisplay *>(dispdev);
        LOG_INFO("[HermesBootAnim] setup direct blocking welcome start");
        playDirectHermesPhotoBootWelcomeBlocking(tft, dispdev->getWidth(), dispdev->getHeight());
        LOG_INFO("[HermesBootAnim] setup direct blocking welcome done; defer meshtastic logo");
        hermesWelcomePlayedBlocking = true;
        hermesXBootWelcomeActive = false;
        hermesXBootWelcomeStartedAtMs = 0;
        hermesXBootWelcomeUntilMs = 0;
        hermesXBootWelcomeAnimElapsedMs = 0;
        hermesXBootWelcomeLastRenderAtMs = 0;
        bootScreenShowHermesWelcome = false;
        bootScreenStartMs = millis();
        bootScreenForceLogo = true;
        showingBootScreen = true;
        showingNormalScreen = false;
        setFrameImmediateDraw(bootScreenFrames);
        setInterval(0);
        runASAP = true;
    }
#endif
#endif

    const bool skipInitialUiUpdateForHermesWelcome =
#if !MESHTASTIC_EXCLUDE_HERMESX && defined(HERMESX_GUARD_POWER_ANIMATIONS) &&                                      \
    (defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) ||              \
     defined(ST7789_CS) || defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS))
        bootScreenShowHermesWelcome || hermesWelcomePlayedBlocking;
#else
        false;
#endif
    LOG_INFO("[HermesBootAnim] setup skip_initial_ui=%d", skipInitialUiUpdateForHermesWelcome ? 1 : 0);

    // On some ssd1306 clones, the first draw command is discarded, so draw it
    // twice initially. Skip this for EINK Displays to save a few seconds during boot
    if (!skipInitialUiUpdateForHermesWelcome) {
        ui->update();
#ifndef USE_EINK
        ui->update();
#endif
    }
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

void Screen::requestImmediateRedraw()
{
    if (!useDisplay || !ui) {
        return;
    }
    enabled = true;
    runASAP = true;
    setInterval(0);
    setFastFramerate();
    if (ui->getUiState()) {
        ui->getUiState()->lastUpdate = 0;
    }
    ui->update();
#ifdef USE_EINK
    forceDisplay(true);
#endif
}

static uint32_t lastScreenTransition;
static bool pendingNormalFrames = false;
static bool hermesXBootHoldActive = false;
static bool hermesXBootHoldReveal = false;
static bool hermesXBootHoldAlertStarted = false;
static uint32_t hermesXBootHoldRevealStartedAtMs = 0;
static uint32_t hermesXBootHoldRevealUntilMs = 0;
static uint32_t hermesXBootHoldHeldMs = 0;
static uint32_t hermesXBootHoldLongMs = 1;
static bool hermesXBootHoldBootScreenPending = false;
static uint32_t hermesXBootHoldBootScreenAtMs = 0;
static uint32_t hermesFinderTftFullRepaintUntilMs = 0;

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

static float easeInOut01(float v)
{
    const float t = clamp01(v);
    return t * t * (3.0f - (2.0f * t));
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

#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
static void drawDirectHermesPhotoBootAnimFrame(TFTDisplay *tft, int16_t width, int16_t height, uint8_t frameIndex)
{
    if (!tft || width <= 0 || height <= 0) {
        return;
    }
    if (frameIndex >= HERMES_PHOTO_BOOT_ANIM_FRAMES) {
        frameIndex = HERMES_PHOTO_BOOT_ANIM_FRAMES - 1;
    }

    static uint16_t row[HERMES_PHOTO_BOOT_ANIM_WIDTH];
    const int16_t drawX = static_cast<int16_t>((width - HERMES_PHOTO_BOOT_ANIM_WIDTH) / 2);
    const int16_t drawY = static_cast<int16_t>((height - HERMES_PHOTO_BOOT_ANIM_HEIGHT) / 2);
    const size_t frameOffset = static_cast<size_t>(frameIndex) * HERMES_PHOTO_BOOT_ANIM_WIDTH * HERMES_PHOTO_BOOT_ANIM_HEIGHT;
    for (int16_t yy = 0; yy < HERMES_PHOTO_BOOT_ANIM_HEIGHT; ++yy) {
        for (int16_t xx = 0; xx < HERMES_PHOTO_BOOT_ANIM_WIDTH; ++xx) {
            row[xx] = pgm_read_word(&hermes_photo_boot_anim_160x80[frameOffset + (yy * HERMES_PHOTO_BOOT_ANIM_WIDTH) + xx]);
        }
        tft->writeRow565(drawX, static_cast<int16_t>(drawY + yy), row, HERMES_PHOTO_BOOT_ANIM_WIDTH);
    }
}

static void drawDirectBootHoldLine(TFTDisplay *tft, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    tft->drawLine565(x0, y0, x1, y1, color);
    tft->drawLine565(x0, static_cast<int16_t>(y0 + 1), x1, static_cast<int16_t>(y1 + 1), color);
}

static void drawDirectBootHoldPartialLine(TFTDisplay *tft,
                                          int16_t x0,
                                          int16_t y0,
                                          int16_t x1,
                                          int16_t y1,
                                          float t,
                                          uint16_t color)
{
    const int16_t ex = lerpI16(x0, x1, t);
    const int16_t ey = lerpI16(y0, y1, t);
    drawDirectBootHoldLine(tft, x0, y0, ex, ey, color);
}

static void drawDirectBootHoldDot(TFTDisplay *tft, int16_t x, int16_t y, uint16_t color)
{
    tft->fillCircle565(x, y, 2, color);
}

static void renderDirectHermesPhotoBootVectorFrame(TFTDisplay *tft, int16_t width, int16_t height, uint32_t elapsedMs)
{
    if (!tft || width <= 0 || height <= 0) {
        return;
    }

    constexpr uint32_t kBootAnimMs = 3000;
    constexpr uint32_t kBootLogoDotsMs = 650;
    constexpr uint32_t kBootLogoLineMs = 1450;
    constexpr uint32_t kBootLogoLineEndMs = kBootLogoDotsMs + kBootLogoLineMs;
    const uint32_t clampedMs = elapsedMs > kBootAnimMs ? kBootAnimMs : elapsedMs;

    tft->fillRect565(0, 0, width, height, TFTDisplay::rgb565(0x00, 0x00, 0x00));
    if (clampedMs >= kBootLogoLineEndMs) {
        drawDirectHermesPhotoBootAnimFrame(tft, width, height, HERMES_PHOTO_BOOT_ANIM_FRAMES - 1);
        return;
    }

    const int16_t drawX = static_cast<int16_t>((width - HERMES_PHOTO_BOOT_ANIM_WIDTH) / 2);
    const int16_t drawY = static_cast<int16_t>((height - HERMES_PHOTO_BOOT_ANIM_HEIGHT) / 2);
    struct DirectPt {
        int16_t x;
        int16_t y;
    };
    static const DirectPt kPts[] = {
        {153, 40}, // right
        {80, 4},   // top
        {8, 40},   // left
        {80, 77},  // bottom
    };
    static const uint8_t kNodeOrder[] = {2, 1, 0, 3};
    static const uint8_t kEdges[][2] = {
        {2, 1}, // left -> top
        {1, 0}, // top -> right
        {2, 3}, // left -> bottom
    };
    const uint16_t lineColor = TFTDisplay::rgb565(0xff, 0xff, 0xf0);

    const uint32_t nodeStepMs = kBootLogoDotsMs / (sizeof(kNodeOrder) / sizeof(kNodeOrder[0]));
    bool nodeVisible[sizeof(kPts) / sizeof(kPts[0])] = {false};
    for (size_t i = 0; i < sizeof(kNodeOrder) / sizeof(kNodeOrder[0]); ++i) {
        if (clampedMs >= i * nodeStepMs) {
            nodeVisible[kNodeOrder[i]] = true;
        }
    }

    if (clampedMs > kBootLogoDotsMs) {
        const float progress =
            easeInOut01(static_cast<float>(clampedMs - kBootLogoDotsMs) / static_cast<float>(kBootLogoLineMs));
        float totalLength = 0.0f;
        float lengths[sizeof(kEdges) / sizeof(kEdges[0])];
        for (size_t i = 0; i < sizeof(kEdges) / sizeof(kEdges[0]); ++i) {
            const DirectPt &a = kPts[kEdges[i][0]];
            const DirectPt &b = kPts[kEdges[i][1]];
            lengths[i] = edgeLength(a.x, a.y, b.x, b.y);
            totalLength += lengths[i];
        }

        float remaining = progress * totalLength;
        for (size_t i = 0; i < sizeof(kEdges) / sizeof(kEdges[0]); ++i) {
            const uint8_t aIndex = kEdges[i][0];
            const uint8_t bIndex = kEdges[i][1];
            const DirectPt &a = kPts[aIndex];
            const DirectPt &b = kPts[bIndex];
            if (remaining >= lengths[i]) {
                drawDirectBootHoldLine(tft, static_cast<int16_t>(drawX + a.x), static_cast<int16_t>(drawY + a.y),
                                       static_cast<int16_t>(drawX + b.x), static_cast<int16_t>(drawY + b.y), lineColor);
                nodeVisible[aIndex] = true;
                nodeVisible[bIndex] = true;
                remaining -= lengths[i];
            } else if (remaining > 0.0f) {
                const float partialT = lengths[i] > 0.0f ? remaining / lengths[i] : 1.0f;
                drawDirectBootHoldPartialLine(tft, static_cast<int16_t>(drawX + a.x), static_cast<int16_t>(drawY + a.y),
                                              static_cast<int16_t>(drawX + b.x), static_cast<int16_t>(drawY + b.y), partialT,
                                              lineColor);
                nodeVisible[aIndex] = true;
                remaining = 0.0f;
            }
        }
    }

    for (size_t i = 0; i < sizeof(kPts) / sizeof(kPts[0]); ++i) {
        if (nodeVisible[i]) {
            drawDirectBootHoldDot(tft, static_cast<int16_t>(drawX + kPts[i].x), static_cast<int16_t>(drawY + kPts[i].y),
                                  lineColor);
        }
    }
}

static void renderDirectHermesPhotoBootFrame(TFTDisplay *tft, int16_t width, int16_t height, uint32_t elapsedMs)
{
    if (!tft || width <= 0 || height <= 0) {
        return;
    }

    renderDirectHermesPhotoBootVectorFrame(tft, width, height, elapsedMs);
}

static void renderDirectHermesPhotoBootProgressFrame(TFTDisplay *tft, int16_t width, int16_t height, uint32_t heldMs, uint32_t longPressMs)
{
    constexpr uint32_t kBootAnimMs = 3000;
    const uint32_t mappedMs = longPressMs > 0 ? static_cast<uint32_t>((static_cast<uint64_t>(heldMs) * kBootAnimMs) / longPressMs) : heldMs;
    renderDirectHermesPhotoBootFrame(tft, width, height, mappedMs);
}

static void playDirectHermesPhotoBootWelcomeBlocking(TFTDisplay *tft, int16_t width, int16_t height)
{
    if (!tft || width <= 0 || height <= 0) {
        return;
    }

    constexpr uint32_t kFrameStepMs = 1000U / 60U;
    uint32_t elapsedMs = 0;
    while (elapsedMs < kHermesXBootWelcomeTotalMs) {
        renderDirectHermesPhotoBootFrame(tft, width, height, elapsedMs);
        delay(kFrameStepMs);
        elapsedMs = std::min<uint32_t>(kHermesXBootWelcomeTotalMs, elapsedMs + kFrameStepMs);
    }
    renderDirectHermesPhotoBootFrame(tft, width, height, kHermesXBootWelcomeTotalMs);
}
#endif

static void drawHermesXBootHoldFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    (void)state;
    if (!display)
        return;

    // Normalized points (0..1) aligned to the photo/video boot logo reference.
    static const BootHoldPoint kPoints[] = {
        {1.00f, 0.38f}, // unused legacy extension point
        {0.90f, 0.40f}, // right node
        {0.50f, 0.08f}, // top node
        {0.07f, 0.40f}, // left node
        {0.50f, 0.80f}, // bottom node
    };
    static const BootHoldEdge kEdges[] = {
        {3, 2}, // left -> top
        {2, 1}, // top -> right
        {3, 4}, // left -> bottom
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

    // Center the main shape (nodes 1..N) within the display.
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
    const uint32_t now = millis();
    uint32_t revealElapsedMs = 0;
    if (hermesXBootHoldReveal) {
        revealElapsedMs = (now >= hermesXBootHoldRevealStartedAtMs) ? (now - hermesXBootHoldRevealStartedAtMs) : 0;
    }

    constexpr uint32_t kBootLogoDotsMs = 650;
    constexpr uint32_t kBootLogoLineMs = 1450;
    const uint32_t lineStartMs = kBootLogoDotsMs;
    const uint32_t lineEndMs = kBootLogoDotsMs + kBootLogoLineMs;
    const bool showFinalLogo = hermesXBootHoldReveal && (revealElapsedMs >= lineEndMs);

    if (showFinalLogo) {
        display->setFont(FONT_LARGE);
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->drawString(x + (display->width() / 2), y + ((display->height() - FONT_HEIGHT_LARGE) / 2), "Hermes");
        return;
    }

    float progress = 0.0f;
    if (hermesXBootHoldReveal) {
        const uint32_t lineElapsedMs = revealElapsedMs > lineStartMs ? (revealElapsedMs - lineStartMs) : 0;
        progress = easeInOut01(static_cast<float>(lineElapsedMs) / static_cast<float>(kBootLogoLineMs));
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
            const uint8_t thickness = 2;
            drawLineThick(display, seg.x0, seg.y0, seg.x1, seg.y1, thickness);
            nodeVisible[seg.startNode] = true;
            if (seg.endsAtNode) {
                nodeVisible[seg.endNode] = true;
            }
        }

        if (partialT > 0.0f && fullSegments < segmentCount) {
            const BootHoldSegment &seg = segments[fullSegments];
            nodeVisible[seg.startNode] = true;
            drawBootHoldPartialLine(display, seg.x0, seg.y0, seg.x1, seg.y1, partialT, 2, !hermesXBootHoldReveal);
        }
    }

    if (hermesXBootHoldReveal) {
        static const uint8_t kRevealNodeOrder[] = {3, 2, 1, 4};
        constexpr uint32_t kNodeStepMs = 150;
        for (size_t i = 0; i < (sizeof(kRevealNodeOrder) / sizeof(kRevealNodeOrder[0])); ++i) {
            if (revealElapsedMs >= (i * kNodeStepMs)) {
                nodeVisible[kRevealNodeOrder[i]] = true;
            }
        }
    }

    for (size_t i = 0; i < (sizeof(nodeVisible) / sizeof(nodeVisible[0])); ++i) {
        if (nodeVisible[i] && kNodeEnabled[i]) {
            display->drawCircle(pts[i].x, pts[i].y, 2);
        }
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
    const bool deferNormalFrames = gatePending || (nodeDB == nullptr) || hermesXBootHoldActive || hermesXBootWelcomeActive;

    if (!stealthRestoreChecked && !deferNormalFrames) {
        logStealthStateProbe("pre-check");
        restoreTakModeAfterBoot();
        restoreStealthModeAfterBoot();
        recoverLegacyStealthCommsIfNeeded();
        gUpdateBootRequested = loadUpdateBootFlag();
        logStealthStateProbe("post-check");
        stealthRestoreChecked = true;
    }

    auto &updateManager = HermesXUpdateManager::instance();
    if (updateManager.poll()) {
        if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateMenu || updateManager.isBusy()) {
            setFastFramerate();
        }
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateIntro) {
        if (hermesUpdateIntroStartedAtMs == 0) {
            hermesUpdateIntroStartedAtMs = millis();
        }
        if (millis() - hermesUpdateIntroStartedAtMs >= kSetupUpdateIntroMs) {
            gHermesFastSetupNavigation.page = HermesFastSetupPage::UpdateMenu;
            gHermesFastSetupNavigation.selected = 0;
            gHermesFastSetupNavigation.offset = 0;
            gHermesFastSetupNavigation.lastNavAtMs = 0;
            gHermesFastSetupNavigation.lastNavDir = 0;
        }
        setFastFramerate();
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateExitPending) {
        if (hermesUpdateIntroStartedAtMs == 0) {
            hermesUpdateIntroStartedAtMs = millis();
        }
        setFastFramerate();
    }

    if (gTakModeUiState.page == graphics::HermesXTakModePage::TransitionEnter ||
        gTakModeUiState.page == graphics::HermesXTakModePage::TransitionExit) {
        if (gTakModeUiState.transitionStartedAtMs == 0) {
            gTakModeUiState.transitionStartedAtMs = millis();
        }
        setFastFramerate();
    }

#if HAS_WIFI && !defined(ARCH_PORTDUINO)
    if ((gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateCheckMenu ||
         gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateCheckFlowPage) &&
        hermesPendingUpdateCheckAction != HermesPendingUpdateCheckAction::None && WiFi.status() == WL_CONNECTED) {
        auto pump = []() {
            if (screen) {
                screen->requestImmediateRedraw();
            }
        };
        const bool wantsDownload = hermesPendingUpdateCheckAction == HermesPendingUpdateCheckAction::Download;
        LOG_INFO("[UpdateCheck] pending action resume=%s wifi=%d", wantsDownload ? "download" : "check",
                 WiFi.status() == WL_CONNECTED ? 1 : 0);
        hermesPendingUpdateCheckAction = HermesPendingUpdateCheckAction::None;
        const bool ok = wantsDownload ? updateManager.downloadRemoteImage(pump) : updateManager.checkRemoteImage(pump);
        LOG_INFO("[UpdateCheck] pending action result ok=%d status=%s err=%s", ok ? 1 : 0, updateManager.getSourceStatus().c_str(),
                 updateManager.getLastError().c_str());
        hermesSetupToast = ok ? updateManager.getSourceStatus()
                              : (updateManager.getLastError().isEmpty() ? updateManager.getSourceStatus()
                                                                        : updateManager.getLastError());
        hermesSetupToastUntilMs = millis() + 2200;
        setFastFramerate();
    }
#endif

    if ((isStealthModeActive() || isTakExperienceActive()) && cannedMessageModule) {
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

    if (hermesXBootHoldBootScreenPending && hermesXBootHoldBootScreenAtMs != 0 && millis() >= hermesXBootHoldBootScreenAtMs &&
        nodeDB != nullptr) {
        LOG_INFO("[HermesBootAnim] boothold pending boot screen resolved nodeDB=1");
        hermesXBootHoldBootScreenPending = false;
        hermesXBootHoldBootScreenAtMs = 0;
        hermesXBootHoldActive = false;
        hermesXBootHoldReveal = false;
        hermesXBootHoldRevealStartedAtMs = 0;
        hermesXBootHoldRevealUntilMs = 0;
        hermesXBootHoldAlertStarted = false;
        hermesXBootHoldHeldMs = 0;
        hermesXBootHoldLongMs = 1;
        hermesXBootWelcomeActive = false;
        hermesXBootWelcomeStartedAtMs = 0;
        hermesXBootWelcomeUntilMs = 0;
        hermesXBootWelcomeAnimElapsedMs = 0;
        hermesXBootWelcomeLastRenderAtMs = 0;
        showingBootScreen = false;
        bootScreenForceLogo = false;
        bootScreenShowHermesWelcome = false;
#ifdef USERPREFS_OEM_TEXT
        showingOEMBootScreen = false;
#endif
        showingNormalScreen = false;
        pendingNormalFrames = true;
    }

    if (!deferNormalFrames && pendingNormalFrames) {
        pendingNormalFrames = false;
        if (!hermesUpdateModalActive) {
            setFrames(gNormalFramesInitializedAfterBoot ? FOCUS_PRESERVE : FOCUS_DEFAULT);
            gNormalFramesInitializedAfterBoot = true;
        }
    }

    if (!deferNormalFrames && gUpdateBootRequested && !gUpdateBootHandled && !hermesUpdateModalActive && !showingBootScreen) {
        LOG_INFO("[UpdateBoot] enter dedicated update environment");
        gUpdateBootHandled = true;
        hermesUpdateModalActive = true;
        showingNormalScreen = false;
        enableUpdateLowLoadMode();
        gHermesFastSetupNavigation.page = HermesFastSetupPage::UpdateIntro;
        hermesUpdateIntroStartedAtMs = millis();
        gHermesFastSetupNavigation.selected = 0;
        gHermesFastSetupNavigation.offset = 0;
        hermesSetupWifiEnabledDraft = config.network.wifi_enabled;
        hermesSetupWifiSsidDraft = config.network.wifi_ssid;
        hermesSetupWifiPasswordDraft = config.network.wifi_psk;
        hermesSetupWifiDirty = false;
        hermesSetupWifiLowercase = false;
        stopMiniUpdateUploadServer();
        HermesXUpdateManager::instance().resetUiSession(true);
        static FrameCallback updateModalFrames[] = {&Screen::drawHermesFastSetupFrame};
        setFrameImmediateDraw(updateModalFrames);
    }

    const HermesXMessagePopupLifecycleDecision popupLifecycle =
        HermesXMessageUiController::instance().updatePopupLifecycle(
            screenOn, isHermesXMainPageActive(), millis(), kIncomingTextPopupMs);
    if (popupLifecycle.activated) {
        fastUntilMs = popupLifecycle.fastUntilMs;
        setFastFramerate();
    } else if (popupLifecycle.dismissed) {
        setFastFramerate();
    }

    if (gTraceRouteRequestState.pending && gTraceRouteRequestState.startedMs != 0 &&
        millis() - gTraceRouteRequestState.startedMs >= kTraceRouteResultTimeoutMs) {
        clearTraceRoutePending();
        if (HermesXInterfaceModule::instance) {
            HermesXInterfaceModule::instance->playNackFail();
        }
        showTraceRoutePopup("TraceRoute", u8"等待回應逾時");
        setFastFramerate();
    }

    auto &nodeBrowserController = HermesXNodeBrowserUiController::instance();
    if ((gFinderPulseState.sendingVisible || hermesFinderUiMode == HermesFinderUiMode::PositionList) && lighthouseModule) {
        const auto pulseResult = lighthouseModule->consumePositionPulseUiResult();
        if (pulseResult == LighthouseModule::PositionPulseUiResult::Success) {
            nodeBrowserController.finishFinderPulseRequest();
            hermesFinderUiMode = HermesFinderUiMode::PositionList;
            gNodeBrowserDataSource.refreshFinder();
            LOG_INFO("[Screen] Finder pulse success -> position list count=%u", static_cast<unsigned>(gFinderNodeState.count));
            gFinderNodeState.listCursor = gFinderNodeState.count > 0 ? 1 : 0;
            gFinderNodeState.selectedIndex = 0;
            gFinderNodeState.detailCursor = 0;
            if (!showingNormalScreen && ui) {
                setFrames(FOCUS_PRESERVE);
            }
            if (!screenOn) {
                handleSetOn(true);
            }
            showFinderListPageSafely(true);
            setFastFramerate();
            requestImmediateRedraw();
        } else if (pulseResult == LighthouseModule::PositionPulseUiResult::Timeout) {
            nodeBrowserController.finishFinderPulseRequest();
            hermesFinderUiMode = HermesFinderUiMode::PositionList;
            gNodeBrowserUiModel.reset(gFinderNodeState);
            LOG_WARN("[Screen] Finder pulse timeout -> position list");
            showFinderListPageSafely(true);
            showTraceRoutePopup(u8"尋人模式", "SEND FAIL");
            setFastFramerate();
            requestImmediateRedraw();
        }
    }

    if (showingNormalScreen && screenOn) {
        const uint32_t freeHeap = memGet.getFreeHeap();
        const uint32_t largest = memGet.getLargestFreeBlock();
        if (releaseLowMemoryProtectionIfRecovered(freeHeap, largest)) {
            gLowMemoryUiModel.setMeasurements(freeHeap, largest);
            snprintf(gLowMemoryUiState.status, sizeof(gLowMemoryUiState.status), u8"Heap 已恢復");
            setFastFramerate();
        }
        const bool lowMemoryDanger = freeHeap < kLowMemoryReminderFreeThreshold || largest < kLowMemoryReminderLargestThreshold;
        const bool confirmedLowMemoryDanger = confirmLowMemoryDanger(lowMemoryDanger, millis());
        if (confirmedLowMemoryDanger) {
            activateLowMemoryProtection(freeHeap, largest);
        }
        if (!gLowMemoryUiState.visible && millis() >= gLowMemoryUiState.suppressUntilMs && confirmedLowMemoryDanger) {
            gLowMemoryUiModel.show(freeHeap, largest);
            playLowMemoryAlert();
            if (ui && framesetInfo.positions.main < framesetInfo.frameCount) {
                ui->switchToFrame(framesetInfo.positions.main);
            }
            LOG_WARN("[LowMemory] protection trigger free=%u largest=%u", freeHeap, largest);
            setFastFramerate();
        }
    }

    if (nodeBrowserController.consumeFinderPulseAutoBroadcast(millis(), kStealthConfirmArmMs)) {
        const bool requestStarted = lighthouseModule && lighthouseModule->requestPositionPulse();
        hermesFinderUiMode = HermesFinderUiMode::Menu;
        nodeBrowserController.setFinderPulseRequestStarted(requestStarted, millis());
        if (requestStarted) {
            showFinderListPageSafely(true);
        }
        if (!requestStarted) {
            if (HermesXInterfaceModule::instance) {
                HermesXInterfaceModule::instance->playNackFail();
            }
            showFinderListPageSafely(true);
            showTraceRoutePopup(u8"尋人模式", "SEND FAIL");
        }
        setFastFramerate();
        requestImmediateRedraw();
    }

    if (!screenOn) { // If we didn't just wake and the screen is still off, then
                     // stop updating until it is on again
        enabled = false;
        return 0;
    }

    if (hermesXBootHoldActive && hermesXBootHoldReveal && hermesXBootHoldRevealUntilMs != 0 &&
        millis() >= hermesXBootHoldRevealUntilMs) {
        hermesXBootHoldBootScreenPending = true;
        hermesXBootHoldBootScreenAtMs = millis();
        if (!hermesXBootHoldRevealCompleteLogged) {
            hermesXBootHoldRevealCompleteLogged = true;
            LOG_INFO("[HermesBootAnim] boothold reveal complete wait_nodeDB=%d", nodeDB == nullptr ? 1 : 0);
        }
    }
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
    if (hermesXBootWelcomeActive) {
        auto *tft = static_cast<TFTDisplay *>(dispdev);
        const uint32_t now = millis();
        if (hermesXBootWelcomeStartedAtMs == 0) {
            hermesXBootWelcomeStartedAtMs = now;
            hermesXBootWelcomeUntilMs = now + kHermesXBootWelcomeTotalMs;
            hermesXBootWelcomeAnimElapsedMs = 0;
            hermesXBootWelcomeLastRenderAtMs = now;
            LOG_INFO("[HermesBootAnim] auto welcome timer start start=%u until=%u anim=%u hold=%u max_step=%u",
                     static_cast<unsigned>(hermesXBootWelcomeStartedAtMs), static_cast<unsigned>(hermesXBootWelcomeUntilMs),
                     static_cast<unsigned>(kHermesXBootWelcomeAnimMs), static_cast<unsigned>(kHermesXBootWelcomeFinalHoldMs),
                     static_cast<unsigned>(kHermesXBootWelcomeMaxStepMs));
        } else {
            const uint32_t rawDelta = now >= hermesXBootWelcomeLastRenderAtMs ? (now - hermesXBootWelcomeLastRenderAtMs) : 0;
            const uint32_t cappedDelta = rawDelta > kHermesXBootWelcomeMaxStepMs ? kHermesXBootWelcomeMaxStepMs : rawDelta;
            hermesXBootWelcomeAnimElapsedMs =
                std::min<uint32_t>(kHermesXBootWelcomeTotalMs, hermesXBootWelcomeAnimElapsedMs + cappedDelta);
            hermesXBootWelcomeLastRenderAtMs = now;
        }
        setFastFramerate();
        if (!hermesXBootWelcomeDirectLogged) {
            hermesXBootWelcomeDirectLogged = true;
            LOG_INFO("[HermesBootAnim] direct auto render start elapsed=%u fps=%u", static_cast<unsigned>(hermesXBootWelcomeAnimElapsedMs),
                     static_cast<unsigned>(targetFramerate));
        }
        renderDirectHermesPhotoBootFrame(tft, dispdev->getWidth(), dispdev->getHeight(), hermesXBootWelcomeAnimElapsedMs);
        if (hermesXBootWelcomeAnimElapsedMs >= kHermesXBootWelcomeTotalMs) {
            const uint32_t realElapsedMs =
                now >= hermesXBootWelcomeStartedAtMs ? (now - hermesXBootWelcomeStartedAtMs) : hermesXBootWelcomeAnimElapsedMs;
            LOG_INFO("[HermesBootAnim] auto welcome done elapsed=%u real=%u -> meshtastic logo",
                     static_cast<unsigned>(hermesXBootWelcomeAnimElapsedMs), static_cast<unsigned>(realElapsedMs));
            hermesXBootWelcomeActive = false;
            hermesXBootWelcomeStartedAtMs = 0;
            hermesXBootWelcomeUntilMs = 0;
            hermesXBootWelcomeAnimElapsedMs = 0;
            hermesXBootWelcomeLastRenderAtMs = 0;
            bootScreenShowHermesWelcome = false;
            bootScreenStartMs = millis();
            bootScreenForceLogo = true;
            showingBootScreen = true;
            showingNormalScreen = false;
            setFrameImmediateDraw(bootScreenFrames);
            setFastFramerate();
        }
        return (1000 / targetFramerate);
    }
    if (hermesXBootHoldActive) {
        auto *tft = static_cast<TFTDisplay *>(dispdev);
        const uint32_t now = millis();
        if (!hermesXBootHoldDirectLogged) {
            hermesXBootHoldDirectLogged = true;
            LOG_INFO("[HermesBootAnim] direct boothold render start reveal=%d held=%u long=%u fps=%u",
                     hermesXBootHoldReveal ? 1 : 0, static_cast<unsigned>(hermesXBootHoldHeldMs),
                     static_cast<unsigned>(hermesXBootHoldLongMs), static_cast<unsigned>(targetFramerate));
        }
        if (hermesXBootHoldReveal) {
            const uint32_t elapsedMs = now >= hermesXBootHoldRevealStartedAtMs ? (now - hermesXBootHoldRevealStartedAtMs) : 0;
            renderDirectHermesPhotoBootFrame(tft, dispdev->getWidth(), dispdev->getHeight(), elapsedMs);
        } else {
            renderDirectHermesPhotoBootProgressFrame(tft, dispdev->getWidth(), dispdev->getHeight(), hermesXBootHoldHeldMs,
                                                     hermesXBootHoldLongMs);
        }
        return (1000 / targetFramerate);
    }
#endif

    bool skipUiUpdate = false;

#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
    HermesXDirectHomeFrameDecision directHomeDecision;
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
    const bool finderTftFrameActive =
        showingNormalScreen && (hermesFinderUiMode == HermesFinderUiMode::Menu || hermesFinderUiMode == HermesFinderUiMode::PositionList) &&
        ((framesetInfo.positions.finderList < framesetInfo.frameCount && currentFrameIndex == framesetInfo.positions.finderList) ||
         (framesetInfo.positions.finderDetail < framesetInfo.frameCount && currentFrameIndex == framesetInfo.positions.finderDetail));
    const bool finderTftFullRepaintActive =
        finderTftFrameActive && hermesFinderTftFullRepaintUntilMs != 0 && millis() < hermesFinderTftFullRepaintUntilMs;
    auto &gpsUiModel = HermesXGpsUiModel::instance();
    const bool onFixedMainFrame = showingNormalScreen && ui->getUiState()->frameState == FIXED &&
                                  framesetInfo.positions.main < framesetInfo.frameCount &&
                                  ui->getUiState()->currentFrame == framesetInfo.positions.main;
    const bool smartPowerHomeActive = isSmartPowerHomeActive();
    auto &messageUiController = HermesXMessageUiController::instance();
    const HermesXMessageOverlayRuntimeDecision messageOverlayRuntime = messageUiController.overlayRuntime();
    const bool emergencyUiActive =
        HermesXInterfaceModule::instance && HermesXInterfaceModule::instance->isEmergencyUiActive();
    auto &homeUiController = HermesXHomeUiController::instance();
    HermesXDirectHomeRuntimeInput directHomeRuntimeInput;
    directHomeRuntimeInput.onFixedMainFrame = onFixedMainFrame;
    directHomeRuntimeInput.smartPowerHomeActive = smartPowerHomeActive;
    directHomeRuntimeInput.directOverlaySupported = isDirectHomePresentationAvailable(dispdev);
    directHomeRuntimeInput.incomingTextPopupActive = messageOverlayRuntime.popupActive;
    directHomeRuntimeInput.emergencyConfirmVisible = gEmergencyConfirmUiState.visible;
    directHomeRuntimeInput.emergencyUiActive = emergencyUiActive;
    directHomeRuntimeInput.lowMemoryReminderVisible = gLowMemoryUiState.visible;
    directHomeRuntimeInput.paletteResetPending = messageOverlayRuntime.paletteRecoveryPending;
    const uint32_t directHomeNowMs = millis();
    const HermesXDirectHomeRuntimeDecision directHomeRuntime =
        homeUiController.evaluateRuntime(directHomeRuntimeInput, directHomeNowMs);
    if (directHomeRuntime.releaseNeonBuffers) {
        freeDirectNeonBuffers();
    }
    HermesXGpsRuntimeInput gpsRuntimeInput;
    gpsRuntimeInput.showingNormalScreen = showingNormalScreen;
    gpsRuntimeInput.frameFixed = ui->getUiState()->frameState == FIXED;
    gpsRuntimeInput.currentFrameIndex = currentFrameIndex;
    gpsRuntimeInput.gpsFrameIndex = framesetInfo.positions.settings;
    gpsRuntimeInput.frameCount = framesetInfo.frameCount;
    gpsRuntimeInput.directOverlaySupported = supportsDirectTftOverlayRendering(dispdev);
    gpsRuntimeInput.releaseWorkspaceWhenInactive = onFixedMainFrame && !smartPowerHomeActive;
    gpsRuntimeInput.layers = getDirectGpsPosterLayers();
    auto &gpsUiController = HermesXGpsUiController::instance();
    const HermesXGpsRuntimeDecision gpsRuntime =
        gpsUiController.evaluateRuntime(gpsRuntimeInput, ensureDirectNeonBuffers, freeDirectNeonBuffers);
    if (gpsRuntime.resetPaletteForFrameSwitch) {
        // Frame just switched to GPS: require one FIXED full repaint before any skip-ui optimization.
        tft->resetColorPalette(true);
        tft->markColorPaletteDirty();
    }
    if (directHomeRuntime.active) {
        HermesXHomeStateInput homeStateInput;
        homeStateInput.rtcSeconds = getValidTime(RTCQuality::RTCQualityNTP, true);
        homeStateInput.battery = readHermesXHomeBatterySource();
        homeStateInput.gpsConnected = gpsStatus && gpsStatus->getIsConnected();
        homeStateInput.satelliteCount = homeStateInput.gpsConnected ? gpsStatus->getNumSatellites() : 0;
        homeStateInput.stealth = isStealthModeActive();
        homeStateInput.role = static_cast<int32_t>(config.device.role);
        HermesXHomeStateSnapshot homeStateSnapshot;
        HermesXHomeStateCollector::instance().collect(homeStateInput, homeStateSnapshot);
        const HermesXHomeBaseState homeBaseState = homeStateSnapshot.baseState();
        directHomeDecision =
            homeUiController.beginActiveFrame(homeBaseState, directHomeNowMs, directHomeRuntime);
        const bool enteringDirectHomeOverlay = directHomeDecision.entering;
        const HermesXHomeBaseDecision &homeBaseDecision = directHomeDecision.base;
        const bool telemetryChanged = homeBaseDecision.telemetryChanged;
        const bool telemetryRefreshDue = homeBaseDecision.telemetryRefreshDue;
        const bool telemetryDirty = homeBaseDecision.telemetryDirty;
        const bool baseDirty = homeBaseDecision.baseDirty;
        if (enteringDirectHomeOverlay || telemetryDirty || baseDirty) {
            LOG_DEBUG("[DirectHome] state enter=%d telemetryChanged=%d telemetryRefreshDue=%d telemetryDirty=%d baseDirty=%d "
                      "basePainted=%d skipUi=%d updateModal=%d showingNormal=%d",
                      enteringDirectHomeOverlay ? 1 : 0, telemetryChanged ? 1 : 0, telemetryRefreshDue ? 1 : 0,
                      telemetryDirty ? 1 : 0, baseDirty ? 1 : 0, directHomeDecision.basePainted ? 1 : 0,
                      skipUiUpdate ? 1 : 0, hermesUpdateModalActive ? 1 : 0, showingNormalScreen ? 1 : 0);
            logDirectHomeNeonSummary("DirectHome state update");
        }

        if (targetFramerate < directHomeRuntime.requiredFps) {
            targetFramerate = directHomeRuntime.requiredFps;
            ui->setTargetFPS(targetFramerate);
        }

        if (enteringDirectHomeOverlay) {
            // First frame when returning to Home must force a full UI-backed repaint, otherwise direct TFT overlays can
            // sit on top of stale pixels from previous frame buffers.
            skipUiUpdate = false;
            HermesXHomeDirectPresenter::enter(tft, initializeDisplayUi, ui);
            LOG_INFO("[DirectHome] entering updateModal=%d showingNormal=%d currentFrame=%u free=%u largest=%u",
                     hermesUpdateModalActive ? 1 : 0, showingNormalScreen ? 1 : 0,
                     ui && ui->getUiState() ? ui->getUiState()->currentFrame : 0xFF, ESP.getFreeHeap(),
                     ESP.getMaxAllocHeap());
            logDirectHomeNeonSummary("DirectHome entering");
        }

        if (directHomeDecision.canSkipUi) {
            skipUiUpdate = true;
        }
    } else {
        if (!onFixedMainFrame) {
            HermesXHomeUiRenderer::resetQuote();
        }
        directHomeDecision = homeUiController.deactivate();
    }

    const bool leftDirectHomeOverlay = directHomeDecision.left;
    if (leftDirectHomeOverlay) {
        skipUiUpdate = false;
        HermesXHomeDirectPresenter::leave(tft,
                                          dispdev->getWidth(),
                                          directHomeDecision.previousDog,
                                          isStealthModeActive() ? stealthFg : normalFg,
                                          normalBg,
                                          initializeDisplayUi,
                                          ui);
    }

    const HermesXGpsPosterVisibilityTransition gpsVisibilityTransition =
        HermesXGpsDirectPresenter::updateVisibility(tft,
                                                    dispdev->getWidth(),
                                                    dispdev->getHeight(),
                                                    gpsUiModel,
                                                    gpsRuntime.visible,
                                                    isStealthModeActive() ? stealthFg : normalFg,
                                                    normalBg,
                                                    initializeDisplayUi,
                                                    ui);
    const bool enteredDirectGpsPoster = gpsVisibilityTransition.entered;
    if (enteredDirectGpsPoster) {
        // Match Home direct-TFT entry behavior: ensure one full UI-backed base redraw before any neon overlay.
        skipUiUpdate = false;
    }
    if (gpsRuntime.visible && !skipUiUpdate) {
        const int16_t renderW = dispdev->getWidth();
        const int16_t renderH = dispdev->getHeight();
        const HermesXGpsStateSnapshot gpsState = collectDirectGpsState(renderW, renderH, gpsStatus);
        if (HermesXGpsDirectPresenter::shouldSkipUi(gpsUiModel,
                                                    gpsState.poster,
                                                    gpsRuntime.layers,
                                                    enteredDirectGpsPoster,
                                                    messageOverlayRuntime.uiSkipBlocked)) {
            skipUiUpdate = true;
        }
    }

    if (messageOverlayRuntime.paletteRecoveryPending) {
        skipUiUpdate = false;
        homeUiController.invalidateForPaletteReset();
        gpsUiModel.invalidatePoster();
        ui->init();
        tft->resetColorPalette(true);
        tft->markColorPaletteDirty();
        if (ui->getUiState()) {
            ui->getUiState()->lastUpdate = 0;
        }
        messageUiController.consumePaletteRecovery();
    }

    if (!skipUiUpdate) {
        tft->clearColorPaletteZones();
        tft->setColorPaletteDefaults(isStealthModeActive() ? stealthFg : normalFg, normalBg);
    }
    if (finderTftFullRepaintActive) {
        skipUiUpdate = false;
        tft->clearColorPaletteZones();
        tft->setColorPaletteDefaults(normalFg, normalBg);
        tft->markColorPaletteDirty();
        if (ui->getUiState()) {
            ui->getUiState()->lastUpdate = 0;
        }
    }
    if (directHomeDecision.baseRedrawRequested && ui && ui->getUiState() && ui->getUiState()->frameState == FIXED) {
        // The dog is rendered directly to TFT after the UI pass. Force the dirty Home base through
        // OLEDDisplayUi's frame budget so basePainted can become true instead of waiting forever
        // while the 60 FPS dog loop keeps running just ahead of the scheduled UI tick.
        ui->getUiState()->lastUpdate = 0;
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
    if (finderTftFullRepaintActive && uiRenderedThisTick) {
        tft->overlayBufferForeground565();
        LOG_INFO("[Screen] Finder TFT foreground overlay frame=%u count=%u",
                 ui->getUiState() ? ui->getUiState()->currentFrame : 0xFF, static_cast<unsigned>(gFinderNodeState.count));
    }
    gpsUiModel.markUiFrameRendered(gpsRuntime.onFixedGpsFrame, uiRenderedThisTick);
    homeUiController.markUiFrameRendered(directHomeDecision.active, uiRenderedThisTick);
    if (directHomeDecision.active) {
        HermesXHomeDirectPresenter::renderDog(tft,
                                              dispdev->getWidth(),
                                              dispdev->getHeight(),
                                              homeUiController,
                                              directHomeDecision.dogFrame,
                                              directHomeDecision.forceDogRedraw);
    }
    if (gpsRuntime.visible) {
        const int16_t renderW = dispdev->getWidth();
        const int16_t renderH = dispdev->getHeight();
        const HermesXGpsStateSnapshot gpsState = collectDirectGpsState(renderW, renderH, gpsStatus);
        const bool gpsPosterDirty = gpsUiModel.isPosterDirty(gpsState.poster, gpsRuntime.layers, enteredDirectGpsPoster);
        const bool gpsPosterNeedsRepaint = gpsUiModel.isBasePainted() && (gpsPosterDirty || uiRenderedThisTick);
        if (gpsPosterNeedsRepaint) {
            // GPS base frame is still drawn by ui->update(); re-apply enabled direct layers after any UI repaint.
            HermesXGpsDirectRenderer::render(tft, gpsState, gpsRuntime.layers, ensureDirectNeonBuffers);
            gpsUiModel.commitPosterState(gpsState.poster);
        }
    } else {
        gpsUiModel.invalidateBase();
    }
    gpsUiModel.commitVisibility(gpsRuntime.visible);
    gpsUiController.commitFrame(currentFrameIndex);
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
        !HermesXHomeUiController::instance().isVisible()) {
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
        if (!isRecentTextMessagesPageActive() && !isRecentTextMessageDetailPageActive() &&
            config.display.auto_screen_carousel_secs > 0 &&
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
    const uint32_t effectiveLongMs = longPressMs ? longPressMs : 1;
    const uint32_t rawBucket = (heldMs >= effectiveLongMs) ? 4 : ((heldMs * 4) / effectiveLongMs);
    const int8_t progressBucket = static_cast<int8_t>(rawBucket > 4 ? 4 : rawBucket);
    if (!hermesXBootHoldProgressLogged || progressBucket != hermesXBootHoldProgressLogBucket) {
        hermesXBootHoldProgressLogged = true;
        hermesXBootHoldProgressLogBucket = progressBucket;
        LOG_INFO("[HermesBootAnim] boothold progress held=%u long=%u bucket=%d", static_cast<unsigned>(heldMs),
                 static_cast<unsigned>(effectiveLongMs), static_cast<int>(progressBucket));
    }
    hermesXBootHoldActive = true;
    hermesXBootHoldReveal = false;
    hermesXBootHoldRevealStartedAtMs = 0;
    hermesXBootHoldRevealUntilMs = 0;
    hermesXBootHoldHeldMs = heldMs;
    hermesXBootHoldLongMs = effectiveLongMs;
    if (!hermesXBootHoldAlertStarted) {
        hermesXBootHoldAlertStarted = true;
        LOG_INFO("[HermesBootAnim] boothold start alert callback");
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

void Screen::showEmergencyConfirmPopup(uint32_t remainingSec)
{
    gEmergencyConfirmUiModel.show(remainingSec);
    setFastFramerate();
}

void Screen::updateEmergencyConfirmPopup(uint32_t remainingSec)
{
    gEmergencyConfirmUiModel.update(remainingSec);
    if (gEmergencyConfirmUiState.visible) {
        setFastFramerate();
    }
}

void Screen::hideEmergencyConfirmPopup()
{
    gEmergencyConfirmUiModel.hide();
    setFastFramerate();
}

bool Screen::consumeEmergencyConfirmCancelRequest()
{
    return gEmergencyConfirmUiModel.consumeCancelRequest();
}

bool Screen::isEmergencyConfirmPopupVisible() const
{
    return gEmergencyConfirmUiState.visible;
}

void Screen::setRotaryLockState(bool locked)
{
    gRotaryLockUiModel.show(locked, millis());
    if (!screenOn) {
        setOn(true);
    }
    setFastFramerate();
    requestImmediateRedraw();
}

bool Screen::isRotaryLocked() const
{
    return gRotaryLockUiState.locked;
}

bool Screen::isRotaryLockPopupVisible() const
{
    return gRotaryLockUiState.visible;
}

void Screen::startBootHoldReveal(uint32_t revealMs)
{
    LOG_INFO("[HermesBootAnim] boothold reveal start duration=%u", static_cast<unsigned>(revealMs ? revealMs : 1));
    hermesXBootHoldActive = true;
    hermesXBootHoldReveal = true;
    hermesXBootHoldDirectLogged = false;
    hermesXBootHoldRevealCompleteLogged = false;
    hermesXBootHoldHeldMs = hermesXBootHoldLongMs;
    const uint32_t duration = revealMs ? revealMs : 1;
    hermesXBootHoldRevealStartedAtMs = millis();
    hermesXBootHoldRevealUntilMs = hermesXBootHoldRevealStartedAtMs + duration;
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
    hermesXBootHoldAlertStarted = true;
#else
    if (!hermesXBootHoldAlertStarted) {
        hermesXBootHoldAlertStarted = true;
        startAlert(drawHermesXBootHoldFrame);
    }
#endif
    setFastFramerate();
}

void Screen::finishBootHoldToBootLogo()
{
    LOG_INFO("[HermesBootAnim] finish boothold -> meshtastic logo");
    hermesXBootHoldActive = false;
    hermesXBootHoldReveal = false;
    hermesXBootHoldRevealStartedAtMs = 0;
    hermesXBootHoldRevealUntilMs = 0;
    hermesXBootHoldBootScreenPending = false;
    hermesXBootHoldBootScreenAtMs = 0;
    hermesXBootHoldAlertStarted = false;
    hermesXBootHoldHeldMs = 0;
    hermesXBootHoldLongMs = 1;
    hermesXBootWelcomeActive = false;
    hermesXBootWelcomeStartedAtMs = 0;
    hermesXBootWelcomeUntilMs = 0;
    hermesXBootWelcomeAnimElapsedMs = 0;
    hermesXBootWelcomeLastRenderAtMs = 0;
    hermesXBootWelcomeDirectLogged = false;
    hermesXBootHoldDirectLogged = false;
    hermesXBootFrameWelcomeLogged = false;
    hermesXBootFramePendingLogged = false;
    hermesXBootHoldProgressLogged = false;
    hermesXBootHoldRevealCompleteLogged = false;
    hermesXBootHoldProgressLogBucket = -1;
    bootScreenStartMs = millis();
    showingBootScreen = true;
    bootScreenForceLogo = true;
    bootScreenShowHermesWelcome = false;
#ifdef USERPREFS_OEM_TEXT
    showingOEMBootScreen = true;
#endif
    showingNormalScreen = false;
    setFrameImmediateDraw(bootScreenFrames);
    setFastFramerate();
}

void Screen::resetBootHoldProgress()
{
    LOG_INFO("[HermesBootAnim] boothold reset progress");
    hermesXBootHoldActive = true;
    hermesXBootHoldReveal = false;
    hermesXBootHoldDirectLogged = false;
    hermesXBootHoldRevealCompleteLogged = false;
    hermesXBootHoldRevealStartedAtMs = 0;
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
    fsi.positions.fault = 0xFF;
    fsi.positions.textMessageList = 0xFF;
    fsi.positions.textMessage = 0xFF;
    fsi.positions.onlineList = 0xFF;
    fsi.positions.onlineDetail = 0xFF;
    fsi.positions.traceRouteList = 0xFF;
    fsi.positions.traceRouteDetail = 0xFF;
    fsi.positions.finderList = 0xFF;
    fsi.positions.finderDetail = 0xFF;
    fsi.positions.groupList = 0xFF;
    fsi.positions.groupDetail = 0xFF;
    fsi.positions.takMode = 0xFF;
    fsi.positions.waypoint = 0xFF;
    fsi.positions.focusedModule = 0;
    fsi.positions.main = 0xFF;
    fsi.positions.mainAction = 0xFF;
    fsi.positions.setup = 0xFF;
    fsi.positions.share = 0xFF;
    fsi.positions.log = 0xFF;
    fsi.positions.settings = 0xFF;
    fsi.positions.wifi = 0xFF;

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
    // Recent Send pages are action-menu destinations, not part of the normal frame carousel.
    fsi.positions.textMessageList = numframes;
    normalFrames[numframes++] = drawRecentTextMessagesFrame;
    if (hasRecentTextMessages()) {
        fsi.positions.textMessage = numframes;
        normalFrames[numframes++] = drawTextMessageFrame;
    }

    // ONLINE pages are also action-menu destinations only.
    fsi.positions.onlineList = numframes;
    normalFrames[numframes++] = &Screen::drawOnlineNodeListFrame;
    fsi.positions.onlineDetail = numframes;
    normalFrames[numframes++] = &Screen::drawOnlineNodeDetailFrame;
    fsi.positions.traceRouteList = numframes;
    normalFrames[numframes++] = &Screen::drawTraceRouteNodeListFrame;
    fsi.positions.traceRouteDetail = numframes;
    normalFrames[numframes++] = &Screen::drawTraceRouteNodeDetailFrame;
    fsi.positions.finderList = numframes;
    normalFrames[numframes++] = &Screen::drawFinderNodeListFrame;
    fsi.positions.finderDetail = numframes;
    normalFrames[numframes++] = &Screen::drawFinderNodeDetailFrame;
    fsi.positions.groupList = numframes;
    normalFrames[numframes++] = &Screen::drawGroupNodeListFrame;
    fsi.positions.groupDetail = numframes;
    normalFrames[numframes++] = &Screen::drawGroupNodeDetailFrame;
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
        drawEmergencyConfirmOverlay,
        drawRotaryLockOverlay,
        drawFinderPulseConfirmOverlay,
        drawFinderPulseSendingOverlay,
        drawIncomingTextPopupOverlay,
        drawTraceRoutePopupOverlay,
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
        else if (canMap(oldFsi.positions.onlineList, fsi.positions.onlineList))
            ui->switchToFrame(fsi.positions.onlineList);
        else if (canMap(oldFsi.positions.onlineDetail, fsi.positions.onlineDetail))
            ui->switchToFrame(fsi.positions.onlineDetail);
        else if (canMap(oldFsi.positions.traceRouteList, fsi.positions.traceRouteList))
            ui->switchToFrame(fsi.positions.traceRouteList);
        else if (canMap(oldFsi.positions.traceRouteDetail, fsi.positions.traceRouteDetail))
            ui->switchToFrame(fsi.positions.traceRouteDetail);
        else if (canMap(oldFsi.positions.finderList, fsi.positions.finderList))
            ui->switchToFrame(fsi.positions.finderList);
        else if (canMap(oldFsi.positions.finderDetail, fsi.positions.finderDetail))
            ui->switchToFrame(fsi.positions.finderDetail);
        else if (canMap(oldFsi.positions.groupList, fsi.positions.groupList))
            ui->switchToFrame(fsi.positions.groupList);
        else if (canMap(oldFsi.positions.groupDetail, fsi.positions.groupDetail))
            ui->switchToFrame(fsi.positions.groupDetail);
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

    // These pages own their input; do not advance the normal frame carousel on raw press.
    if (isHermesFastSetupActive() || isHermesXActionPageActive() || isTakModePageActive() || isSmartPowerHomeActive() ||
        isRecentTextMessageDetailPageActive()) {
        return;
    }
    // If Canned Messages is using the "Scan and Select" input, dismiss the canned message frame when user button is pressed
    // Minimize impact as a courtesy, as "scan and select" may be used as default config for some boards
    if (scanAndSelectInput != nullptr && scanAndSelectInput->dismissCannedMessageFrame())
        return;

    // If screen was off, wake it first. This matters for Stealth, where the PowerFSM
    // state can stay ON while the screen itself has been forcibly blanked.
    if (!screenOn) {
        if (isStealthModeActive()) {
            armStealthWakeWindow();
        }
        handleSetOn(true);
        enabled = true;
        setFastFramerate();
        return;
    }

    if (isRecentTextMessagesPageActive()) {
        clampRecentTextMessageIndices();
        if (gRecentTextMessageState.listCursor == 0) {
            showHermesXActionPage();
        } else {
            gRecentTextMessageState.selectedIndex = gRecentTextMessageState.listCursor - 1;
            showTextMessageDetailPage();
        }
        return;
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
    if (!screenOn) {
        if (isStealthModeActive()) {
            armStealthWakeWindow();
        }
        handleSetOn(true);
        enabled = true;
        setFastFramerate();
        return;
    }
    if (isRecentTextMessagesPageActive() || isRecentTextMessageDetailPageActive()) {
        return;
    }
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
    if (!screenOn) {
        if (isStealthModeActive()) {
            armStealthWakeWindow();
        }
        handleSetOn(true);
        enabled = true;
        setFastFramerate();
        return;
    }
    if (isRecentTextMessagesPageActive() || isRecentTextMessageDetailPageActive()) {
        return;
    }
    // If we are in a transition, the press must have bounced, drop it.
    if (ui->getUiState()->frameState == FIXED) {
        ui->nextFrame();
        lastScreenTransition = millis();
        setFastFramerate();
    }
}

#ifndef SCREEN_TRANSITION_FRAMERATE
#define SCREEN_TRANSITION_FRAMERATE 60 // fps
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
        hermesSetupReturnToGroupMenu = false;
        gHermesFastSetupNavigation.page = page;
        gHermesFastSetupNavigation.selected = 0;
        gHermesFastSetupNavigation.offset = 0;
        gHermesFastSetupNavigation.lastNavAtMs = 0;
        gHermesFastSetupNavigation.lastNavDir = 0;
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
        dismissIncomingTextPopup();
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
    auto openOnlineNodeList = [&]() {
        gNodeBrowserDataSource.refreshOnline();
        gNodeBrowserUiModel.reset(gOnlineNodeState);
        return showOnlineNodeListPage();
    };
    auto openTraceRouteNodeList = [&]() {
        gNodeBrowserDataSource.refreshTraceRoute();
        HermesXTraceRouteUiModel::instance().resetSearch();
        HermesXTraceRouteUiModel::instance().resetNavigation();
        return showTraceRouteNodeListPage();
    };
    auto returnToPrimaryFeatureEntry = [&]() {
        hermesActionFeatureMenuActive = false;
        hermesActionSelected = kMainActionFeatureEntryIndex;
        hermesActionLastNavAtMs = 0;
        hermesActionLastNavDir = 0;
    };
    const uint8_t currentActionCount = hermesActionFeatureMenuActive ? kMainActionFeatureCount : kMainActionPrimaryCount;
    const uint8_t *currentActionOrder = hermesActionFeatureMenuActive ? kMainActionFeatureOrder : kMainActionPrimaryOrder;
    if (hermesActionSelected < 0 || hermesActionSelected >= static_cast<int8_t>(currentActionCount)) {
        hermesActionSelected = 0;
    }

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
        if (hermesActionFeatureMenuActive) {
            returnToPrimaryFeatureEntry();
            setFastFramerate();
            return true;
        }
        showNextFrame();
        setFastFramerate();
        return true;
    }

    if (navDir != 0 && allowNav(navDir)) {
        int selected = hermesActionSelected;
        selected += navDir;
        while (selected < 0) {
            selected += currentActionCount;
        }
        selected %= currentActionCount;
        hermesActionSelected = selected;
        setFastFramerate();
        return true;
    }

    if (!isSelect && !isPress) {
        return false;
    }

    const uint8_t selectedAction = currentActionOrder[hermesActionSelected];
    if (selectedAction == kMainActionFeatureId) {
        hermesActionFeatureMenuActive = true;
        hermesActionSelected = 0;
        hermesActionLastNavAtMs = 0;
        hermesActionLastNavDir = 0;
    } else if (selectedAction == kMainActionFeatureExitId) {
        returnToPrimaryFeatureEntry();
    } else if (selectedAction == 0) {
        bool needsReboot = false;
        if (!isStealthModeActive()) {
            if (isTakExperienceActive()) {
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
    } else if (selectedAction == 1) {
        if (HermesXInterfaceModule::instance) {
            const bool next = !HermesXInterfaceModule::instance->isEmergencyLampEnabled();
            HermesXInterfaceModule::instance->setEmergencyLampEnabled(next);
        }
    } else if (selectedAction == 2) {
        if (ui && framesetInfo.positions.settings < framesetInfo.frameCount) {
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
            // Entering GPS from Action must force one full repaint, otherwise
            // direct-GPS cache can occasionally leave only the title neon on top
            // of the previous page.
            HermesXGpsUiModel::instance().invalidate();
            HermesXGpsUiModel::instance().clearVisibility();
            if (supportsDirectTftOverlayRendering(dispdev)) {
                auto *tft = static_cast<TFTDisplay *>(dispdev);
                tft->resetColorPalette(true);
                tft->markColorPaletteDirty();
            }
#endif
            ui->switchToFrame(framesetInfo.positions.settings);
        } else if (screen) {
            screen->print("GPS page unavailable\n");
        }
    } else if (selectedAction == 3) {
        if (isStealthModeActive()) {
            if (screen) {
                screen->print("Disable Stealth first\n");
            }
        } else if (!isTakExperienceActive()) {
            if (enableTakMode()) {
                if (screen) {
                    screen->print("TAK MODE ON, rebooting...\n");
                }
                startTakModeTransition(true);
                rebootAtMsec = millis() + kTakModeTransitionRebootMs;
            }
        } else {
            gTakModeUiModel.showPopup();
            showHermesXMainPage();
        }
    } else if (selectedAction == 4) {
        if (screen) {
            screen->print("Sleeping...\n");
        }
        runPreDeepSleepHook(SleepPreHookParams{BUTTON_LONGPRESS_MS});
        ::doDeepSleep(Default::getConfiguredOrDefaultMs(config.power.sds_secs), false, false);
    } else if (selectedAction == 5) {
        openHermesMainFrame();
    } else if (selectedAction == 6) {
        openHermesShareFrame();
    } else if (selectedAction == 7) {
        openHermesFastSetupRoot();
    } else if (selectedAction == 8) {
        openRecentTextMessageList();
    } else if (selectedAction == 9) {
        hermesFinderUiMode = HermesFinderUiMode::None;
        openOnlineNodeList();
    } else if (selectedAction == 10) {
        hermesFinderUiMode = HermesFinderUiMode::None;
        openTraceRouteNodeList();
    } else if (selectedAction == 11) {
        gGroupNodeState.menuCursor = 1;
        gGroupNodeState.nodeListVisible = false;
        gNodeBrowserUiModel.resetGroupList();
        hermesSetupReturnToGroupMenu = false;
        showGroupNodeListPage();
    } else if (selectedAction == 12) {
        hermesFinderUiMode = HermesFinderUiMode::Menu;
        hermesFinderMenuSelected = 1;
        showFinderListPageSafely(true);
    }

    setFastFramerate();
    return true;
}

bool Screen::handleLowMemoryReminderInput(const InputEvent *event)
{
    if (!event) {
        return false;
    }

    const auto action = graphics::HermesXLowMemoryUiController::instance().handle(
        {event->inputEvent},
        {static_cast<char>(moduleConfig.canned_message.inputbroker_event_press),
         static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw),
         static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw)},
        millis(), kLowMemoryReminderSuppressMs);

    if (action == graphics::HermesXLowMemoryAction::Unhandled) {
        return false;
    }
    if (action == graphics::HermesXLowMemoryAction::Consumed) {
        return true;
    }
    if (action == graphics::HermesXLowMemoryAction::Refresh ||
        action == graphics::HermesXLowMemoryAction::Dismissed) {
        setFastFramerate();
        return true;
    }

    if (nodeDB) {
        const int beforeCount = static_cast<int>(nodeDB->getNumMeshNodes());
        nodeDB->resetNodes();
        const int removed = beforeCount > 0 ? beforeCount - 1 : 0;
        snprintf(gLowMemoryUiState.status, sizeof(gLowMemoryUiState.status), u8"已清除 %d 筆節點", removed);
        const uint32_t freeHeap = memGet.getFreeHeap();
        const uint32_t largestBlock = memGet.getLargestFreeBlock();
        gLowMemoryUiModel.setMeasurements(freeHeap, largestBlock);
        if (releaseLowMemoryProtectionIfRecovered(freeHeap, largestBlock)) {
            snprintf(gLowMemoryUiState.status, sizeof(gLowMemoryUiState.status), u8"已清除 %d 筆，Heap恢復", removed);
        }
        LOG_WARN("[LowMemory] protection cleared NodeDB removed=%d free=%u largest=%u", removed, freeHeap, largestBlock);
    } else {
        snprintf(gLowMemoryUiState.status, sizeof(gLowMemoryUiState.status), u8"NodeDB 尚未就緒");
    }
    setFastFramerate();
    return true;
}

bool Screen::handleEmergencyConfirmInput(const InputEvent *event)
{
    if (!event) {
        return false;
    }
    const auto action = graphics::HermesXEmergencyConfirmUiController::instance().handle(
        event->inputEvent,
        {static_cast<char>(moduleConfig.canned_message.inputbroker_event_press),
         static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw),
         static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw)});
    if (action == graphics::HermesXEmergencyConfirmAction::Canceled) {
        setFastFramerate();
        return true;
    }
    return action != graphics::HermesXEmergencyConfirmAction::Unhandled;
}

bool Screen::handleRotaryLockInput(const InputEvent *event)
{
    if (!event) {
        return false;
    }
    const bool isRotary = (event->source && strncmp(event->source, "rotEnc", 6) == 0);
    const auto action = graphics::HermesXRotaryLockUiController::instance().handle(
        event->inputEvent, isRotary, millis(),
        {static_cast<char>(moduleConfig.canned_message.inputbroker_event_press),
         static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw),
         static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw)});
    if (action == graphics::HermesXRotaryLockAction::SelectionChanged ||
        action == graphics::HermesXRotaryLockAction::Confirmed ||
        action == graphics::HermesXRotaryLockAction::Canceled) {
        setFastFramerate();
    }
    if (action == graphics::HermesXRotaryLockAction::Confirmed ||
        action == graphics::HermesXRotaryLockAction::Canceled) {
        requestImmediateRedraw();
    }
    return action != graphics::HermesXRotaryLockAction::Unhandled;
}

bool Screen::handleFinderPulseConfirmInput(const InputEvent *event)
{
    if (!event) {
        return false;
    }

    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);
    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
    const HermesXFinderPulseAction action = HermesXNodeBrowserUiController::instance().handleFinderPulseConfirm(
        {event->source, event->inputEvent}, {eventPress, eventCw, eventCcw}, millis(), kStealthConfirmArmMs);
    if (action == HermesXFinderPulseAction::CancelRequested) {
        if (lighthouseModule) {
            lighthouseModule->cancelPositionPulseRequest();
            (void)lighthouseModule->consumePositionPulseUiResult();
        }
        hermesFinderUiMode = HermesFinderUiMode::Menu;
        hermesFinderMenuSelected = 1;
        showFinderListPageSafely(true);
        setFastFramerate();
        requestImmediateRedraw();
        return true;
    }
    if (action == HermesXFinderPulseAction::Refresh) {
        setFastFramerate();
        return true;
    }
    if (action == HermesXFinderPulseAction::BroadcastRequested) {
        const bool requestStarted = lighthouseModule && lighthouseModule->requestPositionPulse();
        HermesXNodeBrowserUiController::instance().setFinderPulseRequestStarted(requestStarted, millis());
        hermesFinderUiMode = HermesFinderUiMode::Menu;
        if (requestStarted) {
            showFinderListPageSafely(true);
        } else {
            if (HermesXInterfaceModule::instance) {
                HermesXInterfaceModule::instance->playNackFail();
            }
            showTraceRoutePopup(u8"尋人模式", "SEND FAIL");
        }
        setFastFramerate();
        requestImmediateRedraw();
        return true;
    }
    return action != HermesXFinderPulseAction::Unhandled;
}

bool Screen::handleFinderPulseSendingInput(const InputEvent *event)
{
    if (!event) {
        return false;
    }

    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);
    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);

    const HermesXFinderPulseAction action = HermesXNodeBrowserUiController::instance().handleFinderPulseSending(
        {event->source, event->inputEvent}, {eventPress, eventCw, eventCcw});
    if (action == HermesXFinderPulseAction::CancelRequested) {
        if (lighthouseModule) {
            lighthouseModule->cancelPositionPulseRequest();
            (void)lighthouseModule->consumePositionPulseUiResult();
        }
        hermesFinderUiMode = HermesFinderUiMode::Menu;
        hermesFinderMenuSelected = 1;
        showFinderListPageSafely(true);
        setFastFramerate();
        requestImmediateRedraw();
        return true;
    }
    return action != HermesXFinderPulseAction::Unhandled;
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
        HermesXFastSetupUiModel::instance().reset(page);
    };
    auto openSetupDetailPage = [&](const char *title, const String &body, HermesFastSetupPage returnPage) {
        LOG_INFO("[UpdateDetailPopup] open title=%s return=%d bodyLen=%u", title ? title : "",
                 static_cast<int>(returnPage), static_cast<unsigned>(body.length()));
        showSetupDetailPopup(title, body);
        HermesXFastSetupUiModel::instance().openDetail(returnPage);
    };

    auto enterMenu = [&](HermesFastSetupPage page, int count, int selected) {
        HermesXFastSetupUiModel::instance().enter(page, count, selected);
    };

    auto saveSetupSegments = [&](int saveWhat) {
        if (nodeDB) {
            nodeDB->saveToDisk(saveWhat);
        }
    };

    auto refreshLocalOwnerNode = [&]() {
        if (nodeDB) {
            meshtastic_NodeInfoLite *local = nodeDB->getMeshNode(nodeDB->getNodeNum());
            if (local) {
                local->user = TypeConversions::ConvertToUserLite(owner);
                local->has_user = true;
            }
        }
    };

    auto saveDeviceInfoName = [&](bool shortName) {
        const String value = hermesSetupDeviceInfoDraft;
        if (value.length() == 0) {
            hermesSetupToast = u8"不可空白";
            hermesSetupToastUntilMs = millis() + 1500;
            return false;
        }
        if (shortName) {
            strlcpy(owner.short_name, value.c_str(), sizeof(owner.short_name));
            hermesSetupToast = u8"裝置ID已更新";
        } else {
            strlcpy(owner.long_name, value.c_str(), sizeof(owner.long_name));
            hermesSetupToast = u8"裝置名稱已更新";
        }
        refreshLocalOwnerNode();
        saveSetupSegments(SEGMENT_DEVICESTATE | SEGMENT_NODEDATABASE);
        if (nodeInfoModule) {
            nodeInfoModule->sendOurNodeInfo(NODENUM_BROADCAST, false, 0, true);
        }
        hermesSetupToastUntilMs = millis() + 1500;
        return true;
    };

    auto refreshWifiConnection = [&]() {
#if HAS_WIFI && !defined(ARCH_PORTDUINO)
        if (config.network.wifi_enabled && config.network.wifi_ssid[0]) {
            requestWifiForUpdateMode();
            hermesSetupToast = u8"WiFi 重連中";
        } else {
            releaseWifiForUpdateMode();
            hermesSetupToast = u8"WiFi 已關閉";
        }
        hermesSetupToastUntilMs = millis() + 1800;
#else
        hermesSetupToast = u8"此平台不支援 WiFi";
        hermesSetupToastUntilMs = millis() + 1800;
#endif
    };

    auto storeWifiConfigDraft = [&](bool updatePassword) {
        (void)updatePassword;
        config.network.wifi_enabled = hermesSetupWifiEnabledDraft;
        strlcpy(config.network.wifi_ssid, hermesSetupWifiSsidDraft.c_str(), sizeof(config.network.wifi_ssid));
        strlcpy(config.network.wifi_psk, hermesSetupWifiPasswordDraft.c_str(), sizeof(config.network.wifi_psk));
        saveSetupSegments(SEGMENT_CONFIG);
        hermesSetupWifiDirty = false;
        hermesSetupToast = u8"WiFi 設定已儲存";
        hermesSetupToastUntilMs = millis() + 1800;
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
    auto enterUpdateModeModal = [&]() {
        hermesUpdateModalActive = true;
        showingNormalScreen = false;
        LOG_INFO("[UpdateModal] enter showingNormalScreen=%d currentFrame=%u free=%u largest=%u",
                 showingNormalScreen ? 1 : 0, ui && ui->getUiState() ? ui->getUiState()->currentFrame : 0xFF,
                 ESP.getFreeHeap(), ESP.getMaxAllocHeap());
        static FrameCallback updateModalFrames[] = {&Screen::drawHermesFastSetupFrame};
        setFrameImmediateDraw(updateModalFrames);
    };
    auto exitUpdateModeModal = [&]() {
        if (!hermesUpdateModalActive) {
            return;
        }
        LOG_INFO("[UpdateModal] exit begin showingNormalScreen=%d free=%u largest=%u", showingNormalScreen ? 1 : 0,
                 ESP.getFreeHeap(), ESP.getMaxAllocHeap());
        hermesUpdateModalActive = false;
        showingNormalScreen = true;
        setFrames(FOCUS_DEFAULT);
        if (ui && framesetInfo.positions.setup < framesetInfo.frameCount) {
            ui->switchToFrame(framesetInfo.positions.setup);
        }
        LOG_INFO("[UpdateModal] exit done showingNormalScreen=%d currentFrame=%u free=%u largest=%u",
                 showingNormalScreen ? 1 : 0, ui && ui->getUiState() ? ui->getUiState()->currentFrame : 0xFF,
                 ESP.getFreeHeap(), ESP.getMaxAllocHeap());
        setFastFramerate();
    };
    auto scheduleEnterUpdateModeReboot = [&]() {
        if (saveUpdateBootFlag()) {
            enterUpdateModeModal();
            stopMiniUpdateUploadServer();
            HermesXUpdateManager::instance().resetUiSession(true);
            hermesSetupToast = u8"重開中";
            hermesSetupToastUntilMs = millis() + kSetupUpdateExitRebootMs;
            rebootAtMsec = millis() + kSetupUpdateExitRebootMs;
            resetMenu(HermesFastSetupPage::UpdateExitPending);
            hermesUpdateIntroStartedAtMs = millis();
            setFastFramerate();
            requestImmediateRedraw();
        } else {
            hermesSetupToast = u8"無法進入更新環境";
            hermesSetupToastUntilMs = millis() + 1500;
            setFastFramerate();
        }
    };
    auto leaveUpdateMode = [&]() -> bool {
        const bool dedicatedUpdateBoot = gUpdateBootRequested;
        stopMiniUpdateUploadServer();
        if (!HermesXUpdateManager::instance().isBusy()) {
            releaseWifiForUpdateMode();
        }
        disableUpdateLowLoadMode();
        if (dedicatedUpdateBoot) {
            clearUpdateBootFlag();
            gUpdateBootRequested = false;
            gUpdateBootHandled = false;
            hermesSetupToast = u8"退出更新模式，重開中";
            hermesSetupToastUntilMs = millis() + kSetupUpdateExitRebootMs;
            rebootAtMsec = millis() + kSetupUpdateExitRebootMs;
            resetMenu(HermesFastSetupPage::UpdateExitPending);
            hermesUpdateIntroStartedAtMs = millis();
            setFastFramerate();
            requestImmediateRedraw();
            return true;
        }
        exitUpdateModeModal();
        return false;
    };

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::Entry) {
        if (HermesXFastSetupUiController::instance().acceptNavigation(navDir, isRotary, now)) {
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::PassEdit) {
        const int rowCount = static_cast<int>(kSetupKeyRowCount);
        if (HermesXFastSetupUiController::instance().acceptNavigation(navDir, isRotary, now)) {
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
                        ok = lighthouseModule->setEmergencyGroupPinSlot(hermesSetupEditingSlot, hermesSetupPassDraft);
                    }
                    const char slotChar = hermesSetupEditingSlot == 0 ? 'A' : 'B';
                    String msg = ok ? String("GROUP PIN ") + slotChar + u8" 已設定: " + hermesSetupPassDraft
                                    : String("GROUP PIN ") + slotChar + u8" 設定失敗";
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::DeviceInfoShortNameEdit ||
        gHermesFastSetupNavigation.page == HermesFastSetupPage::DeviceInfoLongNameEdit) {
        const int rowCount = static_cast<int>(kSetupWifiKeyRowCount);
        const size_t maxLen = (gHermesFastSetupNavigation.page == HermesFastSetupPage::DeviceInfoShortNameEdit)
                                  ? sizeof(owner.short_name) - 1
                                  : sizeof(owner.long_name) - 1;
        if (HermesXFastSetupUiController::instance().acceptNavigation(navDir, isRotary, now)) {
            int totalKeys = 0;
            for (int r = 0; r < rowCount; ++r) {
                totalKeys += kSetupWifiKeyRowLengths[r];
            }
            int index = 0;
            for (int r = 0; r < rowCount; ++r) {
                if (r == hermesSetupKeyRow) {
                    index += hermesSetupKeyCol;
                    break;
                }
                index += kSetupWifiKeyRowLengths[r];
            }
            index = (index + navDir + totalKeys) % totalKeys;
            int remaining = index;
            for (int r = 0; r < rowCount; ++r) {
                const int rowLen = kSetupWifiKeyRowLengths[r];
                if (remaining < rowLen) {
                    hermesSetupKeyRow = r;
                    hermesSetupKeyCol = remaining;
                    break;
                }
                remaining -= rowLen;
            }
        } else if (isSelect || isPress) {
            const char *label = getSetupWifiKeyRows(hermesSetupDeviceInfoLowercase)[hermesSetupKeyRow][hermesSetupKeyCol];
            if (label) {
                if (strcmp(label, "OK") == 0) {
                    if (saveDeviceInfoName(gHermesFastSetupNavigation.page == HermesFastSetupPage::DeviceInfoShortNameEdit)) {
                        resetMenu(HermesFastSetupPage::DeviceInfoMenu);
                    }
                } else if (strcmp(label, "DEL") == 0) {
                    if (hermesSetupDeviceInfoDraft.length() > 0) {
                        hermesSetupDeviceInfoDraft.remove(hermesSetupDeviceInfoDraft.length() - 1);
                    }
                } else if (strcmp(label, "Aa") == 0) {
                    hermesSetupDeviceInfoLowercase = !hermesSetupDeviceInfoLowercase;
                } else if (strcmp(label, "SP") == 0) {
                    if (hermesSetupDeviceInfoDraft.length() < maxLen) {
                        hermesSetupDeviceInfoDraft += ' ';
                    }
                } else if (hermesSetupDeviceInfoDraft.length() < maxLen) {
                    hermesSetupDeviceInfoDraft += label;
                }
            }
        } else if (isCancel) {
            resetMenu(HermesFastSetupPage::DeviceInfoMenu);
        } else {
            return false;
        }

        setFastFramerate();
        return true;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateWifiSsidEdit ||
        gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateWifiPasswordEdit) {
        const int rowCount = static_cast<int>(kSetupWifiKeyRowCount);
        String &draft = (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateWifiSsidEdit) ? hermesSetupWifiSsidDraft
                                                                                      : hermesSetupWifiPasswordDraft;
        const size_t maxLen = (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateWifiSsidEdit) ? kSetupWifiSsidMaxLen
                                                                                            : kSetupWifiPasswordMaxLen;
        if (HermesXFastSetupUiController::instance().acceptNavigation(navDir, isRotary, now)) {
            int totalKeys = 0;
            for (int r = 0; r < rowCount; ++r) {
                totalKeys += kSetupWifiKeyRowLengths[r];
            }
            int index = 0;
            for (int r = 0; r < rowCount; ++r) {
                if (r == hermesSetupKeyRow) {
                    index += hermesSetupKeyCol;
                    break;
                }
                index += kSetupWifiKeyRowLengths[r];
            }
            index = (index + navDir + totalKeys) % totalKeys;
            int remaining = index;
            for (int r = 0; r < rowCount; ++r) {
                const int rowLen = kSetupWifiKeyRowLengths[r];
                if (remaining < rowLen) {
                    hermesSetupKeyRow = r;
                    hermesSetupKeyCol = remaining;
                    break;
                }
                remaining -= rowLen;
            }
        } else if (isSelect || isPress) {
            const char *label = getSetupWifiKeyRows(hermesSetupWifiLowercase)[hermesSetupKeyRow][hermesSetupKeyCol];
            if (label) {
                if (strcmp(label, "OK") == 0) {
                    hermesSetupWifiDirty = true;
                    hermesSetupToast = (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateWifiSsidEdit) ? u8"SSID 已更新草稿"
                                                                                                    : u8"密碼已更新草稿";
                    hermesSetupToastUntilMs = millis() + 1500;
                    resetMenu(HermesFastSetupPage::UpdateWifiConfigMenu);
                } else if (strcmp(label, "DEL") == 0) {
                    if (draft.length() > 0) {
                        draft.remove(draft.length() - 1);
                    }
                } else if (strcmp(label, "Aa") == 0) {
                    hermesSetupWifiLowercase = !hermesSetupWifiLowercase;
                } else if (strcmp(label, "SP") == 0) {
                    if (draft.length() < maxLen) {
                        draft += ' ';
                    }
                } else if (draft.length() < maxLen) {
                    draft += label;
                }
            }
        } else if (isCancel) {
            resetMenu(HermesFastSetupPage::UpdateWifiConfigMenu);
        } else {
            return false;
        }

        setFastFramerate();
        return true;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::FrequencyEdit) {
        const int rowCount = static_cast<int>(kSetupNumericKeyRowCount);
        if (HermesXFastSetupUiController::instance().acceptNavigation(navDir, isRotary, now)) {
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::PassShow) {
        if (isCancel || isSelect || isPress) {
            resetMenu(HermesFastSetupPage::EmacMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    auto handleMenuNav = [&](int count) -> bool {
        return HermesXFastSetupUiController::instance().navigate(navDir, count, isRotary, now) !=
               HermesXFastSetupNavigationAction::Ignored;
    };

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::Root) {
        if (isCancel) {
            exitFastSetupToActionPage();
            return true;
        }
        if (handleMenuNav(kSetupRootCount)) {
            LOG_INFO("[HermesFastSetup] select=%d item=%s", gHermesFastSetupNavigation.selected, kSetupRootItems[gHermesFastSetupNavigation.selected]);
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            const auto action =
                HermesXFastSetupUiController::instance().activateRoot(gHermesFastSetupNavigation.selected);
            if (action == HermesXFastSetupAction::ExitRequested) {
                exitFastSetupToActionPage();
            } else if (action == HermesXFastSetupAction::SaveAndRebootRequested) {
                nodeDB->saveToDisk(SEGMENT_CONFIG | SEGMENT_MODULECONFIG | SEGMENT_CHANNELS | SEGMENT_DEVICESTATE);
                hermesSetupToast = u8"即將重新開機";
                hermesSetupToastUntilMs = millis() + 1500;
                rebootAtMsec = millis() + 2000;
            }
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::NodeMenu) {
        if (handleMenuNav(kSetupNodeMenuCount)) {
            const char *itemName =
                HermesXFastSetupUiController::instance().nodeMenuItemName(gHermesFastSetupNavigation.selected);
            LOG_INFO("[HermesFastSetup] select=%d item=%s", gHermesFastSetupNavigation.selected, itemName);
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            const auto action =
                HermesXFastSetupUiController::instance().activateNodeMenu(gHermesFastSetupNavigation.selected);
            if (action == HermesXFastSetupAction::OpenChannelMenuRequested) {
                enterChannelMenu();
            } else if (action == HermesXFastSetupAction::ToggleBluetoothRequested) {
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
            } else if (action == HermesXFastSetupAction::EnterUpdateModeRequested) {
                scheduleEnterUpdateModeReboot();
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::DeviceInfoMenu) {
        if (handleMenuNav(kSetupDeviceInfoMenuCount)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            const auto action =
                HermesXFastSetupUiController::instance().activateDeviceInfoMenu(gHermesFastSetupNavigation.selected);
            if (action == HermesXFastSetupAction::EditDeviceShortNameRequested) {
                hermesSetupDeviceInfoDraft = owner.short_name;
                hermesSetupDeviceInfoLowercase = false;
                hermesSetupKeyRow = 0;
                hermesSetupKeyCol = 0;
            } else if (action == HermesXFastSetupAction::EditDeviceLongNameRequested) {
                hermesSetupDeviceInfoDraft = owner.long_name;
                hermesSetupDeviceInfoLowercase = false;
                hermesSetupKeyRow = 0;
                hermesSetupKeyCol = 0;
            } else if (action == HermesXFastSetupAction::ShowNodeIdRequested) {
                openSetupDetailPage("NodeID", getSetupNodeNumLabel(), HermesFastSetupPage::DeviceInfoMenu);
            } else if (action == HermesXFastSetupAction::OpenDeviceBroadcastRequested) {
                enterMenu(HermesFastSetupPage::DeviceInfoBroadcastSelect, kSetupNodeInfoBroadcastCount + 1,
                          getSetupNodeInfoBroadcastSelection(getSetupCurrentNodeInfoBroadcast()));
            } else if (action == HermesXFastSetupAction::RequestNodeInfoRequested) {
                if (nodeInfoModule) {
                    const bool sent = nodeInfoModule->sendOurNodeInfo(NODENUM_BROADCAST, true, 0, true);
                    hermesSetupToast = sent ? u8"已請求節點回報" : u8"太頻繁，稍後再試";
                } else {
                    hermesSetupToast = u8"NodeInfo未啟動";
                }
                hermesSetupToastUntilMs = millis() + 1500;
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::DeviceInfoBroadcastSelect) {
        if (handleMenuNav(kSetupNodeInfoBroadcastCount + 1)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (gHermesFastSetupNavigation.selected == 0) {
                resetMenu(HermesFastSetupPage::DeviceInfoMenu);
            } else {
                const uint8_t index = gHermesFastSetupNavigation.selected - 1;
                if (index < kSetupNodeInfoBroadcastCount) {
                    config.device.node_info_broadcast_secs = kSetupNodeInfoBroadcastOptions[index];
                    saveSetupSegments(SEGMENT_CONFIG);
                    hermesSetupToast = String(u8"廣播時間: ") + kSetupNodeInfoBroadcastLabels[index];
                    hermesSetupToastUntilMs = millis() + 1500;
                }
                resetMenu(HermesFastSetupPage::DeviceInfoMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::DeviceInfoMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateMenu) {
        auto &updateManager = HermesXUpdateManager::instance();
        const bool dedicatedUpdateBoot = gUpdateBootRequested;
        const uint8_t itemCount = dedicatedUpdateBoot ? kSetupUpdateMenuCount : kSetupUpdateEntryMenuCount;
        if (handleMenuNav(itemCount)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            const auto action = HermesXFastSetupUiController::instance().activateUpdateMenu(
                gHermesFastSetupNavigation.selected, dedicatedUpdateBoot);
            if (action == HermesXFastSetupAction::LeaveUpdateModeRequested) {
                if (!updateManager.isBusy()) {
                    if (!leaveUpdateMode()) {
                        resetMenu(HermesFastSetupPage::NodeMenu);
                    }
                } else {
                    hermesSetupToast = u8"更新寫入中，請先取消";
                    hermesSetupToastUntilMs = millis() + 1500;
                }
            } else if (action == HermesXFastSetupAction::EnterUpdateModeRequested) {
                scheduleEnterUpdateModeReboot();
            } else if (action == HermesXFastSetupAction::OpenUpdateWifiConfigRequested) {
                resetMenu(HermesFastSetupPage::UpdateWifiConfigMenu);
            } else if (action == HermesXFastSetupAction::ShowCurrentVersionRequested) {
                hermesSetupToast = String(u8"目前版本: ") + updateManager.getCurrentBuildVersion();
                hermesSetupToastUntilMs = millis() + 1800;
            } else if (action == HermesXFastSetupAction::OpenUpdateCheckRequested) {
                hermesPendingUpdateCheckAction = HermesPendingUpdateCheckAction::None;
                updateManager.resetUiSession(false);
                resetMenu(HermesFastSetupPage::UpdateCheckMenu);
            } else if (action == HermesXFastSetupAction::OpenUpdateRuntimeRequested) {
                resetMenu(HermesFastSetupPage::UpdateRuntimeMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            if (!updateManager.isBusy()) {
                if (!leaveUpdateMode()) {
                    resetMenu(HermesFastSetupPage::NodeMenu);
                }
            } else {
                hermesSetupToast = u8"更新寫入中，請先取消";
                hermesSetupToastUntilMs = millis() + 1500;
            }
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateIntro) {
        hermesUpdateIntroStartedAtMs = 0;
        resetMenu(HermesFastSetupPage::UpdateMenu);
        setFastFramerate();
        return true;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateExitPending) {
        if (hermesSetupToast != u8"重開中") {
            hermesSetupToast = u8"退出更新模式，重開中";
        }
        hermesSetupToastUntilMs = std::max<uint32_t>(hermesSetupToastUntilMs, millis() + 250);
        setFastFramerate();
        return true;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateCheckMenu) {
        if (handleMenuNav(kSetupUpdateCheckMenuCount)) {
            setFastFramerate();
            return true;
        }
        auto &updateManager = HermesXUpdateManager::instance();
        if (isSelect || isPress) {
            LOG_INFO("[UpdateCheck] press selected=%d canApply=%d hasCandidate=%d page=%d", gHermesFastSetupNavigation.selected,
                     updateManager.canApply() ? 1 : 0, updateManager.hasCandidate() ? 1 : 0, static_cast<int>(gHermesFastSetupNavigation.page));
            const auto action =
                HermesXFastSetupUiController::instance().activateUpdateCheckMenu(gHermesFastSetupNavigation.selected);
            if (action == HermesXFastSetupAction::ReturnToUpdateMenuRequested) {
                hermesPendingUpdateCheckAction = HermesPendingUpdateCheckAction::None;
                resetMenu(HermesFastSetupPage::UpdateMenu);
            } else if (action == HermesXFastSetupAction::ShowCurrentVersionRequested) {
                openSetupDetailPage(u8"目前版本", updateManager.getCurrentBuildVersion(), HermesFastSetupPage::UpdateCheckMenu);
            } else if (action == HermesXFastSetupAction::ShowUpdateSourceRequested) {
                const char *remoteUrl = updateManager.getRemoteUpdateUrl();
                openSetupDetailPage(u8"更新來源", (remoteUrl && remoteUrl[0]) ? String(remoteUrl) : String(u8"URL 更新來源未設定"),
                                    HermesFastSetupPage::UpdateCheckMenu);
            } else if (action == HermesXFastSetupAction::StartUpdateCheckFlowRequested) {
                resetMenu(HermesFastSetupPage::UpdateCheckFlowPage);
                gHermesFastSetupNavigation.selected = 1;
                if (!updateManager.canApply()) {
                    if (!config.network.wifi_enabled || !config.network.wifi_ssid[0]) {
                        hermesPendingUpdateCheckAction = HermesPendingUpdateCheckAction::None;
                        hermesSetupToast = u8"請先設定 WiFi";
                        hermesSetupToastUntilMs = millis() + 1800;
                        setFastFramerate();
                        return true;
                    }
#if HAS_WIFI && !defined(ARCH_PORTDUINO)
                    if (!isWifiRequestedForUpdateMode()) {
                        requestWifiForUpdateMode();
                    }
                    if (WiFi.status() != WL_CONNECTED) {
                        hermesPendingUpdateCheckAction = updateManager.hasCandidate()
                                                             ? HermesPendingUpdateCheckAction::Download
                                                             : HermesPendingUpdateCheckAction::Check;
                        LOG_INFO("[UpdateCheckFlow] queued action=%s waiting for WiFi", updateManager.hasCandidate() ? "download"
                                                                                                                      : "check");
                        hermesSetupToast = u8"WiFi 連線中，請稍後再試";
                        hermesSetupToastUntilMs = millis() + 1800;
                        setFastFramerate();
                        return true;
                    }
#endif
                    hermesPendingUpdateCheckAction =
                        updateManager.hasCandidate() ? HermesPendingUpdateCheckAction::Download : HermesPendingUpdateCheckAction::Check;
                    LOG_INFO("[UpdateCheckFlow] queued action=%s immediate wifi=1",
                             updateManager.hasCandidate() ? "download" : "check");
                    hermesSetupToast = updateManager.hasCandidate() ? u8"開始下載中" : u8"開始檢查中";
                    hermesSetupToastUntilMs = millis() + 1800;
                }
            } else if (action == HermesXFastSetupAction::ShowUpdateStatusRequested) {
                openSetupDetailPage(u8"目前狀態", updateManager.getSourceStatus(), HermesFastSetupPage::UpdateCheckMenu);
            } else if (action == HermesXFastSetupAction::ShowUpdateCandidateRequested) {
                openSetupDetailPage(u8"待更新版本",
                                    updateManager.getCandidateVersion().isEmpty() ? String(u8"無")
                                                                                  : updateManager.getCandidateVersion(),
                                    HermesFastSetupPage::UpdateCheckMenu);
            } else if (action == HermesXFastSetupAction::ShowUpdateProgressRequested) {
                openSetupDetailPage(u8"下載進度", String(updateManager.getProgressPercent()) + "%", HermesFastSetupPage::UpdateCheckMenu);
            } else if (action == HermesXFastSetupAction::ShowUpdateErrorRequested) {
                openSetupDetailPage(u8"最後錯誤",
                                    updateManager.getLastError().isEmpty() ? String(u8"無") : updateManager.getLastError(),
                                    HermesFastSetupPage::UpdateCheckMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            hermesPendingUpdateCheckAction = HermesPendingUpdateCheckAction::None;
            resetMenu(HermesFastSetupPage::UpdateMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateCheckFlowPage) {
        if (handleMenuNav(kSetupUpdateCheckFlowCount)) {
            setFastFramerate();
            return true;
        }
        auto &updateManager = HermesXUpdateManager::instance();
        if (isSelect || isPress) {
            LOG_INFO("[UpdateCheckFlow] press selected=%d canApply=%d hasCandidate=%d", gHermesFastSetupNavigation.selected,
                     updateManager.canApply() ? 1 : 0, updateManager.hasCandidate() ? 1 : 0);
            const auto action =
                HermesXFastSetupUiController::instance().activateUpdateCheckFlow(gHermesFastSetupNavigation.selected);
            if (action == HermesXFastSetupAction::ReturnToUpdateCheckRequested) {
                hermesPendingUpdateCheckAction = HermesPendingUpdateCheckAction::None;
                resetMenu(HermesFastSetupPage::UpdateCheckMenu);
            } else if (action == HermesXFastSetupAction::ContinueUpdateCheckFlowRequested) {
                if (updateManager.canApply()) {
                    const bool ok = updateManager.applyUpdate();
                    hermesSetupToast = ok ? updateManager.getSourceStatus()
                                          : (updateManager.getLastError().isEmpty() ? updateManager.getSourceStatus()
                                                                                    : updateManager.getLastError());
                    hermesSetupToastUntilMs = millis() + 2200;
                } else {
                    if (!config.network.wifi_enabled || !config.network.wifi_ssid[0]) {
                        hermesPendingUpdateCheckAction = HermesPendingUpdateCheckAction::None;
                        hermesSetupToast = u8"請先設定 WiFi";
                        hermesSetupToastUntilMs = millis() + 1800;
                        setFastFramerate();
                        return true;
                    }
#if HAS_WIFI && !defined(ARCH_PORTDUINO)
                    if (!isWifiRequestedForUpdateMode()) {
                        requestWifiForUpdateMode();
                    }
                    if (WiFi.status() != WL_CONNECTED) {
                        hermesPendingUpdateCheckAction = updateManager.hasCandidate()
                                                             ? HermesPendingUpdateCheckAction::Download
                                                             : HermesPendingUpdateCheckAction::Check;
                        LOG_INFO("[UpdateCheckFlow] queued action=%s waiting for WiFi", updateManager.hasCandidate() ? "download"
                                                                                                                      : "check");
                        hermesSetupToast = u8"WiFi 連線中，請稍後再試";
                        hermesSetupToastUntilMs = millis() + 1800;
                        setFastFramerate();
                        return true;
                    }
#endif
                    hermesPendingUpdateCheckAction =
                        updateManager.hasCandidate() ? HermesPendingUpdateCheckAction::Download : HermesPendingUpdateCheckAction::Check;
                    LOG_INFO("[UpdateCheckFlow] queued action=%s immediate wifi=1",
                             updateManager.hasCandidate() ? "download" : "check");
                    hermesSetupToast = updateManager.hasCandidate() ? u8"開始下載中" : u8"開始檢查中";
                    hermesSetupToastUntilMs = millis() + 1800;
                }
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            hermesPendingUpdateCheckAction = HermesPendingUpdateCheckAction::None;
            resetMenu(HermesFastSetupPage::UpdateCheckMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateDetailPopup) {
        LOG_INFO("[UpdateDetailPopup] input visible=%d title=%s", isSetupDetailPopupVisible() ? 1 : 0,
                 gSetupDetailPopupState.title.c_str());
        if (handleSetupDetailPopupInput(event)) {
            if (!isSetupDetailPopupVisible()) {
                resetMenu(gHermesFastSetupNavigation.returnPage);
            }
            setFastFramerate();
            return true;
        }
        return true;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateRuntimeMenu) {
        if (handleMenuNav(kSetupUpdateRuntimeMenuCount)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            const auto action =
                HermesXFastSetupUiController::instance().activateUpdateRuntimeMenu(gHermesFastSetupNavigation.selected);
            if (action == HermesXFastSetupAction::ReturnToUpdateMenuRequested) {
                hermesManualUpdateFlow = HermesManualUpdateFlow::None;
                resetMenu(HermesFastSetupPage::UpdateMenu);
            } else if (action == HermesXFastSetupAction::OpenWifiUpdateFlowRequested) {
                hermesManualUpdateFlow = HermesManualUpdateFlow::Wifi;
                resetMenu(HermesFastSetupPage::UpdateWifiMenu);
            } else if (action == HermesXFastSetupAction::OpenUsbUpdateFlowRequested) {
                hermesManualUpdateFlow = HermesManualUpdateFlow::Usb;
                resetMenu(HermesFastSetupPage::UpdateUploadMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            hermesManualUpdateFlow = HermesManualUpdateFlow::None;
            resetMenu(HermesFastSetupPage::UpdateMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateWifiMenu) {
        if (handleMenuNav(kSetupUpdateWifiMenuCount)) {
            setFastFramerate();
            return true;
        }
        auto &updateManager = HermesXUpdateManager::instance();
        if (isSelect || isPress) {
            const auto action =
                HermesXFastSetupUiController::instance().activateUpdateWifiMenu(gHermesFastSetupNavigation.selected);
            if (action == HermesXFastSetupAction::ReturnToUpdateRuntimeRequested) {
                hermesManualUpdateFlow = HermesManualUpdateFlow::None;
                resetMenu(HermesFastSetupPage::UpdateRuntimeMenu);
            } else if (action == HermesXFastSetupAction::ShowCurrentVersionRequested) {
                openSetupDetailPage(u8"目前版本", updateManager.getCurrentBuildVersion(), HermesFastSetupPage::UpdateWifiMenu);
            } else if (action == HermesXFastSetupAction::ShowWifiStatusRequested) {
                openSetupDetailPage(u8"連線狀態", getSetupWifiIpLabel(), HermesFastSetupPage::UpdateWifiMenu);
            } else if (action == HermesXFastSetupAction::StartWifiUpdateRequested) {
                hermesManualUpdateFlow = HermesManualUpdateFlow::Wifi;
                stopMiniUpdateUploadServer();
                updateManager.resetUiSession(true);
                refreshWifiConnection();
                hermesSetupToast = "";
                hermesSetupToastUntilMs = 0;
                resetMenu(HermesFastSetupPage::UpdateApplyMenu);
                gHermesFastSetupNavigation.selected = 0;
                gHermesFastSetupNavigation.offset = 0;
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            hermesManualUpdateFlow = HermesManualUpdateFlow::None;
            resetMenu(HermesFastSetupPage::UpdateRuntimeMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateWifiConfigMenu) {
        if (handleMenuNav(kSetupUpdateWifiConfigMenuCount)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            const auto action =
                HermesXFastSetupUiController::instance().activateUpdateWifiConfigMenu(gHermesFastSetupNavigation.selected);
            if (action == HermesXFastSetupAction::ReturnToUpdateMenuRequested) {
                resetMenu(HermesFastSetupPage::UpdateMenu);
            } else if (action == HermesXFastSetupAction::ToggleWifiDraftRequested) {
                hermesSetupWifiEnabledDraft = !hermesSetupWifiEnabledDraft;
                hermesSetupWifiDirty = true;
            } else if (action == HermesXFastSetupAction::EditWifiSsidRequested) {
                hermesSetupWifiLowercase = false;
                hermesSetupKeyRow = 0;
                hermesSetupKeyCol = 0;
                resetMenu(HermesFastSetupPage::UpdateWifiSsidEdit);
            } else if (action == HermesXFastSetupAction::EditWifiPasswordRequested) {
                hermesSetupWifiLowercase = false;
                hermesSetupKeyRow = 0;
                hermesSetupKeyCol = 0;
                resetMenu(HermesFastSetupPage::UpdateWifiPasswordEdit);
            } else if (action == HermesXFastSetupAction::SaveWifiDraftRequested) {
                storeWifiConfigDraft(true);
            } else if (action == HermesXFastSetupAction::ShowWifiIpRequested) {
                hermesSetupToast = String("IP: ") + getSetupWifiIpLabel();
                hermesSetupToastUntilMs = millis() + 1800;
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::UpdateMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateUploadMenu) {
        if (handleMenuNav(kSetupUpdateUploadMenuCount)) {
            setFastFramerate();
            return true;
        }
        auto &updateManager = HermesXUpdateManager::instance();
        if (isSelect || isPress) {
            const auto action =
                HermesXFastSetupUiController::instance().activateUpdateUploadMenu(gHermesFastSetupNavigation.selected);
            if (action == HermesXFastSetupAction::ReturnToUpdateRuntimeRequested) {
                hermesManualUpdateFlow = HermesManualUpdateFlow::None;
                resetMenu(HermesFastSetupPage::UpdateRuntimeMenu);
            } else if (action == HermesXFastSetupAction::ShowCurrentVersionRequested) {
                openSetupDetailPage(u8"目前版本", updateManager.getCurrentBuildVersion(), HermesFastSetupPage::UpdateUploadMenu);
            } else if (action == HermesXFastSetupAction::ShowUsbStatusRequested) {
                openSetupDetailPage(u8"USB連線狀態", getSetupUsbStatusLabel(), HermesFastSetupPage::UpdateUploadMenu);
            } else if (action == HermesXFastSetupAction::StartUsbUpdateRequested) {
                hermesManualUpdateFlow = HermesManualUpdateFlow::Usb;
                stopMiniUpdateUploadServer();
                updateManager.resetUiSession(true);
                hermesSetupToast = "";
                hermesSetupToastUntilMs = 0;
                resetMenu(HermesFastSetupPage::UpdateApplyMenu);
                gHermesFastSetupNavigation.selected = 0;
                gHermesFastSetupNavigation.offset = 0;
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            hermesManualUpdateFlow = HermesManualUpdateFlow::None;
            resetMenu(HermesFastSetupPage::UpdateRuntimeMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UpdateApplyMenu) {
        auto &updateManager = HermesXUpdateManager::instance();
        const uint8_t actionCount = updateManager.canApply() ? kSetupUpdateApplyMenuCount : 1;
        if (handleMenuNav(actionCount)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            const auto action = HermesXFastSetupUiController::instance().activateUpdateApplyMenu(
                gHermesFastSetupNavigation.selected, updateManager.canApply());
            if (action == HermesXFastSetupAction::ReturnFromUpdateApplyRequested) {
                resetMenu(hermesManualUpdateFlow == HermesManualUpdateFlow::Usb ? HermesFastSetupPage::UpdateUploadMenu
                                                                               : HermesFastSetupPage::UpdateWifiMenu);
            } else if (action == HermesXFastSetupAction::ApplyUpdateRequested) {
                LOG_INFO("[UpdateApplyMenu] apply pressed flow=%d canApply=%d target=%s",
                         static_cast<int>(hermesManualUpdateFlow), updateManager.canApply() ? 1 : 0,
                         updateManager.getTargetPartitionLabel().c_str());
                if (updateManager.canApply()) {
                    hermesSetupToast = u8"套用更新中";
                    hermesSetupToastUntilMs = millis() + 1000;
                    updateManager.applyUpdate();
                } else {
                    hermesSetupToast = u8"請先在電腦完成傳送";
                    hermesSetupToastUntilMs = millis() + 1800;
                }
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(hermesManualUpdateFlow == HermesManualUpdateFlow::Usb ? HermesFastSetupPage::UpdateUploadMenu
                                                                           : HermesFastSetupPage::UpdateWifiMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::NodeDatabaseMenu) {
        if (handleMenuNav(kSetupNodeDatabaseMenuCount)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            HermesXFastSetupUiController::instance().activateNodeDatabaseMenu(gHermesFastSetupNavigation.selected);
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::NodeDatabaseResetSelect) {
        if (handleMenuNav(kSetupNodeDatabaseResetCount)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            const auto result =
                HermesXFastSetupUiController::instance().activateNodeDatabaseResetSelect(gHermesFastSetupNavigation.selected);
            int removed = -1;
            if (result.action == HermesXFastSetupAction::CleanupNodesRequested && nodeDB) {
                hermesSetupNodeCleanupAgeSeconds = result.value;
                removed = nodeDB->cleanupNodesOlderThan(result.value, true);
            } else if (result.action == HermesXFastSetupAction::ResetAllNodesRequested && nodeDB) {
                const int beforeCount = static_cast<int>(nodeDB->getNumMeshNodes());
                nodeDB->resetNodes();
                removed = beforeCount > 0 ? beforeCount - 1 : 0;
            }
            if (removed >= 0) {
                char toastBuf[48];
                snprintf(toastBuf, sizeof(toastBuf), u8"已清除 %d 筆節點", removed);
                hermesSetupToast = toastBuf;
                hermesSetupToastUntilMs = millis() + 1800;
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::MqttMenu) {
        if (handleMenuNav(kSetupMqttMenuCount)) {
            const char *itemName =
                HermesXFastSetupUiController::instance().mqttMenuItemName(gHermesFastSetupNavigation.selected);
            LOG_INFO("[HermesFastSetup] select=%d item=%s", gHermesFastSetupNavigation.selected, itemName);
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            const auto action =
                HermesXFastSetupUiController::instance().activateMqttMenu(gHermesFastSetupNavigation.selected);
            if (action == HermesXFastSetupAction::ToggleMqttRequested) {
                moduleConfig.mqtt.enabled = !moduleConfig.mqtt.enabled;
                saveSetupSegments(SEGMENT_MODULECONFIG);
                if (mqtt) {
                    mqtt->start();
                }
                hermesSetupToast = moduleConfig.mqtt.enabled ? u8"MQTT 已啟用" : u8"MQTT 已停用";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (action == HermesXFastSetupAction::ToggleMqttProxyRequested) {
                moduleConfig.mqtt.proxy_to_client_enabled = !moduleConfig.mqtt.proxy_to_client_enabled;
                saveSetupSegments(SEGMENT_MODULECONFIG);
                if (mqtt) {
                    mqtt->start();
                }
                hermesSetupToast = moduleConfig.mqtt.proxy_to_client_enabled ? u8"客戶端代理已啟用" : u8"客戶端代理已停用";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (action == HermesXFastSetupAction::OpenMqttMapReportRequested) {
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::MqttMapReportMenu) {
        if (handleMenuNav(kSetupMqttMapReportMenuCount)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            const auto action =
                HermesXFastSetupUiController::instance().activateMqttMapReportMenu(gHermesFastSetupNavigation.selected);
            if (action == HermesXFastSetupAction::ToggleMqttMapReportRequested) {
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
            } else if (action == HermesXFastSetupAction::OpenMqttMapPrecisionRequested) {
                ensureMqttMapSettings();
                const uint8_t selected = getSetupMqttMapPrecisionSelection(getSetupCurrentMqttMapPrecision());
                enterMenu(HermesFastSetupPage::MqttMapPrecisionSelect, kSetupMqttMapPrecisionCount + 1, selected);
            } else if (action == HermesXFastSetupAction::OpenMqttMapPublishRequested) {
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::MqttMapPrecisionSelect) {
        const int count = kSetupMqttMapPrecisionCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            const auto result = HermesXFastSetupUiController::instance().activateMqttMapPrecisionSelect(
                gHermesFastSetupNavigation.selected, kSetupMqttMapPrecisionCount);
            if (result.action == HermesXFastSetupAction::ApplyMqttMapPrecisionRequested) {
                ensureMqttMapSettings();
                moduleConfig.mqtt.map_report_settings.position_precision = kSetupMqttMapPrecisionOptions[result.optionIndex].value;
                saveSetupSegments(SEGMENT_MODULECONFIG);
                hermesSetupToast = String(u8"地圖精確度: ") + kSetupMqttMapPrecisionOptions[result.optionIndex].label;
                hermesSetupToastUntilMs = millis() + 1500;
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::MqttMapPublishSelect) {
        const int count = kSetupMqttMapPublishCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            const auto result = HermesXFastSetupUiController::instance().activateMqttMapPublishSelect(
                gHermesFastSetupNavigation.selected, kSetupMqttMapPublishCount);
            if (result.action == HermesXFastSetupAction::ApplyMqttMapPublishRequested) {
                ensureMqttMapSettings();
                moduleConfig.mqtt.map_report_settings.publish_interval_secs = kSetupMqttMapPublishOptions[result.optionIndex];
                saveSetupSegments(SEGMENT_MODULECONFIG);
                hermesSetupToast = String(u8"地圖間隔: ") + kSetupMqttMapPublishLabels[result.optionIndex];
                hermesSetupToastUntilMs = millis() + 1500;
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::ChannelMenu) {
        ChannelIndex channelList[MAX_NUM_CHANNELS];
        const uint8_t channelCount = buildSetupChannelList(channelList, MAX_NUM_CHANNELS);
        const int count = channelCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            const auto action =
                HermesXFastSetupUiController::instance().activateChannelMenu(gHermesFastSetupNavigation.selected);
            if (action == HermesXFastSetupAction::OpenChannelDetailRequested) {
                const uint8_t index = gHermesFastSetupNavigation.selected - 1;
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::ChannelDetailMenu) {
        if (handleMenuNav(kSetupChannelDetailMenuCount)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            auto channel = getSetupChannelCopy(hermesSetupChannelIndex);
            const auto action =
                HermesXFastSetupUiController::instance().activateChannelDetailMenu(gHermesFastSetupNavigation.selected);
            if (action == HermesXFastSetupAction::OpenChannelMenuRequested) {
                enterChannelMenu();
            } else if (action == HermesXFastSetupAction::ToggleChannelUplinkRequested) {
                channel.settings.uplink_enabled = !channel.settings.uplink_enabled;
                saveSetupChannel(channel);
                hermesSetupToast = channel.settings.uplink_enabled ? u8"上行已啟用" : u8"上行已停用";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (action == HermesXFastSetupAction::ToggleChannelDownlinkRequested) {
                channel.settings.downlink_enabled = !channel.settings.downlink_enabled;
                saveSetupChannel(channel);
                hermesSetupToast = channel.settings.downlink_enabled ? u8"下行已啟用" : u8"下行已停用";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (action == HermesXFastSetupAction::ToggleChannelPositionRequested) {
                channel.settings.has_module_settings = true;
                const bool next = !isSetupChannelPositionSharingEnabled(hermesSetupChannelIndex);
                channel.settings.module_settings.position_precision = next ? getSetupDefaultChannelPrecision(hermesSetupChannelIndex) : 0U;
                saveSetupChannel(channel);
                hermesSetupToast = next ? u8"位置分享已啟用" : u8"位置分享已停用";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (action == HermesXFastSetupAction::OpenChannelPrecisionRequested) {
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::ChannelPrecisionSelect) {
        const int count = kSetupChannelPrecisionCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            const auto result = HermesXFastSetupUiController::instance().activateChannelPrecisionSelect(
                gHermesFastSetupNavigation.selected, kSetupChannelPrecisionCount);
            if (result.action == HermesXFastSetupAction::ApplyChannelPrecisionRequested) {
                auto channel = getSetupChannelCopy(hermesSetupChannelIndex);
                channel.settings.has_module_settings = true;
                channel.settings.module_settings.position_precision = kSetupChannelPrecisionOptions[result.optionIndex].value;
                saveSetupChannel(channel);
                hermesSetupToast = String(u8"頻道精確度: ") + kSetupChannelPrecisionOptions[result.optionIndex].label;
                hermesSetupToastUntilMs = millis() + 1500;
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::PowerMenu) {
        if (handleMenuNav(kSetupPowerMenuCount)) {
            const char *itemName =
                HermesXFastSetupUiController::instance().powerMenuItemName(gHermesFastSetupNavigation.selected);
            LOG_INFO("[HermesFastSetup] select=%d item=%s", gHermesFastSetupNavigation.selected, itemName);
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            const auto action =
                HermesXFastSetupUiController::instance().activatePowerMenu(gHermesFastSetupNavigation.selected);
            if (action == HermesXFastSetupAction::TogglePowerGuardRequested) {
                const bool next = !HermesXBatteryProtection::isEnabled();
                HermesXBatteryProtection::setEnabled(next);
                hermesSetupToast = next ? u8"過放保護已開啟" : u8"過放保護已關閉";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (action == HermesXFastSetupAction::OpenPowerGuardThresholdRequested) {
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::PowerGuardVoltageSelect) {
        const int count = kSetupPowerGuardThresholdCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (gHermesFastSetupNavigation.selected == 0) {
                resetMenu(HermesFastSetupPage::PowerMenu);
            } else {
                const uint8_t index = gHermesFastSetupNavigation.selected - 1;
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::EmacMenu) {
        if (handleMenuNav(kSetupEmacCount)) {
            LOG_INFO("[HermesFastSetup] select=%d item=%s", gHermesFastSetupNavigation.selected, kSetupEmacItems[gHermesFastSetupNavigation.selected]);
            setFastFramerate();
            return true;
        }
        auto returnFromGroupSettings = [&]() {
            if (hermesSetupReturnToGroupMenu) {
                gGroupNodeState.menuCursor = 1;
                gGroupNodeState.nodeListVisible = false;
                showGroupNodeListPage();
            } else {
                resetMenu(HermesFastSetupPage::Root);
            }
        };
        if (isSelect || isPress) {
            const auto action =
                HermesXFastSetupUiController::instance().activateEmacMenu(gHermesFastSetupNavigation.selected);
            if (action == HermesXFastSetupAction::ReturnFromGroupRequested) {
                returnFromGroupSettings();
            } else if (action == HermesXFastSetupAction::OpenEmInfoRequested) {
                enterMenu(HermesFastSetupPage::EmacEmInfoMenu, kSetupEmInfoCount, 1);
            } else if (action == HermesXFastSetupAction::EditGroupPin0Requested ||
                       action == HermesXFastSetupAction::EditGroupPin1Requested) {
                hermesSetupEditingSlot = action == HermesXFastSetupAction::EditGroupPin0Requested ? 0 : 1;
                hermesSetupPassDraft = lighthouseModule ? lighthouseModule->getEmergencyGroupPin(hermesSetupEditingSlot) : "";
                if (hermesSetupPassDraft.length() > kSetupPassMaxLen) {
                    hermesSetupPassDraft = hermesSetupPassDraft.substring(0, kSetupPassMaxLen);
                }
                hermesSetupKeyRow = 0;
                hermesSetupKeyCol = 0;
            } else if (action == HermesXFastSetupAction::ResetLighthouseRequested) {
                if (hermesXEmUiModule) {
                    hermesXEmUiModule->sendResetLighthouseNow();
                    hermesSetupToast = u8"解除EMAC已送出";
                } else {
                    hermesSetupToast = u8"解除EMAC失敗";
                }
                hermesSetupToastUntilMs = millis() + 1500;
                setFastFramerate();
                return true;
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            returnFromGroupSettings();
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::EmacEmInfoMenu) {
        if (handleMenuNav(kSetupEmInfoCount)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            const auto action =
                HermesXFastSetupUiController::instance().activateEmInfoMenu(gHermesFastSetupNavigation.selected);
            if (action == HermesXFastSetupAction::ToggleEmInfoRequested) {
                if (hermesXEmUiModule) {
                    hermesXEmUiModule->setEmInfoBroadcastEnabled(!hermesXEmUiModule->isEmInfoBroadcastEnabled());
                    hermesSetupToast = hermesXEmUiModule->isEmInfoBroadcastEnabled() ? u8"EMINFO廣播已開啟" : u8"EMINFO廣播已關閉";
                    hermesSetupToastUntilMs = millis() + 1500;
                }
            } else if (action == HermesXFastSetupAction::OpenEmInfoIntervalRequested) {
                const uint8_t selected =
                    getSetupEmInfoIntervalSelection(hermesXEmUiModule ? hermesXEmUiModule->getEmInfoIntervalSec() : 0);
                enterMenu(HermesFastSetupPage::EmacEmInfoIntervalSelect, kSetupEmInfoIntervalCount + 1, selected);
            } else if (action == HermesXFastSetupAction::OpenHeartbeatIntervalRequested) {
                const uint8_t selected = getSetupHeartbeatIntervalSelection(
                    hermesXEmUiModule ? hermesXEmUiModule->getEmHeartbeatIntervalSec() : 0);
                enterMenu(HermesFastSetupPage::EmacHeartbeatIntervalSelect, kSetupHeartbeatIntervalCount + 1, selected);
            } else if (action == HermesXFastSetupAction::OpenOfflineThresholdRequested) {
                const uint8_t selected = getSetupOfflineThresholdSelection(
                    hermesXEmUiModule ? hermesXEmUiModule->getEmOfflineThresholdCount() : 3);
                enterMenu(HermesFastSetupPage::EmacOfflineThresholdSelect, kSetupOfflineThresholdCount + 1, selected);
            } else if (action == HermesXFastSetupAction::OpenBatteryIncludeRequested) {
                enterMenu(HermesFastSetupPage::EmacBatteryIncludeSelect, 3,
                          (hermesXEmUiModule && hermesXEmUiModule->isEmBatteryIncluded()) ? 2 : 1);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::EmacMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::EmacEmInfoIntervalSelect) {
        const int count = kSetupEmInfoIntervalCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (gHermesFastSetupNavigation.selected == 0) {
                resetMenu(HermesFastSetupPage::EmacEmInfoMenu);
            } else {
                const uint8_t index = static_cast<uint8_t>(gHermesFastSetupNavigation.selected - 1);
                if (index < kSetupEmInfoIntervalCount && hermesXEmUiModule) {
                    hermesXEmUiModule->setEmInfoIntervalSec(kSetupEmInfoIntervalOptions[index]);
                    hermesSetupToast = String(u8"EMINFO週期: ") + kSetupEmInfoIntervalLabels[index];
                    hermesSetupToastUntilMs = millis() + 1500;
                }
                resetMenu(HermesFastSetupPage::EmacEmInfoMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::EmacEmInfoMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::EmacHeartbeatIntervalSelect) {
        const int count = kSetupHeartbeatIntervalCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (gHermesFastSetupNavigation.selected == 0) {
                resetMenu(HermesFastSetupPage::EmacEmInfoMenu);
            } else {
                const uint8_t index = static_cast<uint8_t>(gHermesFastSetupNavigation.selected - 1);
                if (index < kSetupHeartbeatIntervalCount && hermesXEmUiModule) {
                    hermesXEmUiModule->setEmHeartbeatIntervalSec(kSetupHeartbeatIntervalOptions[index]);
                    hermesSetupToast = String(u8"Heartbeat週期: ") + kSetupHeartbeatIntervalLabels[index];
                    hermesSetupToastUntilMs = millis() + 1500;
                }
                resetMenu(HermesFastSetupPage::EmacEmInfoMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::EmacEmInfoMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::EmacOfflineThresholdSelect) {
        const int count = kSetupOfflineThresholdCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (gHermesFastSetupNavigation.selected == 0) {
                resetMenu(HermesFastSetupPage::EmacEmInfoMenu);
            } else {
                const uint8_t index = static_cast<uint8_t>(gHermesFastSetupNavigation.selected - 1);
                if (index < kSetupOfflineThresholdCount && hermesXEmUiModule) {
                    hermesXEmUiModule->setEmOfflineThresholdCount(kSetupOfflineThresholdOptions[index]);
                    hermesSetupToast = String(u8"離線門檻: ") + kSetupOfflineThresholdLabels[index];
                    hermesSetupToastUntilMs = millis() + 1500;
                }
                resetMenu(HermesFastSetupPage::EmacEmInfoMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::EmacEmInfoMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::EmacBatteryIncludeSelect) {
        if (handleMenuNav(3)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (gHermesFastSetupNavigation.selected == 0) {
                resetMenu(HermesFastSetupPage::EmacEmInfoMenu);
            } else {
                const bool enabled = (gHermesFastSetupNavigation.selected == 2);
                if (hermesXEmUiModule) {
                    hermesXEmUiModule->setEmBatteryIncluded(enabled);
                }
                hermesSetupToast = enabled ? u8"附帶電量已開啟" : u8"附帶電量已關閉";
                hermesSetupToastUntilMs = millis() + 1500;
                resetMenu(HermesFastSetupPage::EmacEmInfoMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::EmacEmInfoMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UiMenu) {
        if (handleMenuNav(kSetupUiMenuCount)) {
            const char *itemName =
                HermesXFastSetupUiController::instance().uiMenuItemName(gHermesFastSetupNavigation.selected);
            LOG_INFO("[HermesFastSetup] select=%d item=%s", gHermesFastSetupNavigation.selected, itemName);
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            const auto action =
                HermesXFastSetupUiController::instance().activateUiMenu(gHermesFastSetupNavigation.selected);
            if (action == HermesXFastSetupAction::ToggleBuzzerRequested) {
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
            } else if (action == HermesXFastSetupAction::OpenUiBrightnessRequested) {
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
            } else if (action == HermesXFastSetupAction::ToggleRgbRequested) {
                moduleConfig.has_ambient_lighting = true;
                if (moduleConfig.ambient_lighting.current == 0) {
                    moduleConfig.ambient_lighting.current = 10;
                }
                moduleConfig.ambient_lighting.led_state = !moduleConfig.ambient_lighting.led_state;
                saveSetupSegments(SEGMENT_MODULECONFIG);
                hermesSetupToast = moduleConfig.ambient_lighting.led_state ? u8"狀態燈已開啟 (重開生效)"
                                                                           : u8"狀態燈已關閉 (重開生效)";
                hermesSetupToastUntilMs = millis() + 1800;
            } else if (action == HermesXFastSetupAction::OpenScreenSleepRequested) {
                const uint8_t selected = getSetupScreenSleepSelection(getSetupCurrentScreenSleepSeconds());
                enterMenu(HermesFastSetupPage::UiScreenSleepSelect, kSetupScreenSleepCount + 1, selected);
            } else if (action == HermesXFastSetupAction::OpenTimezoneRequested) {
                const uint8_t selected = getSetupTimezoneSelection(config.device.tzdef);
                enterMenu(HermesFastSetupPage::UiTimezoneSelect, kSetupTimezoneCount + 1, selected);
            } else if (action == HermesXFastSetupAction::OpenRotarySwapRequested) {
                const bool rotarySwapped =
                    moduleConfig.canned_message.inputbroker_event_cw ==
                    meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP;
                enterMenu(HermesFastSetupPage::UiRotarySwapSelect, 3, rotarySwapped ? 2 : 1);
            } else if (action == HermesXFastSetupAction::ToggleIncomingPopupRequested) {
                const bool next = !isIncomingTextPopupEnabled();
                setIncomingTextPopupEnabled(next);
                hermesSetupToast = next ? u8"新訊息提示已開啟" : u8"新訊息提示已關閉";
                hermesSetupToastUntilMs = millis() + 1500;
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UiRotarySwapSelect) {
        if (handleMenuNav(3)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (gHermesFastSetupNavigation.selected == 0) {
                resetMenu(HermesFastSetupPage::UiMenu);
            } else {
                const bool swapEnabled = (gHermesFastSetupNavigation.selected == 2);
                moduleConfig.canned_message.inputbroker_event_cw =
                    swapEnabled ? meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP
                                : meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN;
                moduleConfig.canned_message.inputbroker_event_ccw =
                    swapEnabled ? meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN
                                : meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP;
                saveSetupSegments(SEGMENT_MODULECONFIG);
                if (rotaryEncoderInterruptImpl1) {
                    rotaryEncoderInterruptImpl1->applyConfiguredEvents();
                }
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UiBrightnessSelect) {
        if (handleMenuNav(kSetupBrightnessCount + 1)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (gHermesFastSetupNavigation.selected == 0) {
                resetMenu(HermesFastSetupPage::UiMenu);
            } else {
                const uint8_t idx = static_cast<uint8_t>(gHermesFastSetupNavigation.selected - 1);
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UiScreenSleepSelect) {
        if (handleMenuNav(kSetupScreenSleepCount + 1)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (gHermesFastSetupNavigation.selected == 0) {
                resetMenu(HermesFastSetupPage::UiMenu);
            } else {
                const uint8_t idx = static_cast<uint8_t>(gHermesFastSetupNavigation.selected - 1);
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::UiTimezoneSelect) {
        if (handleMenuNav(kSetupTimezoneCount + 1)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (gHermesFastSetupNavigation.selected == 0) {
                resetMenu(HermesFastSetupPage::UiMenu);
            } else {
                const uint8_t idx = static_cast<uint8_t>(gHermesFastSetupNavigation.selected - 1);
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::LoraMenu) {
        if (handleMenuNav(kSetupLoraMenuCount)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            const auto action =
                HermesXFastSetupUiController::instance().activateLoraMenu(gHermesFastSetupNavigation.selected);
            if (action == HermesXFastSetupAction::OpenLoraRoleRequested) {
                int selected = 1;
                for (uint8_t i = 0; i < kSetupRoleOptionCount; ++i) {
                    if (kSetupRoleOptions[i].role == config.device.role) {
                        selected = i + 1;
                        break;
                    }
                }
                enterMenu(HermesFastSetupPage::LoraRoleSelect, kSetupRoleOptionCount + 1, selected);
            } else if (action == HermesXFastSetupAction::OpenLoraPresetRequested) {
                int selected = 1;
                for (uint8_t i = 0; i < kSetupLoraPresetOptionCount; ++i) {
                    if (kSetupLoraPresetOptions[i].preset == config.lora.modem_preset) {
                        selected = i + 1;
                        break;
                    }
                }
                enterMenu(HermesFastSetupPage::LoraPresetSelect, kSetupLoraPresetOptionCount + 1, selected);
            } else if (action == HermesXFastSetupAction::OpenLoraRegionRequested) {
                int selected = 1;
                const uint8_t regionCount = getSetupRegionOptionCount();
                for (uint8_t i = 0; i < regionCount; ++i) {
                    if (regions[i].code == config.lora.region) {
                        selected = i + 1;
                        break;
                    }
                }
                enterMenu(HermesFastSetupPage::LoraRegionSelect, regionCount + 1, selected);
            } else if (action == HermesXFastSetupAction::ToggleIgnoreMqttRequested) {
                config.lora.ignore_mqtt = !config.lora.ignore_mqtt;
                saveSetupSegments(SEGMENT_CONFIG);
                hermesSetupToast = config.lora.ignore_mqtt ? u8"已改為忽視 MQTT" : u8"已接收 MQTT";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (action == HermesXFastSetupAction::ToggleMqttForwardRequested) {
                config.lora.config_ok_to_mqtt = !config.lora.config_ok_to_mqtt;
                saveSetupSegments(SEGMENT_CONFIG);
                hermesSetupToast = config.lora.config_ok_to_mqtt ? u8"允許轉發至 MQTT" : u8"禁止轉發至 MQTT";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (action == HermesXFastSetupAction::ToggleLoraTxRequested) {
                config.lora.tx_enabled = !config.lora.tx_enabled;
                saveSetupSegments(SEGMENT_CONFIG);
                hermesSetupToast = config.lora.tx_enabled ? u8"LoRa 已啟用" : u8"LoRa 已停用";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (action == HermesXFastSetupAction::OpenLoraChannelSlotRequested) {
                const int selected = config.lora.channel_num ? (config.lora.channel_num + 1) : 1;
                enterMenu(HermesFastSetupPage::LoraChannelSlotSelect, getSetupLoraChannelSlotCount() + 2, selected);
            } else if (action == HermesXFastSetupAction::EditLoraFrequencyRequested) {
                hermesSetupFrequencyDraft =
                    (fabsf(config.lora.override_frequency) < 0.0001f) ? String("") : formatSetupFrequencyLabel(config.lora.override_frequency);
                hermesSetupKeyRow = 0;
                hermesSetupKeyCol = 0;
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::LoraRoleSelect) {
        const int count = kSetupRoleOptionCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (gHermesFastSetupNavigation.selected == 0) {
                resetMenu(HermesFastSetupPage::LoraMenu);
            } else {
                const uint8_t index = gHermesFastSetupNavigation.selected - 1;
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::LoraPresetSelect) {
        const int count = kSetupLoraPresetOptionCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (gHermesFastSetupNavigation.selected == 0) {
                resetMenu(HermesFastSetupPage::LoraMenu);
            } else {
                const uint8_t index = gHermesFastSetupNavigation.selected - 1;
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::LoraRegionSelect) {
        const int count = getSetupRegionOptionCount() + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (gHermesFastSetupNavigation.selected == 0) {
                resetMenu(HermesFastSetupPage::LoraMenu);
            } else {
                const uint8_t index = gHermesFastSetupNavigation.selected - 1;
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::LoraChannelSlotSelect) {
        const int count = getSetupLoraChannelSlotCount() + 2;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (gHermesFastSetupNavigation.selected == 0) {
                resetMenu(HermesFastSetupPage::LoraMenu);
            } else if (gHermesFastSetupNavigation.selected == 1) {
                config.lora.channel_num = 0;
                saveSetupSegments(SEGMENT_CONFIG);
                hermesSetupToast = u8"頻段槽位: 自動";
                hermesSetupToastUntilMs = millis() + 1500;
                resetMenu(HermesFastSetupPage::LoraMenu);
            } else {
                config.lora.channel_num = gHermesFastSetupNavigation.selected - 1;
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::CannedMenu) {
        if (handleMenuNav(2)) {
            LOG_INFO("[HermesFastSetup] select=%d item=%s", gHermesFastSetupNavigation.selected, (gHermesFastSetupNavigation.selected == 0) ? "返回" : "目標頻道");
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            const auto action =
                HermesXFastSetupUiController::instance().activateCannedMenu(gHermesFastSetupNavigation.selected);
            if (action == HermesXFastSetupAction::OpenCannedChannelRequested) {
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::CannedChannelSelect) {
        ChannelIndex channelList[MAX_NUM_CHANNELS];
        const uint8_t channelCount = buildSetupChannelList(channelList, MAX_NUM_CHANNELS);
        const int count = channelCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (gHermesFastSetupNavigation.selected == 0) {
                resetMenu(HermesFastSetupPage::CannedMenu);
            } else {
                const uint8_t index = gHermesFastSetupNavigation.selected - 1;
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::GpsMenu) {
        if (handleMenuNav(kSetupGpsMenuCount)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            const auto action =
                HermesXFastSetupUiController::instance().activateGpsMenu(gHermesFastSetupNavigation.selected);
            if (action == HermesXFastSetupAction::OpenGpsUpdateRequested) {
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
            } else if (action == HermesXFastSetupAction::OpenGpsBroadcastRequested) {
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
            } else if (action == HermesXFastSetupAction::ToggleGpsSmartRequested) {
                config.position.position_broadcast_smart_enabled = !config.position.position_broadcast_smart_enabled;
                saveSetupSegments(SEGMENT_CONFIG);
                hermesSetupToast = config.position.position_broadcast_smart_enabled ? u8"智慧位置已啟用"
                                                                                   : u8"智慧位置已停用";
                hermesSetupToastUntilMs = millis() + 1500;
            } else if (action == HermesXFastSetupAction::OpenGpsSmartDistanceRequested) {
                int selected = 1;
                const uint32_t current = getSetupCurrentGpsSmartDistance();
                for (uint8_t i = 0; i < kSetupGpsSmartDistanceCount; ++i) {
                    if (kSetupGpsSmartDistanceOptions[i] == current) {
                        selected = i + 1;
                        break;
                    }
                }
                enterMenu(HermesFastSetupPage::GpsSmartDistanceSelect, kSetupGpsSmartDistanceCount + 1, selected);
            } else if (action == HermesXFastSetupAction::OpenGpsSmartIntervalRequested) {
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::GpsUpdateSelect) {
        const int count = kSetupGpsUpdateCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (gHermesFastSetupNavigation.selected == 0) {
                resetMenu(HermesFastSetupPage::GpsMenu);
            } else {
                const uint8_t index = gHermesFastSetupNavigation.selected - 1;
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::GpsBroadcastSelect) {
        const int count = kSetupGpsBroadcastCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (gHermesFastSetupNavigation.selected == 0) {
                resetMenu(HermesFastSetupPage::GpsMenu);
            } else {
                const uint8_t index = gHermesFastSetupNavigation.selected - 1;
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::GpsSmartDistanceSelect) {
        const int count = kSetupGpsSmartDistanceCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (gHermesFastSetupNavigation.selected == 0) {
                resetMenu(HermesFastSetupPage::GpsMenu);
            } else {
                const uint8_t index = gHermesFastSetupNavigation.selected - 1;
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

    if (gHermesFastSetupNavigation.page == HermesFastSetupPage::GpsSmartIntervalSelect) {
        const int count = kSetupGpsSmartIntervalCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (gHermesFastSetupNavigation.selected == 0) {
                resetMenu(HermesFastSetupPage::GpsMenu);
            } else {
                const uint8_t index = gHermesFastSetupNavigation.selected - 1;
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

bool Screen::handleTextMessagePopupInput(const InputEvent *event)
{
    if (!event || !isIncomingTextPopupActive()) {
        return false;
    }
    const HermesXMessageInputBindings bindings{
        static_cast<char>(moduleConfig.canned_message.inputbroker_event_press),
        static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw),
        static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw),
    };
    const HermesXMessageInput input{event->source, event->inputEvent};
    const HermesXMessageInputAction action = HermesXMessageUiController::instance().handlePopup(input, bindings);

    if (action == HermesXMessageInputAction::OpenPopupDetail) {
        if (cannedMessageModule) {
            const auto runState = cannedMessageModule->getRunState();
            if (runState != CANNED_MESSAGE_RUN_STATE_DISABLED && runState != CANNED_MESSAGE_RUN_STATE_INACTIVE) {
                cannedMessageModule->exitMenu();
            }
        }
        dismissIncomingTextPopup();
        showTextMessageDetailPage();
        return true;
    }
    if (action == HermesXMessageInputAction::DismissPopup) {
        dismissIncomingTextPopup();
        setFastFramerate();
        return true;
    }
    if (action == HermesXMessageInputAction::Refresh) {
        setFastFramerate();
    }
    return action != HermesXMessageInputAction::Unhandled;
}

bool Screen::handleTraceRoutePopupInput(const InputEvent *event)
{
    if (!event || !isTraceRoutePopupVisible()) {
        return false;
    }

    const HermesXTraceRoutePopupInput input(event->source, event->inputEvent);
    const HermesXTraceRoutePopupBindings bindings(
        static_cast<char>(moduleConfig.canned_message.inputbroker_event_press),
        static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw),
        static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw));
    const HermesXTraceRoutePopupAction action = HermesXTraceRouteUiController::instance().handlePopup(
        input, bindings, millis(), FONT_HEIGHT_SMALL + 2, kTraceRoutePopupDismissGuardMs);

    if (action == HermesXTraceRoutePopupAction::DismissRequested) {
        dismissTraceRoutePopup();
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
        if (screen && (screen->isFinderNodeListPageActive() || screen->isFinderNodeDetailPageActive() ||
                       screen->isTraceRouteNodeListPageActive() || screen->isTraceRouteNodeDetailPageActive())) {
            auto *tft = static_cast<TFTDisplay *>(dispdev);
            ui->init();
            tft->resetColorPalette(true);
            tft->markColorPaletteDirty();
            hermesFinderTftFullRepaintUntilMs = millis() + 2000;
        }
#endif
        requestImmediateRedraw();
        setFastFramerate();
    } else if (action == HermesXTraceRoutePopupAction::Refresh) {
        setFastFramerate();
        requestImmediateRedraw();
    }
    return true;
}

bool Screen::handleSetupDetailPopupInput(const InputEvent *event)
{
    if (!event || !isSetupDetailPopupVisible()) {
        return false;
    }

    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);
    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
    const bool isRotary = (event->source && strncmp(event->source, "rotEnc", 6) == 0);

    const bool isSelect =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const bool isConfiguredPress = (eventPress != 0) && (event->inputEvent == eventPress);
    const bool wantsDismiss = isConfiguredPress || (eventPress == 0 && isSelect) ||
                              event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK) ||
                              event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL);

    const bool isUp = event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
    const bool isDown =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
    const bool isLeft =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT);
    const bool isRight =
        event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT);
    const bool isCw = (eventCw != 0) && (event->inputEvent == eventCw);
    const bool isCcw = (eventCcw != 0) && (event->inputEvent == eventCcw);

    int8_t navDir = 0;
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

    const HermesXDetailPopupAction action = HermesXDetailPopupController::instance().handle(
        wantsDismiss, navDir, millis(), FONT_HEIGHT_SMALL + 2);
    if (action == HermesXDetailPopupAction::Dismissed || action == HermesXDetailPopupAction::Refresh) {
        setFastFramerate();
    }
    return action != HermesXDetailPopupAction::Unhandled;
}

bool Screen::handleRecentTextMessageListInput(const InputEvent *event)
{
    if (!event) {
        return false;
    }
    const HermesXMessageInputBindings bindings{
        static_cast<char>(moduleConfig.canned_message.inputbroker_event_press),
        static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw),
        static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw),
    };
    const HermesXMessageInput input{event->source, event->inputEvent};
    const HermesXMessageInputAction action = HermesXMessageUiController::instance().handleRecentList(input, bindings);
    if (action == HermesXMessageInputAction::BackToAction) {
        showHermesXActionPage();
        setFastFramerate();
        return true;
    }
    if (action == HermesXMessageInputAction::OpenDetail) {
        showTextMessageDetailPage();
        return true;
    }
    if (action == HermesXMessageInputAction::Refresh) {
        setFastFramerate();
    }
    return action != HermesXMessageInputAction::Unhandled;
}

bool Screen::handleRecentTextMessageDetailInput(const InputEvent *event)
{
    if (!event || !showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.textMessageList >= framesetInfo.frameCount) {
        return false;
    }

    const HermesXMessageInputBindings bindings{
        static_cast<char>(moduleConfig.canned_message.inputbroker_event_press),
        static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw),
        static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw),
    };
    const uint16_t scrollStep =
        std::max<int>(FONT_HEIGHT_MEDIUM, HermesXMessageUiRenderer::bodyHanziPixelSize()) + 2;
    const HermesXMessageInputAction action =
        HermesXMessageUiController::instance().handleRecentDetail({event->source, event->inputEvent}, bindings, scrollStep);
    if (action == HermesXMessageInputAction::BackToList) {
        ui->switchToFrame(framesetInfo.positions.textMessageList);
        setFastFramerate();
        return true;
    }
    if (action == HermesXMessageInputAction::Refresh) {
        setFastFramerate();
    }
    return action != HermesXMessageInputAction::Unhandled;
}

bool Screen::handleOnlineNodeListInput(const InputEvent *event)
{
    if (!event || !showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.onlineList >= framesetInfo.frameCount) {
        return false;
    }

    if (hermesFinderUiMode == HermesFinderUiMode::Menu) {
        const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
        const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
        const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);
        const HermesXNodeBrowserAction action = HermesXNodeBrowserUiController::instance().handleMenu(
            {event->source, event->inputEvent}, {eventPress, eventCw, eventCcw}, hermesFinderMenuSelected, 3);
        if (action == HermesXNodeBrowserAction::Refresh) {
            setFastFramerate();
            return true;
        }
        if (action == HermesXNodeBrowserAction::ExitRequested) {
            hermesFinderUiMode = HermesFinderUiMode::None;
            showHermesXActionPage();
            setFastFramerate();
            return true;
        }
        if (action == HermesXNodeBrowserAction::ActivateRequested) {
            if (hermesFinderMenuSelected == 1) {
                HermesXNodeBrowserUiController::instance().beginFinderPulseConfirm(millis());
            } else if (hermesFinderMenuSelected == 2) {
                hermesFinderUiMode = HermesFinderUiMode::PositionList;
                gNodeBrowserDataSource.refreshFinder();
                gNodeBrowserUiModel.reset(gFinderNodeState);
                showFinderListPageSafely(true);
            }
            setFastFramerate();
            return true;
        }
        return true;
    }

    const bool finderMode = hermesFinderUiMode == HermesFinderUiMode::PositionList;
    if (finderMode) {
        gNodeBrowserDataSource.refreshFinder();
    } else {
        gNodeBrowserDataSource.refreshOnline();
    }
    LOG_INFO("[Screen] ONLINE list input src=%s event=%d frame=%u onlineList=%u onlineDetail=%u cursor=%u count=%u",
             event->source ? event->source : "(null)", event->inputEvent, ui->getUiState()->currentFrame,
             framesetInfo.positions.onlineList, framesetInfo.positions.onlineDetail,
             finderMode ? gFinderNodeState.listCursor : gOnlineNodeState.listCursor,
             finderMode ? gFinderNodeState.count : gOnlineNodeState.count);

    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);
    HermesXNodeBrowserState &browserState = finderMode ? gFinderNodeState : gOnlineNodeState;
    const HermesXNodeBrowserAction action = HermesXNodeBrowserUiController::instance().handleList(
        {event->source, event->inputEvent}, {eventPress, eventCw, eventCcw}, browserState);
    if (action == HermesXNodeBrowserAction::ExitRequested || action == HermesXNodeBrowserAction::CancelRequested) {
        if (finderMode) {
            hermesFinderUiMode = HermesFinderUiMode::Menu;
        } else {
            showHermesXActionPage();
        }
    } else if (action == HermesXNodeBrowserAction::OpenDetailRequested) {
        showOnlineNodeDetailPage();
    }
    if (action == HermesXNodeBrowserAction::Refresh || action == HermesXNodeBrowserAction::ExitRequested ||
        action == HermesXNodeBrowserAction::CancelRequested ||
        action == HermesXNodeBrowserAction::OpenDetailRequested) {
        setFastFramerate();
    }
    return action != HermesXNodeBrowserAction::Unhandled;
}

bool Screen::handleOnlineNodeDetailInput(const InputEvent *event)
{
    if (!event || !showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.onlineList >= framesetInfo.frameCount ||
        framesetInfo.positions.onlineDetail >= framesetInfo.frameCount) {
        return false;
    }

    const bool finderMode = hermesFinderUiMode == HermesFinderUiMode::PositionList;
    const meshtastic_NodeInfoLite *node = finderMode ? gNodeBrowserDataSource.selectedFinderNode()
                                                     : gNodeBrowserDataSource.selectedOnlineNode();
    LOG_INFO("[Screen] ONLINE detail input src=%s event=%d frame=%u onlineList=%u onlineDetail=%u cursor=%u node=%08lx",
             event->source ? event->source : "(null)", event->inputEvent, ui->getUiState()->currentFrame,
             framesetInfo.positions.onlineList, framesetInfo.positions.onlineDetail,
             finderMode ? gFinderNodeState.detailCursor : gOnlineNodeState.detailCursor,
             static_cast<unsigned long>(node ? node->num : 0));
    if (!node) {
        if (finderMode) {
            showFinderListPageSafely(true);
        } else {
            showOnlineNodeListPage();
        }
        setFastFramerate();
        return true;
    }

    const uint8_t rowCount = buildOnlineNodeDetailRows(*node, finderMode, nullptr, 0);
    uint8_t &detailCursor = finderMode ? gFinderNodeState.detailCursor : gOnlineNodeState.detailCursor;
    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);
    const HermesXNodeBrowserAction action = HermesXNodeBrowserUiController::instance().handleDetail(
        {event->source, event->inputEvent}, {eventPress, eventCw, eventCcw}, detailCursor, rowCount);
    if (action == HermesXNodeBrowserAction::BackToListRequested) {
        if (finderMode) {
            showFinderListPageSafely(true);
        } else {
            showOnlineNodeListPage();
        }
    } else if (action == HermesXNodeBrowserAction::ActivateRequested) {
        if (!finderMode && detailCursor == kOnlineDetailMessageRow) {
            LOG_INFO("[Screen] ONLINE detail select -> MSG node=%08lx", static_cast<unsigned long>(node->num));
            startDirectMessageComposer(node->num, false);
        } else if (!finderMode && detailCursor == kOnlineDetailTraceRouteRow) {
            LOG_INFO("[Screen] ONLINE detail select -> TraceRoute node=%08lx via_mqtt=%u", static_cast<unsigned long>(node->num),
                     node->via_mqtt ? 1 : 0);
            if (node->via_mqtt) {
                if (HermesXInterfaceModule::instance) {
                    HermesXInterfaceModule::instance->playNackFail();
                }
                showTraceRoutePopup("TraceRoute", "LORA ONLY");
            } else {
                sendOnlineNodeTraceRoute(node->num);
            }
        }
    }
    if (action == HermesXNodeBrowserAction::Refresh || action == HermesXNodeBrowserAction::BackToListRequested ||
        action == HermesXNodeBrowserAction::ActivateRequested) {
        setFastFramerate();
    }
    return action != HermesXNodeBrowserAction::Unhandled;
}

bool Screen::handleTraceRouteNodeListInput(const InputEvent *event)
{
    if (!event || !showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.traceRouteList >= framesetInfo.frameCount ||
        framesetInfo.positions.traceRouteDetail >= framesetInfo.frameCount) {
        return false;
    }

    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);

    if (gTraceRouteSearchState.resultVisible || gTraceRouteSearchState.active) {
        const HermesXTraceRoutePopupInput searchInput(event->source, event->inputEvent);
        const HermesXTraceRoutePopupBindings searchBindings(eventPress, eventCw, eventCcw);
        const HermesXTraceRouteSearchAction searchAction =
            HermesXTraceRouteUiController::instance().handleSearch(searchInput, searchBindings, millis());

        if (searchAction == HermesXTraceRouteSearchAction::SearchRequested) {
            gNodeBrowserDataSource.refreshTraceRoute();
            const NodeNum foundNode = gNodeBrowserDataSource.findTraceRouteNodeByShortName(gTraceRouteSearchState.draft);
            const bool found = foundNode != 0;
            if (found) {
                LOG_INFO("[Screen] TraceRoute search found short=%s node=%08lx",
                         gTraceRouteSearchState.draft.c_str(), static_cast<unsigned long>(foundNode));
            }
            const String query = gTraceRouteSearchState.draft;
            HermesXTraceRouteUiModel::instance().showSearchResult(query, found, foundNode);
            setFastFramerate();
            requestImmediateRedraw();
        } else if (searchAction == HermesXTraceRouteSearchAction::BindResultRequested ||
                   searchAction == HermesXTraceRouteSearchAction::ExitResultRequested) {
            if (searchAction == HermesXTraceRouteSearchAction::BindResultRequested) {
                const NodeNum resultNode = gTraceRouteSearchState.resultNode;
                const bool bound = bindTraceRouteNode(resultNode);
                LOG_INFO("[Screen] TraceRoute search bind node=%08lx result=%u",
                         static_cast<unsigned long>(resultNode), bound ? 1 : 0);
            }
            HermesXTraceRouteUiModel::instance().resetSearch();
            gTraceRouteNavigationState.mode = HermesXTraceRouteUiMode::BindOnline;
            gTraceRouteNavigationState.bindCursor = kTraceRouteBindBackRow;
            gTraceRouteNavigationState.bindSelectedIndex = 0;
            setFastFramerate();
            requestImmediateRedraw();
        } else if (searchAction == HermesXTraceRouteSearchAction::CloseRequested) {
            HermesXTraceRouteUiModel::instance().resetSearch();
            setFastFramerate();
            requestImmediateRedraw();
        } else if (searchAction == HermesXTraceRouteSearchAction::Refresh) {
            setFastFramerate();
            requestImmediateRedraw();
        }
        return true;
    }

    gNodeBrowserDataSource.refreshTraceRoute();
    const HermesXTraceRoutePopupInput listInput(event->source, event->inputEvent);
    const HermesXTraceRoutePopupBindings listBindings(eventPress, eventCw, eventCcw);
    const HermesXTraceRouteListAction listAction =
        HermesXTraceRouteUiController::instance().handleList(
            listInput, listBindings, gNodeBrowserDataSource.traceRouteCount());

    if (listAction == HermesXTraceRouteListAction::Ignored) {
        return false;
    }

    if (listAction == HermesXTraceRouteListAction::ExitFeatureRequested) {
        showHermesXActionPage();
    } else if (listAction == HermesXTraceRouteListAction::OpenSearchRequested) {
        HermesXTraceRouteUiModel::instance().beginSearch();
        requestImmediateRedraw();
    } else if (listAction == HermesXTraceRouteListAction::ShowBindInfoRequested) {
        if (showTraceRouteBindInfoForSelectedNode()) {
            requestImmediateRedraw();
        }
    } else if (listAction == HermesXTraceRouteListAction::DeferBindInfoRequested) {
        deferTraceRouteBindShortPressForSelectedNode();
    } else if (listAction == HermesXTraceRouteListAction::OpenBoundRoutesRequested) {
        syncTraceRouteBoundSelection();
        showTraceRouteNodeDetailPage();
    } else if (listAction == HermesXTraceRouteListAction::BindConfirmed) {
        const NodeNum nodeNum = gTraceRouteNavigationState.confirmNode;
        HermesXTraceRouteUiModel::instance().dismissBindConfirm();
        const bool bound = bindTraceRouteNode(nodeNum);
        showTraceRoutePopup("TraceRoute", bound ? u8"已綁定" : u8"已存在或已滿");
    }

    if (listAction != HermesXTraceRouteListAction::Consumed) {
        setFastFramerate();
    }
    return true;
}

bool Screen::handleTraceRouteNodeDetailInput(const InputEvent *event)
{
    if (!event || !showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.traceRouteList >= framesetInfo.frameCount ||
        framesetInfo.positions.traceRouteDetail >= framesetInfo.frameCount) {
        return false;
    }

    syncTraceRouteBoundSelection();

    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);

    const HermesXTraceRoutePopupInput listInput(event->source, event->inputEvent);
    const HermesXTraceRoutePopupBindings listBindings(eventPress, eventCw, eventCcw);
    const HermesXTraceRouteBoundListAction listAction = HermesXTraceRouteUiController::instance().handleBoundList(
        listInput, listBindings, gTraceRouteBindings.count());

    if (listAction == HermesXTraceRouteBoundListAction::Ignored) {
        return false;
    }

    if (listAction == HermesXTraceRouteBoundListAction::ReturnToMenuRequested) {
        showTraceRouteNodeListPage();
    } else if (listAction == HermesXTraceRouteBoundListAction::SendRequested) {
        sendTraceRouteForSelectedBoundNode();
    } else if (listAction == HermesXTraceRouteBoundListAction::DeferSendRequested) {
        deferTraceRouteBoundShortPressForSelectedNode();
    }
    if (listAction != HermesXTraceRouteBoundListAction::Consumed) {
        setFastFramerate();
    }
    return true;
}

bool Screen::handleTraceRouteBindLongPress()
{
    if (!showingNormalScreen || !ui || !isTraceRouteNodeListPageActive() ||
        gTraceRouteNavigationState.mode != HermesXTraceRouteUiMode::BindOnline || gTraceRouteNavigationState.confirmVisible ||
        gTraceRouteSearchState.active || gTraceRouteSearchState.resultVisible ||
        !isTraceRouteBindNodeRow(gTraceRouteNavigationState.bindCursor)) {
        LOG_DEBUG("[Screen] TraceRoute bind long ignored normal=%u ui=%u active=%u mode=%u confirm=%u cursor=%u",
                  showingNormalScreen ? 1 : 0, ui ? 1 : 0, isTraceRouteNodeListPageActive() ? 1 : 0,
                  static_cast<unsigned>(gTraceRouteNavigationState.mode), gTraceRouteNavigationState.confirmVisible ? 1 : 0,
                  static_cast<unsigned>(gTraceRouteNavigationState.bindCursor));
        return false;
    }
    openTraceRouteBindConfirmForSelectedNode();
    setFastFramerate();
    requestImmediateRedraw();
    return true;
}

bool Screen::shouldUseTraceRouteBindQuickLongPress() const
{
    return showingNormalScreen && ui && isTraceRouteNodeListPageActive() &&
           gTraceRouteNavigationState.mode == HermesXTraceRouteUiMode::BindOnline && !gTraceRouteNavigationState.confirmVisible &&
           !gTraceRouteSearchState.active && !gTraceRouteSearchState.resultVisible &&
           isTraceRouteBindNodeRow(gTraceRouteNavigationState.bindCursor);
}

bool Screen::shouldUseTraceRouteBoundQuickLongPress() const
{
    return showingNormalScreen && ui && isTraceRouteNodeDetailPageActive() &&
           gTraceRouteNavigationState.mode == HermesXTraceRouteUiMode::BoundRoutes && gTraceRouteNavigationState.boundCursor != 0 &&
           gTraceRouteBindings.count() > 0;
}

bool Screen::handleTraceRouteBoundLongPress()
{
    if (!shouldUseTraceRouteBoundQuickLongPress()) {
        return false;
    }
    if (!unbindSelectedTraceRouteBoundNode()) {
        return false;
    }
    setFastFramerate();
    requestImmediateRedraw();
    return true;
}

void Screen::completeDeferredTraceRouteBindShortPress()
{
    if (!gTraceRouteNavigationState.deferredBindVisible) {
        return;
    }

    const uint8_t deferredCursor = gTraceRouteNavigationState.deferredBindCursor;
    const NodeNum deferredNode = gTraceRouteNavigationState.deferredBindNode;
    cancelDeferredTraceRouteBindShortPress();

    if (!showingNormalScreen || !ui || !isTraceRouteNodeListPageActive() || gTraceRouteSearchState.active ||
        gTraceRouteSearchState.resultVisible ||
        gTraceRouteNavigationState.mode != HermesXTraceRouteUiMode::BindOnline || gTraceRouteNavigationState.confirmVisible ||
        !isTraceRouteBindNodeRow(deferredCursor) || deferredNode == 0) {
        return;
    }

    gNodeBrowserDataSource.refreshTraceRoute();
    const uint8_t deferredIndex = traceRouteBindNodeIndex(deferredCursor);
    if (deferredIndex >= gNodeBrowserDataSource.traceRouteCount()) {
        return;
    }

    gTraceRouteNavigationState.bindCursor = deferredCursor;
    gTraceRouteNavigationState.bindSelectedIndex = deferredIndex;
    const meshtastic_NodeInfoLite *node = getSelectedTraceRouteNode();
    if (!node || node->num != deferredNode) {
        LOG_WARN("[Screen] TraceRoute bind deferred short stale node=%08lx cursor=%u",
                 static_cast<unsigned long>(deferredNode), static_cast<unsigned>(deferredCursor));
        return;
    }

    if (showTraceRouteBindInfoForSelectedNode()) {
        setFastFramerate();
        requestImmediateRedraw();
    }
}

void Screen::completeDeferredTraceRouteBoundShortPress()
{
    if (!gTraceRouteNavigationState.deferredRouteVisible) {
        return;
    }

    const uint8_t deferredCursor = gTraceRouteNavigationState.deferredRouteCursor;
    const NodeNum deferredNode = gTraceRouteNavigationState.deferredRouteNode;
    cancelDeferredTraceRouteBoundShortPress();

    if (!showingNormalScreen || !ui || !isTraceRouteNodeDetailPageActive() ||
        gTraceRouteNavigationState.mode != HermesXTraceRouteUiMode::BoundRoutes || deferredCursor == 0 || deferredNode == 0) {
        return;
    }

    syncTraceRouteBoundSelection();
    if (deferredCursor > gTraceRouteBindings.count()) {
        return;
    }

    gTraceRouteNavigationState.boundCursor = deferredCursor;
    gTraceRouteNavigationState.boundSelectedIndex = deferredCursor - 1;
    const meshtastic_NodeInfoLite *node = getSelectedTraceRouteNode();
    if (!node || node->num != deferredNode) {
        LOG_WARN("[Screen] TraceRoute bound deferred short stale node=%08lx cursor=%u",
                 static_cast<unsigned long>(deferredNode), static_cast<unsigned>(deferredCursor));
        return;
    }

    if (sendTraceRouteForSelectedBoundNode()) {
        setFastFramerate();
        requestImmediateRedraw();
    }
}

bool Screen::handleDirectMessageComposerInput(const InputEvent *event)
{
    if (!event || !gDirectMessageComposer.active()) {
        return false;
    }

    const HermesXComposerInput composerInput(event->source, event->inputEvent, event->kbchar);
    const HermesXComposerInputBindings bindings(
        static_cast<char>(moduleConfig.canned_message.inputbroker_event_press),
        static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw),
        static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw));
    const HermesXComposerAction action = gDirectMessageComposer.handleInput(
        composerInput, bindings, millis(), meshtastic_Constants_DATA_PAYLOAD_LEN);

    if (action == HermesXComposerAction::CloseRequested) {
        const bool wasGroup = gDirectMessageComposer.fromGroupDetail();
        stopDirectMessageComposer();
        if (wasGroup) {
            showGroupNodeDetailPage();
        } else {
            showOnlineNodeDetailPage();
        }
        setFastFramerate();
        requestImmediateRedraw();
        return true;
    }

    if (action == HermesXComposerAction::SendRequested) {
        const bool sent = sendDirectTextMessage(gDirectMessageComposer.destination(), gDirectMessageComposer.draft());
        if (HermesXInterfaceModule::instance) {
            if (sent) {
                HermesXInterfaceModule::instance->playSendFeedback();
            } else {
                HermesXInterfaceModule::instance->playNackFail();
            }
        }
        if (sent) {
            const bool wasGroup = gDirectMessageComposer.fromGroupDetail();
            stopDirectMessageComposer();
            if (wasGroup) {
                showGroupNodeDetailPage();
            } else {
                showOnlineNodeDetailPage();
            }
            requestImmediateRedraw();
        } else {
            gDirectMessageComposer.showSendFailure(millis());
        }
        setFastFramerate();
        return true;
    }

    if (action == HermesXComposerAction::Refresh) {
        setFastFramerate();
        requestImmediateRedraw();
    }
    return true;
}

bool Screen::handleFinderNodeListInput(const InputEvent *event)
{
    if (!event || !showingNormalScreen || !ui || framesetInfo.positions.finderList >= framesetInfo.frameCount) {
        return false;
    }

    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);
    const HermesXNodeBrowserInput input(event->source, event->inputEvent);
    const HermesXNodeBrowserInputBindings bindings(eventPress, eventCw, eventCcw);
    auto &controller = HermesXNodeBrowserUiController::instance();

    if (hermesFinderUiMode == HermesFinderUiMode::Menu) {
        const HermesXNodeBrowserAction action = controller.handleMenu(input, bindings, hermesFinderMenuSelected, 3);
        if (action == HermesXNodeBrowserAction::Refresh) {
            setFastFramerate();
            return true;
        }
        if (action == HermesXNodeBrowserAction::ExitRequested) {
            hermesFinderUiMode = HermesFinderUiMode::None;
            showHermesXActionPage();
            setFastFramerate();
            return true;
        }
        if (action == HermesXNodeBrowserAction::ActivateRequested) {
            if (hermesFinderMenuSelected == 1) {
                HermesXNodeBrowserUiController::instance().beginFinderPulseConfirm(millis());
            } else if (hermesFinderMenuSelected == 2) {
                hermesFinderUiMode = HermesFinderUiMode::PositionList;
                gNodeBrowserDataSource.refreshFinder();
                gNodeBrowserUiModel.reset(gFinderNodeState);
                showFinderListPageSafely(true);
            }
            setFastFramerate();
            return true;
        }
        return true;
    }

    hermesFinderUiMode = HermesFinderUiMode::PositionList;
    gNodeBrowserDataSource.refreshFinder();
    LOG_INFO("[Screen] FINDER list input src=%s event=%d frame=%u finderList=%u finderDetail=%u cursor=%u count=%u",
             event->source ? event->source : "(null)", event->inputEvent, ui->getUiState()->currentFrame,
             framesetInfo.positions.finderList, framesetInfo.positions.finderDetail, gFinderNodeState.listCursor,
             gFinderNodeState.count);

    const HermesXNodeBrowserAction action = controller.handleList(input, bindings, gFinderNodeState);
    if (action == HermesXNodeBrowserAction::ExitRequested) {
        hermesFinderUiMode = HermesFinderUiMode::None;
        showHermesXActionPage();
    } else if (action == HermesXNodeBrowserAction::CancelRequested) {
        hermesFinderUiMode = HermesFinderUiMode::Menu;
        hermesFinderMenuSelected = 2;
        showFinderListPageSafely(true);
    } else if (action == HermesXNodeBrowserAction::OpenDetailRequested) {
        showFinderNodeDetailPage();
    }
    if (action == HermesXNodeBrowserAction::Refresh || action == HermesXNodeBrowserAction::ExitRequested ||
        action == HermesXNodeBrowserAction::CancelRequested ||
        action == HermesXNodeBrowserAction::OpenDetailRequested) {
        setFastFramerate();
    }
    return action != HermesXNodeBrowserAction::Unhandled;
}

bool Screen::handleFinderNodeDetailInput(const InputEvent *event)
{
    if (!event || !showingNormalScreen || !ui || framesetInfo.positions.finderDetail >= framesetInfo.frameCount) {
        return false;
    }

    const meshtastic_NodeInfoLite *node = gNodeBrowserDataSource.selectedFinderNode();
    LOG_INFO("[Screen] FINDER detail input src=%s event=%d frame=%u finderList=%u finderDetail=%u cursor=%u node=%08lx",
             event->source ? event->source : "(null)", event->inputEvent, ui->getUiState()->currentFrame,
             framesetInfo.positions.finderList, framesetInfo.positions.finderDetail, gFinderNodeState.detailCursor,
             static_cast<unsigned long>(node ? node->num : 0));
    if (!node) {
        hermesFinderUiMode = HermesFinderUiMode::PositionList;
        gNodeBrowserUiModel.reset(gFinderNodeState);
        if (framesetInfo.positions.finderList < framesetInfo.frameCount) {
            ui->switchToFrame(framesetInfo.positions.finderList);
        }
        requestImmediateRedraw();
        setFastFramerate();
        return true;
    }

    const uint8_t rowCount = buildOnlineNodeDetailRows(*node, true, nullptr, 0);
    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);
    const HermesXNodeBrowserAction action = HermesXNodeBrowserUiController::instance().handleDetail(
        {event->source, event->inputEvent}, {eventPress, eventCw, eventCcw}, gFinderNodeState.detailCursor, rowCount);
    if (action == HermesXNodeBrowserAction::BackToListRequested ||
        action == HermesXNodeBrowserAction::ActivateRequested) {
        showFinderListPageSafely(true);
    }
    if (action == HermesXNodeBrowserAction::Refresh || action == HermesXNodeBrowserAction::BackToListRequested ||
        action == HermesXNodeBrowserAction::ActivateRequested) {
        setFastFramerate();
    }
    return action != HermesXNodeBrowserAction::Unhandled;
}

bool Screen::handleGroupNodeListInput(const InputEvent *event)
{
    if (!event || !showingNormalScreen || !ui || framesetInfo.positions.groupList >= framesetInfo.frameCount) {
        return false;
    }

    clampGroupNodeState();
    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);
    const HermesXNodeBrowserInput input(event->source, event->inputEvent);
    const HermesXNodeBrowserInputBindings bindings(eventPress, eventCw, eventCcw);
    auto &controller = HermesXNodeBrowserUiController::instance();

    if (!gGroupNodeState.nodeListVisible) {
        const HermesXNodeBrowserAction action = controller.handleMenu(input, bindings, gGroupNodeState.menuCursor, 3);
        if (action == HermesXNodeBrowserAction::ExitRequested) {
            showHermesXActionPage();
        } else if (action == HermesXNodeBrowserAction::ActivateRequested) {
            if (gGroupNodeState.menuCursor == 1) {
                hermesSetupReturnToGroupMenu = true;
                gHermesFastSetupNavigation.page = HermesFastSetupPage::EmacMenu;
                gHermesFastSetupNavigation.selected = 0;
                gHermesFastSetupNavigation.offset = 0;
                gHermesFastSetupNavigation.lastNavAtMs = 0;
                gHermesFastSetupNavigation.lastNavDir = 0;
                ui->switchToFrame(framesetInfo.positions.setup);
            } else {
                gGroupNodeState.nodeListVisible = true;
                gNodeBrowserUiModel.resetGroupList();
            }
        }
        if (action == HermesXNodeBrowserAction::Refresh || action == HermesXNodeBrowserAction::ExitRequested ||
            action == HermesXNodeBrowserAction::ActivateRequested) {
            setFastFramerate();
        }
        return action != HermesXNodeBrowserAction::Unhandled;
    }

    const HermesXNodeBrowserAction action = controller.handleGroupList(
        input, bindings, gGroupNodeState, static_cast<uint8_t>(std::max(0, getGroupNodeCount())));
    if (action == HermesXNodeBrowserAction::ExitRequested || action == HermesXNodeBrowserAction::CancelRequested) {
        gGroupNodeState.nodeListVisible = false;
    } else if (action == HermesXNodeBrowserAction::OpenDetailRequested) {
        showGroupNodeDetailPage();
    }
    if (action == HermesXNodeBrowserAction::Refresh || action == HermesXNodeBrowserAction::ExitRequested ||
        action == HermesXNodeBrowserAction::CancelRequested ||
        action == HermesXNodeBrowserAction::OpenDetailRequested) {
        setFastFramerate();
    }
    return action != HermesXNodeBrowserAction::Unhandled;
}

bool Screen::handleGroupNodeDetailInput(const InputEvent *event)
{
    if (!event || !showingNormalScreen || !ui || framesetInfo.positions.groupDetail >= framesetInfo.frameCount) {
        return false;
    }

    const auto *entry = getSelectedGroupNode();
    if (!entry) {
        showGroupNodeListPage();
        setFastFramerate();
        return true;
    }

    const uint8_t rowCount = buildGroupNodeDetailRows(*entry, nullptr, 0);
    const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
    const char eventPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);
    const HermesXNodeBrowserAction action = HermesXNodeBrowserUiController::instance().handleDetail(
        {event->source, event->inputEvent}, {eventPress, eventCw, eventCcw}, gGroupNodeState.detailCursor, rowCount);
    if (action == HermesXNodeBrowserAction::BackToListRequested) {
        showGroupNodeListPage();
    } else if (action == HermesXNodeBrowserAction::ActivateRequested) {
        if (gGroupNodeState.detailCursor == kGroupDetailMessageRow) {
            startDirectMessageComposer(entry->nodeNum, true);
        } else if (gGroupNodeState.detailCursor == kGroupDetailTraceRouteRow) {
            const meshtastic_NodeInfoLite *node = getGroupMeshNode(*entry);
            if (node && node->via_mqtt) {
                if (HermesXInterfaceModule::instance) {
                    HermesXInterfaceModule::instance->playNackFail();
                }
                showTraceRoutePopup("TraceRoute", "LORA ONLY");
            } else {
                sendOnlineNodeTraceRoute(entry->nodeNum);
            }
        }
    }
    if (action == HermesXNodeBrowserAction::Refresh || action == HermesXNodeBrowserAction::BackToListRequested ||
        action == HermesXNodeBrowserAction::ActivateRequested) {
        setFastFramerate();
    }
    return action != HermesXNodeBrowserAction::Unhandled;
}

bool Screen::handleTakModeInput(const InputEvent *event)
{
    if (!event || !showingNormalScreen || !ui) {
        return false;
    }

    const char configuredCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
    const char configuredCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
    const char configuredPress = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);
    const char eventNone = static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_NONE);
    const char eventUp = static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
    const char eventDown = static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
    const char eventSelect = static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const char effectiveCw = (configuredCw == eventNone) ? eventDown : configuredCw;
    const char effectiveCcw = (configuredCcw == eventNone) ? eventUp : configuredCcw;
    const char effectivePress = (configuredPress == eventNone) ? eventSelect : configuredPress;

    uint8_t selectedChannelRow = 0;
    for (uint8_t i = 0; i < kTakMissionSlotCount; ++i) {
        if (kTakMissionSlotOptions[i] == gTakModeProfile.missionSlot) {
            selectedChannelRow = i + 1;
            break;
        }
    }

    const graphics::HermesXTakModeAction action = graphics::HermesXTakModeUiController::instance().handle(
        {event->source, event->inputEvent},
        {effectivePress, effectiveCw, effectiveCcw},
        gTakModeUiState,
        kTakPopupRowCount,
        kTakSettingsRowCount,
        kTakMissionSlotCount + 1,
        selectedChannelRow);

    if (action == graphics::HermesXTakModeAction::Unhandled) {
        return false;
    }
    if (action == graphics::HermesXTakModeAction::Consumed) {
        return true;
    }
    if (action == graphics::HermesXTakModeAction::Refresh) {
        setFastFramerate();
        return true;
    }
    if (action == graphics::HermesXTakModeAction::ExitToActionRequested) {
        gTakModeUiModel.showMain();
        showHermesXActionPage();
        return true;
    }

    if (action == graphics::HermesXTakModeAction::SettingSelected) {
        switch (gTakModeUiState.settingsSelected) {
        case 1:
            cycleTakOption(gTakModeProfile.nodeInfoBroadcastSecs, kTakNodeInfoOptions, kTakNodeInfoCount);
            break;
        case 2:
            cycleTakOption(gTakModeProfile.gpsUpdateIntervalSecs, kTakGpsUpdateOptions, kTakGpsUpdateCount);
            break;
        case 3:
            cycleTakOption(gTakModeProfile.positionBroadcastSecs, kTakPositionBroadcastOptions, kTakPositionBroadcastCount);
            break;
        case 4:
            cycleTakOption(gTakModeProfile.smartMinimumDistanceMeters, kTakSmartDistanceOptions, kTakSmartDistanceCount);
            break;
        case 5:
            cycleTakOption(gTakModeProfile.smartMinimumIntervalSecs, kTakSmartIntervalOptions, kTakSmartIntervalCount);
            break;
        case 6:
            if (HermesXInterfaceModule::instance) {
                HermesXInterfaceModule::instance->cycleSmartPowerMinDbm();
            }
            break;
        case 7:
            if (HermesXInterfaceModule::instance) {
                HermesXInterfaceModule::instance->cycleSmartPowerMaxDbm();
            }
            break;
        case 8:
            gTakModeProfile.quietOutputs = !gTakModeProfile.quietOutputs;
            break;
        case 9:
            gTakModeProfile.allowEmUi = !gTakModeProfile.allowEmUi;
            break;
        case 10:
            gTakModeProfile.allowFinder = !gTakModeProfile.allowFinder;
            break;
        default:
            break;
        }
        persistTakModeProfileToFile();
        if (isTakExperienceActive()) {
            applyTakModeSettings();
            syncRetainedTakState();
            persistTakStateToFile();
        }
        setFastFramerate();
        return true;
    }

    if (action == graphics::HermesXTakModeAction::ChannelSelected) {
        const uint8_t index = gTakModeUiState.settingsSelected - 1;
        if (index < kTakMissionSlotCount) {
            gTakModeProfile.missionSlot = kTakMissionSlotOptions[index];
            persistTakModeProfileToFile();
            if (isTakExperienceActive()) {
                applyTakModeSettings();
                syncRetainedTakState();
                persistTakStateToFile();
            }
        }
        setFastFramerate();
        return true;
    }

    if (action == graphics::HermesXTakModeAction::ToggleModeRequested) {
        if (!isTakExperienceActive()) {
            if (isStealthModeActive()) {
                if (screen) {
                    screen->print("Disable Stealth first\n");
                }
            } else if (enableTakMode()) {
                if (screen) {
                    screen->print("TAK MODE ON, rebooting...\n");
                }
                startTakModeTransition(true);
                rebootAtMsec = millis() + kTakModeTransitionRebootMs;
            }
        } else {
            if (!isTakModeActive() && isTakDeviceRole(config.device.role)) {
                enableTakMode();
            }
            if (!disableTakMode()) {
                setFastFramerate();
                return true;
            }
            if (screen) {
                screen->print("TAK MODE OFF, rebooting...\n");
            }
            startTakModeTransition(false);
            rebootAtMsec = millis() + kTakModeTransitionRebootMs;
        }
        setFastFramerate();
        return true;
    }

    if (action == graphics::HermesXTakModeAction::OpenGroupSettingsRequested) {
        if (framesetInfo.positions.setup >= framesetInfo.frameCount) {
            if (screen) {
                screen->print("GROUP settings unavailable\n");
            }
            setFastFramerate();
            return true;
        }
        gTakModeUiModel.showMain();
        gGroupNodeState.menuCursor = 1;
        gGroupNodeState.nodeListVisible = false;
        gNodeBrowserUiModel.resetGroupList();
        hermesSetupReturnToGroupMenu = true;
        gHermesFastSetupNavigation.page = HermesFastSetupPage::EmacMenu;
        gHermesFastSetupNavigation.selected = 0;
        gHermesFastSetupNavigation.offset = 0;
        gHermesFastSetupNavigation.lastNavAtMs = 0;
        gHermesFastSetupNavigation.lastNavDir = 0;
        ui->switchToFrame(framesetInfo.positions.setup);
        setFastFramerate();
        return true;
    }

    if (action == graphics::HermesXTakModeAction::OpenEmUiRequested) {
#if HERMESX_CIV_DISABLE_EMAC
        if (screen) {
            screen->print("EMUI disabled in CIV build\n");
        }
#else
        if (gTakModeProfile.allowEmUi && hermesXEmUiModule) {
            hermesXEmUiModule->enterEmergencyMode(u8"TAK MODE");
        } else if (screen) {
            screen->print("EMUI disabled in TAK profile\n");
        }
#endif
        setFastFramerate();
        return true;
    }

    if (action == graphics::HermesXTakModeAction::OpenFinderRequested) {
        if (gTakModeProfile.allowFinder) {
            hermesFinderUiMode = HermesFinderUiMode::Menu;
            hermesFinderMenuSelected = 0;
            showFinderListPageSafely(true);
        } else if (screen) {
            screen->print("Finder disabled in TAK profile\n");
        }
        setFastFramerate();
        return true;
    }

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

    HermesXUiInputState inputState;
    inputState.updateModal = hermesUpdateModalActive;
    if (HermesXUiInputRouter::selectTarget(inputState) == HermesXUiInputTarget::UpdateModal) {
        if (handleHermesFastSetupInput(event)) {
            return 0;
        }
        return 0;
    }

    // Use left or right input from a keyboard to move between frames,
    // so long as a mesh module isn't using these events for some other purpose
    if (showingNormalScreen) {
        const uint8_t currentFrame = this->ui->getUiState()->currentFrame;
        const bool hasMenuFooter = shouldShowHermesXMenuFooter(currentFrame);
        const char eventCw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_cw);
        const char eventCcw = static_cast<char>(moduleConfig.canned_message.inputbroker_event_ccw);
        const bool isRotary = (event->source && strncmp(event->source, "rotEnc", 6) == 0);
        const bool isCw = (eventCw != 0) && (event->inputEvent == eventCw);
        const bool isCcw = (eventCcw != 0) && (event->inputEvent == eventCcw);
        const bool isUp = event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
        const bool isDown =
            event->inputEvent == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
        const bool hasRotaryFallback = isRotary && (eventCw == 0 && eventCcw == 0) && (isUp || isDown);

        inputState.lowMemoryReminder = gLowMemoryUiState.visible;
        inputState.rotaryLockPopup = gRotaryLockUiState.visible;
        inputState.emergencyConfirm = gEmergencyConfirmUiState.visible;
        inputState.finderPulseConfirm = gFinderPulseState.confirmVisible;
        inputState.finderPulseSending = gFinderPulseState.sendingVisible;
        inputState.traceRoutePopup = isTraceRoutePopupVisible();
        inputState.setupDetailPopup = isSetupDetailPopupVisible();
        inputState.incomingTextPopup = isIncomingTextPopupActive();
        inputState.takMode = isTakModePageActive();
        inputState.actionPage = currentFrame == framesetInfo.positions.mainAction;
        inputState.fastSetup = currentFrame == framesetInfo.positions.setup;
        inputState.recentMessageList = isRecentTextMessagesPageActive();
        inputState.recentMessageDetail = isRecentTextMessageDetailPageActive();
        inputState.finderNodeList = isFinderNodeListPageActive();
        inputState.finderNodeDetail = isFinderNodeDetailPageActive();
        inputState.onlineNodeList = isOnlineNodeListPageActive();
        inputState.onlineNodeDetail = isOnlineNodeDetailPageActive();
        inputState.traceRouteNodeList = isTraceRouteNodeListPageActive();
        inputState.traceRouteNodeDetail = isTraceRouteNodeDetailPageActive();
        inputState.groupNodeList = isGroupNodeListPageActive();
        inputState.groupNodeDetail = isGroupNodeDetailPageActive();

        const HermesXUiInputTarget inputTarget = HermesXUiInputRouter::selectTarget(inputState);
        switch (inputTarget) {
        case HermesXUiInputTarget::LowMemoryReminder:
            handleLowMemoryReminderInput(event);
            return 0;
        case HermesXUiInputTarget::RotaryLockPopup:
            handleRotaryLockInput(event);
            return 0;
        case HermesXUiInputTarget::EmergencyConfirm:
            handleEmergencyConfirmInput(event);
            return 0;
        case HermesXUiInputTarget::FinderPulseConfirm:
            handleFinderPulseConfirmInput(event);
            return 0;
        case HermesXUiInputTarget::FinderPulseSending:
            handleFinderPulseSendingInput(event);
            return 0;
        case HermesXUiInputTarget::TraceRoutePopup:
            handleTraceRoutePopupInput(event);
            return 0;
        case HermesXUiInputTarget::SetupDetailPopup:
            handleSetupDetailPopupInput(event);
            return 0;
        case HermesXUiInputTarget::IncomingTextPopup:
            handleTextMessagePopupInput(event);
            return 0;
        case HermesXUiInputTarget::TakMode:
            LOG_INFO("[Screen] handleInputEvent route -> TAK mode ui frame=%u", currentFrame);
            handleTakModeInput(event);
            return 0;
        default:
            break;
        }

        if (isHermesXMainPageActive() && cannedMessageModule) {
            const auto runState = cannedMessageModule->getRunState();
            const bool cannedInactive =
                (runState == CANNED_MESSAGE_RUN_STATE_DISABLED || runState == CANNED_MESSAGE_RUN_STATE_INACTIVE);
            const bool wantsHomeCannedOpen =
                !gLowMemoryProtectionActive && isRotary && cannedInactive && (isCw || isCcw || hasRotaryFallback);
            if (wantsHomeCannedOpen && cannedMessageModule->openMenu()) {
                setFastFramerate();
                return 0;
            }
        }

        if (inputTarget == HermesXUiInputTarget::ActionPage) {
            handleHermesXActionInput(event);
            // Action page handles navigation internally; keep frame switching locked.
            return 0;
        }

        if (inputTarget == HermesXUiInputTarget::FastSetup) {
            handleHermesFastSetupInput(event);
            // FastSetup page locks frame switching unless Exit is pressed.
            return 0;
        }

        if (inputTarget == HermesXUiInputTarget::RecentMessageList) {
            handleRecentTextMessageListInput(event);
            // Recent message list handles its own navigation and open-detail flow.
            return 0;
        }

        if (inputTarget == HermesXUiInputTarget::RecentMessageDetail) {
            if (handleRecentTextMessageDetailInput(event)) {
                return 0;
            }
        }

        if (inputTarget == HermesXUiInputTarget::FinderNodeList) {
            LOG_INFO("[Screen] handleInputEvent route -> FINDER list frame=%u", currentFrame);
            handleFinderNodeListInput(event);
            return 0;
        }

        if (inputTarget == HermesXUiInputTarget::FinderNodeDetail) {
            LOG_INFO("[Screen] handleInputEvent route -> FINDER detail frame=%u", currentFrame);
            handleFinderNodeDetailInput(event);
            return 0;
        }

        if (inputTarget == HermesXUiInputTarget::OnlineNodeList) {
            LOG_INFO("[Screen] handleInputEvent route -> ONLINE list frame=%u", currentFrame);
            handleOnlineNodeListInput(event);
            return 0;
        }

        if (inputTarget == HermesXUiInputTarget::OnlineNodeDetail) {
            LOG_INFO("[Screen] handleInputEvent route -> ONLINE detail frame=%u", currentFrame);
            if (handleDirectMessageComposerInput(event)) {
                return 0;
            }
            if (handleOnlineNodeDetailInput(event)) {
                return 0;
            }
        }

        if (inputTarget == HermesXUiInputTarget::TraceRouteNodeList) {
            LOG_INFO("[Screen] handleInputEvent route -> TraceRoute list frame=%u", currentFrame);
            handleTraceRouteNodeListInput(event);
            return 0;
        }

        if (inputTarget == HermesXUiInputTarget::TraceRouteNodeDetail) {
            LOG_INFO("[Screen] handleInputEvent route -> TraceRoute detail frame=%u", currentFrame);
            handleTraceRouteNodeDetailInput(event);
            return 0;
        }

        if (inputTarget == HermesXUiInputTarget::GroupNodeList) {
            LOG_INFO("[Screen] handleInputEvent route -> GROUP list frame=%u", currentFrame);
            handleGroupNodeListInput(event);
            return 0;
        }

        if (inputTarget == HermesXUiInputTarget::GroupNodeDetail) {
            LOG_INFO("[Screen] handleInputEvent route -> GROUP detail frame=%u", currentFrame);
            if (handleDirectMessageComposerInput(event)) {
                return 0;
            }
            if (handleGroupNodeDetailInput(event)) {
                return 0;
            }
            return 0;
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
    if (framesetInfo.positions.setup >= framesetInfo.frameCount) {
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
    if (framesetInfo.positions.mainAction >= framesetInfo.frameCount) {
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

bool Screen::isOnlineNodeListPageActive() const
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.onlineList >= framesetInfo.frameCount) {
        return false;
    }
    return ui->getUiState()->currentFrame == framesetInfo.positions.onlineList;
}

bool Screen::isOnlineNodeDetailPageActive() const
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.onlineDetail >= framesetInfo.frameCount) {
        return false;
    }
    return ui->getUiState()->currentFrame == framesetInfo.positions.onlineDetail;
}

bool Screen::isTraceRouteNodeListPageActive() const
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.traceRouteList >= framesetInfo.frameCount) {
        return false;
    }
    return ui->getUiState()->currentFrame == framesetInfo.positions.traceRouteList;
}

bool Screen::isTraceRouteNodeDetailPageActive() const
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.traceRouteDetail >= framesetInfo.frameCount) {
        return false;
    }
    return ui->getUiState()->currentFrame == framesetInfo.positions.traceRouteDetail;
}

bool Screen::isFinderNodeListPageActive() const
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.finderList >= framesetInfo.frameCount) {
        return false;
    }
    return ui->getUiState()->currentFrame == framesetInfo.positions.finderList;
}

bool Screen::isFinderNodeDetailPageActive() const
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.finderDetail >= framesetInfo.frameCount) {
        return false;
    }
    return ui->getUiState()->currentFrame == framesetInfo.positions.finderDetail;
}

bool Screen::isGroupNodeListPageActive() const
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.groupList >= framesetInfo.frameCount) {
        return false;
    }
    return ui->getUiState()->currentFrame == framesetInfo.positions.groupList;
}

bool Screen::isGroupNodeDetailPageActive() const
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.groupDetail >= framesetInfo.frameCount) {
        return false;
    }
    return ui->getUiState()->currentFrame == framesetInfo.positions.groupDetail;
}

bool Screen::isTakModePageActive() const
{
    if (!showingNormalScreen || !ui || !ui->getUiState()) {
        return false;
    }
    if (framesetInfo.positions.main >= framesetInfo.frameCount) {
        return false;
    }
    const bool onMainFrame = ui->getUiState()->currentFrame == framesetInfo.positions.main;
    return onMainFrame && (isTakExperienceActive() || gTakModeUiState.page != graphics::HermesXTakModePage::Main);
}

bool Screen::isFinderPulseSendingVisible() const
{
    return gFinderPulseState.sendingVisible;
}

bool Screen::isHermesInputOverlayActive() const
{
    return hermesUpdateModalActive || gLowMemoryUiState.visible || gEmergencyConfirmUiState.visible ||
           gRotaryLockUiState.visible ||
           gFinderPulseState.confirmVisible || gFinderPulseState.sendingVisible || isIncomingTextPopupActive() ||
           isTraceRoutePopupVisible() || isSetupDetailPopupVisible();
}

bool Screen::shouldAllowRotaryLockLongPress() const
{
    if (!showingNormalScreen || !ui || !ui->getUiState()) {
        return false;
    }
    if (isHermesInputOverlayActive()) {
        return false;
    }
    if (shouldBlockPowerHoldForTraceRouteInput()) {
        return false;
    }
    if (framesetInfo.positions.main >= framesetInfo.frameCount) {
        return false;
    }
    const bool onFixedMainFrame = ui->getUiState()->frameState == FIXED &&
                                  ui->getUiState()->currentFrame == framesetInfo.positions.main;
    return onFixedMainFrame && isSmartPowerHomeActive();
}

bool Screen::shouldSuppressRotaryShortPressAfterHold(uint32_t heldMs) const
{
    constexpr uint32_t kTraceRouteRotaryLongPressMs = 1000;
    if (heldMs < kTraceRouteRotaryLongPressMs) {
        return false;
    }
    if (!showingNormalScreen || !ui) {
        return false;
    }
    return shouldBlockPowerHoldForTraceRouteInput();
}

bool Screen::shouldBlockPowerHoldForTraceRouteInput() const
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    return isTraceRouteNodeListPageActive() || isTraceRouteNodeDetailPageActive();
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
    if (gLowMemoryProtectionActive) {
        return false;
    }
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
    if ((framesetInfo.positions.textMessageList < framesetInfo.frameCount && frameIndex == framesetInfo.positions.textMessageList) ||
        (framesetInfo.positions.textMessage < framesetInfo.frameCount && frameIndex == framesetInfo.positions.textMessage)) {
        return false;
    }
    if (frameIndex == framesetInfo.positions.mainAction) { // Already the menu page.
        return false;
    }
    if (frameIndex == framesetInfo.positions.setup) { // FastSetup already has its own navigation model.
        return false;
    }
    if (frameIndex == framesetInfo.positions.main &&
        (isTakExperienceActive() || gTakModeUiState.page != graphics::HermesXTakModePage::Main)) {
        return false;
    }
    if (frameIndex == framesetInfo.positions.onlineList || frameIndex == framesetInfo.positions.onlineDetail) {
        return false;
    }
    if (frameIndex == framesetInfo.positions.traceRouteList || frameIndex == framesetInfo.positions.traceRouteDetail) {
        return false;
    }
    if (frameIndex == framesetInfo.positions.finderList || frameIndex == framesetInfo.positions.finderDetail) {
        return false;
    }
    if (frameIndex == framesetInfo.positions.groupList || frameIndex == framesetInfo.positions.groupDetail) {
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
    if (gLowMemoryProtectionActive) {
        return false;
    }
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.mainAction >= framesetInfo.frameCount) {
        return false;
    }
    hermesActionFeatureMenuActive = false;
    hermesActionSelected = kMainActionHomeIndex;
    hermesActionLastNavAtMs = 0;
    hermesActionLastNavDir = 0;
    ui->switchToFrame(framesetInfo.positions.mainAction);
    setFastFramerate();
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

    if (cannedMessageModule) {
        const auto runState = cannedMessageModule->getRunState();
        if (runState != CANNED_MESSAGE_RUN_STATE_DISABLED && runState != CANNED_MESSAGE_RUN_STATE_INACTIVE) {
            cannedMessageModule->exitMenu();
        }
    }
    ui->switchToFrame(framesetInfo.positions.textMessageList);
    setFastFramerate();
    return true;
}

bool Screen::showOnlineNodeListPage()
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.onlineList >= framesetInfo.frameCount) {
        return false;
    }

    ui->switchToFrame(framesetInfo.positions.onlineList);
    setFastFramerate();
    return true;
}

bool Screen::showFinderNodeListPage()
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.finderList >= framesetInfo.frameCount) {
        return false;
    }

    ui->switchToFrame(framesetInfo.positions.finderList);
    setFastFramerate();
    return true;
}

bool Screen::showFinderListPageSafely(bool fallbackToActionPage)
{
    if (!ui) {
        return false;
    }

    const bool wasScreenOn = screenOn;
    if (!showingNormalScreen) {
        setFrames(FOCUS_PRESERVE);
    }
    if (!screenOn) {
        handleSetOn(true);
    }

#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||     \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
    auto *tft = static_cast<TFTDisplay *>(dispdev);
    dispdev->displayOn();
    ui->init();
    invalidateDirectTftWakeCaches();
    tft->fillRect565(0, 0, dispdev->getWidth(), dispdev->getHeight(), TFTDisplay::rgb565(0x00, 0x00, 0x00));
    tft->resetColorPalette(true);
    tft->markColorPaletteDirty();
    if (showingNormalScreen && gNormalFramesInitializedAfterBoot) {
        setFrames(FOCUS_PRESERVE);
    }
#endif

    if (showFinderNodeListPage()) {
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||     \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
        if (ui->getUiState()) {
            ui->getUiState()->lastUpdate = 0;
        }
        LOG_INFO("[Screen] Finder list display recovery frame=%u finderList=%u count=%u woke=%u",
                 ui->getUiState() ? ui->getUiState()->currentFrame : 0xFF, framesetInfo.positions.finderList,
                 static_cast<unsigned>(gFinderNodeState.count), wasScreenOn ? 0 : 1);
        hermesFinderTftFullRepaintUntilMs = millis() + 2000;
#endif
        requestImmediateRedraw();
        return true;
    }

    LOG_WARN("[Screen] Finder list frame unavailable normal=%u on=%u finderList=%u frameCount=%u",
             showingNormalScreen ? 1 : 0, screenOn ? 1 : 0, static_cast<unsigned>(framesetInfo.positions.finderList),
             static_cast<unsigned>(framesetInfo.frameCount));
    if (fallbackToActionPage && showHermesXActionPage()) {
        requestImmediateRedraw();
    }
    return false;
}

bool Screen::showOnlineNodeDetailPage()
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.onlineDetail >= framesetInfo.frameCount) {
        return false;
    }

    ui->switchToFrame(framesetInfo.positions.onlineDetail);
    setFastFramerate();
    return true;
}

bool Screen::showTraceRouteNodeListPage()
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.traceRouteList >= framesetInfo.frameCount) {
        return false;
    }

    ui->switchToFrame(framesetInfo.positions.traceRouteList);
    setFastFramerate();
    return true;
}

bool Screen::showTraceRouteNodeDetailPage()
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.traceRouteDetail >= framesetInfo.frameCount) {
        return false;
    }

    ui->switchToFrame(framesetInfo.positions.traceRouteDetail);
    setFastFramerate();
    return true;
}

bool Screen::showFinderNodeDetailPage()
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.finderDetail >= framesetInfo.frameCount) {
        return false;
    }

    ui->switchToFrame(framesetInfo.positions.finderDetail);
    setFastFramerate();
    return true;
}

bool Screen::showGroupNodeListPage()
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.groupList >= framesetInfo.frameCount) {
        return false;
    }

    if (hermesSetupReturnToGroupMenu) {
        hermesSetupReturnToGroupMenu = false;
        gGroupNodeState.nodeListVisible = false;
    }
    ui->switchToFrame(framesetInfo.positions.groupList);
    setFastFramerate();
    return true;
}

bool Screen::showGroupNodeDetailPage()
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    if (framesetInfo.positions.groupDetail >= framesetInfo.frameCount) {
        return false;
    }

    ui->switchToFrame(framesetInfo.positions.groupDetail);
    setFastFramerate();
    return true;
}

bool Screen::showTakModePage()
{
    if (!showingNormalScreen || !ui) {
        return false;
    }

    if (!isTakExperienceActive()) {
        if (isStealthModeActive()) {
            return false;
        }
        if (!enableTakMode()) {
            return false;
        }
        startTakModeTransition(true);
        rebootAtMsec = millis() + kTakModeTransitionRebootMs;
        return true;
    }

    gTakModeUiModel.showPopup();
    showHermesXMainPage();
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

    if (cannedMessageModule) {
        const auto runState = cannedMessageModule->getRunState();
        if (runState != CANNED_MESSAGE_RUN_STATE_DISABLED && runState != CANNED_MESSAGE_RUN_STATE_INACTIVE) {
            cannedMessageModule->exitMenu();
        }
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
