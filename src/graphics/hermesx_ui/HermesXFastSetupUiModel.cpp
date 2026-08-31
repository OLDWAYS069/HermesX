#include "HermesXFastSetupUiModel.h"

#include <algorithm>

namespace graphics
{

HermesXFastSetupUiModel &HermesXFastSetupUiModel::instance()
{
    static HermesXFastSetupUiModel model;
    return model;
}

void HermesXFastSetupUiModel::reset(HermesFastSetupPage page)
{
    navigation_.page = page;
    navigation_.selected = 0;
    navigation_.offset = 0;
    navigation_.lastNavAtMs = 0;
    navigation_.lastNavDir = 0;
}

void HermesXFastSetupUiModel::enter(HermesFastSetupPage page, int count, int selected)
{
    navigation_.page = page;
    navigation_.selected = count <= 0 ? 0 : std::max(0, std::min(selected, count - 1));
    navigation_.offset = 0;
    navigation_.lastNavAtMs = 0;
    navigation_.lastNavDir = 0;
    updateOffset(count);
}

void HermesXFastSetupUiModel::openDetail(HermesFastSetupPage returnPage)
{
    navigation_.returnPage = returnPage;
    reset(HermesFastSetupPage::UpdateDetailPopup);
}

void HermesXFastSetupUiModel::updateOffset(int count)
{
    if (count <= static_cast<int>(VisibleRows)) {
        navigation_.offset = 0;
        return;
    }
    if (navigation_.selected < navigation_.offset) {
        navigation_.offset = navigation_.selected;
    } else if (navigation_.selected >= navigation_.offset + static_cast<int>(VisibleRows)) {
        navigation_.offset = navigation_.selected - static_cast<int>(VisibleRows) + 1;
    }
    navigation_.offset = std::max(0, std::min<int>(navigation_.offset, count - VisibleRows));
}

} // namespace graphics
