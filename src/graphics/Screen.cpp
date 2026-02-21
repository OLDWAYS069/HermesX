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
#if HAS_SCREEN
#include <OLEDDisplay.h>

#include "DisplayFormatters.h"
#if !MESHTASTIC_EXCLUDE_GPS
#include "GPS.h"
#endif
#include "ButtonThread.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "error.h"
#include "gps/GeoCoord.h"
#include "modules/HermesEmUiModule.h"
#include "gps/RTC.h"
#include "graphics/ScreenFonts.h"
// --- HermesX Remove TFT fast-path START
#include "graphics/fonts/HermesX_zh/HermesX_CN12.h"
#include <inttypes.h>
#include <math.h>
#include <time.h>
// --- HermesX Remove TFT fast-path END
#include "graphics/images.h"
#include "input/ScanAndSelect.h"
#include "input/TouchScreenImpl1.h"
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
#include "modules/LighthouseModule.h"
#include "modules/HermesXInterfaceModule.h"
#include "modules/TextMessageModule.h"
#include "modules/WaypointModule.h"
#if !MESHTASTIC_EXCLUDE_HERMESX && defined(HERMESX_GUARD_POWER_ANIMATIONS)
#include "modules/HermesXPowerGuard.h"
#endif
#include "sleep.h"
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
    if (hermesXEmUiModule) {
        hermesXEmUiModule->drawOverlay(display, state);
    }
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

/// Draw the last text message we received
static void drawTextMessageFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    // the max length of this buffer is much longer than we can possibly print
    static char tempBuf[237];

    const meshtastic_MeshPacket &mp = devicestate.rx_text_message;
    meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(getFrom(&mp));
    // LOG_DEBUG("Draw text message from 0x%x: %s", mp.from,
    // mp.decoded.variant.data.decoded.bytes);

    // Demo for drawStringMaxWidth:
    // with the third parameter you can define the width after which words will
    // be wrapped. Currently only spaces and "-" are allowed for wrapping
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);
    if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_INVERTED) {
        display->fillRect(0 + x, 0 + y, x + display->getWidth(), y + FONT_HEIGHT_SMALL);
        display->setColor(BLACK);
    }

    // For time delta
    uint32_t seconds = sinceReceived(&mp);
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    uint32_t days = hours / 24;

    // For timestamp
    uint8_t timestampHours, timestampMinutes;
    int32_t daysAgo;
    bool useTimestamp = deltaToTimestamp(seconds, &timestampHours, &timestampMinutes, &daysAgo);

    // If bold, draw twice, shifting right by one pixel
    for (uint8_t xOff = 0; xOff <= (config.display.heading_bold ? 1 : 0); xOff++) {
        // Show a timestamp if received today, but longer than 15 minutes ago
        if (useTimestamp && minutes >= 15 && daysAgo == 0) {
            display->drawStringf(xOff + x, 0 + y, tempBuf, "At %02hu:%02hu from %s", timestampHours, timestampMinutes,
                                 (node && node->has_user) ? node->user.short_name : "???");
        }
        // Timestamp yesterday (if display is wide enough)
        else if (useTimestamp && daysAgo == 1 && display->width() >= 200) {
            display->drawStringf(xOff + x, 0 + y, tempBuf, "Yesterday %02hu:%02hu from %s", timestampHours, timestampMinutes,
                                 (node && node->has_user) ? node->user.short_name : "???");
        }
        // Otherwise, show a time delta
        else {
            display->drawStringf(xOff + x, 0 + y, tempBuf, "%s ago from %s",
                                 screen->drawTimeDelta(days, hours, minutes, seconds).c_str(),
                                 (node && node->has_user) ? node->user.short_name : "???");
        }
    }

    display->setColor(WHITE);
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
        snprintf(tempBuf, sizeof(tempBuf), "%s", mp.decoded.payload.bytes);
        // --- HermesX Remove TFT fast-path START
        if (screen)
            HermesX_zh::drawMixedBounded(*display, 0 + x, 0 + y + FONT_HEIGHT_SMALL, display->getWidth(), tempBuf, 12,
                                         FONT_HEIGHT_SMALL, nullptr);
        else
            HermesX_zh::drawMixedBounded(*display, 0 + x, 0 + y + FONT_HEIGHT_SMALL, display->getWidth(), tempBuf, 12,
                                         FONT_HEIGHT_SMALL, nullptr);
        // --- HermesX Remove TFT fast-path END
    }
#else
    snprintf(tempBuf, sizeof(tempBuf), "%s", mp.decoded.payload.bytes);
    // --- HermesX Remove TFT fast-path START
    if (screen)
        HermesX_zh::drawMixedBounded(*display, 0 + x, 0 + y + FONT_HEIGHT_SMALL, display->getWidth(), tempBuf, 12,
                                     FONT_HEIGHT_SMALL, nullptr);
    else
        HermesX_zh::drawMixedBounded(*display, 0 + x, 0 + y + FONT_HEIGHT_SMALL, display->getWidth(), tempBuf, 12,
                                     FONT_HEIGHT_SMALL, nullptr);
    // --- HermesX Remove TFT fast-path END
