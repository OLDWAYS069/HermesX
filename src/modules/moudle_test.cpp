///JUST FOR TEST

#include "moudle_test.h"
#include "modules/CannedMessageModule.h"
#include "mesh/Router.h"
#include "meshtastic/mesh.pb.h"
#include "pb.h"
#include "pb_encode.h"
#include <Arduino.h>
#include "HermesXPacketUtils.h"
#include "HermesXLog.h"
#include "configuration.h"

ModuleTest* globalTEST = nullptr;

extern ::ThreadController concurrency::mainController;


ModuleTest::ModuleTest()
  : SinglePortModule("ModuleTest", meshtastic_PortNum_PRIVATE_APP),
    concurrency::OSThread("ModuleTest", 1000,  &concurrency::mainController),
      scheduler(TinyScheduler::millis()){

    globalTEST = this;

    HERMESX_LOG_DEBUG("CONTSTUCT");
}

void ModuleTest::setup() {
    HERMESX_LOG_DEBUG("[HermesX] setup start");

    Thread::canSleep = false;
    OSThread::enabled = true;
    OSThread::setIntervalFromNow(0);

    scheduler.every(5000, [this]() { update(); });

    HERMESX_LOG_DEBUG("SETUP");

}

void ModuleTest::update() {

    HERMESX_LOG_DEBUG("123");
    
}

int32_t ModuleTest::runOnce() {
    scheduler.tick();
    HERMESX_LOG_DEBUG("1Tick");   

    return 1000;

}