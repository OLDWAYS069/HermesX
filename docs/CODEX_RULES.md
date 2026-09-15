# HermesOne 工作規則

## 基線

- 分支：`HermesOne`
- 產品版本：`0.1.0`
- 主要 environment：`nrf52_promicro_diy_tcxo_hermesx`
- 硬體：ProMicro nRF52840、HT-RA62、SSD1309 OLED

## 功能邊界

- 以 `HermesOne_FEATURE_MATRIX.md` 為唯一現行功能狀態表。
- 不得將 GPS、TAK/ATAK、GROUP/Lighthouse、尋人、EMAC、MQTT、Wi-Fi OTA、Waypoint 或感測器描述為 HermesOne 0.1.0 功能。
- 舊 UI 原始碼或殘留選單文字不等於底層功能可用。
- WS2812B 目前為實驗且未通過實燈驗證；蜂鳴器僅確認能發聲。

## 修改原則

- 新增 HermesOne 專用條件時使用明確的 nRF52840 scope，避免改壞其他上游板型。
- 不任意改動 LoRa 核心、SoftDevice、bootloader 與第三方庫。
- UI、輸入與模組註冊要一起檢查，避免只留下可見但無底層功能的入口。
- 文件必須區分 Designed、Compiled、Tested 與 Field Validated。

## 驗證

- `git diff --check`
- `platformio run -e nrf52_promicro_diy_tcxo_hermesx -j 4`
- 從 build log 讀取真正的 `APP_HERMES_VERSION`
- 確認 UF2/HEX/ZIP 產物與 SHA-256
