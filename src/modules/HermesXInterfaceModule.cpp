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
uint32_t safeTimeout = 5000;

HermesXFeedbackCallback hermesXCallback = nullptr;


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
    playStartupLEDAnimation(currentTheme.colorIdleBreathBase);
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
    uint32_t now = millis();

    if (ledFlashActive) {
        if (pulseMode == LedPulseMode::ACK) {
            uint32_t elapsed = now - pulseStartTime;
            uint8_t currentStep = elapsed / pulseInterval;
            if (currentStep != pulseStep) {
                pulseStep = currentStep;
                rgb.clear();
                int center = NUM_LEDS / 2;
                int left = center - pulseStep;
                int right = (NUM_LEDS % 2 == 0) ? center + pulseStep - 1 : center + pulseStep;
                if (left >= 0) rgb.setPixelColor(left, currentTheme.colorAck);
                if (right < NUM_LEDS) rgb.setPixelColor(right, currentTheme.colorAck);
                if (left < 0 && right >= NUM_LEDS) {
                    pulseMode = LedPulseMode::FADE_OUT;
                    fadeStep = 0;
                    ledFlashActive = false;
                }
            }
        } else {
            if ((now / 300) % 2 == 0) {
                rgb.fill(ledFlashColor);
            } else {
                rgb.clear();
            }
        }
    } else {
        if (pulseMode == LedPulseMode::NONE) {
            if (now - lastBreathUpdate > 30) {
                lastBreathUpdate = now;
                breathBrightness += breathDelta;
                if (breathBrightness >= currentTheme.breathBrightnessMax) {
                    breathBrightness = currentTheme.breathBrightnessMax;
                    breathDelta = -breathDelta;
                } else if (breathBrightness <= currentTheme.breathBrightnessMin) {
                    breathBrightness = currentTheme.breathBrightnessMin;
                    breathDelta = -breathDelta;
                }
            }
            uint8_t r = ((currentTheme.colorIdleBreathBase >> 16) & 0xFF) * breathBrightness;
            uint8_t g = ((currentTheme.colorIdleBreathBase >> 8) & 0xFF) * breathBrightness;
            uint8_t b = (currentTheme.colorIdleBreathBase & 0xFF) * breathBrightness;
            rgb.fill(rgb.Color(r, g, b));
        } else if (pulseMode == LedPulseMode::FADE_OUT) {
            float brightness = 1.0f - ((float)fadeStep / fadeMaxStep);
            brightness = constrain(brightness, 0.0f, 1.0f);
            for (int i = 0; i < NUM_LEDS; ++i) {
                uint32_t c = rgb.getPixelColor(i);
                uint8_t r = ((c >> 16) & 0xFF) * brightness;
                uint8_t g = ((c >> 8) & 0xFF) * brightness;
                uint8_t b = (c & 0xFF) * brightness;
                rgb.setPixelColor(i, rgb.Color(r, g, b));
            }
            fadeStep++;
            if (fadeStep >= fadeMaxStep) {
                rgb.clear();
                pulseMode = LedPulseMode::NONE;
                fadeStep = 0;
            }
        } else {
            rgb.clear();
            uint32_t elapsed = now - pulseStartTime;
            uint8_t currentStep = elapsed / pulseInterval;
            if (currentStep != pulseStep) {
                pulseStep = currentStep;
                if (pulseMode == LedPulseMode::SEND && pulseStep < NUM_LEDS) {
                    rgb.setPixelColor(pulseStep, pulseColor);
                    if (pulseStep + 1 < NUM_LEDS)
                        rgb.setPixelColor(pulseStep + 1, pulseColor);
                } else if (pulseMode == LedPulseMode::RECEIVE && pulseStep < NUM_LEDS) {
                    uint8_t idx = NUM_LEDS - 1 - pulseStep;
                    rgb.setPixelColor(idx, pulseColor);
                    if (idx > 0)
                        rgb.setPixelColor(idx - 1, pulseColor);
                } else {
                    pulseMode = LedPulseMode::FADE_OUT;
                    fadeStep = 0;
                }
            }
        }
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
       p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP ||
       p->decoded.portnum == meshtastic_PortNum_NODEINFO_APP;
}

ProcessMessage HermesXInterfaceModule::handleReceived(const meshtastic_MeshPacket &packet)
{
    // Text Message：只處理來自別人的
    if (packet.decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP &&
        !isFromUs(&packet)) {
        playReceiveFeedback();
    }

    // NodeInfo：只處理來自別人的（如 discovery reply）
    if (packet.decoded.portnum == meshtastic_PortNum_NODEINFO_APP &&
        !isFromUs(&packet)) {
        playNodeInfoFeedback();
    }
    
    
    //送出訊息
    if (packet.decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP &&
    isFromUs(&packet)) {
    playSendFeedback();  
    }
    
    //有ack
    if (packet.decoded.request_id == lastSentId || packet.id == lastSentId) {
    waitingForAck = false;
    ackReceived = true;
    playSendSuccessFeedback();
    HERMESX_LOG_INFO("ACK matched! packet.id=0x%08x request_id=0x%08x", packet.id, packet.decoded.request_id);
    }
    return ProcessMessage::CONTINUE;
}


