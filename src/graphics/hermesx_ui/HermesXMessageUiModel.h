#pragma once

#include "mesh/generated/meshtastic/mesh.pb.h"
#include <cstdint>

namespace graphics
{

class HermesXMessageUiModel
{
  public:
    static constexpr uint8_t kRecentMessageCapacity = 8;

    struct RecentMessageState {
        meshtastic_MeshPacket packets[kRecentMessageCapacity];
        uint8_t count = 0;
        uint8_t listCursor = 0;
        uint8_t selectedIndex = 0;
        uint8_t detailIndex = 0;
        uint16_t detailScrollY = 0;
        uint16_t detailMaxScrollY = 0;
    };

    struct IncomingPopupState {
        meshtastic_MeshPacket packet{};
        bool pending = false;
        bool visible = false;
        uint8_t selectedOption = 0;
        uint32_t untilMs = 0;
    };

    static HermesXMessageUiModel &instance();

    RecentMessageState &recentState() { return recent; }
    IncomingPopupState &popupState() { return popup; }

    bool hasRecentMessages() const { return recent.count > 0; }
    uint8_t recentListEntryCount() const { return recent.count + 1; }
    void clampRecentIndices();
    const meshtastic_MeshPacket *recentMessageAt(uint8_t index);
    int findRecentMessageIndex(const meshtastic_MeshPacket &packet) const;
    void selectDetailFromList();
    void storeRecentMessage(const meshtastic_MeshPacket &packet, bool preserveCurrentView);

  private:
    RecentMessageState recent;
    IncomingPopupState popup;
};

} // namespace graphics
