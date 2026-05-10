---
title: GROUP設定

---

# GROUP設定

`GROUP` 是 HermesX 用來把尋人模式、EMAC 與 EMINFO/Heartbeat 收斂在一起的配對機制。

簡單說，同一組裝置要使用同一組 `GROUP PIN`，才會互相接受尋人請求、緊急啟動、EMAC 解除與狀態同步。  
這不是一般聊天群組，而是現場任務用的授權範圍。

:::warning
本頁以目前 HermesX 裝置端 UI 為準。現行主選單中的 `GROUP` 會先進入 `GROUP設定 / 節點列表`，不是直接進入密碼頁。
:::

---

## GROUP入口

從 HermesX 主選單進入 `GROUP` 後，會先看到兩個主要入口：

- `GROUP設定`
- `節點列表`

`GROUP設定` 負責同組 PIN、EMINFO 與 EMAC 相關設定。  
`節點列表` 則顯示已通過 GROUP 授權、並且送出 EMINFO 或 Heartbeat 的同組裝置。

---

## GROUP PIN

`GROUP PIN` 是 GROUP 的核心。

目前可設定兩組 PIN：

- `GROUP PIN A`
- `GROUP PIN B`

裝置送出或接收以下控制封包時，會用 GROUP PIN 進行授權：

- `ACTIVATE: EMAC GROUP <pin>`
- `RESET: EMAC GROUP <pin>`
- `REQUEST: POS GROUP <pin>`

也就是說，尋人模式、EMAC 啟動、EMAC 解除與位置點名，都會被收斂到同一組 GROUP 授權概念底下。

如果裝置沒有設定正確的 GROUP PIN，就不應該被同組流程帶入，也不應該接收不同隊伍的 EMINFO 狀態。

---

## 查看GROUP PIN

`查看GROUP PIN` 可以直接在裝置上查看目前的 `PIN A` 與 `PIN B`。

這個頁面是給現場配對使用的。  
如果有新裝置要加入同一組，先確認雙方 PIN 是否一致，再測試尋人模式或 EMINFO 狀態同步。

---

## EMINFO設定

`EMINFO設定` 控制同組裝置之間的狀態同步。

目前包含：

- `EMINFO廣播`
- `EMINFO週期`
- `Heartbeat週期`
- `離線門檻`
- `附帶電量`

`EMINFO` 用來同步節點目前的 EM 狀態、五線回報資訊與位置相關資料。  
`Heartbeat` 則用來判斷節點是否還在線，讓 `節點列表` 可以顯示 `在線 / 延遲 / 離線`。

如果開啟 `附帶電量`，節點 detail 內也會顯示對方回報的電量百分比。

---

## 解除EMAC

`解除EMAC` 會送出 EMAC 解除訊號。

目前使用的控制格式是：

```text
RESET: EMAC GROUP <pin>
```

同組授權通過的裝置收到後，會退出 EMAC 狀態。  
這個項目保留在 `GROUP設定` 裡，是因為 EMAC 的啟動、解除與狀態同步都已經改由 GROUP PIN 管理。

---

## 節點列表

`節點列表` 不是一般 ONLINE 清單。

它只列出有送出 `EMINFO` 或 `Heartbeat`，而且通過 GROUP fingerprint 的同組節點。  
清單上會顯示節點名稱、EM 狀態，以及目前判斷出的在線狀態。

常見狀態包含：

- `在線`
- `延遲`
- `離線`

如果畫面顯示 `沒有已配對節點`，通常代表目前還沒有收到同組裝置的 EMINFO 或 Heartbeat。

---

## GROUP DETAIL

在 `節點列表` 點進單一節點後，會進入 `GROUP DETAIL`。

目前 detail 內可看到：

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

`MSG` 可以對該節點開啟直接訊息。  
`TraceRoute` 可以測試 LoRa 路由；如果該節點是 MQTT 來源，畫面會顯示 `TraceRoute: --`，操作時會回覆 `LORA ONLY`，避免對 MQTT-only 節點假裝可以走 LoRa 路由。

---

## 跟ONLINE的差別

`ONLINE` 看的是目前 NodeDB 裡的節點狀態，適合瀏覽一般在線節點。

`GROUP` 則是任務群組視角，只關心同組授權與 EMINFO/Heartbeat 狀態。  
因此 `GROUP` 更適合用來看「誰在同一組、誰進入 EM、誰還在線、誰最後一次 Heartbeat 是多久以前」。

---

## 現場使用建議

部署前先設定好同一組 `GROUP PIN`，再讓同組裝置互相測試：

- `查看GROUP PIN` 確認 PIN 一致
- 使用尋人模式測試 `REQUEST: POS`
- 進入 EMAC 測試 EMINFO/Heartbeat 是否出現在 `節點列表`
- 點入 `GROUP DETAIL` 確認狀態、電量、位置與 LastHB

如果要分隊使用，請讓不同隊伍使用不同 GROUP PIN，避免尋人、EMAC 或狀態同步互相干擾。
