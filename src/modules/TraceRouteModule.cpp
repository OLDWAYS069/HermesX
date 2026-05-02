#include "TraceRouteModule.h"
#include "MeshService.h"
#include "main.h"
#include "meshUtils.h"

TraceRouteModule *traceRouteModule;

static String formatTraceRouteNodeLabel(NodeNum nodeNum)
{
    meshtastic_NodeInfoLite *node = nodeDB ? nodeDB->getMeshNode(nodeNum) : nullptr;
    if (node && node->has_user) {
        if (node->user.short_name[0] != '\0') {
            return String(node->user.short_name);
        }
        if (node->user.long_name[0] != '\0') {
            return String(node->user.long_name);
        }
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "!%08lx", static_cast<unsigned long>(nodeNum));
    return String(buf);
}

static void appendTraceRouteHop(String &out, NodeNum from, NodeNum to, int8_t snrQdb, bool hasSnr)
{
    out += formatTraceRouteNodeLabel(from);
    out += " -> ";
    out += formatTraceRouteNodeLabel(to);
    if (hasSnr && snrQdb != INT8_MIN) {
        char snrBuf[24];
        snprintf(snrBuf, sizeof(snrBuf), "  SNR %.2f dB", static_cast<float>(snrQdb) / 4.0f);
        out += snrBuf;
    } else {
        out += "  SNR ?";
    }
    out += "\n";
}

static String buildTraceRoutePopupBody(const meshtastic_MeshPacket &mp, const meshtastic_RouteDiscovery &r)
{
    String body;
    char rxBuf[48];
    snprintf(rxBuf, sizeof(rxBuf), "RX RSSI %d dBm / SNR %.2f dB\n\n", mp.rx_rssi, mp.rx_snr);
    body += rxBuf;
    body += u8"去程\n";

    NodeNum previous = mp.to;
    for (uint8_t i = 0; i < r.route_count; ++i) {
        const bool hasSnr = i < r.snr_towards_count;
        appendTraceRouteHop(body, previous, r.route[i], hasSnr ? r.snr_towards[i] : INT8_MIN, hasSnr);
        previous = r.route[i];
    }
    {
        const bool hasSnr = r.snr_towards_count > 0;
        const int8_t lastSnr = hasSnr ? r.snr_towards[r.snr_towards_count - 1] : INT8_MIN;
        appendTraceRouteHop(body, previous, mp.from, lastSnr, hasSnr);
    }

    if (r.route_back_count > 0) {
        body += "\n";
        body += u8"回程\n";
        previous = mp.from;
        for (uint8_t i = 0; i < r.route_back_count; ++i) {
            const bool hasSnr = i < r.snr_back_count;
            appendTraceRouteHop(body, previous, r.route_back[i], hasSnr ? r.snr_back[i] : INT8_MIN, hasSnr);
            previous = r.route_back[i];
        }
        const bool hasSnr = r.snr_back_count > 0;
        appendTraceRouteHop(body, previous, mp.to, hasSnr ? r.snr_back[r.snr_back_count - 1] : INT8_MIN, hasSnr);
    }

    return body;
}

bool TraceRouteModule::handleReceivedProtobuf(const meshtastic_MeshPacket &mp, meshtastic_RouteDiscovery *r)
{
    if (screen && mp.decoded.request_id && r) {
        LOG_INFO("[TraceRoute] response from=%08lx to=%08lx req=%08lx route=%u back=%u rx_rssi=%d rx_snr=%.2f",
                 static_cast<unsigned long>(mp.from), static_cast<unsigned long>(mp.to),
                 static_cast<unsigned long>(mp.decoded.request_id), static_cast<unsigned>(r->route_count),
                 static_cast<unsigned>(r->route_back_count), mp.rx_rssi, mp.rx_snr);
        String title = String("TraceRoute ") + formatTraceRouteNodeLabel(mp.from);
        String body = buildTraceRoutePopupBody(mp, *r);
        screen->showTraceRouteResultPopup(title.c_str(), body.c_str());
    }
    // We only alter the packet in alterReceivedProtobuf()
    return false; // let it be handled by RoutingModule
}

void TraceRouteModule::alterReceivedProtobuf(meshtastic_MeshPacket &p, meshtastic_RouteDiscovery *r)
{
    const meshtastic_Data &incoming = p.decoded;

    // Insert unknown hops if necessary
    insertUnknownHops(p, r, !incoming.request_id);

    // Append ID and SNR. If the last hop is to us, we only need to append the SNR
    appendMyIDandSNR(r, p.rx_snr, !incoming.request_id, isToUs(&p));
    if (!incoming.request_id)
        printRoute(r, p.from, p.to, true);
    else
        printRoute(r, p.to, p.from, false);

    // Set updated route to the payload of the to be flooded packet
    p.decoded.payload.size =
        pb_encode_to_bytes(p.decoded.payload.bytes, sizeof(p.decoded.payload.bytes), &meshtastic_RouteDiscovery_msg, r);
}

