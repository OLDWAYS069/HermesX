# HermesX Mini Change Log
（每次只寫極簡亮點，便於快速回顧）

## 2025-12-26
- 整合 LoBBS/LoDB/LoFS 子模組：補齊 nanopb 產物、PlatformIO include/filter 並在 Modules 註冊 LoBBS，預設會編入。
- LoBBS 中文化與 UTF-8 放寬：帳號/密碼不再限 ASCII，指令回覆改中文且新用戶登入時推送中文速查。
- HermesX UI/字型條件編譯：無螢幕時自動排除 HermesX/中文字型，減少 headless 佈建負擔。
- 歡迎訊息：新增 WelcomeModule 監聽位置更新，預設 20km 內新節點會在頻道廣播歡迎；/welcome on|off|radius <公里> 可調整。

## 2025-12-27
- LoBBS 可在 Repeater 角色運行：Repeater 分支也建立 TextMessage/LoBBS/Welcome，保留 DM 與歡迎訊息功能。
- 啟動/收包日誌加強：Modules 會印出角色與 LoBBS/L0FS/L0DB 是否編入；LoBBS 啟動與收到 DM 時印 Info，便於確認模組活著。
- 子模組改為內嵌：lobbs/lodb/lofs 直接收進倉庫，PlatformIO 移除 lodb/diagnostics.cpp 編譯以避免缺少 nanopb 產物。

## 2025-11-29
- Lighthouse EMACT：支援全形＠的 `@EmergencyActive:<pass>`，並在拒絕/通過時寫 log（密碼/白名單狀態）。
- 開機版號：改用 `HXB_<語意版號><git>` 的 APP_VERSION_DISPLAY

## 2025-10-30
- 恢復 HermesX CN12 混排的 ASCII 分流，英數回到半形寬度，中文仍交由 CN12 繪製。
- CannedMessage 清單與輸入框改用混排渲染，完整保留 UTF-8 中文並原樣傳送。

## 2025-10-27
- 紀錄 HermesX BootHold 行為：若畫面出現 `Resuming...`，表示由 EXT1/RTC 喚醒，仍須持續按住電源鍵超過 `BUTTON_LONGPRESS_MS` 才會開機；提前放開仍會依設計回到深睡眠。
- 目前已知問題：雖可喚醒開機，但約 5 秒後會再度關機，初判為門檻設定異常。

## 2025-10-29
- 恢復 TFT CN12 中文快路徑，改善彩色面板上的字元性能與顏色一致性。
- Removed the custom TFT fast-path, reverting to the stock drawing pipeline; fixes compass overlay corruption.
