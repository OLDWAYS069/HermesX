#pragma once
#include "SinglePortModule.h"
#include <Adafruit_GFX.h>
#include <Adafruit_NeoPixel.h>
#include "meshtastic/mesh.pb.h"
#include "TinyScheduler.h"
#include "MusicModule.h"
#include <LovyanGFX.hpp>
#include "concurrency/OSThread.h"


class ModuleTest : public SinglePortModule, public concurrency::OSThread {
public:
    ModuleTest();
    
    void setup() override;
    TinyScheduler scheduler;
    void update();

private:
 
    int32_t runOnce() override;
};

extern ModuleTest* globalTEST;

