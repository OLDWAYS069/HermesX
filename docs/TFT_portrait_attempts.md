# Heltec 無線追蹤器 TFT 直式顯示調整紀錄

實際測試確認這片板子仍採用 ST7735S 160×80 彩色 TFT。以下紀錄我們把 UI 改成直向 (80×160) 時做過的調整與結論。

---

## 嘗試 1：直接把 `DISPLAY_WIDTH/HEIGHT` 改成 80×160
- **修改內容**：pins 檔強制定義 80×160，停用 `SCREEN_ROTATE`。
- **結果**：在 `Turn on screen` 時就 SPI timeout、重啟，因為 LovyanGFX 仍照 160×80 寫入導致越界。
- **結論**：硬體仍是 160×80，不能直接把尺寸換成 80×160。

## 嘗試 2：保持 160×80，只呼叫 `setRotation(1)`
- **修改內容**：保留原幾何，開機後 `tft->setRotation(1)`。
- **結果**：畫面雪花＋黑塊，因為 `_colstart/_rowstart` 沒同步調整。
- **結論**：單靠 `setRotation()` 不夠，旋轉後 offset 也要重算。

## 嘗試 3：在 `drawPixel()` 手動換算座標
- **修改內容**：在雙層迴圈裡把 `(x, y)` 轉成 `(hw_x, hw_y)` 再畫。
- **結果**：畫面幾乎全黑且很快重啟，因為容易寫錯範圍。
- **結論**：手動換算脆弱又難維護，不建議。

## 嘗試 4（目前採用）：交給 LovyanGFX 做旋轉
- **修改內容**：保持 `DISPLAY_WIDTH=80 / DISPLAY_HEIGHT=160`。在 `TFTDisplay.cpp` 的 Heltec 分支設定 `cfg.offset_rotation = 1`、並呼叫 `tft->setRotation(0)`。另外把 `cfg.offset_x/y` 在旋轉時互換 (`offset_x = TFT_OFFSET_Y`, `offset_y = TFT_OFFSET_X`)。
- **結果**：避免了 SPI timeout，也不用手動換算座標。只要微調 `TFT_OFFSET_X/Y` 就能貼齊邊界。
- **結論**：把旋轉交給 LovyanGFX 是最穩定的方案。

---

## 實作注意事項
1. pins / variant 維持官方的 160×80 設定 (`DISPLAY_WIDTH=80`, `DISPLAY_HEIGHT=160`, `TFT_OFFSET_X=26`, `TFT_OFFSET_Y=0`)。
2. `TFTDisplay.cpp` 中針對 Heltec：
   ```cpp
   cfg.offset_x = TFT_OFFSET_Y;
   cfg.offset_y = TFT_OFFSET_X;
   cfg.offset_rotation = 1;
   ...
   tft->setRotation(0);
   ```
3. 如仍有邊界偏移，可在 `TFT_OFFSET_X` 24~28、`TFT_OFFSET_Y` 0~2 間 ±1 微調。

依照以上設定，即可在保留原本 ST7735 控制流程的前提下，讓 Heltec Wireless Tracker 顯示直向 UI。

---

### 2025-10-16 更新

- 測試實機後，將 `TFT_OFFSET_Y` 從 0 調整為 **2**，可把整個畫面往下移，消除右上角貼齊的黑框。
- 針對 `_V1_0` 變體同樣套用 `TFT_OFFSET_Y = 2`，保持兩個 Heltec 變體一致。
- 其他設定保持不變；如後續仍看到邊緣偏移，請以此為基準在 0~2 間微調。
- 試圖將其回復成官方版本，但編譯持續錯誤


### 註記
- 尚未確定黑塊及雪花框發生之原因，故以上關於此的論述有待證實
- 關於板子的長寬高，官方給的說法是一塊128x64的OLED
- 但在專案當中找到的蛛絲馬跡卻告訴我們她是160X80，所以到底真相為何有待確認