#include "HermesXDetailPopupController.h"

#include "HermesXDetailPopupModel.h"
#include <algorithm>

namespace graphics
{

HermesXDetailPopupController &HermesXDetailPopupController::instance()
{
    static HermesXDetailPopupController controller;
    return controller;
}

HermesXDetailPopupAction HermesXDetailPopupController::handle(bool dismissRequested,
                                                              int8_t direction,
                                                              uint32_t nowMs,
                                                              uint16_t scrollStep)
{
    auto &model = HermesXDetailPopupModel::instance();
    auto &state = model.state();
    if (!state.visible) {
        return HermesXDetailPopupAction::Unhandled;
    }
    if (nowMs - state.shownAtMs < DismissGuardMs) {
        return HermesXDetailPopupAction::Consumed;
    }
    if (dismissRequested) {
        model.dismiss();
        return HermesXDetailPopupAction::Dismissed;
    }
    if (direction != 0 && state.maxScrollY > 0) {
        const int nextScroll = std::max(
            0, std::min(static_cast<int>(state.scrollY) + direction * static_cast<int>(scrollStep),
                        static_cast<int>(state.maxScrollY)));
        if (nextScroll != state.scrollY) {
            state.scrollY = static_cast<uint16_t>(nextScroll);
            return HermesXDetailPopupAction::Refresh;
        }
    }
    return HermesXDetailPopupAction::Consumed;
}

} // namespace graphics
