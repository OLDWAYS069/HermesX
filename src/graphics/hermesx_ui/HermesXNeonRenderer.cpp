#include "HermesXNeonRenderer.h"

#include "graphics/TFTDisplay.h"

namespace graphics
{
namespace
{

struct PaintContext {
    TFTDisplay *tft;
    uint16_t black;
    HermesXNeonLayerColorMapper layerColor;
};

uint16_t black565()
{
    return TFTDisplay::rgb565(0x00, 0x00, 0x00);
}

void paintRun(const HermesXNeonPaintRun &run, void *context)
{
    auto *paint = static_cast<PaintContext *>(context);
    const uint16_t color = (run.layer > 0) ? paint->layerColor(run.layer) : paint->black;
    paint->tft->fillRect565(run.x, run.y, run.width, 1, color);
}

} // namespace


HermesXNeonTextRenderResult HermesXNeonRenderer::renderText(TFTDisplay *tft,
                                                            HermesXNeonWorkspace &workspace,
                                                            const char *text,
                                                            int16_t drawX,
                                                            int16_t drawY,
                                                            bool halfScale,
                                                            int16_t tracking,
                                                            int16_t marginOverride,
                                                            bool clearBackground,
                                                            HermesXNeonGlyphWidthProvider widthProvider,
                                                            HermesXNeonClockGlyphProvider glyphProvider,
                                                            HermesXNeonLayerColorMapper layerColor)
{
    HermesXNeonTextRenderResult result;
    if (!tft || !text || !*text || !widthProvider || !glyphProvider || !layerColor) {
        return result;
    }
    if (!workspace.composeTextLayerMap(text,
                                       drawX,
                                       drawY,
                                       halfScale,
                                       tracking,
                                       marginOverride,
                                       widthProvider,
                                       glyphProvider,
                                       result.composition)) {
        return result;
    }
    result.composed = true;
    result.rendered = paintText(tft, workspace, result.composition, clearBackground, layerColor);
    return result;
}

bool HermesXNeonRenderer::paintText(TFTDisplay *tft,
                                    HermesXNeonWorkspace &workspace,
                                    const HermesXNeonTextComposition &composition,
                                    bool clearBackground,
                                    HermesXNeonLayerColorMapper layerColor)
{
    if (!tft || !layerColor) {
        return false;
    }
    const uint16_t black = black565();
    if (clearBackground) {
        tft->fillRect565(
            composition.regionX, composition.regionY, composition.regionWidth, composition.regionHeight, black);
    }
    PaintContext paintContext{tft, black, layerColor};
    return workspace.visitSharedMapPaintRuns(composition.regionX,
                                             composition.regionY,
                                             composition.regionWidth,
                                             composition.regionHeight,
                                             paintRun,
                                             &paintContext);
}

} // namespace graphics
