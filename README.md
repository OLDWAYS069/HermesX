<div align="center" markdown="1">

![截圖 2026-01-27 晚上7.29.49](https://hackmd.io/_uploads/H1WY16s8-l.png)

<h1>HermesX Firmware</h1>

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

> 目前此分支為 `HermesX_0.2.9` 的 `GOV` 整合版：以一般版為基底，整合 `CIV` 分支的 UI/功能更新，但不等同於純 `CIV` 配置。

> 在沒有網路或行動訊號的時候，HermesX 讓 LoRa 裝置仍能「看得見、操得到、傳得出去」。

HermesX 是基於 Meshtastic 的客製化韌體，主開發目標是 `heltec-wireless-tracker`，重點放在「不用拿手機也能操作」的本機 UI/UX。

目前 repo 仍保留上游 Meshtastic 的多板型結構與多個 PlatformIO environment；如果你是要編譯 HermesX 的主要體驗，請優先使用 `heltec-wireless-tracker`。

## Overview
- 以 Meshtastic 為核心通訊能力
- 強化本機操作體驗：旋鈕、按鍵、畫面、蜂鳴器、LED
- 針對 HermesX 外勤場景做 UI 與互動優化
- 持續保留上游多板型架構，但 HermesX 功能主要以 Heltec Wireless Tracker 為基準驗證

## Quick Start
- 主要建議目標：`heltec-wireless-tracker`
- 建置指令：`platformio run -e heltec-wireless-tracker`
- 文件入口：`docs/README.md`
- 近期變更：`docs/CHANGELOG_MINI.md`

## 專案現況
- HermesX 的主體功能與 UI 調整，仍以 `heltec-wireless-tracker` 的操作體驗為中心。
- repo 目前的 `platformio.ini` 預設 environment 是 `tbeam`，這是上游多 target 結構的一部分，不代表 HermesX 的主要硬體目標已改變。
- 目前 `HermesX_0.2.9` 為 `GOV` 整合版：已吸收 `CIV` 的多項更新，但預設仍保留 `GOV` 行為，不直接套用 `CIV` 的 `EMAC/Lighthouse` 關閉配置。

## HermesX 特色
HermesX 不只是把 Meshtastic 功能搬上裝置，而是把整體操作重新整理成比較像「可單手操作的隨身終端」。

### 主頁
- 主頁不是單純資訊堆疊，而是偏向一眼可讀的狀態頁。
- 使用者先看到的是時間、基本狀態、電量與 HermesX 自己的畫面語言，而不是密密麻麻的設定文字。
- 整體感覺更像裝置自己的首頁，而不是手機 App 的附屬顯示器。

### 選單設計
- 畫面不是單一路徑一直往下鑽，而是分成主頁、設定、快捷操作、訊息等不同區塊。
- 旋鈕轉動時，使用者看到的是明確的選中項目切換，而不是整頁混亂跳動。
- 常用功能會集中在幾個固定入口，操作上比較像「翻頁 + 進入選單」，不是把所有功能塞成一長串。
- 訊息、Recent Send、罐頭訊息和一般設定頁在畫面上彼此分開，使用時比較不會迷路。

### FastSetup
- FastSetup 是 HermesX 的多層快速設定流程，不只是單一入口頁。
- 進入後看到的是一層一層展開的快速設定選單，集中放進現場最常調的項目。
- 目前整合方向包含 UI、節點、GPS、電源管理與罐頭訊息等常用內容。
- 整體感受偏向「現場可直接調整的裝置內建設定頁」，不是只拿來展示狀態。
- 它在視覺上比較像 HermesX 自己整理過的一套裝置選單，而不是把原始設定項目原封不動列出來。

### 獨立快捷操作頁
- 除了設定之外，還有獨立的快捷操作頁，讓高頻動作不必每次都進完整設定流程。
- 使用者看到的是一個可以直接切換功能的操作清單，而不是參數型設定頁。
- 整體上會把「快速切換」和「深入調整」分開，畫面層次更清楚。

### 潛行模式
- 啟用前顯示確認提示，降低誤觸。
- 啟用後會限制外部通訊並降低亮度，偏向低可見度使用情境。

### 訊息體驗
- 新訊息通知盡量不打斷當前畫面。
- HermesX UI 目前有 Recent Send、訊息 detail 與罐頭訊息等獨立畫面。
- 使用者在裝置上可以比較清楚地分辨「收到什麼」、「最近傳了什麼」和「現在要送哪一則」。

### HermesX 畫面風格
- 畫面不只是在顯示資料，也在傳達裝置當下的狀態與操作節奏。
- 從首頁、提示、訊息到回饋效果，HermesX 都盡量維持一致的視覺語氣，而不是各頁各自長得像不同系統。
- 相較原始 Meshtastic 韌體，HermesX 更強調「拿在手上就能直接操作」的裝置感。

### 聲光回饋
- LED 與蜂鳴器用於送達、失敗、接收與待機狀態提示。
- 電源長按流程帶有可視化回饋，降低誤觸啟動。

## 建置與輸出
建置：

```bash
platformio run -e heltec-wireless-tracker
```

常見輸出位置：
- `.pio/build/heltec-wireless-tracker/firmware.bin`
- `.pio/build/heltec-wireless-tracker/firmware.factory.bin`
- `.pio/build/heltec-wireless-tracker/bootloader.bin`
- `.pio/build/heltec-wireless-tracker/partitions.bin`

## 文件
- `docs/README.md`：文件索引
- `docs/CHANGELOG_MINI.md`：近期變更摘要
- `docs/PLAN_upstream_2.7.15_migration.md`：升級與搬移計畫
- `docs/REF_techspec.md`：技術規格與命名約束

## 備註
- README 只保留對外與開發入口資訊。
- 內部設計筆記、歷史分支脈絡、實驗性規範與任務說明請放在 `docs/`，不要再堆回 README。
