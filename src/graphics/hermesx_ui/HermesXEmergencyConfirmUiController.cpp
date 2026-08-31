#include "HermesXEmergencyConfirmUiController.h"

#include "HermesXEmergencyConfirmUiModel.h"
#include "mesh/generated/meshtastic/module_config.pb.h"

namespace graphics
{

HermesXEmergencyConfirmUiController &HermesXEmergencyConfirmUiController::instance()
{
    static HermesXEmergencyConfirmUiController controller;
    return controller;
}

HermesXEmergencyConfirmAction HermesXEmergencyConfirmUiController::handle(
    char code, const HermesXEmergencyConfirmInputBindings &bindings)
{
    auto &model = HermesXEmergencyConfirmUiModel::instance();
    if (!model.state().visible) {
        return HermesXEmergencyConfirmAction::Unhandled;
    }

    const bool standardInput =
        code == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT) ||
        code == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT) ||
        code == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP) ||
        code == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN) ||
        code == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT) ||
        code == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL) ||
        code == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK);
    const bool configuredInput = (bindings.press != 0 && code == bindings.press) ||
                                 (bindings.clockwise != 0 && code == bindings.clockwise) ||
                                 (bindings.counterClockwise != 0 && code == bindings.counterClockwise);
    if (standardInput || configuredInput) {
        model.requestCancel();
        return HermesXEmergencyConfirmAction::Canceled;
    }
    return HermesXEmergencyConfirmAction::Consumed;
}

} // namespace graphics