#endif
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

    int16_t armThickness = size / 12;
    if (armThickness < 2) {
        armThickness = 2;
    }

    const int16_t centerX = x + (size * 42) / 100;
    const int16_t centerY = y + (size * 42) / 100;
    const int16_t bodyRadius = size / 6;

    drawThickLine(display, x + size / 14, y + (size * 72) / 100, x + (size * 78) / 100, y + size / 14, armThickness);
    display->fillCircle(centerX, centerY, bodyRadius);

    const int16_t dishRadius = size / 8;
    const int16_t dishX = centerX + bodyRadius + dishRadius / 2;
    const int16_t dishY = centerY + bodyRadius + dishRadius / 2;

    display->fillCircle(dishX, dishY, dishRadius);
    display->setColor(BLACK);
    display->fillCircle(dishX + dishRadius / 2, dishY - dishRadius / 2, dishRadius);
    display->setColor(WHITE);
}

static void drawHermesGpsHeroFrame(OLEDDisplay *display, int16_t x, int16_t y, const GPSStatus *gps)
{
    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();

    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setColor(BLACK);
    display->fillRect(x, y, width, height);
    display->setColor(WHITE);

    int16_t leftPaneWidth = (width * 36) / 100;
    if (leftPaneWidth < 90) {
        leftPaneWidth = 90;
    }
    if (leftPaneWidth > width - 100) {
        leftPaneWidth = width - 100;
    }

    int16_t iconSize = leftPaneWidth - 14;
    const int16_t maxIconHeight = height - FONT_HEIGHT_SMALL - 16;
    if (iconSize > maxIconHeight) {
        iconSize = maxIconHeight;
    }
    if (iconSize < 24) {
        iconSize = 24;
    }

    const int16_t iconX = x + (leftPaneWidth - iconSize) / 2;
    const int16_t iconY = y + 4;
    drawHermesGpsSatelliteIcon(display, iconX, iconY, iconSize);

    char satLine[48];
    snprintf(satLine, sizeof(satLine), u8"衛星數量 : %u", gps->getNumSatellites());
    HermesX_zh::drawMixedBounded(*display, x + 4, y + height - FONT_HEIGHT_SMALL - 4, leftPaneWidth - 8, satLine,
                                 HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);

    const int16_t rightGap = 8;
    const int16_t rightX = x + leftPaneWidth + rightGap;
    const int16_t rightWidth = width - leftPaneWidth - rightGap - 4;

    const char *lockStatus = u8"已停用";
    if (config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_ENABLED) {
        if (config.position.fixed_position) {
            lockStatus = u8"固定座標";
        } else if (!gps->getIsConnected()) {
            lockStatus = u8"無 GPS";
        } else if (gps->getHasLock()) {
            lockStatus = u8"已鎖定";
        } else {
            lockStatus = u8"搜尋中";
        }
    }

    char lockLine[64];
    snprintf(lockLine, sizeof(lockLine), "%s%s", u8"衛星定位 : ", lockStatus);

    int16_t rowY = y + 6;
    HermesX_zh::drawMixedBounded(*display, rightX, rowY, rightWidth, lockLine, HermesX_zh::GLYPH_WIDTH,
                                 FONT_HEIGHT_SMALL, nullptr);
    rowY += FONT_HEIGHT_SMALL + 6;

    HermesX_zh::drawMixedBounded(*display, rightX, rowY, rightWidth, u8"座標 :", HermesX_zh::GLYPH_WIDTH,
                                 FONT_HEIGHT_SMALL, nullptr);
    rowY += FONT_HEIGHT_SMALL + 2;

    char lonLine[24] = "--";
    char latLine[24] = "--";
    const bool hasGpsCoordinates = (config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_ENABLED) &&
                                   (config.position.fixed_position || (gps->getIsConnected() && gps->getHasLock()));
    if (hasGpsCoordinates) {
        const double lat = static_cast<double>(gps->getLatitude()) * 1e-7;
        const double lon = static_cast<double>(gps->getLongitude()) * 1e-7;
        snprintf(lonLine, sizeof(lonLine), "%.6f", lon);
        snprintf(latLine, sizeof(latLine), "%.6f", lat);
    }

    const bool largeValueFont = (width >= 280 && height >= 180);
    display->setFont(largeValueFont ? FONT_MEDIUM : FONT_SMALL);
    const int16_t valueHeight = largeValueFont ? FONT_HEIGHT_MEDIUM : FONT_HEIGHT_SMALL;
    display->drawStringMaxWidth(rightX, rowY, rightWidth, lonLine);
    rowY += valueHeight;
    display->drawStringMaxWidth(rightX, rowY, rightWidth, latLine);
    rowY += valueHeight + 4;

    display->setFont(FONT_SMALL);
    HermesX_zh::drawMixedBounded(*display, rightX, rowY, rightWidth, u8"時間 :", HermesX_zh::GLYPH_WIDTH,
                                 FONT_HEIGHT_SMALL, nullptr);
    rowY += FONT_HEIGHT_SMALL + 2;

    char dateLine[20] = "--/--/--";
    char timeLine[20] = "--:--:--";
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
        }
    }

    display->setFont(largeValueFont ? FONT_MEDIUM : FONT_SMALL);
    display->drawStringMaxWidth(rightX, rowY, rightWidth, dateLine);
    rowY += valueHeight;
    display->drawStringMaxWidth(rightX, rowY, rightWidth, timeLine);
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
constexpr uint8_t kMainActionCount = 3;
constexpr uint32_t kStealthConfirmArmMs = 3000;

