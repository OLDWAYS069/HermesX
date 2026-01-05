# HermesX 開機雙頁顯示時序紀錄（b0.3.0, Meshtastic 2.7.15 基底）

## 問題
- 開機頁（官方→HermesX）顯示時長不一致：官方頁可能停留 4~5 秒，HermesX 頁僅 0.5 秒；或官方頁過短、HermesX 過長。log 顯示切換/結束時的 elapsed 超過 `logo_timeout` 設定。

## 追查
- `Screen::setup()` 初始 `alertFrames` 就指向 HermesX，導致官方頁被覆蓋。已改回官方頁，但切換仍受初始化耗時影響。
- `runOnce()` 以 `logo_timeout/2` 切換 HermesX，但啟動流程（GPS/Radio init 等）可能推遲 `runOnce()`，讓官方頁實際停留更久，HermesX 頁顯示過短。
- 嘗試以 `bootScreenStartMs`/`switchDueMs` 計時仍受排程/初始化阻塞影響。

## 現行解法（暫行）
- 在 `Screen::setup()` 同步跑完兩段開機頁：
  - 立即 `ui->update()` 繪製官方頁，`delay(half)`。
  - 切換 HermesX boot 畫面、`update()`，再 `delay(half)`。
  - 結束 boot 畫面（`stopBootScreen()`），不再依賴 `runOnce()` 時序。
- `logo_timeout` 目前 2000ms（官方 ~1s + HermesX ~1s），需要可再調整。

## 待改進
- 改用非阻塞的定時或精準計時，避免 setup 中的 delay 阻塞其他初始化。
- 若需保留 runOnce 流程，需確保 boot 畫面切換獨立於 Radio/GPS 初始化時間，可用獨立計時或高優先序任務。
- 允許配置官方/自訂頁顯示時長或跳過官方頁。

