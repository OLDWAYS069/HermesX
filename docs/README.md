# HermesOne Documentation Index

> 現行產品為 `HermesOne 0.1.0`，分支 `HermesOne`，主要硬體為 ProMicro nRF52840。文件中的 HermesX C0.x、Heltec Tracker、ESP32、TFT、GOV/CIV 與 Wi-Fi OTA 內容均視為歷史參考，除非文件明確標示適用 HermesOne。

## 現行 HermesOne 文件

| 文件 | 用途 |
|------|------|
| [HermesOne.md](HermesOne.md) | 產品定位、硬體基線、建置與 0.1.0 功能邊界 |
| [HermesOne_FEATURE_MATRIX.md](HermesOne_FEATURE_MATRIX.md) | 唯一現行功能狀態表；區分保留、排除、實驗與待驗證 |
| [HermesX_nRF52840_ProMicro_port.md](HermesX_nRF52840_ProMicro_port.md) | GPIO、UF2 燒錄、容量與實機驗證 |
| [從零開始了解HermesX.md](從零開始了解HermesX.md) | HermesOne 初次燒錄、開機、BLE 與操作 |
| [AI_UPDATE_HANDOFF.md](AI_UPDATE_HANDOFF.md) | HermesOne UF2/HEX/ZIP 建置交接 |
| [VERSION_BRANCHES.md](VERSION_BRANCHES.md) | 分支、產品版號與相容版本規則 |
| [CHANGELOG_MINI.md](CHANGELOG_MINI.md) | 近期變更摘要 |

## HermesOne 保留功能文件

下列功能文件仍適用於 HermesOne 的保留功能；其中彩色 TFT 截圖或 ESP32 專屬操作不代表 nRF52840 實作完全相同：

- `主頁.md`、`設定.md`、`online.md`、`TraceRoute.md`、`罐頭模組.md`、`注音輸入.md`
- `GROUP設定.md`、`Lighthouse_portable.md`、`TAK_MODE.md`、`SMART_POWER.md`

## HermesX 舊版／其他硬體參考

下列文件保留設計與歷史價值，但不直接定義 HermesOne 0.1.0：

- `eminfo.md`
- `EM模組簡介.md`、`HermesX_EM_UI*.md`、`REF_3021.md`、`REF_30211_status.md`、`REF_EMACT_lite.md`、`REF_EmergencyMode.md`
- `TFT_portrait_attempts.md`、`UI_orientation_notes.md`、`ISSUE_neon_memory_pressure_2026-03-20.md`
- 所有 `RELEASE_HXB_*.md` 與 `RELEASE_HermesX_*.md`
- `HermesX 2026v3.md` 與 `HermesX_新版專案架構.md`

`尋人模組.md` 是 Finder 封包與操作概念參考。功能是否已編譯與實機驗證，請回到 [HermesOne_FEATURE_MATRIX.md](HermesOne_FEATURE_MATRIX.md)。
