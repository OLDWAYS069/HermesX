#pragma once

#include <stdint.h>
#include <stddef.h>

namespace HermesEmergencyUi {

struct FaceFrame {
    const char *text;
    uint16_t color;
    uint16_t durationMs;
};

struct Animation {
    const FaceFrame *frames;
    size_t count;
};

void setup();
void onEmergencyModeChanged(bool active, bool drillMode);
void onAck(bool success);
void onSend();
void onReceive();

bool currentFace(const char *&text, uint16_t &color);

} // namespace HermesEmergencyUi
