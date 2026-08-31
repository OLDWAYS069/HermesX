#include "HermesXNodeBrowserDataSource.h"

#include "HermesXNodeBrowserUiModel.h"
#include "gps/GeoCoord.h"
#include "mesh/NodeDB.h"
#include "modules/LighthouseModule.h"

namespace graphics
{

HermesXNodeBrowserDataSource &HermesXNodeBrowserDataSource::instance()
{
    static HermesXNodeBrowserDataSource source;
    return source;
}

bool HermesXNodeBrowserDataSource::isOnlineCandidate(const meshtastic_NodeInfoLite &node)
{
    if (node.num == 0 || !nodeDB || node.num == nodeDB->getNodeNum() || node.last_heard == 0) {
        return false;
    }
    return sinceLastSeen(&node) < (60U * 60U * 2U);
}

bool HermesXNodeBrowserDataSource::isFinderCandidate(const meshtastic_NodeInfoLite &node)
{
    if (node.num == 0 || !nodeDB || node.num == nodeDB->getNodeNum()) {
        return false;
    }
    if (!lighthouseModule || !lighthouseModule->didNodeRespondToLastPositionPulse(node.num)) {
        return false;
    }
    return nodeDB->hasValidPosition(const_cast<meshtastic_NodeInfoLite *>(&node));
}

float HermesXNodeBrowserDataSource::finderDistanceScore(const meshtastic_NodeInfoLite &node)
{
    meshtastic_NodeInfoLite *ourNode = nodeDB ? nodeDB->getMeshNode(nodeDB->getNodeNum()) : nullptr;
    auto *candidate = const_cast<meshtastic_NodeInfoLite *>(&node);
    if (!ourNode || !nodeDB->hasValidPosition(ourNode) || !nodeDB->hasValidPosition(candidate)) {
        return 1.0e12f;
    }
    return GeoCoord::latLongToMeter(ourNode->position.latitude_i * 1e-7f, ourNode->position.longitude_i * 1e-7f,
                                    node.position.latitude_i * 1e-7f, node.position.longitude_i * 1e-7f);
}

uint8_t HermesXNodeBrowserDataSource::refreshOnline()
{
    auto &model = HermesXNodeBrowserUiModel::instance();
    auto &state = model.onlineState();
    state.count = 0;
    if (!nodeDB || !nodeDB->meshNodes) {
        model.clamp(state);
        return 0;
    }

    for (uint16_t i = 0; i < nodeDB->getNumMeshNodes() && state.count < HermesXNodeBrowserState::Capacity; ++i) {
        const auto &node = nodeDB->meshNodes->at(i);
        if (isOnlineCandidate(node)) {
            state.order[state.count++] = i;
        }
    }
    for (uint8_t i = 0; i < state.count; ++i) {
        for (uint8_t j = i + 1; j < state.count; ++j) {
            const auto &a = nodeDB->meshNodes->at(state.order[i]);
            const auto &b = nodeDB->meshNodes->at(state.order[j]);
            if (b.last_heard > a.last_heard) {
                const uint16_t previous = state.order[i];
                state.order[i] = state.order[j];
                state.order[j] = previous;
            }
        }
    }
    model.clamp(state);
    return state.count;
}

uint8_t HermesXNodeBrowserDataSource::refreshFinder()
{
    auto &model = HermesXNodeBrowserUiModel::instance();
    auto &state = model.finderState();
    state.count = 0;
    if (!nodeDB || !nodeDB->meshNodes) {
        model.clamp(state);
        return 0;
    }

    for (uint16_t i = 0; i < nodeDB->getNumMeshNodes() && state.count < HermesXNodeBrowserState::Capacity; ++i) {
        const auto &node = nodeDB->meshNodes->at(i);
        if (isFinderCandidate(node)) {
            state.order[state.count++] = i;
        }
    }
    for (uint8_t i = 0; i < state.count; ++i) {
        for (uint8_t j = i + 1; j < state.count; ++j) {
            const auto &a = nodeDB->meshNodes->at(state.order[i]);
            const auto &b = nodeDB->meshNodes->at(state.order[j]);
            const float distanceA = finderDistanceScore(a);
            const float distanceB = finderDistanceScore(b);
            if (distanceB < distanceA || (distanceB == distanceA && b.last_heard > a.last_heard)) {
                const uint16_t previous = state.order[i];
                state.order[i] = state.order[j];
                state.order[j] = previous;
            }
        }
    }
    model.clamp(state);
    return state.count;
}

uint8_t HermesXNodeBrowserDataSource::refreshTraceRoute()
{
    traceRouteCount_ = 0;
    if (!nodeDB || !nodeDB->meshNodes) {
        return 0;
    }

    for (uint16_t i = 0; i < nodeDB->getNumMeshNodes() && traceRouteCount_ < TraceRouteCapacity; ++i) {
        const auto &node = nodeDB->meshNodes->at(i);
        if (isOnlineCandidate(node)) {
            traceRouteOrder_[traceRouteCount_++] = i;
        }
    }
    for (uint8_t i = 0; i < traceRouteCount_; ++i) {
        for (uint8_t j = i + 1; j < traceRouteCount_; ++j) {
            const auto &a = nodeDB->meshNodes->at(traceRouteOrder_[i]);
            const auto &b = nodeDB->meshNodes->at(traceRouteOrder_[j]);
            if (b.last_heard > a.last_heard) {
                const uint16_t previous = traceRouteOrder_[i];
                traceRouteOrder_[i] = traceRouteOrder_[j];
                traceRouteOrder_[j] = previous;
            }
        }
    }
    return traceRouteCount_;
}

const meshtastic_NodeInfoLite *HermesXNodeBrowserDataSource::onlineNodeAt(uint8_t index) const
{
    const auto &state = HermesXNodeBrowserUiModel::instance().onlineState();
    if (!nodeDB || !nodeDB->meshNodes || index >= state.count) {
        return nullptr;
    }
    return &nodeDB->meshNodes->at(state.order[index]);
}

const meshtastic_NodeInfoLite *HermesXNodeBrowserDataSource::finderNodeAt(uint8_t index) const
{
    const auto &state = HermesXNodeBrowserUiModel::instance().finderState();
    if (!nodeDB || !nodeDB->meshNodes || index >= state.count) {
        return nullptr;
    }
    return &nodeDB->meshNodes->at(state.order[index]);
}

const meshtastic_NodeInfoLite *HermesXNodeBrowserDataSource::traceRouteNodeAt(uint8_t index) const
{
    if (!nodeDB || !nodeDB->meshNodes || index >= traceRouteCount_) {
        return nullptr;
    }
    return &nodeDB->meshNodes->at(traceRouteOrder_[index]);
}

const meshtastic_NodeInfoLite *HermesXNodeBrowserDataSource::nodeByNum(uint32_t nodeNum) const
{
    return nodeDB ? nodeDB->getMeshNode(nodeNum) : nullptr;
}

const meshtastic_NodeInfoLite *HermesXNodeBrowserDataSource::selectedOnlineNode()
{
    refreshOnline();
    const auto &state = HermesXNodeBrowserUiModel::instance().onlineState();
    return state.selectedIndex < state.count ? onlineNodeAt(state.selectedIndex) : nullptr;
}

const meshtastic_NodeInfoLite *HermesXNodeBrowserDataSource::selectedFinderNode()
{
    refreshFinder();
    const auto &state = HermesXNodeBrowserUiModel::instance().finderState();
    return state.selectedIndex < state.count ? finderNodeAt(state.selectedIndex) : nullptr;
}

uint32_t HermesXNodeBrowserDataSource::findTraceRouteNodeByShortName(const String &shortName) const
{
    if (shortName.length() == 0) {
        return 0;
    }
    for (uint8_t index = 0; index < traceRouteCount_; ++index) {
        const auto *node = traceRouteNodeAt(index);
        if (node && node->has_user && String(node->user.short_name).equalsIgnoreCase(shortName)) {
            return node->num;
        }
    }
    return 0;
}

} // namespace graphics
