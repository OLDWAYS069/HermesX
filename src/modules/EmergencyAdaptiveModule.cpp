#include "EmergencyAdaptiveModule.h"

#include <algorithm>
#include <ctime>
#include <cstring>

#include "Channels.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "PowerStatus.h"
#include "configuration.h"
#include "mesh/generated/meshtastic/localonly.pb.h"
#include "pb.h"

#if !MESHTASTIC_EXCLUDE_GPS
#include "modules/PositionModule.h"
#endif

EmergencyAdaptiveModule *EmergencyAdaptiveModule::instance = nullptr;
EmergencyAdaptiveModule *emergencyModule = nullptr;

namespace {
meshtastic_MeshPacket_Priority mapPriority(meshtastic_EmergencyType type)
{
    switch (type) {
    case meshtastic_EmergencyType_SOS:
        return meshtastic_MeshPacket_Priority_ALERT;
    case meshtastic_EmergencyType_NEED:
        return meshtastic_MeshPacket_Priority_HIGH;
    case meshtastic_EmergencyType_RESOURCE:
    case meshtastic_EmergencyType_STATUS:
        return meshtastic_MeshPacket_Priority_DEFAULT;
    case meshtastic_EmergencyType_SAFE:
    case meshtastic_EmergencyType_HEARTBEAT:
    default:
        return meshtastic_MeshPacket_Priority_RELIABLE;
    }
}

uint64_t currentUnixTime()
{
    time_t now = time(nullptr);
    if (now <= 0) {
        return static_cast<uint64_t>(millis() / 1000ULL);
    }
    return static_cast<uint64_t>(now);
}
} // namespace

EmergencyAdaptiveModule::EmergencyAdaptiveModule()
    : ProtobufModule("emergency", PORTNUM_HERMESX_EMERGENCY, &meshtastic_EmergencyMsg_msg)
{
    loopbackOk = true;
    instance = this;
emergencyModule = this;
}

void EmergencyAdaptiveModule::setup()
{
    MeshModule::setup();
    emergencyActive = false;
    lastActivatingNode = nodeDB->getNodeNum();
}

bool EmergencyAdaptiveModule::setEmergencyActive(bool on, uint32_t senderNodeId)
{
    if (on && moduleConfig.emergency.mode == meshtastic_ModuleConfig_EmergencyConfig_Mode_OFF) {
        LOG_INFO("Emergency mode override: config is OFF but allowing activation from node=0x%x", senderNodeId);
    }

    if (!isWhitelisted(senderNodeId)) {
        LOG_WARN("Node 0x%x not in emergency whitelist (count=%u)", senderNodeId, moduleConfig.emergency.whitelist_count);
        return false;
    }

    if (emergencyActive == on) {
        lastActivatingNode = senderNodeId;
        LOG_DEBUG("Emergency already %s (node=0x%x)", on ? "active" : "inactive", senderNodeId);
        return true;
    }

    emergencyActive = on;
    lastActivatingNode = senderNodeId;
    LOG_INFO("Emergency state -> %s by node=0x%x", on ? "ACTIVE" : "INACTIVE", senderNodeId);
    notifyModeChanged(emergencyActive);
    return true;
}

bool EmergencyAdaptiveModule::sendSOS()
{
    if (!setEmergencyActive(true, nodeDB->getNodeNum()))
        return false;
    return sendEmergencyMessage(meshtastic_EmergencyType_SOS, nullptr, true);
}

bool EmergencyAdaptiveModule::sendSafe()
{
    if (!setEmergencyActive(false, nodeDB->getNodeNum()))
        return false;
    return sendEmergencyMessage(meshtastic_EmergencyType_SAFE, nullptr, false);
}

bool EmergencyAdaptiveModule::sendNeed(uint32_t needCode)
{
    if (!setEmergencyActive(true, nodeDB->getNodeNum()))
        return false;

    meshtastic_EmergencyPayload payload = meshtastic_EmergencyPayload_init_default;
    payload.has_need = true;
    payload.need = needCode;

    return sendEmergencyMessage(meshtastic_EmergencyType_NEED, &payload, true);
}

bool EmergencyAdaptiveModule::broadcastResource(const uint32_t *codes, size_t count)
{
    if (moduleConfig.emergency.role != meshtastic_ModuleConfig_EmergencyConfig_Role_SHELTER)
        return false;

    if (!codes || count == 0)
        return false;

    meshtastic_EmergencyPayload payload = meshtastic_EmergencyPayload_init_default;
    const pb_size_t resourceCapacity = static_cast<pb_size_t>(sizeof(payload.resource) / sizeof(payload.resource[0]));
    payload.resource_count = std::min(static_cast<pb_size_t>(count), resourceCapacity);
    memcpy(payload.resource, codes, payload.resource_count * sizeof(uint32_t));

    return sendEmergencyMessage(meshtastic_EmergencyType_RESOURCE, &payload, false);
}

void EmergencyAdaptiveModule::addModeListener(const ModeChangedHandler &cb)
{
    if (cb)
        modeListeners.push_back(cb);
}

void EmergencyAdaptiveModule::addTxResultListener(const TxResultHandler &cb)
{
    if (cb)
        txListeners.push_back(cb);
}

