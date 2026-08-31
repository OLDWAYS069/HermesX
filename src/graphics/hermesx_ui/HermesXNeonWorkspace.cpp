#include "HermesXNeonWorkspace.h"

#include "HermesXNeonMask.h"

#include <cstdlib>
#include <cstring>

namespace graphics
{
namespace
{

template <typename T> void releaseBuffer(T *&buffer)
{
    if (!buffer) {
        return;
    }
    std::free(buffer);
    buffer = nullptr;
}

void stampClockGlyphMask(uint8_t *mask, const HermesXNeonClockGlyphSource &source)
{
    if (!mask || !source.bitmap || source.height == 0 || source.height > HermesXHomeUiRenderer::NeonClockGlyphHeight) {
        return;
    }

    constexpr int16_t Margin = HermesXHomeUiRenderer::NeonClockMargin;
    for (uint8_t row = 0; row < source.height; ++row) {
        const int16_t targetY = Margin + row;
        for (uint8_t byteIndex = 0; byteIndex < source.bytesPerRow; ++byteIndex) {
            const uint8_t rowBits = pgm_read_byte(source.bitmap + row * source.bytesPerRow + byteIndex);
            for (uint8_t bit = 0; bit < 8; ++bit) {
                const uint8_t column = static_cast<uint8_t>(byteIndex * 8 + bit);
                if (column >= source.width) {
                    break;
                }
                if (rowBits & (0x80 >> bit)) {
                    mask[(targetY * HermesXNeonClockGlyphTileWidth) + Margin + column] = 1;
                }
            }
        }
    }
}

void visitLayerMapPaintRuns(const uint8_t *layerMap,
                            const uint8_t *previousLayerMap,
                            int16_t regionX,
                            int16_t regionY,
                            int16_t regionWidth,
                            int16_t regionHeight,
                            bool fullRepaint,
                            bool includeClears,
                            HermesXNeonRunVisitor visitor,
                            void *context)
{
    for (int16_t row = 0; row < regionHeight; ++row) {
        int16_t runStart = -1;
        uint8_t runLayer = 0xFF;
        for (int16_t column = 0; column <= regionWidth; ++column) {
            bool changed = fullRepaint;
            uint8_t layer = 0xFF;
            if (column < regionWidth) {
                const int16_t index = (row * regionWidth) + column;
                layer = layerMap[index];
                changed = fullRepaint || previousLayerMap[index] != layer;
            } else {
                changed = false;
            }

            if (!changed || layer != runLayer) {
                if (!fullRepaint && runStart >= 0 && runLayer == 0 && !includeClears) {
                    runStart = -1;
                    continue;
                }
                if (runStart >= 0 && (runLayer > 0 || includeClears)) {
                    HermesXNeonPaintRun run;
                    run.x = regionX + runStart;
                    run.y = regionY + row;
                    run.width = column - runStart;
                    run.layer = runLayer;
                    visitor(run, context);
                }
                runStart = -1;
            }

            if (changed) {
                if (runStart < 0) {
                    runStart = column;
                    runLayer = layer;
                }
            } else {
                runLayer = 0xFF;
            }
        }
    }
}

} // namespace

HermesXNeonWorkspace &HermesXNeonWorkspace::instance()
{
    static HermesXNeonWorkspace workspace;
    return workspace;
}

bool HermesXNeonWorkspace::ensureAllocated()
{
    const size_t glyphMaskBytes =
        static_cast<size_t>(HermesXNeonClockGlyphTileWidth) * HermesXNeonClockGlyphTileHeight;
    if (!glyphCaches_) {
        glyphCaches_ = static_cast<HermesXNeonClockGlyphCache *>(
            std::calloc(HermesXNeonClockGlyphCacheCount, sizeof(HermesXNeonClockGlyphCache)));
    }
    if (!sharedMap_) {
        sharedMap_ = static_cast<uint8_t *>(std::malloc(HermesXNeonSharedMapCapacity));
    }
    if (!clockFullMask_) {
        clockFullMask_ = static_cast<uint8_t *>(std::malloc(glyphMaskBytes));
    }
    if (!clockCoreMask_) {
        clockCoreMask_ = static_cast<uint8_t *>(std::malloc(glyphMaskBytes));
    }
    if (!gpsTitleFullMask_) {
        gpsTitleFullMask_ = static_cast<uint8_t *>(std::malloc(HermesXGpsTitleMaskCapacity));
    }
    if (!gpsTitleLayerMap_) {
        gpsTitleLayerMap_ = static_cast<uint8_t *>(std::malloc(HermesXGpsTitleMaskCapacity));
    }

    const HermesXNeonWorkspaceStatus currentStatus = status();
    return currentStatus.glyphCache && currentStatus.sharedMap && currentStatus.clockMasks &&
           currentStatus.gpsTitleMasks;
}

HermesXNeonClockGlyphCache *HermesXNeonWorkspace::getOrBuildClockGlyph(
    size_t glyphIndex, const HermesXNeonClockGlyphSource &source, bool &built)
{
    built = false;
    if (glyphIndex >= HermesXNeonClockGlyphCacheCount || !source.bitmap || source.width == 0 ||
        source.width > HermesXHomeUiRenderer::NeonClockGlyphMaxWidth || source.height == 0 ||
        source.height > HermesXHomeUiRenderer::NeonClockGlyphHeight || source.bytesPerRow == 0 || !glyphCaches_ ||
        !clockFullMask_ || !clockCoreMask_) {
        return nullptr;
    }

    auto &cache = glyphCaches_[glyphIndex];
    if (cache.valid) {
        return &cache;
    }

    constexpr size_t GlyphMaskBytes =
        static_cast<size_t>(HermesXNeonClockGlyphTileWidth) * HermesXNeonClockGlyphTileHeight;
    std::memset(clockFullMask_, 0, GlyphMaskBytes);
    std::memset(clockCoreMask_, 0, GlyphMaskBytes);
    std::memset(cache.layerMap, 0, sizeof(cache.layerMap));
    stampClockGlyphMask(clockFullMask_, source);

    for (int16_t y = 0; y < HermesXNeonClockGlyphTileHeight; ++y) {
        for (int16_t x = 0; x < HermesXNeonClockGlyphTileWidth; ++x) {
            const int16_t index = (y * HermesXNeonClockGlyphTileWidth) + x;
            if (!clockFullMask_[index]) {
                continue;
            }
            if (HermesXNeonMask::isInteriorPixel(
                    clockFullMask_, HermesXNeonClockGlyphTileWidth, HermesXNeonClockGlyphTileHeight, x, y)) {
                clockCoreMask_[index] = 1;
            }
        }
    }

    for (int16_t y = 0; y < HermesXNeonClockGlyphTileHeight; ++y) {
        for (int16_t x = 0; x < HermesXNeonClockGlyphTileWidth; ++x) {
            const int16_t index = (y * HermesXNeonClockGlyphTileWidth) + x;
            uint8_t value = 0;
            if (clockCoreMask_[index]) {
                value = 7;
            } else if (clockFullMask_[index]) {
                value = 6;
            } else if (HermesXNeonMask::hasPixelInRadius(
                           clockFullMask_, HermesXNeonClockGlyphTileWidth, HermesXNeonClockGlyphTileHeight, x, y, 1)) {
                value = 5;
            } else if (HermesXNeonMask::hasPixelInRadius(
                           clockFullMask_, HermesXNeonClockGlyphTileWidth, HermesXNeonClockGlyphTileHeight, x, y, 2)) {
                value = 4;
            } else if (HermesXNeonMask::hasPixelInRadius(
                           clockFullMask_, HermesXNeonClockGlyphTileWidth, HermesXNeonClockGlyphTileHeight, x, y, 3)) {
                value = 3;
            } else if (HermesXNeonMask::hasPixelInRadius(
                           clockFullMask_, HermesXNeonClockGlyphTileWidth, HermesXNeonClockGlyphTileHeight, x, y, 4)) {
                value = 2;
            } else if (HermesXNeonMask::hasPixelInRadius(clockFullMask_,
                                                         HermesXNeonClockGlyphTileWidth,
                                                         HermesXNeonClockGlyphTileHeight,
                                                         x,
                                                         y,
                                                         HermesXHomeUiRenderer::NeonClockGlowRadiusOuter)) {
                value = 1;
            }
            cache.layerMap[index] = value;
        }
    }

    cache.valid = true;
    cache.ch = source.ch;
    cache.coreW = source.width;
    cache.tileW = source.width + (HermesXHomeUiRenderer::NeonClockMargin * 2);
    cache.tileH = HermesXNeonClockGlyphTileHeight;
    ++glyphCacheBuildCount_;
    built = true;
    return &cache;
}


int16_t HermesXNeonWorkspace::measureTextWidth(const char *text,
                                              bool halfScale,
                                              int16_t tracking,
                                              HermesXNeonGlyphWidthProvider widthProvider)
{
    if (!text || !widthProvider) {
        return 0;
    }
    int16_t width = 0;
    bool hasGlyph = false;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        const uint8_t glyphWidth = widthProvider(*cursor);
        if (!glyphWidth) {
            continue;
        }
        if (hasGlyph) {
            width += tracking;
        }
        width += halfScale ? static_cast<int16_t>((glyphWidth + 1) / 2) : glyphWidth;
        hasGlyph = true;
    }
    return width;
}

