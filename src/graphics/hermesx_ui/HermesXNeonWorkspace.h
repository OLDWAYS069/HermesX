#pragma once

#include "HermesXHomeUiRenderer.h"

#include <cstddef>
#include <cstdint>

namespace graphics
{

constexpr int16_t HermesXNeonClockGlyphTileWidth =
    HermesXHomeUiRenderer::NeonClockGlyphMaxWidth + (HermesXHomeUiRenderer::NeonClockMargin * 2);
constexpr int16_t HermesXNeonClockGlyphTileHeight =
    HermesXHomeUiRenderer::NeonClockGlyphHeight + (HermesXHomeUiRenderer::NeonClockMargin * 2);
constexpr size_t HermesXNeonClockGlyphCacheCount = 13;
constexpr int16_t HermesXNeonTextMaxWidth = 128;
constexpr int16_t HermesXNeonTextMaxHeight = 32;
constexpr int16_t HermesXGpsTitleMaskMaxWidth = 96;
constexpr int16_t HermesXGpsTitleMaskMaxHeight = 48;
constexpr size_t HermesXGpsTitleMaskCapacity =
    static_cast<size_t>(HermesXGpsTitleMaskMaxWidth) * HermesXGpsTitleMaskMaxHeight;
constexpr int16_t HermesXNeonSharedMaxWidth =
    (HermesXHomeUiRenderer::NeonClockMaxRegionWidth > HermesXNeonTextMaxWidth)
        ? HermesXHomeUiRenderer::NeonClockMaxRegionWidth
        : HermesXNeonTextMaxWidth;
constexpr int16_t HermesXNeonSharedMaxHeight =
    (HermesXHomeUiRenderer::NeonClockMaxRegionHeight > HermesXNeonTextMaxHeight)
        ? HermesXHomeUiRenderer::NeonClockMaxRegionHeight
        : HermesXNeonTextMaxHeight;
constexpr size_t HermesXNeonSharedMapCapacity =
    static_cast<size_t>(HermesXNeonSharedMaxWidth) * HermesXNeonSharedMaxHeight;

struct HermesXNeonClockGlyphCache {
    bool valid = false;
    char ch = '\0';
    uint8_t coreW = 0;
    uint8_t tileW = 0;
    uint8_t tileH = 0;
    uint8_t layerMap[HermesXNeonClockGlyphTileWidth * HermesXNeonClockGlyphTileHeight]{};
};

struct HermesXNeonClockGlyphSource {
    char ch = '\0';
    uint8_t width = 0;
    uint8_t height = 0;
    uint8_t bytesPerRow = 0;
    const uint8_t *bitmap = nullptr;
};

using HermesXNeonClockGlyphProvider = HermesXNeonClockGlyphCache *(*)(char ch);
using HermesXNeonGlyphWidthProvider = uint8_t (*)(char ch);

struct HermesXNeonWorkspaceStatus {
    bool glyphCache = false;
    bool sharedMap = false;
    bool clockMasks = false;
    bool gpsTitleMasks = false;
};

struct HermesXNeonPaintRun {
    int16_t x = 0;
    int16_t y = 0;
    int16_t width = 0;
    uint8_t layer = 0;
};

using HermesXNeonRunVisitor = void (*)(const HermesXNeonPaintRun &run, void *context);

struct HermesXNeonTextComposition {
    int16_t drawX = 0;
    int16_t drawY = 0;
    int16_t regionX = 0;
    int16_t regionY = 0;
    int16_t regionWidth = 0;
    int16_t regionHeight = 0;
    int16_t margin = 0;
    int16_t tracking = 0;
    bool halfScale = false;
};

class HermesXNeonWorkspace
{
  public:
    static HermesXNeonWorkspace &instance();

    bool ensureAllocated();
    void release();
    HermesXNeonWorkspaceStatus status() const;
    uint32_t glyphCacheBuildCount() const { return glyphCacheBuildCount_; }
    HermesXNeonClockGlyphCache *getOrBuildClockGlyph(
        size_t glyphIndex, const HermesXNeonClockGlyphSource &source, bool &built);
    static int16_t measureTextWidth(const char *text,
                                    bool halfScale,
                                    int16_t tracking,
                                    HermesXNeonGlyphWidthProvider widthProvider);
    uint8_t *composeTextLayerMap(const char *text,
                                 int16_t drawX,
                                 int16_t drawY,
                                 bool halfScale,
                                 int16_t tracking,
                                 int16_t marginOverride,
                                 HermesXNeonGlyphWidthProvider widthProvider,
                                 HermesXNeonClockGlyphProvider glyphProvider,
                                 HermesXNeonTextComposition &composition);
    bool visitSharedMapPaintRuns(int16_t regionX,
                                 int16_t regionY,
                                 int16_t regionWidth,
                                 int16_t regionHeight,
                                 HermesXNeonRunVisitor visitor,
                                 void *context) const;
    HermesXNeonClockGlyphCache *glyphCaches() { return glyphCaches_; }
    uint8_t *gpsTitleFullMask() { return gpsTitleFullMask_; }
    uint8_t *gpsTitleLayerMap() { return gpsTitleLayerMap_; }

  private:
    HermesXNeonWorkspace() = default;

    HermesXNeonClockGlyphCache *glyphCaches_ = nullptr;
    uint8_t *sharedMap_ = nullptr;
    uint8_t *clockFullMask_ = nullptr;
    uint8_t *clockCoreMask_ = nullptr;
    uint8_t *gpsTitleFullMask_ = nullptr;
    uint8_t *gpsTitleLayerMap_ = nullptr;
    uint32_t glyphCacheBuildCount_ = 0;
};

} // namespace graphics
