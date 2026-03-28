# HermesX Change Log

本文件為可對外發布版本的更新紀錄，整理 HermesX 韌體的重要功能更新、體驗調整與修正項目。

## 2026-03-27

### 修正

- 修正 HermesX 多個滾動式選單在快速互動時，內部選取狀態已變更但畫面仍停留在前一項的 redraw 不同步問題。
- 修正 `CannedMessage` 選單移動路徑中，`UIFrameEvent` 未穩定帶有有效 action，導致某些上下切換沒有正確要求畫面重繪的問題。

### 改進

- 調整 `Screen::setFastFramerate()` 的 redraw 行為：當前頁面為固定 frame 的互動式畫面時，會強制下一次 UI tick 立即生效，不再被 `OLEDDisplayUi` 內部 FPS budget 延後一幀。
- 此修正套用於共用 `Screen` redraw 路徑，因此不只 `CannedMessage`，HermesX 其他滾動式選單也一併受益。
- Fast Setup `裝置管理 > 電源管理` 新增唯讀欄位 `當前電壓`，可直接在裝置端查看目前電池電壓；若暫時沒有有效讀值則顯示 `未知`。

### 影響

- 本次調整不新增大型 buffer、不引入新的常駐快取，也不增加額外動態配置；主要代價是互動時 redraw 更積極，但記憶體占用幾乎不變。

## 2026-03-26

### 新增

- Fast Setup `裝置管理 > LoRa` 新增 `Role` 設定入口，可直接在裝置端切換 `Client`、`Client Mute`、`Client Hidden`、`Tracker`、`Sensor`、`TAK`、`TAK Tracker` 與 `Lost&Found`。

### 改進

- HermesX `Home` 與 `GPS` 頁改為 role-aware：只在 `Client`、`Client Mute`、`Client Hidden` 類角色建立與顯示；`Tracker`、`Sensor`、`TAK` 等非 client 類角色保留操作選單與 Fast Setup，但不再載入這兩頁。
- 將 Home/GPS direct neon 相關快取改為按需配置（lazy allocation），並在切換到非 client 類角色時主動釋放，避免該批 UI buffer 長期常駐記憶體。
- `Role` 切換後會立即重建 frames，讓 `Home/GPS` 是否存在的頁面狀態可即時反映，不必等到下次開機才看見差異。

### 驗證結果

- 現場量測顯示，`TAK` 模式 free heap 明顯高於 `Client`，可確認 Home/GPS UI 與 direct neon 緩衝已不再於非 client 類角色常駐。
- 切回 `Client` 後，Home 頁的 direct glyph cache 會依需求重新建立，符合預期的 client-only 記憶體配置策略。
- 本輪現場參考值：重開後 `TAK` 模式約為 `147872` bytes free heap、`139252` bytes largest free block；重開後 `Client` 進 Home 後約為 `45548` bytes free heap、`36852` bytes largest free block。

## 2026-03-23

### 新增

- 新增低記憶體提醒 popup：當可用記憶體落入危險區時，會跳出警告並提示使用者前往 `裝置管理 > 節點資料庫` 清理過期節點資料，但不會自動執行刪除。
- 新增手動節點資料庫整理入口：`設定 > 裝置管理 > 節點資料庫 > 重設資料庫`，可選擇清除 `12hr / 24hr / 48hr` 未更新節點。
- 新增 UI 設定 `旋鈕對調`，支援在裝置上直接切換 rotary 方向，不需重新刷機或改底層設定。
- 新增 monitor 擷取工具 `tools/capture_monitor_log.sh`，可將 `platformio device monitor` 的終端輸出同步保存到 `logs/monitor/`，便於長時間觀察 heap、MQTT 與 crash 前後事件。

### 改進

- Fast Setup 選單中的 `裝置設定` 改名為 `裝置管理`，讓節點資料庫、裝置整理與維護操作更集中。
- 低記憶體提醒加入較明顯的三連高音蜂鳴提示，降低只看畫面時容易忽略警告的情況。
- 進入 Stealth 模式時，也改用同一組較醒目的提示音，讓使用者更容易辨識模式切換。
- `rotEnc1` 與 `CannedMessage` 的 rotary 行為改為統一讀取設定值，不再固定寫死 `CW/CCW/Press -> Down/Up/Select`。

### 使用方式

- 若要手動整理節點資料庫：
  - 進入 `設定 > 裝置管理 > 節點資料庫 > 重設資料庫`
  - 選擇要清理的門檻（`12hr / 24hr / 48hr`）
- 若要長時間保存 monitor 輸出：
  - 執行 `zsh tools/capture_monitor_log.sh heltec-wireless-tracker`
  - monitor 畫面會同步寫入 `logs/monitor/<env>_monitor_時間戳.log`
  - 適合用來追 `Heap`、`MQTT`、`BLE disconnect`、`PANIC` 等現場問題

### 備註

- `logs/monitor/` 為本機 debug 輸出目錄，已排除於 Git 追蹤之外，不會納入版本控管。

