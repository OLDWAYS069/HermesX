#include "MusicModule.h"

#define NOTE_CS5 554  // C#5 / Db5
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_FS5 739
#define NOTE_G5  784
#define NOTE_GS5 830
#define NOTE_A5  880
#define NOTE_B5  988
#define NOTE_C6  1046
#define NOTE_CS6 1108
#define NOTE_E6  1318
#define NOTE_C4 261
#define NOTE_G4 392
#define NOTE_A5 880
#define NOTE_C6 1046
#define NOTE_FS5 739
#define NOTE_E5 659
#define NOTE_E4  330
#define NOTE_GS4 415
#define NOTE_B4  494

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

static const ToneNote startupMelody[] = {
    {NOTE_E4, 100},
    {NOTE_GS4, 100},
    {NOTE_B4, 150},
    {0, 0}
};

//  接收音：柔和下降，像通知
static const ToneNote receiveMelody[] = {
    {NOTE_E6, 80},
    {NOTE_CS6, 80},
    {NOTE_A5, 100},
    {0, 0}
};

//  傳送音：快速上升，像「啾！」
static const ToneNote sendMelody[] = {
    {NOTE_GS5, 80},
    {NOTE_B5, 100},
    {0, 0}
};

// 失敗音：短促下降，像「嘟」
static const ToneNote FailedMelody[] = {
    {NOTE_E5, 80},
    {NOTE_CS5, 100},
    {0, 0}
};

//  成功音：明亮上升，像「叮噠」
static const ToneNote SuccessSound[] = {
    {NOTE_D5, 100},
    {NOTE_G5, 120},
    {0, 0}
};

//  Node Info 音：提示感，帶一點科技味
static const ToneNote InfoMelody[] = {
    {NOTE_A5, 80},
    {NOTE_E6, 100},
    {0, 0}
};

void MusicModule::playStartupSound() { playMelody(startupMelody); }
void MusicModule::playReceiveSound() { playMelody(receiveMelody); }
void MusicModule::playSendSound() { playMelody(sendMelody); }
void MusicModule::playFailedSound() { playMelody(FailedMelody); }
void MusicModule::playSuccessSound(){ playMelody(SuccessSound);}
void MusicModule::playNodeInfoSound(){playMelody(InfoMelody);}