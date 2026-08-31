#include "HermesXTraceRouteUiController.h"

#include "HermesXTraceRouteUiModel.h"
#include "mesh/generated/meshtastic/module_config.pb.h"
#include <algorithm>
#include <cstring>

namespace graphics
{
namespace
{

using InputChar = meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar;

const char *kSearchKeyRows[][10] = {
    {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
    {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},
    {"A", "S", "D", "F", "G", "H", "J", "K", "L", nullptr},
    {"Z", "X", "C", "V", "B", "N", "M", nullptr, nullptr, nullptr},
    {"EXIT", "DEL", "OK", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
};
const uint8_t kSearchKeyRowLengths[] = {10, 10, 9, 7, 3};
constexpr uint8_t kSearchKeyRowCount = sizeof(kSearchKeyRowLengths) / sizeof(kSearchKeyRowLengths[0]);

bool matches(const HermesXTraceRoutePopupInput &event, InputChar input)
{
    return event.code == static_cast<char>(input);
}

bool isRotary(const HermesXTraceRoutePopupInput &event)
{
    return event.source && std::strncmp(event.source, "rotEnc", 6) == 0;
}

int8_t navigationDirection(const HermesXTraceRoutePopupInput &event,
                           const HermesXTraceRoutePopupBindings &bindings)
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
    if (up || left || counterClockwise) {
        return -1;
    }
    if (down || right || clockwise) {
        return 1;
    }
    return 0;
}

} // namespace

HermesXTraceRouteUiController &HermesXTraceRouteUiController::instance()
{
    static HermesXTraceRouteUiController controller;
    return controller;
}

const char *const (*HermesXTraceRouteUiController::searchKeyRows() const)[10]
{
    return kSearchKeyRows;
}

const uint8_t *HermesXTraceRouteUiController::searchKeyRowLengths() const
{
    return kSearchKeyRowLengths;
}

uint8_t HermesXTraceRouteUiController::searchKeyRowCount() const
{
    return kSearchKeyRowCount;
}

HermesXTraceRoutePopupAction HermesXTraceRouteUiController::handlePopup(
    const HermesXTraceRoutePopupInput &event,
    const HermesXTraceRoutePopupBindings &bindings,
    uint32_t nowMs,
    uint16_t scrollStep,
    uint32_t dismissGuardMs)
{
    auto &popup = HermesXTraceRouteUiModel::instance().popupState();
    if (!popup.visible) {
        return HermesXTraceRoutePopupAction::Consumed;
    }
    if (nowMs - popup.shownAtMs < dismissGuardMs) {
        return HermesXTraceRoutePopupAction::Consumed;
    }

    const bool select = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const bool configuredPress = bindings.press != 0 && event.code == bindings.press;
    const bool dismiss = configuredPress || (bindings.press == 0 && select) ||
                         matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK) ||
                         matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL);
    if (dismiss) {
        return HermesXTraceRoutePopupAction::DismissRequested;
    }

    const int8_t direction = navigationDirection(event, bindings);
    if (direction != 0 && popup.maxScrollY > 0) {
        const int nextScroll = std::max(
            0, std::min(static_cast<int>(popup.scrollY) + direction * static_cast<int>(scrollStep),
                        static_cast<int>(popup.maxScrollY)));
        if (nextScroll != popup.scrollY) {
            popup.scrollY = static_cast<uint16_t>(nextScroll);
            return HermesXTraceRoutePopupAction::Refresh;
        }
    }
    return HermesXTraceRoutePopupAction::Consumed;
}

