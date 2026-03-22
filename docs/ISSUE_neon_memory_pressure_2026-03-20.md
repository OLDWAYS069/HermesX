# Neon Memory Pressure Investigation

日期：2026-03-20

## 背景

Heltec Wireless Tracker 在 HermesX 自訂 TFT UI、BLE、MQTT client proxy 同時啟用時，曾反覆出現：

- `Disconnected (read failed: [Errno 6] Device not configured)`
- 重連後 `esp_reset_reason=PANIC(4)`

同時現場觀察到：

- 不開 MQTT 時可穩定運作較長時間
- 開 MQTT client proxy 後較容易異常重啟
- Home / GPS 霓虹頁面啟用時，free heap 會再明顯下降

本次工作目標是先確認 HermesX 自訂 TFT/neon 路徑是否佔用過多 RAM，並優先做低風險減量。

## 量測結果

### 開機階段 heap

以 `src/main.cpp` 階段式 log 量測：

- 開機初始 `Free heap: 187460`
- `main after NodeDB free=147956`
- `main after Screen ctor free=140852`
- `main after MeshService ctor free=126716`
- `main after MeshService init free=126732`
- `main after mqttInit free=112104`
- `main after PowerFSMThread free=110400`

代表主要常駐成本依序為：

1. `NodeDB`
2. `MeshService`
3. `mqttInit`
4. `Screen/TFT`

TFT 不是唯一大戶，但 HermesX 的 direct neon 路徑在進入 Home/GPS 畫面後，會把 free heap 再往下壓。

### Direct Home / GPS neon 路徑

在 `src/graphics/Screen.cpp` 補 log 後，確認：

- `PattanakarnClock32::kGlyphCount = 13`
- 字集只有 `0-9`, `:`, `-`, `.`
- glyph cache 是 lazy build，不是開機一次把全部字元 cache 建滿

真正吃 RAM 的不是 timer 變數，而是幾塊大 scratch / cache：

- Home clock current composed layer map
- Home clock previous composed layer map
- GPS / text composed layer map
- GPS title neon 的 `fullMask/coreMask/layerMap`

## 原本為何這樣設計

Home direct neon clock 不是走一般 UI buffer，而是 direct TFT fast-path。

當初保留 `previousComposedLayerMap` 的目的有三個：

1. 避免 direct-TFT 時鐘殘影
2. 用上一幀 layer map 做差分重繪，降低每秒整塊刷新
3. 配合 `skipUiUpdate`，讓 Home 穩態時不用整頁重畫

所以不能直接粗暴砍掉整個 previous map，否則很容易回到殘影或閃爍。

## 本次實作

### 1. Home / GPS 共用 composed scratch buffer

調整 `src/graphics/Screen.cpp`：

- 將 Home clock 的 `composedLayerMap`
- 與 GPS/text 的 `composedLayerMap`

改為共用同一塊 `gDirectNeonSharedComposedLayerMap`

前提是：

- Home neon 與 GPS neon 不會同時 render

這一刀保留了原本視覺效果，但少掉一整塊常駐 composed map。

### 2. Home clock region 上限縮到實際值

原本 Home clock region 使用較保守的上限。

現已依實際 slot 幾何改成固定：

- `152 x 37`

因此：

- `previousComposedLayerMap`
- Home clock 對 shared scratch 的需求

都不必再為更大的保守上限買單。

### 3. GPS title neon 拿掉一塊中間 mask

`renderDirectGpsPosterTitleWord()` 原本有三塊常駐陣列：

- `fullMask`
- `coreMask`
- `layerMap`

本次改為：

- 保留 `fullMask`
- 保留 `layerMap`
- 移除 `coreMask`

核心層改用 `fullMask + isMaskInteriorPixel()` 即時計算，不再為 `coreMask` 常駐保留一整塊 RAM。

### 4. 保留安全邊界，避免 buffer 過小

實作過程中曾誤把 shared scratch 縮得比 Home clock 還小，導致畫面右側殘影與錯位。

已修正為：

