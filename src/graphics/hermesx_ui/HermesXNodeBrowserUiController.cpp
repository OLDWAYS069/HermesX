#include "HermesXNodeBrowserUiController.h"

#include "mesh/generated/meshtastic/module_config.pb.h"
#include <algorithm>
#include <cstring>

namespace graphics
{
namespace
{

using InputChar = meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar;
constexpr uint32_t PulseNavMinIntervalMs = 80;
constexpr uint32_t PulseNavFlipGuardMs = 800;

bool matches(const HermesXNodeBrowserInput &event, InputChar input)
{
    return event.code == static_cast<char>(input);
}

bool isRotary(const HermesXNodeBrowserInput &event)
{
    return event.source && std::strncmp(event.source, "rotEnc", 6) == 0;
}

bool isSelect(const HermesXNodeBrowserInput &event, const HermesXNodeBrowserInputBindings &bindings)
{
    return matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT) ||
           (bindings.press != 0 && event.code == bindings.press);
}

bool isCancel(const HermesXNodeBrowserInput &event)
{
    return matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL) ||
           matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK);
}

int8_t listDirection(const HermesXNodeBrowserInput &event, const HermesXNodeBrowserInputBindings &bindings)
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

int8_t detailDirection(const HermesXNodeBrowserInput &event, const HermesXNodeBrowserInputBindings &bindings)
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
    if (up || counterClockwise) {
        return -1;
    }
    if (down || clockwise) {
        return 1;
    }
    return 0;
}

int8_t pulseDirection(const HermesXNodeBrowserInput &event, const HermesXNodeBrowserInputBindings &bindings)
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
    if (left || counterClockwise) {
        return -1;
    }
    if (right || clockwise) {
        return 1;
    }
    return 0;
}

HermesXNodeBrowserAction moveCursor(uint8_t &cursor, uint8_t itemCount, int8_t direction)
{
    if (itemCount == 0) {
        cursor = 0;
        return HermesXNodeBrowserAction::Consumed;
    }
    const int next = std::max(0, std::min(static_cast<int>(cursor) + direction, static_cast<int>(itemCount) - 1));
    if (next == cursor) {
        return HermesXNodeBrowserAction::Consumed;
    }
    cursor = static_cast<uint8_t>(next);
    return HermesXNodeBrowserAction::Refresh;
}

} // namespace

HermesXNodeBrowserUiController &HermesXNodeBrowserUiController::instance()
{
    static HermesXNodeBrowserUiController controller;
    return controller;
}

HermesXNodeBrowserAction HermesXNodeBrowserUiController::handleMenu(
    const HermesXNodeBrowserInput &event,
    const HermesXNodeBrowserInputBindings &bindings,
    uint8_t &cursor,
    uint8_t itemCount)
{
    const int8_t direction = listDirection(event, bindings);
    if (direction != 0) {
        return moveCursor(cursor, itemCount, direction);
    }
    if (isCancel(event) || (isSelect(event, bindings) && cursor == 0)) {
        return HermesXNodeBrowserAction::ExitRequested;
    }
    if (isSelect(event, bindings)) {
        return HermesXNodeBrowserAction::ActivateRequested;
    }
    return HermesXNodeBrowserAction::Unhandled;
}

HermesXNodeBrowserAction HermesXNodeBrowserUiController::handleList(
    const HermesXNodeBrowserInput &event,
    const HermesXNodeBrowserInputBindings &bindings,
    HermesXNodeBrowserState &state)
{
    const int8_t direction = listDirection(event, bindings);
    if (direction != 0) {
        const HermesXNodeBrowserAction action = moveCursor(state.listCursor, state.count + 1, direction);
        if (state.listCursor > 0) {
            state.selectedIndex = state.listCursor - 1;
        }
        return action;
    }
    if (isCancel(event)) {
        return HermesXNodeBrowserAction::CancelRequested;
    }
    if (isSelect(event, bindings) && state.listCursor == 0) {
        return HermesXNodeBrowserAction::ExitRequested;
    }
    if (isSelect(event, bindings)) {
        state.selectedIndex = state.listCursor - 1;
        state.detailCursor = 0;
        return HermesXNodeBrowserAction::OpenDetailRequested;
    }
    return HermesXNodeBrowserAction::Unhandled;
}

HermesXNodeBrowserAction HermesXNodeBrowserUiController::handleGroupList(
    const HermesXNodeBrowserInput &event,
    const HermesXNodeBrowserInputBindings &bindings,
    HermesXGroupBrowserState &state,
    uint8_t nodeCount)
{
    const int8_t direction = listDirection(event, bindings);
    if (direction != 0) {
        const HermesXNodeBrowserAction action = moveCursor(state.listCursor, nodeCount + 1, direction);
        if (state.listCursor > 0) {
            state.selectedIndex = state.listCursor - 1;
        }
        return action;
    }
    if (isCancel(event)) {
        return HermesXNodeBrowserAction::CancelRequested;
    }
    if (isSelect(event, bindings) && state.listCursor == 0) {
        return HermesXNodeBrowserAction::ExitRequested;
    }
    if (isSelect(event, bindings)) {
        state.selectedIndex = state.listCursor - 1;
        state.detailCursor = 0;
        return HermesXNodeBrowserAction::OpenDetailRequested;
    }
    return HermesXNodeBrowserAction::Unhandled;
}

