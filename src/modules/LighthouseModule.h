#pragma once
#ifndef MESHTASTIC_EXCLUDE_LIGHTHOUSE
#define MESHTASTIC_EXCLUDE_LIGHTHOUSE 0
#endif

#include "SinglePortModule.h"
#include "concurrency/OSThread.h"
#include <vector>
#include <Arduino.h>

class LighthouseModule : public SinglePortModule, private concurrency::OSThread
{
  public:
    LighthouseModule();
    void markEmergencySafe();
    int32_t getEmergencyGraceRemainingSec() const;

  protected:
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override;
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    virtual int32_t runOnce() override;

  private:
    void loadBoot();
    void saveBoot();
    void loadState();
    void saveState();
    void loadWhitelist();
    void loadPassphrase();
    bool isEmergencyActiveAllowed(NodeNum from) const;
    bool isEmergencyActiveAuthorized(const char *txt, NodeNum from) const;
 
    void broadcastStatusMessage();
    void IntroduceMessage();
    void sendEmergencyOk(NodeNum dest);
    void sendEmergencySos();

    bool emergencyModeActive = false;
    bool roleCorrected = false;
    bool pollingModeRequested = false;
    bool hihermes = false;
    bool awaitingEmergencyOkAck = false;
    PacketId emergencyOkRequestId = 0;
    uint32_t firstBootMillis = 0;
    uint32_t emergencyActivatedAtMs = 0;
    uint32_t emergencyLastSosAtMs = 0;
    PacketId lastEmergencyActiveId = 0;
    uint32_t lastEmergencyActiveAtMs = 0;
    bool emergencySafeAcked = false;
    std::vector<NodeNum> emergencyWhitelist;
    String emergencyPassphrase;

};

extern LighthouseModule *lighthouseModule;