- `text` 的邏輯上限可小
- 但真正 shared scratch 仍至少要覆蓋 Home clock 的最大 region

## 效果

最新量測：

- `main after PowerFSMThread free=110400`
- `DirectHome state update free=46580`
- `DirectHome entering free=46032`

相較於先前約 `85KB` 開機餘量、Home 頁只剩約 `20KB` 的狀態，本次優化後已明顯回升。

這代表：

- HermesX neon/TFT 路徑確實曾佔用過多常駐 RAM
- 經過 cache / scratch 減量後，系統 free heap 與 largest block 都比較健康

## 對當機原因的判斷

目前最合理的工程判斷是：

- 先前反覆 `PANIC(4)` 的主因，高度懷疑與整體記憶體壓力過高有關
- 不是單一「某個函式必 crash」那麼簡單
- 更像是：
  - `NodeDB + MeshService + MQTT + TFT/neon + BLE`
  - 疊加後把 free heap 與 largest block 壓得太低
  - 在 MQTT proxy / BLE / UI render 等高壓路徑下，更容易觸發 panic

也就是說：

- `高記憶體佔用` 很可能是主背景條件
- `特定路徑`（例如 MQTT client proxy）則是較容易把系統推倒的觸發點

目前證據足以說：

- **記憶體佔用過多，是先前常當機的高機率主因之一，而且是目前最值得優先處理的主線。**

但仍不建議把所有 panic 都絕對歸因成單一 heap 問題，因為電源量測抖動、BLE/MQTT 時序等因素仍可能共同放大風險。

## 後續建議

1. 先持續觀察這版在 MQTT client proxy 開啟時的穩定性
2. 若仍有 panic，再優先檢查：
   - `NodeDB` 常駐量
   - `MeshService` queue / pool
   - MQTT proxy queue 壓力
3. 若還要繼續瘦 UI，可再看：
   - GPS title neon 的 mask 上限是否能再縮
   - 是否能把部分 direct neon scratch 改成更細緻的共用池

## 2026-03-21 追加觀察

- 連續運作約 `98 分鐘` 後，裝置仍發生重啟。
- 主機端先看到：
  - `Disconnected (read failed: [Errno 6] Device not configured)`
- 裝置重連開機後回報：
  - `esp_reset_reason=PANIC(4)`

### 觸發前最後關鍵路徑

Crash 前最後可見流程不是單純 BLE 斷線，而是：

1. `Client wants config, nonce=69420`
2. `BLE onConfigStart`
3. `Start file manifest rebuild`
4. `Acquiring SPI lock for file manifest`
5. `SPI lock acquired for file manifest`
6. 隨後主機端連線中斷，裝置 panic 重啟

這表示目前最可疑的直接觸發點，已從單純 `DirectHome` / MQTT 壓力，收斂到：

- `BLE config start`
- `file manifest rebuild`

### 當下記憶體狀態

在進入上述路徑前，heap 已降到危險區：

- `DirectHome state update free=6324 largest=4596`
- `DirectHome entering free=5808 largest=3572`

之後系統仍要同時承受：

- `DirectHome` 畫面進場
- BLE config 流程
- file manifest rebuild
- SPI / filesystem / protobuf encode 類路徑

這很可能就是把系統推過臨界點的最後一擊。

### 補充觀察

- panic 重開後 heap 立即恢復健康：
  - `Free heap: 187444`
  - `[Heap] main after NodeDB free=147940`
  - `[Heap] main after Screen ctor free=140852`
- 本次開機也可見：
  - `cleanupMeshDB purged 17 entries`

這表示：

- 開機 baseline 本身沒有立即不足
- 問題更像是 runtime 壓力逐步累積
- 本次新增的 stale NodeDB cleanup 已有生效，但尚不足以完全避免 panic

### 目前最值得優先檢查的點

1. `BLE onConfigStart` 前後 heap / largest block
2. `file manifest rebuild` 的配置大小與暫存生命週期
3. 低水位時是否應延後或拒絕 config start
4. config 流程期間是否應暫停 `DirectHome` 進場或其他高壓 UI 路徑
