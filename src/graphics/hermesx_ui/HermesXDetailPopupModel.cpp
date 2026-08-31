#include "HermesXDetailPopupModel.h"

namespace graphics
{

HermesXDetailPopupModel &HermesXDetailPopupModel::instance()
{
    static HermesXDetailPopupModel model;
    return model;
}

void HermesXDetailPopupModel::show(const char *title, const String &body, uint32_t nowMs)
{
    state_.title = (title && title[0] != '\0') ? String(title) : String(u8"詳細資訊");
    state_.body = body.isEmpty() ? String(u8"無") : body;
    state_.visible = true;
    state_.scrollY = 0;
    state_.maxScrollY = 0;
    state_.shownAtMs = nowMs;
}

void HermesXDetailPopupModel::dismiss()
{
    state_ = HermesXDetailPopupState{};
}

} // namespace graphics
