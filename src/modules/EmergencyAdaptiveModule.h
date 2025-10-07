#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

#include "ProtobufModule.h"
#include "mesh/MeshTypes.h"
#include "mesh/mesh-pb-constants.h"
#include "mesh/HermesPortnums.h"
#include "mesh/generated/meshtastic/emergency.pb.h"

class EmergencyAdaptiveModule : public ProtobufModule<meshtastic_EmergencyMsg>
{
  public:
    using ModeChangedHandler = std::function<void(bool)>;
    using TxResultHandler = std::function<void(uint8_t, bool)>;


    static EmergencyAdaptiveModule *instance;

    EmergencyAdaptiveModule();

    bool setEmergencyActive(bool on, uint32_t senderNodeId);
    bool sendSOS();
    bool sendSafe();
    bool sendNeed(uint32_t needCode);
    bool broadcastResource(const uint32_t *codes, size_t count);
    bool isEmergencyActive() const { return emergencyActive; }

    void addModeListener(const ModeChangedHandler &cb);
    void addTxResultListener(const TxResultHandler &cb);

    void handleTxCompletion(PacketId id, bool ok);

  protected:
    bool handleReceivedProtobuf(const meshtastic_MeshPacket &mp, meshtastic_EmergencyMsg *msg) override;
    void setup() override;

  private:
    bool emergencyActive = false;
    uint32_t lastActivatingNode = 0;
    std::vector<ModeChangedHandler> modeListeners;
    std::vector<TxResultHandler> txListeners;
    std::unordered_map<uint32_t, meshtastic_EmergencyType> pendingByPacketId;
    uint32_t lastPositionRequestMs = 0;

    bool isWhitelisted(uint32_t nodeId) const;
    void notifyModeChanged(bool active);
    void notifyTxResult(uint8_t type, bool ok);
    bool sendEmergencyMessage(meshtastic_EmergencyType type, const meshtastic_EmergencyPayload *payload = nullptr,
                              bool requireAck = false);
    void fillCommonFields(meshtastic_EmergencyMsg &msg);
    void maybeQueuePositionRequest(meshtastic_EmergencyType type);
};

extern EmergencyAdaptiveModule *emergencyModule;