uint8_t *HermesXNeonWorkspace::composeTextLayerMap(const char *text,
                                                  int16_t drawX,
                                                  int16_t drawY,
                                                  bool halfScale,
                                                  int16_t tracking,
                                                  int16_t marginOverride,
                                                  HermesXNeonGlyphWidthProvider widthProvider,
                                                  HermesXNeonClockGlyphProvider glyphProvider,
                                                  HermesXNeonTextComposition &composition)
{
    if (!text || !*text || !widthProvider || !glyphProvider || !sharedMap_) {
        return nullptr;
    }

    const int16_t coreWidth = measureTextWidth(text, halfScale, tracking, widthProvider);
    if (coreWidth <= 0) {
        return nullptr;
    }
    const int16_t coreHeight = halfScale ? static_cast<int16_t>((HermesXHomeUiRenderer::NeonClockGlyphHeight + 1) / 2)
                                         : HermesXHomeUiRenderer::NeonClockGlyphHeight;
    int16_t margin = halfScale ? static_cast<int16_t>((HermesXHomeUiRenderer::NeonClockMargin + 1) / 2)
                               : HermesXHomeUiRenderer::NeonClockMargin;
    if (marginOverride >= 0 && marginOverride < margin) {
        margin = marginOverride;
    }

    composition.drawX = drawX;
    composition.drawY = drawY;
    composition.regionX = drawX - margin;
    composition.regionY = drawY - margin;
    composition.regionWidth = coreWidth + (margin * 2);
    composition.regionHeight = coreHeight + (margin * 2);
    composition.margin = margin;
    composition.tracking = tracking;
    composition.halfScale = halfScale;
    if (composition.regionWidth <= 0 || composition.regionHeight <= 0 ||
        composition.regionWidth > HermesXNeonTextMaxWidth || composition.regionHeight > HermesXNeonTextMaxHeight) {
        return nullptr;
    }

    std::memset(sharedMap_, 0, static_cast<size_t>(composition.regionWidth) * composition.regionHeight);
    int16_t cursorX = composition.drawX;
    bool hasGlyph = false;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        auto *cache = glyphProvider(*cursor);
        if (!cache) {
            continue;
        }
        if (hasGlyph) {
            cursorX += composition.tracking;
        }

        const int16_t glyphX = cursorX - composition.margin;
        const int16_t glyphY = composition.drawY - composition.margin;
        for (int16_t row = 0; row < cache->tileH; ++row) {
            const int16_t pixelY = glyphY + (composition.halfScale ? (row / 2) : row);
            if (pixelY < composition.regionY || pixelY >= (composition.regionY + composition.regionHeight)) {
                continue;
            }
            for (int16_t column = 0; column < cache->tileW; ++column) {
                const uint8_t value = cache->layerMap[(row * HermesXNeonClockGlyphTileWidth) + column];
                if (!value) {
                    continue;
                }

                const int16_t pixelX = glyphX + (composition.halfScale ? (column / 2) : column);
                if (pixelX < composition.regionX || pixelX >= (composition.regionX + composition.regionWidth)) {
                    continue;
                }

                const int16_t index = ((pixelY - composition.regionY) * composition.regionWidth) +
                                      (pixelX - composition.regionX);
                if (value > sharedMap_[index]) {
                    sharedMap_[index] = value;
                }
            }
        }

        cursorX += composition.halfScale ? static_cast<int16_t>((cache->coreW + 1) / 2) : cache->coreW;
        hasGlyph = true;
    }
    return sharedMap_;
}


