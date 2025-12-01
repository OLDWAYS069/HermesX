# HermesX b0.2.6 功能摘要

## 版本基線
- 分支／版號：`hermesX_b0.2.6`（APP_VERSION `HXB_0.2.64158fb2`，build flags in build.log）
- 目標硬體：Heltec Wireless Tracker（主鍵 GPIO04，active-low/pullup）
- 參考 commit：`4158fb2 add password for lighthouse module, but not test yet`

## 電源鍵與喚醒行為（集中式 LED 管理）
- 開機長按閘門：`kWakeHoldMs = 3000ms`（ButtonThread），僅在 powerFSM `stateDARK` 時觸發。
- 長按進度與動畫：按下立即啟動集中式 PowerHold 逐格進度（5s 預設門檻），達門檻後進入 fade→latchedRed。鬆手未達門檻會重置。
- 關機長按：`BUTTON_LONGPRESS_MS = 5000ms`，達門檻即啟動 ShutdownEffect 動畫，等待動畫時間後才執行 shutdown，並在關機前清空所有 LED 狀態。
- Startup/Shutdown 動畫：由集中 LED 管理器控制，啟動時播放 StartupEffect，關機前播放 ShutdownEffect。
- BootHold/Resuming：EXT1/RTC 喚醒仍需按住達門檻才開機；提前放開會回深睡，動畫路徑也走集中管理。

## Lighthouse 模組（文字指令）
- 授權：`@EmergencyActive` 需要通過 `/prefs/lighthouse_passphrase.txt` 或 `/prefs/lighthouse_whitelist.txt`（NodeNum）任一授權；缺檔或空值時會拒絕（LighthouseModule.cpp）。
- 模式切換：
  - `@EmergencyActive <pass>`：開啟緊急中繼，role 設為 `ROUTER`、關閉省電，寫入狀態後重啟。
  - `@GoToSleep`：切到節能輪詢（醒 300s / 睡 1800s），紀錄首次開機時間並重啟。
  - `@ResetLighthouse`：清除狀態、重設 role 為 `ROUTER_LATE`，重啟。
- 訊息廣播：
  - `@Status`：廣播目前模式（Active／Silent／中繼）與剩餘喚醒/睡眠時間。
  - `@HiHermes`：廣播介紹訊息（Shine1 說明與連結）。
- 永久化：首次開機時間 `/prefs/lighthouse_boot.bin`；模式旗標 `/prefs/lighthouse_mode.bin`；白名單/passphrase 皆在 `/prefs` 底下存放。

## 已知問題／風險
- 実機長按喚醒失效：在 Heltec Tracker 上按住會反覆重啟且未看到 ButtonThread 事件或 power-hold 動畫（docs/ISSUE_powerhold_longpress.md）。需確認按鍵電平、activeLow 設定、powerFSM 狀態與 HermesX 模組初始化時序。
- Lighthouse passphrase 功能尚未實測（commit 註記），需驗證授權判斷與檔案讀寫。

## 建議驗證
- 關機／Resuming 畫面按住 3 秒應播放 PowerOn 動畫並穩定進入系統；鬆手前後 log 須出現 `startPowerHoldAnimation` / `EVENT_PRESS`。
- 於 `/prefs` 準備 passphrase 與 whitelist，分別測試 `@EmergencyActive` 通過與拒絕案例，並確認狀態持久化與重啟後行為。
- 在節能輪詢模式下驗證醒 300s、睡 1800s 節奏，以及 `@Status` 報文內容。
