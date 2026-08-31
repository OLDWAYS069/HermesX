#include "HermesXTakModeUiController.h"

#include "mesh/generated/meshtastic/module_config.pb.h"
#include <cstring>

namespace graphics
{
namespace
{

using InputChar = meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar;

bool matches(const HermesXTakModeInput &event, InputChar input)
{
    return event.code == static_cast<char>(input);
}

bool isRotary(const HermesXTakModeInput &event)
{
    return event.source && std::strncmp(event.source, "rotEnc", 6) == 0;
}

bool isRotaryOne(const HermesXTakModeInput &event)
{
    return event.source && std::strcmp(event.source, "rotEnc1") == 0;
}

bool isSelect(const HermesXTakModeInput &event, const HermesXTakModeInputBindings &bindings)
{
    return matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT) ||
           (bindings.press != 0 && event.code == bindings.press);
}

bool isCancel(const HermesXTakModeInput &event)
{
    return matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL) ||
           matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK);
}

int8_t navigationDirection(const HermesXTakModeInput &event, const HermesXTakModeInputBindings &bindings)
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
        if (!isRotaryOne(event)) {
            if (up || left) {
                return -1;
            }
            if (down || right) {
                return 1;
            }
        }
        return 0;
    }
    if (up || left || counterClockwise) {
        return -1;
    }
    if (down || right || clockwise) {
        return 1;
    }
    return 0;
}

void wrapSelection(uint8_t &selected, uint8_t rowCount, int8_t direction)
{
    if (rowCount == 0) {
        selected = 0;
        return;
    }
    int next = static_cast<int>(selected) + direction;
    if (next < 0) {
        next = rowCount - 1;
    } else if (next >= rowCount) {
        next = 0;
    }
    selected = static_cast<uint8_t>(next);
}

} // namespace

HermesXTakModeUiController &HermesXTakModeUiController::instance()
{
    static HermesXTakModeUiController controller;
    return controller;
}

HermesXTakModeAction HermesXTakModeUiController::handle(const HermesXTakModeInput &event,
                                                        const HermesXTakModeInputBindings &bindings,
                                                        HermesXTakModeUiState &state,
                                                        uint8_t popupRowCount,
                                                        uint8_t settingsRowCount,
                                                        uint8_t channelRowCount,
                                                        uint8_t selectedChannelRow)
{
    if (state.page == HermesXTakModePage::TransitionEnter || state.page == HermesXTakModePage::TransitionExit) {
        return HermesXTakModeAction::Consumed;
    }

    if (isCancel(event)) {
        if (state.page == HermesXTakModePage::ChannelSelect) {
            state.page = HermesXTakModePage::Main;
        } else if (state.page == HermesXTakModePage::Settings) {
            state.page = HermesXTakModePage::Popup;
            state.popupSelected = 1;
            state.popupOffset = 0;
        } else if (state.page == HermesXTakModePage::Popup) {
            state.page = HermesXTakModePage::Main;
        } else {
            state.page = HermesXTakModePage::Popup;
            state.popupSelected = 0;
            state.popupOffset = 0;
        }
        return HermesXTakModeAction::Refresh;
    }

    const int8_t direction = navigationDirection(event, bindings);
    if (state.page == HermesXTakModePage::Main) {
        if (direction > 0) {
            state.page = HermesXTakModePage::Popup;
            state.popupSelected = 0;
            state.popupOffset = 0;
            return HermesXTakModeAction::Refresh;
        }
        if (direction < 0) {
            state.page = HermesXTakModePage::ChannelSelect;
            state.settingsSelected = selectedChannelRow;
            state.settingsOffset = 0;
            return HermesXTakModeAction::Refresh;
        }
        const bool wantsSelect = isRotaryOne(event) ? (bindings.press != 0 && event.code == bindings.press)
                                                    : isSelect(event, bindings);
        if (wantsSelect) {
            return HermesXTakModeAction::ExitToActionRequested;
        }
        return HermesXTakModeAction::Unhandled;
    }

    if (direction != 0) {
        uint8_t &selected = state.page == HermesXTakModePage::Popup ? state.popupSelected : state.settingsSelected;
        const uint8_t rowCount = state.page == HermesXTakModePage::Settings      ? settingsRowCount
                                 : state.page == HermesXTakModePage::ChannelSelect ? channelRowCount
                                                                                   : popupRowCount;
        wrapSelection(selected, rowCount, direction);
        return HermesXTakModeAction::Refresh;
    }

    if (!isSelect(event, bindings)) {
        return HermesXTakModeAction::Unhandled;
    }

    if (state.page == HermesXTakModePage::Settings) {
        if (state.settingsSelected == 0) {
            state.page = HermesXTakModePage::Popup;
            state.popupSelected = 1;
            state.popupOffset = 0;
            return HermesXTakModeAction::Refresh;
        }
        return HermesXTakModeAction::SettingSelected;
    }

    if (state.page == HermesXTakModePage::ChannelSelect) {
        if (state.settingsSelected == 0) {
            state.page = HermesXTakModePage::Main;
            return HermesXTakModeAction::Refresh;
        }
        return HermesXTakModeAction::ChannelSelected;
    }

    switch (state.popupSelected) {
    case 0:
        return HermesXTakModeAction::ToggleModeRequested;
    case 1:
        state.page = HermesXTakModePage::Settings;
        state.settingsSelected = 0;
        state.settingsOffset = 0;
        return HermesXTakModeAction::Refresh;
    case 2:
        state.page = HermesXTakModePage::ChannelSelect;
        state.settingsSelected = 0;
        state.settingsOffset = 0;
        return HermesXTakModeAction::Refresh;
    case 3:
        return HermesXTakModeAction::OpenGroupSettingsRequested;
    case 4:
        return HermesXTakModeAction::OpenEmUiRequested;
    case 5:
        return HermesXTakModeAction::OpenFinderRequested;
    default:
        state.page = HermesXTakModePage::Main;
        return HermesXTakModeAction::ExitToActionRequested;
    }
}

} // namespace graphics
