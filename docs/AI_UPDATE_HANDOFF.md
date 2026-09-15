# HermesOne nRF52840 Build Handoff

本文件適用 `HermesOne` 分支。舊 HermesX 的 Heltec Tracker Wi-Fi OTA、URL OTA、`firmware.bin` 與 `firmware.factory.bin` 流程不適用本分支。

## 1. 建置正式 environment

```bash
platformio run -e nrf52_promicro_diy_tcxo_hermesx -j 4
```

必須從當次 build log 讀取實際版本，例如：

```text
-DAPP_HERMES_VERSION=HermesOne_0.1.0_YYYYMMDD_HHMM
```

不得用舊 log 或自行猜測時間戳。

## 2. 驗證產物

必須確認以下檔案存在：

```text
.pio/build/nrf52_promicro_diy_tcxo_hermesx/firmware.uf2
.pio/build/nrf52_promicro_diy_tcxo_hermesx/firmware.hex
.pio/build/nrf52_promicro_diy_tcxo_hermesx/firmware.zip
```

交付前對三個檔案執行 SHA-256，並在 Changelog 或回覆中記錄當次結果。

## 3. 燒錄方式

1. 快速短接兩次 Reset 進入 UF2 bootloader。
2. 等待 bootloader 磁碟掛載。
3. 將 `firmware.uf2` 複製到該磁碟。
4. 磁碟退出、裝置重新啟動後，確認 OLED 顯示新版本。

Finder 回報 `-36` 時可改用終端機 `cp`；複製目標必須使用當下實際掛載路徑。

## 4. 回覆內容

交付時應包含：

- 實際 `APP_HERMES_VERSION`
- 正式 environment 是否成功
- RAM / Flash 使用量
- UF2、HEX、ZIP 的絕對路徑與 SHA-256
- 尚未完成的實機驗證項目

不得提供 HermesX ESP32 的 Wi-Fi upload 指令，因為 HermesOne 0.1.0 沒有該更新路徑。