void EmergencyAdaptiveModule::handleTxCompletion(PacketId id, bool ok)
{
    auto it = pendingByPacketId.find(id);
    if (it == pendingByPacketId.end())
        return;

    auto type = it->second;
    pendingByPacketId.erase(it);
    notifyTxResult(static_cast<uint8_t>(type), ok);
}

bool EmergencyAdaptiveModule::handleReceivedProtobuf(const meshtastic_MeshPacket &mp, meshtastic_EmergencyMsg *msg)
{
    if (!msg)
        return false;

    bool handled = false;
    switch (msg->type) {
    case meshtastic_EmergencyType_SOS:
    case meshtastic_EmergencyType_NEED:
        handled = setEmergencyActive(true, mp.from);
        break;
    case meshtastic_EmergencyType_SAFE:
        handled = setEmergencyActive(false, mp.from);
        break;
    default:
        handled = true;
        break;
    }

    if (handled)
        notifyTxResult(static_cast<uint8_t>(msg->type), true);

    return false;
}

bool EmergencyAdaptiveModule::isWhitelisted(uint32_t nodeId) const
{
    const pb_size_t count = moduleConfig.emergency.whitelist_count;
    if (count == 0)
        return nodeId == nodeDB->getNodeNum();

    for (pb_size_t i = 0; i < count; ++i) {
        if (moduleConfig.emergency.whitelist[i] == nodeId)
            return true;
    }
    return false;
}

void EmergencyAdaptiveModule::notifyModeChanged(bool active)
{
    for (auto &cb : modeListeners) {
        cb(active);
    }
}

void EmergencyAdaptiveModule::notifyTxResult(uint8_t type, bool ok)
{
    for (auto &cb : txListeners) {
        cb(type, ok);
    }
}

bool EmergencyAdaptiveModule::sendEmergencyMessage(meshtastic_EmergencyType type,
                                                   const meshtastic_EmergencyPayload *payload,
                                                   bool requireAck)
{
    meshtastic_EmergencyMsg message = meshtastic_EmergencyMsg_init_default;
    fillCommonFields(message);
    message.type = type;
    if (payload) {
        message.has_payload = true;
        message.payload = *payload;
    }

    meshtastic_MeshPacket *packet = allocDataProtobuf(message);
    if (!packet)
        return false;

    packet->to = NODENUM_BROADCAST;
    packet->channel = channels.getPrimaryIndex();
    packet->want_ack = requireAck;
    packet->priority = mapPriority(type);

    if (requireAck) {
        pendingByPacketId[packet->id] = type;
    } else {
        notifyTxResult(static_cast<uint8_t>(type), true);
    }
    service->sendToMesh(packet, RX_SRC_LOCAL, true);
    maybeQueuePositionRequest(type);
    return true;
}

void EmergencyAdaptiveModule::maybeQueuePositionRequest(meshtastic_EmergencyType type)
{
#if !MESHTASTIC_EXCLUDE_GPS
    if (!(type == meshtastic_EmergencyType_SOS || type == meshtastic_EmergencyType_NEED))
        return;

    if (!moduleConfig.emergency.emit_position_request_on_activate)
        return;

    if (!positionModule)
        return;

    uint32_t now = millis();
    uint64_t cooldownMs64 = static_cast<uint64_t>(moduleConfig.emergency.position_request_cooldown_sec) * 1000ULL;
    uint32_t cooldownMs = cooldownMs64 > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(cooldownMs64);
    if (cooldownMs > 0 && lastPositionRequestMs != 0) {
        uint32_t elapsed = now - lastPositionRequestMs;
        if (elapsed < cooldownMs) {
            LOG_DEBUG("Skip position request broadcast due to cooldown (%ums remaining)", cooldownMs - elapsed);
            return;
        }
    }

    uint32_t delayMs = moduleConfig.emergency.position_request_delay_ms;
    uint32_t jitterMs = moduleConfig.emergency.position_request_jitter_ms;

    positionModule->scheduleBroadcastPositionRequest(delayMs, jitterMs, channels.getPrimaryIndex(), true);
    lastPositionRequestMs = now;
    LOG_INFO("EMA issued -> queued position request (delay=%ums, jitter=%ums)", delayMs, jitterMs);
#else
    (void)type;
#endif
}

void EmergencyAdaptiveModule::fillCommonFields(meshtastic_EmergencyMsg &msg)
{
    msg.node_id = myNodeInfo.my_node_num;
    msg.ts = currentUnixTime();

    meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(nodeDB->getNodeNum());
    if (node && nodeDB->hasValidPosition(node)) {
        msg.has_lat = true;
        msg.lat = node->position.latitude_i;
        msg.has_lon = true;
        msg.lon = node->position.longitude_i;
        if (node->position.altitude != 0) {
            msg.has_alt = true;
            msg.alt = node->position.altitude;
        }
    }

    if (powerStatus && powerStatus->getHasBattery()) {
        msg.has_batt = true;
        msg.batt = std::max(static_cast<uint8_t>(0), powerStatus->getBatteryChargePercent());
    }
}


