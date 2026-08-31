#pragma once

#include <Arduino.h>
#include <OLEDDisplay.h>
#include <cstdint>

namespace graphics
{

struct HermesXFastSetupListContext {
    HermesXFastSetupListContext(OLEDDisplay *display,
                                int16_t width,
                                int16_t height,
                                int selectedIndex,
                                int listOffset,
                                bool forceFullRedraw,
                                String &toast,
                                uint32_t &toastUntilMs)
        : display(display), width(width), height(height), selectedIndex(selectedIndex), listOffset(listOffset),
          forceFullRedraw(forceFullRedraw), toast(toast), toastUntilMs(toastUntilMs)
    {
    }

    OLEDDisplay *display;
    int16_t width;
    int16_t height;
    int selectedIndex;
    int listOffset;
    bool forceFullRedraw;
    String &toast;
    uint32_t &toastUntilMs;
};

class HermesXFastSetupUiRenderer
{
  public:
    static void drawEntryPage(OLEDDisplay *display, int16_t width, int16_t height);
    static void drawHeader(OLEDDisplay *display, int16_t width, const char *title);
    static void drawNutIcon(OLEDDisplay *display, int16_t centerX, int16_t centerY, int16_t radius);
    static void drawList(OLEDDisplay *display,
                         int16_t width,
                         int16_t height,
                         const char *title,
                         const char *const *items,
                         int itemCount,
                         int selectedIndex,
                         int listOffset);
    static void drawToast(OLEDDisplay *display,
                          int16_t width,
                          int16_t height,
                          String &toast,
                          uint32_t &toastUntilMs);
    static void resetTftPalette(OLEDDisplay *display);
    static void applyTftPalette(OLEDDisplay *display,
                                int16_t width,
                                int16_t height,
                                bool forceFullRedraw);
    static void drawNodeDatabaseMenu(const HermesXFastSetupListContext &context, bool resetSelection);
    static void drawMqttMenu(const HermesXFastSetupListContext &context,
                             bool mqttEnabled,
                             bool proxyEnabled,
                             bool mapEnabled);
    static void drawMqttMapReportMenu(const HermesXFastSetupListContext &context,
                                      bool mapEnabled,
                                      const char *precisionLabel,
                                      const char *publishIntervalLabel);
    static void drawSelectionMenu(const HermesXFastSetupListContext &context,
                                  const char *title,
                                  const char *const *optionLabels,
                                  int optionCount);
    static void drawMenu(const HermesXFastSetupListContext &context,
                         const char *title,
                         const char *const *items,
                         int itemCount);
    static void drawToggleSelectionMenu(const HermesXFastSetupListContext &context, const char *title);
    static void drawChannelMenu(const HermesXFastSetupListContext &context,
                                const char *const *channelLabels,
                                int channelCount);
    static void drawChannelDetailMenu(const HermesXFastSetupListContext &context,
                                      const char *title,
                                      bool uplinkEnabled,
                                      bool downlinkEnabled,
                                      bool positionSharingEnabled,
                                      const char *precisionLabel);
    static void drawPowerMenu(const HermesXFastSetupListContext &context,
                              const char *currentVoltageLabel,
                              bool guardEnabled,
                              const char *thresholdLabel);
    static void drawLoraMenu(const HermesXFastSetupListContext &context,
                             const char *roleLabel,
                             const char *presetLabel,
                             const char *regionLabel,
                             bool ignoreMqtt,
                             bool allowMqttForward,
                             bool txEnabled,
                             const char *channelSlotLabel,
                             const char *frequencyLabel);
    static void drawLoraChannelSlotMenu(const HermesXFastSetupListContext &context, int channelSlotCount);
    static void drawGpsMenu(const HermesXFastSetupListContext &context,
                            uint32_t updateSeconds,
                            const char *updateLabel,
                            uint32_t broadcastSeconds,
                            const char *broadcastLabel,
                            bool smartEnabled,
                            const char *smartDistanceLabel,
                            const char *smartIntervalLabel);
    static void drawGroupPins(const HermesXFastSetupListContext &context, const char *pinA, const char *pinB);
    static void drawUiMenu(const HermesXFastSetupListContext &context,
                           bool buzzerEnabled,
                           const char *brightnessLabel,
                           bool ambientEnabled,
                           const char *screenSleepLabel,
                           const char *timezoneLabel,
                           bool rotarySwapped,
                           bool messagePopupEnabled);
    static void drawEmInfoMenu(const HermesXFastSetupListContext &context,
                               bool broadcastEnabled,
                               const char *emInfoIntervalLabel,
                               const char *heartbeatIntervalLabel,
                               int offlineThreshold,
                               bool batteryIncluded);
    static void drawCannedMenu(const HermesXFastSetupListContext &context, const char *channelName);
    static void drawNodeMenu(const HermesXFastSetupListContext &context,
                             bool mqttEnabled,
                             bool bluetoothEnabled);
    static void drawDeviceInfoMenu(const HermesXFastSetupListContext &context,
                                   const char *shortName,
                                   const char *longName,
                                   const char *nodeId,
                                   const char *broadcastLabel);
    static void drawUpdateMenu(const HermesXFastSetupListContext &context,
                               bool dedicatedUpdateBoot,
                               const char *currentVersion);
    static void drawUpdateCheckMenu(const HermesXFastSetupListContext &context,
                                    const char *currentVersion,
                                    const char *remoteUrl,
                                    const char *sourceStatus,
                                    const char *candidateVersion,
                                    int progressPercent,
                                    const char *lastError,
                                    bool hasCandidate,
                                    bool canApply);
    static void drawUpdateRuntimeMenu(const HermesXFastSetupListContext &context);
    static void drawUpdateWifiConfigMenu(const HermesXFastSetupListContext &context,
                                         bool enabled,
                                         const char *ssid,
                                         const char *maskedPassword,
                                         bool dirty,
                                         const char *ipLabel);
    static void drawUpdateTransportMenu(const HermesXFastSetupListContext &context,
                                        bool wifiTransport,
                                        const char *currentVersion,
                                        const char *connectionStatus);
    static void drawUpdateProgressPage(OLEDDisplay *display,
                                       int16_t width,
                                       int16_t height,
                                       const char *title,
                                       const String &instruction,
                                       int progressPercent,
                                       const String &statusLine,
                                       const String &targetVersion,
                                       const String &errorLine,
                                       bool canApply,
                                       int actionCount,
                                       const char *primaryLabel,
                                       const char *secondaryLabel,
                                       int selectedIndex,
                                       String &toast,
                                       uint32_t &toastUntilMs);
    static void drawUpdateTransitionPage(OLEDDisplay *display,
                                         int16_t width,
                                         int16_t height,
                                         uint32_t startedAtMs,
                                         const char *title,
                                         const char *status);
    static void drawUpdateIntroPage(OLEDDisplay *display, int16_t width, int16_t height, uint32_t startedAtMs);
    static void drawUrlUpdateFlowPage(OLEDDisplay *display,
                                      int16_t width,
                                      int16_t height,
                                      const String &statusValue,
                                      const String &candidateVersion,
                                      int progressPercent,
                                      const String &errorValue,
                                      const char *actionLabel,
                                      int selectedIndex,
                                      String &toast,
                                      uint32_t &toastUntilMs);
    static void drawKeyboardPage(OLEDDisplay *display,
                                 int16_t width,
                                 int16_t height,
                                 const char *header,
                                 const String &draft,
                                 const char *const (*rows)[10],
                                 const uint8_t *rowLengths,
                                 uint8_t rowCount,
                                 uint8_t selectedRow,
                                 uint8_t selectedCol,
                                 String &toast,
                                 uint32_t &toastUntilMs);
    static void drawDetailPopup(OLEDDisplay *display,
                                const String &title,
                                const String &body,
                                uint16_t &scrollY,
                                uint16_t &maxScrollY);
};

} // namespace graphics
