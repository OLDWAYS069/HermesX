#pragma once

#include <stdint.h>

class OLEDDisplay;

enum class HermesFaceMode : uint8_t {
    Neutral,
    Sending,
    Receiving,
    AckSuccess,
    AckFailed,
    Disabled,
    Thinking,
};

struct HermesFaceRenderContext {
    OLEDDisplay *display;
    int16_t originX;
    int16_t originY;
    HermesFaceMode mode;
};

bool HermesX_TryGetFaceRenderContext(HermesFaceRenderContext &ctx) __attribute__((weak));

__attribute__((weak)) void HermesX_DrawFace(OLEDDisplay *display, int16_t x, int16_t y, HermesFaceMode mode);
