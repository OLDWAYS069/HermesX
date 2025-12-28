#pragma once

#include "mesh/MeshModule.h"
#include "mesh/generated/meshtastic/mesh.pb.h"
#include <unordered_set>

struct WelcomeConfig {
    bool enabled = true;
    uint32_t radius_m = 20000; // default 20km
};

class WelcomeModule : public MeshModule
{
  public:
    WelcomeModule();

    static WelcomeConfig &getConfig();
    static void setEnabled(bool on);
    static void setRadiusMeters(uint32_t meters);
    static bool hasWelcomed(NodeNum node);
    static void markWelcomed(NodeNum node);

  protected:
    bool wantPacket(const meshtastic_MeshPacket *p) override;
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

  private:
    static WelcomeConfig config;
    static std::unordered_set<NodeNum> welcomed;

    bool haveLocalFix() const;
    bool decodePosition(const meshtastic_MeshPacket &mp, meshtastic_Position &out) const;
    bool withinRadius(const meshtastic_Position &remote) const;
    void broadcastWelcome(NodeNum target);
};
