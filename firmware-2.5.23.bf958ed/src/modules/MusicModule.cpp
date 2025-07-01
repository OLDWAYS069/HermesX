#include "MusicModule.h"
//蜂鳴器定義用
#define NOTE_C4 261.63
#define NOTE_E5 659.25
#define NOTE_A5 880.00
#define NOTE_G4 392.00
#define NOTE_C6 1046.50
#define NOTE_FS5 739.99


void MusicModule::begin() {
    ledcAttachPin(pin, 0);        
    ledcSetup(0, 2000, 8);        
}


void MusicModule::stopTone() {
    ledcWriteTone(0, 0); 
}

void MusicModule::playTone(float freq, uint32_t duration) {
    if (freq > 0) {
        ledcWriteTone(0, freq);
        scheduler.timeout(duration, [this]() {
            stopTone();
        });
    }
}

void MusicModule::playMelody(const ToneNote* melody) {
    _playMelodyStep(melody, 0);  
}

void MusicModule::_playMelodyStep(const ToneNote* melody, int index) {
    if (melody[index].freq == 0) {
        stopTone();
        return;
    }

    playTone(melody[index].freq, melody[index].duration);
    scheduler.timeout(melody[index].duration, [this, melody, index]() {
        _playMelodyStep(melody, index + 1);
    });
}

//預設音效 

static const ToneNote startupMelody[] = {
    {NOTE_C4, 150},
    {NOTE_G4, 150},
    {0, 0}
};

static const ToneNote receiveMelody[] = {
    {NOTE_E5, 120},
    {NOTE_C6, 120},
    {0, 0}
};

static const ToneNote sendMelody[] = {
    {NOTE_A5, 100},
    {NOTE_FS5, 100},
    {0, 0}
};

void MusicModule::playStartupSound() {
    playMelody(startupMelody);
}

void MusicModule::playReceiveSound() {
    playMelody(receiveMelody);
}

void MusicModule::playSendSound() {
    playMelody(sendMelody);
}