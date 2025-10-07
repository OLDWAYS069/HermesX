#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_HERMESX

#include "HermesFace.h"
#include "HermesXInterfaceModule.h"
#include "HermesXLog.h"
#include "graphics/ScreenFonts.h"

#include <Arduino.h>

namespace {
constexpr uint16_t kColorWhite = 0xFFFF;
constexpr uint16_t kColorGreen = 0x07E0;
constexpr uint16_t kColorRed = 0xF800;
constexpr uint16_t kColorGray = 0x8410;
constexpr uint16_t kColorYellow = 0xFFE0;

struct FaceAnimFrame {
    const char *text;
    uint16_t color;
    uint16_t durationMs;
};

struct FaceAnimation {
    const FaceAnimFrame *frames;
    size_t frameCount;
};

struct FaceContextCache {
    OLEDDisplay *display = nullptr;
    int16_t originX = 0;
    int16_t originY = 0;
    HermesFaceMode mode = HermesFaceMode::Neutral;
    bool valid = false;
};

struct AnimationState {
    HermesFaceMode mode = HermesFaceMode::Neutral;
    size_t frameIndex = 0;
    uint32_t nextFrameAt = 0;
};

constexpr FaceAnimFrame kNeutralFrames[] = {
    {":|", kColorWhite, 1000},
};

constexpr FaceAnimFrame kSendingFrames[] = {
    {">_>", kColorWhite, 160},
    {">.>", kColorWhite, 160},
    {">o>", kColorWhite, 160},
    {">.>", kColorWhite, 160},
};

constexpr FaceAnimFrame kReceivingFrames[] = {
    {"<_<", kColorWhite, 160},
    {"<.<", kColorWhite, 160},
    {"<o<", kColorWhite, 160},
    {"<.<", kColorWhite, 160},
};

constexpr FaceAnimFrame kAckSuccessFrames[] = {
    {"^_^", kColorGreen, 220},
    {"^o^", kColorGreen, 220},
};

constexpr FaceAnimFrame kAckFailedFrames[] = {
    {">_<", kColorRed, 260},
    {"x_x", kColorRed, 260},
};

constexpr FaceAnimFrame kDisabledFrames[] = {
    {"-_-", kColorGray, 400},
    {"z_z", kColorGray, 400},
};

constexpr FaceAnimFrame kThinkingFrames[] = {
    {"o_o", kColorYellow, 180},
    {"o.O", kColorYellow, 180},
    {"o_o", kColorYellow, 180},
    {"o.o", kColorYellow, 180},
};

constexpr FaceAnimation kNeutralAnimation{ kNeutralFrames, sizeof(kNeutralFrames) / sizeof(kNeutralFrames[0]) };
constexpr FaceAnimation kSendingAnimation{ kSendingFrames, sizeof(kSendingFrames) / sizeof(kSendingFrames[0]) };
constexpr FaceAnimation kReceivingAnimation{ kReceivingFrames, sizeof(kReceivingFrames) / sizeof(kReceivingFrames[0]) };
constexpr FaceAnimation kAckSuccessAnimation{ kAckSuccessFrames, sizeof(kAckSuccessFrames) / sizeof(kAckSuccessFrames[0]) };
constexpr FaceAnimation kAckFailedAnimation{ kAckFailedFrames, sizeof(kAckFailedFrames) / sizeof(kAckFailedFrames[0]) };
constexpr FaceAnimation kDisabledAnimation{ kDisabledFrames, sizeof(kDisabledFrames) / sizeof(kDisabledFrames[0]) };
constexpr FaceAnimation kThinkingAnimation{ kThinkingFrames, sizeof(kThinkingFrames) / sizeof(kThinkingFrames[0]) };

FaceContextCache g_faceContext;
AnimationState g_animationState;

const FaceAnimation &selectAnimation(HermesFaceMode mode)
{
    switch (mode) {
    case HermesFaceMode::Sending:
        return kSendingAnimation;
    case HermesFaceMode::Receiving:
        return kReceivingAnimation;
    case HermesFaceMode::AckSuccess:
        return kAckSuccessAnimation;
    case HermesFaceMode::AckFailed:
        return kAckFailedAnimation;
    case HermesFaceMode::Disabled:
        return kDisabledAnimation;
    case HermesFaceMode::Thinking:
        return kThinkingAnimation;
    case HermesFaceMode::Neutral:
    default:
        return kNeutralAnimation;
    }
}
}

bool HermesX_TryGetFaceRenderContext(HermesFaceRenderContext &ctx)
{
    if (!g_faceContext.valid) {
        return false;
    }

    ctx.display = g_faceContext.display;
    ctx.originX = g_faceContext.originX;
    ctx.originY = g_faceContext.originY;
    ctx.mode = g_faceContext.mode;

    return true;
}

void HermesX_DrawFace(OLEDDisplay *display, int16_t x, int16_t y, HermesFaceMode mode)
{
    if (HermesXInterfaceModule::instance == nullptr) {
        return;
    }

    const FaceAnimation &animation = selectAnimation(mode);
    if (animation.frameCount == 0 || animation.frames == nullptr) {
        return;
    }

    const uint32_t now = millis();
    if (g_animationState.mode != mode) {
        g_animationState.mode = mode;
        g_animationState.frameIndex = 0;
        const uint16_t duration = animation.frames[0].durationMs;
        g_animationState.nextFrameAt = duration ? now + duration : 0;
    } else if (animation.frameCount > 1 && g_animationState.nextFrameAt != 0) {
        if (static_cast<int32_t>(now - g_animationState.nextFrameAt) >= 0) {
            g_animationState.frameIndex = (g_animationState.frameIndex + 1) % animation.frameCount;
            const uint16_t duration = animation.frames[g_animationState.frameIndex].durationMs;
            g_animationState.nextFrameAt = duration ? now + duration : 0;
        }
    }

    const FaceAnimFrame &frame = animation.frames[g_animationState.frameIndex];

    g_faceContext.display = display;
    g_faceContext.originX = x;
    g_faceContext.originY = y;
    g_faceContext.mode = mode;
    g_faceContext.valid = true;

    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->drawFace(frame.text, frame.color);
    }

    static HermesFaceMode lastLoggedMode = HermesFaceMode::Neutral;
    if (lastLoggedMode != mode) {
        HERMESX_LOG_INFO("HermesFaceAdapter drawFace mode=%d", static_cast<int>(mode));
        lastLoggedMode = mode;
    }

    g_faceContext.valid = false;
}

#endif // !MESHTASTIC_EXCLUDE_HERMESX
