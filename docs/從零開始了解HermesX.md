---
title: 從零開始使用 HermesOne
---

# 從零開始使用 HermesOne 0.1.0

HermesOne 是 ProMicro nRF52840、HT-RA62 與 2.42 吋 SSD1309 OLED 的開發版本。目前適合燒錄、操作與通訊測試；指定的 HermesX 本機功能均已編入，但仍需逐頁完成實機驗證。

## 燒錄韌體

1. 使用 USB 連接 HermesOne。
2. 快速短接兩次 Reset，讓 nRF52840 進入 UF2 bootloader。
3. 等待系統掛載 bootloader 磁碟。
4. 將 `.pio/build/nrf52_promicro_diy_tcxo_hermesx/firmware.uf2` 複製到該磁碟。
5. 複製完成後裝置會自動重新啟動。

macOS Finder 若回報 `-36`，可改用終端機的 `cp`，並確認 bootloader 磁碟仍掛載中。複製時磁碟自動退出通常是燒錄完成，不一定是失敗。

## 第一次開機

正常情況會先看到 HermesOne 開機畫面，接著進入 Meshtastic 的地區設定或首頁。台灣使用者應選擇合法的 `TW` 區域；未設定地區前 LoRa 不會正常發射。

目前首頁會顯示裝置名稱、時間或 `HermesOne_0.1.0`、Role 與在線節點數。這是 128x64 OLED 專用的精簡首頁，不是 Heltec Tracker 的彩色 TFT 版面。

## 旋鈕操作

- 旋轉：切換頁面或移動選項。
- 短按：進入或確認。
- 長按與休眠／關機行為尚未完成 nRF52840 實機驗證，測試時不要把它當成可靠的硬體斷電。

## 連接 Meshtastic App

HermesOne 保留 nRF52 BLE。可在 Android 或 iOS 的 Meshtastic App 搜尋裝置並配對。0.1.0 尚需驗證長時間同步、斷線重連與大量 NodeDB 資料，若 App 看不到裝置，先重新啟動韌體並確認不是停在 UF2 bootloader。

## 目前可以測試的功能

- LoRa SX126x 基本收發
- BLE 配對與 App 同步
- ONLINE / NodeDB 顯示
- 文字訊息與罐頭訊息
- TraceRoute
- 中文顯示與注音輸入
- 蜂鳴器提示

以上多數功能是「已編譯、待端到端實機驗證」，不是已宣告完成。測試結果應回填 `HermesOne_FEATURE_MATRIX.md`。

## 0.1.0 保留與不提供的功能

HermesOne 0.1.0 保留 GPS、TAK/ATAK、GROUP、Lighthouse/Finder、文字訊息、ONLINE/NodeDB、TraceRoute、Setup、Fast Setup、更新 UI 及本機聲光回饋。

不提供的項目是 MQTT、Wi-Fi/URL OTA、Waypoint、Range Test、Neighbor Info、未接的感測器、外接 CardKB／額外 Up/Down 輸入、獨立 EM UI 執行緒與 Heltec 彩色 TFT Direct UI。nRF 更新 UI 保留，但實際燒錄走 UF2/HEX/DFU ZIP。

## 已知問題

- WS2812B DATA 使用 P0.06（板上 `006`）；GPS 模組 TX 改接 P1.06/D10，GPS 模組 RX 接 P0.08。
- 蜂鳴器已可發聲，完整提示音行為尚未驗證。
- LoRa、訊息、BLE 穩定性與睡眠電流仍待測試。

完整狀態請見 [HermesOne_FEATURE_MATRIX.md](HermesOne_FEATURE_MATRIX.md)。