void TraceRouteModule::insertUnknownHops(meshtastic_MeshPacket &p, meshtastic_RouteDiscovery *r, bool isTowardsDestination)
{
    pb_size_t *route_count;
    uint32_t *route;
    pb_size_t *snr_count;
    int8_t *snr_list;

    // Pick the correct route array and SNR list
    if (isTowardsDestination) {
        route_count = &r->route_count;
        route = r->route;
        snr_count = &r->snr_towards_count;
        snr_list = r->snr_towards;
    } else {
        route_count = &r->route_back_count;
        route = r->route_back;
        snr_count = &r->snr_back_count;
        snr_list = r->snr_back;
    }

    // Only insert unknown hops if hop_start is valid
    if (p.hop_start != 0 && p.hop_limit <= p.hop_start) {
        uint8_t hopsTaken = p.hop_start - p.hop_limit;
        int8_t diff = hopsTaken - *route_count;
        for (uint8_t i = 0; i < diff; i++) {
            if (*route_count < ROUTE_SIZE) {
                route[*route_count] = NODENUM_BROADCAST; // This will represent an unknown hop
                *route_count += 1;
            }
        }
        // Add unknown SNR values if necessary
        diff = *route_count - *snr_count;
        for (uint8_t i = 0; i < diff; i++) {
            if (*snr_count < ROUTE_SIZE) {
                snr_list[*snr_count] = INT8_MIN; // This will represent an unknown SNR
                *snr_count += 1;
            }
        }
    }
}

void TraceRouteModule::appendMyIDandSNR(meshtastic_RouteDiscovery *updated, float snr, bool isTowardsDestination, bool SNRonly)
{
    pb_size_t *route_count;
    uint32_t *route;
    pb_size_t *snr_count;
    int8_t *snr_list;

    // Pick the correct route array and SNR list
    if (isTowardsDestination) {
        route_count = &updated->route_count;
        route = updated->route;
        snr_count = &updated->snr_towards_count;
        snr_list = updated->snr_towards;
    } else {
        route_count = &updated->route_back_count;
        route = updated->route_back;
        snr_count = &updated->snr_back_count;
        snr_list = updated->snr_back;
    }

    if (*snr_count < ROUTE_SIZE) {
        snr_list[*snr_count] = (int8_t)(snr * 4); // Convert SNR to 1 byte
        *snr_count += 1;
    }
    if (SNRonly)
        return;

    // Length of route array can normally not be exceeded due to the max. hop_limit of 7
    if (*route_count < ROUTE_SIZE) {
        route[*route_count] = myNodeInfo.my_node_num;
        *route_count += 1;
    } else {
        LOG_WARN("Route exceeded maximum hop limit!"); // Are you bridging networks?
    }
}

void TraceRouteModule::printRoute(meshtastic_RouteDiscovery *r, uint32_t origin, uint32_t dest, bool isTowardsDestination)
{
#ifdef DEBUG_PORT
    std::string route = "Route traced:\n";
    route += vformat("0x%x --> ", origin);
    for (uint8_t i = 0; i < r->route_count; i++) {
        if (i < r->snr_towards_count && r->snr_towards[i] != INT8_MIN)
            route += vformat("0x%x (%.2fdB) --> ", r->route[i], (float)r->snr_towards[i] / 4);
        else
            route += vformat("0x%x (?dB) --> ", r->route[i]);
    }
    // If we are the destination, or it has already reached the destination, print it
    if (dest == nodeDB->getNodeNum() || !isTowardsDestination) {
        if (r->snr_towards_count > 0 && r->snr_towards[r->snr_towards_count - 1] != INT8_MIN)
            route += vformat("0x%x (%.2fdB)", dest, (float)r->snr_towards[r->snr_towards_count - 1] / 4);

        else
            route += vformat("0x%x (?dB)", dest);
    } else
        route += "...";

    // If there's a route back (or we are the destination as then the route is complete), print it
    if (r->route_back_count > 0 || origin == nodeDB->getNodeNum()) {
        route += "\n";
        if (r->snr_towards_count > 0 && origin == nodeDB->getNodeNum())
            route += vformat("(%.2fdB) 0x%x <-- ", (float)r->snr_back[r->snr_back_count - 1] / 4, origin);
        else
            route += "...";

        for (int8_t i = r->route_back_count - 1; i >= 0; i--) {
            if (i < r->snr_back_count && r->snr_back[i] != INT8_MIN)
                route += vformat("(%.2fdB) 0x%x <-- ", (float)r->snr_back[i] / 4, r->route_back[i]);
            else
                route += vformat("(?dB) 0x%x <-- ", r->route_back[i]);
        }
        route += vformat("0x%x", dest);
    }
    LOG_INFO(route.c_str());
#endif
}

meshtastic_MeshPacket *TraceRouteModule::allocReply()
{
    assert(currentRequest);

    // Ignore multi-hop broadcast requests
    if (isBroadcast(currentRequest->to) && currentRequest->hop_limit < currentRequest->hop_start) {
        ignoreRequest = true;
        return NULL;
    }

    // Copy the payload of the current request
    auto req = *currentRequest;
    const auto &p = req.decoded;
    meshtastic_RouteDiscovery scratch;
    meshtastic_RouteDiscovery *updated = NULL;
    memset(&scratch, 0, sizeof(scratch));
    pb_decode_from_bytes(p.payload.bytes, p.payload.size, &meshtastic_RouteDiscovery_msg, &scratch);
    updated = &scratch;

    // Create a MeshPacket with this payload and set it as the reply
    meshtastic_MeshPacket *reply = allocDataProtobuf(*updated);

    return reply;
}

TraceRouteModule::TraceRouteModule()
    : ProtobufModule("traceroute", meshtastic_PortNum_TRACEROUTE_APP, &meshtastic_RouteDiscovery_msg)
{
    ourPortNum = meshtastic_PortNum_TRACEROUTE_APP;
    isPromiscuous = true; // We need to update the route even if it is not destined to us
}
