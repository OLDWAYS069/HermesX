#include "HermesXNodeBrowserUiModel.h"

namespace graphics
{

HermesXNodeBrowserUiModel &HermesXNodeBrowserUiModel::instance()
{
    static HermesXNodeBrowserUiModel model;
    return model;
}

void HermesXNodeBrowserUiModel::reset(HermesXNodeBrowserState &state)
{
    state.listCursor = 0;
    state.selectedIndex = 0;
    state.detailCursor = 0;
}

void HermesXNodeBrowserUiModel::clamp(HermesXNodeBrowserState &state)
{
    if (state.count == 0) {
        reset(state);
        return;
    }
    if (state.listCursor > state.count) {
        state.listCursor = state.count;
    }
    if (state.selectedIndex >= state.count) {
        state.selectedIndex = state.count - 1;
    }
}

void HermesXNodeBrowserUiModel::resetGroupList()
{
    group_.listCursor = 0;
    group_.selectedIndex = 0;
    group_.detailCursor = 0;
}

void HermesXNodeBrowserUiModel::clampGroup(uint8_t nodeCount)
{
    if (nodeCount == 0) {
        resetGroupList();
        return;
    }
    if (group_.listCursor > nodeCount) {
        group_.listCursor = nodeCount;
    }
    if (group_.selectedIndex >= nodeCount) {
        group_.selectedIndex = nodeCount - 1;
    }
}

} // namespace graphics
