// HermesXInterfaceModule.cpp - Refactored without TinyScheduler

#include "HermesXInterfaceModule.h"
#include "mesh/MeshService.h"
#include "mesh/NodeDB.h"
#include "mesh/Channels.h"
#include "mesh/Router.h"
#include "meshtastic/mesh.pb.h"
#include "modules/CannedMessageModule.h"
#include "SinglePortModule.h"
#include "pb.h"
#include "pb_encode.h"
#include "HermesXPacketUtils.h"

#include "MusicModule.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Arduino.h>
#include "RedirectablePrint.h"
#include "DebugConfiguration.h"
#include "meshtastic/portnums.pb.h"
#include "HermesXLog.h"

#include "ReliableRouter.h"
#include "Default.h"
#include "MeshTypes.h"
#include "configuration.h"
#include "mesh-pb-constants.h"
#include "modules/NodeInfoModule.h"
#include "modules/RoutingModule.h"

#include "pb_decode.h"


#define PIN_LED 6
#define NUM_LEDS 8
#define ROTARY_SW 4
#define ROTARY_DT 26
#define ROTARY_CLK 37
#define BUZZER_PIN 17

HermesXInterfaceModule* globalHermes = nullptr;
HermesXInterfaceModule *HermesXInterfaceModule::instance = nullptr;


void IRAM_ATTR rotaryISR() {
    if (globalHermes) globalHermes->handleRotary();
}

HermesXInterfaceModule::HermesXInterfaceModule()
  : SinglePortModule("hermesx", meshtastic_PortNum_PRIVATE_APP),
    OSThread("hermesTask", 500),
    rgb(NUM_LEDS, PIN_LED, NEO_GRB + NEO_KHZ800),
    music(BUZZER_PIN)
{
    globalHermes = this;
    HermesXInterfaceModule::instance = this;
    observe(&service->fromNumChanged);  
        
    isPromiscuous = true;              
           

    initLED();
    initRotary();
 
    HERMESX_LOG_DEBUG("constroct");

    for (int i = 0; i < 2; ++i) {
        rgb.fill(rgb.Color(0, 0, 50));
        rgb.show();
        delay(150);
        rgb.fill(rgb.Color(0, 0, 0));
        rgb.show();
        delay(150);
    }


}

void HermesXInterfaceModule::setup() 
{
    

}

void HermesXInterfaceModule::drawFace(const char* face, uint16_t color) {
    // Empty placeholder
}

void HermesXInterfaceModule::initLED() {
    rgb.begin();
    rgb.setBrightness(60);
    rgb.fill(rgb.Color(0, 0, 20));
    rgb.show();
    HERMESX_LOG_INFO("LED setup\n");
}

void HermesXInterfaceModule::initRotary() {
    pinMode(ROTARY_SW, INPUT_PULLUP);
    pinMode(ROTARY_DT, INPUT);
    pinMode(ROTARY_CLK, INPUT);
    attachInterrupt(digitalPinToInterrupt(ROTARY_DT), rotaryISR, CHANGE);
    HERMESX_LOG_INFO("rotary setup\n");
}

void HermesXInterfaceModule::updateLED() {
    if (ledFlashActive) {
        if ((millis() / 300) % 2 == 0) {
            rgb.fill(ledFlashColor);
        } else {
            rgb.fill(rgb.Color(0, 0, 0));
        }
    } else {
        rgb.fill(rgb.Color(0, 0, 20));
    }
    rgb.show();
}



void HermesXInterfaceModule::onPacketSent() {
    lastEventTime = millis();
    ledFlashActive = true;
    ledFlashColor = rgb.Color(0, 255, 0);
    music.playSendSound();
    HERMESX_LOG_DEBUG("Sent MSG \n");
}

void HermesXInterfaceModule::onPacketFailed() {
    lastEventTime = millis();
    ledFlashActive = true;
    ledFlashColor = rgb.Color(255, 0, 0);
    HERMESX_LOG_DEBUG("Sent fail\n");   
}

void HermesXInterfaceModule::playTone(float freq, uint32_t duration_ms) {
    if (freq > 0) {
        ledcWriteTone(0, freq);
        toneStopTime = millis() + duration_ms;
    }
}

bool HermesXInterfaceModule::wantPacket(const meshtastic_MeshPacket *p)
{
    // 只對 Routing 封包和 Text Message 封包有興趣
    return p->decoded.portnum == meshtastic_PortNum_ROUTING_APP ||
           p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP;
}

ProcessMessage HermesXInterfaceModule::handleReceived(const meshtastic_MeshPacket &packet)
{
    // Routing 封包：播放回饋音效（不管是否為 ACK/NAK）
    if (packet.decoded.portnum == meshtastic_PortNum_ROUTING_APP) {
        meshtastic_Routing routingMsg = meshtastic_Routing_init_default;
        pb_istream_t stream = pb_istream_from_buffer(packet.decoded.payload.bytes, packet.decoded.payload.size);
        if (pb_decode(&stream, meshtastic_Routing_fields, &routingMsg)) {
            playReceiveFeedback();
        }
    }

    // Text Message 封包：來自別人的才播放音效
    if (packet.decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP && !isFromUs(&packet)) {
        playReceiveFeedback();
    }

    return ProcessMessage::CONTINUE;
}





int32_t HermesXInterfaceModule::runOnce() {
    static bool firstTime = true;

    if (firstTime) {
        firstTime = false;
        music.begin(); 
        music.playStartupSound(); // 測試音效
        HERMESX_LOG_INFO("first runOnce() call");
    }

    static bool testPlayed = false;

    if (!testPlayed) {
        testPlayed = true;
        music.playSendSound(); // 測試音效
    }

    uint32_t now = millis();

    // 控制蜂鳴器自動關閉
    if (toneStopTime && now >= toneStopTime) {
        stopTone();
        toneStopTime = 0;
    }

    // 控制 LED 閃爍時間
    if (ledFlashActive && (now - lastEventTime > 300)) {
        ledFlashActive = false;
    }
    
if (waitingForAck && (millis() - lastSentTime > 3000)) {
    waitingForAck = false;
    ackReceived = false;
    playSendFailedFeedback(); // 超時視為失敗
}

    updateLED();

    return 100;  // 100ms 後再執行一次
}

void HermesXInterfaceModule::stopTone() {
    ledcWriteTone(0, 0);
}

void HermesXInterfaceModule::handleRotary() {
 
}


int HermesXInterfaceModule::onNotify(uint32_t fromNum)
 {
    HERMESX_LOG_INFO("onNotify fromNum=%u", fromNum);
    playSendFeedback();
    return 0;
}
void HermesXInterfaceModule::playSendFeedback() {
    music.playSendSound();  
    lastEventTime = millis();
    ledFlashActive = true;
    ledFlashColor = rgb.Color(0, 255, 0);
    HERMESX_LOG_INFO("Send feedback triggered");
}

void HermesXInterfaceModule::playReceiveFeedback() {
    music.playReceiveSound();  
    lastEventTime = millis();
    ledFlashActive = true;
    ledFlashColor = rgb.Color(0, 0, 255);
    HERMESX_LOG_INFO("Receive feedback triggered");
}

void HermesXInterfaceModule::playSendFailedFeedback() {
    music.playFailedSound();  
    lastEventTime = millis();
    ledFlashActive = true;
    ledFlashColor = rgb.Color(255, 0, 0);
    HERMESX_LOG_INFO("Failed feedback triggered");
}


