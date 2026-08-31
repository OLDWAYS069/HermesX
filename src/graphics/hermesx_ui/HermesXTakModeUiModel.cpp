#include "HermesXTakModeUiModel.h"

namespace graphics
{

HermesXTakModeUiModel &HermesXTakModeUiModel::instance()
{
    static HermesXTakModeUiModel model;
    return model;
}

void HermesXTakModeUiModel::showMain()
{
    state_.page = HermesXTakModePage::Main;
}

void HermesXTakModeUiModel::showPopup(uint8_t selected)
{
    state_.page = HermesXTakModePage::Popup;
    state_.popupSelected = selected;
    state_.popupOffset = 0;
}

void HermesXTakModeUiModel::showSettings()
{
    state_.page = HermesXTakModePage::Settings;
    state_.settingsSelected = 0;
    state_.settingsOffset = 0;
}

void HermesXTakModeUiModel::showChannelSelect(uint8_t selected)
{
    state_.page = HermesXTakModePage::ChannelSelect;
    state_.settingsSelected = selected;
    state_.settingsOffset = 0;
}

void HermesXTakModeUiModel::startTransition(bool entering, uint32_t nowMs)
{
    state_.page = entering ? HermesXTakModePage::TransitionEnter : HermesXTakModePage::TransitionExit;
    state_.popupSelected = 0;
    state_.popupOffset = 0;
    state_.settingsSelected = 0;
    state_.settingsOffset = 0;
    state_.transitionStartedAtMs = nowMs;
}

} // namespace graphics
