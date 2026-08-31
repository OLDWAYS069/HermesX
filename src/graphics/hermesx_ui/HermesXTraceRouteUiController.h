#pragma once

#include <cstdint>

namespace graphics
{

struct HermesXTraceRoutePopupInput {
    HermesXTraceRoutePopupInput(const char *sourceValue = nullptr, char codeValue = 0)
        : source(sourceValue), code(codeValue)
    {
    }

    const char *source;
    char code;
};

struct HermesXTraceRoutePopupBindings {
    HermesXTraceRoutePopupBindings(char pressValue = 0, char clockwiseValue = 0, char counterClockwiseValue = 0)
        : press(pressValue), clockwise(clockwiseValue), counterClockwise(counterClockwiseValue)
    {
    }

    char press;
    char clockwise;
    char counterClockwise;
};

enum class HermesXTraceRoutePopupAction : uint8_t {
    Consumed,
    Refresh,
    DismissRequested,
};

enum class HermesXTraceRouteSearchAction : uint8_t {
    Consumed,
    Refresh,
    CloseRequested,
    SearchRequested,
    ExitResultRequested,
    BindResultRequested,
};

enum class HermesXTraceRouteListAction : uint8_t {
    Ignored,
    Consumed,
    Refresh,
    ExitFeatureRequested,
    OpenSearchRequested,
    ShowBindInfoRequested,
    DeferBindInfoRequested,
    OpenBoundRoutesRequested,
    BindConfirmed,
};

enum class HermesXTraceRouteBoundListAction : uint8_t {
    Ignored,
    Consumed,
    Refresh,
    ReturnToMenuRequested,
    SendRequested,
    DeferSendRequested,
};

class HermesXTraceRouteUiController
{
  public:
    static HermesXTraceRouteUiController &instance();

    HermesXTraceRoutePopupAction handlePopup(const HermesXTraceRoutePopupInput &event,
                                              const HermesXTraceRoutePopupBindings &bindings,
                                              uint32_t nowMs,
                                              uint16_t scrollStep,
                                              uint32_t dismissGuardMs);
    HermesXTraceRouteSearchAction handleSearch(const HermesXTraceRoutePopupInput &event,
                                               const HermesXTraceRoutePopupBindings &bindings,
                                               uint32_t nowMs);
    HermesXTraceRouteListAction handleList(const HermesXTraceRoutePopupInput &event,
                                           const HermesXTraceRoutePopupBindings &bindings,
                                           uint8_t onlineNodeCount);
    HermesXTraceRouteBoundListAction handleBoundList(const HermesXTraceRoutePopupInput &event,
                                                     const HermesXTraceRoutePopupBindings &bindings,
                                                     uint8_t boundNodeCount);
    const char *const (*searchKeyRows() const)[10];
    const uint8_t *searchKeyRowLengths() const;
    uint8_t searchKeyRowCount() const;

  private:
    HermesXTraceRouteUiController() = default;
};

} // namespace graphics
