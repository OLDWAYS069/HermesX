#include "HermesXFastSetupUiRenderer.h"

#include "HermesXTextLayout.h"

#include "graphics/ScreenFonts.h"
#include "graphics/TFTDisplay.h"
#include "graphics/fonts/HermesX_zh/HermesX_CN12.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace graphics
{
namespace
{

constexpr int16_t HeaderHeight = 14;
constexpr int16_t RowHeight = 12;
constexpr uint8_t VisibleRows = 4;
constexpr uint32_t UpdateTransitionMs = 900U;

#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
struct FastSetupTftPaletteState {
    const OLEDDisplay *display = nullptr;
    int16_t width = 0;
    int16_t height = 0;
    bool valid = false;
};

FastSetupTftPaletteState paletteState;
#endif

template <typename LabelProvider>
void drawListItems(OLEDDisplay *display,
                   int16_t width,
                   int16_t height,
                   const char *title,
                   int itemCount,
                   int selectedIndex,
                   int listOffset,
                   LabelProvider labelAt)
{
    if (!display) {
        return;
    }
    display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
    display->fillRect(0, 0, width, height);
    HermesXFastSetupUiRenderer::drawHeader(display, width, title);
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif

    const int16_t listTop = HeaderHeight + 2;
    for (int row = 0; row < VisibleRows; ++row) {
        const int entryIndex = listOffset + row;
        if (entryIndex >= itemCount) {
            break;
        }
        const int16_t rowY = listTop + row * RowHeight;
        if (rowY + RowHeight > height) {
            break;
        }
        if (entryIndex == selectedIndex) {
#if defined(USE_EINK)
            display->fillRect(0, rowY - 1, width, RowHeight);
            display->setColor(EINK_WHITE);
#else
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
            display->fillRect(0, rowY - 1, width, RowHeight);
            display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
        }
        HermesX_zh::drawMixedBounded(
            *display, 4, rowY, width - 4, labelAt(entryIndex), HermesX_zh::GLYPH_WIDTH, RowHeight, nullptr);
        if (entryIndex == selectedIndex) {
#if defined(USE_EINK)
            display->setColor(EINK_BLACK);
#else
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
        }
    }
}

void prepareList(const HermesXFastSetupListContext &context)
{
    HermesXFastSetupUiRenderer::applyTftPalette(
        context.display, context.width, context.height, context.forceFullRedraw);
}

void finishList(const HermesXFastSetupListContext &context)
{
    HermesXFastSetupUiRenderer::drawToast(
        context.display, context.width, context.height, context.toast, context.toastUntilMs);
}

} // namespace

void HermesXFastSetupUiRenderer::drawEntryPage(OLEDDisplay *display, int16_t width, int16_t height)
{
    if (!display) {
        return;
    }
    display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
    display->fillRect(0, 0, width, height);
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif

    drawHeader(display, width, u8"HermesFastSetup");

    const int16_t line1Y = height - (FONT_HEIGHT_SMALL * 2) - 2;
    const int16_t line2Y = height - FONT_HEIGHT_SMALL;
    const int16_t iconTop = HeaderHeight + 2;
    const int16_t iconBottom = line1Y - 4;
    int16_t iconRadius = (iconBottom > iconTop) ? ((iconBottom - iconTop) / 2) : 10;
    const int16_t maxRadiusByWidth = (width / 2) - 6;
    if (iconRadius > maxRadiusByWidth) {
        iconRadius = maxRadiusByWidth;
    }
    if (iconRadius < 10) {
        iconRadius = 10;
    }
    const int16_t iconCx = width / 2;
    const int16_t iconCy = iconTop + iconRadius;
    drawNutIcon(display, iconCx, iconCy, iconRadius);

    HermesX_zh::drawMixedBounded(*display, 2, line1Y, width - 4, u8"旋轉以進入設定", HermesX_zh::GLYPH_WIDTH,
                                  FONT_HEIGHT_SMALL, nullptr);
    HermesX_zh::drawMixedBounded(*display, 2, line2Y, width - 4, u8"短按=下一頁", HermesX_zh::GLYPH_WIDTH,
                                  FONT_HEIGHT_SMALL, nullptr);
}

void HermesXFastSetupUiRenderer::drawHeader(OLEDDisplay *display, int16_t width, const char *title)
{
    if (!display) {
        return;
    }
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
    display->fillRect(0, 0, width, HeaderHeight);
#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
    display->setFont(FONT_SMALL);
    HermesX_zh::drawMixed(*display, 2, 1, title, HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
}

void HermesXFastSetupUiRenderer::drawNutIcon(OLEDDisplay *display, int16_t centerX, int16_t centerY, int16_t radius)
{
    if (!display) {
        return;
    }
    radius = std::max<int16_t>(10, radius);
    constexpr float Pi = 3.14159265f;
    int16_t outerX[6]{};
    int16_t outerY[6]{};
    int16_t innerX[6]{};
    int16_t innerY[6]{};
    const int16_t innerRadius = (radius * 62) / 100;

    for (uint8_t i = 0; i < 6; ++i) {
        const float angle = (Pi / 3.0f) * static_cast<float>(i) - (Pi / 2.0f);
        outerX[i] = centerX + static_cast<int16_t>(cosf(angle) * radius);
        outerY[i] = centerY + static_cast<int16_t>(sinf(angle) * radius);
        innerX[i] = centerX + static_cast<int16_t>(cosf(angle) * innerRadius);
        innerY[i] = centerY + static_cast<int16_t>(sinf(angle) * innerRadius);
    }
    for (uint8_t i = 0; i < 6; ++i) {
        const uint8_t next = (i + 1) % 6;
        display->drawLine(outerX[i], outerY[i], outerX[next], outerY[next]);
        display->drawLine(innerX[i], innerY[i], innerX[next], innerY[next]);
        display->drawLine(outerX[i], outerY[i], innerX[i], innerY[i]);
    }

    const int16_t holeRadius = radius / 3;
#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
    display->fillCircle(centerX, centerY, holeRadius);
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
    display->drawCircle(centerX, centerY, holeRadius);
}

void HermesXFastSetupUiRenderer::drawList(OLEDDisplay *display,
                                           int16_t width,
                                           int16_t height,
                                           const char *title,
                                           const char *const *items,
                                           int itemCount,
                                           int selectedIndex,
                                           int listOffset)
{
    drawListItems(display, width, height, title, itemCount, selectedIndex, listOffset,
                  [items](int index) { return items[index]; });
}

void HermesXFastSetupUiRenderer::drawToast(OLEDDisplay *display,
                                            int16_t width,
                                            int16_t height,
                                            String &toast,
                                            uint32_t &toastUntilMs)
{
    if (!display || toastUntilMs == 0) {
        return;
    }
    if (millis() > toastUntilMs) {
        toastUntilMs = 0;
        toast = "";
        return;
    }
    if (toast.length() > 0) {
        HermesX_zh::drawMixedBounded(*display, 2, height - FONT_HEIGHT_SMALL, width - 4, toast.c_str(),
                                     HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
    }
}

void HermesXFastSetupUiRenderer::resetTftPalette(OLEDDisplay *display)
{
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
    if (display) {
        static_cast<TFTDisplay *>(display)->resetColorPalette(false);
    }
    paletteState.valid = false;
#else
    (void)display;
#endif
}

void HermesXFastSetupUiRenderer::applyTftPalette(OLEDDisplay *display,
                                                  int16_t width,
                                                  int16_t height,
                                                  bool forceFullRedraw)
{
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
    const bool unchanged = paletteState.valid && paletteState.display == display && paletteState.width == width &&
                           paletteState.height == height;
    if (unchanged && !forceFullRedraw) {
        return;
    }
    auto *tft = static_cast<TFTDisplay *>(display);
    tft->clearColorPaletteZones();
    paletteState.display = display;
    paletteState.width = width;
    paletteState.height = height;
    paletteState.valid = true;
    if (forceFullRedraw) {
        tft->markColorPaletteDirty();
    }
#else
    (void)display;
    (void)width;
    (void)height;
    (void)forceFullRedraw;
#endif
}

void HermesXFastSetupUiRenderer::drawNodeDatabaseMenu(const HermesXFastSetupListContext &context, bool resetSelection)
{
    prepareList(context);
    if (resetSelection) {
        static const char *const items[] = {
            u8"返回", u8"清除12hr未更新", u8"清除24hr未更新", u8"清除48hr未更新", u8"全部清除",
        };
        drawList(context.display, context.width, context.height, u8"重設資料庫", items, 5, context.selectedIndex,
                 context.listOffset);
    } else {
        static const char *const items[] = {u8"返回", u8"重設資料庫"};
        drawList(context.display, context.width, context.height, u8"節點資料庫", items, 2, context.selectedIndex,
                 context.listOffset);
    }
    finishList(context);
}

void HermesXFastSetupUiRenderer::drawMqttMenu(const HermesXFastSetupListContext &context,
                                               bool mqttEnabled,
                                               bool proxyEnabled,
                                               bool mapEnabled)
{
    prepareList(context);
    String mqttLine = String("MQTT: ") + (mqttEnabled ? u8"開" : u8"關");
    String proxyLine = String(u8"啟用客戶端代理: ") + (proxyEnabled ? u8"開" : u8"關");
    String mapLine = String(u8"地圖報告: ") + (mapEnabled ? u8"開" : u8"關");
    const char *items[] = {u8"返回", mqttLine.c_str(), proxyLine.c_str(), mapLine.c_str()};
    drawList(context.display, context.width, context.height, "MQTT", items, 4, context.selectedIndex,
             context.listOffset);
    finishList(context);
}

void HermesXFastSetupUiRenderer::drawMqttMapReportMenu(const HermesXFastSetupListContext &context,
                                                        bool mapEnabled,
                                                        const char *precisionLabel,
                                                        const char *publishIntervalLabel)
{
    prepareList(context);
    String enabledLine = String(u8"上報: ") + (mapEnabled ? u8"開" : u8"關");
    String precisionLine = String(u8"精確度: ") + precisionLabel;
    String publishLine = String(u8"廣播間隔: ") + publishIntervalLabel;
    const char *items[] = {u8"返回", enabledLine.c_str(), precisionLine.c_str(), publishLine.c_str()};
    drawList(context.display, context.width, context.height, u8"地圖報告", items, 4, context.selectedIndex,
             context.listOffset);
    finishList(context);
}

void HermesXFastSetupUiRenderer::drawSelectionMenu(const HermesXFastSetupListContext &context,
                                                    const char *title,
                                                    const char *const *optionLabels,
                                                    int optionCount)
{
    prepareList(context);
    drawListItems(context.display, context.width, context.height, title, optionCount + 1, context.selectedIndex,
                  context.listOffset,
                  [optionLabels](int index) { return index == 0 ? u8"返回" : optionLabels[index - 1]; });
    finishList(context);
}

void HermesXFastSetupUiRenderer::drawMenu(const HermesXFastSetupListContext &context,
                                           const char *title,
                                           const char *const *items,
                                           int itemCount)
{
    prepareList(context);
    drawList(context.display, context.width, context.height, title, items, itemCount, context.selectedIndex,
             context.listOffset);
    finishList(context);
}

void HermesXFastSetupUiRenderer::drawToggleSelectionMenu(const HermesXFastSetupListContext &context, const char *title)
{
    static const char *const labels[] = {u8"關閉", u8"開啟"};
    drawSelectionMenu(context, title, labels, 2);
}

void HermesXFastSetupUiRenderer::drawChannelMenu(const HermesXFastSetupListContext &context,
                                                  const char *const *channelLabels,
                                                  int channelCount)
{
    drawSelectionMenu(context, u8"頻道設定", channelLabels, channelCount);
}

void HermesXFastSetupUiRenderer::drawChannelDetailMenu(const HermesXFastSetupListContext &context,
                                                        const char *title,
                                                        bool uplinkEnabled,
                                                        bool downlinkEnabled,
                                                        bool positionSharingEnabled,
                                                        const char *precisionLabel)
{
    prepareList(context);
    String uplinkLine = String(u8"上行: ") + (uplinkEnabled ? u8"開" : u8"關");
    String downlinkLine = String(u8"下行: ") + (downlinkEnabled ? u8"開" : u8"關");
    String shareLine = String(u8"位置分享: ") + (positionSharingEnabled ? u8"開" : u8"關");
    String precisionLine = String(u8"精確度: ") + (positionSharingEnabled ? precisionLabel : u8"關閉");
    const char *items[] = {u8"返回", uplinkLine.c_str(), downlinkLine.c_str(), shareLine.c_str(), precisionLine.c_str()};
    drawList(context.display, context.width, context.height, title, items, 5, context.selectedIndex, context.listOffset);
    finishList(context);
}

void HermesXFastSetupUiRenderer::drawPowerMenu(const HermesXFastSetupListContext &context,
                                                const char *currentVoltageLabel,
                                                bool guardEnabled,
                                                const char *thresholdLabel)
{
    prepareList(context);
    String voltageLine = String(u8"當前電壓: ") + currentVoltageLabel;
    String guardLine = String(u8"過放保護: ") + (guardEnabled ? u8"開" : u8"關");
    String thresholdLine = String(u8"過放門檻: ") + thresholdLabel;
    const char *items[] = {u8"返回", voltageLine.c_str(), guardLine.c_str(), thresholdLine.c_str()};
    drawList(context.display, context.width, context.height, u8"電源管理", items, 4, context.selectedIndex,
             context.listOffset);
    finishList(context);
}

void HermesXFastSetupUiRenderer::drawLoraMenu(const HermesXFastSetupListContext &context,
                                               const char *roleLabel,
                                               const char *presetLabel,
                                               const char *regionLabel,
                                               bool ignoreMqtt,
                                               bool allowMqttForward,
                                               bool txEnabled,
                                               const char *channelSlotLabel,
                                               const char *frequencyLabel)
{
    prepareList(context);
    String roleLine = String("Role: ") + roleLabel;
    String presetLine = String("Preset: ") + presetLabel;
    String regionLine = String(u8"地區: ") + regionLabel;
    String ignoreMqttLine = String(u8"無視MQTT: ") + (ignoreMqtt ? u8"開" : u8"關");
    String allowMqttLine = String(u8"允許轉發至MQTT: ") + (allowMqttForward ? u8"開" : u8"關");
    String txLine = String(u8"啟用LoRa: ") + (txEnabled ? u8"開" : u8"關");
    String slotLine = String(u8"頻段槽位: ") + channelSlotLabel;
    String frequencyLine = String(u8"手動頻率: ") + frequencyLabel;
    const char *items[] = {u8"返回", roleLine.c_str(),       presetLine.c_str(), regionLine.c_str(),
                           ignoreMqttLine.c_str(), allowMqttLine.c_str(), txLine.c_str(),     slotLine.c_str(),
                           frequencyLine.c_str()};
    drawList(context.display, context.width, context.height, "LoRa", items, 9, context.selectedIndex,
             context.listOffset);
    finishList(context);
}

void HermesXFastSetupUiRenderer::drawLoraChannelSlotMenu(const HermesXFastSetupListContext &context,
                                                          int channelSlotCount)
{
    prepareList(context);
    char labelBuffers[VisibleRows][20]{};
    drawListItems(context.display, context.width, context.height, u8"頻段槽位", channelSlotCount + 2,
                  context.selectedIndex, context.listOffset, [&](int index) -> const char * {
                      if (index == 0) {
                          return u8"返回";
                      }
                      if (index == 1) {
                          return u8"自動";
                      }
                      const int visibleRow = index - context.listOffset;
                      snprintf(labelBuffers[visibleRow], sizeof(labelBuffers[visibleRow]), "%d", index - 1);
                      return labelBuffers[visibleRow];
                  });
    finishList(context);
}

void HermesXFastSetupUiRenderer::drawGpsMenu(const HermesXFastSetupListContext &context,
                                              uint32_t updateSeconds,
                                              const char *updateLabel,
                                              uint32_t broadcastSeconds,
                                              const char *broadcastLabel,
                                              bool smartEnabled,
                                              const char *smartDistanceLabel,
                                              const char *smartIntervalLabel)
{
    prepareList(context);
    const String updateValue = updateLabel ? String(updateLabel) : String(updateSeconds) + u8"秒";
    const String broadcastValue = broadcastLabel ? String(broadcastLabel) : String(broadcastSeconds) + u8"秒";
    String updateLine = String(u8"衛星更新: ") + updateValue;
    String broadcastLine = String(u8"廣播時間: ") + broadcastValue;
    String smartLine = String(u8"智慧位置: ") + (smartEnabled ? u8"開" : u8"關");
    String distanceLine = String(u8"最小距離: ") + smartDistanceLabel;
    String intervalLine = String(u8"最小間隔: ") + smartIntervalLabel;
    const char *items[] = {u8"返回", updateLine.c_str(), broadcastLine.c_str(), smartLine.c_str(),
                           distanceLine.c_str(), intervalLine.c_str()};
    drawList(context.display, context.width, context.height, u8"GPS", items, 6, context.selectedIndex,
             context.listOffset);
    finishList(context);
}

void HermesXFastSetupUiRenderer::drawGroupPins(const HermesXFastSetupListContext &context,
                                                const char *pinA,
                                                const char *pinB)
{
    prepareList(context);
    OLEDDisplay *display = context.display;
    if (!display) {
        return;
    }
    display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
    display->fillRect(0, 0, context.width, context.height);
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
    drawHeader(display, context.width, "GROUP PIN");
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
    display->setFont(FONT_SMALL);
    const String lineA = String("PIN A: ") + ((pinA && pinA[0]) ? pinA : u8"未設定");
    const String lineB = String("PIN B: ") + ((pinB && pinB[0]) ? pinB : u8"未設定");
    HermesX_zh::drawMixedBounded(*display, 2, HeaderHeight + 2, context.width - 4, lineA.c_str(),
                                 HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
    HermesX_zh::drawMixedBounded(*display, 2, HeaderHeight + 2 + FONT_HEIGHT_SMALL + 2, context.width - 4,
                                 lineB.c_str(), HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
    HermesX_zh::drawMixedBounded(*display, 2, context.height - FONT_HEIGHT_SMALL, context.width - 4, u8"按返回離開",
                                 HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
}

void HermesXFastSetupUiRenderer::drawUiMenu(const HermesXFastSetupListContext &context,
                                             bool buzzerEnabled,
                                             const char *brightnessLabel,
                                             bool ambientEnabled,
                                             const char *screenSleepLabel,
                                             const char *timezoneLabel,
                                             bool rotarySwapped,
                                             bool messagePopupEnabled)
{
    String buzzerLine = String(u8"全域蜂鳴器: ") + (buzzerEnabled ? u8"開" : u8"關");
    String brightnessLine = String(u8"Hermes狀態條: ") + brightnessLabel;
    String ambientLine = String(u8"板載RGB燈: ") + (ambientEnabled ? u8"開" : u8"關");
    String sleepLine = String(u8"螢幕休眠: ") + screenSleepLabel;
    String timezoneLine = String(u8"時區: ") + timezoneLabel;
    String rotaryLine = String(u8"旋鈕對調: ") + (rotarySwapped ? u8"開" : u8"關");
    String popupLine = String(u8"新訊息提示: ") + (messagePopupEnabled ? u8"開" : u8"關");
    const char *items[] = {u8"返回", buzzerLine.c_str(), brightnessLine.c_str(), ambientLine.c_str(),
                           sleepLine.c_str(), timezoneLine.c_str(), rotaryLine.c_str(), popupLine.c_str()};
    drawMenu(context, u8"UI設定", items, 8);
}

void HermesXFastSetupUiRenderer::drawEmInfoMenu(const HermesXFastSetupListContext &context,
                                                 bool broadcastEnabled,
                                                 const char *emInfoIntervalLabel,
                                                 const char *heartbeatIntervalLabel,
                                                 int offlineThreshold,
                                                 bool batteryIncluded)
{
    String broadcastLine = String(u8"EMINFO廣播: ") + (broadcastEnabled ? u8"開" : u8"關");
    String emInfoLine = String(u8"EMINFO週期: ") + emInfoIntervalLabel;
    String heartbeatLine = String(u8"Heartbeat週期: ") + heartbeatIntervalLabel;
    String offlineLine = String(u8"離線門檻: ") + String(offlineThreshold) + "x";
    String batteryLine = String(u8"附帶電量: ") + (batteryIncluded ? u8"開" : u8"關");
    const char *items[] = {u8"返回", broadcastLine.c_str(), emInfoLine.c_str(), heartbeatLine.c_str(),
                           offlineLine.c_str(), batteryLine.c_str()};
    drawMenu(context, u8"EMINFO設定", items, 6);
}

void HermesXFastSetupUiRenderer::drawCannedMenu(const HermesXFastSetupListContext &context, const char *channelName)
{
    String channelLine = String(u8"目標頻道: ") + ((channelName && channelName[0]) ? channelName : u8"未知");
    const char *items[] = {u8"返回", channelLine.c_str()};
    drawMenu(context, u8"罐頭訊息", items, 2);
}

void HermesXFastSetupUiRenderer::drawNodeMenu(const HermesXFastSetupListContext &context,
                                              bool mqttEnabled,
                                              bool bluetoothEnabled)
{
    String mqttLine = String("MQTT: ") + (mqttEnabled ? u8"開" : u8"關");
    String bluetoothLine = String(u8"藍牙: ") + (bluetoothEnabled ? u8"開" : u8"關");
    const char *items[] = {u8"返回",     u8"裝置資訊", "LoRa",       u8"GPS",      mqttLine.c_str(),
                           u8"頻道設定", bluetoothLine.c_str(), u8"電源管理", u8"節點資料庫", u8"更新模式"};
    drawMenu(context, u8"裝置管理", items, 10);
}

void HermesXFastSetupUiRenderer::drawDeviceInfoMenu(const HermesXFastSetupListContext &context,
                                                    const char *shortName,
                                                    const char *longName,
                                                    const char *nodeId,
                                                    const char *broadcastLabel)
{
    String shortLine = String(u8"裝置ID: ") + ((shortName && shortName[0]) ? shortName : u8"未設定");
    String longLine = String(u8"裝置名稱: ") + ((longName && longName[0]) ? longName : u8"未設定");
    String nodeIdLine = String("NodeID: ") + nodeId;
    String broadcastLine = String(u8"廣播時間: ") + broadcastLabel;
    const char *items[] = {u8"返回", shortLine.c_str(), longLine.c_str(), nodeIdLine.c_str(), broadcastLine.c_str(),
                           u8"立即廣播"};
    drawMenu(context, u8"裝置資訊", items, 6);
}

void HermesXFastSetupUiRenderer::drawUpdateMenu(const HermesXFastSetupListContext &context,
                                                bool dedicatedUpdateBoot,
                                                const char *currentVersion)
{
    if (!dedicatedUpdateBoot) {
        const char *items[] = {u8"返回", u8"開始更新"};
        drawMenu(context, u8"更新模式", items, 2);
        return;
    }

    String currentLine = String(u8"目前版本: ") + currentVersion;
    const char *items[] = {u8"退出更新模式", u8"WiFi設定", currentLine.c_str(), u8"檢查更新", u8"手動更新"};
    drawMenu(context, u8"更新模式", items, 5);
}

void HermesXFastSetupUiRenderer::drawUpdateCheckMenu(const HermesXFastSetupListContext &context,
                                                     const char *currentVersion,
                                                     const char *remoteUrl,
                                                     const char *sourceStatus,
                                                     const char *candidateVersion,
                                                     int progressPercent,
                                                     const char *lastError,
                                                     bool hasCandidate,
                                                     bool canApply)
{
    String remoteUrlPreview = (remoteUrl && remoteUrl[0]) ? String(remoteUrl) : String(u8"未設定");
    if (remoteUrlPreview.length() > 10) {
        remoteUrlPreview = remoteUrlPreview.substring(0, 10);
    }
    String currentLine = String(u8"目前版本: ") + currentVersion;
    String urlLine = String(u8"更新來源: ") + remoteUrlPreview;
    String actionLine = canApply ? String(u8"套用更新")
                                 : (hasCandidate ? String(u8"開始下載") : String(u8"開始檢查"));
    String statusLine = String(u8"目前狀態: ") + sourceStatus;
    String targetLine = String(u8"待更新版本: ") +
                        ((candidateVersion && candidateVersion[0]) ? candidateVersion : u8"無");
    String progressLine = String(u8"下載進度: ") + String(progressPercent) + "%";
    String errorLine = String(u8"最後錯誤: ") + ((lastError && lastError[0]) ? lastError : u8"無");
    const char *items[] = {u8"返回", currentLine.c_str(), urlLine.c_str(), actionLine.c_str(), statusLine.c_str(),
                           targetLine.c_str(), progressLine.c_str(), errorLine.c_str()};
    drawMenu(context, u8"檢查更新", items, 8);
}

void HermesXFastSetupUiRenderer::drawUpdateRuntimeMenu(const HermesXFastSetupListContext &context)
{
    const char *items[] = {u8"返回", u8"WiFi更新", u8"USB更新"};
    drawMenu(context, u8"手動更新", items, 3);
}

void HermesXFastSetupUiRenderer::drawUpdateWifiConfigMenu(const HermesXFastSetupListContext &context,
                                                          bool enabled,
                                                          const char *ssid,
                                                          const char *maskedPassword,
                                                          bool dirty,
                                                          const char *ipLabel)
{
    String enabledLine = String("WiFi: ") + (enabled ? u8"開" : u8"關");
    String ssidLine = String("SSID: ") + ((ssid && ssid[0]) ? ssid : u8"未設定");
    String passwordLine = String(u8"密碼: ") + maskedPassword;
    String saveLine = String(u8"儲存設定: ") + (dirty ? u8"未套用" : u8"已同步");
    String ipLine = String("IP: ") + ipLabel;
    const char *items[] = {u8"返回", enabledLine.c_str(), ssidLine.c_str(), passwordLine.c_str(), saveLine.c_str(),
                           ipLine.c_str()};
    drawMenu(context, u8"WiFi設定", items, 6);
}

void HermesXFastSetupUiRenderer::drawUpdateTransportMenu(const HermesXFastSetupListContext &context,
                                                         bool wifiTransport,
                                                         const char *currentVersion,
                                                         const char *connectionStatus)
{
    String currentLine = String(u8"目前版本: ") + currentVersion;
    String statusLine = String(wifiTransport ? u8"連線狀態: " : u8"USB連線狀態: ") + connectionStatus;
    const char *items[] = {u8"返回", currentLine.c_str(), statusLine.c_str(), u8"開始更新"};
    drawMenu(context, wifiTransport ? u8"WiFi更新" : u8"USB更新", items, 4);
}

static void drawSetupDonutProgress(OLEDDisplay *display, int16_t cx, int16_t cy, int16_t radius, int progressPercent)
{
    if (!display || radius < 8) {
        return;
    }

    const int boundedPercent = std::max(0, std::min(100, progressPercent));
    const int16_t arcR = std::max<int16_t>(2, radius / 8);
    const uint32_t now = millis();
    const int spinnerHead = static_cast<int>((now / 9U) % 360U);
    const int pulse = static_cast<int>((now / 140U) % 4U);

    display->drawCircle(cx, cy, radius);
    if (radius > 3) {
        display->drawCircle(cx, cy, radius - 3);
    }

    int arcStart = -90;
    int arcEnd = -90 + (360 * boundedPercent) / 100;
    if (boundedPercent <= 0) {
        arcStart = spinnerHead - 58;
        arcEnd = spinnerHead;
    } else if (boundedPercent >= 100) {
        arcStart = -90;
        arcEnd = 270;
    }

    for (int deg = arcStart; deg <= arcEnd; deg += 4) {
        const float angle = deg * PI / 180.0f;
        const int16_t x = cx + static_cast<int16_t>(lrintf(cosf(angle) * radius));
        const int16_t y = cy + static_cast<int16_t>(lrintf(sinf(angle) * radius));
        display->fillCircle(x, y, arcR);
    }

    if (arcEnd >= arcStart) {
        const float startAngle = arcStart * PI / 180.0f;
        const float endAngle = arcEnd * PI / 180.0f;
        display->fillCircle(cx + static_cast<int16_t>(lrintf(cosf(startAngle) * radius)),
                            cy + static_cast<int16_t>(lrintf(sinf(startAngle) * radius)), arcR);
        display->fillCircle(cx + static_cast<int16_t>(lrintf(cosf(endAngle) * radius)),
                            cy + static_cast<int16_t>(lrintf(sinf(endAngle) * radius)), arcR + (pulse == 0 ? 1 : 0));
    }

    const int shineDeg = (boundedPercent <= 0) ? spinnerHead - 24 : arcEnd - 18;
    const float shineA = shineDeg * PI / 180.0f;
    display->fillCircle(cx + static_cast<int16_t>(lrintf(cosf(shineA) * std::max<int16_t>(3, radius - arcR - 2))),
                        cy + static_cast<int16_t>(lrintf(sinf(shineA) * std::max<int16_t>(3, radius - arcR - 2))), 1);

    char percentText[8];
    snprintf(percentText, sizeof(percentText), "%d%%", boundedPercent);
    display->setFont(FONT_SMALL);
    const int16_t textW = display->getStringWidth(percentText);
    display->drawString(cx - textW / 2, cy - FONT_HEIGHT_SMALL / 2, percentText);
}

void HermesXFastSetupUiRenderer::drawUpdateProgressPage(OLEDDisplay *display,
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
                                        uint32_t &toastUntilMs)
{
    if (!display) {
        return;
    }

    display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
    display->fillRect(0, 0, width, height);
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif

    drawHeader(display, width, title);
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
    display->setFont(FONT_SMALL);

    const int16_t bodyTop = HeaderHeight + 3;
    const int16_t lineH = FONT_HEIGHT_SMALL + 2;
    const bool compactLayout = height <= 80;
    const int16_t footerH = 14;
    const int16_t footerY = height - footerH - 2;
    const int16_t gap = 4;
    const int16_t actionW = (width - 12 - gap * (actionCount - 1)) / actionCount;
    const int16_t actionH = footerH;
    const bool showToastHere = !compactLayout;

    int16_t contentY = bodyTop;
    int16_t metaBottom = footerY - 2;

    auto drawWrappedCapped = [&](const String &text, int16_t x, int16_t y, int16_t maxWidth, int maxLines) -> int16_t {
        if (text.isEmpty() || maxWidth <= 0 || maxLines <= 0) {
            return 0;
        }
        const std::vector<String> wrappedLines =
            HermesXTextLayout::buildMixedWrappedLines(display, text.c_str(), maxWidth, graphics::HermesX_zh::GLYPH_WIDTH);
        const int16_t linesToDraw = std::min<int16_t>(static_cast<int16_t>(wrappedLines.size()), maxLines);
        for (int16_t i = 0; i < linesToDraw; ++i) {
            graphics::HermesX_zh::drawMixedBounded(*display, x, y + lineH * i, maxWidth, wrappedLines[i].c_str(),
                                                   graphics::HermesX_zh::GLYPH_WIDTH, lineH, nullptr);
        }
        return linesToDraw;
    };

    auto drawSingleLine = [&](const String &text, int16_t x, int16_t y, int16_t maxWidth) {
        if (text.isEmpty() || maxWidth <= 0) {
            return;
        }
        const std::vector<String> wrappedLines =
            HermesXTextLayout::buildMixedWrappedLines(display, text.c_str(), maxWidth, graphics::HermesX_zh::GLYPH_WIDTH);
        if (!wrappedLines.empty()) {
            graphics::HermesX_zh::drawMixedBounded(*display, x, y, maxWidth, wrappedLines.front().c_str(),
                                                   graphics::HermesX_zh::GLYPH_WIDTH, lineH, nullptr);
        }
    };

    const int boundedPercent = std::max(0, std::min(100, progressPercent));

    if (compactLayout) {
        const String statusText = instruction.isEmpty() ? String(u8"待命中") : instruction;
        String infoText;
        if (errorLine.length() > 0) {
            infoText = errorLine;
        } else if (canApply && targetVersion.length() > 0) {
            infoText = targetVersion;
        } else if (statusLine.length() > 0) {
            infoText = statusLine;
        } else if (targetVersion.length() > 0) {
            infoText = targetVersion;
        } else {
            infoText = u8"待更新版本: 無";
        }

        const int16_t donutRadius = std::min<int16_t>(16, std::max<int16_t>(12, (footerY - bodyTop - 4) / 2));
        const int16_t donutX = 4 + donutRadius;
        const int16_t donutY = bodyTop + 2 + donutRadius;
        drawSetupDonutProgress(display, donutX, donutY, donutRadius, boundedPercent);

        const int16_t textX = donutX + donutRadius + 8;
        const int16_t textW = width - textX - 4;
        const int16_t statusY = bodyTop + 2;
        const int16_t infoY = statusY + lineH;
        drawSingleLine(statusText, textX, statusY, textW);
        drawSingleLine(infoText, textX, infoY, textW);

        for (int i = 0; i < actionCount; ++i) {
            const bool selected = (selectedIndex == i);
            const int16_t actionX = 6 + i * (actionW + gap);
            const char *label = (i == 0) ? primaryLabel : secondaryLabel;

            if (selected) {
#if defined(USE_EINK)
                display->fillRect(actionX, footerY, actionW, actionH);
                display->setColor(EINK_WHITE);
#else
                display->setColor(OLEDDISPLAY_COLOR::WHITE);
                display->fillRect(actionX, footerY, actionW, actionH);
                display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
            } else {
                display->drawRect(actionX, footerY, actionW, actionH);
            }

            const int16_t textW = HermesX_zh::stringAdvance(label, HermesX_zh::GLYPH_WIDTH, display);
            const int16_t textX = actionX + std::max<int16_t>(2, (actionW - textW) / 2);
            const int16_t textY = footerY + ((actionH - FONT_HEIGHT_SMALL) / 2);
            graphics::HermesX_zh::drawMixedBounded(*display, textX, textY, actionW - 4, label,
                                                   graphics::HermesX_zh::GLYPH_WIDTH, lineH, nullptr);

            if (selected) {
#if defined(USE_EINK)
                display->setColor(EINK_BLACK);
#else
                display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
            }
        }
        return;
    } else {
        const int16_t drawnInstructionLines = std::max<int16_t>(1, drawWrappedCapped(instruction, 4, contentY, width - 8, 2));
        contentY += lineH * drawnInstructionLines;
    }

    const int16_t availableForDonut = std::max<int16_t>(24, footerY - contentY - 26);
    const int16_t donutRadius = std::min<int16_t>(24, std::max<int16_t>(14, availableForDonut / 2));
    const int16_t donutY = contentY + 3 + donutRadius;
    drawSetupDonutProgress(display, width / 2, donutY, donutRadius, boundedPercent);

    const int16_t metaY = donutY + donutRadius + 3;

    String percentLine = String(u8"進度 ") + String(boundedPercent) + "%";
    String detailLine;
    if (!compactLayout) {
        if (errorLine.length() > 0) {
            detailLine = errorLine;
        } else if (canApply && targetVersion.length() > 0) {
            detailLine = targetVersion;
        } else {
            detailLine = statusLine;
        }
    } else if (canApply && targetVersion.length() > 0) {
        detailLine = targetVersion;
    } else if (errorLine.length() > 0) {
        detailLine = errorLine;
    }

    const int16_t maxMetaLines = std::max<int16_t>(0, (metaBottom - metaY) / lineH);
    int16_t drawnMetaLines = 0;
    const String metaValues[3] = {percentLine, detailLine,
                                  (!compactLayout && !canApply && targetVersion.length() > 0 && detailLine != targetVersion)
                                      ? targetVersion
                                      : String()};
    for (int16_t i = 0; i < 3 && drawnMetaLines < maxMetaLines; ++i) {
        if (metaValues[i].isEmpty()) {
            continue;
        }
        const int16_t remainingMetaLines = maxMetaLines - drawnMetaLines;
        const int16_t justDrawn = drawWrappedCapped(metaValues[i], 4, metaY + lineH * drawnMetaLines, width - 8, remainingMetaLines);
        drawnMetaLines += justDrawn;
    }

    for (int i = 0; i < actionCount; ++i) {
        const bool selected = (selectedIndex == i);
        const int16_t actionX = 6 + i * (actionW + gap);
        const char *label = (i == 0) ? primaryLabel : secondaryLabel;

        if (selected) {
#if defined(USE_EINK)
            display->fillRect(actionX, footerY, actionW, actionH);
            display->setColor(EINK_WHITE);
#else
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
            display->fillRect(actionX, footerY, actionW, actionH);
            display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
        } else {
            display->drawRect(actionX, footerY, actionW, actionH);
        }

        const int16_t textW = HermesX_zh::stringAdvance(label, HermesX_zh::GLYPH_WIDTH, display);
        const int16_t textX = actionX + std::max<int16_t>(2, (actionW - textW) / 2);
        const int16_t textY = footerY + ((actionH - FONT_HEIGHT_SMALL) / 2);
        graphics::HermesX_zh::drawMixedBounded(*display, textX, textY, actionW - 4, label, graphics::HermesX_zh::GLYPH_WIDTH,
                                               lineH, nullptr);

        if (selected) {
#if defined(USE_EINK)
            display->setColor(EINK_BLACK);
#else
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
        }
    }

    const bool toastWouldOverlap = toastUntilMs != 0 && (footerY - FONT_HEIGHT_SMALL - 1) <= metaBottom;
    if (showToastHere && !toastWouldOverlap) {
        drawToast(display, width, height, toast, toastUntilMs);
    }
}

void HermesXFastSetupUiRenderer::drawUpdateTransitionPage(OLEDDisplay *display,
                                          int16_t width,
                                          int16_t height,
                                          uint32_t startedAtMs,
                                          const char *title,
                                          const char *status)
{
    if (!display) {
        return;
    }

    const uint32_t elapsed = startedAtMs == 0 ? 0 : millis() - startedAtMs;
    const uint32_t phase = elapsed % UpdateTransitionMs;
    const uint32_t clampedElapsed = std::min<uint32_t>(elapsed, UpdateTransitionMs);
    const bool compactLayout = height <= 80;
    const int16_t lineH = FONT_HEIGHT_SMALL + 2;
    const int16_t barX = compactLayout ? 12 : 22;
    const int16_t barW = std::max<int16_t>(width - barX * 2, 32);
    const int16_t barH = compactLayout ? 10 : 12;
    const int16_t titleY = compactLayout ? 14 : 22;
    const bool hasStatus = status && status[0];
    const int16_t statusY = titleY + (compactLayout ? 18 : 24);
    const int16_t barY = statusY + lineH + (compactLayout ? 4 : 8);

    display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
    display->fillRect(0, 0, width, height);
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif

    display->setFont(FONT_SMALL);
    auto drawCenteredMixed = [&](const char *text, int16_t y) {
        if (!text || !text[0]) {
            return;
        }
        const int16_t textW = HermesX_zh::stringAdvance(text, HermesX_zh::GLYPH_WIDTH, display);
        const int16_t textX = std::max<int16_t>(2, (width - textW) / 2);
        const int16_t maxW = std::max<int16_t>(1, width - textX * 2);
        HermesX_zh::drawMixedBounded(*display, textX, y, maxW, text, HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);
    };
    drawCenteredMixed(title, hasStatus ? titleY : titleY + (compactLayout ? 7 : 10));
    drawCenteredMixed(status, statusY);

    display->drawRect(barX, barY, barW, barH);
    const int16_t innerW = barW - 2;
    const int16_t fillW = static_cast<int16_t>((static_cast<uint32_t>(innerW) * clampedElapsed) / UpdateTransitionMs);
    if (fillW > 0) {
        display->fillRect(barX + 1, barY + 1, fillW, barH - 2);
    }

    const int16_t scanW = std::max<int16_t>(8, barW / 5);
    const int16_t travelW = std::max<int16_t>(1, innerW - scanW);
    const int16_t scanX = barX + 1 + static_cast<int16_t>((static_cast<uint32_t>(travelW) * phase) / UpdateTransitionMs);
    display->fillRect(scanX, barY + 1, scanW, barH - 2);

    const int16_t edgeY = height - 5;
    display->drawLine(4, 4, 18, 4);
    display->drawLine(4, 4, 4, 12);
    display->drawLine(width - 19, edgeY, width - 5, edgeY);
    display->drawLine(width - 5, edgeY - 8, width - 5, edgeY);
}

void HermesXFastSetupUiRenderer::drawUpdateIntroPage(OLEDDisplay *display, int16_t width, int16_t height, uint32_t startedAtMs)
{
    drawUpdateTransitionPage(display, width, height, startedAtMs, u8"進入更新模式", "");
}

void HermesXFastSetupUiRenderer::drawUrlUpdateFlowPage(OLEDDisplay *display,
                                       int16_t width,
                                       int16_t height,
                                       const String &statusValue,
                                       const String &candidateVersion,
                                       int progressPercent,
                                       const String &errorValue,
                                       const char *actionLabel,
                                       int selectedIndex,
                                       String &toast,
                                       uint32_t &toastUntilMs)
{
    if (!display) {
        return;
    }

    display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
    display->fillRect(0, 0, width, height);
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif

    drawHeader(display, width, u8"URL更新");
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
    display->setFont(FONT_SMALL);

    const bool compactLayout = height <= 80;
    const int16_t lineH = FONT_HEIGHT_SMALL + 2;
    const int16_t bodyTop = HeaderHeight + 3;
    const int16_t barX = 6;
    const int16_t barW = width - 12;
    const int16_t barH = 10;
    const int16_t footerH = 14;
    const int16_t footerY = height - footerH - 2;
    const int16_t gap = 4;
    const int16_t actionCount = 2;
    const int16_t actionW = (width - 12 - gap) / 2;
    const int16_t contentW = width - 8;

    auto drawWrappedCapped = [&](const String &text, int16_t x, int16_t y, int16_t maxWidth, int maxLines) -> int16_t {
        if (text.isEmpty() || maxWidth <= 0 || maxLines <= 0) {
            return 0;
        }
        const std::vector<String> wrappedLines =
            HermesXTextLayout::buildMixedWrappedLines(display, text.c_str(), maxWidth, graphics::HermesX_zh::GLYPH_WIDTH);
        const int16_t linesToDraw = std::min<int16_t>(static_cast<int16_t>(wrappedLines.size()), maxLines);
        for (int16_t i = 0; i < linesToDraw; ++i) {
            graphics::HermesX_zh::drawMixedBounded(*display, x, y + lineH * i, maxWidth, wrappedLines[i].c_str(),
                                                   graphics::HermesX_zh::GLYPH_WIDTH, lineH, nullptr);
        }
        return linesToDraw;
    };

    auto drawSingleLine = [&](const String &text, int16_t x, int16_t y, int16_t maxWidth) {
        if (text.isEmpty() || maxWidth <= 0) {
            return;
        }
        const std::vector<String> wrappedLines =
            HermesXTextLayout::buildMixedWrappedLines(display, text.c_str(), maxWidth, graphics::HermesX_zh::GLYPH_WIDTH);
        if (!wrappedLines.empty()) {
            graphics::HermesX_zh::drawMixedBounded(*display, x, y, maxWidth, wrappedLines.front().c_str(),
                                                   graphics::HermesX_zh::GLYPH_WIDTH, lineH, nullptr);
        }
    };

    if (compactLayout) {
        const String statusText = statusValue.isEmpty() ? String(u8"待命中") : statusValue;
        String infoText;
        if (!errorValue.isEmpty()) {
            infoText = String(u8"錯誤: ") + errorValue;
        } else if (!candidateVersion.isEmpty()) {
            infoText = String(u8"版本: ") + candidateVersion;
        } else {
            infoText = u8"版本: 無";
        }

        const int16_t statusY = bodyTop;
        const int16_t infoY = statusY + lineH;
        const int16_t barY = infoY + lineH + 2;

        drawSingleLine(statusText, 4, statusY, contentW);
        drawSingleLine(infoText, 4, infoY, contentW);

        display->drawRect(barX, barY, barW, barH);
        const int boundedPercent = std::max(0, std::min(100, progressPercent));
        const int16_t innerW = barW - 2;
        const int16_t fillW = (innerW * boundedPercent) / 100;
        if (fillW > 0) {
            display->fillRect(barX + 1, barY + 1, fillW, barH - 2);
        }

        for (int i = 0; i < actionCount; ++i) {
            const bool selected = (selectedIndex == i);
            const int16_t actionX = 6 + i * (actionW + gap);
            const char *label = (i == 0) ? u8"返回" : actionLabel;

            if (selected) {
#if defined(USE_EINK)
                display->fillRect(actionX, footerY, actionW, footerH);
                display->setColor(EINK_WHITE);
#else
                display->setColor(OLEDDISPLAY_COLOR::WHITE);
                display->fillRect(actionX, footerY, actionW, footerH);
                display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
            } else {
                display->drawRect(actionX, footerY, actionW, footerH);
            }

            const int16_t textW = HermesX_zh::stringAdvance(label, HermesX_zh::GLYPH_WIDTH, display);
            const int16_t textX = actionX + std::max<int16_t>(2, (actionW - textW) / 2);
            const int16_t textY = footerY + ((footerH - FONT_HEIGHT_SMALL) / 2);
            graphics::HermesX_zh::drawMixedBounded(*display, textX, textY, actionW - 4, label,
                                                   graphics::HermesX_zh::GLYPH_WIDTH, lineH, nullptr);

            if (selected) {
#if defined(USE_EINK)
                display->setColor(EINK_BLACK);
#else
                display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
            }
        }
        return;
    }

    int16_t y = bodyTop;
    graphics::HermesX_zh::drawMixedBounded(*display, 4, y, contentW, u8"目前狀態", graphics::HermesX_zh::GLYPH_WIDTH, lineH, nullptr);
    y += lineH;
    const int maxStatusLines = compactLayout ? 2 : 3;
    const int16_t statusLines =
        std::max<int16_t>(1, drawWrappedCapped(statusValue.isEmpty() ? String(u8"待命中") : statusValue, 4, y, contentW, maxStatusLines));
    y += statusLines * lineH + 2;

    const int16_t barY = y;
    display->drawRect(barX, barY, barW, barH);
    const int boundedPercent = std::max(0, std::min(100, progressPercent));
    const int16_t innerW = barW - 2;
    const int16_t fillW = (innerW * boundedPercent) / 100;
    if (fillW > 0) {
        display->fillRect(barX + 1, barY + 1, fillW, barH - 2);
    }
    y = barY + barH + 2;

    const String progressLine = String(u8"下載進度: ") + String(boundedPercent) + "%";
    graphics::HermesX_zh::drawMixedBounded(*display, 4, y, contentW, progressLine.c_str(), graphics::HermesX_zh::GLYPH_WIDTH, lineH,
                                           nullptr);
    y += lineH;

    String bottomInfo;
    if (!errorValue.isEmpty()) {
        bottomInfo = String(u8"最後錯誤: ") + errorValue;
    } else if (!candidateVersion.isEmpty()) {
        bottomInfo = String(u8"待更新版本: ") + candidateVersion;
    } else {
        bottomInfo = u8"待更新版本: 無";
    }
    const int16_t availableInfoLines = std::max<int16_t>(0, (footerY - 2 - y) / lineH);
    drawWrappedCapped(bottomInfo, 4, y, contentW, compactLayout ? std::min<int16_t>(2, availableInfoLines)
                                                                 : std::min<int16_t>(3, availableInfoLines));

    for (int i = 0; i < actionCount; ++i) {
        const bool selected = (selectedIndex == i);
        const int16_t actionX = 6 + i * (actionW + gap);
        const char *label = (i == 0) ? u8"返回" : actionLabel;

        if (selected) {
#if defined(USE_EINK)
            display->fillRect(actionX, footerY, actionW, footerH);
            display->setColor(EINK_WHITE);
#else
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
            display->fillRect(actionX, footerY, actionW, footerH);
            display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
        } else {
            display->drawRect(actionX, footerY, actionW, footerH);
        }

        const int16_t textW = HermesX_zh::stringAdvance(label, HermesX_zh::GLYPH_WIDTH, display);
        const int16_t textX = actionX + std::max<int16_t>(2, (actionW - textW) / 2);
        const int16_t textY = footerY + ((footerH - FONT_HEIGHT_SMALL) / 2);
        graphics::HermesX_zh::drawMixedBounded(*display, textX, textY, actionW - 4, label, graphics::HermesX_zh::GLYPH_WIDTH,
                                               lineH, nullptr);

        if (selected) {
#if defined(USE_EINK)
            display->setColor(EINK_BLACK);
#else
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
        }
    }

    drawToast(display, width, height, toast, toastUntilMs);
}


void HermesXFastSetupUiRenderer::drawKeyboardPage(OLEDDisplay *display,
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
                                  uint32_t &toastUntilMs)
{
    if (!display) {
        return;
    }

    display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
    display->fillRect(0, 0, width, height);
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
    drawHeader(display, width, header);
    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
    graphics::HermesX_zh::drawMixedBounded(*display, 2, HeaderHeight + 2, width - 4, draft.c_str(),
                                           graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);

    const int16_t keyTop = (FONT_HEIGHT_SMALL * 2) + 6;
    const int16_t keyAreaHeight = height - keyTop;
    const int16_t rowHeight = (rowCount > 0) ? keyAreaHeight / rowCount : 0;
    display->setTextAlignment(TEXT_ALIGN_CENTER);

    for (uint8_t row = 0; row < rowCount; ++row) {
        const int rowLen = rowLengths[row];
        if (rowLen <= 0) {
            continue;
        }
        const int16_t cellWidth = width / rowLen;
        const int16_t rowY = keyTop + row * rowHeight;
        for (int col = 0; col < rowLen; ++col) {
            const int16_t cellX = col * cellWidth;
            const char *label = rows[row][col];
            if (!label) {
                continue;
            }
            const bool selected = (row == selectedRow && col == selectedCol);
            if (selected) {
#if defined(USE_EINK)
                display->setColor(EINK_BLACK);
#else
                display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
                display->fillRect(cellX, rowY, cellWidth, rowHeight);
#if defined(USE_EINK)
                display->setColor(EINK_WHITE);
#else
                display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
            } else {
#if defined(USE_EINK)
                display->setColor(EINK_BLACK);
#else
                display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
            }
            display->drawRect(cellX, rowY, cellWidth, rowHeight);
            const int16_t labelWidth = graphics::HermesX_zh::stringAdvance(
                label, graphics::HermesX_zh::GLYPH_WIDTH, display);
            const bool hasUtf8 = strlen(label) > 0 && static_cast<uint8_t>(label[0]) >= 0x80u;
            const int16_t labelHeight = hasUtf8 ? graphics::HermesX_zh::GLYPH_HEIGHT : FONT_HEIGHT_SMALL;
            const int16_t labelX = cellX + std::max<int16_t>(0, (cellWidth - labelWidth) / 2);
            const int16_t labelY = rowY + std::max<int16_t>(0, (rowHeight - labelHeight) / 2);
            graphics::HermesX_zh::drawMixed(*display, labelX, labelY, label,
                                            graphics::HermesX_zh::GLYPH_WIDTH, rowHeight, nullptr);
        }
    }

    display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
    drawToast(display, width, height, toast, toastUntilMs);
}

void HermesXFastSetupUiRenderer::drawDetailPopup(OLEDDisplay *display,
                                                     const String &title,
                                                     const String &body,
                                                     uint16_t &scrollY,
                                                     uint16_t &maxScrollY)
{
    if (!display) {
        return;
    }

    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    const bool compactLayout = (width < 200 || height < 120);
    const int16_t boxX = 0;
    const int16_t boxY = 0;
    const int16_t boxW = width;
    const int16_t boxH = height;
    const int16_t titlePadX = compactLayout ? 4 : 6;
    const int16_t titlePadY = compactLayout ? 3 : 4;
    const int16_t titleH = compactLayout ? 16 : 18;
    const int16_t footerH = compactLayout ? 14 : 18;
    const int16_t dividerY = boxY + titleH + titlePadY * 2;
    const int16_t footerY = boxY + boxH - footerH;
    const int16_t bodyX = boxX + titlePadX;
    const int16_t bodyY = dividerY + 3;
    const int16_t scrollbarW = 4;
    const int16_t bodyW = std::max<int16_t>(boxW - titlePadX * 2 - scrollbarW - 2, 20);
    const int16_t bodyH = std::max<int16_t>(footerY - bodyY - 2, titleH);
    const int16_t lineHeight = compactLayout ? 12 : 14;

#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(BLACK);
#endif
    display->fillRect(boxX, boxY, boxW, boxH);
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(WHITE);
#endif

    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);
    HermesX_zh::drawMixedBounded(*display, bodyX, boxY + titlePadY, boxW - titlePadX * 2, title.c_str(),
                                 HermesX_zh::GLYPH_WIDTH, titleH, nullptr);
    display->drawLine(boxX, dividerY, boxX + boxW - 1, dividerY);
    display->drawLine(boxX, footerY - 1, boxX + boxW - 1, footerY - 1);

    const uint16_t contentHeight =
        HermesXTextLayout::measureMixedWrappedTextHeight(display, body.c_str(), bodyW, lineHeight);
    maxScrollY = (contentHeight > bodyH) ? (contentHeight - bodyH) : 0;
    if (scrollY > maxScrollY) {
        scrollY = maxScrollY;
    }

    const std::vector<String> wrappedLines =
        HermesXTextLayout::buildMixedWrappedLines(display, body.c_str(), bodyW, HermesX_zh::GLYPH_WIDTH);
    HermesXTextLayout::drawVisibleWrappedLines(display, wrappedLines, bodyX, bodyY, bodyW, bodyH, lineHeight, scrollY,
                            HermesX_zh::GLYPH_WIDTH);

    if (maxScrollY > 0) {
        const int16_t trackX = boxX + boxW - scrollbarW;
        display->drawRect(trackX, bodyY, scrollbarW, bodyH);
        const int16_t thumbH =
            std::max<int16_t>(6, static_cast<int16_t>((static_cast<int32_t>(bodyH) * bodyH) / contentHeight));
        const int16_t thumbTravel = std::max<int16_t>(bodyH - thumbH - 2, 0);
        const int16_t thumbY =
            bodyY + 1 + static_cast<int16_t>((static_cast<int32_t>(thumbTravel) * scrollY) /
                                             maxScrollY);
        display->fillRect(trackX + 1, thumbY, scrollbarW - 2, thumbH);
    }

    const int16_t exitTextY = footerY + std::max<int16_t>(0, (footerH - FONT_HEIGHT_SMALL) / 2);
    const char *exitLabel = u8"退出";
    const int16_t exitW = display->getStringWidth(exitLabel);
    const int16_t exitX = boxX + std::max<int16_t>(2, (boxW - exitW) / 2);
    display->drawRect(exitX - 4, footerY + 1, exitW + 8, footerH - 3);
    HermesX_zh::drawMixedBounded(*display, exitX, exitTextY, exitW + 2, exitLabel, HermesX_zh::GLYPH_WIDTH, lineHeight, nullptr);
}

} // namespace graphics
