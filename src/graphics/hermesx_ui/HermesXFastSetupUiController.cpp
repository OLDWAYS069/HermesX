#include "HermesXFastSetupUiController.h"

namespace graphics
{
namespace
{

constexpr uint32_t NavMinIntervalMs = 80;
constexpr uint32_t NavFlipGuardMs = 800;

HermesXFastSetupSelectionResult activateIndexedSelection(int selected,
                                                         int optionCount,
                                                         HermesFastSetupPage returnPage,
                                                         HermesXFastSetupAction applyAction)
{
    HermesXFastSetupSelectionResult result;
    if (selected == 0) {
        HermesXFastSetupUiModel::instance().reset(returnPage);
        result.action = HermesXFastSetupAction::StateChanged;
    } else if (selected > 0 && selected <= optionCount) {
        HermesXFastSetupUiModel::instance().reset(returnPage);
        result.action = applyAction;
        result.optionIndex = selected - 1;
    }
    return result;
}

} // namespace

HermesXFastSetupUiController &HermesXFastSetupUiController::instance()
{
    static HermesXFastSetupUiController controller;
    return controller;
}

bool HermesXFastSetupUiController::acceptNavigation(int8_t direction, bool rotaryInput, uint32_t nowMs)
{
    if (direction == 0) {
        return false;
    }
    if (rotaryInput) {
        return true;
    }

    auto &state = HermesXFastSetupUiModel::instance().navigation();
    if (state.lastNavAtMs != 0 && (nowMs - state.lastNavAtMs) < NavMinIntervalMs) {
        return false;
    }
    if (state.lastNavDir != 0 && direction != state.lastNavDir && (nowMs - state.lastNavAtMs) < NavFlipGuardMs) {
        return false;
    }
    state.lastNavAtMs = nowMs;
    state.lastNavDir = direction;
    return true;
}

HermesXFastSetupNavigationAction HermesXFastSetupUiController::navigate(int8_t direction,
                                                                        int count,
                                                                        bool rotaryInput,
                                                                        uint32_t nowMs)
{
    if (direction == 0) {
        return HermesXFastSetupNavigationAction::Ignored;
    }

    auto &model = HermesXFastSetupUiModel::instance();
    auto &state = model.navigation();
    if (!acceptNavigation(direction, rotaryInput, nowMs)) {
        return HermesXFastSetupNavigationAction::Consumed;
    }

    if (count <= 0) {
        state.selected = 0;
        state.offset = 0;
        return HermesXFastSetupNavigationAction::Consumed;
    }
    if (direction < 0) {
        state.selected = state.selected > 0 ? state.selected - 1 : count - 1;
    } else {
        state.selected = state.selected < count - 1 ? state.selected + 1 : 0;
    }
    model.updateOffset(count);
    return HermesXFastSetupNavigationAction::Changed;
}

HermesXFastSetupAction HermesXFastSetupUiController::activateRoot(int selected)
{
    auto &model = HermesXFastSetupUiModel::instance();
    switch (selected) {
    case 0:
        return HermesXFastSetupAction::ExitRequested;
    case 1:
        model.reset(HermesFastSetupPage::EmacMenu);
        return HermesXFastSetupAction::StateChanged;
    case 2:
        model.reset(HermesFastSetupPage::UiMenu);
        return HermesXFastSetupAction::StateChanged;
    case 3:
        model.reset(HermesFastSetupPage::NodeMenu);
        return HermesXFastSetupAction::StateChanged;
    case 4:
        model.reset(HermesFastSetupPage::CannedMenu);
        return HermesXFastSetupAction::StateChanged;
    case 5:
        return HermesXFastSetupAction::SaveAndRebootRequested;
    default:
        return HermesXFastSetupAction::Ignored;
    }
}

HermesXFastSetupAction HermesXFastSetupUiController::activateNodeMenu(int selected)
{
    auto &model = HermesXFastSetupUiModel::instance();
    switch (selected) {
    case 0:
        model.reset(HermesFastSetupPage::Root);
        return HermesXFastSetupAction::StateChanged;
    case 1:
        model.reset(HermesFastSetupPage::DeviceInfoMenu);
        return HermesXFastSetupAction::StateChanged;
    case 2:
        model.reset(HermesFastSetupPage::LoraMenu);
        return HermesXFastSetupAction::StateChanged;
    case 3:
        model.reset(HermesFastSetupPage::GpsMenu);
        return HermesXFastSetupAction::StateChanged;
    case 4:
        model.reset(HermesFastSetupPage::MqttMenu);
        return HermesXFastSetupAction::StateChanged;
    case 5:
        return HermesXFastSetupAction::OpenChannelMenuRequested;
    case 6:
        return HermesXFastSetupAction::ToggleBluetoothRequested;
    case 7:
        model.reset(HermesFastSetupPage::PowerMenu);
        return HermesXFastSetupAction::StateChanged;
    case 8:
        model.reset(HermesFastSetupPage::NodeDatabaseMenu);
        return HermesXFastSetupAction::StateChanged;
    case 9:
        return HermesXFastSetupAction::EnterUpdateModeRequested;
    default:
        return HermesXFastSetupAction::Ignored;
    }
}

HermesXFastSetupAction HermesXFastSetupUiController::activateCannedMenu(int selected)
{
    if (selected == 0) {
        HermesXFastSetupUiModel::instance().reset(HermesFastSetupPage::Root);
        return HermesXFastSetupAction::StateChanged;
    }
    return selected == 1 ? HermesXFastSetupAction::OpenCannedChannelRequested : HermesXFastSetupAction::Ignored;
}

HermesXFastSetupAction HermesXFastSetupUiController::activateDeviceInfoMenu(int selected)
{
    switch (selected) {
    case 0:
        HermesXFastSetupUiModel::instance().reset(HermesFastSetupPage::NodeMenu);
        return HermesXFastSetupAction::StateChanged;
    case 1:
        HermesXFastSetupUiModel::instance().reset(HermesFastSetupPage::DeviceInfoShortNameEdit);
        return HermesXFastSetupAction::EditDeviceShortNameRequested;
    case 2:
        HermesXFastSetupUiModel::instance().reset(HermesFastSetupPage::DeviceInfoLongNameEdit);
        return HermesXFastSetupAction::EditDeviceLongNameRequested;
    case 3:
        return HermesXFastSetupAction::ShowNodeIdRequested;
    case 4:
        return HermesXFastSetupAction::OpenDeviceBroadcastRequested;
    case 5:
        return HermesXFastSetupAction::RequestNodeInfoRequested;
    default:
        return HermesXFastSetupAction::Ignored;
    }
}

HermesXFastSetupAction HermesXFastSetupUiController::activatePowerMenu(int selected)
{
    switch (selected) {
    case 0:
        HermesXFastSetupUiModel::instance().reset(HermesFastSetupPage::NodeMenu);
        return HermesXFastSetupAction::StateChanged;
    case 1:
        return HermesXFastSetupAction::Ignored;
    case 2:
        return HermesXFastSetupAction::TogglePowerGuardRequested;
    case 3:
        return HermesXFastSetupAction::OpenPowerGuardThresholdRequested;
    default:
        return HermesXFastSetupAction::Ignored;
    }
}

HermesXFastSetupAction HermesXFastSetupUiController::activateGpsMenu(int selected)
{
    switch (selected) {
    case 0:
        HermesXFastSetupUiModel::instance().reset(HermesFastSetupPage::NodeMenu);
        return HermesXFastSetupAction::StateChanged;
    case 1:
        return HermesXFastSetupAction::OpenGpsUpdateRequested;
    case 2:
        return HermesXFastSetupAction::OpenGpsBroadcastRequested;
    case 3:
        return HermesXFastSetupAction::ToggleGpsSmartRequested;
    case 4:
        return HermesXFastSetupAction::OpenGpsSmartDistanceRequested;
    case 5:
        return HermesXFastSetupAction::OpenGpsSmartIntervalRequested;
    default:
        return HermesXFastSetupAction::Ignored;
    }
}

HermesXFastSetupAction HermesXFastSetupUiController::activateUiMenu(int selected)
{
    switch (selected) {
    case 0:
        HermesXFastSetupUiModel::instance().reset(HermesFastSetupPage::Root);
        return HermesXFastSetupAction::StateChanged;
    case 1:
        return HermesXFastSetupAction::ToggleBuzzerRequested;
    case 2:
        return HermesXFastSetupAction::OpenUiBrightnessRequested;
    case 3:
        return HermesXFastSetupAction::ToggleRgbRequested;
    case 4:
        return HermesXFastSetupAction::OpenScreenSleepRequested;
    case 5:
        return HermesXFastSetupAction::OpenTimezoneRequested;
    case 6:
        return HermesXFastSetupAction::OpenRotarySwapRequested;
    case 7:
        return HermesXFastSetupAction::ToggleIncomingPopupRequested;
    default:
        return HermesXFastSetupAction::Ignored;
    }
}

HermesXFastSetupAction HermesXFastSetupUiController::activateLoraMenu(int selected)
{
    switch (selected) {
    case 0:
        HermesXFastSetupUiModel::instance().reset(HermesFastSetupPage::NodeMenu);
        return HermesXFastSetupAction::StateChanged;
    case 1:
        return HermesXFastSetupAction::OpenLoraRoleRequested;
    case 2:
        return HermesXFastSetupAction::OpenLoraPresetRequested;
    case 3:
        return HermesXFastSetupAction::OpenLoraRegionRequested;
    case 4:
        return HermesXFastSetupAction::ToggleIgnoreMqttRequested;
    case 5:
        return HermesXFastSetupAction::ToggleMqttForwardRequested;
    case 6:
        return HermesXFastSetupAction::ToggleLoraTxRequested;
    case 7:
        return HermesXFastSetupAction::OpenLoraChannelSlotRequested;
    case 8:
        HermesXFastSetupUiModel::instance().reset(HermesFastSetupPage::FrequencyEdit);
        return HermesXFastSetupAction::EditLoraFrequencyRequested;
    default:
        return HermesXFastSetupAction::Ignored;
    }
}

HermesXFastSetupAction HermesXFastSetupUiController::activateEmacMenu(int selected)
{
    switch (selected) {
    case 0:
        return HermesXFastSetupAction::ReturnFromGroupRequested;
    case 1:
        return HermesXFastSetupAction::OpenEmInfoRequested;
    case 2:
        HermesXFastSetupUiModel::instance().reset(HermesFastSetupPage::PassEdit);
        return HermesXFastSetupAction::EditGroupPin0Requested;
    case 3:
        HermesXFastSetupUiModel::instance().reset(HermesFastSetupPage::PassEdit);
        return HermesXFastSetupAction::EditGroupPin1Requested;
    case 4:
        HermesXFastSetupUiModel::instance().reset(HermesFastSetupPage::PassShow);
        return HermesXFastSetupAction::ShowGroupPinsRequested;
    case 5:
        return HermesXFastSetupAction::ResetLighthouseRequested;
    default:
        return HermesXFastSetupAction::ReturnFromGroupRequested;
    }
}

HermesXFastSetupAction HermesXFastSetupUiController::activateEmInfoMenu(int selected)
{
    switch (selected) {
    case 0:
        HermesXFastSetupUiModel::instance().reset(HermesFastSetupPage::EmacMenu);
        return HermesXFastSetupAction::StateChanged;
    case 1:
        return HermesXFastSetupAction::ToggleEmInfoRequested;
    case 2:
        return HermesXFastSetupAction::OpenEmInfoIntervalRequested;
    case 3:
        return HermesXFastSetupAction::OpenHeartbeatIntervalRequested;
    case 4:
        return HermesXFastSetupAction::OpenOfflineThresholdRequested;
    case 5:
        return HermesXFastSetupAction::OpenBatteryIncludeRequested;
    default:
        return HermesXFastSetupAction::Ignored;
    }
}

HermesXFastSetupAction HermesXFastSetupUiController::activateUpdateMenu(int selected, bool dedicatedUpdateBoot)
{
    if (!dedicatedUpdateBoot) {
        return selected == 0 ? HermesXFastSetupAction::LeaveUpdateModeRequested
                             : (selected == 1 ? HermesXFastSetupAction::EnterUpdateModeRequested
                                              : HermesXFastSetupAction::Ignored);
    }

    switch (selected) {
    case 0:
        return HermesXFastSetupAction::LeaveUpdateModeRequested;
    case 1:
        return HermesXFastSetupAction::OpenUpdateWifiConfigRequested;
    case 2:
        return HermesXFastSetupAction::ShowCurrentVersionRequested;
    case 3:
        return HermesXFastSetupAction::OpenUpdateCheckRequested;
    case 4:
        return HermesXFastSetupAction::OpenUpdateRuntimeRequested;
    default:
        return HermesXFastSetupAction::Ignored;
    }
}

HermesXFastSetupAction HermesXFastSetupUiController::activateUpdateCheckMenu(int selected)
{
    static const HermesXFastSetupAction actions[] = {
        HermesXFastSetupAction::ReturnToUpdateMenuRequested,
        HermesXFastSetupAction::ShowCurrentVersionRequested,
        HermesXFastSetupAction::ShowUpdateSourceRequested,
        HermesXFastSetupAction::StartUpdateCheckFlowRequested,
        HermesXFastSetupAction::ShowUpdateStatusRequested,
        HermesXFastSetupAction::ShowUpdateCandidateRequested,
        HermesXFastSetupAction::ShowUpdateProgressRequested,
        HermesXFastSetupAction::ShowUpdateErrorRequested,
    };
    return selected >= 0 && selected < static_cast<int>(sizeof(actions) / sizeof(actions[0]))
               ? actions[selected]
               : HermesXFastSetupAction::Ignored;
}

HermesXFastSetupAction HermesXFastSetupUiController::activateUpdateCheckFlow(int selected)
{
    if (selected == 0) {
        return HermesXFastSetupAction::ReturnToUpdateCheckRequested;
    }
    return selected == 1 ? HermesXFastSetupAction::ContinueUpdateCheckFlowRequested : HermesXFastSetupAction::Ignored;
}

HermesXFastSetupAction HermesXFastSetupUiController::activateUpdateRuntimeMenu(int selected)
{
    static const HermesXFastSetupAction actions[] = {
        HermesXFastSetupAction::ReturnToUpdateMenuRequested,
        HermesXFastSetupAction::OpenWifiUpdateFlowRequested,
        HermesXFastSetupAction::OpenUsbUpdateFlowRequested,
    };
    return selected >= 0 && selected < static_cast<int>(sizeof(actions) / sizeof(actions[0]))
               ? actions[selected]
               : HermesXFastSetupAction::Ignored;
}

HermesXFastSetupAction HermesXFastSetupUiController::activateUpdateWifiMenu(int selected)
{
    static const HermesXFastSetupAction actions[] = {
        HermesXFastSetupAction::ReturnToUpdateRuntimeRequested,
        HermesXFastSetupAction::ShowCurrentVersionRequested,
        HermesXFastSetupAction::ShowWifiStatusRequested,
        HermesXFastSetupAction::StartWifiUpdateRequested,
    };
    return selected >= 0 && selected < static_cast<int>(sizeof(actions) / sizeof(actions[0]))
               ? actions[selected]
               : HermesXFastSetupAction::Ignored;
}

HermesXFastSetupAction HermesXFastSetupUiController::activateUpdateWifiConfigMenu(int selected)
{
    static const HermesXFastSetupAction actions[] = {
        HermesXFastSetupAction::ReturnToUpdateMenuRequested,
        HermesXFastSetupAction::ToggleWifiDraftRequested,
        HermesXFastSetupAction::EditWifiSsidRequested,
        HermesXFastSetupAction::EditWifiPasswordRequested,
        HermesXFastSetupAction::SaveWifiDraftRequested,
        HermesXFastSetupAction::ShowWifiIpRequested,
    };
    return selected >= 0 && selected < static_cast<int>(sizeof(actions) / sizeof(actions[0]))
               ? actions[selected]
               : HermesXFastSetupAction::Ignored;
}

HermesXFastSetupAction HermesXFastSetupUiController::activateUpdateUploadMenu(int selected)
{
    static const HermesXFastSetupAction actions[] = {
        HermesXFastSetupAction::ReturnToUpdateRuntimeRequested,
        HermesXFastSetupAction::ShowCurrentVersionRequested,
        HermesXFastSetupAction::ShowUsbStatusRequested,
        HermesXFastSetupAction::StartUsbUpdateRequested,
    };
    return selected >= 0 && selected < static_cast<int>(sizeof(actions) / sizeof(actions[0]))
               ? actions[selected]
               : HermesXFastSetupAction::Ignored;
}

HermesXFastSetupAction HermesXFastSetupUiController::activateUpdateApplyMenu(int selected, bool canApply)
{
    if (selected == 0) {
        return HermesXFastSetupAction::ReturnFromUpdateApplyRequested;
    }
    return selected == 1 && canApply ? HermesXFastSetupAction::ApplyUpdateRequested : HermesXFastSetupAction::Ignored;
}

HermesXFastSetupAction HermesXFastSetupUiController::activateMqttMenu(int selected)
{
    switch (selected) {
    case 0:
        HermesXFastSetupUiModel::instance().reset(HermesFastSetupPage::NodeMenu);
        return HermesXFastSetupAction::StateChanged;
    case 1:
        return HermesXFastSetupAction::ToggleMqttRequested;
    case 2:
        return HermesXFastSetupAction::ToggleMqttProxyRequested;
    case 3:
        return HermesXFastSetupAction::OpenMqttMapReportRequested;
    default:
        return HermesXFastSetupAction::Ignored;
    }
}

HermesXFastSetupAction HermesXFastSetupUiController::activateMqttMapReportMenu(int selected)
{
    switch (selected) {
    case 0:
        HermesXFastSetupUiModel::instance().reset(HermesFastSetupPage::MqttMenu);
        return HermesXFastSetupAction::StateChanged;
    case 1:
        return HermesXFastSetupAction::ToggleMqttMapReportRequested;
    case 2:
        return HermesXFastSetupAction::OpenMqttMapPrecisionRequested;
    case 3:
        return HermesXFastSetupAction::OpenMqttMapPublishRequested;
    default:
        return HermesXFastSetupAction::Ignored;
    }
}

HermesXFastSetupAction HermesXFastSetupUiController::activateChannelMenu(int selected)
{
    if (selected == 0) {
        HermesXFastSetupUiModel::instance().reset(HermesFastSetupPage::NodeMenu);
        return HermesXFastSetupAction::StateChanged;
    }
    return selected > 0 ? HermesXFastSetupAction::OpenChannelDetailRequested : HermesXFastSetupAction::Ignored;
}

HermesXFastSetupAction HermesXFastSetupUiController::activateChannelDetailMenu(int selected)
{
    static const HermesXFastSetupAction actions[] = {
        HermesXFastSetupAction::OpenChannelMenuRequested,
        HermesXFastSetupAction::ToggleChannelUplinkRequested,
        HermesXFastSetupAction::ToggleChannelDownlinkRequested,
        HermesXFastSetupAction::ToggleChannelPositionRequested,
        HermesXFastSetupAction::OpenChannelPrecisionRequested,
    };
    return selected >= 0 && selected < static_cast<int>(sizeof(actions) / sizeof(actions[0]))
               ? actions[selected]
               : HermesXFastSetupAction::Ignored;
}

HermesXFastSetupAction HermesXFastSetupUiController::activateNodeDatabaseMenu(int selected)
{
    if (selected == 0) {
        HermesXFastSetupUiModel::instance().reset(HermesFastSetupPage::NodeMenu);
        return HermesXFastSetupAction::StateChanged;
    }
    if (selected == 1) {
        HermesXFastSetupUiModel::instance().reset(HermesFastSetupPage::NodeDatabaseResetSelect);
        return HermesXFastSetupAction::StateChanged;
    }
    return HermesXFastSetupAction::Ignored;
}

HermesXFastSetupSelectionResult HermesXFastSetupUiController::activateNodeDatabaseResetSelect(int selected)
{
    HermesXFastSetupSelectionResult result;
    if (selected == 0) {
        HermesXFastSetupUiModel::instance().reset(HermesFastSetupPage::NodeDatabaseMenu);
        result.action = HermesXFastSetupAction::StateChanged;
    } else if (selected >= 1 && selected <= 3) {
        static const uint32_t cleanupAgesSeconds[] = {12U * 60U * 60U, 24U * 60U * 60U, 48U * 60U * 60U};
        HermesXFastSetupUiModel::instance().reset(HermesFastSetupPage::NodeDatabaseMenu);
        result.action = HermesXFastSetupAction::CleanupNodesRequested;
        result.optionIndex = selected - 1;
        result.value = cleanupAgesSeconds[result.optionIndex];
    } else if (selected == 4) {
        HermesXFastSetupUiModel::instance().reset(HermesFastSetupPage::NodeDatabaseMenu);
        result.action = HermesXFastSetupAction::ResetAllNodesRequested;
    }
    return result;
}

HermesXFastSetupSelectionResult HermesXFastSetupUiController::activateMqttMapPrecisionSelect(int selected, int optionCount)
{
    return activateIndexedSelection(selected, optionCount, HermesFastSetupPage::MqttMapReportMenu,
                                    HermesXFastSetupAction::ApplyMqttMapPrecisionRequested);
}

HermesXFastSetupSelectionResult HermesXFastSetupUiController::activateMqttMapPublishSelect(int selected, int optionCount)
{
    return activateIndexedSelection(selected, optionCount, HermesFastSetupPage::MqttMapReportMenu,
                                    HermesXFastSetupAction::ApplyMqttMapPublishRequested);
}

HermesXFastSetupSelectionResult HermesXFastSetupUiController::activateChannelPrecisionSelect(int selected, int optionCount)
{
    return activateIndexedSelection(selected, optionCount, HermesFastSetupPage::ChannelDetailMenu,
                                    HermesXFastSetupAction::ApplyChannelPrecisionRequested);
}

const char *HermesXFastSetupUiController::nodeMenuItemName(int selected) const
{
    static const char *const names[] = {
        "返回", "DeviceInfo", "LoRa", "GPS", "MQTT", "Channel", "Bluetooth", "Power", "NodeDB", "Update",
    };
    return selected >= 0 && selected < static_cast<int>(sizeof(names) / sizeof(names[0])) ? names[selected] : "未知";
}

const char *HermesXFastSetupUiController::powerMenuItemName(int selected) const
{
    static const char *const names[] = {"返回", "當前電壓", "過放保護", "過放門檻"};
    return selected >= 0 && selected < static_cast<int>(sizeof(names) / sizeof(names[0])) ? names[selected] : "未知";
}

const char *HermesXFastSetupUiController::uiMenuItemName(int selected) const
{
    static const char *const names[] = {
        "返回", "全域蜂鳴器", "Hermes狀態條", "板載RGB燈", "螢幕休眠時間", "時區設定", "旋鈕對調", "新訊息提示",
    };
    return selected >= 0 && selected < static_cast<int>(sizeof(names) / sizeof(names[0])) ? names[selected] : "未知";
}

const char *HermesXFastSetupUiController::mqttMenuItemName(int selected) const
{
    static const char *const names[] = {"返回", "MQTT", "客戶端代理", "地圖報告"};
    return selected >= 0 && selected < static_cast<int>(sizeof(names) / sizeof(names[0])) ? names[selected] : "未知";
}

} // namespace graphics
