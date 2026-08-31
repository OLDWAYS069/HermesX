#pragma once

#include "HermesXNeonWorkspace.h"

#include <cstdint>

class TFTDisplay;

namespace graphics
{

using HermesXNeonLayerColorMapper = uint16_t (*)(uint8_t layer);

struct HermesXNeonTextRenderResult {
    bool composed = false;
    bool rendered = false;
    HermesXNeonTextComposition composition;
};

class HermesXNeonRenderer
{
  public:
    static HermesXNeonTextRenderResult renderText(TFTDisplay *tft,
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
                                                  HermesXNeonLayerColorMapper layerColor);

  private:
    static bool paintText(TFTDisplay *tft,
                          HermesXNeonWorkspace &workspace,
                          const HermesXNeonTextComposition &composition,
                          bool clearBackground,
                          HermesXNeonLayerColorMapper layerColor);
};

} // namespace graphics