HermesXNodeBrowserAction HermesXNodeBrowserUiController::handleDetail(
    const HermesXNodeBrowserInput &event,
    const HermesXNodeBrowserInputBindings &bindings,
    uint8_t &cursor,
    uint8_t rowCount)
{
    if (rowCount == 0) {
        cursor = 0;
        return HermesXNodeBrowserAction::BackToListRequested;
    }
    if (cursor >= rowCount) {
        cursor = rowCount - 1;
    }
    const int8_t direction = detailDirection(event, bindings);
    if (direction != 0) {
        return moveCursor(cursor, rowCount, direction);
    }
    if (isCancel(event) || matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT) ||
        matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT) ||
        (isSelect(event, bindings) && cursor == 0)) {
        return HermesXNodeBrowserAction::BackToListRequested;
    }
    if (isSelect(event, bindings)) {
        return HermesXNodeBrowserAction::ActivateRequested;
    }
    return HermesXNodeBrowserAction::Unhandled;
}

void HermesXNodeBrowserUiController::beginFinderPulseConfirm(uint32_t nowMs)
{
    auto &state = HermesXNodeBrowserUiModel::instance().finderPulseState();
    state = HermesXFinderPulseState{};
    state.confirmVisible = true;
    state.confirmShownAtMs = nowMs;
}

bool HermesXNodeBrowserUiController::consumeFinderPulseAutoBroadcast(uint32_t nowMs, uint32_t armDelayMs)
{
    auto &state = HermesXNodeBrowserUiModel::instance().finderPulseState();
    if (!state.confirmVisible || state.dispatched || state.confirmShownAtMs == 0 ||
        nowMs - state.confirmShownAtMs < armDelayMs) {
        return false;
    }
    state.dispatched = true;
    state.confirmVisible = false;
    state.confirmSelected = 0;
    state.confirmShownAtMs = 0;
    return true;
}

void HermesXNodeBrowserUiController::setFinderPulseRequestStarted(bool started, uint32_t nowMs)
{
    auto &state = HermesXNodeBrowserUiModel::instance().finderPulseState();
    state.sendingVisible = started;
    state.sendingShownAtMs = started ? nowMs : 0;
}

void HermesXNodeBrowserUiController::finishFinderPulseRequest()
{
    auto &state = HermesXNodeBrowserUiModel::instance().finderPulseState();
    state.sendingVisible = false;
    state.sendingShownAtMs = 0;
}

void HermesXNodeBrowserUiController::clearFinderPulse()
{
    HermesXNodeBrowserUiModel::instance().finderPulseState() = HermesXFinderPulseState{};
}

HermesXFinderPulseAction HermesXNodeBrowserUiController::handleFinderPulseConfirm(
    const HermesXNodeBrowserInput &event,
    const HermesXNodeBrowserInputBindings &bindings,
    uint32_t nowMs,
    uint32_t armDelayMs)
{
    auto &state = HermesXNodeBrowserUiModel::instance().finderPulseState();
    if (!state.confirmVisible) {
        return HermesXFinderPulseAction::Unhandled;
    }
    if (isCancel(event)) {
        clearFinderPulse();
        return HermesXFinderPulseAction::CancelRequested;
    }

    const bool broadcastArmed = state.confirmShownAtMs == 0 || nowMs - state.confirmShownAtMs >= armDelayMs;
    const int8_t direction = pulseDirection(event, bindings);
    if (direction != 0) {
        if (!isRotary(event)) {
            if (state.lastNavAtMs != 0 && nowMs - state.lastNavAtMs < PulseNavMinIntervalMs) {
                return HermesXFinderPulseAction::Consumed;
            }
            if (state.lastNavDir != 0 && direction != state.lastNavDir &&
                nowMs - state.lastNavAtMs < PulseNavFlipGuardMs) {
                return HermesXFinderPulseAction::Consumed;
            }
        }
        state.lastNavAtMs = nowMs;
        state.lastNavDir = direction;
        state.confirmSelected = (!broadcastArmed || direction < 0) ? 0 : 1;
        return HermesXFinderPulseAction::Refresh;
    }

    if (isSelect(event, bindings)) {
        const bool broadcast = broadcastArmed && state.confirmSelected != 0;
        clearFinderPulse();
        return broadcast ? HermesXFinderPulseAction::BroadcastRequested : HermesXFinderPulseAction::CancelRequested;
    }
    return HermesXFinderPulseAction::Consumed;
}

HermesXFinderPulseAction HermesXNodeBrowserUiController::handleFinderPulseSending(
    const HermesXNodeBrowserInput &event,
    const HermesXNodeBrowserInputBindings &bindings)
{
    const auto &state = HermesXNodeBrowserUiModel::instance().finderPulseState();
    if (!state.sendingVisible) {
        return HermesXFinderPulseAction::Unhandled;
    }
    if (isCancel(event) || matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT)) {
        clearFinderPulse();
        return HermesXFinderPulseAction::CancelRequested;
    }
    (void)bindings;
    return HermesXFinderPulseAction::Consumed;
}

} // namespace graphics
