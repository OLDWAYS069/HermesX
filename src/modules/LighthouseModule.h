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
    enum class PositionPulseUiResult : uint8_t {
        None,
        Success,
        Timeout,
    };

    LighthouseModule();
    void markEmergencySafe();
    void activateEmergencyLocal();
    void resetEmergencyState(bool restartDevice);
    int32_t getEmergencyGraceRemainingSec() const;
    bool setEmergencyPassphraseSlot(uint8_t slot, const String &value);
    String getEmergencyPassphrase(uint8_t slot) const;
    bool requestPositionPulse();
    bool isPositionPulseAwaitingResult() const;
    PositionPulseUiResult consumePositionPulseUiResult();
    void cancelPositionPulseRequest(bool clearResponders = false);
    bool didNodeRespondToLastPositionPulse(NodeNum nodeNum) const;

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
    void savePassphrase();
    void captureConfigSnapshot();
    void restoreConfigSnapshot();
    bool isEmergencyActiveAllowed(NodeNum from) const;
    bool isEmergencyActiveAuthorized(const char *txt, NodeNum from) const;
    bool isEmergencyCommandAuthorized(const char *txt, const char *prefix, NodeNum from, bool allowWhitelist) const;
 
    void broadcastStatusMessage();
    void IntroduceMessage();
    void sendEmergencyOk(NodeNum dest);
    void sendEmergencySos();
    void broadcastEmergencyActive();
    void triggerPositionPulse(uint8_t channel);
    void finishPositionPulseRequest(PositionPulseUiResult result, NodeNum responder = 0);

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
    PacketId lastPositionPulseRequestId = 0;
    uint32_t lastPositionPulseAtMs = 0;
    bool awaitingPositionPulseResult = false;
    bool collectingPositionPulseResponses = false;
    uint32_t positionPulseRequestAtMs = 0;
    NodeNum lastPositionPulseResponder = 0;
    PositionPulseUiResult positionPulseUiResult = PositionPulseUiResult::None;
    std::vector<NodeNum> lastPositionPulseResponders;
    bool emergencySafeAcked = false;
    bool restoreConfigValid = false;
    bool restoreUsePreset = true;
    bool restorePowerSaving = false;
    meshtastic_Config_DeviceConfig_Role restoreRole = meshtastic_Config_DeviceConfig_Role_CLIENT;
    meshtastic_Config_LoRaConfig_ModemPreset restoreModemPreset = meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST;
    std::vector<NodeNum> emergencyWhitelist;
    String emergencyPassphrase[2];

};

extern LighthouseModule *lighthouseModule;
