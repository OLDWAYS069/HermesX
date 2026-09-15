# HermesOne 0.1.0 版本與分支

HermesOne 是從 HermesX C0.4.0 移植出的 nRF52840 產品分支。第一個版本為 `0.1.0`，工作分支為 `HermesOne`。

## 目前分支

- 工作分支：`HermesOne`
- 產品版本：`0.1.0`
- 顯示版號：`HermesOne_0.1.0`
- Build 版號：`HermesOne_0.1.0_YYYYMMDD_HHMM`
- 主要 environment：`nrf52_promicro_diy_tcxo_hermesx`
- 架構：`nRF52840`

## 歷史 GOV / CIV 與 HermesOne 差異

| 分支 | 類型 | EMAC / EM UI | Lighthouse / GROUP 尋人 | build flags |
|------|------|---------------|---------------------------|-------------|
| `HermesX_G0.3.2` | `GOV` | 啟用 | 啟用 | `MESHTASTIC_EXCLUDE_LIGHTHOUSE=0`, `HERMESX_CIV_DISABLE_EMAC=0` |
| `HermesX_C0.4.0` | `CIV` | 關閉 | 啟用 | `MESHTASTIC_EXCLUDE_LIGHTHOUSE=0`, `HERMESX_CIV_DISABLE_EMAC=1` |
| `HermesOne` | `nRF52840` | 關閉 | 關閉 | `HERMESX_NRF52840_PROMICRO=1`, `MESHTASTIC_EXCLUDE_LIGHTHOUSE=1` |

上表前兩列是 HermesX 舊產品線。`CIV` 的 `HERMESX_CIV_DISABLE_EMAC=1` 只關閉 EMAC，仍保留 GROUP 與尋人；HermesOne 則進一步排除整個 Lighthouse/GROUP/尋人模組，兩者不可混用。

## 切換版本號時要同步的位置

1. `bin/platformio-custom.py` 的 `display_short`
2. `README.md` 與 `docs/README.md` 的版本說明
3. 本檔的顯示版號與 OTA build 版號
4. `docs/HermesOne.md` 與 nRF52840 移植文件

`APP_VERSION` 仍保留 Meshtastic 相容版本；HermesOne 的畫面使用 `APP_VERSION_DISPLAY`，唯一 build 識別使用 `APP_HERMES_VERSION`。nRF52840 的交付檔案以 UF2/HEX/ZIP 為主，不使用 ESP32 的 Factory BIN 流程。
