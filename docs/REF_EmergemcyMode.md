# EM MODE 架構（現行實作）

> 此文件為快速摘要；完整細節請見 `docs/REF_EmergencyMode.md`。

## 入口
- 收到 `@EmergencyActive` 且通過白名單或 passphrase。
- Lighthouse 進入 EM，彈出 EM UI（banner 預設「請在60秒內回復」）。
- 若來源為手機（from==0）立即啟用 EM Tx lock；否則回送 Emergency OK，ACK 後啟用 EM Tx lock。

## UI
- 專用模組 `HermesXEmUiModule` 全螢幕 overlay。
- 標頭：⚠️ + 「人員尋回模式啟動」。
- 清單：`受困 / 醫療 / 物資 / 安全`。
- 右側欄：倒數 + 狀態（等待中 / 已傳送 / 傳送成功請原地待命 / 傳送失敗）。
- 底部：banner 文字。

## 行為
- 送出封包：port 300 broadcast；TRAPPED/MEDICAL/SUPPLIES 需 ACK；OK 不需 ACK 並停止自動 SOS。
- 自動 SOS：60 秒寬限後送 `SOS`，之後每 60 秒重送。
- 只要送出過一次即停止蜂鳴器。

## EM Tx lock
- 啟用時僅允許 emergency port。
- 允許手機文字指令：`@EmergencyActive/@ResetLighthouse/@GoToSleep/@HiHermes/@Status`。
