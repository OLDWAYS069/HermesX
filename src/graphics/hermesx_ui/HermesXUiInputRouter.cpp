#include "HermesXUiInputRouter.h"

namespace graphics
{

HermesXUiInputTarget HermesXUiInputRouter::selectTarget(const HermesXUiInputState &state)
{
    if (state.updateModal)
        return HermesXUiInputTarget::UpdateModal;
    if (state.lowMemoryReminder)
        return HermesXUiInputTarget::LowMemoryReminder;
    if (state.rotaryLockPopup)
        return HermesXUiInputTarget::RotaryLockPopup;
    if (state.emergencyConfirm)
        return HermesXUiInputTarget::EmergencyConfirm;
    if (state.finderPulseConfirm)
        return HermesXUiInputTarget::FinderPulseConfirm;
    if (state.finderPulseSending)
        return HermesXUiInputTarget::FinderPulseSending;
    if (state.traceRoutePopup)
        return HermesXUiInputTarget::TraceRoutePopup;
    if (state.setupDetailPopup)
        return HermesXUiInputTarget::SetupDetailPopup;
    if (state.incomingTextPopup)
        return HermesXUiInputTarget::IncomingTextPopup;
    if (state.takMode)
        return HermesXUiInputTarget::TakMode;
    if (state.actionPage)
        return HermesXUiInputTarget::ActionPage;
    if (state.fastSetup)
        return HermesXUiInputTarget::FastSetup;
    if (state.recentMessageList)
        return HermesXUiInputTarget::RecentMessageList;
    if (state.recentMessageDetail)
        return HermesXUiInputTarget::RecentMessageDetail;
    if (state.finderNodeList)
        return HermesXUiInputTarget::FinderNodeList;
    if (state.finderNodeDetail)
        return HermesXUiInputTarget::FinderNodeDetail;
    if (state.onlineNodeList)
        return HermesXUiInputTarget::OnlineNodeList;
    if (state.onlineNodeDetail)
        return HermesXUiInputTarget::OnlineNodeDetail;
    if (state.traceRouteNodeList)
        return HermesXUiInputTarget::TraceRouteNodeList;
    if (state.traceRouteNodeDetail)
        return HermesXUiInputTarget::TraceRouteNodeDetail;
    if (state.groupNodeList)
        return HermesXUiInputTarget::GroupNodeList;
    if (state.groupNodeDetail)
        return HermesXUiInputTarget::GroupNodeDetail;
    return HermesXUiInputTarget::None;
}

} // namespace graphics
