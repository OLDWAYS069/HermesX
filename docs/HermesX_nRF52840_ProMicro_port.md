# HermesOne 0.1.0：nRF52840 ProMicro 移植基線

本文件是 `HermesOne` 分支的硬體與移植紀錄。產品版本為 `0.1.0`，韌體顯示識別為 `HermesOne_0.1.0`。

## 參考專案

- 使用者提到的 `GAT52` 實際專案名稱是 `GAT562`。
- 硬體專案：<https://github.com/gat-iot/GAT562-family>
- 繁中 Meshtastic 韌體：<https://github.com/Kent-Liu/meshtastic-firmware-zhtw>
- HermesX 現有注音引擎已在 `src/graphics/hermesx_input/README.md` 記錄其 GAT562-zhtw 來源。

## 第一階段目標

PlatformIO environment：

```text
nrf52_promicro_diy_tcxo_hermesx
```

建置：

```bash
pio run -e nrf52_promicro_diy_tcxo_hermesx
```

產物：

```text
.pio/build/nrf52_promicro_diy_tcxo_hermesx/firmware.uf2
.pio/build/nrf52_promicro_diy_tcxo_hermesx/firmware.hex
.pio/build/nrf52_promicro_diy_tcxo_hermesx/firmware.zip
```

若板上的 UF2 bootloader 已可正常運作，雙擊 Reset 進入 bootloader 後，把 `firmware.uf2` 複製到它掛載出的磁碟。磁碟名稱依使用的 ProMicro 相容板與 bootloader 而異。

## 目前沿用腳位

照片確認硬體為 fakeTec 類型的 ProMicro 載板、Heltec `HT-RA62 LF` LoRa 模組，以及 `2.42OLED-IIC VER:1.1` 顯示板。載板接線與 Meshtastic 原有的 `nrf52_promicro_diy_tcxo` 腳位相符；LoRa 僅保留 SX1262/SX1268：

| 功能 | nRF52840 GPIO |
| --- | --- |
| LoRa MISO | P0.02 |
| LoRa MOSI | P1.15 |
| LoRa SCK | P1.11 |
| LoRa CS | P1.13 |
| LoRa IRQ / DIO1 | P0.10 |
| LoRa BUSY | P0.29 |
| LoRa RESET | P0.09 |
| LoRa RXEN | P0.17 |
| I2C SDA | P1.04 |
| I2C SCL | P0.11 |
| 按鍵 | P1.00 |
| GPS UART RX（接 GPS TX） | P1.06（D10） |
| GPS UART TX（接 GPS RX） | P0.08（板上 `008`） |
| 旋鈕 A | P0.20（D3） |
| 旋鈕 B | P0.22（D4） |
| 旋鈕 SW | P1.00（D6，與板載按鍵共用） |
| 無源蜂鳴器正極 | P0.24（D5） |
| WS2812B DATA（8 顆） | P0.06（板上 `006`） |

SX126x TCXO 電壓設定為 1.8 V，並保留 `TCXO_OPTIONAL` 自動退回晶振模式。

## 顯示器、旋鈕與蜂鳴器

- 顯示器為 2.42 吋、128×64、SSD1309、I2C；模組端子為 `GND / VDD / SCL / SDA`。
- SSD1309 使用專案現有 `SSD1306Wire` 相容路徑，避免自動偵測把未知控制器套用 SH1106 的欄位偏移。
- I2C 位址依顯示板電阻設定為 7-bit `0x3C` 或 `0x3D`，韌體開機掃描後使用實際偵測到的位址。
- 旋轉編碼器已固定為 A=P0.20、B=P0.22、SW=P1.00；韌體開機會校正保存過的 Canned Message/InputBroker 設定，不必先 Factory Reset。
- 無源蜂鳴器正極接 P0.24、負極接 GND；一般啟動音、旋鈕音與事件提示音均保留。
- P0.17（D2）是 HT-RA62 的 LoRa RXEN，禁止接 LED、蜂鳴器或 GPS。
- GPS 模組 TX 接 P1.06/D10，GPS 模組 RX 接 P0.08，電源依 GPS 模組規格接 3.3 V 與 GND。此版沒有獨立 GPS_EN，GPS 會持續供電；真正的低功耗結果需納入 GPS 模組電流量測。
- WS2812B DATA 保留在 P0.06（板上 `006`）；不可再將 GPS TX 接到同一腳位。

## 容量設定

ProMicro 的 SoftDevice/bootloader 配置留下 815,104 bytes 應用程式空間。HermesOne 輕量 profile 保留 BLE、核心 Mesh、Position/GPS、Lighthouse/Finder、ATAK、文字訊息、NodeDB、TraceRoute，以及使用者指定的完整本機 UI。

容量從未接硬體與非核心背景模組回收；WS2812B 動畫與音效直接沿用 HermesX C0.4.0，不再使用 HermesOne 自行簡化的整數呼吸與整排啟動脈衝。HOME、時鐘、TAK 設定、Setup、訊息編輯、ONLINE、TraceRoute、GROUP、Fast Setup、更新 UI 及一般聲光回饋均保留。其他 HermesX/ESP32 目標不受影響。

## 驗證狀態

- 已完成：nRF52840 完整編譯、連結、容量檢查、UF2/HEX/ZIP 產生。
- 2026-09-04 的 C0.4.0 移植驗證映像：`HXB_C0.4.0_20260904_0306`；Flash 780,124 / 815,104 bytes，RAM 43,028 / 248,832 bytes。此項保留為 HermesOne 0.1.0 建立前的歷史基準。
- `firmware.uf2` SHA-256：`ec8be6f673883ac9e547e25734946750e1a2097ffbaecdae688cfae84561e73c`。
- 2026-09-05 完整保留 UI profile 驗證映像：`HermesOne_0.1.0_20260905_0650`；Flash 793,692 / 815,104 bytes（97.4%），RAM 43,292 / 248,832 bytes（17.4%）。
- HermesOne 0.1.0 UF2 SHA-256：`ddf0c658316d6e781c243c45c7da14a41a774d616b6a983628bdd545736614d2`。
- HermesOne 0.1.0 HEX SHA-256：`5ee808f4550fb7fb14a7b67b548fa5ffeff8dc93e0cc1f11786a1eb3d3befc8a`。
- HermesOne 0.1.0 ZIP SHA-256：`6b0654040b1d559467430e406ca341fef4d054c2c80800b070adc5fbf9dc55d6`。
- 已由最終 ELF 符號確認：Position、Lighthouse、ATAK、Text Message、NodeDB、TraceRoute、Canned Message、訊息編輯、ONLINE、GROUP、TAK、Fast Setup、更新 UI、啟動音與中央 LED 動畫均已連結。
- 已確認排除符號不存在：Fusion Compass、CardKB I2C 與額外 Up/Down input。
- 已完成實機項目：UF2 bootloader 寫入、韌體開機、2.42 吋 SSD1309 OLED 顯示、區域設定畫面，以及旋鈕/按鍵的基本畫面操作。
- 無源蜂鳴器已確認能發聲；正式 profile 保留一般啟動與互動音效。
- 尚未完成：GPS UART、LoRa 實際收發、Finder 脈衝、ATAK、BLE 長時間穩定性、GPS 開啟時待機電流與深度睡眠電流。
