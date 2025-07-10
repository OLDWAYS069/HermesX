#pragma once
#include "SinglePortModule.h" 
#include <Adafruit_GFX.h>
//#include <Adafruit_ST7789.h> 暫時先保留
#include <Adafruit_NeoPixel.h>
#include "meshtastic/mesh.pb.h"
#include "TinyScheduler.h"
#include "MusicModule.h"
#include <LovyanGFX.hpp>
#include "concurrency/OSThread.h"


#ifndef TFT_INVERT
#define TFT_INVERT false
#endif

///TFT色彩自訂義，好像MESH官方原生就有定義了，但我先用自訂的，之候再來找。

#define TFT_BLACK   0x0000
#define TFT_RED     0xF800
#define TFT_GREEN   0x07E0
#define TFT_BLUE    0x001F
#define TFT_YELLOW  0xFFE0
#define TFT_CYAN    0x07FF
#define TFT_WHITE   0xFFFF



class LGFX_HermesX_ST7789 : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel;
    lgfx::Bus_SPI _bus;

    public:
  LGFX_HermesX_ST7789() {
    auto cfg = _bus.config();
    cfg.spi_host = SPI3_HOST;     
    cfg.spi_mode = 0;
    cfg.freq_write = 40000000;
    cfg.freq_read  = 16000000;
    cfg.spi_3wire = false;
    cfg.use_lock = true;
    cfg.dma_channel = SPI_DMA_CH_AUTO;

    cfg.pin_sclk = 7;   // TFT_SCL
    cfg.pin_mosi = 45;  // TFT_SDA
    cfg.pin_miso = -1;  //沒有用到
    cfg.pin_dc   = 43;  // TFT_DC
    

    _bus.config(cfg);
    _panel.setBus(&_bus);

    auto pcfg = _panel.config();
    pcfg.panel_width  = 240;
    pcfg.panel_height = 240;
    pcfg.memory_width  = 240;
    pcfg.memory_height = 240;
    pcfg.offset_x = 0;
    pcfg.offset_y = 0;
    pcfg.offset_rotation = 0;
    pcfg.dummy_read_pixel = 8;
    pcfg.dummy_read_bits = 1;
    pcfg.invert = TFT_INVERT;
    pcfg.rgb_order = false;
    pcfg.dlen_16bit = false;
    pcfg.bus_shared = false;
    pcfg.pin_cs = -1;
    pcfg.pin_rst = 44;
    pcfg.pin_busy = -1;

    _panel.config(pcfg);
    setPanel(&_panel);
  }
};




class HermesXInterfaceModule : public SinglePortModule , public concurrency::OSThread {
public:
    HermesXInterfaceModule();
    void setup();
    ProcessMessage handleReceived(const meshtastic_MeshPacket &packet);
    void handleRotary();
    void handleButtonPress();  

    
    Adafruit_NeoPixel rgb;
    LGFX_HermesX_ST7789 tft;
    MusicModule music;
   
    

private:
    void initDisplay();
    void initLED();
    void initRotary();
    
    void drawFace(const char* face, uint16_t color);
    void updateFace();
    void updateLED();
    
    void sendCannedMessage(const char* msg);
    void onPacketSent();
    void onPacketFailed();
    bool handleRadioPacket(meshtastic_MeshPacket* p);
    
    void initBuzzer();
    void playTone(float freq, uint32_t duration_ms);
    void stopTone(); 
    void playStartupTone();
    void playReceiveTone();
    void playSendTone();   
   
    TinyScheduler scheduler;
    
    

    bool wantPacket(const meshtastic_MeshPacket *p) override;

    enum HermesFaceState { FACE_IDLE, FACE_RECEIVED, FACE_SENT, FACE_ERROR } ;
    HermesFaceState faceState = FACE_IDLE;
    HermesFaceState lastState = FACE_IDLE;

    uint32_t lastEventTime = 0;
    bool ledFlashActive = false;
    uint32_t ledFlashColor = 0;

    concurrency::OSThread *thread = nullptr;
    int32_t runOnce() override {
            // 放這裡才對！
            return 100;
        };
        
};

extern HermesXInterfaceModule* globalHermes;



