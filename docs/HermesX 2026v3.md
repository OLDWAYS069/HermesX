---
title: HermesX 2026v3

---

# HermesX 2026v4s

![HermesX](https://hackmd.io/_uploads/HkuLGCQrWl.jpg)

<div align="center">

<a href="https://github.com/OLDWAYS069/HermesX/releases">
  <img src="https://img.shields.io/github/v/release/OLDWAYS069/HermesX?include_prereleases&label=release" alt="Release"/>
</a>

<a href="LICENSE">
  <img src="https://img.shields.io/github/license/OLDWAYS069/HermesX" alt="License"/>
</a>

</div>

<div align="center">
  <a href="https://github.com/OLDWAYS069/HermesX">Repository</a>
</div>

> 在沒有網路或行動訊號的時候，HermesX 讓 LoRa 裝置仍能「看得見、操得到、傳得出去」。

**HermesX** 是基於 [Meshtastic](https://github.com/meshtastic/Meshtastic-device) 的客製化韌體分支，主要開發與驗證目標是 `heltec-wireless-tracker`。  
目前這份文件對應 `HermesX_C0.3.2` 的 `CIV` 版：由 `HermesX_0.2.9` 功能基底升版，關閉 EMAC / EM UI，但保留 GROUP、尋人模組、ONLINE、TAK MODE、Fast Setup 與更新模式。

:::warning
`CIV` 版不提供本機 EMAC 進入、遠端 `ACTIVATE: EMAC`、`@EmergencyActive`、`STATUS: LOST` 與 Lighthouse sleep 控制。  
如果你需要完整 EMAC / EM UI，請確認使用的是 GOV 分支與對應韌體。
:::

## Overview

HermesX 的目標不是取代 Meshtastic App，而是把現場最常用的操作搬到裝置端：

- 用旋鈕操作主選單、設定、訊息與節點列表
- 不拿手機也能查看在線節點、同組節點與位置資訊
- 透過 GROUP PIN 管理尋人與同組狀態同步
- 讓更新、設定、GPS、LoRa、MQTT、電源與節點資料庫維護都能在裝置上處理

目前功能以 Heltec Wireless Tracker 的小螢幕、旋鈕、蜂鳴器、RGB / 狀態燈與 LoRa 操作體驗為中心設計。

---

## 目前版本重點

### ==HermesX 主頁與主選單==

HermesX 的首頁以裝置端可讀性為主，顯示時間、日期、Role、電池與基本狀態。  
主選單則是圖示化 action page，目前包含：

- `潛行模式`
- `緊急照明燈`
- `GPS`
- `TAK MODE`
- `休眠`
- `Home`
- `頻道`
- `設定`
- `MSG`
- `ONLINE`
- `TraceRoute`
- `GROUP`
- `尋人模組`

從 Home 短按進入主選單時，焦點會停在 `Home`，避免誤觸第一個高風險功能。

### ==Fast Setup 設定頁==

HermesX 內建多層快速設定，不需要每次都拿手機調整。

目前設定根目錄包含：

- `GROUP設定`
- `UI設定`
- `裝置管理`
- `罐頭訊息`
- `儲存並重新開機`

`UI設定` 可調整全域蜂鳴器、Hermes 狀態條、板載 RGB 燈、螢幕休眠、時區與旋鈕對調。  
`裝置管理` 則集中裝置資訊、LoRa、GPS、MQTT、頻道設定、藍牙、電源管理、節點資料庫與更新模式。

### ==GROUP 與同組節點==

`GROUP` 是 HermesX C0.3.2 裡最重要的任務分組概念。  
主選單進入 `GROUP` 後會先看到：

- `GROUP設定`
- `節點列表`

`GROUP設定` 可設定 `GROUP PIN A/B`、查看 PIN，並調整 `EMINFO設定`。  
在 CIV 版中，`解除EMAC` 不會作為本版主功能出現，因為 EMAC 已被 build flag 關閉。

`節點列表` 顯示同組裝置的 presence。只要設定 GROUP PIN，CIV 版也會透過 GROUP Heartbeat 維持同組節點狀態，不需要進入 EMAC。

### ==尋人模組==

尋人模組是 CIV 版保留的核心功能。  
它用 `GROUP PIN` 做授權，送出輕量位置點名：

```text
REQUEST: POS GROUP <pin>
```

使用者從 `尋人模組` 進入後，可選擇：

- `離開`
- `發送尋人訊號`
- `節點列表`

送出尋人訊號後，畫面會顯示雷達掃描動畫並等待同組裝置回傳位置。  
收到有效位置後，會進入尋人模組自己的清單與明細頁，而不是混用一般 ONLINE 清單。

### ==ONLINE 節點瀏覽==

`ONLINE` 是裝置端 NodeDB 瀏覽器。  
它會列出最近仍有活動的節點，適合快速確認附近或曾經聽到的裝置。

ONLINE detail 目前提供：

- `MSG`
- `TraceRoute`
- `LongName`
- `LastHeard`
- `Link`
- `location`
- `經`
- `緯`
- `高度`
- `相對位置`

如果節點是 MQTT 來源，TraceRoute 會顯示 `TraceRoute: --`，操作時提示 `LORA ONLY`，避免對 MQTT-only 節點送出不合理的 LoRa route request。

主選單也提供獨立 `TraceRoute` 頁面。進入後會顯示類似 ONLINE 的節點列表，選取節點後會進入只針對 TraceRoute 操作的明細頁，焦點預設停在 `開始TraceRoute` 按鈕。

### ==MSG 與罐頭訊息==

`MSG` 入口整合 HermesX 的裝置端訊息流程。  
Recent Send、訊息 detail 與 canned message 編輯流程已經和 HermesX 專用頁面分離，避免旋鈕輸入被錯誤搶走。

在 ONLINE 或 GROUP detail 裡選 `MSG`，會針對目前選取節點開啟直接訊息流程。

### ==TAK MODE==

`TAK MODE` 是獨立 action page。  
短按會打開彈窗操作頁，可切換 TAK ON/OFF、進入 `TAKMODE設定`，並依設定開啟或關閉相關附屬入口。

`TAKMODE設定` 可調整：

- 裝置資訊廣播
- GPS 刷新
- 位置廣播
- 智慧距離
- 智慧間隔
- 聲光靜默
- EMUI
- 尋人模組

在 CIV 版中，EMUI 即使出現在 TAK 設定語意裡，也不代表 EMAC 可用；本分支的 EM UI 進入會被 CIV build 關閉。

### ==潛行模式與聲光回饋==

潛行模式會先顯示確認提示，降低誤觸。  
啟用後會保存原狀態並關閉或降低容易暴露的輸出，例如狀態燈、蜂鳴器、GPS / 藍牙 / LoRa TX 等通訊與聲光行為。

一般使用時，HermesX 仍保留蜂鳴器與狀態燈回饋，用於送出、接收、失敗、模式切換與低記憶體提醒。  
如果需要安靜使用，可到 `UI設定` 關閉全域蜂鳴器或調整 Hermes 狀態條。

### ==更新模式==

HermesX C0.3.2 保留裝置端更新流程。

從：

```text
設定 > 裝置管理 > 更新模式
```

按下後會進入獨立更新轉場頁，顯示 `進入更新模式` / `重開中`，再進入 dedicated update environment。

更新模式支援：

- `WiFi設定`
- `檢查更新`
- `手動更新`
- `WiFi更新`
- `USB更新`

`WiFi設定` 位於更新模式第一層，供 URL 更新檢查與 WiFi 手動更新共用。
WiFi / USB 手動更新頁會顯示目前版本、連線狀態、接收狀態、待更新版本、進度與錯誤資訊；更新進度以小螢幕友善的圓形進度呈現。

---

## CIV 版功能邊界

`HermesX_C0.3.2` 是 CIV 版，因此功能邊界如下：

| 功能 | CIV 狀態 |
|------|----------|
| Home / GPS / HermesX 主選單 | 啟用 |
| Fast Setup | 啟用 |
| GROUP PIN | 啟用 |
| GROUP 節點列表 / Heartbeat presence | 啟用 |
| 尋人模組 `REQUEST: POS GROUP <pin>` | 啟用 |
| ONLINE / MSG / TraceRoute | 啟用 |
| TAK MODE | 啟用 |
| WiFi / USB / URL 更新流程 | 啟用 |
| EMAC / EM UI 本機進入 | 關閉 |
| 遠端 `ACTIVATE: EMAC` / `@EmergencyActive` | 關閉 |
| `STATUS: LOST` 週期保護與 Lighthouse sleep 控制 | 關閉 |

簡單說，CIV 版保留「裝置端操作、尋人、同組 presence、節點瀏覽與更新」，但不提供完整緊急接管流程。

---

## 建置與下載

主要建置目標：

```bash
platformio run -e heltec-wireless-tracker
```

常見輸出：

- `.pio/build/heltec-wireless-tracker/firmware.bin`
- `.pio/build/heltec-wireless-tracker/firmware.factory.bin`

HermesX 顯示版號目前使用 `HXB_C0.3.2`；OTA / 韌體檔名會帶有建置時間，例如：

```text
HXB_C0.3.2_YYYYMMDD_HHMM
```

---

## 應用場景

- 民防與社群通訊
- 山域、野外與場域活動定位
- 離線環境下的同組位置點名
- Meshtastic 教學與實驗
- 自主 LoRa Mesh 通訊部署

![DSC00211](https://hackmd.io/_uploads/ByP9fR7Bbg.jpg)

---

## 畫廊

<img src="https://hackmd.io/_uploads/r1WQKUFcZx.jpg" alt="HermesX" width="400"/>
<img src="https://hackmd.io/_uploads/SkUnaIFcbe.jpg" alt="HermesX" width="400"/>
<img src="https://hackmd.io/_uploads/Skv2aLKqWg.jpg" alt="HermesX" width="400"/>
<img src="https://hackmd.io/_uploads/B1n20LYqZe.png" alt="HermesX" width="400"/>
<img src="https://hackmd.io/_uploads/Sk33CUFcZg.jpg" alt="HermesX" width="400"/>

---

## Source Code

HermesX 維持開源。韌體、文件與工具可在 GitHub 查看：

{%preview https://github.com/OLDWAYS069/HermesX %}

---

## Maintainer

> **OLDWAYS069**  
> HermesTrack 計畫發起人  
> GitHub: [github.com/OLDWAYS069](https://github.com/OLDWAYS069)

![Maintainer](https://hackmd.io/_uploads/HyazVAQrbe.jpg)

## Stats

![GitHub Repo stars](https://img.shields.io/github/stars/OLDWAYS069/HermesX)
![GitHub forks](https://img.shields.io/github/forks/OLDWAYS069/HermesX)
![GitHub watchers](https://img.shields.io/github/watchers/OLDWAYS069/HermesX)