bool HermesXNeonWorkspace::visitSharedMapPaintRuns(int16_t regionX,
                                                   int16_t regionY,
                                                   int16_t regionWidth,
                                                   int16_t regionHeight,
                                                   HermesXNeonRunVisitor visitor,
                                                   void *context) const
{
    if (!visitor || !sharedMap_ || regionWidth <= 0 || regionHeight <= 0 || regionWidth > HermesXNeonSharedMaxWidth ||
        regionHeight > HermesXNeonSharedMaxHeight) {
        return false;
    }
    visitLayerMapPaintRuns(
        sharedMap_, nullptr, regionX, regionY, regionWidth, regionHeight, true, false, visitor, context);
    return true;
}


void HermesXNeonWorkspace::release()
{
    releaseBuffer(glyphCaches_);
    releaseBuffer(sharedMap_);
    releaseBuffer(clockFullMask_);
    releaseBuffer(clockCoreMask_);
    releaseBuffer(gpsTitleFullMask_);
    releaseBuffer(gpsTitleLayerMap_);

    glyphCacheBuildCount_ = 0;
}

HermesXNeonWorkspaceStatus HermesXNeonWorkspace::status() const
{
    HermesXNeonWorkspaceStatus currentStatus;
    currentStatus.glyphCache = glyphCaches_ != nullptr;
    currentStatus.sharedMap = sharedMap_ != nullptr;
    currentStatus.clockMasks = clockFullMask_ != nullptr && clockCoreMask_ != nullptr;
    currentStatus.gpsTitleMasks = gpsTitleFullMask_ != nullptr && gpsTitleLayerMap_ != nullptr;
    return currentStatus;
}

} // namespace graphics
