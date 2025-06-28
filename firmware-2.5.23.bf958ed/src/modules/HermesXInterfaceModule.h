#pragma once
#include "mesh/MeshModule.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_NeoPixel.h>
#include "meshtastic/mesh.pb.h"
#include "TinyScheduler.h"


class HermesXInterfaceModule : public MeshModule {
public:
    HermesXInterfaceModule();
    void setup();
    ProcessMessage handleReceived(const meshtastic_MeshPacket &packet);
    void handleRotary();
    void handleButtonPress();  
    TinyScheduler scheduler = TinyScheduler::millis();
    // ✅ ← 加這行        

private:
    void initDisplay();
    void initLED();
    void initRotary();
    void initBuzzer();
    void drawFace(const char* face, uint16_t color);
    void updateFace();
    void updateLED();
    void playBuzzer();
    void sendCannedMessage(const char* msg);
    void onPacketSent() ;
    void onPacketFailed() ;
    bool handleRadioPacket(meshtastic_MeshPacket* p) ;
  

    SPIClass spi_st7789;
    Adafruit_ST7789 tft;
    Adafruit_NeoPixel rgb;

    enum HermesFaceState { FACE_IDLE, FACE_RECEIVED, FACE_SENT,FACE_ERROR };
    HermesFaceState faceState = FACE_IDLE;
    HermesFaceState lastState = FACE_IDLE;

    uint32_t lastEventTime = 0;
    bool ledFlashActive = false;
    uint32_t ledFlashColor = 0;
};


