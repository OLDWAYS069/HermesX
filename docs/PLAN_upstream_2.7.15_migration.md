# HermesX 升級與完整搬移計劃

本文件規劃 HermesX 從既有分支升級到上游 `2.7.15` 的完整搬移方式，目標是在保留 HermesX 原本整套 UI 設計與功能的前提下，避免把舊分支裡已知或潛在的 heap / BLE / queue / allocator 問題重新帶回來。

## 目標

- 以 upstream `2.7.15` 作為新基底。
- 完整搬移 HermesX 原本 UI 設計。
- 完整搬移 HermesX 原本功能與模組。
- 保留上游 `2.7.x` 後段對 heap / BLE / PhoneAPI / packet allocator 的穩定性修正。
- 避免直接覆蓋 core 通訊與記憶體管理檔案。

## 核心原則

- 升級策略不是「先升版，再把舊分支整包蓋回去」。
- 正確策略是「以 `2.7.15` 為底，逐層搬移 HermesX UI 與功能」。
- 所有與 BLE、PhoneAPI、Router、MeshService、MQTT、packetPool 有關的改動，都必須人工 merge。
- HermesX 的視覺、字型、畫面、互動、文案、偏好設定可以完整搬移，但不可因此回退上游穩定性修正。

## 已知風險背景

目前本地分支與上游 heap 問題相關的差異，至少包括：

- `NimbleBluetooth` 與 `NRF52Bluetooth` 仍存在 `new BluetoothStatus(...)` 用法，可能造成持續性 heap leak。
- embedded target 目前仍使用動態 packet pool，而上游後續改為 static memory pool。
- phone-facing queues 目前仍使用動態 `PointerQueue`，而上游後續改為 `StaticPointerQueue`。
- 本地雖已包含部分修正，例如 `lastToRadio` 清理與 `NextHopRouter` 釋放邏輯，但整體仍未完全對齊上游 `2.7.15` 穩定性路徑。

因此，若未經篩選直接把舊 HermesX core 檔案覆蓋回新基底，仍有機會重現：

- 長時間運行後 heap 漸增或碎片化
- BLE 配對/同步異常
- MQTT proxy 或收包處理異常
- 長 uptime 後出現封包解析或傳輸不穩定問題

## 分階段計劃

### Phase 1：建立乾淨 `2.7.15` 基底

1. 從 upstream `2.7.15` 建立新分支。
2. 確認目標機型可以成功編譯。
3. 先不搬 HermesX 客製，直接測 baseline。
4. 記錄 baseline 指標：
   - 開機 free heap
   - min free heap
   - largest free block
   - free psram
   - BLE 配對與同步是否正常
   - MQTT 若啟用，確認收發與 decode 是否正常

### Phase 2：盤點 HermesX 變更

將 HermesX 變更拆成四類：

1. UI 視覺與渲染
2. UI 互動與設定
3. HermesX 功能模組
4. Core/系統層改動

並將檔案分為三類：

- 直接搬
- 人工 merge
- 禁止直接覆蓋

### Phase 3：先搬 UI 設計本體

優先搬移低風險、高可見度內容：

- 畫面 layout
- applets
- fonts
- icon / 文案 / 視覺風格
- 動畫與渲染樣式

這一階段目標是讓 HermesX UI 外觀先在 `2.7.15` 上重現，但不動核心通訊行為。

### Phase 4：搬 UI 互動與 HermesX 模組

第二層再搬：

- HermesX 自訂互動流程
- HermesX 設定頁與偏好設定
- HermesX 功能模組
- LED、快捷切換、緊急模式等功能

這一階段若碰到以下路徑，必須停下來人工比對：

- `bluetoothStatus`
- `PhoneAPI`
- `sendToPhone`
- `packetPool`
- `MQTT proxy`
- `MeshService`

### Phase 5：鎖住 upstream 穩定性修正

以下修正不可被 HermesX 舊檔覆蓋掉：

1. `NimbleBluetooth` / `NRF52Bluetooth`
   - `BluetoothStatus` 必須維持 stack object 用法
   - 不可恢復成 `new BluetoothStatus(...)`

2. `Router`
   - embedded target 必須維持 upstream static packet pool

3. `MeshService`
   - embedded target 必須維持 upstream static pointer queue

4. `PhoneAPI` / BLE disconnect 路徑
   - 必須保留上游已修正的狀態清理與 queue 清理行為

5. `MQTT`
   - 若 HermesX 要重新啟用 MQTT，必須用新基底重新驗證，不能假設舊分支行為安全

### Phase 6：整合驗證

功能驗證：

- UI 所有頁面是否正常顯示
- 字型與排版是否符合 HermesX 原設計
- 按鍵 / 編碼器 / 觸控 / 選單互動是否正常
- BLE 配對與同步是否正常
- GPS、LoRa、通知、LED、快捷操作是否正常
- MQTT 若啟用，確認收發與 decode 正常

穩定性驗證：

- 冷開機 heap
- 配對後 heap
- 長時間 idle 24h / 48h / 72h
- 長時間收包情境
- MQTT 下行 / proxy 情境

至少記錄：

- `free heap`
- `min free heap`
- `largest free block`
- `free psram`

### Phase 7：回歸與收斂

1. 若 heap drift 再次出現，先比對是否為 HermesX 搬移造成。
2. 問題若來自 UI 層邏輯，優先局部修補。
3. 問題若來自 core 路徑，優先保留 upstream 實作，避免直接回退。
4. 完成後再整理 changelog 與移植紀錄。

## 建議搬移順序

1. 先完成 upstream `2.7.15` baseline 編譯與驗證。
2. 搬 `graphics` / applets / fonts / 視覺設計。
3. 搬 HermesX 專用模組與互動。
4. 搬偏好設定、文案與資料檔。
5. 做整合測試。
6. 最後才處理高風險 core 相依點。

## 檔案策略

### 可優先搬移

- `src/graphics/*`
- HermesX UI applets
- 字型與視覺資產
- 文案與介面呈現
- HermesX UI 專用 docs / prefs

### 必須人工 merge

- `src/modules/HermesX*`
- `src/modules/HermesEmUi*`
- `src/graphics/Screen.cpp`
- 與輸入、通知、狀態聯動的 UI 邏輯

### 禁止直接整檔覆蓋

- `src/nimble/NimbleBluetooth.cpp`
- `src/platform/nrf52/NRF52Bluetooth.cpp`
- `src/mesh/PhoneAPI.*`
- `src/mesh/MeshService.*`
- `src/mesh/Router.*`
- `src/mqtt/*`

## 執行重點

- 搬 UI，可以完整搬。
- 搬功能，可以完整搬。
- 搬 core，不可整檔覆蓋。
- 若 HermesX 功能依賴舊 core，應以局部 patch 或 adapter 方式重建。
- 所有 heap / BLE / queue / allocator 路徑都以 upstream `2.7.15` 為準。

## 後續建議

下一步建議產出一份實際搬移清單，逐檔標註：

- `直接搬`
- `人工 merge`
- `禁止覆蓋`

如此可作為後續升級與移植工作的執行表。
