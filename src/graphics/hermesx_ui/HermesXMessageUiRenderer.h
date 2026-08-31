#pragma once

#include "mesh/generated/meshtastic/mesh.pb.h"
#include "mesh/generated/meshtastic/deviceonly.pb.h"
#include <OLEDDisplay.h>
#include <OLEDDisplayUi.h>
#include <string>
#include <vector>

namespace graphics
{

struct HermesXMessageRenderContext {
    bool inverted = false;
    meshtastic_NodeInfoLite *(*resolveNode)(const meshtastic_MeshPacket &packet) = nullptr;
    const char *(*channelName)(uint8_t channel) = nullptr;
    bool (*drawEmoji)(OLEDDisplay *display,
                      int16_t x,
                      int16_t y,
                      int16_t width,
                      int16_t height,
                      const char *payload) = nullptr;
};

class HermesXMessageUiRenderer
{
  public:
    static uint8_t bodyHanziPixelSize();
    static void drawRecentList(OLEDDisplay *display,
                               OLEDDisplayUiState *state,
                               int16_t x,
                               int16_t y,
                               const HermesXMessageRenderContext &context);
    static void drawIncomingPopup(OLEDDisplay *display,
                                  OLEDDisplayUiState *state,
                                  const HermesXMessageRenderContext &context);
    static void drawDetail(OLEDDisplay *display,
                           OLEDDisplayUiState *state,
                           int16_t x,
                           int16_t y,
                           const meshtastic_MeshPacket *packet,
                           const char *timeLabel,
                           const HermesXMessageRenderContext &context);
    static void drawComposerCandidates(OLEDDisplay *display,
                                       int16_t width,
                                       int16_t height,
                                       const String &preview,
                                       const std::vector<std::string> &candidates,
                                       uint8_t candidateCursor);
};

} // namespace graphics
