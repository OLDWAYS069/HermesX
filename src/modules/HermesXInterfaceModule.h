#pragma once
#include "SinglePortModule.h" 
#include <Adafruit_GFX.h>
#include <Adafruit_NeoPixel.h>
#include "meshtastic/mesh.pb.h"
#include "MusicModule.h"
#include "concurrency/OSThread.h"



class HermesXInterfaceModule : public SinglePortModule, public concurrency::OSThread, public Observer<uint32_t> {

public:
    HermesXInterfaceModule();
    void setup();

    void handleRotary();
    void handleButtonPress();  

    Adafruit_NeoPixel rgb;
    MusicModule music;

    static HermesXInterfaceModule *instance;  // 全域存取點
    static void onLocalTextMessageSent();     // 通知用的靜態方法

    int onNotify(uint32_t fromNum) override;



private:
    void initDisplay();
    void initLED();
    void initRotary();

    void drawFace(const char* face, uint16_t color);
    void updateFace();
    void updateLED();

    void playSendFeedback(); 
    void playReceiveFeedback();
    void playSendFailedFeedback();

    bool waitingForAck = false;
 
    bool ackReceived = false;
    uint32_t waitingAckId = 0;
    uint32_t lastSentTime = 0;
    
    void sendText(NodeNum dest, ChannelIndex channel, const char *message, bool wantAck);



    void sendCannedMessage(const char* msg);
    void onPacketSent();
    void onPacketFailed();
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &packet) override;


    void initBuzzer();
    void playTone(float freq, uint32_t duration_ms);
    void stopTone(); 
    void playStartupTone();
    void playReceiveTone();
    void playSendTone();   
    void playFailedTone();  

    bool wantPacket(const meshtastic_MeshPacket *p) override;

    enum HermesFaceState { FACE_IDLE, FACE_RECEIVED, FACE_SENT, FACE_ERROR } ;
    HermesFaceState faceState = FACE_IDLE;
    HermesFaceState lastState = FACE_IDLE;

    uint32_t lastEventTime = 0;
    bool ledFlashActive = false;
    uint32_t ledFlashColor = 0;

    uint32_t toneStopTime = 0;

    int32_t runOnce() override;
};

extern HermesXInterfaceModule* globalHermes;
