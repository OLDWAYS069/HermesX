#include "HermesXGpsDirectRenderer.h"

#include "HermesXDirectTftPrimitives.h"
#include "HermesXHomeUiRenderer.h"
#include "HermesXNeonRenderer.h"
#include "HermesXNeonWorkspace.h"
#include "HermesXPattanakarnNeonFont.h"
#include "graphics/TFTDisplay.h"
#include "main.h"

namespace graphics
{
namespace
{

uint16_t warmLayerColor(uint8_t value)
{
    static constexpr uint8_t colors[][3] = {
        {0x00, 0x00, 0x00}, {0x1A, 0x07, 0x00}, {0x34, 0x10, 0x00}, {0x64, 0x1E, 0x00},
        {0xA8, 0x36, 0x00}, {0xFF, 0x6A, 0x00}, {0xFF, 0x9E, 0x4A}, {0xFF, 0xF2, 0xE6},
    };
    const uint8_t index = value < 8 ? value : 0;
    return TFTDisplay::rgb565(colors[index][0], colors[index][1], colors[index][2]);
}

uint16_t coolLayerColor(uint8_t value)
{
    static constexpr uint8_t colors[][3] = {
        {0x00, 0x00, 0x00}, {0x03, 0x12, 0x2D}, {0x05, 0x30, 0x6B}, {0x00, 0x5E, 0xB6},
        {0x20, 0xA6, 0xFF}, {0x73, 0xE6, 0xFF}, {0xCC, 0xFB, 0xFF}, {0xF6, 0xFF, 0xFF},
    };
    const uint8_t index = value < 8 ? value : 0;
    return TFTDisplay::rgb565(colors[index][0], colors[index][1], colors[index][2]);
}

uint16_t alertLayerColor(uint8_t value)
{
    static constexpr uint8_t colors[][3] = {
        {0x00, 0x00, 0x00}, {0x28, 0x09, 0x0E}, {0x4E, 0x12, 0x1F}, {0x7E, 0x1B, 0x34},
        {0xBE, 0x2D, 0x50}, {0xFF, 0x5E, 0x86}, {0xFF, 0xB4, 0xC8}, {0xFF, 0xEF, 0xF3},
    };
    const uint8_t index = value < 8 ? value : 0;
    return TFTDisplay::rgb565(colors[index][0], colors[index][1], colors[index][2]);
}

int16_t measureText(const char *text, bool halfScale, int16_t tracking)
{
    return HermesXPattanakarnNeonFont::measureText(text, halfScale, tracking);
}

bool renderText(TFTDisplay *display,
                int16_t drawX,
                int16_t drawY,
                const char *text,
                bool halfScale,
                uint16_t (*layerColor)(uint8_t),
                bool clearBackground,
                int16_t tracking,
                int16_t marginOverride)
{
    if (!display || !text || !*text) {
        return false;
    }
    const HermesXNeonTextRenderResult result = HermesXNeonRenderer::renderText(
        display,
        HermesXNeonWorkspace::instance(),
        text,
        drawX,
        drawY,
        halfScale,
        tracking,
        marginOverride,
        clearBackground,
        HermesXPattanakarnNeonFont::glyphWidth,
        HermesXPattanakarnNeonFont::glyphCache,
        layerColor ? layerColor : HermesXHomeUiRenderer::neonClockLayerColor);
    if (result.composed) {
        LOG_DEBUG("[DirectGps] neon text text='%s' half=%d tracking=%d margin=%d region=%dx%d", text,
                  halfScale ? 1 : 0, tracking, result.composition.margin, result.composition.regionWidth,
                  result.composition.regionHeight);
    }
    return result.rendered;
}

} // namespace

bool HermesXGpsDirectRenderer::render(TFTDisplay *display,
                                      const HermesXGpsStateSnapshot &snapshot,
                                      const HermesXGpsPosterLayers &layers,
                                      HermesXGpsWorkspaceEnsure ensureWorkspace)
{
    const int16_t width = snapshot.poster.width;
    const int16_t height = snapshot.poster.height;
    if (!display || width <= 0 || height <= 0 || !ensureWorkspace || !ensureWorkspace()) {
        return false;
    }

    const HermesXGpsPosterLayout layout =
        HermesXGpsUiRenderer::makePosterLayout(width, height, HermesXPattanakarnNeonFont::HalfHeight);
    bool rendered = false;
    if (layers.decor) {
        HermesXGpsPosterDecorView decor = snapshot.decor;
        decor.satelliteTextHeight = HermesXPattanakarnNeonFont::HalfHeight;
        decor.neonTextMargin = static_cast<int16_t>((HermesXHomeUiRenderer::NeonClockMargin + 1) / 2);
        HermesXGpsPosterDecorCallbacks callbacks;
        callbacks.drawCircle = directFillCircle565;
        callbacks.drawLine = directDrawLine565;
        callbacks.drawNeonLine = directDrawNeonLine565;
        callbacks.measureText = measureText;
        callbacks.drawText = renderText;
        callbacks.warmColorMapper = warmLayerColor;
        rendered = HermesXGpsUiRenderer::drawPosterDecor(display, width, height, layout, decor, callbacks) || rendered;
    }
    if (layers.title) {
        auto &workspace = HermesXNeonWorkspace::instance();
        const char *stateWord = snapshot.gpsEnabled ? "ON" : "OFF";
        rendered = HermesXGpsUiRenderer::drawPosterTitleNeon(display,
                                                             width,
                                                             height,
                                                             layout,
                                                             stateWord,
                                                             workspace.gpsTitleFullMask(),
                                                             workspace.gpsTitleLayerMap(),
                                                             HermesXGpsTitleMaskCapacity,
                                                             warmLayerColor,
                                                             snapshot.gpsEnabled ? coolLayerColor : alertLayerColor) ||
                   rendered;
    }
    if (layers.coordinates) {
        rendered = HermesXGpsUiRenderer::drawPosterCoordinates(
                       display, layout, snapshot.coordinates, measureText, renderText, warmLayerColor) ||
                   rendered;
    }
    return rendered;
}

} // namespace graphics
