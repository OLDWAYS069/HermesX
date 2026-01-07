<div align="center" markdown="1">

<img src="images/face.png" alt="HermesX Logo" width="96"/>
<h1>HermesX Firmware</h1>

<a href="https://github.com/OLDWAYS069/HermesX/releases">
  <img src="https://img.shields.io/github/v/release/OLDWAYS069/HermesX?include_prereleases&label=release" alt="Release"/>
</a>
<a href="https://github.com/OLDWAYS069/HermesX/releases">
  <img src="https://img.shields.io/github/downloads/OLDWAYS069/HermesX/total" alt="GitHub release downloads"/>
</a>
<a href="https://github.com/OLDWAYS069/HermesX/actions/workflows/main_matrix.yml">
  <img src="https://img.shields.io/github/actions/workflow/status/OLDWAYS069/HermesX/main_matrix.yml?branch=master&label=actions&logo=github&color=yellow" alt="CI"/>
</a>
<a href="LICENSE">
  <img src="https://img.shields.io/github/license/OLDWAYS069/HermesX" alt="License"/>
</a>

</div>

<div align="center">
  <a href="https://github.com/OLDWAYS069/HermesX">Repository</a>
</div>

> 在沒有網路或行動訊號的時候，HermesX 讓 LoRa 裝置仍能「看得見、操得到、傳得出去」。

**HermesX** 是基於 [Meshtastic](https://github.com/meshtastic/Meshtastic-device) 開源韌體延伸開發的自訂分支，
專為 LoRa 裝置打造具有互動表情顯示、旋鈕操作、訊息回覆與緊急應變的個人戰術通訊系統。

## Overview

HermesX 的核心目標是提供更直覺的操作介面與緊急模式控制，
讓離線通訊更適合日常互動與突發場景。

## 專案特點

目前版本已具備以下功能：

* **互動表情與操作介面**：
    * 支援 ST7789 OLED 表情顯示（`drawFace()` / `updateFace()`）。
    * Rotary Encoder 控制 canned messages 選單與 UI 切換。
* **視覺與聲音回饋**：
    * 搭配 WS2812 RGB LED 指示燈與被動式蜂鳴器（GPIO 可設定）。
* **快速訊息操作**：
    * 按鍵按壓觸發 canned message 發送。
    * 整合 `sendCannedMessage()`，可自訂訊息發送邏輯。
    * 解決無法退出選單的問題。
* **中文字型引入**：
    * 引用教育部標準字型，無論是訊息內容\罐頭訊息模組都可顯示中文字型。

## 特色功能

### HermesXInterfaceModule

> 讓裝置具有互動表情與操作界面。

- 支援 ST7789 OLED 表情顯示（`drawFace()` / `updateFace()`）
- 搭配 WS2812 RGB LED 指示燈、被動式蜂鳴器（GPIO 可設定）
- Rotary Encoder 控制 canned messages 選單
- 按鍵按壓觸發 canned message 發送
- 整合 `sendCannedMessage()`，自訂訊息發送邏輯


## 建置方式 (Build Instructions)

1. 安裝 PlatformIO（建議搭配 VSCode）  
   [https://platformio.org/install](https://platformio.org/install)
2. Clone 此專案

```bash
git clone https://github.com/OLDWAYS069/HermesX.git
cd HermesX
```

> [!IMPORTANT]
> 請切換到 `master` 分支（非預設的 `main`）。

3. 編譯

```bash
platformio run -e heltec-wireless-tracker
```

## 支援硬體 (Supported Hardware)

目前已於以下板子測試：

- Heltec Wireless Tracker (ESP32S3)
- 其他自定板子請根據原腳位自行修改 `GPIO` 設定

## 計劃目標 (Roadmap)

- [ ] 民防通訊（Disaster-Ready）
- [ ] 搜救定位（Rescue Beacon）
- [ ] 情感互動（LoRa Companion）
- [ ] 自主緊急應變網路（Emergency LoRa Mesh）

## Maintainer

> **OLDWAYS069**  
> HermesTrack 計畫發起人  
> GitHub: [github.com/OLDWAYS069](https://github.com/OLDWAYS069)

## License

This project is licensed under the MIT License.

## Stats

![GitHub Repo stars](https://img.shields.io/github/stars/OLDWAYS069/HermesX)
![GitHub forks](https://img.shields.io/github/forks/OLDWAYS069/HermesX)
![GitHub watchers](https://img.shields.io/github/watchers/OLDWAYS069/HermesX)

## 特別感謝
- 感謝codeX在這六個月以來陪我熬過了無數日夜，幫我寫了一堆東西出來，雖然很多時候讓我很抓狂，但沒有你我不可能一個人完成這個項目。
- ChatGPT你也有份啦，還有HermesTrack團隊的所有人
- 我不可能一個人又要寫code又要畫設計圖還要叫貨進貨出貨。
