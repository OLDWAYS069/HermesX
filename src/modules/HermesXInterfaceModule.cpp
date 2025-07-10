// HermesXInterfaceModule.cpp - 含 WS2812, ST7789, 旋轉編碼器顯示與反應

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
#include "TinyScheduler.h"
#include "MusicModule.h"
#include "freertos/FreeRTOS.h"    
#include "freertos/task.h"
#include <Arduino.h>            
#include "RedirectablePrint.h"
#include "DebugConfiguration.h"
#include "meshtastic/portnums.pb.h"
#include "HermesXLog.h"





//一直搞我的WS2812B的腳位(希望部會在搞我了)
#define PIN_LED 6
#define NUM_LEDS 8
//旋轉控制器
#define ROTARY_SW 4
#define ROTARY_DT 26
#define ROTARY_CLK 37
//蜂鳴器(無緣~沒緣~大給來做伙~)
#define BUZZER_PIN 17

//TFT ST7789，另外加的那塊
#define TFT_SCL 7
#define TFT_SDA 45
#define TFT_DC 43
#define TFT_RES 44
#define TFT_BLK 46 







extern TinyScheduler scheduler;  // 全域 scheduler



HermesXInterfaceModule* globalHermes = nullptr;



extern HermesXInterfaceModule* globalHermes;
void hermesSchedulerTask(void* pvParameters) {
    while (true) {
        globalHermes->scheduler.tick();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void IRAM_ATTR rotaryISR() {
    if (globalHermes) globalHermes->handleRotary();
}



HermesXInterfaceModule::HermesXInterfaceModule()
    : SinglePortModule("hermesx", meshtastic_PortNum_PRIVATE_APP),
      rgb(NUM_LEDS, PIN_LED, NEO_GRB + NEO_KHZ800),
      music(BUZZER_PIN, scheduler) 
{
    globalHermes = this;
}

void HermesXInterfaceModule::setup() {
    HERMESX_LOG_INFO("HermesX", "建構 HermesXInterfaceModule\n");
    tft.fillScreen(TFT_RED);
    HERMESX_LOG_INFO("DEBUG", "HermesX", "LED 狀態 = %d\n", ledFlashActive);

    
    scheduler.timeout(5, [this]() {
     
        initDisplay();
        drawFace("^_^", TFT_GOLD);
       if (cannedMessageModule && cannedMessageModule->hasMessages()) {
            const char* msg = cannedMessageModule->getCurrentMessage();
           
            drawFace(msg, TFT_CYAN);
        }
    });

   if (cannedMessageModule && cannedMessageModule->hasMessages()) {
      
        drawFace(cannedMessageModule->getCurrentMessage(), TFT_CYAN);
    }
    xTaskCreatePinnedToCore(
        hermesSchedulerTask,
        "hermes_sched_task",
        4096,
        nullptr,
        1,
        nullptr,
        1
    );

    HERMESX_LOG_INFO("HermesX", "初始化完成\n");
}


void HermesXInterfaceModule::initDisplay() {
    tft.init();
    tft.setRotation(0);

    pinMode(TFT_BLK, OUTPUT);      // 背光控制腳（46）
    HERMESX_LOG_INFO("開啟背光\n");
    digitalWrite(TFT_BLK, HIGH);   // 開啟背光

    tft.fillScreen(TFT_BLACK);
}


MusicModule::MusicModule(uint8_t buzzerPin, TinyScheduler& sched)
    : pin(buzzerPin), scheduler(sched) {
    HERMESX_LOG_INFO("音樂模組初始化\n");
}

void HermesXInterfaceModule::initLED() {
    rgb.begin();
    rgb.setBrightness(60);
    rgb.fill(rgb.Color(0, 0, 20));
    rgb.show();
    HERMESX_LOG_INFO("LED初始化\n");
}

void HermesXInterfaceModule::initRotary() {
    pinMode(ROTARY_SW, INPUT_PULLUP);
    pinMode(ROTARY_DT, INPUT);
    pinMode(ROTARY_CLK, INPUT);
    attachInterrupt(digitalPinToInterrupt(ROTARY_DT), rotaryISR, CHANGE);
    HERMESX_LOG_INFO("旋鈕初始化\n");
}



void HermesXInterfaceModule::drawFace(const char* face, uint16_t color) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(color, TFT_BLACK);
    tft.setTextSize(4);
    tft.setCursor(60, 100);
    tft.println(face);
}

void HermesXInterfaceModule::updateFace() {
    if ((faceState == FACE_RECEIVED || faceState == FACE_SENT || faceState == FACE_ERROR) && millis() - lastEventTime > 3000) {
        faceState = FACE_IDLE;
        ledFlashActive = false;
    }

    if (faceState != lastState) {
        lastState = faceState;
        switch (faceState) {
            case FACE_RECEIVED: drawFace(">O<", TFT_BLUE); break;
            case FACE_SENT: drawFace("^D^", TFT_GREEN); break;
            case FACE_IDLE:
            default: drawFace("^_^", TFT_YELLOW); break;
            case FACE_ERROR:    drawFace("x_x", TFT_RED); break;
        }
    }

    
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

meshtastic_MeshPacket* allocPacketForSend() {
    return packetPool.allocZeroed();  
}



meshtastic_MeshPacket* HermesXPacketUtils::makeFromData(const meshtastic_Data* data, uint32_t to, bool want_ack) {
    uint8_t buffer[256];  
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    meshtastic_MeshPacket* p = packetPool.allocZeroed();  
    if (!p) return nullptr;

    if (!pb_encode(&stream, meshtastic_Data_fields, data)) {
        Serial.printf("Failed to encode message: %s\n", PB_GET_ERROR(&stream));
        packetPool.release(p);
        return nullptr;
    }

    p->to = to;
    p->want_ack = want_ack;
    p->decoded.portnum = data->portnum;
    p->decoded.payload.size = stream.bytes_written;
    memcpy(p->decoded.payload.bytes, buffer, stream.bytes_written);

    return p;
}




void HermesXInterfaceModule::sendCannedMessage(const char* msg) {
    meshtastic_Data data = meshtastic_Data_init_default;
    data.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;

    size_t len = strlen(msg);
    if (len > meshtastic_Constants_DATA_PAYLOAD_LEN) len = meshtastic_Constants_DATA_PAYLOAD_LEN;

   
    memset(&data.payload, 0, sizeof(data.payload));
    data.payload.size = len;
    memcpy(data.payload.bytes, msg, len);

    meshtastic_MeshPacket* p = HermesXPacketUtils::makeFromData(&data);
    if (p) {
        p->id = random(1, UINT32_MAX);  
        service->sendToMesh(p, RX_SRC_LOCAL, false);
    }

    
    music.playSendSound();
    faceState = FACE_SENT;
    lastEventTime = millis();
    ledFlashActive = true;
    ledFlashColor = rgb.Color(0, 255, 0);
    HERMESX_LOG_INFO("變臉(傳送)");
}




void HermesXInterfaceModule::handleButtonPress() {
    if (!cannedMessageModule || !cannedMessageModule->hasMessages()) return;

    const char* msg = cannedMessageModule->getCurrentMessage();
    if (msg && strlen(msg) > 0) {
        sendCannedMessage(msg);
    }
}



bool HermesXInterfaceModule::handleRadioPacket(meshtastic_MeshPacket* p) {
    if (p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP) {
        
        music.playReceiveSound();
        faceState = FACE_RECEIVED;
        lastEventTime = millis();
        ledFlashActive = true;
        ledFlashColor = rgb.Color(0, 0, 255);
        

        scheduler.timeout(3, [this]() {
           
            music.playReceiveSound();
            faceState = FACE_IDLE;
            ledFlashActive = false;
        });
    }
    return false;  
}
void HermesXInterfaceModule::onPacketSent() {
    faceState = FACE_SENT;
    lastEventTime = millis();
    ledFlashActive = true;
    ledFlashColor = rgb.Color(0, 255, 0);
    

    scheduler.timeout(3, [this]() {
       
        music.playSendSound();
        faceState = FACE_IDLE;
        ledFlashActive = false;
    });
}

void HermesXInterfaceModule::onPacketFailed() {
    faceState = FACE_ERROR;
    lastEventTime = millis();
    ledFlashActive = true;
    ledFlashColor = rgb.Color(255, 0, 0);
    

    scheduler.timeout(3, [this]() {
        faceState = FACE_IDLE;
        ledFlashActive = false;
    });
}




void HermesXInterfaceModule::playTone(float freq, uint32_t duration_ms) {
    if (freq > 0) {
        ledcWriteTone(0, freq);

        // 使用 TinyScheduler 的 timeout() 方法排程 stopTone()
        scheduler.timeout(duration_ms, [this]() {
            stopTone();
        });
    }
}


bool HermesXInterfaceModule::wantPacket(const meshtastic_MeshPacket *p) {
    // 目前不處理任何封包( 為了迎合Mesh官方架構，目前先保留
    return false;
}

ProcessMessage HermesXInterfaceModule::handleReceived(const meshtastic_MeshPacket &packet) {
    this->handleRadioPacket(const_cast<meshtastic_MeshPacket*>(&packet));
    return ProcessMessage::CONTINUE;

}