## 2026-03-20

### 修正

- 修正 HermesX direct neon cache/scratch 常駐 RAM 過大，導致在 TFT Home、GPS neon、BLE 與 MQTT client proxy 併用時，系統可用 heap 被明顯壓低的問題。
- 修正 Home 與 GPS neon 改為共用 scratch buffer 後，shared buffer 過小造成的 Home 時鐘右側殘影/錯位問題。
- 修正 InkHUD 訊息 Banner 彈出後，主按鍵短按只能關閉通知、無法直接進入 `Recent Send / All Messages` 的問題；現在短按會優先切到訊息頁，未啟用時才退回單純關閉 Banner。
- 修正 `Recent Send` 列表對 UTF-8/中文摘要的顯示不穩定問題；列表行改為使用 HermesX 混排字型繪製，避免 sender / preview 被截成亂碼或 `???`。

### 改進

- 將 Home clock 與 GPS neon text 的 `composedLayerMap` 改為共用同一塊 scratch buffer，減少重複常駐記憶體。
- 將 Home direct neon clock 的 region 上限縮為實際固定值 `152x37`，不再為過大的保守上限保留記憶體。
- 將 GPS title neon 的 `coreMask` 常駐陣列移除，改為直接以 `fullMask` 即時計算核心層，進一步減少靜態 RAM 佔用。
- 新增 direct neon / heap 診斷 log，便於現場比對 Home、GPS 與 MQTT client proxy 佔用情況。
- 將 HermesX 新訊息 popup 顯示時間由 `3s` 延長為 `5s`。
- 調整 `Recent Send` 訊息 detail 頁版面：標頭改為兩行緊湊資訊，正文區域放大並加入上下滾動與右側簡易 scrollbar，減少長訊息頁面的空白浪費。

### 備註

- 本次調整後，開機與進入 Home 頁面的 free heap 已明顯回升；目前工程判斷認為，先前 HermesX 分支較容易 panic，主因之一高度懷疑為整體記憶體壓力過高。
- HermesX 新訊息 popup 的 `Press` 直開最新訊息 detail 仍有已知時序問題；目前現場測試在某些喚醒/重繪時機下仍可能只關閉 popup，後續需再追 `pending -> visible` 與輸入事件先後順序。

## 2026-03-19

### 修正

- 修正 BLE 配對後在同步設定或接收封包時，HermesX 分支較容易異常重啟的問題；`getFiles()` 檔案清單建立流程已同步回官方版實作，降低與官方行為差異。
- 修正 `狀態燈亮度` 設為 `0` 時，蜂鳴器也被一併靜音的問題；現在 `狀態燈亮度 = 0` 只會關閉 LED，不再影響 buzzer。
- 修正 `狀態燈亮度` 在裝置重新開機後無法正確保留的問題。
- 修正 `狀態燈亮度 = 0` 時，開機動畫、一般 LED 動畫與長按電源提示燈效仍可能被重新點亮的問題。

### 改進

- 將 HermesX 狀態燈亮度改為獨立偏好設定儲存，不再借用 `uiconfig.screen_brightness`。
- 新增 HermesX 狀態燈亮度儲存檔：`/prefs/hermesx_ui_led_brightness.txt`。
- 調整 HermesX LED 輸出邏輯：當狀態燈亮度為 `0` 時，除非為緊急紅燈模式，其他一般 LED 動畫都不再顯示。

### 介面調整

- 將 UI 設定名稱由 `WS2812亮度` 改為 `狀態燈亮度`。
- 將相關提示文字與日誌文案統一改為 `狀態燈 / status LED`，避免與螢幕亮度設定混淆。

## 2026-03-12

### 改進

- GPS 座標顯示策略改為「只縮整體，不壓字距」：保留半尺寸字形縮放（`coordHalfScale=true`），並將字距追蹤回復正常（`coordTracking=0`），降低數字擠壓感。

### 繪製邏輯註記

- `Neon effect`（文字）：
  - 由 `renderDirectNeonPattanakarnText()` 先合併每個字元的 layer map（同像素取最大層級），再依層級 palette 進行 scanline-run 輸出。
  - 層級輸出順序形成「外暈 -> 內暈 -> 核心」的 direct-TFT 霓虹效果，且可用 `clearBackground=false` 保留底圖不強制鋪黑。
- `Bloom effect`（球體）：
  - GPS 右側球體由 `renderDirectGpsGlobeBloom()` 以 40 層同心填圓渲染，亮度由外圈 `1%` 緩升至內圈 `72%`（外層先畫、內層後畫）。
  - Home 右下球體由 `renderDirectHomeOrangeMesh()` 以 20 層橘色同心 Bloom 渲染，亮度由外圈 `5%` 緩升至內圈 `34%`，再疊加線框與節點層。

## 2026-03-07

### 改進

