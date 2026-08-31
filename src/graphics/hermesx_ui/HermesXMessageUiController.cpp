#include "HermesXMessageUiController.h"

#include "HermesXMessageUiModel.h"
#include "mesh/generated/meshtastic/module_config.pb.h"
#include <algorithm>
#include <cstring>

namespace graphics
{
namespace
{

using InputChar = meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar;

bool matches(const HermesXMessageInput &event, InputChar input)
{
    return event.code == static_cast<char>(input);
}

bool isRotary(const HermesXMessageInput &event)
{
    return event.source && std::strncmp(event.source, "rotEnc", 6) == 0;
}

int8_t listDirection(const HermesXMessageInput &event, const HermesXMessageInputBindings &bindings)
{
    const bool up = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
    const bool down = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
    const bool left = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT);
    const bool right = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT);
    const bool clockwise = bindings.clockwise != 0 && event.code == bindings.clockwise;
    const bool counterClockwise = bindings.counterClockwise != 0 && event.code == bindings.counterClockwise;

    if (isRotary(event)) {
        if (counterClockwise) {
            return -1;
        }
        if (clockwise) {
            return 1;
        }
        if (bindings.clockwise == 0 && bindings.counterClockwise == 0) {
            if (up || left) {
                return -1;
            }
            if (down || right) {
                return 1;
            }
        }
        return 0;
    }

    if (counterClockwise || up || left) {
        return -1;
    }
    if (clockwise || down || right) {
        return 1;
    }
    return 0;
}

int8_t detailDirection(const HermesXMessageInput &event, const HermesXMessageInputBindings &bindings)
{
    const bool up = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
    const bool down = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
    const bool clockwise = bindings.clockwise != 0 && event.code == bindings.clockwise;
    const bool counterClockwise = bindings.counterClockwise != 0 && event.code == bindings.counterClockwise;

    if (isRotary(event)) {
        if (counterClockwise) {
            return -1;
        }
        if (clockwise) {
            return 1;
        }
        if (bindings.clockwise == 0 && bindings.counterClockwise == 0) {
            if (up) {
                return -1;
            }
            if (down) {
                return 1;
            }
        }
        return 0;
    }

    if (up) {
        return -1;
    }
    if (down) {
        return 1;
    }
    return 0;
}

int8_t popupDirection(const HermesXMessageInput &event, const HermesXMessageInputBindings &bindings)
{
    const bool up = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
    const bool down = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
    const bool left = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT);
    const bool right = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT);
    const bool clockwise = bindings.clockwise != 0 && event.code == bindings.clockwise;
    const bool counterClockwise = bindings.counterClockwise != 0 && event.code == bindings.counterClockwise;
    if (counterClockwise || left || up) {
        return -1;
    }
    if (clockwise || right || down) {
        return 1;
    }
    return 0;
}

} // namespace

HermesXMessageUiController &HermesXMessageUiController::instance()
{
    static HermesXMessageUiController controller;
    return controller;
}

bool HermesXMessageUiController::dismissPopup()
{
    auto &popup = HermesXMessageUiModel::instance().popupState();
    const bool wasActive = popup.pending || popup.visible;
    popup.pending = false;
    popup.visible = false;
    popup.selectedOption = 0;
    popup.untilMs = 0;
    if (wasActive) {
        paletteRecoveryPending_ = true;
    }
    return wasActive;
}

HermesXMessagePopupLifecycleDecision HermesXMessageUiController::updatePopupLifecycle(
    bool screenOn,
    bool mainPageActive,
    uint32_t nowMs,
    uint32_t visibleDurationMs)
{
    auto &popup = HermesXMessageUiModel::instance().popupState();
    HermesXMessagePopupLifecycleDecision decision;
    if (popup.pending && screenOn && mainPageActive) {
        popup.pending = false;
        popup.visible = true;
        popup.untilMs = nowMs + visibleDurationMs;
        decision.activated = true;
        decision.fastUntilMs = popup.untilMs + 50;
    } else if (popup.visible && popup.untilMs != 0 && nowMs >= popup.untilMs) {
        decision.dismissed = dismissPopup();
    }
    return decision;
}

