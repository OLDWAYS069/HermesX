# HermesX UI 垂直化切入筆記

## 目標
- 將 Heltec Wireless Tracker (160×80 TFT) 的 UI 從橫式改為直式，同時保持現有畫面功能。

## 既有設定來源
- 尺寸定義：`variants/heltec_wireless_tracker/pins_arduino.h`
  - `DISPLAY_WIDTH 160`
  - `DISPLAY_HEIGHT 80`
- 幾何組合：`variants/heltec_wireless_tracker/variant.h`
  - 透過 `SCREEN_ROTATE`、`TFT_WIDTH`、`TFT_HEIGHT`、`TFT_OFFSET_X/Y` 供顯示驅動使用。
- 顯示初始化：`src/graphics/TFTDisplay.cpp`
  - `setGeometry()` 在 `SCREEN_ROTATE` 情境下交換寬高。
  - `tft->setRotation()` 依照板子選擇 0/1/2/3，決定畫面實際方向。

## 垂直化流程
1. **確認解析度與偏移**
   - 調整 `DISPLAY_WIDTH/HEIGHT` 以及 `TFT_OFFSET_X/Y`，讓 `setGeometry()` 取得 80×160 並避免裁切。
2. **設定旋轉角度**
   - 在 `TFTDisplay::setup()`（約 `src/graphics/TFTDisplay.cpp:1169-1182`）為 Heltec Tracker 指定對應的 `setRotation()`。常見直式角度為 0 或 2，實機驗證觸控與字向。
3. **驗證 UI 排版**
   - `Screen.cpp` 透過 `display->getWidth()/getHeight()` 取得尺寸。若直式畫面仍擁擠，重構座標常數（如 `SCREEN_WIDTH * 0.63`）為單欄式配置。
4. **實機檢測**
   - 編譯 `platformio run -e heltec_wireless_tracker`，刷機後檢查是否存在裁切、顛倒或座標偏移，必要時回調偏移與字級。

## 後續建議
- 建立 `ScreenLayout` 結構保存橫/直式參數，避免散落常數。
- 在設定檔加入顯示方向選項，未來可在程式內切換 Landscape/Portrait。
- 使用線上 UI 原型工具（Figma、Penpot 等）快速規劃 80×160 版面，再轉入程式座標。
