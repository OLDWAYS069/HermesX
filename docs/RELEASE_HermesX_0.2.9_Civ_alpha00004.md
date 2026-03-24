# HermesX Firmware `HermesX_0.2.9_Civ_alpha00004`

預先發佈版

`HermesX_0.2.9_Civ_alpha00004` 是 HermesX `0.2.9 Civ` 系列的 Alpha 釋出，基於 Meshtastic 韌體架構，主目標板為 `heltec-wireless-tracker`。

這一版延續 HermesX 一貫的方向，重點不是把設定項目全塞進裝置，而是把節點操作整理成更適合單手操作、外勤可直覺使用的本機終端。相較上游原版介面，這版更強調首頁辨識、訊息處理、快捷設定、聲光回饋與現場直接操作的節奏。

## 這版重點

- HermesX 主頁與整體 UI 佈局持續以裝置操作為中心，讓使用者優先看到時間、狀態、電量與主要互動入口。
- Fast Setup 持續擴充，將常用的 UI、節點、GPS、電源與罐頭訊息相關設定集中到裝置端。
- 訊息體驗持續整理，包含 `Recent Send`、訊息 detail 與通知入口，降低現場查看與回應訊息的成本。
- 聲光回饋持續整合，LED 與蜂鳴器會對送達、失敗、接收、待機與模式切換提供較明確的提示。
- 目前預設走 `Civ` 配置，通用編譯旗標包含 `HERMESX_CIV_DISABLE_EMAC=1` 與 `MESHTASTIC_EXCLUDE_LIGHTHOUSE=1`。

## 本版更新摘要

### 穩定度與記憶體

- 調整 Home 與 GPS 畫面的繪圖暫存用法，降低 direct-TFT 霓虹效果對常駐 RAM 的壓力。
- 修正 scratch buffer 初版過小時，首頁時鐘區可能出現的殘影與錯位。
- 現場量測顯示 free heap 較前版已有回升，這版也延續相關穩定度調整。
- 新增低記憶體提醒 popup，當可用記憶體過低時，會提示使用者前往 `裝置管理 > 節點資料庫` 進行清理。

### 訊息與互動

- 修正訊息 Banner 短按後，無法正確跳進 `Recent Send / All Messages` 的問題，現在會優先導向訊息頁。
- `Recent Send` 列表摘要改為 UTF-8 安全渲染，中文與混合文字顯示更穩定。
- `Recent Send` detail 頁新增上下滾動與 scrollbar，長訊息可讀性較前版更好。
- 新訊息 popup 顯示時間延長為 `5 秒`，現場辨識性更高。

### 裝置管理與 UI 設定

- Fast Setup 中的 `裝置設定` 已整理為 `裝置管理`。
- 新增 `節點資料庫 > 重設資料庫`，可手動清除 `12hr / 24hr / 48hr` 未更新節點。
- `UI設定` 新增 `旋鈕對調`，旋鈕方向不再硬編碼，已同步套用到 HermesX UI 與 Canned Message 操作。
- `狀態燈亮度 = 0` 現在只會關閉 LED，不再連帶把蜂鳴器一併靜音。
- 狀態燈亮度已改為獨立保存，重開機後可保留設定值。

### 畫面與使用感受

- GPS 頁面視覺持續優化，包含標題、座標霓虹層次與地圖光暈效果。
- HermesX 首頁、訊息、提示與快捷操作頁的畫面節奏持續統一，減少使用時的割裂感。
- 潛行模式的提示音與回饋進一步調整，進入時會給出更明確的狀態提示。

## 使用建議

- 本版主要建議刷入目標為 `heltec-wireless-tracker`。
- 若你要體驗 HermesX 的完整 UI/UX，請優先以這個板型為準。
- 這是一版 Alpha，適合測試、現場驗證與回報問題，不建議直接視為最終穩定版。

## 已知限制

- 這個 `Civ` 版本目前不是 Emergency/Lighthouse 主軸版本，`EMAC` 與 `Lighthouse` 相關功能不在本版預設能力範圍內。
- repo 仍保留上游 Meshtastic 的多板型結構，但 HermesX 的主要體驗與驗證重心仍是 `heltec-wireless-tracker`，其他板型不代表具備相同完整度。

## 目前已知問題

- HermesX 訊息 popup 的 `Press` 直接開啟最新訊息 detail 仍有時序問題，這一輪尚未完全修復。
- 在節點資料累積較多、記憶體壓力較高的情況下，系統雖已有改善，但仍可能需要手動清理節點資料庫來維持穩定度。
- 電源鍵長按喚醒流程在歷史上曾出現特定情境異常；目前流程已加上喚醒 gate 與動畫回饋，但若遇到睡眠喚醒或長按判定異常，仍建議保留 log 進一步確認。

## 建置資訊

- Base: Meshtastic firmware upstream architecture
- Primary target: `heltec-wireless-tracker`
- Release tag: `HermesX_0.2.9_Civ_alpha00004`

## 回報方式

如果你在這版遇到問題，建議回報以下資訊：

- 使用板型
- 刷入檔案名稱
- 問題發生前的操作流程
- 是否為冷開機、睡眠喚醒或長時間待機後發生
- 若有 log，請一併附上 monitor 輸出
