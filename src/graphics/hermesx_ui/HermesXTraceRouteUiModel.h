#pragma once

#include <Arduino.h>
#include <cstdint>

namespace graphics
{

struct HermesXTraceRoutePopupState {
    String title;
    String body;
    bool visible = false;
    uint16_t scrollY = 0;
    uint16_t maxScrollY = 0;
    uint32_t shownAtMs = 0;
};

struct HermesXTraceRouteRequestState {
    uint32_t destination = 0;
    uint32_t requestId = 0;
    bool pending = false;
    uint32_t startedMs = 0;
};

struct HermesXTraceRouteSearchState {
    bool active = false;
    bool resultVisible = false;
    bool resultFound = false;
    uint32_t resultNode = 0;
    uint8_t resultCursor = 0;
    String draft;
    String resultQuery;
    String toast;
    uint32_t toastUntilMs = 0;
    uint8_t keyRow = 0;
    uint8_t keyCol = 0;
};

enum class HermesXTraceRouteUiMode : uint8_t {
    Menu,
    BindOnline,
    BoundRoutes,
};

struct HermesXTraceRouteNavigationState {
    HermesXTraceRouteUiMode mode = HermesXTraceRouteUiMode::Menu;
    uint8_t menuCursor = 1;
    uint8_t bindCursor = 0;
    uint8_t bindSelectedIndex = 0;
    uint8_t boundCursor = 0;
    uint8_t boundSelectedIndex = 0;
    bool confirmVisible = false;
    uint8_t confirmSelected = 1;
    uint32_t confirmNode = 0;
    bool deferredBindVisible = false;
    uint8_t deferredBindCursor = 0;
    uint32_t deferredBindNode = 0;
    bool deferredRouteVisible = false;
    uint8_t deferredRouteCursor = 0;
    uint32_t deferredRouteNode = 0;
};

class HermesXTraceRouteUiModel
{
  public:
    static HermesXTraceRouteUiModel &instance();

    HermesXTraceRoutePopupState &popupState() { return popup_; }
    const HermesXTraceRoutePopupState &popupState() const { return popup_; }
    HermesXTraceRouteRequestState &requestState() { return request_; }
    const HermesXTraceRouteRequestState &requestState() const { return request_; }
    HermesXTraceRouteSearchState &searchState() { return search_; }
    const HermesXTraceRouteSearchState &searchState() const { return search_; }
    HermesXTraceRouteNavigationState &navigationState() { return navigation_; }
    const HermesXTraceRouteNavigationState &navigationState() const { return navigation_; }

    void showPopup(const char *title, const char *body, uint32_t nowMs);
    void dismissPopup();
    void startRequest(uint32_t destination, uint32_t requestId, uint32_t nowMs);
    void clearRequest();
    bool isExpectedResult(uint32_t source, uint32_t requestId) const;
    void beginSearch();
    void resetSearch();
    void showSearchResult(const String &query, bool found, uint32_t node);
    void resetNavigation();
    void enterMenu();
    void enterBindList();
    void enterBoundRoutes();
    void showBindConfirm(uint32_t node);
    void dismissBindConfirm();
    void deferBindShortPress(uint8_t cursor, uint32_t node);
    void cancelDeferredBindShortPress();
    void deferRouteShortPress(uint8_t cursor, uint32_t node);
    void cancelDeferredRouteShortPress();
    void clampBoundSelection(uint8_t count);

  private:
    HermesXTraceRouteUiModel() = default;

    HermesXTraceRoutePopupState popup_;
    HermesXTraceRouteRequestState request_;
    HermesXTraceRouteSearchState search_;
    HermesXTraceRouteNavigationState navigation_;
};

} // namespace graphics