HermesXTraceRouteSearchAction HermesXTraceRouteUiController::handleSearch(
    const HermesXTraceRoutePopupInput &event,
    const HermesXTraceRoutePopupBindings &bindings,
    uint32_t nowMs)
{
    auto &search = HermesXTraceRouteUiModel::instance().searchState();
    const bool select = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const bool press = bindings.press != 0 && event.code == bindings.press;
    const bool cancel = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL) ||
                        matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK);
    const int8_t direction = navigationDirection(event, bindings);

    if (search.resultVisible) {
        if (direction != 0 && search.resultFound) {
            search.resultCursor = search.resultCursor == 0 ? 1 : 0;
            return HermesXTraceRouteSearchAction::Refresh;
        }
        if (select || press || cancel) {
            if (!cancel && search.resultFound && search.resultCursor == 0) {
                return HermesXTraceRouteSearchAction::BindResultRequested;
            }
            return HermesXTraceRouteSearchAction::ExitResultRequested;
        }
        return HermesXTraceRouteSearchAction::Consumed;
    }

    if (!search.active) {
        return HermesXTraceRouteSearchAction::Consumed;
    }
    if (cancel) {
        return HermesXTraceRouteSearchAction::CloseRequested;
    }
    if (direction != 0) {
        int totalKeys = 0;
        int keyIndex = 0;
        for (uint8_t row = 0; row < kSearchKeyRowCount; ++row) {
            totalKeys += kSearchKeyRowLengths[row];
            if (row < search.keyRow) {
                keyIndex += kSearchKeyRowLengths[row];
            }
        }
        keyIndex += search.keyCol;
        keyIndex = (keyIndex + direction + totalKeys) % totalKeys;
        for (uint8_t row = 0; row < kSearchKeyRowCount; ++row) {
            if (keyIndex < kSearchKeyRowLengths[row]) {
                search.keyRow = row;
                search.keyCol = static_cast<uint8_t>(keyIndex);
                break;
            }
            keyIndex -= kSearchKeyRowLengths[row];
        }
        return HermesXTraceRouteSearchAction::Refresh;
    }
    if (!select && !press) {
        return HermesXTraceRouteSearchAction::Consumed;
    }

    const char *label = kSearchKeyRows[search.keyRow][search.keyCol];
    if (!label) {
        return HermesXTraceRouteSearchAction::Consumed;
    }
    if (std::strcmp(label, "OK") == 0) {
        if (search.draft.length() == 0) {
            search.toast = u8"請輸入ShortName";
            search.toastUntilMs = nowMs + 1500;
            return HermesXTraceRouteSearchAction::Refresh;
        }
        return HermesXTraceRouteSearchAction::SearchRequested;
    }
    if (std::strcmp(label, "EXIT") == 0) {
        return HermesXTraceRouteSearchAction::CloseRequested;
    }
    if (std::strcmp(label, "DEL") == 0) {
        if (search.draft.length() > 0) {
            search.draft.remove(search.draft.length() - 1);
        }
    } else if (search.draft.length() < 4) {
        search.draft += label;
    }
    return HermesXTraceRouteSearchAction::Refresh;
}

HermesXTraceRouteListAction HermesXTraceRouteUiController::handleList(
    const HermesXTraceRoutePopupInput &event,
    const HermesXTraceRoutePopupBindings &bindings,
    uint8_t onlineNodeCount)
{
    auto &model = HermesXTraceRouteUiModel::instance();
    auto &navigation = model.navigationState();
    const bool select = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const bool press = bindings.press != 0 && event.code == bindings.press;
    const bool cancel = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL) ||
                        matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK);
    const int8_t direction = navigationDirection(event, bindings);

    if (navigation.confirmVisible) {
        if (direction != 0) {
            navigation.confirmSelected = navigation.confirmSelected == 0 ? 1 : 0;
            return HermesXTraceRouteListAction::Refresh;
        }
        if (cancel) {
            model.dismissBindConfirm();
            return HermesXTraceRouteListAction::Refresh;
        }
        if (select || press) {
            if (navigation.confirmSelected == 0) {
                return HermesXTraceRouteListAction::BindConfirmed;
            }
            model.dismissBindConfirm();
            return HermesXTraceRouteListAction::Refresh;
        }
        return HermesXTraceRouteListAction::Consumed;
    }

    if (navigation.mode == HermesXTraceRouteUiMode::Menu) {
        if (direction != 0) {
            model.cancelDeferredBindShortPress();
            navigation.menuCursor = static_cast<uint8_t>(
                std::max(0, std::min(static_cast<int>(navigation.menuCursor) + direction, 2)));
            return HermesXTraceRouteListAction::Refresh;
        }
        if (cancel) {
            model.cancelDeferredBindShortPress();
            return HermesXTraceRouteListAction::ExitFeatureRequested;
        }
        if (!select && !press) {
            return HermesXTraceRouteListAction::Ignored;
        }
        model.cancelDeferredBindShortPress();
        if (navigation.menuCursor == 0) {
            return HermesXTraceRouteListAction::ExitFeatureRequested;
        }
        if (navigation.menuCursor == 1) {
            model.enterBindList();
            return HermesXTraceRouteListAction::Refresh;
        }
        model.enterBoundRoutes();
        return HermesXTraceRouteListAction::OpenBoundRoutesRequested;
    }

    const int totalEntries = static_cast<int>(onlineNodeCount) + 2;
    if (navigation.bindCursor >= totalEntries) {
        navigation.bindCursor = 0;
        navigation.bindSelectedIndex = 0;
    }
    if (direction != 0) {
        model.cancelDeferredBindShortPress();
        const int nextCursor = std::max(
            0, std::min(static_cast<int>(navigation.bindCursor) + direction, totalEntries - 1));
        if (nextCursor != navigation.bindCursor) {
            navigation.bindCursor = static_cast<uint8_t>(nextCursor);
            if (nextCursor >= 2) {
                navigation.bindSelectedIndex = static_cast<uint8_t>(nextCursor - 2);
            }
            return HermesXTraceRouteListAction::Refresh;
        }
        return HermesXTraceRouteListAction::Consumed;
    }
    if (cancel) {
        model.enterMenu();
        return HermesXTraceRouteListAction::Refresh;
    }
    if (!select && !press) {
        return HermesXTraceRouteListAction::Ignored;
    }

    model.cancelDeferredBindShortPress();
    if (navigation.bindCursor == 0) {
        return HermesXTraceRouteListAction::OpenSearchRequested;
    }
    if (navigation.bindCursor == 1) {
        model.enterMenu();
        return HermesXTraceRouteListAction::Refresh;
    }
    return isRotary(event) ? HermesXTraceRouteListAction::DeferBindInfoRequested
                           : HermesXTraceRouteListAction::ShowBindInfoRequested;
}

