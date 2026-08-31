#include "HermesXTraceRouteBindings.h"

#include "FSCommon.h"
#include "configuration.h"
#include <cstdio>
#include <cstdlib>

namespace
{

constexpr const char *kBoundNodesFile = "/prefs/hermesx_tr_bound_nodes.txt";

} // namespace

HermesXTraceRouteBindings &HermesXTraceRouteBindings::instance()
{
    static HermesXTraceRouteBindings bindings;
    return bindings;
}

uint8_t HermesXTraceRouteBindings::count()
{
    ensureLoaded();
    return count_;
}

uint32_t HermesXTraceRouteBindings::nodeAt(uint8_t index)
{
    ensureLoaded();
    return index < count_ ? nodes_[index] : 0;
}

bool HermesXTraceRouteBindings::contains(uint32_t nodeNum)
{
    ensureLoaded();
    if (nodeNum == 0) {
        return false;
    }
    for (uint8_t index = 0; index < count_; ++index) {
        if (nodes_[index] == nodeNum) {
            return true;
        }
    }
    return false;
}

bool HermesXTraceRouteBindings::bind(uint32_t nodeNum)
{
    ensureLoaded();
    if (nodeNum == 0 || contains(nodeNum) || count_ >= Capacity) {
        return false;
    }
    nodes_[count_++] = nodeNum;
    save();
    return true;
}

bool HermesXTraceRouteBindings::unbindAt(uint8_t index)
{
    ensureLoaded();
    if (index >= count_) {
        return false;
    }
    for (uint8_t cursor = index; cursor + 1 < count_; ++cursor) {
        nodes_[cursor] = nodes_[cursor + 1];
    }
    nodes_[count_ - 1] = 0;
    --count_;
    save();
    return true;
}

void HermesXTraceRouteBindings::ensureLoaded()
{
    if (loaded_) {
        return;
    }
    loaded_ = true;
    if (!FSCom.exists(kBoundNodesFile)) {
        return;
    }

    auto file = FSCom.open(kBoundNodesFile, FILE_O_READ);
    if (!file) {
        LOG_WARN("[TraceRouteBindings] Failed to load bound nodes");
        return;
    }

    while (file.available() && count_ < Capacity) {
        String raw = file.readStringUntil('\n');
        raw.trim();
        if (raw.isEmpty()) {
            continue;
        }
        const uint32_t nodeNum = static_cast<uint32_t>(strtoul(raw.c_str(), nullptr, 16));
        if (nodeNum == 0 || contains(nodeNum)) {
            continue;
        }
        nodes_[count_++] = nodeNum;
    }
    LOG_INFO("[TraceRouteBindings] Loaded %u bound nodes", static_cast<unsigned>(count_));
}

bool HermesXTraceRouteBindings::save()
{
    if (!FSCom.exists("/prefs")) {
        FSCom.mkdir("/prefs");
    }
    if (FSCom.exists(kBoundNodesFile)) {
        FSCom.remove(kBoundNodesFile);
    }

    auto file = FSCom.open(kBoundNodesFile, FILE_O_WRITE);
    if (!file) {
        LOG_WARN("[TraceRouteBindings] Failed to save bound nodes");
        return false;
    }
    char nodeId[12];
    for (uint8_t index = 0; index < count_; ++index) {
        snprintf(nodeId, sizeof(nodeId), "%08lx\n", static_cast<unsigned long>(nodes_[index]));
        file.print(nodeId);
    }
    return true;
}
