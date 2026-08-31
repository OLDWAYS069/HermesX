#include "HermesXMessageUiModel.h"

namespace graphics
{

HermesXMessageUiModel &HermesXMessageUiModel::instance()
{
    static HermesXMessageUiModel model;
    return model;
}

void HermesXMessageUiModel::clampRecentIndices()
{
    const uint8_t lastCursor = recentListEntryCount() - 1;
    if (recent.listCursor > lastCursor) {
        recent.listCursor = lastCursor;
    }

    if (recent.count == 0) {
        recent.selectedIndex = 0;
        recent.detailIndex = 0;
        return;
    }

    const uint8_t lastIndex = recent.count - 1;
    if (recent.selectedIndex > lastIndex) {
        recent.selectedIndex = lastIndex;
    }
    if (recent.detailIndex > lastIndex) {
        recent.detailIndex = lastIndex;
    }
}

const meshtastic_MeshPacket *HermesXMessageUiModel::recentMessageAt(uint8_t index)
{
    clampRecentIndices();
    return index < recent.count ? &recent.packets[index] : nullptr;
}

int HermesXMessageUiModel::findRecentMessageIndex(const meshtastic_MeshPacket &packet) const
{
    for (uint8_t i = 0; i < recent.count; ++i) {
        const meshtastic_MeshPacket &candidate = recent.packets[i];
        if (candidate.from == packet.from && candidate.id == packet.id) {
            return i;
        }
    }
    return -1;
}

void HermesXMessageUiModel::selectDetailFromList()
{
    clampRecentIndices();
    recent.detailIndex = recent.selectedIndex;
    recent.detailScrollY = 0;
}

void HermesXMessageUiModel::storeRecentMessage(const meshtastic_MeshPacket &packet, bool preserveCurrentView)
{
    if (packet.from == 0) {
        return;
    }

    meshtastic_MeshPacket selectedPacket{};
    meshtastic_MeshPacket detailPacket{};
    const bool hadSelectedPacket = preserveCurrentView && recent.count > 0 && recent.selectedIndex < recent.count;
    const bool hadDetailPacket = preserveCurrentView && recent.count > 0 && recent.detailIndex < recent.count;
    const bool listWasOnBack = preserveCurrentView && recent.listCursor == 0;
    if (hadSelectedPacket) {
        selectedPacket = recent.packets[recent.selectedIndex];
    }
    if (hadDetailPacket) {
        detailPacket = recent.packets[recent.detailIndex];
    }
    const uint16_t previousDetailScrollY = recent.detailScrollY;

    uint8_t existingIndex = recent.count;
    for (uint8_t i = 0; i < recent.count; ++i) {
        const meshtastic_MeshPacket &candidate = recent.packets[i];
        if (candidate.from == packet.from && candidate.id == packet.id) {
            existingIndex = i;
            break;
        }
    }

    if (existingIndex == recent.count) {
        if (recent.count < kRecentMessageCapacity) {
            ++recent.count;
        }
        existingIndex = recent.count - 1;
    }

    for (uint8_t i = existingIndex; i > 0; --i) {
        recent.packets[i] = recent.packets[i - 1];
    }
    recent.packets[0] = packet;

    if (!preserveCurrentView) {
        recent.listCursor = 1;
        recent.selectedIndex = 0;
        recent.detailIndex = 0;
        recent.detailScrollY = 0;
        recent.detailMaxScrollY = 0;
        return;
    }

    const int selectedIndex = hadSelectedPacket ? findRecentMessageIndex(selectedPacket) : -1;
    const int detailIndex = hadDetailPacket ? findRecentMessageIndex(detailPacket) : -1;
    if (selectedIndex >= 0) {
        recent.selectedIndex = static_cast<uint8_t>(selectedIndex);
        recent.listCursor = listWasOnBack ? 0 : static_cast<uint8_t>(selectedIndex + 1);
    } else {
        recent.selectedIndex = 0;
        recent.listCursor = listWasOnBack ? 0 : 1;
    }
    if (detailIndex >= 0) {
        recent.detailIndex = static_cast<uint8_t>(detailIndex);
        recent.detailScrollY = previousDetailScrollY;
    } else {
        recent.detailIndex = 0;
        recent.detailScrollY = 0;
    }
}

} // namespace graphics
