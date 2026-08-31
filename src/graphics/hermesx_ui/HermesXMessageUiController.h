#pragma once

#include <cstdint>

namespace graphics
{

struct HermesXMessageInput {
    HermesXMessageInput(const char *sourceValue = nullptr, char codeValue = 0) : source(sourceValue), code(codeValue) {}

    const char *source;
    char code;
};

struct HermesXMessageInputBindings {
    HermesXMessageInputBindings(char pressValue = 0, char clockwiseValue = 0, char counterClockwiseValue = 0)
        : press(pressValue), clockwise(clockwiseValue), counterClockwise(counterClockwiseValue)
    {
    }

    char press;
    char clockwise;
    char counterClockwise;
};

struct HermesXMessagePopupLifecycleDecision {
    bool activated = false;
    bool dismissed = false;
    uint32_t fastUntilMs = 0;
};

struct HermesXMessageOverlayRuntimeDecision {
    bool popupActive = false;
    bool paletteRecoveryPending = false;
    bool uiSkipBlocked = false;
};

enum class HermesXMessageInputAction : uint8_t {
    Unhandled,
    Consumed,
    Refresh,
    BackToAction,
    OpenDetail,
    BackToList,
    DismissPopup,
    OpenPopupDetail,
};

class HermesXMessageUiController
{
  public:
    static HermesXMessageUiController &instance();

    HermesXMessageInputAction handlePopup(const HermesXMessageInput &event, const HermesXMessageInputBindings &bindings);
    HermesXMessageInputAction handleRecentList(const HermesXMessageInput &event,
                                               const HermesXMessageInputBindings &bindings);
    HermesXMessageInputAction handleRecentDetail(const HermesXMessageInput &event,
                                                 const HermesXMessageInputBindings &bindings,
                                                 uint16_t scrollStep);
    bool dismissPopup();
    HermesXMessagePopupLifecycleDecision updatePopupLifecycle(bool screenOn,
                                                              bool mainPageActive,
                                                              uint32_t nowMs,
                                                              uint32_t visibleDurationMs);
    HermesXMessageOverlayRuntimeDecision overlayRuntime() const;
    void consumePaletteRecovery() { paletteRecoveryPending_ = false; }

  private:
    HermesXMessageUiController() = default;

    bool paletteRecoveryPending_ = false;
};

} // namespace graphics
