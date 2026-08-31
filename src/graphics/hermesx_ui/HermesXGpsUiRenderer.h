#pragma once

#include <OLEDDisplay.h>
#include <cstddef>
#include <cstdint>

class TFTDisplay;

namespace graphics
{

using HermesXGpsColorZoneDrawer = void (*)(OLEDDisplay *display,
                                           int16_t x,
                                           int16_t y,
                                           int16_t width,
                                           int16_t height,
                                           uint16_t foreground,
                                           uint16_t background);
using HermesXGpsLayerColorMapper = uint16_t (*)(uint8_t value);
using HermesXGpsTextMeasure = int16_t (*)(const char *text, bool halfScale, int16_t tracking);
using HermesXGpsTextDrawer = bool (*)(TFTDisplay *display,
                                      int16_t x,
                                      int16_t y,
                                      const char *text,
                                      bool halfScale,
                                      HermesXGpsLayerColorMapper colorMapper,
                                      bool clearBackground,
                                      int16_t tracking,
                                      int16_t marginOverride);
using HermesXGpsCircleDrawer = void (*)(TFTDisplay *display,
                                        int16_t displayWidth,
                                        int16_t displayHeight,
                                        int16_t centerX,
                                        int16_t centerY,
                                        int16_t radius,
                                        uint16_t color);
using HermesXGpsLineDrawer = void (*)(TFTDisplay *display,
                                      int16_t displayWidth,
                                      int16_t displayHeight,
                                      int16_t x0,
                                      int16_t y0,
                                      int16_t x1,
                                      int16_t y1,
                                      uint16_t color);
using HermesXGpsNeonLineDrawer = void (*)(TFTDisplay *display,
                                          int16_t displayWidth,
                                          int16_t displayHeight,
                                          int16_t x0,
                                          int16_t y0,
                                          int16_t x1,
                                          int16_t y1,
                                          uint16_t coreColor,
                                          uint16_t glowNearColor,
                                          uint16_t glowFarColor,
                                          int16_t coreThickness,
                                          int16_t glowNearThickness,
                                          int16_t glowFarThickness);

struct HermesXGpsPosterLayout
{
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

struct HermesXGpsPosterTitleLayout
{
    bool valid = false;
    int16_t scale = 0;
    int16_t spacing = 0;
    int16_t gpsX = 0;
    int16_t stateX = 0;
    int16_t y = 0;
};

struct HermesXGpsPosterCoordinateView
{
    bool hasCoordinates = false;
    double longitude = 0.0;
    double latitude = 0.0;
    bool hasAltitude = false;
    int32_t altitudeMeters = 0;
};

struct HermesXGpsPosterDecorView
{
    uint8_t satelliteCount = 0;
    int16_t satelliteTextHeight = 0;
    int16_t neonTextMargin = 0;
};

struct HermesXGpsPosterDecorCallbacks
{
    HermesXGpsCircleDrawer drawCircle = nullptr;
    HermesXGpsLineDrawer drawLine = nullptr;
    HermesXGpsNeonLineDrawer drawNeonLine = nullptr;
    HermesXGpsTextMeasure measureText = nullptr;
    HermesXGpsTextDrawer drawText = nullptr;
    HermesXGpsLayerColorMapper warmColorMapper = nullptr;
};

struct HermesXGpsStatusView
{
    HermesXGpsStatusView(uint32_t satelliteCount,
                         const char *title,
                         const char *lockStatus,
                         const char *lockStatusShort,
                         const char *longitude,
                         const char *latitude,
                         const char *date,
                         const char *time,
                         const char *compactTime)
        : satelliteCount(satelliteCount), title(title), lockStatus(lockStatus), lockStatusShort(lockStatusShort),
          longitude(longitude), latitude(latitude), date(date), time(time), compactTime(compactTime)
    {
    }

    uint32_t satelliteCount;
    const char *title;
    const char *lockStatus;
    const char *lockStatusShort;
    const char *longitude;
    const char *latitude;
    const char *date;
    const char *time;
    const char *compactTime;
};

class HermesXGpsUiRenderer
{
  public:
    static HermesXGpsPosterLayout makePosterLayout(int16_t width, int16_t height, int16_t altitudeVisualHeight);
    static HermesXGpsPosterTitleLayout makePosterTitleLayout(
        int16_t width, const HermesXGpsPosterLayout &posterLayout, const char *stateWord);
    static int16_t measurePosterTitleText(const char *text, int16_t scale, int16_t spacing);
    static void stampPosterTitleMask(uint8_t *mask,
                                     int16_t maskWidth,
                                     int16_t maskHeight,
                                     int16_t x,
                                     int16_t y,
                                     const char *text,
                                     int16_t scale,
                                     int16_t spacing);
    static bool drawPosterTitleNeon(TFTDisplay *display,
                                    int16_t width,
                                    int16_t height,
                                    const HermesXGpsPosterLayout &posterLayout,
                                    const char *stateWord,
                                    uint8_t *fullMask,
                                    uint8_t *layerMap,
                                    size_t bufferCapacity,
                                    HermesXGpsLayerColorMapper gpsColorMapper,
                                    HermesXGpsLayerColorMapper stateColorMapper);
    static bool drawPosterCoordinates(TFTDisplay *display,
                                      const HermesXGpsPosterLayout &layout,
                                      const HermesXGpsPosterCoordinateView &view,
                                      HermesXGpsTextMeasure measureText,
                                      HermesXGpsTextDrawer drawText,
                                      HermesXGpsLayerColorMapper colorMapper);
    static bool drawPosterDecor(TFTDisplay *display,
                                int16_t width,
                                int16_t height,
                                const HermesXGpsPosterLayout &layout,
                                const HermesXGpsPosterDecorView &view,
                                const HermesXGpsPosterDecorCallbacks &callbacks);
    static void drawStatusFrame(OLEDDisplay *display, int16_t x, int16_t y, const HermesXGpsStatusView &view);
    static void drawPosterBase(OLEDDisplay *display,
                               int16_t x,
                               int16_t y,
                               int16_t width,
                               int16_t height,
                               bool gpsEnabled,
                               HermesXGpsColorZoneDrawer drawColorZone);
    static void drawSatelliteIcon(OLEDDisplay *display, int16_t x, int16_t y, int16_t size);
    static void drawPosterSatelliteMark(OLEDDisplay *display, int16_t centerX, int16_t centerY, int16_t size);
    static void drawFallbackCornerMapOutline(
        OLEDDisplay *display, int16_t x, int16_t y, int16_t width, int16_t height);

  private:
    static void drawPosterBackdrop(
        OLEDDisplay *display, int16_t x, int16_t y, HermesXGpsColorZoneDrawer drawColorZone);
    static void drawPosterHeader(OLEDDisplay *display,
                                 int16_t x,
                                 int16_t y,
                                 int16_t width,
                                 int16_t height,
                                 bool gpsEnabled,
                                 HermesXGpsColorZoneDrawer drawColorZone);
    static void drawNeonAsciiText(OLEDDisplay *display,
                                  int16_t x,
                                  int16_t y,
                                  const char *text,
                                  int16_t textHeight,
                                  uint16_t glowOuterColor,
                                  uint16_t glowInnerColor,
                                  uint16_t coreColor,
                                  HermesXGpsColorZoneDrawer drawColorZone);
};

} // namespace graphics
