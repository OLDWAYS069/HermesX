#pragma once

#include "HermesXNodeBrowserUiModel.h"
#include <cstdint>

namespace graphics
{

struct HermesXNodeBrowserInput {
    HermesXNodeBrowserInput(const char *sourceValue = nullptr, char codeValue = 0)
        : source(sourceValue), code(codeValue)
    {
    }

    const char *source;
    char code;
};

struct HermesXNodeBrowserInputBindings {
    HermesXNodeBrowserInputBindings(char pressValue = 0, char clockwiseValue = 0, char counterClockwiseValue = 0)
        : press(pressValue), clockwise(clockwiseValue), counterClockwise(counterClockwiseValue)
    {
    }

    char press;
    char clockwise;
    char counterClockwise;
};

enum class HermesXNodeBrowserAction : uint8_t {
    Unhandled,
    Consumed,
    Refresh,
    ExitRequested,
    CancelRequested,
    OpenDetailRequested,
    BackToListRequested,
    ActivateRequested,
};

enum class HermesXFinderPulseAction : uint8_t {
    Unhandled,
    Consumed,
    Refresh,
    CancelRequested,
    BroadcastRequested,
};

class HermesXNodeBrowserUiController
{
  public:
    static HermesXNodeBrowserUiController &instance();

    HermesXNodeBrowserAction handleMenu(const HermesXNodeBrowserInput &event,
                                        const HermesXNodeBrowserInputBindings &bindings,
                                        uint8_t &cursor,
                                        uint8_t itemCount);
    HermesXNodeBrowserAction handleList(const HermesXNodeBrowserInput &event,
                                        const HermesXNodeBrowserInputBindings &bindings,
                                        HermesXNodeBrowserState &state);
    HermesXNodeBrowserAction handleGroupList(const HermesXNodeBrowserInput &event,
                                             const HermesXNodeBrowserInputBindings &bindings,
                                             HermesXGroupBrowserState &state,
                                             uint8_t nodeCount);
    HermesXNodeBrowserAction handleDetail(const HermesXNodeBrowserInput &event,
                                          const HermesXNodeBrowserInputBindings &bindings,
                                          uint8_t &cursor,
                                          uint8_t rowCount);
    void beginFinderPulseConfirm(uint32_t nowMs);
    bool consumeFinderPulseAutoBroadcast(uint32_t nowMs, uint32_t armDelayMs);
    void setFinderPulseRequestStarted(bool started, uint32_t nowMs);
    void finishFinderPulseRequest();
    void clearFinderPulse();
    HermesXFinderPulseAction handleFinderPulseConfirm(const HermesXNodeBrowserInput &event,
                                                      const HermesXNodeBrowserInputBindings &bindings,
                                                      uint32_t nowMs,
                                                      uint32_t armDelayMs);
    HermesXFinderPulseAction handleFinderPulseSending(const HermesXNodeBrowserInput &event,
                                                      const HermesXNodeBrowserInputBindings &bindings);

  private:
    HermesXNodeBrowserUiController() = default;
};

} // namespace graphics
