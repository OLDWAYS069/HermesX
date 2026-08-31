# HXB_C0.4.0_20260901_0049

HermesX CIV C0.4.0 是一次 UI 架構重整版本。原本集中於 `Screen.cpp` 的功能狀態、輸入判斷與繪圖，已按功能拆分為 Model、Controller、Renderer、Presenter 與 StateCollector；`Screen` 保留上游畫面生命週期、硬體取樣、頁面協調及副作用 dispatch。

## 主要變更

- 新增 `HermesXUiInputRouter`，依 overlay 與頁面優先權為每次輸入選出唯一接收目標，避免底層頁面同時處理旋鈕或按鍵事件。
- Message、TraceRoute、Online／Finder／GROUP Node Browser、FastSetup、Home、GPS、TAK Mode、Low Memory、Emergency Confirm 與 Rotary Lock 建立獨立 UI 責任邊界。
- Home／GPS Direct TFT 路徑拆出狀態收集、Model／Cache、Controller、Presenter、Renderer、Neon workspace 與共用繪圖 primitive。
- 新增 `HermesXPreferences` 與 `HermesXTraceRouteBindings` service，將偏好與 TraceRoute 綁定持久化移出畫面繪圖狀態。
- Emergency Confirm 與 Rotary Lock 的狀態、倒數、選擇、timeout 與輸入轉換移出 `Screen`，既有公開 API 維持不變。
- 新增 `docs/HermesX_新版專案架構.md`，記錄分層、輸入優先權、功能檔案對照、服務邊界與擴充規則。

## 相容性與驗證

- 目標板：Heltec Wireless Tracker。
- CIV build；關閉 EMAC，保留 GROUP 與尋人模組。
- `platformio run -e heltec-wireless-tracker -j 4` 全量建置成功。
- 內嵌 `APP_HERMES_VERSION`：`HXB_C0.4.0_20260901_0049`。
- RAM：34.7%（113,560 / 327,680 bytes）。
- Flash：90.8%（3,033,709 / 3,342,336 bytes）。
- OTA SHA256：`7342f72065b62cc1fc733d08930c56b66296e71c6f854a63ef13b46f14e4294b`。
- Factory SHA256：`6de40efcaf3044d2be69d5859df39648e1716f6629c3abb7c740e966a4758786`。

編譯成功表示所有新元件已整合進韌體；旋鈕長按、popup timeout、Emergency 倒數、Radio／GPS 與重開機保存仍應在實機進行完整操作回歸。

## 下載檔案

- OTA：`HXB_C0.4.0_20260901_0049.bin`
- Factory：`HXB_C0.4.0_20260901_0049.factory.bin`