HermesXMessageOverlayRuntimeDecision HermesXMessageUiController::overlayRuntime() const
{
    const auto &popup = HermesXMessageUiModel::instance().popupState();
    HermesXMessageOverlayRuntimeDecision decision;
    decision.popupActive = popup.pending || popup.visible;
    decision.paletteRecoveryPending = paletteRecoveryPending_;
    decision.uiSkipBlocked = decision.popupActive || decision.paletteRecoveryPending;
    return decision;
}

HermesXMessageInputAction HermesXMessageUiController::handlePopup(const HermesXMessageInput &event,
                                                                  const HermesXMessageInputBindings &bindings)
{
    auto &model = HermesXMessageUiModel::instance();
    auto &popup = model.popupState();
    const bool select = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const bool configuredPress = bindings.press != 0 && event.code == bindings.press;
    const bool confirm = configuredPress || (bindings.press == 0 && select);
    const bool cancel = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK) ||
                        matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL);
    const int8_t direction = popupDirection(event, bindings);

    if (direction != 0) {
        popup.selectedOption = direction < 0 ? 0 : 1;
        return HermesXMessageInputAction::Refresh;
    }

    if (confirm && popup.selectedOption == 0) {
        const int popupIndex = model.findRecentMessageIndex(popup.packet);
        if (popupIndex >= 0) {
            auto &recent = model.recentState();
            recent.listCursor = static_cast<uint8_t>(popupIndex + 1);
            recent.selectedIndex = static_cast<uint8_t>(popupIndex);
            return HermesXMessageInputAction::OpenPopupDetail;
        }
        return HermesXMessageInputAction::DismissPopup;
    }

    if ((confirm && popup.selectedOption == 1) || cancel) {
        return HermesXMessageInputAction::DismissPopup;
    }

    return HermesXMessageInputAction::Consumed;
}

HermesXMessageInputAction HermesXMessageUiController::handleRecentList(const HermesXMessageInput &event,
                                                                       const HermesXMessageInputBindings &bindings)
{
    auto &model = HermesXMessageUiModel::instance();
    auto &recent = model.recentState();
    const int8_t direction = listDirection(event, bindings);

    if (direction != 0) {
        model.clampRecentIndices();
        const int lastCursor = static_cast<int>(model.recentListEntryCount()) - 1;
        const int nextCursor = std::max(0, std::min(static_cast<int>(recent.listCursor) + direction, lastCursor));
        if (nextCursor == recent.listCursor) {
            return HermesXMessageInputAction::Consumed;
        }
        recent.listCursor = static_cast<uint8_t>(nextCursor);
        if (nextCursor > 0) {
            recent.selectedIndex = static_cast<uint8_t>(nextCursor - 1);
        }
        return HermesXMessageInputAction::Refresh;
    }

    const bool select = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const bool press = bindings.press != 0 && event.code == bindings.press;
    if (select || press) {
        model.clampRecentIndices();
        if (recent.listCursor == 0) {
            return HermesXMessageInputAction::BackToAction;
        }
        recent.selectedIndex = recent.listCursor - 1;
        return HermesXMessageInputAction::OpenDetail;
    }

    if (matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL) ||
        matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK)) {
        return HermesXMessageInputAction::BackToAction;
    }
    return HermesXMessageInputAction::Unhandled;
}

HermesXMessageInputAction HermesXMessageUiController::handleRecentDetail(const HermesXMessageInput &event,
                                                                         const HermesXMessageInputBindings &bindings,
                                                                         uint16_t scrollStep)
{
    auto &recent = HermesXMessageUiModel::instance().recentState();
    const int8_t direction = detailDirection(event, bindings);
    if (direction != 0) {
        if (recent.detailMaxScrollY == 0) {
            return HermesXMessageInputAction::Consumed;
        }
        const int nextScroll = std::max(
            0, std::min(static_cast<int>(recent.detailScrollY) + direction * static_cast<int>(scrollStep),
                        static_cast<int>(recent.detailMaxScrollY)));
        if (nextScroll == recent.detailScrollY) {
            return HermesXMessageInputAction::Consumed;
        }
        recent.detailScrollY = static_cast<uint16_t>(nextScroll);
        return HermesXMessageInputAction::Refresh;
    }

    const bool back = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL) ||
                      matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK) ||
                      matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT) ||
                      matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT) ||
                      matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT) ||
                      (bindings.press != 0 && event.code == bindings.press);
    return back ? HermesXMessageInputAction::BackToList : HermesXMessageInputAction::Unhandled;
}

} // namespace graphics
