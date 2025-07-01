//負責唱歌的模組，老實說我只是無聊加的

#pragma once
#include <Arduino.h>          
#include "TinyScheduler.h"

struct ToneNote {
    float freq;
    uint16_t duration;
};

class MusicModule {
public:
    MusicModule(uint8_t buzzerPin, TinyScheduler& scheduler);

    void begin();
    void stopTone();
    void playTone(float freq, uint32_t duration);
    void playMelody(const ToneNote* melody);

    
    void playStartupSound();
    void playReceiveSound();
    void playSendSound();
    void playEasteregg();

private:
    uint8_t pin;
    TinyScheduler& scheduler;
    void _playMelodyStep(const ToneNote* melody, int index);
};

