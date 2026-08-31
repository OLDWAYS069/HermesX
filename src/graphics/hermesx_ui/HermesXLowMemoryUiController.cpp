#include "HermesXLowMemoryUiController.h"

#include "mesh/generated/meshtastic/module_config.pb.h"

namespace graphics
{
namespace
{

using InputChar = meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar;

bool matches(const HermesXLowMemoryInput &event, InputChar input)
{
    return event.code == static_cast<char>(input);
}

} // namespace

HermesXLowMemoryUiController &HermesXLowMemoryUiController::instance()
{
    static HermesXLowMemoryUiController controller;
    return controller;
}

HermesXLowMemoryAction HermesXLowMemoryUiController::handle(const HermesXLowMemoryInput &event,
                                                            const HermesXLowMemoryInputBindings &bindings,
                                                            uint32_t nowMs,
                                                            uint32_t suppressMs)
{
    auto &model = HermesXLowMemoryUiModel::instance();
    auto &state = model.state();
    if (!state.visible) {
        return HermesXLowMemoryAction::Unhandled;
    }

    const bool previous = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT) ||
                          matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP) ||
                          (bindings.counterClockwise != 0 && event.code == bindings.counterClockwise);
    const bool next = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT) ||
                      matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN) ||
                      (bindings.clockwise != 0 && event.code == bindings.clockwise);
    if (previous) {
        state.selected = 0;
        return HermesXLowMemoryAction::Refresh;
    }
    if (next) {
        state.selected = 1;
        return HermesXLowMemoryAction::Refresh;
    }

    const bool cancel = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL) ||
                        matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK);
    if (cancel) {
        model.dismiss(nowMs + suppressMs);
        return HermesXLowMemoryAction::Dismissed;
    }

    const bool select = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT) ||
                        (bindings.press != 0 && event.code == bindings.press);
    if (!select) {
        return HermesXLowMemoryAction::Consumed;
    }
    if (state.selected == 0) {
        model.dismiss(nowMs + suppressMs);
        return HermesXLowMemoryAction::Dismissed;
    }

    state.visible = true;
    state.selected = 0;
    return HermesXLowMemoryAction::CleanNodesRequested;
}

} // namespace graphics
