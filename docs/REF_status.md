# HermesX 專案狀態 (REF_status.md)

## 專案總覽
- 版本：BETA 0.2.1
- 硬體：Heltec Wireless Tracker (ESP32-S3)

## 目標任務
- [暫時移除] GPIO5 三擊觸發 SOS 功能整合(包括整合302.1協議的測試)
- [完成] HermesXInterfaceModule LED 動畫（SEND/RECV/ACK/NACK + Idle 呼吸）(不過LED的發送接收邏輯左右要反過來)
- [完成] EM 模式切換時自動導向緊急清單
- [已完成] EM 最小中文字型整合（HermesX_EM16_ZH 1bpp/16px，維持 UTF-8 顯示流程）
- [暫時移除] 將CANNED UI功能轉90度的直式顯示(包括SENDING FACE ERROR FACE等)0.2.2回來


## 近期進度
- 302.1 MVP 提交：EmergencyAdaptiveModule / HermesXInterface / ReliableRouter Patch
- 302.1 Direct Sender 與 UI 鉤子於 0.2.1 暫時移除，預計 0.3.0 重新導入

## 已知議題
- GPIO5 預計在 BETA 0.2.1 調整為 GPIO4(GPT說GPIO4才能實現長按喚醒)
- EM 功能仍在聯調（SOS / SAFE 流程倒數與 debounce）
- 251015:LED的發送接收邏輯左右要反過來 
- 302.1協議不確定會部會影響到現有的傳輸邏輯(格式的部分，不知道會部會占用太多頻寬)
- 電源鍵長按喚醒實機無觸發：分支 hermesX_b0.2.6 (HXB_0.2.64158fb2) / Heltec Wireless Tracker / GPIO04 按鍵。開機長按時 log 只有系統啟動，沒有 ButtonThread “press/long press” 或 `startPowerHoldAnimation()` 訊號，按住期間會反覆重啟直到鬆手，長按動畫與 3 秒喚醒閘門都未生效。待確認按鍵電平/activeLow、powerFSM 是否仍在 DARK、或 HermesX 模組初始化時序。

## 對 Codex 的提醒
- 若任務涉及 EM / Rotary / 302.1 請優先參照：
  - HermesX_EM_UI_v2.1.md
  - task_emui_rotary3021.md
- 所有提交請使用 patch 格式（見 docs/CODEX_PATCH_FORMAT.md）
- TRACKER 的 TFT是160X80
## 備註
- `Resuming...` 畫面代表此次為 EXT1/RTC 喚醒，需長按電源鍵直到超過 `BUTTON_LONGPRESS_MS` 才能完成啟動；若提前放開會依照 BootHold 流程重新進入深睡。
