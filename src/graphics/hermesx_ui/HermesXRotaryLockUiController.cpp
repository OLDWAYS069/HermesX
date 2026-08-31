#include "HermesXRotaryLockUiController.h"

#include "HermesXRotaryLockUiModel.h"
#include "mesh/generated/meshtastic/module_config.pb.h"

namespace graphics
{

HermesXRotaryLockUiController &HermesXRotaryLockUiController::instance()
{
    static HermesXRotaryLockUiController controller;
    return controller;
}

HermesXRotaryLockAction HermesXRotaryLockUiController::handle(
    char code, bool rotarySource, uint32_t nowMs, const HermesXRotaryLockInputBindings &bindings)
{
    auto &model = HermesXRotaryLockUiModel::instance();
    if (!model.state().visible) {
        return HermesXRotaryLockAction::Unhandled;
    }

    const bool isLeft =
        code == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT);
    const bool isRight =
        code == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT);
    const bool isUp = code == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
    const bool isDown = code == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
    const bool isSelect =
        code == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const bool isCancel =
        code == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL) ||
        code == static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK);
    const bool isPress = bindings.press != 0 && code == bindings.press;
    const bool isClockwise = bindings.clockwise != 0 && code == bindings.clockwise;
    const bool isCounterClockwise = bindings.counterClockwise != 0 && code == bindings.counterClockwise;

    int8_t direction = 0;
    if (rotarySource) {
        if (isCounterClockwise) {
            direction = -1;
        } else if (isClockwise) {
            direction = 1;
        } else if (bindings.clockwise == 0 && bindings.counterClockwise == 0) {
            if (isUp || isLeft) {
                direction = -1;
            } else if (isDown || isRight) {
                direction = 1;
            }
        }
    } else if (isLeft || isUp || isCounterClockwise) {
        direction = -1;
    } else if (isRight || isDown || isClockwise) {
        direction = 1;
    }

    if (direction != 0) {
        model.select(direction > 0, nowMs);
        return HermesXRotaryLockAction::SelectionChanged;
    }
    if (isSelect || isPress) {
        model.confirm();
        return HermesXRotaryLockAction::Confirmed;
    }
    if (isCancel) {
        model.cancel();
        return HermesXRotaryLockAction::Canceled;
    }
    return HermesXRotaryLockAction::Consumed;
}

} // namespace graphics