HermesXTraceRouteBoundListAction HermesXTraceRouteUiController::handleBoundList(
    const HermesXTraceRoutePopupInput &event,
    const HermesXTraceRoutePopupBindings &bindings,
    uint8_t boundNodeCount)
{
    auto &model = HermesXTraceRouteUiModel::instance();
    auto &navigation = model.navigationState();
    navigation.mode = HermesXTraceRouteUiMode::BoundRoutes;
    model.clampBoundSelection(boundNodeCount);

    const bool select = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const bool press = bindings.press != 0 && event.code == bindings.press;
    const bool cancel = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL) ||
                        matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK);
    const bool up = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
    const bool down = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
    const bool left = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT);
    const bool right = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT);
    const bool clockwise = bindings.clockwise != 0 && event.code == bindings.clockwise;
    const bool counterClockwise = bindings.counterClockwise != 0 && event.code == bindings.counterClockwise;
    int8_t direction = 0;
    if (isRotary(event)) {
        if (counterClockwise) {
            direction = -1;
        } else if (clockwise) {
            direction = 1;
        } else if (bindings.clockwise == 0 && bindings.counterClockwise == 0) {
            direction = up ? -1 : (down ? 1 : 0);
        }
    } else if (up || counterClockwise) {
        direction = -1;
    } else if (down || clockwise) {
        direction = 1;
    }

    if (direction != 0) {
        model.cancelDeferredRouteShortPress();
        const int nextCursor = std::max(
            0, std::min(static_cast<int>(navigation.boundCursor) + direction, static_cast<int>(boundNodeCount)));
        if (nextCursor != navigation.boundCursor) {
            navigation.boundCursor = static_cast<uint8_t>(nextCursor);
            if (nextCursor > 0) {
                navigation.boundSelectedIndex = static_cast<uint8_t>(nextCursor - 1);
            }
            return HermesXTraceRouteBoundListAction::Refresh;
        }
        return HermesXTraceRouteBoundListAction::Consumed;
    }
    if (cancel || left || right) {
        model.enterMenu();
        return HermesXTraceRouteBoundListAction::ReturnToMenuRequested;
    }
    if (!select && !press) {
        return HermesXTraceRouteBoundListAction::Ignored;
    }

    model.cancelDeferredRouteShortPress();
    if (navigation.boundCursor == 0) {
        model.enterMenu();
        return HermesXTraceRouteBoundListAction::ReturnToMenuRequested;
    }
    return isRotary(event) ? HermesXTraceRouteBoundListAction::DeferSendRequested
                           : HermesXTraceRouteBoundListAction::SendRequested;
}

} // namespace graphics
