#include "HermesXGpsUiRenderer.h"

#include "graphics/ScreenFonts.h"
#include "graphics/TFTDisplay.h"
#include "graphics/fonts/HermesX_zh/HermesX_CN12.h"
#include "graphics/hermesx_ui/HermesXNeonMask.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace graphics
{
namespace
{

void drawThickLine(OLEDDisplay *display,
                   int16_t x0,
                   int16_t y0,
                   int16_t x1,
                   int16_t y1,
                   int16_t thickness)
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

void drawMonoNeonStringMaxWidth(OLEDDisplay *display,
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

void drawMonoNeonMixedBounded(OLEDDisplay *display,
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

constexpr uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
    return static_cast<uint16_t>(((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3));
}

struct PosterTitleGlyphPattern
{
    char ch;
    const char *rows[7];
};

const PosterTitleGlyphPattern kPosterTitleGlyphs[] = {
    {'G', {" ### ", "#   #", "#    ", "# ###", "#   #", "#   #", " ### "}},
    {'P', {"#### ", "#   #", "#   #", "#### ", "#    ", "#    ", "#    "}},
    {'S', {" ####", "#    ", "#    ", " ### ", "    #", "    #", "#### "}},
    {'O', {" ### ", "#   #", "#   #", "#   #", "#   #", "#   #", " ### "}},
    {'N', {"#   #", "##  #", "# # #", "#  ##", "#   #", "#   #", "#   #"}},
    {'F', {"#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#    "}},
};

const PosterTitleGlyphPattern *findPosterTitleGlyph(char ch)
{
    for (const auto &glyph : kPosterTitleGlyphs) {
        if (glyph.ch == ch) {
            return &glyph;
        }
    }
    return nullptr;
}

void drawPosterGlobeBloom(TFTDisplay *display,
                          int16_t width,
                          int16_t height,
                          const HermesXGpsPosterLayout &layout,
                          HermesXGpsCircleDrawer drawCircle)
{
    if (!display || !drawCircle || width <= 0 || height <= 0 || layout.globeR <= 6) {
        return;
    }

    static constexpr int kBloomLayers = 40;
    static constexpr int16_t kBloomInnerPercent = 72;
    static constexpr int16_t kBloomOuterPercent = 1;
    const int16_t innerExtra = layout.tiny ? 5 : 8;
    const int16_t extraStep = layout.tiny ? 1 : 2;
    for (int layer = kBloomLayers - 1; layer >= 0; --layer) {
        const int16_t radius = layout.globeR + innerExtra + (layer * extraStep);
        const int16_t numerator =
            (kBloomInnerPercent - kBloomOuterPercent) * (kBloomLayers - 1 - layer);
        const int16_t percent = static_cast<int16_t>(
            kBloomOuterPercent + ((numerator + ((kBloomLayers - 1) / 2)) / (kBloomLayers - 1)));
        const uint8_t value = static_cast<uint8_t>((255U * percent) / 100U);
        drawCircle(display,
                   width,
                   height,
                   layout.globeCx,
                   layout.globeCy,
                   radius,
                   TFTDisplay::rgb565(value, value, value));
    }
}

void drawPosterWireGlobe(TFTDisplay *display,
                         int16_t width,
                         int16_t height,
                         const HermesXGpsPosterLayout &layout,
                         HermesXGpsLineDrawer drawLine)
{
    if (!display || !drawLine || width <= 0 || height <= 0 || layout.globeR <= 6) {
        return;
    }

    static constexpr float kPi = 3.14159265358979323846f;
    static constexpr int kMaxLonSegments = 14;
    static constexpr int kMaxLatSegments = 8;
    const int lonSegments = layout.tiny ? 10 : kMaxLonSegments;
    const int latSegments = layout.tiny ? 6 : kMaxLatSegments;
    struct Point3D {
        int16_t x = 0;
        int16_t y = 0;
        float z = 0.0f;
    };
    static Point3D points[kMaxLatSegments + 1][kMaxLonSegments];

    const float rotationY = -0.55f;
    const float tiltX = 0.45f;
    const float cosY = cosf(rotationY);
    const float sinY = sinf(rotationY);
    const float cosX = cosf(tiltX);
    const float sinX = sinf(tiltX);
    const uint16_t frontColor = TFTDisplay::rgb565(0xFF, 0xFF, 0xFF);
    const uint16_t backColor = TFTDisplay::rgb565(0xD8, 0xDE, 0xE8);

    for (int latIndex = 0; latIndex <= latSegments; ++latIndex) {
        const float latitude = (-0.5f * kPi) +
                               (static_cast<float>(latIndex) / static_cast<float>(latSegments) * kPi);
        const float cosLatitude = cosf(latitude);
        const float sinLatitude = sinf(latitude);
        for (int lonIndex = 0; lonIndex < lonSegments; ++lonIndex) {
            const float longitude =
                static_cast<float>(lonIndex) / static_cast<float>(lonSegments) * 2.0f * kPi;
            const float x = cosLatitude * cosf(longitude);
            const float y = sinLatitude;
            const float z = cosLatitude * sinf(longitude);
            const float rotatedX = (x * cosY) + (z * sinY);
            const float rotatedZ = (-x * sinY) + (z * cosY);
            const float rotatedY = (y * cosX) - (rotatedZ * sinX);
            const float depth = (y * sinX) + (rotatedZ * cosX);
            points[latIndex][lonIndex].x =
                layout.globeCx + static_cast<int16_t>(lrintf(rotatedX * layout.globeR));
            points[latIndex][lonIndex].y =
                layout.globeCy + static_cast<int16_t>(lrintf(rotatedY * layout.globeR));
            points[latIndex][lonIndex].z = depth;
        }
        yield();
    }

    const auto drawEdge = [&](const Point3D &a, const Point3D &b) {
        const uint16_t color = ((a.z + b.z) * 0.5f < -0.12f) ? backColor : frontColor;
        drawLine(display, width, height, a.x, a.y, b.x, b.y, color);
    };
    for (int latIndex = 0; latIndex <= latSegments; ++latIndex) {
        for (int lonIndex = 0; lonIndex < lonSegments; ++lonIndex) {
            const int nextLongitude = (lonIndex + 1) % lonSegments;
            drawEdge(points[latIndex][lonIndex], points[latIndex][nextLongitude]);
            if (latIndex < latSegments) {
                drawEdge(points[latIndex][lonIndex], points[latIndex + 1][lonIndex]);
                drawEdge(points[latIndex][lonIndex], points[latIndex + 1][nextLongitude]);
            }
        }
        yield();
    }

    for (int latIndex = 0; latIndex <= latSegments; ++latIndex) {
        for (int lonIndex = 0; lonIndex < lonSegments; ++lonIndex) {
            const Point3D &point = points[latIndex][lonIndex];
            if (point.x >= -2 && point.x <= (width + 2) && point.y >= -2 && point.y <= (height + 2)) {
                display->drawPixel565(point.x, point.y, frontColor);
            }
        }
        yield();
    }
}

} // namespace

HermesXGpsPosterLayout HermesXGpsUiRenderer::makePosterLayout(
    int16_t width, int16_t height, int16_t altitudeVisualHeight)
{
    HermesXGpsPosterLayout layout;
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
    layout.mountainY = layout.altitudeY + ((altitudeVisualHeight - layout.mountainH) / 2) + (layout.tiny ? 1 : 2);
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

HermesXGpsPosterTitleLayout HermesXGpsUiRenderer::makePosterTitleLayout(
    int16_t width, const HermesXGpsPosterLayout &posterLayout, const char *stateWord)
{
    HermesXGpsPosterTitleLayout layout;
    if (width <= 0 || !stateWord || !*stateWord) {
        return layout;
    }

    static constexpr const char *kGpsWord = "GPS";
    layout.scale = posterLayout.tiny ? 2 : 3;
    layout.spacing = std::max<int16_t>(1, layout.scale / 2);
    const int16_t gpsWidth = measurePosterTitleText(kGpsWord, layout.scale, layout.spacing);
    const int16_t stateWidth = measurePosterTitleText(stateWord, layout.scale, layout.spacing);
    if (gpsWidth <= 0 || stateWidth <= 0) {
        return layout;
    }

    int16_t titleGap = width / 30;
    if (titleGap < (posterLayout.tiny ? 4 : 6)) {
        titleGap = posterLayout.tiny ? 4 : 6;
    }
    const int16_t titleWidth = gpsWidth + titleGap + stateWidth;
    const int16_t leftCenterX = posterLayout.leftX + (posterLayout.leftW / 2);
    layout.gpsX = leftCenterX - (titleWidth / 2);
    if (layout.gpsX < posterLayout.leftX) {
        layout.gpsX = posterLayout.leftX;
    }
    const int16_t maxTitleX =
        std::max<int16_t>(posterLayout.leftX, posterLayout.leftX + posterLayout.leftW - titleWidth);
    if (layout.gpsX > maxTitleX) {
        layout.gpsX = maxTitleX;
    }
    layout.stateX = layout.gpsX + gpsWidth + titleGap;
    layout.y = posterLayout.titleY;
    layout.valid = true;
    return layout;
}

int16_t HermesXGpsUiRenderer::measurePosterTitleText(const char *text, int16_t scale, int16_t spacing)
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
        const auto *glyph = findPosterTitleGlyph(*cursor);
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

void HermesXGpsUiRenderer::stampPosterTitleMask(uint8_t *mask,
                                                int16_t maskWidth,
                                                int16_t maskHeight,
                                                int16_t x,
                                                int16_t y,
                                                const char *text,
                                                int16_t scale,
                                                int16_t spacing)
{
    if (!mask || !text || maskWidth <= 0 || maskHeight <= 0 || scale <= 0) {
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
        const auto *glyph = findPosterTitleGlyph(ch);
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
                const int16_t pixelX = cursorX + (col * scale);
                const int16_t pixelY = y + (row * scale);
                for (int16_t scaleY = 0; scaleY < scale; ++scaleY) {
                    const int16_t targetY = pixelY + scaleY;
                    if (targetY < 0 || targetY >= maskHeight) {
                        continue;
                    }
                    const int16_t rowBase = targetY * maskWidth;
                    for (int16_t scaleX = 0; scaleX < scale; ++scaleX) {
                        const int16_t targetX = pixelX + scaleX;
                        if (targetX >= 0 && targetX < maskWidth) {
                            mask[rowBase + targetX] = 1;
                        }
                    }
                }
            }
        }

        cursorX += scale * 5;
        hasGlyph = true;
    }
}

bool HermesXGpsUiRenderer::drawPosterTitleNeon(TFTDisplay *display,
                                               int16_t width,
                                               int16_t height,
                                               const HermesXGpsPosterLayout &posterLayout,
                                               const char *stateWord,
                                               uint8_t *fullMask,
                                               uint8_t *layerMap,
                                               size_t bufferCapacity,
                                               HermesXGpsLayerColorMapper gpsColorMapper,
                                               HermesXGpsLayerColorMapper stateColorMapper)
{
    if (!display || width <= 0 || height <= 0 || !stateWord || !*stateWord || !fullMask || !layerMap ||
        !gpsColorMapper || !stateColorMapper) {
        return false;
    }

    const HermesXGpsPosterTitleLayout titleLayout = makePosterTitleLayout(width, posterLayout, stateWord);
    if (!titleLayout.valid) {
        return false;
    }

    const auto drawWord = [&](int16_t drawX,
                              int16_t drawY,
                              const char *text,
                              int16_t scale,
                              HermesXGpsLayerColorMapper colorMapper) {
        const int16_t spacing = std::max<int16_t>(1, scale / 2);
        const int16_t coreWidth = measurePosterTitleText(text, scale, spacing);
        const int16_t coreHeight = scale * 7;
        const int16_t margin = std::max<int16_t>(4, scale + 1);
        const int16_t regionX = drawX - margin;
        const int16_t regionY = drawY - margin;
        const int16_t regionWidth = coreWidth + (margin * 2);
        const int16_t regionHeight = coreHeight + (margin * 2);
        const size_t regionSize =
            (regionWidth > 0 && regionHeight > 0) ? static_cast<size_t>(regionWidth) * regionHeight : 0;
        if (coreWidth <= 0 || coreHeight <= 0 || regionWidth > 96 || regionHeight > 48 || regionSize == 0 ||
            regionSize > bufferCapacity) {
            return false;
        }

        memset(fullMask, 0, regionSize);
        memset(layerMap, 0, regionSize);
        stampPosterTitleMask(fullMask, regionWidth, regionHeight, margin, margin, text, scale, spacing);

        const int16_t outerRadius = std::max<int16_t>(3, scale + 1);
        for (int16_t targetY = 0; targetY < regionHeight; ++targetY) {
            for (int16_t targetX = 0; targetX < regionWidth; ++targetX) {
                const int16_t index = (targetY * regionWidth) + targetX;
                uint8_t value = 0;
                if (fullMask[index] &&
                    HermesXNeonMask::isInteriorPixel(fullMask, regionWidth, regionHeight, targetX, targetY)) {
                    value = 7;
                } else if (fullMask[index]) {
                    value = 6;
                } else if (HermesXNeonMask::hasPixelInRadius(
                               fullMask, regionWidth, regionHeight, targetX, targetY, 1)) {
                    value = 5;
                } else if (HermesXNeonMask::hasPixelInRadius(
                               fullMask, regionWidth, regionHeight, targetX, targetY, 2)) {
                    value = 4;
                } else if (HermesXNeonMask::hasPixelInRadius(
                               fullMask, regionWidth, regionHeight, targetX, targetY, 3)) {
                    value = 3;
                } else if (HermesXNeonMask::hasPixelInRadius(
                               fullMask, regionWidth, regionHeight, targetX, targetY, 4)) {
                    value = 2;
                } else if (HermesXNeonMask::hasPixelInRadius(
                               fullMask, regionWidth, regionHeight, targetX, targetY, outerRadius)) {
                    value = 1;
                }
                layerMap[index] = value;
            }
        }

        for (int16_t row = 0; row < regionHeight; ++row) {
            const int16_t pixelY = regionY + row;
            if (pixelY < 0 || pixelY >= height) {
                continue;
            }
            int16_t runStart = -1;
            uint8_t runValue = 0xFF;
            for (int16_t col = 0; col <= regionWidth; ++col) {
                const uint8_t value = (col < regionWidth) ? layerMap[(row * regionWidth) + col] : 0xFF;
                if (value == runValue) {
                    continue;
                }
                if (runStart >= 0 && runValue > 0) {
                    int16_t startX = regionX + runStart;
                    int16_t endX = regionX + col - 1;
                    if (!(endX < 0 || startX >= width)) {
                        startX = std::max<int16_t>(0, startX);
                        endX = std::min<int16_t>(width - 1, endX);
                        const int16_t runWidth = endX - startX + 1;
                        if (runWidth > 0) {
                            display->fillRect565(startX, pixelY, runWidth, 1, colorMapper(runValue));
                        }
                    }
                }
                runValue = value;
                runStart = (col < regionWidth) ? col : -1;
            }
        }
        return true;
    };

    if (!drawWord(titleLayout.gpsX, titleLayout.y, "GPS", titleLayout.scale, gpsColorMapper)) {
        return false;
    }
    return drawWord(titleLayout.stateX, titleLayout.y, stateWord, titleLayout.scale, stateColorMapper);
}

bool HermesXGpsUiRenderer::drawPosterCoordinates(TFTDisplay *display,
                                                 const HermesXGpsPosterLayout &layout,
                                                 const HermesXGpsPosterCoordinateView &view,
                                                 HermesXGpsTextMeasure measureText,
                                                 HermesXGpsTextDrawer drawText,
                                                 HermesXGpsLayerColorMapper colorMapper)
{
    if (!display || !measureText || !drawText || !colorMapper) {
        return false;
    }

    const bool halfScale = true;
    const int16_t coordinateTracking = 0;
    const int16_t altitudeTracking = 1;
    const int16_t neonMargin = 0;
    const int16_t altitudeMargin = layout.tiny ? 1 : 2;
    const int16_t leftRegionMinX = layout.leftX + neonMargin;
    const int16_t leftRegionMaxX = layout.leftX + layout.leftW - neonMargin;
    const int16_t leftCenterX = layout.leftX + (layout.leftW / 2);

    char longitudeLine[24];
    char latitudeLine[24];
    char altitudeLine[16];
    const auto writeCoordinatePlaceholder = [](char *output, size_t outputSize, int wholeDigits, int decimals) {
        if (!output || outputSize == 0) {
            return;
        }
        size_t position = 0;
        for (int index = 0; index < wholeDigits && position < (outputSize - 1); ++index) {
            output[position++] = '-';
        }
        if (position < (outputSize - 1)) {
            output[position++] = '.';
        }
        for (int index = 0; index < decimals && position < (outputSize - 1); ++index) {
            output[position++] = '-';
        }
        output[position] = '\0';
    };

    int16_t longitudeWidth = 0;
    int16_t latitudeWidth = 0;
    const int16_t coordinateUsableWidth = std::max<int16_t>(44, layout.leftW - (neonMargin * 2));
    const auto measureCoordinate =
        [&](const char *line) { return measureText(line, halfScale, coordinateTracking); };
    if (view.hasCoordinates) {
        int coordinateDecimals = -1;
        for (int decimals = 7; decimals >= 2; --decimals) {
            snprintf(longitudeLine, sizeof(longitudeLine), "%.*f", decimals, view.longitude);
            snprintf(latitudeLine, sizeof(latitudeLine), "%.*f", decimals, view.latitude);
            longitudeWidth = measureCoordinate(longitudeLine);
            latitudeWidth = measureCoordinate(latitudeLine);
            if (longitudeWidth > 0 && latitudeWidth > 0 && longitudeWidth <= coordinateUsableWidth &&
                latitudeWidth <= coordinateUsableWidth) {
                coordinateDecimals = decimals;
                break;
            }
        }
        if (coordinateDecimals < 0) {
            snprintf(longitudeLine, sizeof(longitudeLine), "%.2f", view.longitude);
            snprintf(latitudeLine, sizeof(latitudeLine), "%.2f", view.latitude);
            longitudeWidth = measureCoordinate(longitudeLine);
            latitudeWidth = measureCoordinate(latitudeLine);
        }
    } else {
        writeCoordinatePlaceholder(longitudeLine, sizeof(longitudeLine), 2, 2);
        writeCoordinatePlaceholder(latitudeLine, sizeof(latitudeLine), 2, 2);
        longitudeWidth = measureCoordinate(longitudeLine);
        latitudeWidth = measureCoordinate(latitudeLine);
    }

    if (longitudeWidth <= 0 || latitudeWidth <= 0) {
        return false;
    }

    if (view.hasAltitude) {
        snprintf(altitudeLine, sizeof(altitudeLine), "%ldm", static_cast<long>(view.altitudeMeters));
    } else {
        snprintf(altitudeLine, sizeof(altitudeLine), "--m");
    }
    const int16_t altitudeWidth = measureText(altitudeLine, halfScale, altitudeTracking);

    const auto clampLineX = [&](int16_t wantedX, int16_t lineWidth) {
        const int16_t maxX = leftRegionMaxX - lineWidth;
        if (maxX <= leftRegionMinX) {
            return leftRegionMinX;
        }
        return std::max<int16_t>(leftRegionMinX, std::min<int16_t>(wantedX, maxX));
    };

    const int16_t longitudeX = clampLineX(leftCenterX - (longitudeWidth / 2), longitudeWidth);
    const int16_t latitudeX = clampLineX(leftCenterX - (latitudeWidth / 2), latitudeWidth);
    int16_t altitudeX =
        clampLineX(layout.mountainX + layout.mountainW + (layout.tiny ? 6 : 8), altitudeWidth);
    if (altitudeWidth > 0 && (altitudeX + altitudeWidth) > leftRegionMaxX) {
        altitudeX = clampLineX(leftCenterX - (altitudeWidth / 2), altitudeWidth);
    }

    bool rendered = drawText(display,
                             longitudeX,
                             layout.coordY1,
                             longitudeLine,
                             halfScale,
                             colorMapper,
                             false,
                             coordinateTracking,
                             -1);
    rendered = drawText(display,
                        latitudeX,
                        layout.coordY2,
                        latitudeLine,
                        halfScale,
                        colorMapper,
                        false,
                        coordinateTracking,
                        -1) ||
               rendered;
    if (altitudeWidth > 0) {
        rendered = drawText(display,
                            altitudeX,
                            layout.altitudeY,
                            altitudeLine,
                            halfScale,
                            colorMapper,
                            false,
                            altitudeTracking,
                            altitudeMargin) ||
                   rendered;
    }
    return rendered;
}

bool HermesXGpsUiRenderer::drawPosterDecor(TFTDisplay *display,
                                           int16_t width,
                                           int16_t height,
                                           const HermesXGpsPosterLayout &layout,
                                           const HermesXGpsPosterDecorView &view,
                                           const HermesXGpsPosterDecorCallbacks &callbacks)
{
    if (!display || width <= 0 || height <= 0 || !callbacks.drawCircle || !callbacks.drawLine ||
        !callbacks.drawNeonLine || !callbacks.measureText || !callbacks.drawText || !callbacks.warmColorMapper) {
        return false;
    }

    static constexpr float kPi = 3.14159265358979323846f;
    const uint16_t black = TFTDisplay::rgb565(0x00, 0x00, 0x00);
    const uint16_t waveCore = TFTDisplay::rgb565(0x1D, 0xD7, 0xFF);
    const uint16_t waveGlowNear = TFTDisplay::rgb565(0x08, 0x67, 0xC8);
    const uint16_t waveGlowFar = TFTDisplay::rgb565(0x03, 0x23, 0x75);
    const uint16_t mountainCore = TFTDisplay::rgb565(0xFF, 0xFF, 0xFF);
    const uint16_t mountainGlowNear = TFTDisplay::rgb565(0x88, 0xDE, 0xFF);
    const uint16_t mountainGlowFar = TFTDisplay::rgb565(0x0A, 0x2E, 0x74);

    display->fillRect565(0, 0, width, height, black);
    drawPosterGlobeBloom(display, width, height, layout, callbacks.drawCircle);

    const int16_t waveLines = layout.tiny ? 8 : 10;
    const int16_t waveSegments = layout.tiny ? 22 : 24;
    const int16_t waveTargetY = layout.globeCy + layout.globeR / 4;
    for (int16_t index = 0; index < waveLines; ++index) {
        const float spread = static_cast<float>(index - (waveLines / 2));
        const int16_t startY = height - 1 - (index * (layout.tiny ? 3 : 5));
        const int16_t endY =
            waveTargetY + static_cast<int16_t>(spread * (layout.tiny ? 2.2f : 1.9f));
        int16_t previousX = 0;
        int16_t previousY = startY;
        for (int16_t segment = 1; segment <= waveSegments; ++segment) {
            const float progress = static_cast<float>(segment) / static_cast<float>(waveSegments);
            const int16_t x = static_cast<int16_t>(lrintf((width - 1) * progress));
            const float baseY = static_cast<float>(startY) * (1.0f - progress) +
                                static_cast<float>(endY) * progress;
            const float amplitude = (layout.tiny ? 1.4f : 3.2f) * (1.0f - progress);
            const float wave =
                sinf((progress * (layout.tiny ? 2.1f : 2.5f) * kPi) + (index * 0.32f)) * amplitude;
            const int16_t y = static_cast<int16_t>(lrintf(baseY + wave));
            callbacks.drawNeonLine(display,
                                   width,
                                   height,
                                   previousX,
                                   previousY,
                                   x,
                                   y,
                                   waveCore,
                                   waveGlowNear,
                                   waveGlowFar,
                                   1,
                                   layout.tiny ? 0 : 1,
                                   layout.tiny ? 0 : 2);
            previousX = x;
            previousY = y;
            if ((segment & 0x03) == 0) {
                yield();
            }
        }
        yield();
    }

    const int16_t topArcLines = layout.tiny ? 5 : 6;
    static constexpr int16_t kTopArcSegments = 16;
    for (int16_t index = 0; index < topArcLines; ++index) {
        const int16_t startX = width - 1;
        const int16_t startY = -4 + (index * (layout.tiny ? 3 : 4));
        const int16_t endX = layout.globeCx - (layout.globeR / 3) + (index / 2);
        const int16_t endY = layout.globeCy - (layout.globeR / 2) + (index * (layout.tiny ? 2 : 3));
        int16_t previousX = startX;
        int16_t previousY = startY;
        for (int16_t segment = 1; segment <= kTopArcSegments; ++segment) {
            const float progress = static_cast<float>(segment) / static_cast<float>(kTopArcSegments);
            const float curve = (1.0f - progress) * progress;
            const int16_t x = static_cast<int16_t>(lrintf((startX * (1.0f - progress)) +
                                                          (endX * progress) -
                                                          curve * (layout.tiny ? 18.0f : 28.0f)));
            const int16_t y = static_cast<int16_t>(
                lrintf((startY * (1.0f - progress)) + (endY * progress) +
                       sinf((progress + (index * 0.06f)) * kPi) * 2.0f));
            callbacks.drawNeonLine(display,
                                   width,
                                   height,
                                   previousX,
                                   previousY,
                                   x,
                                   y,
                                   waveCore,
                                   waveGlowNear,
                                   waveGlowFar,
                                   1,
                                   layout.tiny ? 0 : 1,
                                   layout.tiny ? 0 : 2);
            previousX = x;
            previousY = y;
            if ((segment & 0x03) == 0) {
                yield();
            }
        }
        yield();
    }

    const int16_t mountainLeftX = layout.mountainX;
    const int16_t mountainLeftY = layout.mountainY + layout.mountainH;
    const int16_t mountainPeakX = layout.mountainX + (layout.mountainW * 40) / 100;
    const int16_t mountainPeakY = layout.mountainY;
    const int16_t mountainRightX = layout.mountainX + layout.mountainW;
    const int16_t mountainRightY = mountainLeftY;
    const int16_t smallLeftX = layout.mountainX + (layout.mountainW * 20) / 100;
    const int16_t smallPeakX = layout.mountainX + (layout.mountainW * 47) / 100;
    const int16_t smallPeakY = layout.mountainY + (layout.mountainH * 44) / 100;
    const int16_t smallRightX = layout.mountainX + (layout.mountainW * 72) / 100;
    const auto drawMountainLine = [&](int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
        callbacks.drawNeonLine(display,
                               width,
                               height,
                               x0,
                               y0,
                               x1,
                               y1,
                               mountainCore,
                               mountainGlowNear,
                               mountainGlowFar,
                               1,
                               2,
                               3);
    };
    drawMountainLine(mountainLeftX, mountainLeftY, mountainPeakX, mountainPeakY);
    drawMountainLine(mountainPeakX, mountainPeakY, mountainRightX, mountainRightY);
    drawMountainLine(mountainLeftX, mountainLeftY, mountainRightX, mountainRightY);
    drawMountainLine(smallLeftX, mountainLeftY, smallPeakX, smallPeakY);
    drawMountainLine(smallPeakX, smallPeakY, smallRightX, mountainRightY);

    drawPosterWireGlobe(display, width, height, layout, callbacks.drawLine);

    char satelliteLine[4];
    snprintf(satelliteLine, sizeof(satelliteLine), "%u", static_cast<unsigned>(view.satelliteCount));
    const int16_t tracking = layout.tiny ? 1 : 2;
    const int16_t textWidth = callbacks.measureText(satelliteLine, true, tracking);
    if (textWidth <= 0) {
        return true;
    }
    int16_t textX = layout.globeCx - (textWidth / 2);
    int16_t textY = layout.globeCy - (view.satelliteTextHeight / 2);
    const int16_t minX = 2 + view.neonTextMargin;
    const int16_t maxX = width - textWidth - 2 - view.neonTextMargin;
    const int16_t minY = 2 + view.neonTextMargin;
    const int16_t maxY = height - view.satelliteTextHeight - 2 - view.neonTextMargin;
    textX = std::max<int16_t>(minX, std::min<int16_t>(textX, maxX));
    textY = std::max<int16_t>(minY, std::min<int16_t>(textY, maxY));
    return callbacks.drawText(display,
                              textX,
                              textY,
                              satelliteLine,
                              true,
                              callbacks.warmColorMapper,
                              false,
                              tracking,
                              -1);
}

void HermesXGpsUiRenderer::drawStatusFrame(
    OLEDDisplay *display, int16_t x, int16_t y, const HermesXGpsStatusView &view)
{
    if (!display) {
        return;
    }

    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    const bool largeLayout = (width >= 240 && height >= 130);
    const bool compactLayout = (width < 220 || height < 120);
    const int16_t compactLineH = _fontHeight(FONT_SMALL_LOCAL);

    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setColor(BLACK);
    display->fillRect(x, y, width, height);
    display->setColor(WHITE);
    display->setFont(compactLayout ? FONT_SMALL_LOCAL : FONT_SMALL);

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
    drawSatelliteIcon(display, iconX, iconY, iconSize);

    char satLine[48];
    snprintf(satLine, sizeof(satLine), compactLayout ? u8"衛星:%u" : u8"衛星數量：%u", view.satelliteCount);
    const int16_t satY = y + usableHeight - (compactLayout ? compactLineH : FONT_HEIGHT_SMALL) - 2;
    HermesX_zh::drawMixedBounded(*display, x + 4, satY, leftPaneWidth - 8, satLine,
                                 HermesX_zh::GLYPH_WIDTH, compactLayout ? compactLineH : FONT_HEIGHT_SMALL, nullptr);

    const int16_t rightGap = compactLayout ? 6 : (largeLayout ? 10 : 8);
    const int16_t rightX = x + leftPaneWidth + rightGap;
    const int16_t rightWidth = width - leftPaneWidth - rightGap - 4;
    if (rightWidth < 30) {
        return;
    }

    if (compactLayout) {
        int16_t rowY = y + 2;

        display->setFont(FONT_SMALL_LOCAL);
        drawMonoNeonStringMaxWidth(display, rightX, rowY, rightWidth, view.title, 1, 0);
        rowY += compactLineH + 1;

        char lockLineCompact[40];
        snprintf(lockLineCompact, sizeof(lockLineCompact), "%s%s", u8"定位:", view.lockStatusShort);
        drawMonoNeonMixedBounded(
            display, rightX, rowY, rightWidth, lockLineCompact, HermesX_zh::GLYPH_WIDTH, compactLineH, 1, 0);
        rowY += compactLineH + 1;

        const char *coordLabelCompact = u8"座標:";
        const int16_t coordLabelCompactW = HermesX_zh::stringAdvance(coordLabelCompact, HermesX_zh::GLYPH_WIDTH, display);
        const int16_t coordCompactValueX = rightX + coordLabelCompactW + 2;
        const int16_t coordCompactValueW = rightWidth - (coordCompactValueX - rightX);
        HermesX_zh::drawMixedBounded(*display, rightX, rowY, rightWidth, coordLabelCompact, HermesX_zh::GLYPH_WIDTH,
                                     compactLineH, nullptr);
        display->setFont(FONT_SMALL_LOCAL);
        drawMonoNeonStringMaxWidth(display, coordCompactValueX, rowY, coordCompactValueW, view.longitude, 1, 0);
        rowY += compactLineH;
        drawMonoNeonStringMaxWidth(display, coordCompactValueX, rowY, coordCompactValueW, view.latitude, 1, 0);
        rowY += compactLineH + 1;

        const char *timeLabelCompact = u8"時間:";
        const int16_t timeLabelCompactW = HermesX_zh::stringAdvance(timeLabelCompact, HermesX_zh::GLYPH_WIDTH, display);
        const int16_t timeCompactValueX = rightX + timeLabelCompactW + 2;
        const int16_t timeCompactValueW = rightWidth - (timeCompactValueX - rightX);
        HermesX_zh::drawMixedBounded(*display, rightX, rowY, rightWidth, timeLabelCompact, HermesX_zh::GLYPH_WIDTH,
                                     compactLineH, nullptr);
        drawMonoNeonStringMaxWidth(display, timeCompactValueX, rowY, timeCompactValueW, view.compactTime, 1, 0);
        display->setFont(FONT_SMALL_LOCAL);
        return;
    }

    const int16_t valueFontHeight = largeLayout ? FONT_HEIGHT_MEDIUM : FONT_HEIGHT_SMALL;
    const int16_t sectionGap = largeLayout ? 8 : 4;
    const int16_t valueGap = largeLayout ? 2 : 1;
    int16_t rowY = y + (largeLayout ? 8 : 6);

    display->setFont(largeLayout ? FONT_MEDIUM : FONT_SMALL);
    const int16_t titleLineH = largeLayout ? FONT_HEIGHT_MEDIUM : FONT_HEIGHT_SMALL;
    drawMonoNeonStringMaxWidth(display, rightX, rowY, rightWidth, view.title, 2, 1);
    rowY += titleLineH + sectionGap;

    char lockLine[64];
    snprintf(lockLine, sizeof(lockLine), "%s%s", u8"衛星定位：", view.lockStatus);
    drawMonoNeonMixedBounded(display,
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
    drawMonoNeonStringMaxWidth(display, coordValueX, rowY, coordValueW, view.longitude, 2, 1);
    rowY += valueFontHeight + valueGap;
    drawMonoNeonStringMaxWidth(display, coordValueX, rowY, coordValueW, view.latitude, 2, 1);
    rowY += valueFontHeight + sectionGap;

    const char *timeLabel = u8"時間：";
    const int16_t timeLabelW = HermesX_zh::stringAdvance(timeLabel, HermesX_zh::GLYPH_WIDTH, display);
    const int16_t timeValueX = rightX + timeLabelW + (largeLayout ? 8 : 4);
    const int16_t timeValueW = rightWidth - (timeValueX - rightX);
    display->setFont(FONT_SMALL);
    HermesX_zh::drawMixedBounded(*display, rightX, rowY, rightWidth, timeLabel, HermesX_zh::GLYPH_WIDTH,
                                 largeLayout ? FONT_HEIGHT_MEDIUM : FONT_HEIGHT_SMALL, nullptr);
    display->setFont(largeLayout ? FONT_MEDIUM : FONT_SMALL);
    display->drawStringMaxWidth(timeValueX, rowY, timeValueW, view.date);
    rowY += valueFontHeight + valueGap;
    display->drawStringMaxWidth(timeValueX, rowY, timeValueW, view.time);
    drawFallbackCornerMapOutline(display, x, y, width, usableHeight);
    display->setFont(FONT_SMALL);
}

void HermesXGpsUiRenderer::drawPosterBackdrop(
    OLEDDisplay *display, int16_t x, int16_t y, HermesXGpsColorZoneDrawer drawColorZone)
{
    if (!display) {
        return;
    }

    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    const bool tinyLayout = (width <= 176 || height <= 96);

    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setColor(BLACK);
    display->fillRect(x, y, width, height);

    static const uint8_t kStars[][3] = {
        {5, 4, 2},   {17, 16, 2}, {45, 9, 2},   {65, 6, 2},  {6, 50, 2},   {38, 61, 2},
        {58, 44, 2}, {27, 85, 2}, {2, 97, 2},   {42, 98, 2}, {58, 86, 2},  {49, 29, 2},
    };
    display->setColor(WHITE);
    for (const auto &star : kStars) {
        const int16_t starX = x + static_cast<int16_t>((width * star[0]) / 100);
        const int16_t starY = y + static_cast<int16_t>((height * star[1]) / 100);
        display->fillCircle(starX, starY, tinyLayout ? 1 : star[2]);
    }

    int16_t satelliteSize = std::min(width, height);
    satelliteSize = (satelliteSize * (tinyLayout ? 58 : 52)) / 100;
    if (satelliteSize < (tinyLayout ? 34 : 46)) {
        satelliteSize = tinyLayout ? 34 : 46;
    }
    const int16_t satelliteCenterX = x + (width * (tinyLayout ? 16 : 18)) / 100;
    const int16_t satelliteCenterY = y + (height * (tinyLayout ? 52 : 49)) / 100;
    const int16_t satellitePadding = tinyLayout ? 9 : 12;
    if (drawColorZone) {
        drawColorZone(display,
                      satelliteCenterX - satelliteSize / 2 - satellitePadding,
                      satelliteCenterY - satelliteSize / 2 - satellitePadding,
                      satelliteSize + satellitePadding * 2,
                      satelliteSize + satellitePadding * 2,
                      rgb565(0x70, 0xB5, 0xD3),
                      0x0000);
    }
    drawPosterSatelliteMark(display, satelliteCenterX, satelliteCenterY, satelliteSize);

    int16_t earthRadius = (height * (tinyLayout ? 52 : 64)) / 100;
    if (earthRadius < (tinyLayout ? 28 : 40)) {
        earthRadius = tinyLayout ? 28 : 40;
    }
    const int16_t earthCenterX = x + width + (tinyLayout ? std::max<int16_t>(4, earthRadius / 5) : earthRadius / 3);
    const int16_t earthCenterY = y + height + (tinyLayout ? std::max<int16_t>(4, earthRadius / 6) : earthRadius / 2);
    const int16_t redOuterRadius = earthRadius + (tinyLayout ? 10 : 13);
    const int16_t redInnerRadius = earthRadius + (tinyLayout ? 7 : 9);
    const int16_t glowOuterRadius = earthRadius + (tinyLayout ? 4 : 6);
    const int16_t glowInnerRadius = earthRadius + (tinyLayout ? 2 : 4);

    if (drawColorZone) {
        const auto addEarthZone = [&](int16_t radius, uint16_t foreground) {
            drawColorZone(display,
                          earthCenterX - radius,
                          earthCenterY - radius,
                          radius * 2 + 2,
                          radius * 2 + 2,
                          foreground,
                          0x0000);
        };
        addEarthZone(redOuterRadius, rgb565(0x70, 0x08, 0x1A));
        addEarthZone(redInnerRadius, rgb565(0xE4, 0x4C, 0x7A));
        addEarthZone(glowOuterRadius, rgb565(0x1B, 0x59, 0x73));
        addEarthZone(glowInnerRadius, rgb565(0x67, 0xBA, 0xD7));
        addEarthZone(earthRadius, rgb565(0xA9, 0xD0, 0xE0));
        drawColorZone(display,
                      earthCenterX - earthRadius + earthRadius / 2,
                      earthCenterY - earthRadius + earthRadius / 4,
                      earthRadius,
                      earthRadius + earthRadius / 2,
                      rgb565(0x6D, 0xB1, 0xC9),
                      0x0000);
    }

    display->setColor(WHITE);
    display->fillCircle(earthCenterX, earthCenterY, redOuterRadius);
    display->fillCircle(earthCenterX, earthCenterY, redInnerRadius);
    display->fillCircle(earthCenterX, earthCenterY, glowOuterRadius);
    display->fillCircle(earthCenterX, earthCenterY, glowInnerRadius);
    display->fillCircle(earthCenterX, earthCenterY, earthRadius);
    display->fillCircle(earthCenterX - earthRadius / 2, earthCenterY - earthRadius / 2, earthRadius / 4);
    display->fillCircle(earthCenterX - earthRadius / 3, earthCenterY - earthRadius / 8, earthRadius / 5);
    display->fillCircle(earthCenterX - earthRadius / 2, earthCenterY + earthRadius / 3, earthRadius / 5);
    display->fillCircle(earthCenterX - earthRadius / 5, earthCenterY + earthRadius / 6, earthRadius / 7);
}

void HermesXGpsUiRenderer::drawNeonAsciiText(OLEDDisplay *display,
                                             int16_t x,
                                             int16_t y,
                                             const char *text,
                                             int16_t textHeight,
                                             uint16_t glowOuterColor,
                                             uint16_t glowInnerColor,
                                             uint16_t coreColor,
                                             HermesXGpsColorZoneDrawer drawColorZone)
{
    if (!display || !text || !*text || textHeight <= 0) {
        return;
    }
    const int16_t textWidth = display->getStringWidth(text);
    if (textWidth <= 0) {
        return;
    }

    const int16_t outerRadius = (textHeight >= FONT_HEIGHT_LARGE) ? 3 : 2;
    const int16_t innerRadius = (outerRadius > 2) ? 2 : 1;
    if (drawColorZone) {
        drawColorZone(display,
                      x - outerRadius,
                      y - outerRadius,
                      textWidth + outerRadius * 2,
                      textHeight + outerRadius * 2,
                      glowOuterColor,
                      0x0000);
        drawColorZone(display,
                      x - innerRadius,
                      y - innerRadius,
                      textWidth + innerRadius * 2,
                      textHeight + innerRadius * 2,
                      glowInnerColor,
                      0x0000);
        drawColorZone(display, x, y, textWidth, textHeight, coreColor, 0x0000);
    }

    const auto drawGlowRing = [&](int16_t radius) {
        if (radius <= 0) {
            return;
        }
        display->drawString(x + radius, y, text);
        display->drawString(x - radius, y, text);
        display->drawString(x, y + radius, text);
        display->drawString(x, y - radius, text);
        display->drawString(x + radius, y + radius, text);
        display->drawString(x - radius, y + radius, text);
        display->drawString(x + radius, y - radius, text);
        display->drawString(x - radius, y - radius, text);
    };
    drawGlowRing(outerRadius);
    if (innerRadius != outerRadius) {
        drawGlowRing(innerRadius);
    }
    display->drawString(x, y, text);
}

void HermesXGpsUiRenderer::drawPosterBase(OLEDDisplay *display,
                                          int16_t x,
                                          int16_t y,
                                          int16_t width,
                                          int16_t height,
                                          bool gpsEnabled,
                                          HermesXGpsColorZoneDrawer drawColorZone)
{
    if (!display || width <= 0 || height <= 0) {
        return;
    }

    if (x != 0 || y != 0) {
        // Slide transitions render adjacent frame tiles. Keep poster decoration
        // inside the active tile so it cannot leave residue on its neighbours.
        display->setColor(BLACK);
        display->fillRect(x, y, width, height);
        display->setFont(FONT_SMALL);
        return;
    }

    drawPosterBackdrop(display, x, y, drawColorZone);
    drawPosterHeader(display, x, y, width, height, gpsEnabled, drawColorZone);
}

void HermesXGpsUiRenderer::drawPosterHeader(OLEDDisplay *display,
                                            int16_t x,
                                            int16_t y,
                                            int16_t width,
                                            int16_t height,
                                            bool gpsEnabled,
                                            HermesXGpsColorZoneDrawer drawColorZone)
{
    if (!display || width <= 0 || height <= 0) {
        return;
    }

    const bool tinyLayout = (width <= 176 || height <= 96);
    display->setFont(tinyLayout ? FONT_MEDIUM : FONT_LARGE);
    int16_t titleFontHeight = tinyLayout ? FONT_HEIGHT_MEDIUM : FONT_HEIGHT_LARGE;
    if (!tinyLayout && display->getStringWidth("GPS ON") > ((width * 52) / 100)) {
        display->setFont(FONT_MEDIUM);
        titleFontHeight = FONT_HEIGHT_MEDIUM;
    }

    static constexpr const char *kGpsWord = "GPS";
    const char *stateWord = gpsEnabled ? "ON" : "OFF";
    int16_t titleGap = width / 30;
    if (titleGap < (tinyLayout ? 4 : 6)) {
        titleGap = tinyLayout ? 4 : 6;
    }
    const int16_t gpsWidth = display->getStringWidth(kGpsWord);
    const int16_t stateWidth = display->getStringWidth(stateWord);
    const int16_t titleY = y + (height * (tinyLayout ? 9 : 17)) / 100;
    const int16_t titleCenterX = x + (width * (tinyLayout ? 60 : 58)) / 100;
    const int16_t titleX = titleCenterX - (gpsWidth + titleGap + stateWidth) / 2;

    const uint16_t stateGlowOuter = gpsEnabled ? rgb565(0x00, 0x56, 0xAF) : rgb565(0x1B, 0x12, 0x12);
    const uint16_t stateGlowInner = gpsEnabled ? rgb565(0x7D, 0xE5, 0xFF) : rgb565(0xA8, 0x58, 0x58);
    const uint16_t stateCore = gpsEnabled ? rgb565(0xE4, 0xF5, 0xFF) : rgb565(0xFF, 0xE8, 0xE8);
    drawNeonAsciiText(display,
                      titleX,
                      titleY,
                      kGpsWord,
                      titleFontHeight,
                      rgb565(0x8A, 0x2A, 0x00),
                      rgb565(0xFF, 0xC5, 0x5F),
                      rgb565(0xFF, 0xF4, 0xDB),
                      drawColorZone);
    drawNeonAsciiText(display,
                      titleX + gpsWidth + titleGap,
                      titleY,
                      stateWord,
                      titleFontHeight,
                      stateGlowOuter,
                      stateGlowInner,
                      stateCore,
                      drawColorZone);
    display->setFont(FONT_SMALL);
}

void HermesXGpsUiRenderer::drawSatelliteIcon(OLEDDisplay *display, int16_t x, int16_t y, int16_t size)
{
    if (!display) {
        return;
    }
    size = std::max<int16_t>(24, size);

    const int16_t centerX = x + size / 2;
    const int16_t topY = y + 1;
    const int16_t bodyW = std::max<int16_t>(4, size / 7);
    const int16_t bodyH = std::max<int16_t>(8, size / 4);
    const int16_t panelW = std::max<int16_t>(6, size / 4);
    const int16_t panelH = std::max<int16_t>(3, size / 8);
    const int16_t panelGap = std::max<int16_t>(2, size / 12);
    const int16_t mastH = std::max<int16_t>(4, size / 5);
    const int16_t arcR1 = std::max<int16_t>(4, size / 7);
    const int16_t arcR2 = std::max<int16_t>(6, size / 5);

    const int16_t bodyX = centerX - bodyW / 2;
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
    display->drawLine(centerX, mastY1, centerX, mastY2);
    const int16_t dishY = mastY2 + 2;
    display->drawCircle(centerX, dishY, 1);

    const auto drawSignalArc = [&](int16_t radius, int16_t thickness) {
        const int16_t pointsX[] = {static_cast<int16_t>(centerX - radius),
                                   static_cast<int16_t>(centerX - (radius * 2) / 3), centerX,
                                   static_cast<int16_t>(centerX + (radius * 2) / 3),
                                   static_cast<int16_t>(centerX + radius)};
        const int16_t pointsY[] = {static_cast<int16_t>(dishY + 1), static_cast<int16_t>(dishY + radius / 2),
                                   static_cast<int16_t>(dishY + (radius * 2) / 3),
                                   static_cast<int16_t>(dishY + radius / 2), static_cast<int16_t>(dishY + 1)};
        for (int16_t offset = 0; offset < thickness; ++offset) {
            const int16_t yOffset = offset - thickness / 2;
            for (uint8_t point = 0; point < 4; ++point) {
                display->drawLine(pointsX[point], pointsY[point] + yOffset, pointsX[point + 1],
                                  pointsY[point + 1] + yOffset);
            }
        }
    };

    drawSignalArc(arcR1, 1);
    drawSignalArc(arcR2, size >= 40 ? 2 : 1);
}

void HermesXGpsUiRenderer::drawPosterSatelliteMark(
    OLEDDisplay *display, int16_t centerX, int16_t centerY, int16_t size)
{
    if (!display) {
        return;
    }
    size = std::max<int16_t>(30, size);
    const int16_t hubRadius = std::max<int16_t>(8, size / 6);
    const int16_t armSpan = std::max<int16_t>(hubRadius + 8, size / 2);
    const int16_t armThickness = std::max<int16_t>(4, size / 8);

    drawThickLine(display, centerX - armSpan, centerY + armSpan, centerX - hubRadius, centerY + hubRadius,
                  armThickness);
    drawThickLine(display, centerX + hubRadius, centerY - hubRadius, centerX + armSpan, centerY - armSpan,
                  armThickness);
    display->fillCircle(centerX, centerY, hubRadius);

    const int16_t dishRadius = hubRadius / 2;
    const int16_t dishCenterX = centerX + hubRadius + dishRadius + 6;
    const int16_t dishCenterY = centerY + hubRadius - 1;
    display->fillCircle(dishCenterX, dishCenterY, dishRadius);
    display->setColor(BLACK);
    display->fillCircle(dishCenterX + dishRadius / 2 + 1, dishCenterY, dishRadius);
    display->setColor(WHITE);
}

void HermesXGpsUiRenderer::drawFallbackCornerMapOutline(
    OLEDDisplay *display, int16_t x, int16_t y, int16_t width, int16_t height)
{
    if (!display || width < 150 || height < 86) {
        return;
    }

    const bool compactLayout = width < 220 || height < 120;
    const int16_t earthRadius = compactLayout ? 18 : 26;
    const int16_t earthCenterX = x + width + (compactLayout ? earthRadius / 3 : earthRadius / 4);
    const int16_t earthCenterY = y + height + (compactLayout ? earthRadius / 5 : earthRadius / 7);
    const int16_t haloOuterRadius = earthRadius + (compactLayout ? 5 : 7);
    const int16_t haloInnerRadius = earthRadius + (compactLayout ? 3 : 5);

    display->drawCircle(earthCenterX, earthCenterY, haloOuterRadius);
    display->drawCircle(earthCenterX, earthCenterY, haloInnerRadius);
    display->drawCircle(earthCenterX, earthCenterY, earthRadius);
    display->drawCircle(earthCenterX - earthRadius / 2, earthCenterY - earthRadius / 2,
                        std::max<int16_t>(2, earthRadius / 5));
    display->drawCircle(earthCenterX - earthRadius / 3, earthCenterY - earthRadius / 8,
                        std::max<int16_t>(2, earthRadius / 6));
    display->drawCircle(earthCenterX - earthRadius / 2, earthCenterY + earthRadius / 3,
                        std::max<int16_t>(2, earthRadius / 7));
}

} // namespace graphics
