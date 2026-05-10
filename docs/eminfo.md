---
title: EMINFO

---

# EMINFO

`EMINFO` 是 HermesX 在緊急狀態下用來同步同組裝置狀態的擴充機制。

它的目標不是取代 SOS 或五線回報，而是讓同一組 GROUP 裝置可以持續知道彼此目前是什麼狀態、最後一次 Heartbeat 是多久以前、是否還在線，以及有沒有附帶電量與位置資訊。

:::warning
本頁以目前 HermesX 裝置端 UI 與程式碼為準。EMINFO 目前跟 `GROUP PIN` 綁在一起，只有通過 GROUP fingerprint 的同組資料會被顯示。
:::

---

## EMINFO是什麼

EMINFO 可以理解成 EMAC / 緊急模式底下的狀態同步封包。

它目前會同步：

- 節點短名
- EM 狀態
- 電量
- 位置資料
- 五線回報中的 `地` / `物`
- 最後收到時間

這些資料會被用在：

- `GROUP > 節點列表`
- `GROUP DETAIL`
- EM UI 內的 `各裝置狀態`

因此 EMINFO 比較像「狀態面板的資料來源」，不是使用者手動發出去的一則文字訊息。

---

## GROUP授權

EMINFO 目前使用 v2 格式，封包內會帶 GROUP fingerprint。

本機收到 EMINFO 或 Heartbeat 時，會先檢查 fingerprint。  
如果 fingerprint 不屬於目前設定的 `GROUP PIN A/B`，這筆資料會被忽略。

這表示不同隊伍即使在同一個 LoRa 頻道上，也不應該互相污染 EMINFO 狀態列表。

---

## EM狀態

EMINFO 會把本機目前 EM 狀態同步出去。

目前狀態包含：

- `IDLE`：待命
- `TRAPPED`：受困
- `MEDICAL`：醫療
- `SUPPLIES`：物資
- `SAFE`：安全

在裝置 UI 上，這些狀態會顯示成中文，例如 `受困`、`醫療`、`物資`、`安全` 或 `待命`。

---

## Heartbeat

`Heartbeat` 是 EMINFO 的在線判斷來源。

EMINFO 負責同步狀態；Heartbeat 則負責告訴其他同組裝置「我還在」。  
目前預設 Heartbeat 週期是 `5s`，也可以在 `GROUP設定 > EMINFO設定` 裡調整。

節點在線狀態會依 Heartbeat 判斷：

- Heartbeat 還在週期內：`在線`
- 超過一個週期但未超過離線門檻：`延遲`
- 超過離線門檻：`離線`

如果 Heartbeat 關閉或還沒有收到 Heartbeat，系統會暫時退回 EMINFO 最近收到時間判斷。

---

## EMINFO設定

設定路徑：

```text
GROUP > GROUP設定 > EMINFO設定
```

目前可調整：

- `EMINFO廣播`
- `EMINFO週期`
- `Heartbeat週期`
- `離線門檻`
- `附帶電量`

`EMINFO廣播` 控制是否送出狀態同步。  
`EMINFO週期` 控制狀態封包多久送一次。  
`Heartbeat週期` 控制在線心跳多久送一次。  
`離線門檻` 控制幾個 Heartbeat 週期後判定離線。  
`附帶電量` 開啟後，EMINFO / Heartbeat 會附帶本機電量百分比。

---

## 節點列表

`GROUP > 節點列表` 會列出通過 GROUP fingerprint，且有送出 EMINFO 或 Heartbeat 的節點。

清單上會顯示：

- 節點名稱
- EM 狀態
- 在線狀態

如果沒有任何同組裝置送出資料，畫面會顯示 `沒有已配對節點`。

---

## GROUP DETAIL

點進單一節點後，`GROUP DETAIL` 會顯示更完整的資料。

目前包含：

- `MSG`
- `TraceRoute`
- `EM狀態`
- `在線`
- `LastHB`
- `電量`
- `LongName`
- `地`
- `物`
- `經`
- `緯`
- `高度`
- `Node`

`MSG` 可直接對該節點發訊息。  
`TraceRoute` 可測 LoRa 路由；如果該節點是 MQTT 來源，會顯示 `TraceRoute: --` 並提示 `LORA ONLY`。

---

## 各裝置狀態

EM UI 裡的 `各裝置狀態` 也使用 EMINFO 資料。

這裡會把五線回報與節點狀態整理成比較適合緊急模式閱讀的格式，例如：

- `人`
- `事`
- `時`
- `地`
- `物`
- 經緯度
- 高度
- 相對位置

如果本機與對方都有有效 GPS，還可以看相對距離、方位與角度。

---

## 跟五線回報的差別

五線回報是使用者在 EM UI 裡主動填寫的狀態回報，重點是 `人 / 事 / 時 / 地 / 物`。

EMINFO 則是把這些狀態整理後週期性同步給同組裝置，讓其他人可以在 GROUP 或 EM UI 裡看到。  
所以五線回報偏「輸入內容」，EMINFO 偏「同步與顯示」。

---

## 跟ONLINE的差別

`ONLINE` 看的是 NodeDB 裡最近 2 小時有活動的節點。

`EMINFO` 看的是同組 GROUP 裝置的緊急狀態資料。  
如果你只是要看誰在線，用 `ONLINE`。  
如果你要看誰進入 EM、目前狀態、LastHB、電量與五線資訊，用 `GROUP` / `EMINFO`。

---

## 現場使用建議

部署前先確認同組裝置的 `GROUP PIN` 一致。

建議測試流程：

- 開啟 `EMINFO廣播`
- 確認 `Heartbeat週期` 不為關閉
- 進入 EMAC 或 EM UI
- 到 `GROUP > 節點列表` 確認同組節點出現
- 點進 `GROUP DETAIL` 檢查 EM 狀態、LastHB、電量與位置

如果節點沒有出現，先檢查 GROUP PIN 是否一致，再確認對方是否真的有進入 EM 狀態並開始送出 EMINFO / Heartbeat。