static const char *kSetupRootItems[] = {u8"返回", u8"EMAC設定", u8"UI設定", u8"節點設定", u8"罐頭訊息",
                                        u8"儲存並重新開機"};
static const uint8_t kSetupRootCount = sizeof(kSetupRootItems) / sizeof(kSetupRootItems[0]);
static const char *kSetupEmacItems[] = {u8"返回", u8"設定密碼A", u8"設定密碼B", u8"顯示密碼", u8"EMAC解除"};
static const uint8_t kSetupEmacCount = sizeof(kSetupEmacItems) / sizeof(kSetupEmacItems[0]);

struct SetupRoleOption {
    meshtastic_Config_DeviceConfig_Role role;
    const char *label;
};

static const SetupRoleOption kSetupRoleOptions[] = {
    {meshtastic_Config_DeviceConfig_Role_CLIENT, "Client"},
    {meshtastic_Config_DeviceConfig_Role_CLIENT_MUTE, "Client Mute"},
    {meshtastic_Config_DeviceConfig_Role_ROUTER, "Router"},
    {meshtastic_Config_DeviceConfig_Role_ROUTER_CLIENT, "Router+Client"},
    {meshtastic_Config_DeviceConfig_Role_REPEATER, "Repeater"},
    {meshtastic_Config_DeviceConfig_Role_TRACKER, "Tracker"},
    {meshtastic_Config_DeviceConfig_Role_SENSOR, "Sensor"},
    {meshtastic_Config_DeviceConfig_Role_TAK, "TAK"},
    {meshtastic_Config_DeviceConfig_Role_CLIENT_HIDDEN, "Client Hidden"},
    {meshtastic_Config_DeviceConfig_Role_LOST_AND_FOUND, "Lost&Found"},
    {meshtastic_Config_DeviceConfig_Role_TAK_TRACKER, "TAK Tracker"},
    {meshtastic_Config_DeviceConfig_Role_ROUTER_LATE, "Router Late"},
};
static const uint8_t kSetupRoleOptionCount = sizeof(kSetupRoleOptions) / sizeof(kSetupRoleOptions[0]);

static const uint8_t kSetupHopOptions[] = {1, 2, 3, 4, 5, 6, 7};
static const uint8_t kSetupHopOptionCount = sizeof(kSetupHopOptions) / sizeof(kSetupHopOptions[0]);
static const char *kSetupHopLabels[] = {"1", "2", "3", "4", "5", "6", "7"};

static const uint32_t kSetupGpsUpdateOptions[] = {30, 60, 120, 300, 600, 1800};
static const uint8_t kSetupGpsUpdateCount = sizeof(kSetupGpsUpdateOptions) / sizeof(kSetupGpsUpdateOptions[0]);
static const uint32_t kSetupGpsBroadcastOptions[] = {60, 300, 600, 900, 1800, 3600};
static const uint8_t kSetupGpsBroadcastCount = sizeof(kSetupGpsBroadcastOptions) / sizeof(kSetupGpsBroadcastOptions[0]);
static const char *kSetupGpsUpdateLabels[] = {u8"30秒", u8"60秒", u8"2分鐘", u8"5分鐘", u8"10分鐘", u8"30分鐘"};
static const char *kSetupGpsBroadcastLabels[] = {u8"1分鐘", u8"5分鐘", u8"10分鐘", u8"15分鐘", u8"30分鐘", u8"60分鐘"};

struct SetupBrightnessOption {
    uint8_t value;
    const char *label;
};
static const SetupBrightnessOption kSetupBrightnessOptions[] = {
    {0, u8"關閉"},
    {30, u8"低"},
    {60, u8"中"},
    {120, u8"高"},
    {200, u8"最大"},
};
static const uint8_t kSetupBrightnessCount = sizeof(kSetupBrightnessOptions) / sizeof(kSetupBrightnessOptions[0]);

