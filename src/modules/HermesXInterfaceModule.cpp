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
#include "mesh/mesh-pb-constants.h"

#define PIN_LED 6
#define NUM_LEDS 8

#define BUZZER_PIN 17

HermesXInterfaceModule* globalHermes = nullptr;
HermesXInterfaceModule *HermesXInterfaceModule::instance = nullptr;
uint32_t safeTimeout = 5000;

HermesXFeedbackCallback hermesXCallback = nullptr;






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
    // === ROUTING ACK/NACK 處理 ===
    if (packet.decoded.portnum == meshtastic_PortNum_ROUTING_APP && waitingForAck) {
        if (packet.decoded.request_id != 0) {
            meshtastic_Routing decoded = meshtastic_Routing_init_default;

            bool ok = pb_decode_from_bytes(
                packet.decoded.payload.bytes,
                packet.decoded.payload.size,
                meshtastic_Routing_fields,
                &decoded);

            if (ok) {
                ackReceived = (decoded.error_reason == meshtastic_Routing_Error_NONE);
                waitingForAck = false;

                if (ackReceived) {
                    pendingSuccessFeedback = true;
                    successFeedbackTime = millis() + 300;

                    HERMESX_LOG_INFO("Routing ACK received, no error. request_id=0x%08x", packet.decoded.request_id);
                } else {
                    playSendFailedFeedback();
                    HERMESX_LOG_WARN("Routing NACK (error_reason=%d) for request_id=0x%08x",
                        decoded.error_reason,
                        packet.decoded.request_id);
                }
            } else {
                HERMESX_LOG_ERROR("Failed to decode Routing payload.");
            }
        }
    }

    // === Text Message：收到別人傳的
    if (packet.decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP &&
        !isFromUs(&packet)) {
        playReceiveFeedback();
    }

    // === NodeInfo：收到別人傳的
    if (packet.decoded.portnum == meshtastic_PortNum_NODEINFO_APP &&
        !isFromUs(&packet)) {
        playNodeInfoFeedback();
    }

    // === Text Message：自己送出
    if (packet.decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP &&
        isFromUs(&packet)) {
        playSendFeedback();
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
    static bool testPlayed = false;
    uint32_t now = millis();

    // === 初始化階段，只執行一次 ===
    if (firstTime) {
        firstTime = false;
        music.begin(); 
        music.playStartupSound();
        HERMESX_LOG_INFO("first runOnce() call");

        // 建立 feedback 回呼
        hermesXCallback = [](int index, bool state) {
            if (HermesXInterfaceModule::instance) {
                HermesXInterfaceModule::instance->handleExternalNotification(index, state);
                HERMESX_LOG_INFO("callbacksetup");
            }
        };
    }

    // === 測試音效（僅一次）===
    if (!testPlayed) {
        testPlayed = true;
        music.playSendSound();
    }

    // === 停止 tone 播放 ===
    if (toneStopTime && now >= toneStopTime) {
        stopTone();
        toneStopTime = 0;
    }

    // === 停止閃燈（例如 Send/Receive Feedback 結束）===
    if (ledFlashActive && (now - lastEventTime > 300)) {
        ledFlashActive = false;
    }

    // === Timeout: 等待 ACK 超過 3 秒，視為失敗 ===
    if (waitingForAck && (now - lastSentTime > 3000)) {
        waitingForAck = false;
        ackReceived = false;
        playSendFailedFeedback();
        HERMESX_LOG_WARN("ACK Timeout: Delivery failed");
    }

    // === ACK 成功動畫與音效觸發 ===
    if (pendingSuccessFeedback && now >= successFeedbackTime) {
        pendingSuccessFeedback = false;
        music.playSuccessSound();
        lastEventTime = now;

        triggerAckPulse();  // 啟動 LED 成功動畫
        HERMESX_LOG_INFO("Success feedback triggered (ACK animation)");
    }

    // === 更新 LED 狀態 ===
    updateLED();

    return 100;  // 執行間隔：100ms
}

void HermesXInterfaceModule::stopTone() {
    ledcWriteTone(0, 0);
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