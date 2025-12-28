#include "WelcomeModule.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "TextMessageModule.h"
#include "gps/GeoCoord.h"
#include "mesh/MeshTypes.h"
#include "mesh/mesh-pb-constants.h"
#include "pb_decode.h"
#include <cstring>
#include <string>

extern meshtastic_Position localPosition;

WelcomeModule::WelcomeModule() : MeshModule("Welcome")
{
    isPromiscuous = true;
}

WelcomeConfig WelcomeModule::config{};
std::unordered_set<NodeNum> WelcomeModule::welcomed{};

WelcomeConfig &WelcomeModule::getConfig()
{
    return config;
}

void WelcomeModule::setEnabled(bool on)
{
    config.enabled = on;
}

void WelcomeModule::setRadiusMeters(uint32_t meters)
{
    config.radius_m = meters;
}

bool WelcomeModule::hasWelcomed(NodeNum node)
{
    return welcomed.find(node) != welcomed.end();
}

void WelcomeModule::markWelcomed(NodeNum node)
{
    welcomed.insert(node);
}

bool WelcomeModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return p->which_payload_variant == meshtastic_MeshPacket_decoded_tag &&
           p->decoded.portnum == meshtastic_PortNum_POSITION_APP;
}

ProcessMessage WelcomeModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (!config.enabled) {
        return ProcessMessage::CONTINUE;
    }

    if (mp.from == nodeDB->getNodeNum()) {
        return ProcessMessage::CONTINUE;
    }

    meshtastic_Position pos = meshtastic_Position_init_zero;
    if (!decodePosition(mp, pos)) {
        return ProcessMessage::CONTINUE;
    }

    if (!haveLocalFix()) {
        return ProcessMessage::CONTINUE;
    }

    if (!withinRadius(pos)) {
        return ProcessMessage::CONTINUE;
    }

    if (hasWelcomed(mp.from)) {
        return ProcessMessage::CONTINUE;
    }

    broadcastWelcome(mp.from);
    markWelcomed(mp.from);
    return ProcessMessage::CONTINUE;
}

bool WelcomeModule::decodePosition(const meshtastic_MeshPacket &mp, meshtastic_Position &out) const
{
    pb_istream_t stream = pb_istream_from_buffer(mp.decoded.payload.bytes, mp.decoded.payload.size);
    if (!pb_decode(&stream, meshtastic_Position_fields, &out)) {
        return false;
    }
    return out.latitude_i != 0 && out.longitude_i != 0;
}

bool WelcomeModule::haveLocalFix() const
{
    return localPosition.latitude_i != 0 && localPosition.longitude_i != 0;
}

bool WelcomeModule::withinRadius(const meshtastic_Position &remote) const
{
    GeoCoord here(localPosition.latitude_i, localPosition.longitude_i, 0);
    GeoCoord there(remote.latitude_i, remote.longitude_i, 0);
    int32_t dist = here.distanceTo(there);
    return dist >= 0 && static_cast<uint32_t>(dist) <= config.radius_m;
}

void WelcomeModule::broadcastWelcome(NodeNum target)
{
    const meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(target);
    const char *shortName = (node && node->user.short_name[0]) ? node->user.short_name : "新朋友";

    meshtastic_MeshPacket *p = router->allocForSending();
    p->to = NODENUM_BROADCAST;
    p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;

    char msg[200];
    snprintf(msg, sizeof(msg),
             "歡迎 %s 進入花蓮Mesh網路!\nLoBBS 指令：/hi <帳號> <密碼> 登入，/news 看公告，@user <msg> 傳私信",
             shortName);

    size_t len = strnlen(msg, sizeof(p->decoded.payload.bytes));
    p->decoded.payload.size = len;
    memcpy(p->decoded.payload.bytes, msg, len);

    // set a minimal hop limit for local broadcast
    p->hop_limit = 3;
    service->sendToMesh(p);
}
