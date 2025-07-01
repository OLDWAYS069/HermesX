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
#include "MusicModule.cpp"


//一直搞我的WS2812B的腳位(希望部會在搞我了)
#define PIN_LED 6
#define NUM_LEDS 8
//TFT
#define TFT_SCL 7
#define TFT_SDA 45
#define TFT_DC 43
#define TFT_RES 44
#define TFT_BLK 46
//旋轉控制器
#define ROTARY_SW 4
#define ROTARY_DT 26
#define ROTARY_CLK 37
//蜂鳴器(無緣~沒緣~大給來做伙~)
#define BUZZER_PIN 17

extern TinyScheduler scheduler;  // 全域 scheduler 實例



static HermesXInterfaceModule* globalHermes = nullptr;

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
    : MeshModule("hermesx"), spi_st7789(SPI), tft(&spi_st7789, TFT_DC, TFT_RES, -1),
      rgb(NUM_LEDS, PIN_LED, NEO_GRB + NEO_KHZ800),
      music(BUZZER_PIN, scheduler) 
{
    globalHermes = this;

}

void HermesXInterfaceModule::setup() {
    initDisplay();
    initLED();
    initRotary();
    music.begin();
    music.playStartupSound();
    drawFace("^_^", ST77XX_YELLOW);

    if (cannedMessageModule && cannedMessageModule->hasMessages()) {
        drawFace(cannedMessageModule->getCurrentMessage(), ST77XX_CYAN);

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
}


void HermesXInterfaceModule::initDisplay() {
    spi_st7789.begin(TFT_SCL, -1, TFT_SDA, -1);
    tft.init(240, 240);
    tft.setRotation(0);
    pinMode(TFT_BLK, OUTPUT);
    digitalWrite(TFT_BLK, HIGH);
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(4);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
}


MusicModule::MusicModule(uint8_t buzzerPin, TinyScheduler& sched)
    : pin(buzzerPin), scheduler(sched) {

}

void HermesXInterfaceModule::initLED() {
    rgb.begin();
    rgb.setBrightness(60);
    rgb.fill(rgb.Color(0, 0, 20));
    rgb.show();
}

void HermesXInterfaceModule::initRotary() {
    pinMode(ROTARY_SW, INPUT_PULLUP);
    pinMode(ROTARY_DT, INPUT);
    pinMode(ROTARY_CLK, INPUT);
    attachInterrupt(digitalPinToInterrupt(ROTARY_DT), rotaryISR, CHANGE);
}



void HermesXInterfaceModule::drawFace(const char* face, uint16_t color) {
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(color, ST77XX_BLACK);
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
            case FACE_RECEIVED: drawFace(">O<", ST77XX_BLUE); break;
            case FACE_SENT: drawFace("^D^", ST77XX_GREEN); break;
            case FACE_IDLE:
            default: drawFace("^_^", ST77XX_YELLOW); break;
            case FACE_ERROR:    drawFace("x_x", ST77XX_RED); break;
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

    music.begin();
    music.playSendSound();
    faceState = FACE_SENT;
    lastEventTime = millis();
    ledFlashActive = true;
    ledFlashColor = rgb.Color(0, 255, 0);
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
        music.begin();
        music.playReceiveSound();
        faceState = FACE_RECEIVED;
        lastEventTime = millis();
        ledFlashActive = true;
        ledFlashColor = rgb.Color(0, 0, 255);
        

        scheduler.timeout(3000, [this]() {
            music.begin();
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
    

    scheduler.timeout(3000, [this]() {
        music.begin();
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
    

    scheduler.timeout(3000, [this]() {
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

