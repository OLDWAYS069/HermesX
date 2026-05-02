#pragma once

#include "configuration.h"

#if HAS_SCREEN

#include "SinglePortModule.h"
#include "mesh/HermesPortnums.h"
#include "mesh/generated/meshtastic/mesh.pb.h"
#include "input/InputBroker.h"
#include "modules/HermesXInterfaceModule.h"
#include "concurrency/OSThread.h"
#include <Arduino.h>

class HermesXEmUiModule : public SinglePortModule, public Observable<const UIFrameEvent *>, private concurrency::OSThread
{
    CallbackObserver<HermesXEmUiModule, const InputEvent *> inputObserver =
        CallbackObserver<HermesXEmUiModule, const InputEvent *>(this, &HermesXEmUiModule::handleInputEvent);

  public:
    HermesXEmUiModule();

    void enterEmergencyMode(const char *reason = nullptr);
    void exitEmergencyMode();
    bool isActive() const { return active; }
    bool isSirenEnabled() const { return sirenEnabled; }
    void setSirenEnabled(bool enabled);
    void tickSiren(uint32_t now);
    void drawOverlay(OLEDDisplay *display, OLEDDisplayUiState *state);
    void sendResetLighthouseNow();
    bool isEmInfoBroadcastEnabled() const { return emInfoBroadcastEnabled; }
    void setEmInfoBroadcastEnabled(bool enabled);
    uint32_t getEmInfoIntervalSec() const { return emInfoIntervalSec; }
    void setEmInfoIntervalSec(uint32_t seconds);
    uint32_t getEmHeartbeatIntervalSec() const { return emHeartbeatIntervalSec; }
    void setEmHeartbeatIntervalSec(uint32_t seconds);
    uint8_t getEmOfflineThresholdCount() const { return emOfflineThresholdCount; }
    void setEmOfflineThresholdCount(uint8_t count);
    bool isEmBatteryIncluded() const { return emBatteryIncluded; }
    void setEmBatteryIncluded(bool enabled);

