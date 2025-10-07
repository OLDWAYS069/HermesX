#pragma once
#ifndef MESHTASTIC_EXCLUDE_LIGHTHOUSE
#define MESHTASTIC_EXCLUDE_LIGHTHOUSE 0
#endif

#include "SinglePortModule.h"
#include "concurrency/OSThread.h"
#include <Arduino.h>

class LighthouseModule : public SinglePortModule, private concurrency::OSThread
{
  public:
    LighthouseModule();

  protected:
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override;
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    virtual int32_t runOnce() override;

  private:
    void loadBoot();
    void saveBoot();
    void loadState();
    void saveState();
 
    void broadcastStatusMessage();
    void IntroduceMessage();

    bool emergencyModeActive = false;
    bool roleCorrected = false;
    bool pollingModeRequested = false;
    bool hihermes = false;
    uint32_t firstBootMillis = 0;

};

extern LighthouseModule *lighthouseModule;
