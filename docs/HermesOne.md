# HermesOne 0.1.0

HermesOne 是 HermesX 的 nRF52840 低耗能、輕功能產品分支。「輕功能」代表減少未使用的硬體驅動、背景模組與重複實作，不代表刪除裝置端任務操作。GPS、Finder/Lighthouse、TAK/ATAK，以及 HermesX 與 Meshtastic 的主要本機功能都保留。

功能狀態以 [HermesOne_FEATURE_MATRIX.md](HermesOne_FEATURE_MATRIX.md) 為準。

## 產品原則

- 保留 LoRa Mesh、BLE、GPS、Finder/Lighthouse、TAK/ATAK。
- 保留首頁、時鐘、TAK 設定、Setup、訊息編輯、ONLINE、TraceRoute、GROUP、Fast Setup 與更新 UI。
- 保留 Meshtastic 文字訊息、Canned Message、NodeDB/NodeInfo、TraceRoute 與裝置遙測等核心能力。
- 保留旋鈕、一般啟動音、旋鈕音、事件音及 WS2812B 裝飾聲光回饋。
- WS2812B 動畫與無源蜂鳴器音序完整沿用 HermesX C0.4.0；輕量化不改寫聲光體驗。
- 深度睡眠保留；實際續航仍必須以 GPS 開啟及關閉時的實機電流量測為準。

## 版本

- 分支：`HermesOne`
- 產品版本：`0.1.0`
- 畫面識別：`HermesOne_0.1.0`
- Build 識別：`HermesOne_0.1.0_YYYYMMDD_HHMM`
- Meshtastic 協議／App 相容基線：`2.6.11`

## 硬體基線

- ProMicro nRF52840
- Heltec HT-RA62 LF LoRa 模組
- 2.42 吋 128x64 SSD1309 I2C OLED
- 旋轉編碼器與按壓開關
- 無源蜂鳴器：P0.24 / D5
- UART GPS：MCU RX=P1.06/D10（接 GPS TX）、MCU TX=P0.08（接 GPS RX）
- 8 顆 WS2812B：DATA=P0.06（板上 `006`）

GPS 與 LED 不可共用 P0.06。LED DATA 保留在 P0.06；GPS 模組 TX 必須改接 D10/P1.06。

## 建置與燒錄

```bash
pio run -e nrf52_promicro_diy_tcxo_hermesx
```

產物位於：

```text
.pio/build/nrf52_promicro_diy_tcxo_hermesx/firmware.uf2
.pio/build/nrf52_promicro_diy_tcxo_hermesx/firmware.hex
.pio/build/nrf52_promicro_diy_tcxo_hermesx/firmware.zip
```

雙擊 Reset 進入 UF2 bootloader，再將 `firmware.uf2` 複製到掛載磁碟。

## 本機功能

HermesOne 沿用完整的 HermesX 本機操作集合，包括 HOME、GPS、訊息、ONLINE/NodeDB、TraceRoute、GROUP、Finder、TAK、Setup、Fast Setup、更新流程與睡眠。中文顯示、注音輸入、訊息編輯及對應的旋鈕操作也保留。

訊息狀態以小威搬箱子的 1-bit 動畫呈現：送出往右、收到往左；送出失敗時箱子翻倒、內容物散落，小威跌倒在旁。接收提示的 `查看／略過` 仍由原訊息 popup 控制。

為符合 nRF52840 容量，0.1.0 不建立未接的外接 I2C 鍵盤與感測器驅動。啟動逐顆展開、待機呼吸與 runner、收發、ACK/NACK、關機淡出及相關音序均沿用 HermesX C0.4.0。

## 尚待實機驗證

- GPS UART 定位及長時間耗電
- 兩台裝置 LoRa 收發
- Finder 尋人脈衝
- ATAK 手機端端到端流程
- 所有保留頁面的逐頁操作
- WS2812B 使用 P0.06 後的完整 HermesX 聲光回饋
- 深度睡眠電流

## 編譯驗證

正式 environment 已以 `HermesOne_0.1.0_20260905_0650` 完整建置成功：Flash 793,692 / 815,104 bytes（97.4%），RAM 43,292 / 248,832 bytes（17.4%）。最終 ELF 已確認 Position、Lighthouse、ATAK、Text Message、NodeDB、TraceRoute、Canned Message、訊息編輯器、ONLINE、GROUP、TAK、Fast Setup、更新 UI、啟動音與中央 LED 動畫符號存在。