static const char *kSetupKeyRows[][10] = {
    {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
    {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},
    {"A", "S", "D", "F", "G", "H", "J", "K", "L", nullptr},
    {"Z", "X", "C", "V", "B", "N", "M", "DEL", "OK", nullptr},
};
static const uint8_t kSetupKeyRowLengths[] = {10, 10, 9, 9};
static const uint8_t kSetupKeyRowCount = sizeof(kSetupKeyRowLengths) / sizeof(kSetupKeyRowLengths[0]);

static const char *getSetupRoleLabel(meshtastic_Config_DeviceConfig_Role role)
{
    for (uint8_t i = 0; i < kSetupRoleOptionCount; ++i) {
        if (kSetupRoleOptions[i].role == role) {
            return kSetupRoleOptions[i].label;
        }
    }
    return u8"未知";
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
    uint8_t uiLedBrightness = 60;
    uint8_t screenBrightness = BRIGHTNESS_DEFAULT;
    meshtastic_Config_PositionConfig_GpsMode gpsMode = meshtastic_Config_PositionConfig_GpsMode_NOT_PRESENT;
    bool bluetoothEnabled = false;
    bool loraTxEnabled = true;
};
static StealthRuntimeState gStealthRuntimeState;

static bool isStealthModeActive()
{
    return gStealthRuntimeState.active;
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
        if (HermesXInterfaceModule::instance->isEmergencyLampEnabled()) {
            HermesXInterfaceModule::instance->setEmergencyLampEnabled(false);
            changed = true;
        }
        HermesXInterfaceModule::instance->setUiLedBrightness(0);
        HermesXInterfaceModule::instance->stopEmergencySiren();
        changed = true;
    }
    if (hermesXEmUiModule) {
        gStealthRuntimeState.sirenEnabled = hermesXEmUiModule->isSirenEnabled();
        if (hermesXEmUiModule->isSirenEnabled()) {
            changed = true;
        }
        hermesXEmUiModule->setSirenEnabled(false);
    }

    if (screen) {
        const uint8_t current = screen->getBrightnessLevel();
        const uint8_t dimmed = static_cast<uint8_t>((static_cast<uint16_t>(current) * 50U) / 100U);
        if (dimmed != current) {
            screen->setBrightnessLevel(dimmed);
            changed = true;
        }
    }

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
    if (config.bluetooth.enabled) {
        config.bluetooth.enabled = false;
        changed = true;
    }

    if (config.lora.tx_enabled) {
        config.lora.tx_enabled = false;
        changed = true;
    }
    if (rIf) {
        rIf->disable();
        changed = true;
    }
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

#if !MESHTASTIC_EXCLUDE_GPS
    if (gps && gStealthRuntimeState.gpsMode != meshtastic_Config_PositionConfig_GpsMode_DISABLED &&
        gStealthRuntimeState.gpsMode != meshtastic_Config_PositionConfig_GpsMode_NOT_PRESENT) {
        gps->enable();
    }
#endif
    config.position.gps_mode = gStealthRuntimeState.gpsMode;
    config.bluetooth.enabled = gStealthRuntimeState.bluetoothEnabled;
    config.lora.tx_enabled = gStealthRuntimeState.loraTxEnabled;
    if (rIf) {
        rIf->enable();
    }
    if (config.bluetooth.enabled) {
        setBluetoothEnable(true);
    }

    if (needsReboot) {
        *needsReboot = gStealthRuntimeState.needsRebootOnExit;
    }
    gStealthRuntimeState.active = false;
    gStealthRuntimeState.needsRebootOnExit = false;
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
                          uint8_t itemCount,
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

    const int16_t logoX = x + (width - HERMESX_WORD_WIDTH) / 2;
    const int16_t logoY = y + (height - HERMESX_WORD_HEIGHT) / 2;
    display->drawXbm(logoX, logoY, HERMESX_WORD_WIDTH, HERMESX_WORD_HEIGHT, hermesx_word_bits);

    const char *kHermesXBuildTag = "HXB_0.2.9";
    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    const int16_t tagWidth = display->getStringWidth(kHermesXBuildTag);
    const int16_t tagX = x + width - tagWidth - 2;
    const int16_t tagY = y + height - FONT_HEIGHT_SMALL - 2;
    display->drawString(tagX, tagY, kHermesXBuildTag);
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
        const String lampLine = String(u8"緊急照明燈: ") + (lampOn ? u8"開" : u8"關");
        const String stealthLine = String(u8"潛行模式: ") + (isStealthModeActive() ? u8"開" : u8"關");
        const char *items[kMainActionCount] = {u8"返回", lampLine.c_str(), stealthLine.c_str()};
        drawSetupList(display, width, height, u8"操作選單", items, kMainActionCount, hermesActionSelected, 0);
        graphics::HermesX_zh::drawMixedBounded(*display, 2, height - FONT_HEIGHT_SMALL, width - 4, u8"返回=下一頁",
                                               graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
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
        const char *header = (hermesSetupEditingSlot == 0) ? u8"設定密碼A" : u8"設定密碼B";
        drawSetupHeader(display, width, header);
#if defined(USE_EINK)
        display->setColor(EINK_BLACK);
#else
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
        display->setFont(FONT_SMALL);
        graphics::HermesX_zh::drawMixedBounded(*display, 2, kSetupHeaderHeight + 2, width - 4, hermesSetupPassDraft.c_str(),
                                               graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);

        const int16_t keyTop = (FONT_HEIGHT_SMALL * 2) + 6;
        const int16_t keyAreaHeight = height - keyTop;
        const int16_t rowHeight = (kSetupKeyRowCount > 0) ? keyAreaHeight / kSetupKeyRowCount : 0;
        display->setTextAlignment(TEXT_ALIGN_CENTER);

        for (uint8_t row = 0; row < kSetupKeyRowCount; ++row) {
            const int rowLen = kSetupKeyRowLengths[row];
            if (rowLen <= 0) {
                continue;
            }
            const int16_t cellWidth = width / rowLen;
            const int16_t rowY = keyTop + row * rowHeight;
            for (int col = 0; col < rowLen; ++col) {
                const int16_t cellX = col * cellWidth;
                const char *label = kSetupKeyRows[row][col];
                if (!label) {
                    continue;
                }
                const bool selected = (row == hermesSetupKeyRow && col == hermesSetupKeyCol);
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
        if (hermesSetupToastUntilMs != 0) {
            if (millis() > hermesSetupToastUntilMs) {
                hermesSetupToastUntilMs = 0;
                hermesSetupToast = "";
            } else if (hermesSetupToast.length() > 0) {
                display->setTextAlignment(TEXT_ALIGN_LEFT);
                graphics::HermesX_zh::drawMixedBounded(*display, 2, height - FONT_HEIGHT_SMALL, width - 4,
                                                       hermesSetupToast.c_str(), graphics::HermesX_zh::GLYPH_WIDTH,
                                                       FONT_HEIGHT_SMALL, nullptr);
            }
        }
        display->setTextAlignment(TEXT_ALIGN_LEFT);
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
        applyPaletteList(3);
        String label = u8"全域蜂鳴器: ";
        if (isBuzzerGloballyEnabled()) {
            label += u8"開";
        } else {
            label += u8"關";
        }
        const uint8_t ledBrightness =
            HermesXInterfaceModule::instance ? HermesXInterfaceModule::instance->getUiLedBrightness() : 60;
        String brightnessLine = String(u8"LED亮度: ") + getSetupBrightnessLabel(ledBrightness);
        const char *items[] = {u8"返回", label.c_str(), brightnessLine.c_str()};
        drawSetupList(display, width, height, u8"UI設定", items, 3, hermesSetupSelected, hermesSetupOffset);
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
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::UiBrightnessSelect) {
        applyPaletteList(kSetupBrightnessCount + 1);
        const char *items[kSetupBrightnessCount + 1] = {u8"返回"};
        for (uint8_t i = 0; i < kSetupBrightnessCount; ++i) {
            items[i + 1] = kSetupBrightnessOptions[i].label;
        }
        drawSetupList(display, width, height, u8"WS2812亮度調整", items, kSetupBrightnessCount + 1, hermesSetupSelected,
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
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::EmacMenu) {
        applyPaletteList(kSetupEmacCount);
        drawSetupList(display, width, height, u8"EMAC設定", kSetupEmacItems, kSetupEmacCount, hermesSetupSelected,
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
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::NodeMenu) {
        applyPaletteList(3);
        const char *items[] = {u8"返回", "Role", u8"GPS"};
        drawSetupList(display, width, height, u8"節點設定", items, 3, hermesSetupSelected, hermesSetupOffset);
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
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::RoleMenu) {
        applyPaletteList(3);
        const char *roleLabel = getSetupRoleLabel(config.device.role);
        String roleLine = String("Role: ") + roleLabel;
        const uint8_t hopLimit = Default::getConfiguredOrDefaultHopLimit(config.lora.hop_limit);
        String hopLine = String(u8"中繼跳數: ") + String(hopLimit);
        const char *items[] = {u8"返回", roleLine.c_str(), hopLine.c_str()};
        drawSetupList(display, width, height, "Role", items, 3, hermesSetupSelected, hermesSetupOffset);
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
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::RoleSelect) {
        const uint8_t itemCount = kSetupRoleOptionCount + 1;
        applyPaletteList(itemCount);
        const char *items[kSetupRoleOptionCount + 1];
        items[0] = u8"返回";
        for (uint8_t i = 0; i < kSetupRoleOptionCount; ++i) {
            items[i + 1] = kSetupRoleOptions[i].label;
        }
        drawSetupList(display, width, height, "Role", items, itemCount, hermesSetupSelected, hermesSetupOffset);
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
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::HopSelect) {
        const uint8_t itemCount = kSetupHopOptionCount + 1;
        applyPaletteList(itemCount);
        const char *items[kSetupHopOptionCount + 1];
        items[0] = u8"返回";
        for (uint8_t i = 0; i < kSetupHopOptionCount; ++i) {
            items[i + 1] = kSetupHopLabels[i];
        }
        drawSetupList(display, width, height, u8"中繼跳數", items, itemCount, hermesSetupSelected, hermesSetupOffset);
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
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::GpsMenu) {
        applyPaletteList(3);
        const uint32_t currentUpdate =
            Default::getConfiguredOrDefault(config.position.gps_update_interval, default_gps_update_interval);
        const uint32_t currentBroadcast =
            Default::getConfiguredOrDefault(config.position.position_broadcast_secs, default_broadcast_interval_secs);
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
        const char *items[] = {u8"返回", updateLine.c_str(), broadcastLine.c_str()};
        drawSetupList(display, width, height, u8"GPS", items, 3, hermesSetupSelected, hermesSetupOffset);
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
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::GpsUpdateSelect) {
        const uint8_t itemCount = kSetupGpsUpdateCount + 1;
        applyPaletteList(itemCount);
        const char *items[kSetupGpsUpdateCount + 1];
        items[0] = u8"返回";
        for (uint8_t i = 0; i < kSetupGpsUpdateCount; ++i) {
            items[i + 1] = kSetupGpsUpdateLabels[i];
        }
        drawSetupList(display, width, height, u8"衛星更新", items, itemCount, hermesSetupSelected, hermesSetupOffset);
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
        return;
    }

    if (hermesSetupPage == HermesFastSetupPage::GpsBroadcastSelect) {
        const uint8_t itemCount = kSetupGpsBroadcastCount + 1;
        applyPaletteList(itemCount);
        const char *items[kSetupGpsBroadcastCount + 1];
        items[0] = u8"返回";
        for (uint8_t i = 0; i < kSetupGpsBroadcastCount; ++i) {
            items[i + 1] = kSetupGpsBroadcastLabels[i];
        }
        drawSetupList(display, width, height, u8"廣播時間", items, itemCount, hermesSetupSelected, hermesSetupOffset);
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

    if (on != screenOn) {
        if (on) {
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
            }
#endif
#ifdef USE_ST7789
            pinMode(VTFT_CTRL, OUTPUT);
            digitalWrite(VTFT_CTRL, LOW);
            ui->init();
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
#endif
            enabled = true;
            setInterval(0); // Draw ASAP
            runASAP = true;
            // 重新開啟後強制更新畫面，避免黑屏
            forceDisplay(true);
        } else {
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
        setFrames(FOCUS_PRESERVE);
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

#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
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
    tft->setColorPaletteDefaults(isStealthModeActive() ? stealthFg : normalFg, normalBg);
    if (isStealthModeActive()) {
        tft->clearColorPaletteZones();
    }
#endif

    // this must be before the frameState == FIXED check, because we always
    // want to draw at least one FIXED frame before doing forceDisplay
    ui->update();

    if (showingNormalScreen && hasUnreadTextMessage && devicestate.has_rx_text_message &&
        shouldDrawMessage(&devicestate.rx_text_message) && framesetInfo.positions.textMessage < framesetInfo.frameCount &&
        ui->getUiState()->currentFrame == framesetInfo.positions.textMessage) {
        hasUnreadTextMessage = false;
        syncTextMessageNotification();
    }

    // Switch to a low framerate (to save CPU) when we are not in transition
    // but we should only call setTargetFPS when framestate changes, because
    // otherwise that breaks animations.
    if (targetFramerate != IDLE_FRAMERATE && ui->getUiState()->frameState == FIXED && !hermesXBootHoldActive) {
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
    startAlert([text](OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) -> void {
        (void)state;
        const int lineHeight = HermesX_zh::GLYPH_HEIGHT + 1;
        const int width = HermesX_zh::stringAdvance(text, HermesX_zh::GLYPH_WIDTH, display);
        const int16_t drawX = x + (display->width() - width) / 2;
        const int16_t drawY = y + (display->height() - lineHeight) / 2;
        HermesX_zh::drawMixed(*display, drawX, drawY, text, HermesX_zh::GLYPH_WIDTH, lineHeight, nullptr);
    });
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

    // If we have a text message - show it next, unless it's a phone message and we aren't using any special modules
    if (devicestate.has_rx_text_message && shouldDrawMessage(&devicestate.rx_text_message)) {
        fsi.positions.textMessage = numframes;
        normalFrames[numframes++] = drawTextMessageFrame;
    }

    // Main screen (HermesX logo)
    fsi.positions.main = numframes;
    normalFrames[numframes++] = &Screen::drawHermesXMainFrame;

    // Main action menu (standalone page)
    fsi.positions.mainAction = numframes;
    normalFrames[numframes++] = &Screen::drawHermesXActionFrame;

    // HermesFastSetup replaces node info frames
    fsi.positions.setup = numframes;
    normalFrames[numframes++] = &Screen::drawHermesFastSetupFrame;

    // Share channels (QR)
    fsi.positions.share = numframes;
    normalFrames[numframes++] = &Screen::drawHermesXShareChannelFrame;

    // then the debug info
    //
    // Since frames are basic function pointers, we have to use a helper to
    // call a method on debugInfo object.
    fsi.positions.log = numframes;
    normalFrames[numframes++] = &Screen::drawDebugInfoTrampoline;

    // call a method on debugInfoScreen object (for more details)
    fsi.positions.settings = numframes;
    normalFrames[numframes++] = &Screen::drawDebugInfoSettingsTrampoline;

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
    ui->enableAllIndicators();

    // Add function overlay here. This can show when notifications muted, modifier key is active etc
    static OverlayCallback functionOverlay[] = {drawHermesXEmUiOverlay, drawFunctionOverlay};
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
        ui->switchToFrame(fsi.positions.focusedModule);
        break;

    case FOCUS_PRESERVE:
        // If we can identify which type of frame "originalPosition" was, can move directly to it in the new frameset
        const FramesetInfo &oldFsi = this->framesetInfo;
        if (originalPosition == oldFsi.positions.log)
            ui->switchToFrame(fsi.positions.log);
        else if (originalPosition == oldFsi.positions.settings)
            ui->switchToFrame(fsi.positions.settings);
        else if (originalPosition == oldFsi.positions.wifi)
            ui->switchToFrame(fsi.positions.wifi);
        else if (originalPosition == oldFsi.positions.mainAction)
            ui->switchToFrame(fsi.positions.mainAction);

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

    if (!devicestate.has_rx_text_message) {
        hasUnreadTextMessage = false;
        return;
    }

    const bool hasVisibleTextFrame =
        shouldDrawMessage(&devicestate.rx_text_message) && framesetInfo.positions.textMessage < framesetInfo.frameCount;
    if (!hasVisibleTextFrame || !hasUnreadTextMessage) {
        return;
    }

    const uint8_t textFrame = framesetInfo.positions.textMessage;
    const bool isCurrentTextFrame =
        showingNormalScreen && ui->getUiState() && (ui->getUiState()->currentFrame == textFrame);
    if (isCurrentTextFrame) {
        hasUnreadTextMessage = false;
        return;
    }

    if (ui->addFrameToNotifications(textFrame, true)) {
        notifyingTextMessageFrame = textFrame;
    }
}

// Dismisses the currently displayed screen frame, if possible
// Relevant for text message, waypoint, others in future?
// Triggered with a CardKB keycombo
void Screen::dismissCurrentFrame()
{
    uint8_t currentFrame = ui->getUiState()->currentFrame;
    bool dismissed = false;

    if (currentFrame == framesetInfo.positions.textMessage && devicestate.has_rx_text_message) {
        LOG_INFO("Dismiss Text Message");
        devicestate.has_rx_text_message = false;
        hasUnreadTextMessage = false;
        dismissed = true;
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
    // FastSetup page locks frame switching unless Exit is pressed.
    if (isHermesFastSetupActive() || isHermesXActionPageActive()) {
        return;
    }
    // If Canned Messages is using the "Scan and Select" input, dismiss the canned message frame when user button is pressed
    // Minimize impact as a courtesy, as "scan and select" may be used as default config for some boards
    if (scanAndSelectInput != nullptr && scanAndSelectInput->dismissCannedMessageFrame())
        return;

    // If screen was off, just wake it, otherwise advance to next frame
    // If we are in a transition, the press must have bounced, drop it.
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
    if (display->getWidth() >= 240 && display->getHeight() >= 160) {
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
    case STATUS_TYPE_NODE:
        if (showingNormalScreen && nodeStatus->getLastNumTotal() != nodeStatus->getNumTotal()) {
            setFrames(FOCUS_PRESERVE); // Regen the list of screen frames (returning to same frame, if possible)
        }
        nodeDB->updateGUI = false;
        break;
    }

    return 0;
}

int Screen::handleTextMessage(const meshtastic_MeshPacket *packet)
{
    if (showingNormalScreen) {
        // Outgoing message
        if (packet->from == 0) {
            setFrames(FOCUS_PRESERVE); // Return to same frame (quietly hiding the rx text message frame)
        } else {
            const bool onTextFrameNow =
                ui && ui->getUiState() && ui->getUiState()->currentFrame == framesetInfo.positions.textMessage;
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
        // Regenerate the frameset, potentially honoring a module's internal requestFocus() call
        if (event->action == UIFrameEvent::Action::REGENERATE_FRAMESET)
            setFrames(FOCUS_MODULE);

        // Regenerate the frameset, while Attempt to maintain focus on the current frame
        else if (event->action == UIFrameEvent::Action::REGENERATE_FRAMESET_BACKGROUND)
            setFrames(FOCUS_PRESERVE);

        // Don't regenerate the frameset, just re-draw whatever is on screen ASAP
        else if (event->action == UIFrameEvent::Action::REDRAW_ONLY)
            setFastFramerate();
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
        if (isUp || isLeft || isCcw) {
            navDir = -1;
        } else if (isDown || isRight || isCw) {
            navDir = 1;
        }
    }
    if (navDir == 0 && isCancel && !hermesActionStealthConfirmVisible) {
        navDir = 1;
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
        hermesActionSelected = 0;
        hermesActionStealthConfirmVisible = false;
        hermesActionStealthConfirmSelected = 0;
        hermesActionStealthConfirmShownAtMs = 0;
        showNextFrame();
    } else if (hermesActionSelected == 1) {
        if (HermesXInterfaceModule::instance) {
            const bool next = !HermesXInterfaceModule::instance->isEmergencyLampEnabled();
            HermesXInterfaceModule::instance->setEmergencyLampEnabled(next);
        }
    } else if (hermesActionSelected == 2) {
        bool needsReboot = false;
        if (!isStealthModeActive()) {
            hermesActionStealthConfirmVisible = true;
            hermesActionStealthConfirmSelected = 0;
            hermesActionStealthConfirmShownAtMs = millis();
        } else if (disableStealthMode(&needsReboot)) {
            if (screen) {
                screen->print(needsReboot ? "Stealth OFF, rebooting...\n" : "Stealth OFF\n");
            }
            if (needsReboot) {
                rebootAtMsec = millis() + 1500;
            }
        }
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
        hermesSetupSelected = selected;
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
            // Treat cancel as "next page" at top-level.
            resetMenu(HermesFastSetupPage::Entry);
            showNextFrame();
            return true;
        }
        if (handleMenuNav(kSetupRootCount)) {
            LOG_INFO("[HermesFastSetup] select=%d item=%s", hermesSetupSelected, kSetupRootItems[hermesSetupSelected]);
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::Entry);
                showNextFrame();
            } else if (hermesSetupSelected == 1) {
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
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::NodeMenu) {
        if (handleMenuNav(3)) {
            LOG_INFO("[HermesFastSetup] select=%d item=%s", hermesSetupSelected,
                     (hermesSetupSelected == 0) ? "返回" : (hermesSetupSelected == 1 ? "Role" : "GPS"));
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::Root);
            } else if (hermesSetupSelected == 1) {
                resetMenu(HermesFastSetupPage::RoleMenu);
            } else {
                resetMenu(HermesFastSetupPage::GpsMenu);
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
        if (handleMenuNav(3)) {
            LOG_INFO("[HermesFastSetup] select=%d item=%s", hermesSetupSelected,
                     (hermesSetupSelected == 0) ? "返回" : (hermesSetupSelected == 1 ? "全域蜂鳴器" : "WS2812亮度調整"));
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
                        HermesXInterfaceModule::instance->setUiLedBrightness(kSetupBrightnessOptions[idx].value);
                    }
                    hermesSetupToast = String(u8"LED亮度: ") + kSetupBrightnessOptions[idx].label;
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

    if (hermesSetupPage == HermesFastSetupPage::RoleMenu) {
        if (handleMenuNav(3)) {
            LOG_INFO("[HermesFastSetup] select=%d item=%s", hermesSetupSelected,
                     (hermesSetupSelected == 0) ? "返回" : (hermesSetupSelected == 1 ? "裝置模式" : "中繼跳數"));
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
                enterMenu(HermesFastSetupPage::RoleSelect, kSetupRoleOptionCount + 1, selected);
            } else {
                const uint8_t currentHop = Default::getConfiguredOrDefaultHopLimit(config.lora.hop_limit);
                int selected = 1;
                for (uint8_t i = 0; i < kSetupHopOptionCount; ++i) {
                    if (kSetupHopOptions[i] == currentHop) {
                        selected = i + 1;
                        break;
                    }
                }
                enterMenu(HermesFastSetupPage::HopSelect, kSetupHopOptionCount + 1, selected);
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

    if (hermesSetupPage == HermesFastSetupPage::RoleSelect) {
        const int count = kSetupRoleOptionCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::RoleMenu);
            } else {
                const uint8_t index = hermesSetupSelected - 1;
                if (index < kSetupRoleOptionCount) {
                    config.device.role = kSetupRoleOptions[index].role;
                    nodeDB->saveToDisk(SEGMENT_CONFIG);
                    hermesSetupToast = String(u8"角色已設定: ") + kSetupRoleOptions[index].label;
                    hermesSetupToastUntilMs = millis() + 1500;
                }
                resetMenu(HermesFastSetupPage::RoleMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::RoleMenu);
            setFastFramerate();
            return true;
        }
        return false;
    }

    if (hermesSetupPage == HermesFastSetupPage::HopSelect) {
        const int count = kSetupHopOptionCount + 1;
        if (handleMenuNav(count)) {
            setFastFramerate();
            return true;
        }
        if (isSelect || isPress) {
            if (hermesSetupSelected == 0) {
                resetMenu(HermesFastSetupPage::RoleMenu);
            } else {
                const uint8_t index = hermesSetupSelected - 1;
                if (index < kSetupHopOptionCount) {
                    config.lora.hop_limit = kSetupHopOptions[index];
                    nodeDB->saveToDisk(SEGMENT_CONFIG);
                    hermesSetupToast = String(u8"跳數已設定: ") + String(kSetupHopOptions[index]);
                    hermesSetupToastUntilMs = millis() + 1500;
                }
                resetMenu(HermesFastSetupPage::RoleMenu);
            }
            setFastFramerate();
            return true;
        }
        if (isCancel) {
            resetMenu(HermesFastSetupPage::RoleMenu);
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
        if (handleMenuNav(3)) {
            LOG_INFO("[HermesFastSetup] select=%d item=%s", hermesSetupSelected,
                     (hermesSetupSelected == 0) ? "返回" : (hermesSetupSelected == 1 ? "衛星更新" : "廣播時間"));
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
            } else {
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
                    nodeDB->saveToDisk(SEGMENT_CONFIG);
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
                    nodeDB->saveToDisk(SEGMENT_CONFIG);
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

    return false;
}

int Screen::handleInputEvent(const InputEvent *event)
{
    if (!event) {
        return 0;
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
        if (this->ui->getUiState()->currentFrame == framesetInfo.positions.mainAction) {
            if (handleHermesXActionInput(event)) {
                return 0;
            }
            // Action page handles navigation internally; keep frame switching locked.
            return 0;
        }

        if (this->ui->getUiState()->currentFrame == framesetInfo.positions.setup) {
            if (handleHermesFastSetupInput(event)) {
                return 0;
            }
            // FastSetup page locks frame switching unless Exit is pressed.
            return 0;
        }

        // Ask any MeshModules if they're handling keyboard input right now
        bool inputIntercepted = false;
        for (MeshModule *module : moduleFrames) {
            if (module->interceptingKeyboardInput())
                inputIntercepted = true;
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

bool Screen::isHermesFastSetupActive() const
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    return ui->getUiState()->currentFrame == framesetInfo.positions.setup;
}

bool Screen::isHermesXActionPageActive() const
{
    if (!showingNormalScreen || !ui) {
        return false;
    }
    return ui->getUiState()->currentFrame == framesetInfo.positions.mainAction;
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
