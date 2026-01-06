#include "configuration.h"
#if HAS_SCREEN
#include "MeshService.h"
#include "RTC.h"
#include "draw/NodeListRenderer.h"
#include "graphics/ScreenFonts.h"
#include "graphics/SharedUIDisplay.h"
#include "graphics/draw/UIRenderer.h"
#include "graphics/fonts/HermesX_zh/HermesX_CN12.h"
#include "main.h"
#include "meshtastic/config.pb.h"
#include "power.h"
#include <OLEDDisplay.h>
#include <graphics/images.h>

namespace graphics
{

void determineResolution(int16_t screenheight, int16_t screenwidth)
{
    if (screenwidth > 128) {
        isHighResolution = true;
    }

    if (screenwidth > 128 && screenheight <= 64) {
        isHighResolution = false;
    }

    // Special case for Heltec Wireless Tracker v1.1
    if (screenwidth == 160 && screenheight == 80) {
        isHighResolution = false;
    }
}

// === Shared External State ===
bool hasUnreadMessage = false;
bool isMuted = false;
bool isHighResolution = false;
OLEDDISPLAY_TEXT_ALIGNMENT lastTextAlignment = TEXT_ALIGN_LEFT;

struct ZhEntry {
    const char *en;
    const char *zh;
};

static const ZhEntry zhTable[] = {
    {"Messages", "訊息"},                   {"Settings", "設定"},             {"Nodes", "節點"},
    {"Select Destination", "選擇目的地"},    {"Select Emote", "選擇表情"},     {"No messages", "沒有訊息"},
    {"Sending...", "傳送中..."},            {"Canned Message\nModule disabled.", "罐頭訊息模組已停用"},
    {"HermesX", "HermesX"},                 {"Alert", "警示"},                {"Confirm", "確認"},
    {"Cancel", "取消"},                     {"OK", "確定"},                   {"Please wait . . .", "請稍候..."},
    {"Updating", "更新中"},                  {"Back", "返回"},                 {"System", "系統"},
    {"System Action", "系統操作"},           {"Notifications", "通知"},        {"Display Options", "顯示選項"},
    {"Bluetooth", "藍牙"},                  {"Bluetooth Toggle", "藍牙開關"}, {"Reboot/Shutdown", "重啟/關機"},
    {"Reboot / Shutdown", "重啟 / 關機"},    {"Power", "電源"},                {"Test Menu", "測試選單"},
    {"Favorites", "常用"},                  {"Favorites Action", "常用節點操作"},
    {"New Preset Msg", "新增預設訊息"},      {"New Preset", "新增預設"},      {"New Freetext Msg", "新增自由文字"},
    {"New Freetext", "新增自由文字"},       {"Trace Route", "路由追蹤"},      {"Remove Favorite", "移除最愛"},
    {"Message Action", "訊息操作"},         {"Position Action", "定位操作"},   {"GPS Toggle", "GPS 開關"},
    {"GPS Format", "GPS 格式"},            {"Compass", "羅盤"},              {"Compass Calibrate", "羅盤校正"},
    {"North Directions?", "北向模式"},       {"Dynamic", "動態"},              {"Fixed Ring", "固定環"},
    {"Freeze Heading", "凍結方位"},         {"Toggle GPS", "切換 GPS"},       {"Enabled", "啟用"},
    {"Disabled", "停用"},                  {"Decimal Degrees", "十進位"},    {"Degrees Minutes Seconds", "度分秒"},
    {"Universal Transverse Mercator", "UTM"}, {"Military Grid Reference System", "MGRS"},
    {"Open Location Code", "OLC"},          {"Ordnance Survey Grid Ref", "OSGR"},
    {"Maidenhead Locator", "MLS"},          {"Node Action", "節點操作"},      {"Add Favorite", "加入最愛"},
    {"Key Verification", "鑰匙驗證"},       {"Reset NodeDB", "重置節點庫"},    {"Reset Node", "重置節點"},
    {"Node Name Length", "節點名稱長度"},   {"Long", "長名稱"},               {"Short", "短名稱"},
    {"Confirm Reset NodeDB", "確認重置節點庫"}, {"Reset All", "全部重置"},    {"Preserve Favorites", "保留最愛"},
    {"Node Favorite", "選擇最愛節點"},      {"Node To Favorite", "選擇要收藏的節點"},
    {"Unfavorite This Node?\n", "取消收藏此節點？\n"}, {"Node to Trace", "選擇路由追蹤節點"},
    {"Node to Verify", "選擇驗證節點"},     {"Last seen", "上次看到"},        {"Distance", "距離"},
    {"Hops", "跳數"},                       {"Clock", "時鐘"},                {"Compass", "羅盤"},
    {"Buzzer Mode", "蜂鳴模式"},             {"All Enabled", "全部啟用"},      {"System Only", "僅系統"},
    {"DMs Only", "僅私訊"},                 {"Brightness", "亮度"},           {"Low", "低"},
    {"Medium", "中"},                       {"High", "高"},                   {"Very High", "很高"},
    {"Switch to MUI?", "切換到 MUI？"},     {"No", "否"},                    {"Yes", "是"},
    {"Select Screen Color", "選擇螢幕顏色"}, {"Default", "預設"},             {"Meshtastic Green", "Meshtastic 綠"},
    {"Yellow", "黃"},                       {"Red", "紅"},                   {"Orange", "橘"},
    {"Purple", "紫"},                       {"Teal", "青綠"},                {"Pink", "粉紅"},
    {"White", "白"},                        {"Reboot", "重新啟動"},           {"Shutdown", "關機"},
    {"Reboot Device?", "確定重新啟動？"},     {"Shutdown Device?", "確定關機？"},
    {"Rebooting...", "重新啟動中..."},       {"WiFi Toggle", "WiFi 開關"},    {"WiFi Menu", "WiFi 選單"},
    {"Disable Wifi and\nEnable Bluetooth?", "關閉 WiFi 並啟用藍牙？"}, {"Disable", "停用"},
    {"Buzzer Actions", "蜂鳴設定"},         {"Show Long/Short Name", "顯示長/短名稱"},
    {"Screen Color", "螢幕顏色"},           {"Frame Visiblity Toggle", "顯示/隱藏畫面"},
    {"Display Units", "顯示單位"},          {"Show/Hide Frames", "顯示/隱藏框架"},
    {"Show Node List", "顯示節點列表"},      {"Hide Node List", "隱藏節點列表"},
    {"Show NL - Last Heard", "顯示 NL-最後聽到"}, {"Hide NL - Last Heard", "隱藏 NL-最後聽到"},
    {"Show NL - Hops/Signal", "顯示 NL-跳數/訊號"}, {"Hide NL - Hops/Signal", "隱藏 NL-跳數/訊號"},
    {"Show NL - Distance", "顯示 NL-距離"},   {"Hide NL - Distance", "隱藏 NL-距離"},
    {"Show Bearings", "顯示方位"},          {"Hide Bearings", "隱藏方位"},
    {"Show Position", "顯示位置"},          {"Hide Position", "隱藏位置"},
    {"Show LoRa", "顯示 LoRa"},             {"Hide LoRa", "隱藏 LoRa"},
    {"Show Clock", "顯示時鐘"},             {"Hide Clock", "隱藏時鐘"},
    {"Show Favorites", "顯示最愛"},         {"Hide Favorites", "隱藏最愛"},
    {"Show Telemetry", "顯示遙測"},         {"Hide Telemetry", "隱藏遙測"},
    {"Show Power", "顯示電源"},             {"Hide Power", "隱藏電源"},
    {"Finish", "完成"},                     {"Metric", "公制"},               {"Imperial", "英制"},
    {" Select display units", "選擇顯示單位"}, {"Hidden Test Menu", "隱藏測試選單"},
    {"Number Picker", "選擇數字"},          {"Show Chirpy", "顯示 Chirpy"},  {"Hide Chirpy", "隱藏 Chirpy"},
    {"Pick a number\n ", "挑選一個數字\n "},
    {"Home", "首頁"},                       {"Home Action", "首頁操作"},
    {"Sleep Screen", "關閉螢幕"},           {"Send Position", "傳送位置"},    {"Send Node Info", "傳送節點資訊"},
    {"LoRa Actions", "LoRa 操作"},          {"Clock Action", "時鐘設定"},     {"Which Face?", "選擇錶面"},
    {"Pick 時區", "選擇時區"},              {"Set the LoRa region", "設定 LoRa 區域"},
    {"Accept", "接受"},                     {"Reject", "拒絕"},              {"Dismiss", "關閉"},
    {"Reply Preset", "回覆預設"},            {"Reply via Preset", "以預設回覆"}, {"Reply via Freetext", "以自由文字回覆"},
    {"Read Aloud", "語音朗讀"},             {"Too Many Attempts\nTry again in 60 seconds.", "嘗試過多，60 秒後重試。"},
    {"WiFi Menu", "WiFi 選單"},             {"Frame Visiblity Toggle", "顯示/隱藏畫面"},
    {"Screen brightness set to %d", "螢幕亮度設為 %d"}, {"Home Action", "首頁操作"},
    {"No Lock", "失去衛星定位"},            {"No Sats", "失去衛星定位"},     {"No sats", "失去衛星定位"},
    {"No GPS", "未偵測到 GPS"},            {"GPS off", "GPS 關閉"},        {"GPS not present", "未偵測到 GPS"},
    {"GPS is disabled", "GPS 已停用"},      {"Fixed", "固定"},               {"Fixed GPS", "固定座標"},
    {"Altitude: %.0im", "高度: %.0im"},     {"Altitude: %.0fft", "高度: %.0fft"},
    {"Alt: %.0im", "高度: %.0im"},          {"Alt: %.0fft", "高度: %.0fft"},
    {"Up: %ud %uh", "上線: %ud 天 %uh 小時"}, {"Up: %uh %um", "上線: %uh 小時 %um 分"}, {"Up: %um", "上線: %um 分"},
    {"Uptime: %ud %uh", "上線: %ud 天 %uh 小時"}, {"Uptime: %uh %um", "上線: %uh 小時 %um 分"},
    {"Uptime: %um", "上線: %um 分"},        {"online", "在線"},
    {"Sig", "訊號"},                        {"%u sats", "%u 顆衛星"},
    {"[%d %s]", "[%d 跳]"},                 {"Hop", "跳"},                  {"Hops", "跳"},
    {"ChUtil:", "頻寬使用率:"},             {"Home", "首頁"},               {"LoRa Actions", "LoRa 操作"},
    {"Clock Action", "時鐘設定"},           {"Which Face?", "選擇錶面"},     {"Pick 時區", "選擇時區"},
    {"Set the LoRa region", "設定 LoRa 區域"}, {"New Preset Msg", "罐頭訊息"},
    {"Key Verification", "交換 PSK"},        {"Position", "位置"},
    {"Position Action", "定位操作"},        {"Send Position", "傳送位置"},   {"Send Node Info", "傳送節點資訊"},
    {"Verification: \\n", "交換 PSK：\\n"},
};

const char *translateZh(const char *text)
{
    if (!text)
        return "";
    // Trim leading/trailing whitespace/newlines for lookup
    std::string key(text);
    auto l = key.find_first_not_of(" \r\n\t");
    if (l == std::string::npos)
        return text;
    auto r = key.find_last_not_of(" \r\n\t");
    key = key.substr(l, r - l + 1);
    for (const auto &e : zhTable) {
        if (key == e.en)
            return e.zh;
    }
    return text;
}

// =======================
// HermesX mixed-text helpers
// =======================
int stringWidthMixed(OLEDDisplay *display, const char *text, int advanceX)
{
    if (!display || !text)
        return 0;
    const char *zh = translateZh(text);
    return HermesX_zh::stringAdvance(zh, advanceX, display);
}

void drawStringMixed(OLEDDisplay *display, int16_t x, int16_t y, const char *text, int lineHeight)
{
    if (!display || !text)
        return;

    const char *zh = translateZh(text);
    HermesX_zh::drawMixed(*display, x, y, zh, HermesX_zh::GLYPH_WIDTH, lineHeight, nullptr);
}

void drawStringMixedCentered(OLEDDisplay *display, int16_t centerX, int16_t y, const char *text, int lineHeight)
{
    if (!display || !text)
        return;
    const char *zh = translateZh(text);
    int width = stringWidthMixed(display, zh, HermesX_zh::GLYPH_WIDTH);
    int startX = centerX - (width / 2);
    drawStringMixed(display, startX, y, zh, lineHeight);
}

void drawStringMixedBounded(OLEDDisplay *display, int16_t x, int16_t y, int16_t maxWidth, const char *text,
                            int lineHeight)
{
    if (!display || !text)
        return;

    const char *zh = translateZh(text);
    HermesX_zh::drawMixedBounded(*display, x, y, maxWidth, zh, HermesX_zh::GLYPH_WIDTH, lineHeight, nullptr);
}

// === Internal State ===
bool isBoltVisibleShared = true;
uint32_t lastBlinkShared = 0;
bool isMailIconVisible = true;
uint32_t lastMailBlink = 0;

// *********************************
// * Rounded Header when inverted *
// *********************************
void drawRoundedHighlight(OLEDDisplay *display, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r)
{
    // Draw the center and side rectangles
    display->fillRect(x + r, y, w - 2 * r, h);         // center bar
    display->fillRect(x, y + r, r, h - 2 * r);         // left edge
    display->fillRect(x + w - r, y + r, r, h - 2 * r); // right edge

    // Draw the rounded corners using filled circles
    display->fillCircle(x + r + 1, y + r, r);             // top-left
    display->fillCircle(x + w - r - 1, y + r, r);         // top-right
    display->fillCircle(x + r + 1, y + h - r - 1, r);     // bottom-left
    display->fillCircle(x + w - r - 1, y + h - r - 1, r); // bottom-right
}

// *************************
// * Common Header Drawing *
// *************************
void drawCommonHeader(OLEDDisplay *display, int16_t x, int16_t y, const char *titleStr, bool force_no_invert, bool show_date)
{
    constexpr int HEADER_OFFSET_Y = 1;
    y += HEADER_OFFSET_Y;

    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    const int xOffset = 4;
    const int highlightHeight = FONT_HEIGHT_SMALL - 1;
    const bool isInverted = (config.display.displaymode != meshtastic_Config_DisplayConfig_DisplayMode_INVERTED);
    const bool isBold = config.display.heading_bold;

    const int screenW = display->getWidth();
    const int screenH = display->getHeight();

    if (!force_no_invert) {
        // === Inverted Header Background ===
        if (isInverted) {
            display->setColor(BLACK);
            display->fillRect(0, 0, screenW, highlightHeight + 2);
            display->setColor(WHITE);
            drawRoundedHighlight(display, x, y, screenW, highlightHeight, 2);
            display->setColor(BLACK);
        } else {
            display->setColor(BLACK);
            display->fillRect(0, 0, screenW, highlightHeight + 2);
            display->setColor(WHITE);
            if (isHighResolution) {
                display->drawLine(0, 20, screenW, 20);
            } else {
                display->drawLine(0, 14, screenW, 14);
            }
        }

        // === Screen Title ===
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        drawStringMixedCentered(display, SCREEN_WIDTH / 2, y, titleStr, FONT_HEIGHT_SMALL);
        if (config.display.heading_bold) {
            drawStringMixedCentered(display, (SCREEN_WIDTH / 2) + 1, y, titleStr, FONT_HEIGHT_SMALL);
        }
    }
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    // === Battery State ===
    int chargePercent = powerStatus->getBatteryChargePercent();
    bool isCharging = powerStatus->getIsCharging();
    bool usbPowered = powerStatus->getHasUSB();

    if (chargePercent >= 100) {
        isCharging = false;
    }
    if (chargePercent == 101) {
        usbPowered = true; // Forcing this flag on for the express purpose that some devices have no concept of having a USB cable
                           // plugged in
    }

    uint32_t now = millis();

#ifndef USE_EINK
    if (isCharging && now - lastBlinkShared > 500) {
        isBoltVisibleShared = !isBoltVisibleShared;
        lastBlinkShared = now;
    }
#endif

    bool useHorizontalBattery = (isHighResolution && screenW >= screenH);
    const int textY = y + (highlightHeight - FONT_HEIGHT_SMALL) / 2;

    int batteryX = 1;
    int batteryY = HEADER_OFFSET_Y + 1;
#if !defined(M5STACK_UNITC6L)
    // === Battery Icons ===
    if (usbPowered && !isCharging) { // This is a basic check to determine USB Powered is flagged but not charging
        batteryX += 1;
        batteryY += 2;
        if (isHighResolution) {
            display->drawXbm(batteryX, batteryY, 19, 12, imgUSB_HighResolution);
            batteryX += 20; // Icon + 1 pixel
        } else {
            display->drawXbm(batteryX, batteryY, 10, 8, imgUSB);
            batteryX += 11; // Icon + 1 pixel
        }
    } else {
        if (useHorizontalBattery) {
            batteryX += 1;
            batteryY += 2;
            display->drawXbm(batteryX, batteryY, 9, 13, batteryBitmap_h_bottom);
            display->drawXbm(batteryX + 9, batteryY, 9, 13, batteryBitmap_h_top);
            if (isCharging && isBoltVisibleShared)
                display->drawXbm(batteryX + 4, batteryY, 9, 13, lightning_bolt_h);
            else {
                display->drawLine(batteryX + 5, batteryY, batteryX + 10, batteryY);
                display->drawLine(batteryX + 5, batteryY + 12, batteryX + 10, batteryY + 12);
                int fillWidth = 14 * chargePercent / 100;
                display->fillRect(batteryX + 1, batteryY + 1, fillWidth, 11);
            }
            batteryX += 18; // Icon + 2 pixels
        } else {
#ifdef USE_EINK
            batteryY += 2;
#endif
            display->drawXbm(batteryX, batteryY, 7, 11, batteryBitmap_v);
            if (isCharging && isBoltVisibleShared)
                display->drawXbm(batteryX + 1, batteryY + 3, 5, 5, lightning_bolt_v);
            else {
                display->drawXbm(batteryX - 1, batteryY + 4, 8, 3, batteryBitmap_sidegaps_v);
                int fillHeight = 8 * chargePercent / 100;
                int fillY = batteryY - fillHeight;
                display->fillRect(batteryX + 1, fillY + 10, 5, fillHeight);
            }
            batteryX += 9; // Icon + 2 pixels
        }
    }

    if (chargePercent != 101) {
        // === Battery % Display ===
        char chargeStr[4];
        snprintf(chargeStr, sizeof(chargeStr), "%d", chargePercent);
        int chargeNumWidth = display->getStringWidth(chargeStr);
        display->drawString(batteryX, textY, chargeStr);
        display->drawString(batteryX + chargeNumWidth - 1, textY, "%");
        if (isBold) {
            display->drawString(batteryX + 1, textY, chargeStr);
            display->drawString(batteryX + chargeNumWidth, textY, "%");
        }
    }

    // === Time and Right-aligned Icons ===
    uint32_t rtc_sec = getValidTime(RTCQuality::RTCQualityDevice, true);
    char timeStr[10] = "--:--";                          // Fallback display
    int timeStrWidth = display->getStringWidth("12:34"); // Default alignment
    int timeX = screenW - xOffset - timeStrWidth + 4;

    if (rtc_sec > 0) {
        // === Build Time String ===
        long hms = (rtc_sec % SEC_PER_DAY + SEC_PER_DAY) % SEC_PER_DAY;
        int hour = hms / SEC_PER_HOUR;
        int minute = (hms % SEC_PER_HOUR) / SEC_PER_MIN;
        snprintf(timeStr, sizeof(timeStr), "%d:%02d", hour, minute);

        // === Build Date String ===
        char datetimeStr[25];
        UIRenderer::formatDateTime(datetimeStr, sizeof(datetimeStr), rtc_sec, display, false);
        char dateLine[40];

        if (isHighResolution) {
            snprintf(dateLine, sizeof(dateLine), "%s", datetimeStr);
        } else {
            if (hasUnreadMessage) {
                snprintf(dateLine, sizeof(dateLine), "%s", &datetimeStr[5]);
            } else {
                snprintf(dateLine, sizeof(dateLine), "%s", &datetimeStr[2]);
            }
        }

        if (config.display.use_12h_clock) {
            bool isPM = hour >= 12;
            hour %= 12;
            if (hour == 0)
                hour = 12;
            snprintf(timeStr, sizeof(timeStr), "%d:%02d%s", hour, minute, isPM ? "p" : "a");
        }

        if (show_date) {
            timeStrWidth = display->getStringWidth(dateLine);
        } else {
            timeStrWidth = display->getStringWidth(timeStr);
        }
        timeX = screenW - xOffset - timeStrWidth + 3;

        // === Show Mail or Mute Icon to the Left of Time ===
        int iconRightEdge = timeX - 2;

        bool showMail = false;

#ifndef USE_EINK
        if (hasUnreadMessage) {
            if (now - lastMailBlink > 500) {
                isMailIconVisible = !isMailIconVisible;
                lastMailBlink = now;
            }
            showMail = isMailIconVisible;
        }
#else
        if (hasUnreadMessage) {
            showMail = true;
        }
#endif

        if (showMail) {
            if (useHorizontalBattery) {
                int iconW = 16, iconH = 12;
                int iconX = iconRightEdge - iconW;
                int iconY = textY + (FONT_HEIGHT_SMALL - iconH) / 2 - 1;
                if (isInverted && !force_no_invert) {
                    display->setColor(WHITE);
                    display->fillRect(iconX - 1, iconY - 1, iconW + 3, iconH + 2);
                    display->setColor(BLACK);
                } else {
                    display->setColor(BLACK);
                    display->fillRect(iconX - 1, iconY - 1, iconW + 3, iconH + 2);
                    display->setColor(WHITE);
                }
                display->drawRect(iconX, iconY, iconW + 1, iconH);
                display->drawLine(iconX, iconY, iconX + iconW / 2, iconY + iconH - 4);
                display->drawLine(iconX + iconW, iconY, iconX + iconW / 2, iconY + iconH - 4);
            } else {
                int iconX = iconRightEdge - (mail_width - 2);
                int iconY = textY + (FONT_HEIGHT_SMALL - mail_height) / 2;
                if (isInverted && !force_no_invert) {
                    display->setColor(WHITE);
                    display->fillRect(iconX - 1, iconY - 1, mail_width + 2, mail_height + 2);
                    display->setColor(BLACK);
                } else {
                    display->setColor(BLACK);
                    display->fillRect(iconX - 1, iconY - 1, mail_width + 2, mail_height + 2);
                    display->setColor(WHITE);
                }
                display->drawXbm(iconX, iconY, mail_width, mail_height, mail);
            }
        } else if (isMuted) {
            if (isHighResolution) {
                int iconX = iconRightEdge - mute_symbol_big_width;
                int iconY = textY + (FONT_HEIGHT_SMALL - mute_symbol_big_height) / 2;

                if (isInverted && !force_no_invert) {
                    display->setColor(WHITE);
                    display->fillRect(iconX - 1, iconY - 1, mute_symbol_big_width + 2, mute_symbol_big_height + 2);
                    display->setColor(BLACK);
                } else {
                    display->setColor(BLACK);
                    display->fillRect(iconX - 1, iconY - 1, mute_symbol_big_width + 2, mute_symbol_big_height + 2);
                    display->setColor(WHITE);
                }
                display->drawXbm(iconX, iconY, mute_symbol_big_width, mute_symbol_big_height, mute_symbol_big);
            } else {
                int iconX = iconRightEdge - mute_symbol_width;
                int iconY = textY + (FONT_HEIGHT_SMALL - mail_height) / 2;

                if (isInverted) {
                    display->setColor(WHITE);
                    display->fillRect(iconX - 1, iconY - 1, mute_symbol_width + 2, mute_symbol_height + 2);
                    display->setColor(BLACK);
                } else {
                    display->setColor(BLACK);
                    display->fillRect(iconX - 1, iconY - 1, mute_symbol_width + 2, mute_symbol_height + 2);
                    display->setColor(WHITE);
                }
                display->drawXbm(iconX, iconY, mute_symbol_width, mute_symbol_height, mute_symbol);
            }
        }

        if (show_date) {
            // === Draw Date ===
            display->drawString(timeX, textY, dateLine);
            if (isBold)
                display->drawString(timeX - 1, textY, dateLine);
        } else {
            // === Draw Time ===
            display->drawString(timeX, textY, timeStr);
            if (isBold)
                display->drawString(timeX - 1, textY, timeStr);
        }

    } else {
        // === No Time Available: Mail/Mute Icon Moves to Far Right ===
        int iconRightEdge = screenW - xOffset;

        bool showMail = false;

#ifndef USE_EINK
        if (hasUnreadMessage) {
            if (now - lastMailBlink > 500) {
                isMailIconVisible = !isMailIconVisible;
                lastMailBlink = now;
            }
            showMail = isMailIconVisible;
        }
#else
        if (hasUnreadMessage) {
            showMail = true;
        }
#endif

        if (showMail) {
            if (useHorizontalBattery) {
                int iconW = 16, iconH = 12;
                int iconX = iconRightEdge - iconW;
                int iconY = textY + (FONT_HEIGHT_SMALL - iconH) / 2 - 1;
                display->drawRect(iconX, iconY, iconW + 1, iconH);
                display->drawLine(iconX, iconY, iconX + iconW / 2, iconY + iconH - 4);
                display->drawLine(iconX + iconW, iconY, iconX + iconW / 2, iconY + iconH - 4);
            } else {
                int iconX = iconRightEdge - mail_width;
                int iconY = textY + (FONT_HEIGHT_SMALL - mail_height) / 2;
                display->drawXbm(iconX, iconY, mail_width, mail_height, mail);
            }
        } else if (isMuted) {
            if (isHighResolution) {
                int iconX = iconRightEdge - mute_symbol_big_width;
                int iconY = textY + (FONT_HEIGHT_SMALL - mute_symbol_big_height) / 2;
                display->drawXbm(iconX, iconY, mute_symbol_big_width, mute_symbol_big_height, mute_symbol_big);
            } else {
                int iconX = iconRightEdge - mute_symbol_width;
                int iconY = textY + (FONT_HEIGHT_SMALL - mail_height) / 2;
                display->drawXbm(iconX, iconY, mute_symbol_width, mute_symbol_height, mute_symbol);
            }
        }
    }
#endif
    display->setColor(WHITE); // Reset for other UI
}

const int *getTextPositions(OLEDDisplay *display)
{
    static int textPositions[7]; // Static array that persists beyond function scope

    if (isHighResolution) {
        textPositions[0] = textZeroLine;
        textPositions[1] = textFirstLine_medium;
        textPositions[2] = textSecondLine_medium;
        textPositions[3] = textThirdLine_medium;
        textPositions[4] = textFourthLine_medium;
        textPositions[5] = textFifthLine_medium;
        textPositions[6] = textSixthLine_medium;
    } else {
        textPositions[0] = textZeroLine;
        textPositions[1] = textFirstLine;
        textPositions[2] = textSecondLine;
        textPositions[3] = textThirdLine;
        textPositions[4] = textFourthLine;
        textPositions[5] = textFifthLine;
        textPositions[6] = textSixthLine;
    }
    return textPositions;
}

// *************************
// * Common Footer Drawing *
// *************************
void drawCommonFooter(OLEDDisplay *display, int16_t x, int16_t y)
{
    bool drawConnectionState = false;
    if (service->api_state == service->STATE_BLE || service->api_state == service->STATE_WIFI ||
        service->api_state == service->STATE_SERIAL || service->api_state == service->STATE_PACKET ||
        service->api_state == service->STATE_HTTP || service->api_state == service->STATE_ETH) {
        drawConnectionState = true;
    }

    if (drawConnectionState) {
        if (isHighResolution) {
            const int scale = 2;
            const int bytesPerRow = (connection_icon_width + 7) / 8;
            int iconX = 0;
            int iconY = SCREEN_HEIGHT - (connection_icon_height * 2);

            for (int yy = 0; yy < connection_icon_height; ++yy) {
                const uint8_t *rowPtr = connection_icon + yy * bytesPerRow;
                for (int xx = 0; xx < connection_icon_width; ++xx) {
                    const uint8_t byteVal = pgm_read_byte(rowPtr + (xx >> 3));
                    const uint8_t bitMask = 1U << (xx & 7); // XBM is LSB-first
                    if (byteVal & bitMask) {
                        display->fillRect(iconX + xx * scale, iconY + yy * scale, scale, scale);
                    }
                }
            }

        } else {
            display->drawXbm(0, SCREEN_HEIGHT - connection_icon_height, connection_icon_width, connection_icon_height,
                             connection_icon);
        }
    }
}

bool isAllowedPunctuation(char c)
{
    const std::string allowed = ".,!?;:-_()[]{}'\"@#$/\\&+=%~^ ";
    return allowed.find(c) != std::string::npos;
}

std::string sanitizeString(const std::string &input)
{
    std::string output;
    bool inReplacement = false;

    for (char c : input) {
        if (std::isalnum(static_cast<unsigned char>(c)) || isAllowedPunctuation(c)) {
            output += c;
            inReplacement = false;
        } else {
            if (!inReplacement) {
                output += 0xbf; // ISO-8859-1 for inverted question mark
                inReplacement = true;
            }
        }
    }

    return output;
}

} // namespace graphics
#endif
