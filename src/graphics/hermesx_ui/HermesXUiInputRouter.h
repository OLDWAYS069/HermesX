#pragma once

#include <cstdint>

namespace graphics
{

enum class HermesXUiInputTarget : uint8_t {
    None,
    UpdateModal,
    LowMemoryReminder,
    RotaryLockPopup,
    EmergencyConfirm,
    FinderPulseConfirm,
    FinderPulseSending,
    TraceRoutePopup,
    SetupDetailPopup,
    IncomingTextPopup,
    TakMode,
    ActionPage,
    FastSetup,
    RecentMessageList,
    RecentMessageDetail,
    FinderNodeList,
    FinderNodeDetail,
    OnlineNodeList,
    OnlineNodeDetail,
    TraceRouteNodeList,
    TraceRouteNodeDetail,
    GroupNodeList,
    GroupNodeDetail,
};

struct HermesXUiInputState {
    bool updateModal = false;
    bool lowMemoryReminder = false;
    bool rotaryLockPopup = false;
    bool emergencyConfirm = false;
    bool finderPulseConfirm = false;
    bool finderPulseSending = false;
    bool traceRoutePopup = false;
    bool setupDetailPopup = false;
    bool incomingTextPopup = false;
    bool takMode = false;
    bool actionPage = false;
    bool fastSetup = false;
    bool recentMessageList = false;
    bool recentMessageDetail = false;
    bool finderNodeList = false;
    bool finderNodeDetail = false;
    bool onlineNodeList = false;
    bool onlineNodeDetail = false;
    bool traceRouteNodeList = false;
    bool traceRouteNodeDetail = false;
    bool groupNodeList = false;
    bool groupNodeDetail = false;
};

class HermesXUiInputRouter
{
  public:
    static HermesXUiInputTarget selectTarget(const HermesXUiInputState &state);
};

} // namespace graphics
