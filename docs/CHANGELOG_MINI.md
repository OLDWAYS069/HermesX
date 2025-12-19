# HermesX Mini Change Log
（每次只寫極簡亮點，便於快速回顧）

## 2025-12-18
- TAK/TAK_TRACKER：預設停用 HermesX UI（NeoPixel LED 動畫 + HermesX 音效），讓 TAK profile 保持低可視/低干擾。
- TAK/TAK_TRACKER：同時抑制 `LED_PIN` heartbeat 與 AmbientLighting（RGB/NeoPixel 氛圍燈），盡量確保整機不發光。
- Emergency override：等待 SAFE 的 EM 狀態允許 HermesX UI 啟用，用於顯示/提示 EM 流程。
- CannedMessage：在 HermesX UI runtime 停用時自動回退到原本的文字 UI，避免畫面空白。

## 2025-11-29
- Lighthouse EMACT：支援全形＠的 `@EmergencyActive:<pass>`，並在拒絕/通過時寫 log（密碼/白名單狀態）。
- 開機版號：改用 `HXB_<語意版號><git>` 的 APP_VERSION_DISPLAY，從分支名推語意版號，方便現場辨識。

## 2025-10-30
- 恢復 HermesX CN12 混排的 ASCII 分流，英數回到半形寬度，中文仍交由 CN12 繪製。
- CannedMessage 清單與輸入框改用混排渲染，完整保留 UTF-8 中文並原樣傳送。

## 2025-10-27
- 紀錄 HermesX BootHold 行為：若畫面出現 `Resuming...`，表示由 EXT1/RTC 喚醒，仍須持續按住電源鍵超過 `BUTTON_LONGPRESS_MS` 才會開機；提前放開仍會依設計回到深睡眠。
- 目前已知問題：雖可喚醒開機，但約 5 秒後會再度關機，初判為門檻設定異常。

## 2025-10-29
- 恢復 TFT CN12 中文快路徑，改善彩色面板上的字元性能與顏色一致性。
- Removed the custom TFT fast-path, reverting to the stock drawing pipeline; fixes compass overlay corruption.
