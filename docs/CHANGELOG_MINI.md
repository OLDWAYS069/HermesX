# HermesX Mini Change Log
（每次只寫極簡亮點，便於快速回顧）

## 2026-03-05
- 修正 Home Timer 每秒整頁重刷：改為時鐘差分更新，降低閃爍。
- 修正返回 Home 殘影與只剩 Timer 問題：加強進出 Home 清區與首次完整重繪。
- 修正 `CannedMessage` 的 `Cancel` 誤跳 `Recent Send`：預設改回 Home，並加入 `restoreReturnTarget default -> home` log。
- 修正 `CannedMessage` 旋鈕誤退出：旋轉改為優先做訊息選單導航，不再誤觸 `Cancel/Back` 導致回 Home。

## 2026-03-03
- HermesX 主頁改版：Home 改為大時鐘主頁，整合日期 / Role / 橫向電池 / 右下衛星角標，並加入 `Pattanakarn` 數字字型資產。
- TFT Home 時鐘改為 direct-TFT 霓虹渲染：新增 `fillRect565()/drawPixel565()` 直繪 API，改用多層藍光暈 + 青白管壁 + 白色核心，不再只靠單色 buffer 假發光。
- Home 時鐘改為局部差分更新：秒數變化只更新變動字形與光暈層，降低每秒整頁重刷感。
- 新增 `Recent Send` 訊息流程：最近訊息清單與詳頁分離，`MSG` 入口整合到 HermesX 自訂 UI。
- 新訊息提示改為 3 秒彈出：只在「訊息喚醒螢幕」時顯示；短按可直進該則訊息，旋鈕可關閉。
- 修正訊息彈窗時序：`EVENT_RECEIVED_MSG` 先喚醒螢幕導致 popup 不觸發的問題，改為先由 `Screen` 判斷是否 arm popup 再喚醒。
- CannedMessage 與 Recent Send 隔離：Recent list / detail 頁不再被罐頭選單搶事件；取消時優先關閉 CannedMessage，避免誤退回訊息列表。
- 旋鈕輸入補 fallback：若未明確設定 rotary 的 `cw/ccw/press` 事件，預設回退為 `UP/DOWN/SELECT`，減少自訂頁面「有 raw log 但 UI 沒反應」情況。

## 2026-02-22
- 建立分支 `HermesX_0.2.9_Civ`。
- Civ 版已排除 EMAC 功能（FastSetup 隱藏 EMAC 設定、停用 EM 緊急 UI 觸發）。
- Lighthouse 模組改為關閉（不建立模組實例）。

## 2026-02-18
- Fast Setup：新增多層快速設定流程，整合 EMAC/UI/節點/GPS/罐頭訊息。
- UI設定：`LED亮度` 改為控制 **WS2812 使用者燈**（不再改 TFT 背光），新增檔位 `關閉 / 低 / 中 / 高 / 最大`。
- 潛行模式：新增一鍵關閉 UI LED(2812)/蜂鳴器，並停用 GPS、BLE、LoRa TX 與 radio 介面。
- TFT：Fast Setup 新增分區彩色（header/選中列/toast），頁層切換才全重繪，旋鈕移動不再每步整頁刷新。
- 體感修正：退出 FAST Setup 改為局部 palette 區塊失效重繪，降低卡頓。
- 版本差異（vs `HXB_0.2.8`）：新增 Fast Setup、WS2812 亮度設定、潛行模式與 TFT 彩色分區/刷新優化；App 回報版本仍為 2.6.11.x。

## 2026-02-10
- EM UI：新增 HermesXEmUi 模組，`@EmergencyActive` 彈出中文緊急選單（⚠️ + 倒數 + 受困/醫療/物資/安全），旋鈕送出後停止蜂鳴器。
- EM UI 右欄：上半倒數、下半狀態（已傳送／傳送成功請原地待命／傳送失敗），NACK 會停留直到下一次送出。
- Lighthouse EMAC：白名單/密碼授權；SAFE 60s 寬限與 60s 週期自動 SOS；Emergency OK ACK 後啟用 EM Tx lock；手機觸發 EMAC 直接啟用鎖。
- EM Tx lock：鎖定時僅允許 emergency port 與手機的 `@EmergencyActive/@ResetLighthouse/@GoToSleep/@HiHermes/@Status`。
- Rotary 待機恢復：light sleep 前後 detach/attach interrupts，並增加 Press 去彈跳與低電位檢查。

## 2026-01-26
- BootHold：gate 提前到 NodeDB 之前，僅在睡眠喚醒時啟動；改為 GPIO 直讀 + baseline 判定，按鍵門檻前不放行主系統，並輸出 gate log。
- BootHold UI：短按喚醒後顯示「. .. ... ....」長按點點動畫，避免與 Resuming 互相覆蓋；等待期內不播放 LED 啟動動畫。
- Prefs 保護：設定讀取失敗時先嘗試還原 `/backups/backup.proto`，成功即不覆寫預設；正常啟動會自動建立/更新備份，避免設定被洗掉。

## 2026-02-04
- 螢幕顯示版號更新為 `HXB_0.2.9`（App 版本仍為 2.6.11）。

## 2026-01-25
- BootHold：短按喚醒後進入 5 秒等待期，期間需長按才放行開機；等待期改為 setup 內阻塞式 gate，未達門檻前不初始化 MeshService/模組；新增長按期間由暗轉亮的進度動畫與「.」循環提示，喚醒時以點點提示覆蓋 Resuming；開機時記錄喚醒原因（timer/ext/reset）。

## 2026-01-09
- 修正開機頁面右上角 Node ID/短名顯示向左偏移，改為逐行右對齊計算後繪製。

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
