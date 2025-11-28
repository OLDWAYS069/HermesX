# Lighthouse 隨身節點行為調整（0.2.7）

## 目的
- 避免隨身裝置開機就對公頻廣播狀態，改為本地顯示。
- 保留原本中繼節點的廣播行為，可由編譯旗標開關。

## 變更
- 開機不再呼叫 `broadcastStatusMessage()`；改用 `screen->startAlert()` 在主螢幕顯示「Lighthouse Active/Silent/Idle」。
- `@Status` 指令同樣只顯示本地狀態（隨身模式）。
- 新增編譯旗標 `HERMESX_LH_BROADCAST_ON_BOOT`（預設 0）：設為 1 可恢復舊版開機廣播，供中繼節點使用。

## 使用方式
- 隨身節點：無需動作，預設不廣播。
- 中繼節點：在對應環境的 `build_flags` 增加 `-DHERMESX_LH_BROADCAST_ON_BOOT=1`，即可恢復開機/`@Status` 廣播。
