#pragma once
#include <Arduino.h>

struct ToneNote {
    float freq;
    uint16_t duration;
};

class MusicModule {
public:
    MusicModule(uint8_t buzzerPin);

    void begin();
    void stopTone();
    void playTone(float freq, uint32_t duration);
    void playMelody(const ToneNote* melody);

    void playStartupSound();
    void playReceiveSound();
    void playSendSound();
    void playFailedSound();
    void playSuccessSound();
    void playNodeInfoSound();

private:
    uint8_t pin;
    void _playMelodyStep(const ToneNote* melody, int index);
};