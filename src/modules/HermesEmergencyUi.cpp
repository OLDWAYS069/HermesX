#include "HermesEmergencyUi.h"
#include <Arduino.h>

namespace HermesEmergencyUi {
namespace {
bool emergencyActive = false;
bool drillModeActive = false;
Animation currentAnimation{};
size_t frameIndex = 0;
uint32_t nextFrameAt = 0;

FaceFrame neutralFrame{" :| ", 0xFFFF, 1000};
Animation neutralAnimation{&neutralFrame, 1};

const FaceFrame sendFrames[] = {
    {">_>", 0xFFFF, 160},
    {">.>", 0xFFFF, 160},
    {">o>", 0xFFFF, 160},
    {">.>", 0xFFFF, 160},
};
Animation sendAnimation{sendFrames, sizeof(sendFrames) / sizeof(sendFrames[0])};

const FaceFrame receiveFrames[] = {
    {"<_<", 0xFFFF, 160},
    {"<.<", 0xFFFF, 160},
    {"<o<", 0xFFFF, 160},
    {"<.<", 0xFFFF, 160},
};
Animation recvAnimation{receiveFrames, sizeof(receiveFrames) / sizeof(receiveFrames[0])};

const FaceFrame ackFrames[] = {
    {"^_^", 0x07E0, 220},
    {"^o^", 0x07E0, 220},
};
Animation ackAnimation{ackFrames, sizeof(ackFrames) / sizeof(ackFrames[0])};

const FaceFrame nackFrames[] = {
    {">_<", 0xF800, 260},
    {"x_x", 0xF800, 260},
};
Animation nackAnimation{nackFrames, sizeof(nackFrames) / sizeof(nackFrames[0])};

void resetAnimation(const Animation &anim)
{
    currentAnimation = anim;
    frameIndex = 0;
    const uint16_t duration = currentAnimation.count ? currentAnimation.frames[0].durationMs : 0;
    nextFrameAt = duration ? millis() + duration : 0;
}

void advanceAnimation()
{
    if (!currentAnimation.count || !currentAnimation.frames)
        return;

    if (currentAnimation.count == 1)
        return;

    const uint32_t now = millis();
    if (nextFrameAt && static_cast<int32_t>(now - nextFrameAt) >= 0) {
        frameIndex = (frameIndex + 1) % currentAnimation.count;
        const uint16_t duration = currentAnimation.frames[frameIndex].durationMs;
        nextFrameAt = duration ? now + duration : 0;
    }
}

} // namespace

void setup()
{
    resetAnimation(neutralAnimation);
}

void onEmergencyModeChanged(bool active, bool drill)
{
    emergencyActive = active;
    drillModeActive = drill;
    resetAnimation(active ? sendAnimation : neutralAnimation);
}

void onAck(bool success)
{
    resetAnimation(success ? ackAnimation : nackAnimation);
}

void onSend()
{
    resetAnimation(sendAnimation);
}

void onReceive()
{
    resetAnimation(recvAnimation);
}

bool currentFace(const char *&text, uint16_t &color)
{
    advanceAnimation();
    if (!currentAnimation.count || !currentAnimation.frames) {
        text = nullptr;
        color = 0;
        return false;
    }
    const FaceFrame &frame = currentAnimation.frames[frameIndex];
    text = frame.text;
    color = frame.color;
    return true;
}

} // namespace HermesEmergencyUi
