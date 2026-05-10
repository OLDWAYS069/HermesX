---
title: ONLINE

---

# ONLINE

`ONLINE` 是 HermesX 裝置端的在線節點瀏覽頁。

它不是另外維護一份獨立名單，而是直接讀取目前裝置裡的 NodeDB，整理出最近仍有活動的節點，讓使用者不用打開手機也能快速確認附近有哪些裝置在線。

:::warning
本頁以目前 HermesX 裝置端 UI 為準。ONLINE 顯示的是 NodeDB 裡最近聽到的節點，不等於 GROUP，也不等於尋人模式的回報列表。
:::

---

## ONLINE入口

從 HermesX 主選單進入 `ONLINE` 後，畫面會顯示在線節點列表。

列表每一列會顯示：

- 節點名稱
- Node ID
- 最後聽到時間

節點名稱會優先使用 short name；如果沒有 short name，才會退回 long name 或 Node ID 後四碼。

如果目前沒有符合條件的節點，畫面會顯示 `沒有在線節點`。

---

## 在線判斷

ONLINE 會從 NodeDB 中篩選節點。

目前會排除：

- 本機節點
- 沒有 user 資料的節點
- 從未聽到過的節點
- 最後聽到時間超過 2 小時的節點

符合條件的節點會依照 `last_heard` 排序，最近聽到的節點排在前面。

這表示 ONLINE 看到的是「目前這台裝置記得、而且最近還有活動」的節點，不是伺服器名單，也不是手機 App 另外整理出來的清單。

---

## ONLINE DETAIL

在 ONLINE 列表點進單一節點後，會進入 `ONLINE DETAIL`。

目前 detail 內可看到：

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

如果本機與對方都有有效 GPS，`相對位置` 會顯示距離、方位文字與角度。  
如果其中一方沒有有效座標，位置欄位會顯示 `--`。

---

## MSG

`MSG` 是 ONLINE detail 裡的直接訊息入口。

選取 `MSG` 後，HermesX 會開啟對該節點的直接訊息編輯流程。  
這裡的 `MSG` 是「對目前選取節點發訊息」，不是單純進入 canned message 設定頁。

---

## TraceRoute

`TraceRoute` 用來測試到該節點的 LoRa 路由。

選取後，裝置會送出 TraceRoute request，並顯示：

```text
SEND

等待回應...
```

如果收到回應，會以 popup 顯示路由結果。  
如果等不到回應或送出失敗，畫面會顯示 `SEND FAIL`。

---

## MQTT節點限制

如果 ONLINE detail 裡的節點是 MQTT 來源，畫面會顯示：

```text
TraceRoute: --
```

這代表該節點不是目前可直接走 LoRa TraceRoute 的目標。  
如果仍然選取這一列，HermesX 會提示：

```text
LORA ONLY
```

這是刻意保留的限制，避免對 MQTT-only 節點假裝可以測 LoRa 路由。

---

## 跟GROUP的差別

`ONLINE` 看的是 NodeDB 裡最近在線的節點，適合快速瀏覽附近或曾經聽到的裝置。

`GROUP` 看的是通過 GROUP PIN / fingerprint 的同組節點，重點是 EMINFO、Heartbeat、EM 狀態與任務群組。  
如果你只是想知道「現在有哪些節點在線」，用 `ONLINE`。  
如果你要看「同組任務成員、EM 狀態、LastHB、電量或五線資料」，用 `GROUP`。

---

## 跟尋人模式的差別

`尋人模式` 的 `位置訊息` 也沿用 ONLINE 的節點瀏覽架構，但資料來源不同。

ONLINE 顯示最近 2 小時內 NodeDB 裡仍有活動的節點。  
尋人模式則先送出 `REQUEST: POS`，只顯示有回應最近一次位置點名、而且有有效座標的節點。

所以 ONLINE 適合看一般在線狀態；尋人模式適合在現場臨時點名位置。

---

## 節點資料太多時

如果 ONLINE 累積太多舊節點，可以到：

```text
設定 > 裝置管理 > 節點資料庫 > 重設資料庫
```

目前可選：

- `清除12hr未更新`
- `清除24hr未更新`
- `清除48hr未更新`
- `全部清除`

`全部清除` 會清掉其他節點資料，但保留本機節點。  
清理後，ONLINE 會依新的 NodeDB 狀態重新顯示。
