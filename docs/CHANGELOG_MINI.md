# HermesX Mini Change Log
（每次只寫極簡亮點，便於快速回顧）

## 2026-01-01 (b0.3.0)
- 基底同步 Meshtastic 2.7.15，跟進新版 device-ui/Shared UI 繪圖管線（SharedUIDisplay、ScreenGlobals、TimeFormatters、graphics/draw、emotes）。
- 版號顯示與回報統一為 0.3.0，維持 HermesX 分支識別。
- 保持 Lighthouse 編譯排除、罐頭訊息內建 Cancel/長按退出、ACK/NACK 只在等待自身封包時觸發。
- 與 2.6.11 差異：官方 2.7.15 將彩色 UI 主要導向觸控，EC11 長按不再預設關機；現已強制保留 Rotary/CannedMessage，長按送 `INPUT_BROKER_SHUTDOWN`，長按門檻 5 秒並保留逐格＋關機動畫（延遲 1.2s）。
- 深睡喚醒新增 GPIO4 ext1，需按住約 2 秒才算有效；未達門檻會回深睡並等待放開，避免連續抖動喚醒。可透過 `HERMESX_WAKE_GPIO` / `HERMESX_WAKE_ACTIVE_LOW` 調腳位/極性。
- 開機畫面：恢復與 0.2.8 相同的 Meshtastic logo/版本開機畫面（含 HXB_0.3.0 顯示），Resuming 時仍顯示恢復提示。

## 2025-12-25 (b0.2.8)
- App 回報改為官方格式 2.6.11.x，螢幕顯示固定 `HXB_0.2.8`。
- Lighthouse 模組暫時排除編譯。
- CannedMessage 增加內建 Cancel 項，長按約 1 秒可退出選單，ACK/NACK 不再強制搶焦。

## 2025-12-25
- TAK/TAK_TRACKER 角色靜默 HermesX 介面：關閉 LED 與蜂鳴器，包含啟動/關機動畫與各種提示音/動畫。
- 版號維持 0.2.6 顯示，EMAC/SAFE 行為回復 0.2.6 流程。

## 2025-12-05
- 顯示版號分離：對 App 回報 2.6.11（APP_VERSION/SHORT），螢幕顯示 0.2.6（APP_VERSION_DISPLAY），便於相容與現場辨識。
- TFT 喚醒重繪：ST77xx/ILI9xxx 等面板在 VEXT 斷電後會遺失 GRAM，醒來時強制 `ui->init()` + `forceDisplay(true)`，避免亮背光但黑屏。
- 清理冗長 log：移除 HermesX LED `selectActiveAnimation` 的大量狀態列印。

## 2025-11-30
- 分支 `hermesX_b0.2.6` 紀錄 Heltec Tracker 電源鍵長按喚醒異常：按住反覆重啟、無 ButtonThread 事件與 power-hold 動畫，文件化環境與待查方向。

## 2025-11-29
- Lighthouse EMACT：支援全形＠的 `@EmergencyActive:<pass>`，並在拒絕/通過時寫 log（密碼/白名單狀態）。
- 開機版號：改用 `HXB_<語意版號><git>` 的 APP_VERSION_DISPLAY，從分支名推語意版號，方便現場辨識。

## 2025-10-30
- 恢復 HermesX CN12 混排的 ASCII 分流，英數回到半形寬度，中文仍交由 CN12 繪製。
- CannedMessage 清單與輸入框改用混排渲染，完整保留 UTF-8 中文並原樣傳送。

## 2025-10-27
- 紀錄 HermesX BootHold 行為：若畫面出現 `Resuming...`，表示由 EXT1/RTC 喚醒，仍須持續按住電源鍵超過 `BUTTON_LONGPRESS_MS` 才會開機；提前放開仍會依設計回到深睡眠。
- 目前已知問題：雖可喚醒開機，但約 5 秒後會再度關機，初判為門檻設定異常。

## 2025-10-29
- 恢復 TFT CN12 中文快路徑，改善彩色面板上的字元性能與顏色一致性。
- Removed the custom TFT fast-path, reverting to the stock drawing pipeline; fixes compass overlay corruption.
