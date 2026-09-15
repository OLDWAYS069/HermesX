#pragma once

#include <OLEDDisplay.h>
#include <cstddef>
#include <cstdint>

class TFTDisplay;

namespace graphics
{

struct HermesXHomeStatusView {
    HermesXHomeStatusView(const char *date,
                          const char *role,
                          bool hasBattery,
                          uint8_t batteryPercent,
                          uint8_t satelliteCount)
        : date(date), role(role), hasBattery(hasBattery), batteryPercent(batteryPercent), satelliteCount(satelliteCount)
    {
    }

    const char *date;
    const char *role;
    bool hasBattery;
    uint8_t batteryPercent;
    uint8_t satelliteCount;
};

struct HermesXHomeNeonClockLayout {
    int16_t regionX = 0;
    int16_t regionY = 0;
    int16_t regionWidth = 0;
    int16_t regionHeight = 0;
    int16_t frameX = 0;
};

enum class HermesXParcelAnimationMode : uint8_t {
    Outgoing,
    Incoming,
    Failed,
};

class HermesXHomeUiRenderer
{
  public:
    static constexpr int16_t NeonClockGlowRadiusOuter = 5;
    static constexpr int16_t NeonClockMargin = NeonClockGlowRadiusOuter + 1;
    static constexpr int16_t NeonClockGlyphMaxWidth = 21;
    static constexpr int16_t NeonClockGlyphHeight = 25;
    static constexpr size_t NeonClockSlotCount = 8;
    static constexpr int16_t NeonClockMaxRegionWidth =
        (NeonClockGlyphMaxWidth * 6) + 14 + (NeonClockMargin * 2);
    static constexpr int16_t NeonClockMaxRegionHeight = NeonClockGlyphHeight + (NeonClockMargin * 2);

    static void resetQuote();
    static void startQuote();
    static void drawQuote(OLEDDisplay *display, int16_t x, int16_t y, int16_t width, int16_t height);
    static void drawDog(OLEDDisplay *display,
                        int16_t width,
                        int16_t timeY,
                        bool compactLayout,
                        uint32_t nowMs);
    static void drawMonochromeSittingDog(
        OLEDDisplay *display,
        int16_t x,
        int16_t y,
        uint8_t scale = 1,
        uint8_t frameIndex = 0,
        bool outlineOnly = false,
        bool mirror = false);
    static void drawHermesOneThoughtBubble(
        OLEDDisplay *display, int16_t x, int16_t y, const char *symbol, bool stomachIcon = false);
    static void drawHermesOneTombstone(OLEDDisplay *display, int16_t x, int16_t y);
    static void drawHermesOnePetDialog(
        OLEDDisplay *display, int16_t width, int16_t height, bool revive, uint8_t selectedAction);
    static void drawSittingDogNotification(OLEDDisplay *display,
                                           int16_t width,
                                           int16_t height,
                                           const char *message,
                                           uint32_t nowMs,
                                           bool showActions = false,
                                           uint8_t selectedAction = 0);
    static void drawHermesOneParcelNotification(OLEDDisplay *display,
                                                 int16_t width,
                                                 int16_t height,
                                                 HermesXParcelAnimationMode mode,
                                                 uint32_t elapsedMs,
                                                 bool showActions = false,
                                                 uint8_t selectedAction = 0,
                                                 const char *message = nullptr);
    static void renderDirectSittingDogNotification(TFTDisplay *display,
                                                   int16_t width,
                                                   int16_t height,
                                                   uint32_t nowMs);
    static void drawHorizontalBattery(
        OLEDDisplay *display, int16_t x, int16_t y, int16_t width, int16_t height, uint8_t percent);
    static void drawStatus(OLEDDisplay *display, const HermesXHomeStatusView &view);
    static int16_t neonClockSlotWidth(size_t slotIndex);
    static int16_t neonClockFrameWidth();
    static bool makeNeonClockLayout(int16_t displayWidth, int16_t originY, HermesXHomeNeonClockLayout &layout);
    static uint16_t neonClockLayerColor(uint8_t value);
};

} // namespace graphics