void hermesXFeedbackHandler(int index, bool state) {
    if (index == 0 && state) {
        
        HermesXInterfaceModule::instance->playReceiveFeedback();
    } else if (index == 2 && state) {
        
        HermesXInterfaceModule::instance->playSendFeedback();
    }
}



int32_t HermesXInterfaceModule::runOnce() {
    static bool firstTime = true;

    if (firstTime) {
        firstTime = false;
        music.begin(); 
        music.playStartupSound();
        HERMESX_LOG_INFO("first runOnce() call");

        hermesXCallback = [](int index, bool state) {
        if (HermesXInterfaceModule::instance) {
            HermesXInterfaceModule::instance->handleExternalNotification(index, state);
            HERMESX_LOG_INFO("callbacksetup");
        }
    };
    }

    static bool testPlayed = false;

    if (!testPlayed) {
        testPlayed = true;
        music.playSendSound();
    }

    uint32_t now = millis();

    
    if (toneStopTime && now >= toneStopTime) {
        stopTone();
        toneStopTime = 0;
    }

    
    if (ledFlashActive && (now - lastEventTime > 300)) {
        ledFlashActive = false;
    }
    
if (waitingForAck && (millis() - lastSentTime > 3000)) {
    waitingForAck = false;
    ackReceived = false;
    playSendFailedFeedback();
    HERMESX_LOG_DEBUG("FAILED");
}

if (pendingSuccessFeedback && millis() >= successFeedbackTime) {
    music.playSuccessSound();
    lastEventTime = millis();
    pendingSuccessFeedback = false;

    triggerAckPulse();  //  改成觸發動畫而非閃爍
    HERMESX_LOG_INFO("Success feedback triggered (ACK animation)");
}

    updateLED();

    return 100;
}

void HermesXInterfaceModule::stopTone() {
    ledcWriteTone(0, 0);
}

void HermesXInterfaceModule::handleRotary() {
 
}


int HermesXInterfaceModule::onNotify(uint32_t fromNum)
 {
    HERMESX_LOG_INFO("onNotify fromNum=%u", fromNum);
    
    return 0;
}

void HermesXInterfaceModule::handleExternalNotification(int index, bool state) {
    if (index == 0 && state) playReceiveFeedback();
    if (index == 2 && state) playSendFeedback();
    HERMESX_LOG_DEBUG("handleExternalNotification!");
}



void HermesXInterfaceModule::playSendFeedback() {
    music.playSendSound();  
    lastEventTime = millis();
    ledFlashActive = false;
    triggerSendPulse(currentTheme.colorSendPrimary);  // 使用主題顏色
    HERMESX_LOG_INFO("Send feedback triggered");
}

void HermesXInterfaceModule::playReceiveFeedback() {
    music.playReceiveSound();  
    lastEventTime = millis();
    ledFlashActive = false;
    triggerReceivePulse(currentTheme.colorReceivePrimary);
    HERMESX_LOG_INFO("Receive feedback triggered");
}

void HermesXInterfaceModule::playSendSuccessFeedback() {
    pendingSuccessFeedback = true;
    successFeedbackTime = millis() + 1000;
    HERMESX_LOG_INFO("Success feedback scheduled");
}

void HermesXInterfaceModule::playNodeInfoFeedback() {
    music.playNodeInfoSound();  
    lastEventTime = millis();
    ledFlashActive = true;
    ledFlashColor = currentTheme.colorAck;  // 或定義 colorNodeInfo
    HERMESX_LOG_INFO("NodeInfo feedback triggered");
}

void HermesXInterfaceModule::playSendFailedFeedback() {
    music.playFailedSound();
    lastEventTime = millis();
    ledFlashActive = true;
    ledFlashColor = currentTheme.colorFailed;
    HERMESX_LOG_INFO("Failed feedback triggered");
}

void HermesXInterfaceModule::triggerSendPulse(uint32_t color) {
    pulseMode = LedPulseMode::SEND;
    pulseColor = color;
    pulseStartTime = millis();
    pulseStep = 0;
    pulseInterval = 130;  // 可依主題調整
}

void HermesXInterfaceModule::triggerReceivePulse(uint32_t color) {
    pulseMode = LedPulseMode::RECEIVE;
    pulseColor = color;
    pulseStartTime = millis();
    pulseStep = 0;
    pulseInterval = 100;
}

void HermesXInterfaceModule::triggerAckPulse() {
    pulseMode = LedPulseMode::ACK;
    pulseStartTime = millis();
    pulseStep = 0;
    ledFlashActive = true;
    pulseInterval = 90;
    HERMESX_LOG_INFO("ACK animation started");
}

void HermesXInterfaceModule::playStartupLEDAnimation(uint32_t color) {
    rgb.clear();
    rgb.show();
    delay(100);

    int center = NUM_LEDS / 2;

    for (int i = 0; i <= center; ++i) {
        if (center - i >= 0) rgb.setPixelColor(center - i, color);
        if (center + i < NUM_LEDS) rgb.setPixelColor(center + i, color);
        rgb.show();
        delay(80);
    }

    delay(300);

    for (int i = 0; i <= center; ++i) {
        if (center - i >= 0) rgb.setPixelColor(center - i, 0);
        if (center + i < NUM_LEDS) rgb.setPixelColor(center + i, 0);
        rgb.show();
        delay(60);
    }
}