---
title: TraceRoute
---

# TraceRoute

`TraceRoute` 是 HermesX 裝置端用來測試 LoRa 路由的功能。

它可以從 `ONLINE DETAIL` 或 `GROUP DETAIL` 直接對目前節點發起，也可以從主選單的獨立 `TraceRoute` 頁面操作。獨立頁面目前以「先綁定常用節點，再快速 TR」為主，適合現場反覆測試固定幾台裝置的路由狀態。

## 入口

從主選單進入 `TraceRoute` 後，會看到兩個主要項目：

- `綁定節點`
- `TraceRoute`

`綁定節點` 用來從目前在線的節點中選擇常用目標。

`TraceRoute` 則只顯示已綁定的節點，讓使用者可以快速送出 TR，不需要每次都重新從在線清單裡找目標。

## 綁定節點

進入 `綁定節點` 後，畫面會列出目前在線中的節點。

這裡的在線判斷與 `ONLINE` 頁面一致：HermesX 會從本機 NodeDB 中找出最近仍有活動的節點。即使對方還沒有完整 NodeInfo / User 資料，只要最近聽到過，仍會用 Node ID fallback 顯示。

在節點上操作：

- 短按：顯示該節點的 `LongName`、`role`、`最近一次聽到`
- 長按 1 秒：跳出 `是否綁定？` 確認框

確認框提供：

- `是`：綁定該節點
- `否`：取消綁定

一次可以綁定多個節點。已綁定的節點在清單上會標示為 `已綁`。

## 快速 TraceRoute

進入 `TraceRoute` 後，畫面只會顯示已綁定的節點。

在已綁定節點上操作：

- 短按：立即送出 TraceRoute request
- 長按 1 秒：解除該節點的綁定

短按送出後，HermesX 會顯示既有 TraceRoute popup，先提示正在等待回應，收到結果後顯示路由與訊號資訊。

長按解除綁定後，畫面會顯示 `已解除綁定`。解除後該節點不會再出現在快速 TraceRoute 清單中；如果之後還需要快速 TR，可以重新到 `綁定節點` 綁定。

## TraceRoute 結果

TraceRoute 結果 popup 會顯示本機收到封包的訊號資訊，以及路由中每一跳的訊號資料。

畫面中的重點欄位包含：

- `本機收到`：本機收到 TraceRoute 回覆封包時的 RSSI / SNR
- `去程訊號`：往目標節點方向的 hop 訊號
- `回程訊號`：從目標節點回來方向的 hop 訊號
- `訊號SNR`：每一跳的 SNR 數值

這些數值用來判斷節點與節點之間的 LoRa 路由品質，不是單純的 NodeDB 節點資訊。

## MQTT 節點限制

如果節點最近來源是 MQTT，HermesX 不會假裝它可以走 LoRa TraceRoute。

遇到 MQTT-only 節點時，畫面會顯示或提示：

```text
LORA ONLY
```

這代表目前不能對該節點送出合理的 LoRa route request。這是刻意保留的防呆，避免使用者把 MQTT 轉送節點誤判成可直接測 LoRa 路由的目標。

## 跟 ONLINE 的關係

`ONLINE` 是節點瀏覽頁，重點是查看目前本機 NodeDB 裡有哪些最近在線的節點。

`TraceRoute` 是路由測試頁，重點是對指定節點送出 LoRa route request。

獨立 `TraceRoute` 頁的 `綁定節點` 清單會沿用 ONLINE 的在線判斷；但綁定完成後，快速 TR 清單只顯示已綁定的節點。

## 操作摘要

| 頁面 | 操作 | 行為 |
| --- | --- | --- |
| `綁定節點` | 短按節點 | 查看 `LongName`、`role`、`最近一次聽到` |
| `綁定節點` | 長按節點 1 秒 | 跳出 `是否綁定？` |
| `TraceRoute` | 短按已綁定節點 | 立即送出 TraceRoute |
| `TraceRoute` | 長按已綁定節點 1 秒 | 解除該節點綁定 |