  protected:
    bool wantPacket(const meshtastic_MeshPacket *p) override
    {
        return p->decoded.portnum == PORTNUM_HERMESX_EMERGENCY ||
               (awaitingAck && p->decoded.portnum == meshtastic_PortNum_ROUTING_APP);
    }
    bool wantUIFrame() override { return active; }
    Observable<const UIFrameEvent *> *getUIFrameObservable() override { return this; }
    void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) override;
    bool interceptingKeyboardInput() override { return active; }
    int handleInputEvent(const InputEvent *event);
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    int32_t runOnce() override;

  private:
    enum class EmAction {
        Trapped,
        Medical,
        Supplies,
        OkHere,
    };

    enum class UiMode {
        Menu,
        PassphraseEdit,
        FiveLineReport,
        TextEdit,
        Stats,
        DeviceStatus,
        DeviceDetail,
        RelativePosition,
    };

    enum class ReportEditField {
        None,
        Place,
        Item,
    };

    enum class TextEditContext {
        None,
        ReportPlace,
        ReportItem,
    };

    struct EmergencyReportStats {
        uint16_t trapped = 0;
        uint16_t medical = 0;
        uint16_t supplies = 0;
        uint16_t safe = 0;
    };

    struct EmInfoNodeStatus {
        NodeNum nodeNum = 0;
        char shortName[16] = {0};
        char state[20] = {0};
        char place[24] = {0};
        char item[24] = {0};
        char timeCode[20] = {0};
        int32_t latitudeI = 0;
        int32_t longitudeI = 0;
        int32_t altitude = 0;
        uint8_t batteryPercent = 0;
        uint32_t lastSeenMs = 0;
        uint32_t lastHeartbeatMs = 0;
        uint32_t remoteTimestamp = 0;
        uint8_t heartbeatSeq = 0;
        bool heartbeatActive = false;
        bool valid = false;
    };

    bool active = false;
    UiMode uiMode = UiMode::Menu;
    String banner;
    int selectedIndex = 0;
    int listOffset = 0;
    bool awaitingAck = false;
    PacketId lastRequestId = 0;
    bool lastAckSuccess = false;
    uint32_t lastAckAtMs = 0;
    bool selectionArmed = false;
    uint32_t lastNavAtMs = 0;
    int8_t lastNavDir = 0;
    uint32_t lastSendAtMs = 0;
    bool hasSentOnce = false;
    bool screamActive = false;
    bool sirenEnabled = true;
    uint32_t nextScreamAtMs = 0;
    EmergencyReportStats reportStats;
    static constexpr uint8_t kMaxEmInfoNodes = 16;
    EmInfoNodeStatus emInfoNodes[kMaxEmInfoNodes]{};
    bool emInfoBroadcastEnabled = true;
    uint32_t lastEmInfoSentMs = 0;
    uint32_t lastEmHeartbeatSentMs = 0;
    uint8_t currentEmInfoStateCode = 0;
    uint32_t emInfoIntervalSec = 0;
    uint32_t emHeartbeatIntervalSec = 5;
    uint8_t emOfflineThresholdCount = 3;
    bool emBatteryIncluded = true;
    uint8_t emHeartbeatSeq = 0;
    int deviceStatusOffset = 0;
    int deviceStatusSelectedIndex = 0;
    int deviceDetailScrollOffset = 0;
    int deviceDetailSelectedIndex = 0;
    LedTheme savedTheme{};
    bool themeSaved = false;
    uint8_t editingPassSlot = 0;
    String passDraft;
    EmAction pendingReportAction = EmAction::Trapped;
    ReportEditField reportEditField = ReportEditField::None;
    TextEditContext textEditContext = TextEditContext::None;
    String reportPlaceDraft;
    String reportItemDraft;
    String textEditDraft;
    int reportSelectedIndex = 0;
    int reportListOffset = 0;
    String lastAlertMessage;
    int keyRow = 0;
    int keyCol = 0;

    void setThemeActive(bool enabled);
    void sendEmergencyAction(EmAction action);
    void sendFiveLineReport();
    void sendResetLighthouse();
    void enterPassphraseEdit(uint8_t slot);
    void exitPassphraseEdit(bool save);
    void enterFiveLineReport(EmAction action);
    void exitFiveLineReport();
    void enterReportTextEdit(ReportEditField field);
    void sendLocalTextToPhone(const String &text);
    void notifyPassphraseSaved(uint8_t slot, const String &value, bool ok);
    void recordEmergencyReportPayload(const meshtastic_MeshPacket &mp);
    void recordEmInfoPayload(const meshtastic_MeshPacket &mp);
    void recordEmHeartbeatPayload(const meshtastic_MeshPacket &mp);
    EmInfoNodeStatus *findOrCreateNodeStatus(NodeNum nodeNum);
    void sendEmInfoNow();
    void sendEmHeartbeatNow();
    uint32_t getEmInfoIntervalMs() const;
    uint32_t getEmHeartbeatIntervalMs() const;
    int getVisibleEmInfoNodeCount() const;
    const EmInfoNodeStatus *getVisibleEmInfoNodeByIndex(int index) const;
    const char *getNodePresenceLabel(const EmInfoNodeStatus &entry) const;
    String getNodeTimeLabel(const EmInfoNodeStatus &entry) const;
    String getNodeRelativeHeardLabel(const EmInfoNodeStatus &entry) const;
    const EmInfoNodeStatus *getSelectedEmInfoNode() const;
    String getNodeLatitudeText(const EmInfoNodeStatus &entry) const;
    String getNodeLongitudeText(const EmInfoNodeStatus &entry) const;
    String getNodeAltitudeText(const EmInfoNodeStatus &entry) const;
    int buildDeviceDetailLines(const EmInfoNodeStatus &entry, String *lines, int maxLines) const;
    int buildRelativePositionLines(const EmInfoNodeStatus &entry, String *lines, int maxLines) const;
    void updateListOffset();
    void updateReportListOffset();
    void updateDeviceStatusOffset();
    String buildReportActionZh() const;
    String buildReportTimeCode() const;
    String getTextEditHeader() const;
    int handleMenuInput(const InputEvent *event);
    int handlePassphraseInput(const InputEvent *event);
    int handleFiveLineReportInput(const InputEvent *event);
    int handleTextEditInput(const InputEvent *event);
    int handleStatsInput(const InputEvent *event);
    int handleDeviceStatusInput(const InputEvent *event);
    void exitTextEdit(bool save);
    void startScream();
    void stopScream();
    void tickScream(uint32_t now);
};

extern HermesXEmUiModule *hermesXEmUiModule;

#endif
