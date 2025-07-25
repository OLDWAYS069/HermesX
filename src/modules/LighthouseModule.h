#pragma once

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

    bool emergencyModeActive = false;
    bool roleCorrected = false;
    bool pollingModeRequested = false;
    uint32_t firstBootMillis = 0;

};

extern LighthouseModule *lighthouseModule;