# HermesX 專案狀態 (REF_status.md)

## 專案總覽
- 版本：ALPHA 0.2.0
- 硬體：Heltec Wireless Tracker (ESP32-S3) for HermesX 套件

## 目標任務
- [進行中] GPIO5 三擊觸發 SOS 功能整合
- [完成] HermesXInterfaceModule LED 動畫（SEND/RECV/ACK/NACK + Idle 呼吸）
- [完成] EM 模式切換時自動導向緊急清單（CannedMessageModule::setActiveList）

## 近期進度
- 302.1 MVP 提交：EmergencyAdaptiveModule / HermesXInterface / ReliableRouter Patch

## 已知議題
- 現行測試版仍沿用舊 HermesX LOGO 與字樣（參考舊專案 `graphics/img/icon.xbm` 與 `graphics/Screen.cpp`）
- GPIO5 預計在 ALPHA 0.2.1 調整為 GPIO4 以符合 RTC 限制
- EM 功能仍在聯調，SOS / SAFE 等流程尚未定版倒數與 debounce 參數
- 系統啟動流程偶爾會觸發 NACK 動畫，需持續觀察

## 注意事項
- ExternalNotificationModule 與 handleReceived() 不可同時處理 ACK/NACK；僅透過 ReliableRouter → hermesXCallback → handleAckNotification()
- handleAckNotification() 負責 ACK/NACK 的成功與失敗邏輯（src/modules/HermesXInterfaceModule.cpp:905；宣告於 src/modules/HermesXInterfaceModule.h:64）
- handleExternalNotification() 收到 index 3/4 時會呼叫 handleAckNotification()，維持 waitingForAck、LED 與表情狀態一致（src/modules/HermesXInterfaceModule.cpp:789）
