<div align="center" markdown="1">

![截圖 2026-01-27 晚上7.29.49](https://hackmd.io/_uploads/H1WY16s8-l.png)

<h1>HermesOne Firmware</h1>

![GitHub release downloads](https://img.shields.io/github/downloads/OLDWAYS069/HermesX/total)
[![CI](https://img.shields.io/github/actions/workflow/status/OLDWAYS069/HermesX/main_matrix.yml?branch=master&label=actions&logo=github&color=yellow)](https://github.com/OLDWAYS069/HermesX/actions/workflows/main_matrix.yml)
[![License](https://img.shields.io/github/license/OLDWAYS069/HermesX)](LICENSE)
![Build](https://img.shields.io/badge/Build-PlatformIO-blue)
![Upstream](https://img.shields.io/badge/Upstream-Meshtastic-brightgreen)

</div>

<div align="center">
  <a href="https://github.com/OLDWAYS069/HermesX">Repository</a>
  -
  <a href="docs/README.md">Documentation</a>
  -
  <a href="docs/CHANGELOG_MINI.md">Changelog</a>
</div>

> 目前分支為 `HermesOne`，產品版本為 `HermesOne 0.1.0`；這是以 nRF52840 ProMicro 為主要硬體的 HermesX 分支。

> 在沒有網路或行動訊號的時候，HermesOne 仍提供低耗能的 Mesh、定位、尋人與 TAK 能力。

HermesOne 是基於 HermesX 與 Meshtastic 的 nRF52840 客製化韌體，搭配 HT-RA62 與 2.42 吋 SSD1309 OLED。產品定位是低耗能、輕功能野外節點；輕量化針對未使用的硬體驅動、背景模組與重複實作，指定的本機 UI 與 Meshtastic 核心功能仍保留。

目前 repo 仍保留上游 Meshtastic 的多板型結構與多個 PlatformIO environment；HermesOne 0.1.0 請使用 `nrf52_promicro_diy_tcxo_hermesx`。

## Overview
- 以 Meshtastic 為核心通訊能力
- 指定保留 GPS、Finder/Lighthouse 與 TAK/ATAK
- 保留 HOME、時鐘、TAK 設定、Setup、訊息編輯、ONLINE、TraceRoute、GROUP、Fast Setup 與更新 UI
- 保留 Meshtastic Text Message、Canned Message、NodeDB/NodeInfo、TraceRoute、BLE 與裝置遙測
- 保留旋鈕操作、啟動／事件音及 WS2812B 裝飾聲光回饋

## Quick Start
- 主要目標：`nrf52_promicro_diy_tcxo_hermesx`
- 建置指令：`platformio run -e nrf52_promicro_diy_tcxo_hermesx`
- UF2 產物：`.pio/build/nrf52_promicro_diy_tcxo_hermesx/firmware.uf2`
- 文件入口：`docs/README.md`
- 近期變更：`docs/CHANGELOG_MINI.md`
- 功能矩陣：`docs/HermesOne_FEATURE_MATRIX.md`

## 專案現況
- HermesOne 0.1.0 的主要硬體基線是 ProMicro nRF52840、HT-RA62、SSD1309 OLED、旋鈕、無源蜂鳴器與 UART GPS。
- repo 根層的預設 environment 仍是上游的 `tbeam`；編譯 HermesOne 時必須明確指定 `nrf52_promicro_diy_tcxo_hermesx`。
- WS2812B DATA 使用 P0.06（板上 `006`）；GPS 模組 TX 改接 P1.06/D10，GPS 模組 RX 接 P0.08。
- 本機 Text Message、Canned Message、NodeDB/ONLINE、TraceRoute、GROUP、Setup 與更新頁面均保留。

## HermesOne 0.1.0 功能邊界

目前已在實機確認 UF2 寫入、開機、SSD1309 顯示與基本旋鈕操作。LoRa、GPS、Finder、ATAK、BLE 長時間同步與低功耗仍需逐項完成實機驗證。

0.1.0 保留 GPS、TAK/ATAK、Finder/Lighthouse 及完整指定 UI；排除 MQTT、Wi-Fi/URL OTA、Waypoint、Store & Forward server、Range Test、Neighbor Info、外部通知、未接感測器與非旋鈕輸入驅動。

無源蜂鳴器使用 P0.24/D5；啟動音、按鍵音及事件音沿用 HermesX。WS2812B 使用 P0.06，GPS UART 使用 P1.06/P0.08。

完整矩陣請見 `docs/HermesOne_FEATURE_MATRIX.md`。

## 建置與輸出
建置：

```bash
platformio run -e nrf52_promicro_diy_tcxo_hermesx
```

常見輸出位置：
- `.pio/build/nrf52_promicro_diy_tcxo_hermesx/firmware.uf2`
- `.pio/build/nrf52_promicro_diy_tcxo_hermesx/firmware.hex`
- `.pio/build/nrf52_promicro_diy_tcxo_hermesx/firmware.zip`

## 文件
- `docs/README.md`：文件索引
- `docs/HermesOne.md`：HermesOne 0.1.0 定位、硬體與功能邊界
- `docs/HermesX_nRF52840_ProMicro_port.md`：接線、建置及實機驗證狀態
- `docs/CHANGELOG_MINI.md`：近期變更摘要
- `docs/VERSION_BRANCHES.md`：HermesOne 分支與版本規則
- `docs/TAK_MODE.md`：TAK MODE 操作與設定參考
- `docs/PLAN_upstream_2.7.15_migration.md`：HermesX 舊版升級計畫；不代表 HermesOne 基線
- `docs/REF_techspec.md`：HermesX／Heltec 歷史規格；HermesOne 以 nRF 移植文件為準

## 備註
- README 只保留對外與開發入口資訊。
- 內部設計筆記、歷史分支脈絡、實驗性規範與任務說明請放在 `docs/`，不要再堆回 README。
