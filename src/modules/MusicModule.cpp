#include "MusicModule.h"

#define NOTE_C4 261
#define NOTE_E5 659
#define NOTE_A5 880
#define NOTE_G4 392
#define NOTE_C6 1046
#define NOTE_FS5 739

MusicModule::MusicModule(uint8_t buzzerPin)
    : pin(buzzerPin) {}

void MusicModule::begin() {
    ledcSetup(3, 2000, 8);
    ledcAttachPin(pin, 3);
}

void MusicModule::stopTone() {
    ledcWriteTone(3, 0);
}

void MusicModule::playTone(float freq, uint32_t duration) {
    if (freq > 0) {
        ledcWriteTone(3, freq);
        // 注意：由外部控制何時呼叫 stopTone()，由 HermesXInterfaceModule 控制延遲時間
    }
}

void MusicModule::playMelody(const ToneNote* melody) {
    int index = 0;
    while (melody[index].freq > 0 && melody[index].duration > 0) {
        playTone(melody[index].freq, melody[index].duration);
        delay(melody[index].duration);
        stopTone();
        delay(20);
        index++;
    }
}

static const ToneNote startupMelody[] = {{NOTE_C4,150},{NOTE_G4,150},{0,0}};
static const ToneNote receiveMelody[] = {{NOTE_E5,120},{NOTE_C6,120},{0,0}};
static const ToneNote sendMelody[] = {{NOTE_A5,100},{NOTE_FS5,100},{0,0}};
static const ToneNote FailedMelody[] = {{NOTE_C6,100},{NOTE_FS5,100},{0,0}};

void MusicModule::playStartupSound() { playMelody(startupMelody); }
void MusicModule::playReceiveSound() { playMelody(receiveMelody); }
void MusicModule::playSendSound() { playMelody(sendMelody); }
void MusicModule::playFailedSound() { playMelody(FailedMelody); }