- GPS 頁 Hero 視覺改版：強化 `GPS ON` 與座標資訊的霓虹層次，座標維持 `Pattanakarn` 字型與動態小數位寬度自適應。
- GPS 霓虹補強：半尺寸座標改為核心外描 + 雙層 glow，避免核心筆畫吞掉外暈，並同步拉開標題/座標 glow 色階。
- GPS 座標霓虹改為與 Home Timer 相同的 direct-TFT neon 演算法：改在 `ui->update()` 後直繪，沿用同一套多層 glow 與層級色帶邏輯。
- 右下角地圖改為「藍地球 + 紅外暈」：保留藍色地球主體與藍色內暈，新增淡紅外層 halo，提升輪廓辨識。
- 非 TFT fallback 加入黑白近似霓虹樣式：關鍵文字改為雙層白色描邊 + 核心字，並在可用空間足夠時新增右下角地圖外圈描邊。

## 2026-03-06

### 修正

- 修正 `CannedMessage` 送出後畫面誤跳到 `Recent Send` 的問題。
- 修正 `CannedMessage` ACK/回饋視窗清場後，`FOCUS_PRESERVE` 因 frame index 映射到 `Recent Send` 的焦點偏移行為。

### 改進

- `CannedMessage` 在送出/ACK/訊息回饋清場與逾時退場後，改為明確返回 `Home`，不再依賴 module frame 移除後的索引保留結果。
- 新增 `CannedMessage` 返回路徑診斷 log：
  - `captureReturnTarget=...`
  - `restoreReturnTarget=...`
  便於現場快速確認返回目標是否正確。

## 2026-03-05

### 修正

- 修正 Home 頁 Timer 每秒觸發整頁重刷的問題，改為以時鐘區域為主的差分更新，降低閃爍與刷新感。
- 修正由功能頁返回 Home 時可能只剩 Timer、且底圖遺留前頁圖形的問題，強化進出 Home 的重繪與清區流程。
- 修正 `CannedMessage` 選單按下 `Cancel` 後誤跳轉到 `Recent Send` 的問題；現在預設回到 Home。
- 修正 `CannedMessage` 選單中旋鈕事件誤落入退出路徑、導致旋轉直接回 Home 的問題。

### 改進

- Home 直繪時鐘的底圖刷新改為節流策略，避免每秒更新時連帶觸發非必要資訊層重繪。
- 新增 `CannedMessage` 返回目標 fallback 行為：未捕捉到有效 return target 時，明確導向 Home。
- `CannedMessage` 旋鈕事件改為優先映射為清單上下導航，並避免與 `Cancel/Back` 分支衝突。
- 新增診斷 log：`[CannedMessage] restoreReturnTarget default -> home`，便於現場快速驗證返回路徑。

## 2026-03-03

### 亮點

- 主頁重新設計為大時鐘資訊頁，整合時間、日期、目前角色、電池資訊與衛星數量顯示。
- 主頁時鐘改用直接在 TFT 上渲染的霓虹風格顯示，提升辨識度與視覺風格。
- 新增 `Recent Send` 訊息流程，提供最近訊息清單與訊息詳頁，並整合 `MSG` 入口。
- 新增新訊息彈出提示，當訊息喚醒螢幕時可在短時間內直接查看新訊息。

### 新增

- 新增 `Recent Send` 訊息清單頁面，可快速檢視最近收到的訊息。
- 新增訊息詳頁，可從清單進入查看單則訊息內容。
- 新增 `MSG` 入口整合至 HermesX 自訂操作流程，讓訊息存取更直接。
- 新增 `Pattanakarn` 數字字型資產，套用於主頁大型時間顯示。

### 改進

- 主頁版面改為資訊導向設計：
  - 上方為大型時間顯示。
  - 下方顯示日期與目前角色。
  - 右側整合電池與衛星資訊。
- 主頁時間顯示改為 direct-TFT 霓虹風格渲染，不再只依賴單色 buffer 模擬發光。
- 主頁時鐘改為局部差分更新，降低秒數變化時的整頁刷新感。
- 訊息流程改為清單與詳頁分離，較符合日常瀏覽邏輯。

### 修正

- 修正新訊息喚醒螢幕時，通知彈窗未正確顯示的時序問題。
- 修正 `CannedMessage` 與 `Recent Send` 頁面之間的輸入事件互相干擾問題。
- 修正旋鈕在未完整設定 `cw/ccw/press` 事件時，自訂頁面可能無法操作的情況，新增 fallback 行為。

### 使用體驗調整

- 當新訊息喚醒螢幕時，會顯示短時間提示視窗：
  - 短按可直接進入該則訊息。
  - 旋轉可關閉提示。
- 通知彈窗僅在「螢幕原本為關閉，且此次由新訊息喚醒」時顯示，避免在螢幕已開啟期間重複干擾。

### 備註

- 若使用者將螢幕顯示時間設定為 `0`，則不會啟用新訊息彈出提示。
- 本次更新包含多項 HermesX 自訂 UI 調整，建議升級後重新確認個人操作習慣與裝置設定。
