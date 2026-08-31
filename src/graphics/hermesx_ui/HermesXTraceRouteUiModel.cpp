#include "HermesXTraceRouteUiModel.h"

namespace graphics
{

HermesXTraceRouteUiModel &HermesXTraceRouteUiModel::instance()
{
    static HermesXTraceRouteUiModel model;
    return model;
}

void HermesXTraceRouteUiModel::showPopup(const char *title, const char *body, uint32_t nowMs)
{
    popup_.title = (title && title[0] != '\0') ? String(title) : String("TraceRoute");
    popup_.body = (body && body[0] != '\0') ? String(body) : String(u8"沒有可顯示的路徑資料");
    popup_.visible = true;
    popup_.scrollY = 0;
    popup_.maxScrollY = 0;
    popup_.shownAtMs = nowMs;
}

void HermesXTraceRouteUiModel::dismissPopup()
{
    popup_ = HermesXTraceRoutePopupState{};
}

void HermesXTraceRouteUiModel::startRequest(uint32_t destination, uint32_t requestId, uint32_t nowMs)
{
    request_.destination = destination;
    request_.requestId = requestId;
    request_.pending = true;
    request_.startedMs = nowMs;
}

void HermesXTraceRouteUiModel::clearRequest()
{
    request_ = HermesXTraceRouteRequestState{};
}

bool HermesXTraceRouteUiModel::isExpectedResult(uint32_t source, uint32_t requestId) const
{
    return request_.pending && source != 0 && requestId != 0 && request_.destination == source &&
           request_.requestId == requestId;
}

void HermesXTraceRouteUiModel::beginSearch()
{
    search_ = HermesXTraceRouteSearchState{};
    search_.active = true;
}

void HermesXTraceRouteUiModel::resetSearch()
{
    search_ = HermesXTraceRouteSearchState{};
}

void HermesXTraceRouteUiModel::showSearchResult(const String &query, bool found, uint32_t node)
{
    search_ = HermesXTraceRouteSearchState{};
    search_.resultVisible = true;
    search_.resultFound = found;
    search_.resultNode = node;
    search_.resultCursor = found ? 0 : 1;
    search_.resultQuery = query;
}

void HermesXTraceRouteUiModel::resetNavigation()
{
    navigation_ = HermesXTraceRouteNavigationState{};
}

void HermesXTraceRouteUiModel::enterMenu()
{
    navigation_.mode = HermesXTraceRouteUiMode::Menu;
    cancelDeferredBindShortPress();
    cancelDeferredRouteShortPress();
    dismissBindConfirm();
}

void HermesXTraceRouteUiModel::enterBindList()
{
    navigation_.mode = HermesXTraceRouteUiMode::BindOnline;
    navigation_.bindCursor = 0;
    navigation_.bindSelectedIndex = 0;
    cancelDeferredBindShortPress();
    dismissBindConfirm();
}

void HermesXTraceRouteUiModel::enterBoundRoutes()
{
    navigation_.mode = HermesXTraceRouteUiMode::BoundRoutes;
    navigation_.boundCursor = 0;
    navigation_.boundSelectedIndex = 0;
    cancelDeferredRouteShortPress();
}

void HermesXTraceRouteUiModel::showBindConfirm(uint32_t node)
{
    cancelDeferredBindShortPress();
    navigation_.confirmNode = node;
    navigation_.confirmSelected = 1;
    navigation_.confirmVisible = node != 0;
}

void HermesXTraceRouteUiModel::dismissBindConfirm()
{
    navigation_.confirmVisible = false;
    navigation_.confirmNode = 0;
    navigation_.confirmSelected = 1;
}

void HermesXTraceRouteUiModel::deferBindShortPress(uint8_t cursor, uint32_t node)
{
    navigation_.deferredBindVisible = node != 0;
    navigation_.deferredBindCursor = cursor;
    navigation_.deferredBindNode = node;
}

void HermesXTraceRouteUiModel::cancelDeferredBindShortPress()
{
    navigation_.deferredBindVisible = false;
    navigation_.deferredBindCursor = 0;
    navigation_.deferredBindNode = 0;
}

void HermesXTraceRouteUiModel::deferRouteShortPress(uint8_t cursor, uint32_t node)
{
    navigation_.deferredRouteVisible = node != 0;
    navigation_.deferredRouteCursor = cursor;
    navigation_.deferredRouteNode = node;
}

void HermesXTraceRouteUiModel::cancelDeferredRouteShortPress()
{
    navigation_.deferredRouteVisible = false;
    navigation_.deferredRouteCursor = 0;
    navigation_.deferredRouteNode = 0;
}

void HermesXTraceRouteUiModel::clampBoundSelection(uint8_t count)
{
    if (navigation_.boundCursor > count) {
        navigation_.boundCursor = count;
    }
    if (count == 0) {
        navigation_.boundSelectedIndex = 0;
    } else if (navigation_.boundSelectedIndex >= count) {
        navigation_.boundSelectedIndex = count - 1;
    }
}

} // namespace graphics
