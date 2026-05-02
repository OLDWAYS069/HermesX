# HermesX Emergency Mode Reference（現行實作）

## 入口 / 觸發
- 收到 `@EmergencyActive`（TEXT_MESSAGE_APP）且通過白名單或 passphrase。
- Rotary 三擊（本地觸發）：直接進入 EM UI，並對外廣播 `@EmergencyActive`。
- Lighthouse 設 `emergencyModeActive=true`、關閉省電、角色改為 `ROUTER`，並彈出 EM UI（banner 預設「請在60秒內回復」）。
- 若來源為手機（`from==0`）立即啟用 EM Tx lock；否則回送 Emergency OK（port 300，want_ack=true），ACK 後啟用 EM Tx lock。

## EM UI 畫面
- 模組：`HermesXEmUiModule`。
- 標頭：⚠️ + 「人員尋回模式啟動」。
- 左側清單：`受困 / 醫療 / 物資 / 安全`。
- 右側欄：上半倒數、下半狀態（等待中 / 已傳送 / 傳送成功請原地待命 / 傳送失敗）。
- 底部：banner 文字（由 Lighthouse 進入原因提供）。

## 輸入規則
- 使用 `canned_message` 的 rotary 設定（CW/CCW/Press）。
- 必須先旋轉導覽（5 秒內 armed）才能 Press 送出。
- 送出冷卻 1.5 秒。

## 送出封包（EM UI）
- Port：`PORTNUM_HERMESX_EMERGENCY`（300），Broadcast。
- Payload 對映：
  - 受困 → `STATUS: TRAPPED`（want_ack=true）
  - 醫療 → `NEED: MEDICAL`（want_ack=true）
  - 物資 → `NEED: SUPPLIES`（want_ack=true）
  - 安全 → `STATUS: OK`（want_ack=false，會 `markEmergencySafe()` 並退出 EM UI）
- ACK/NACK：routing ACK 回報；成功顯示 2 秒，失敗持續直到下一次送出。
- 只要送出過一次即停止蜂鳴器。

## 自動 SOS（Lighthouse）
- EM Active 且未 SAFE 時：60 秒寬限後送 `SOS`，之後每 60 秒重送。
- SAFE 會停止自動 SOS，倒數消失。

## EM Tx lock（外送封鎖）
- 啟用時只允許 port 300 的 Emergency 封包。
- 允許手機文字指令：`@EmergencyActive`, `@ResetLighthouse`, `@GoToSleep`, `@HiHermes`, `@Status`。

## EMINFO / 各裝置狀態
- `EMINFO` 為 HermesX 擴充機制，用於同步各節點目前的 EM 狀態。
- `各裝置狀態` 頁面只列出有送出 `EMINFO` 的裝置。
- `EMINFO` 不等於主回報封包：
  - 主回報封包用於 `受困 / 醫療 / 物資 / 安全`
  - `EMINFO` 用於狀態同步與狀態頁面更新
- `EMINFO` 目前資料至少包含：
  - 狀態代碼
  - 電量百分比
  - 裝置短名

## EM Heartbeat（規劃）
- `EM Heartbeat` 預計作為 HermesX 擴充，用於：
  - 確認對方是否仍在線
  - 提供最後活動時間
  - 作為 `各裝置狀態` 的在線判定基礎
- 建議預設週期為 `5 秒`
- 建議超過 `15 秒` 未收到則視為離線或失聯

## EMAC 設定中的相關參數
- `EMAC設定` 應可調整以下參數：
  - `EMINFO廣播`
  - `EMINFO週期`
  - `Heartbeat週期`
  - `離線判定門檻`
  - `是否附帶電量`

## 相關實作位置
- `src/modules/HermesEmUiModule.cpp`
  - `setEmInfoBroadcastEnabled(...)`
  - `sendEmInfoNow()`
  - `getEmInfoIntervalMs()`
  - `recordEmInfoPayload(...)`
  - `runOnce()`
- `src/modules/HermesEmUiModule.h`
- `src/mesh/MeshService.cpp`

## 相關檔案
- `src/modules/HermesEmUiModule.cpp`
- `src/modules/LighthouseModule.cpp`
- `src/mesh/MeshService.cpp`
