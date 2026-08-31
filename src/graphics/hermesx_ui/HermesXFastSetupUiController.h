#pragma once

#include "HermesXFastSetupUiModel.h"
#include <cstdint>

namespace graphics
{

enum class HermesXFastSetupNavigationAction : uint8_t {
    Ignored,
    Consumed,
    Changed,
};

enum class HermesXFastSetupAction : uint8_t {
    Ignored,
    StateChanged,
    ExitRequested,
    SaveAndRebootRequested,
    OpenChannelMenuRequested,
    ToggleBluetoothRequested,
    EnterUpdateModeRequested,
    OpenCannedChannelRequested,
    EditDeviceShortNameRequested,
    EditDeviceLongNameRequested,
    ShowNodeIdRequested,
    OpenDeviceBroadcastRequested,
    RequestNodeInfoRequested,
    TogglePowerGuardRequested,
    OpenPowerGuardThresholdRequested,
    OpenGpsUpdateRequested,
    OpenGpsBroadcastRequested,
    ToggleGpsSmartRequested,
    OpenGpsSmartDistanceRequested,
    OpenGpsSmartIntervalRequested,
    ToggleBuzzerRequested,
    OpenUiBrightnessRequested,
    ToggleRgbRequested,
    OpenScreenSleepRequested,
    OpenTimezoneRequested,
    OpenRotarySwapRequested,
    ToggleIncomingPopupRequested,
    OpenLoraRoleRequested,
    OpenLoraPresetRequested,
    OpenLoraRegionRequested,
    ToggleIgnoreMqttRequested,
    ToggleMqttForwardRequested,
    ToggleLoraTxRequested,
    OpenLoraChannelSlotRequested,
    EditLoraFrequencyRequested,
    ReturnFromGroupRequested,
    OpenEmInfoRequested,
    EditGroupPin0Requested,
    EditGroupPin1Requested,
    ShowGroupPinsRequested,
    ResetLighthouseRequested,
    ToggleEmInfoRequested,
    OpenEmInfoIntervalRequested,
    OpenHeartbeatIntervalRequested,
    OpenOfflineThresholdRequested,
    OpenBatteryIncludeRequested,
    LeaveUpdateModeRequested,
    ReturnToUpdateMenuRequested,
    ReturnToUpdateCheckRequested,
    ReturnToUpdateRuntimeRequested,
    ReturnFromUpdateApplyRequested,
    ShowCurrentVersionRequested,
    OpenUpdateWifiConfigRequested,
    OpenUpdateCheckRequested,
    OpenUpdateRuntimeRequested,
    ShowUpdateSourceRequested,
    StartUpdateCheckFlowRequested,
    ShowUpdateStatusRequested,
    ShowUpdateCandidateRequested,
    ShowUpdateProgressRequested,
    ShowUpdateErrorRequested,
    ContinueUpdateCheckFlowRequested,
    OpenWifiUpdateFlowRequested,
    OpenUsbUpdateFlowRequested,
    ShowWifiStatusRequested,
    StartWifiUpdateRequested,
    ToggleWifiDraftRequested,
    EditWifiSsidRequested,
    EditWifiPasswordRequested,
    SaveWifiDraftRequested,
    ShowWifiIpRequested,
    ShowUsbStatusRequested,
    StartUsbUpdateRequested,
    ApplyUpdateRequested,
    ToggleMqttRequested,
    ToggleMqttProxyRequested,
    OpenMqttMapReportRequested,
    ToggleMqttMapReportRequested,
    OpenMqttMapPrecisionRequested,
    OpenMqttMapPublishRequested,
    OpenChannelDetailRequested,
    ToggleChannelUplinkRequested,
    ToggleChannelDownlinkRequested,
    ToggleChannelPositionRequested,
    OpenChannelPrecisionRequested,
    CleanupNodesRequested,
    ResetAllNodesRequested,
    ApplyMqttMapPrecisionRequested,
    ApplyMqttMapPublishRequested,
    ApplyChannelPrecisionRequested,
};

struct HermesXFastSetupSelectionResult {
    HermesXFastSetupAction action = HermesXFastSetupAction::Ignored;
    int optionIndex = -1;
    uint32_t value = 0;
};

class HermesXFastSetupUiController
{
  public:
    static HermesXFastSetupUiController &instance();

    bool acceptNavigation(int8_t direction, bool rotaryInput, uint32_t nowMs);
    HermesXFastSetupNavigationAction navigate(int8_t direction, int count, bool rotaryInput, uint32_t nowMs);
    HermesXFastSetupAction activateRoot(int selected);
    HermesXFastSetupAction activateNodeMenu(int selected);
    HermesXFastSetupAction activateCannedMenu(int selected);
    HermesXFastSetupAction activateDeviceInfoMenu(int selected);
    HermesXFastSetupAction activatePowerMenu(int selected);
    HermesXFastSetupAction activateGpsMenu(int selected);
    HermesXFastSetupAction activateUiMenu(int selected);
    HermesXFastSetupAction activateLoraMenu(int selected);
    HermesXFastSetupAction activateEmacMenu(int selected);
    HermesXFastSetupAction activateEmInfoMenu(int selected);
    HermesXFastSetupAction activateUpdateMenu(int selected, bool dedicatedUpdateBoot);
    HermesXFastSetupAction activateUpdateCheckMenu(int selected);
    HermesXFastSetupAction activateUpdateCheckFlow(int selected);
    HermesXFastSetupAction activateUpdateRuntimeMenu(int selected);
    HermesXFastSetupAction activateUpdateWifiMenu(int selected);
    HermesXFastSetupAction activateUpdateWifiConfigMenu(int selected);
    HermesXFastSetupAction activateUpdateUploadMenu(int selected);
    HermesXFastSetupAction activateUpdateApplyMenu(int selected, bool canApply);
    HermesXFastSetupAction activateMqttMenu(int selected);
    HermesXFastSetupAction activateMqttMapReportMenu(int selected);
    HermesXFastSetupAction activateChannelMenu(int selected);
    HermesXFastSetupAction activateChannelDetailMenu(int selected);
    HermesXFastSetupAction activateNodeDatabaseMenu(int selected);
    HermesXFastSetupSelectionResult activateNodeDatabaseResetSelect(int selected);
    HermesXFastSetupSelectionResult activateMqttMapPrecisionSelect(int selected, int optionCount);
    HermesXFastSetupSelectionResult activateMqttMapPublishSelect(int selected, int optionCount);
    HermesXFastSetupSelectionResult activateChannelPrecisionSelect(int selected, int optionCount);
    const char *nodeMenuItemName(int selected) const;
    const char *powerMenuItemName(int selected) const;
    const char *uiMenuItemName(int selected) const;
    const char *mqttMenuItemName(int selected) const;

  private:
    HermesXFastSetupUiController() = default;
};

} // namespace graphics
