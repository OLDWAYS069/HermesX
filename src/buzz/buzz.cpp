#include "buzz.h"
#include "NodeDB.h"
#include "configuration.h"

#if !defined(ARCH_ESP32) && !defined(ARCH_RP2040) && !defined(ARCH_PORTDUINO)
#include "Tone.h"
#endif

#if !defined(ARCH_PORTDUINO)
extern "C" void delay(uint32_t dwMs);
#endif

struct ToneDuration {
    int frequency_khz;
    int duration_ms;
};

// Some common frequencies.
#define NOTE_C3 131
#define NOTE_CS3 139
#define NOTE_D3 147
#define NOTE_DS3 156
#define NOTE_E3 165
#define NOTE_F3 175
#define NOTE_FS3 185
#define NOTE_G3 196
#define NOTE_GS3 208
#define NOTE_A3 220
#define NOTE_AS3 233
#define NOTE_B3 247
#define NOTE_CS4 277
#define NOTE_E4 330
#define NOTE_G4 392

const int DURATION_1_8 = 125;  // 1/8 note
const int DURATION_1_4 = 250;  // 1/4 note
const int DURATION_1_2 = 500;  // 1/2 note
const int DURATION_3_4 = 750;  // 1/4 note
const int DURATION_1_1 = 1000; // 1/1 note

bool isBuzzerGloballyEnabled()
{
    return config.device.buzzer_mode != meshtastic_Config_DeviceConfig_BuzzerMode_DISABLED;
}

void setGlobalBuzzerEnabled(bool enabled)
{
    config.device.buzzer_mode = enabled ? meshtastic_Config_DeviceConfig_BuzzerMode_ALL_ENABLED
                                        : meshtastic_Config_DeviceConfig_BuzzerMode_DISABLED;
    if (!enabled) {
        if (config.device.buzzer_gpio) {
            noTone(config.device.buzzer_gpio);
        }
#ifdef PIN_BUZZER
        if (PIN_BUZZER && PIN_BUZZER != config.device.buzzer_gpio) {
            noTone(PIN_BUZZER);
        }
#endif
    }
}

void playTones(const ToneDuration *tone_durations, int size)
{
    if (!isBuzzerGloballyEnabled()) {
        return;
    }

#ifdef PIN_BUZZER
    if (!config.device.buzzer_gpio)
        config.device.buzzer_gpio = PIN_BUZZER;
#endif
    if (config.device.buzzer_gpio) {
        for (int i = 0; i < size; i++) {
            const auto &tone_duration = tone_durations[i];
            tone(config.device.buzzer_gpio, tone_duration.frequency_khz, tone_duration.duration_ms);
            // to distinguish the notes, set a minimum time between them.
            delay(1.3 * tone_duration.duration_ms);
        }
    }
}

void playBeep()
{
    ToneDuration melody[] = {{NOTE_B3, DURATION_1_4}};
    playTones(melody, sizeof(melody) / sizeof(ToneDuration));
}

void playLongBeep()
{
    ToneDuration melody[] = {{NOTE_B3, DURATION_1_1}};
    playTones(melody, sizeof(melody) / sizeof(ToneDuration));
}

void playGPSEnableBeep()
{
    ToneDuration melody[] = {{NOTE_C3, DURATION_1_8}, {NOTE_FS3, DURATION_1_4}, {NOTE_CS4, DURATION_1_4}};
    playTones(melody, sizeof(melody) / sizeof(ToneDuration));
}

void playGPSDisableBeep()
{
    ToneDuration melody[] = {{NOTE_CS4, DURATION_1_8}, {NOTE_FS3, DURATION_1_4}, {NOTE_C3, DURATION_1_4}};
    playTones(melody, sizeof(melody) / sizeof(ToneDuration));
}

void playStartMelody()
{
    ToneDuration melody[] = {{NOTE_FS3, DURATION_1_8}, {NOTE_AS3, DURATION_1_8}, {NOTE_CS4, DURATION_1_4}};
    playTones(melody, sizeof(melody) / sizeof(ToneDuration));
}

void playShutdownMelody()
{
    ToneDuration melody[] = {{NOTE_CS4, DURATION_1_8}, {NOTE_AS3, DURATION_1_8}, {NOTE_FS3, DURATION_1_4}};
    playTones(melody, sizeof(melody) / sizeof(ToneDuration));
}

void playLowMemoryAlert()
{
    ToneDuration melody[] = {
        {NOTE_E4, 120},
        {NOTE_G4, 120},
        {NOTE_E4, 160},
    };
    playTones(melody, sizeof(melody) / sizeof(ToneDuration));
}
