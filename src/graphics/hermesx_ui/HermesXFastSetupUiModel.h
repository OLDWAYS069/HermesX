#pragma once

#include <cstdint>

namespace graphics
{

enum class HermesFastSetupPage : uint8_t {
    Entry,
    Root,
    EmacMenu,
    EmacEmInfoMenu,
    EmacEmInfoIntervalSelect,
    EmacHeartbeatIntervalSelect,
    EmacOfflineThresholdSelect,
    EmacBatteryIncludeSelect,
    UiMenu,
    UiBrightnessSelect,
    UiScreenSleepSelect,
    UiTimezoneSelect,
    UiRotarySwapSelect,
    NodeMenu,
    UpdateIntro,
    UpdateExitPending,
    UpdateMenu,
    UpdateCheckMenu,
    UpdateCheckFlowPage,
    UpdateDetailPopup,
    UpdateRuntimeMenu,
    UpdateWifiConfigMenu,
    UpdateWifiMenu,
    UpdateUploadMenu,
    UpdateApplyMenu,
    UpdateWifiSsidEdit,
    UpdateWifiPasswordEdit,
    DeviceInfoMenu,
    DeviceInfoShortNameEdit,
    DeviceInfoLongNameEdit,
    DeviceInfoBroadcastSelect,
    NodeDatabaseMenu,
    NodeDatabaseResetSelect,
    MqttMenu,
    MqttMapReportMenu,
    MqttMapPrecisionSelect,
    MqttMapPublishSelect,
    ChannelMenu,
    ChannelDetailMenu,
    ChannelPrecisionSelect,
    PowerMenu,
    PowerGuardVoltageSelect,
    PassEdit,
    FrequencyEdit,
    PassShow,
    LoraMenu,
    LoraRoleSelect,
    LoraPresetSelect,
    LoraRegionSelect,
    LoraChannelSlotSelect,
    CannedMenu,
    CannedChannelSelect,
    GpsMenu,
    GpsUpdateSelect,
    GpsBroadcastSelect,
    GpsSmartDistanceSelect,
    GpsSmartIntervalSelect,
};

struct HermesXFastSetupNavigationState {
    HermesFastSetupPage page = HermesFastSetupPage::Entry;
    HermesFastSetupPage returnPage = HermesFastSetupPage::Root;
    int16_t selected = 0;
    int16_t offset = 0;
    uint32_t lastNavAtMs = 0;
    int8_t lastNavDir = 0;
};

class HermesXFastSetupUiModel
{
  public:
    static constexpr uint8_t VisibleRows = 4;

    static HermesXFastSetupUiModel &instance();

    HermesXFastSetupNavigationState &navigation() { return navigation_; }
    const HermesXFastSetupNavigationState &navigation() const { return navigation_; }

    void reset(HermesFastSetupPage page);
    void enter(HermesFastSetupPage page, int count, int selected);
    void openDetail(HermesFastSetupPage returnPage);
    void updateOffset(int count);

  private:
    HermesXFastSetupUiModel() = default;

    HermesXFastSetupNavigationState navigation_;
};

} // namespace graphics
