#pragma once
#include "SinglePortModule.h" 
#include <Adafruit_GFX.h>
#include <Adafruit_NeoPixel.h>
#include "meshtastic/mesh.pb.h"
#include "MusicModule.h"
#include "concurrency/OSThread.h"

typedef void (*HermesXFeedbackCallback)(int index, bool state);

extern HermesXFeedbackCallback hermesXCallback;

struct LedTheme {
    uint32_t colorSendPrimary;
    uint32_t colorSendSecondary;
    uint32_t colorReceivePrimary;
    uint32_t colorReceiveSecondary;
    uint32_t colorAck;
    uint32_t colorFailed;
    uint32_t colorIdleBreathBase;
    float breathBrightnessMin;
    float breathBrightnessMax;
};

class HermesXInterfaceModule : public SinglePortModule, public concurrency::OSThread, public Observer<uint32_t> {

public:
    HermesXInterfaceModule();
    void setup();

    void handleRotary();
    void handleButtonPress();  

    Adafruit_NeoPixel rgb;
    MusicModule music;

    static HermesXInterfaceModule *instance;
    static void onLocalTextMessageSent();

    int onNotify(uint32_t fromNum) override;
    void handleExternalNotification(int index, bool state);

    void playSendFeedback(); 
    void playReceiveFeedback();
    void playSendFailedFeedback();
    void playSendSuccessFeedback();
    void playNodeInfoFeedback();

    void triggerSendPulse(uint32_t color = 0xFF2800);
    void triggerReceivePulse(uint32_t color = 0xFF2800);
    void triggerAckPulse();
    void playStartupLEDAnimation(uint32_t color);

    void setLedTheme(const LedTheme& theme) { currentTheme = theme; }

private:
    void initDisplay();
    void initLED();
    void initRotary();

    void drawFace(const char* face, uint16_t color);
    void updateFace();
    void updateLED();

    bool waitingForAck = false;
    bool ackReceived = false;
    uint32_t waitingAckId = 0;
    uint32_t lastSentTime = 0;
    uint32_t lastSentId = 0;

    void sendText(NodeNum dest, ChannelIndex channel, const char *message, bool wantAck);
    void sendCannedMessage(const char* msg);
    void onPacketSent();
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &packet) override;

    void initBuzzer();
    void playTone(float freq, uint32_t duration_ms);
    void stopTone(); 
    void playStartupTone();
    void playReceiveTone();
    void playSendTone();   
    void playFailedTone();  

    bool wantPacket(const meshtastic_MeshPacket *p) override;
    bool pendingSuccessFeedback = false;
    uint32_t successFeedbackTime = 0;

    enum HermesFaceState { FACE_IDLE, FACE_RECEIVED, FACE_SENT, FACE_ERROR } ;
    HermesFaceState faceState = FACE_IDLE;
    HermesFaceState lastState = FACE_IDLE;

    uint32_t lastBreathUpdate = 0;
    float breathBrightness = 0.0f;
    float breathDelta = 0.1f;

    uint8_t fadeStep = 0;
    uint8_t fadeMaxStep = 20;

    uint32_t lastEventTime = 0;
    bool ledFlashActive = false;
    uint32_t ledFlashColor = 0;

    enum class LedPulseMode {
        NONE,
        SEND,
        RECEIVE,
        ACK,
        FADE_OUT
    };

    LedPulseMode pulseMode = LedPulseMode::NONE;
    uint32_t pulseStartTime = 0;
    uint8_t pulseStep = 0;
    uint32_t pulseInterval = 120;
    uint32_t pulseColor = 0;  // 新增: 存傳的動畫顏色

    uint32_t toneStopTime = 0;

    LedTheme currentTheme {
        .colorSendPrimary = 0xFFFFFF,
        .colorSendSecondary = 0x5050FF,
        .colorReceivePrimary = 0xFFFFFF,
        .colorReceiveSecondary = 0x5050FF,
        .colorAck = 0x00FF00,
        .colorFailed = 0xFF0000,
        .colorIdleBreathBase = 0xFF5000,
        .breathBrightnessMin = 0.5f,
        .breathBrightnessMax = 1.0f
    };

    int32_t runOnce() override;
};

extern HermesXInterfaceModule* globalHermes;