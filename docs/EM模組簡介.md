---
title: EM模組簡介

---

# EM模組簡介

`EM` 是 HermesX 的緊急模式。

它是給隊伍在現場發生失聯、受困、醫療需求或物資需求時使用的裝置端流程。  
使用者不需要打開手機，也可以直接從裝置進入緊急 UI、送出狀態、同步隊員資訊，並讓同組裝置一起進入緊急處理狀態。

:::warning
本頁以目前 HermesX GOV 整合版的現行行為為準。EM 模組目前與 `GROUP PIN`、`EMINFO`、`Heartbeat`、`GROUP > 節點列表` 一起運作。
:::

---

## EM模組在做什麼

EM 模組的核心目標是：

- 快速啟動同組裝置的緊急狀態
- 讓隊員回報 `受困 / 醫療 / 物資 / 安全`
- 用五線回報整理 `人 / 事 / 時 / 地 / 物`
- 用 EMINFO / Heartbeat 持續同步同組狀態
- 限制非必要傳輸，避免緊急期間被一般訊息干擾

假設某位隊員長時間沒有移動、失聯，或需要被確認狀況，管理者可以觸發 EMAC。  
同組裝置收到授權通過的 EMAC 啟動封包後，會進入 EM UI，要求使用者回覆目前狀態。

---

## EMAC啟動

EMAC 是把同組裝置帶入緊急模式的啟動流程。

目前主要控制封包走 HermesX emergency port，格式會收斂到 GROUP 授權：

```text
ACTIVATE: EMAC GROUP <pin>
```

同一組裝置必須設定相同 `GROUP PIN`，才會接受這個啟動訊號。  
這樣可以避免不同隊伍在同頻道上互相觸發緊急模式。

啟動後，裝置會進入 EM UI，並顯示類似 `請在90秒內回復` 的提示。

---

## EM UI

EM UI 是緊急模式中的主要操作畫面。

目前可選的主要回報包含：

- `受困`
- `醫療`
- `物資`
- `安全`

選擇 `受困 / 醫療 / 物資` 時，會進入五線回報流程。  
選擇 `安全` 時，會送出安全狀態並退出 EM UI。

EM UI 啟動後也會配合蜂鳴器與畫面提示，讓使用者能明確知道裝置已進入緊急狀態。

---

## 五線回報

五線回報是 EM 模組裡用來整理狀態的格式。

目前會引導使用者填寫：

- `人`
- `事`
- `時`
- `地`
- `物`

使用者選擇 `受困 / 醫療 / 物資` 後，不是只送出一個簡單標籤，而是可以進一步填入現場資訊。  
這些資訊後續會被 EMINFO 整理，讓其他同組裝置在 `各裝置狀態` 或 `GROUP DETAIL` 裡看到。

---

## 傳送內容

EM UI 主要狀態會送出對應封包：

- `受困` -> `STATUS: TRAPPED`
- `醫療` -> `NEED: MEDICAL`
- `物資` -> `NEED: SUPPLIES`
- `安全` -> `STATUS: OK`

`受困 / 醫療 / 物資` 會要求 ACK。  
`安全` 會標記本機安全，並退出 EM UI。

如果 EMAC 週期保護判斷有隊員逾時未回覆，會送出：

```text
STATUS: LOST
```

收到 `STATUS: LOST` 的裝置會被帶入 EM UI，避免失聯狀態被忽略。

---

## EM Tx Lock

緊急模式啟動後，HermesX 會限制一般傳輸，降低非緊急訊息佔用頻寬的機會。

目前 EM Tx lock 會保留 emergency port 的緊急封包，並允許必要手機文字指令，例如：

- `@EmergencyActive`
- `@ResetLighthouse`
- `@GoToSleep`
- `@HiHermes`
- `@Status`

這個設計是為了讓緊急期間的通訊更集中，不讓一般訊息干擾 EM 流程。

---

## GROUP與PIN

EM 模組現在與 `GROUP` 收斂在一起。

`GROUP PIN` 會影響：

- EMAC 啟動
- EMAC 解除
- 尋人模式位置點名
- EMINFO / Heartbeat 狀態同步

相關控制格式包含：

```text
ACTIVATE: EMAC GROUP <pin>
RESET: EMAC GROUP <pin>
REQUEST: POS GROUP <pin>
```

也就是說，同一組 PIN 的裝置才會被視為同一個任務群組。

設定路徑：

```text
GROUP > GROUP設定
```

這裡可以設定 `GROUP PIN A/B`、查看 PIN、調整 EMINFO，或執行 `解除EMAC`。

---

## EMINFO與Heartbeat

EMINFO 是 EM 模組的狀態同步層。

它會把同組裝置目前狀態同步出去，例如：

- 節點短名
- EM 狀態
- 電量
- 位置
- 五線回報中的 `地 / 物`
- 最後收到時間

Heartbeat 則用來判斷同組節點是否還在線。  
`GROUP > 節點列表` 會依 EMINFO / Heartbeat 顯示節點狀態：

- `在線`
- `延遲`
- `離線`

這些資料也會進入 `GROUP DETAIL`，讓管理者快速檢查 LastHB、電量、位置與 Node ID。

---

## 解除EMAC

如果需要結束 EMAC，可以在 GROUP 設定中執行：

```text
GROUP > GROUP設定 > 解除EMAC
```

目前解除封包格式是：

```text
RESET: EMAC GROUP <pin>
```

同組授權通過的裝置收到後，會退出 EMAC 狀態。  
本機執行解除時，也會同步退出 EM UI。

---

## 跟尋人模式的關係

尋人模式可以理解成比較輕量的 EMAC 相關功能。

它使用：

```text
REQUEST: POS GROUP <pin>
```

同組裝置收到後會回報一次位置。  
尋人模式不等於完整 EMAC，不會要求所有人進入緊急回報流程；它比較適合快速確認隊友位置。

---

## 現場使用建議

部署前建議先完成：

- 設定同組 `GROUP PIN`
- 開啟或確認 `EMINFO廣播`
- 確認 `Heartbeat週期`
- 用 `GROUP > 節點列表` 確認同組節點會出現
- 測試一次尋人模式位置回報
- 再測試 EMAC 進入與 `解除EMAC`

如果是需要擴大訊號覆蓋的場景，可以把其中一台裝置放在較高位置。  
空中平台或高點中繼的覆蓋通常會比貼地部署更好，但仍要先確認電源、固定方式與合法頻率設定。
