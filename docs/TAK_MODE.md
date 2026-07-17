# TAK MODE

本文描述 HermesX C0.3.7 CIV 版目前的 `TAK MODE` 行為。TAK MODE 是面向 TAK / ATAK 現場使用的裝置模式，重點是降低 LoRa 阻塞風險、讓功率調整自動化，並把 GROUP 配對與尋人入口收斂到同一個現場操作流程。`TAK Tracker` role 也共用這套 HermesX UI、功能與頻道設定，但保留 upstream `TAK_TRACKER` 的追蹤器角色語意。

## TAK Tracker 共用範圍

裝置 role 設為 `TAK Tracker` 並重新開機後，會直接進入和 TAK MODE 相同的智慧功率主頁與操作流程：

- 右轉開啟完整 TAK Tracker 選單
- 左轉開啟相同的 `頻道選擇`
- 共用 `TAKMODE設定`、`GROUP設定`、`尋人模組` 與 CIV build 限制
- 共用智慧功率、聲光靜默與 CannedMessage 輸入隔離
- 共用 `/prefs/hermesx_tak_profile.bin`，因此 TAK A-E、`自動` 與 profile 參數在兩種 role 間一致

套用這些設定時不會把 `TAK Tracker` 改成 `TAK`；位置送出、省電與 rebroadcast 行為仍由 `TAK_TRACKER` role 負責。

App 已建立的 Primary / Secondary 自訂頻道也會保留。TAK 的 `頻道選擇` 只調整 LoRa channel slot 與手動頻率回復基線，不會覆寫自訂頻道名稱、PSK、uplink/downlink 或位置分享設定。

## 進入與退出

從 HermesX 主選單進入 `TAK MODE` 時，裝置會顯示 `進入TAK模式` transition 動畫，接著排程重開機。

退出 TAK MODE 時會顯示 `退出TAK模式` transition 動畫，接著排程重開機。

這個流程刻意設計成和更新模式相近，原因是 TAK MODE 會暫存並套用多個 runtime 設定，包括 device role、LoRa preset、LoRa 頻道、位置廣播、智慧功率與聲光狀態。透過重開機可讓 role 與 module 狀態在啟用後保持一致。

進入 TAK MODE 時會自動把 LoRa preset 切到 `Short_Fast`，讓 TAK / ATAK 現場封包優先使用較短 airtime，同時避免 `Short_Turbo` 對距離與法規頻寬的要求過於激進。退出 TAK MODE 時會還原進入前的 LoRa preset；若進入前是 custom LoRa 參數，也會還原原本的 bandwidth / spread factor / coding rate。

## 主頁

TAK MODE 啟用後，主頁會切換成「智慧功率」UI。這個頁面顯示：

- 目前 LoRa 發射功率
- 最近 RX 的 RSSI / SNR
- 最近收訊時間
- GROUP heard 數量
- 目前 TAK 頻道 / slot
- `ChUtil` airtime 使用率與擁塞狀態

`ChUtil` 來自 airtime/channel utilization。它可以用來判斷目前頻道是否偏忙或擁塞，但它不是頻譜掃描器，只能反映本機看到的 LoRa 空中占用狀態。

## 主頁操作

在 TAK 智慧功率主頁：

- 旋轉編碼器往右轉：開啟 TAK 選單
- 旋轉編碼器往左轉：開啟 `頻道選擇`
- 短按：回到原本 HermesX 主選單

TAK MODE 會用和 CannedMessage 相同的 `rotEnc1` effective cw/ccw/press 解析方式，避免旋鈕設定對調後 TAK 選單無法控制。

## TAK 選單

TAK 選單目前包含：

- `TAK MODE`: 切換 TAK ON/OFF
- `TAKMODE設定`: 調整 TAK profile
- `頻道選擇`: 選擇 TAK 使用的 LoRa channel slot
- `GROUP設定`: 進入 GROUP PIN / 配對設定
- `EMUI`: CIV 版會保留語意，但 EMAC 入口由 build 關閉
- `尋人模組`: 開啟尋人功能入口
- `返回主選單`: 回到 HermesX 主選單

## TAKMODE設定

`TAKMODE設定` 會保存到 `/prefs/hermesx_tak_profile.bin`。目前可調整：

- `裝置資訊`: node info broadcast 間隔
- `GPS刷新`: GPS update interval
- `位置廣播`: position broadcast interval
- `智慧距離`: SmartPosition minimum distance
- `智慧間隔`: SmartPosition minimum interval
- `智慧功率低`: 智慧功率最低 dBm
- `智慧功率高`: 智慧功率最高 dBm
- `聲光靜默`: TAK 啟用時暫時關閉聲光輸出
- `EMUI`: 是否允許 TAK profile 的 EMUI 語意入口
- `尋人模組`: 是否允許 TAK profile 的尋人入口

如果 TAK MODE 已啟用，修改後會立即套用到 runtime，並同步保存 TAK state。

## 頻道選擇

`頻道選擇` 不是 FHSS。它是固定 LoRa channel slot 的快速選擇，用來讓現場使用者避開同區域已知擁塞或重疊的頻道。

目前選項：

- `自動`
- `TAK A`
- `TAK B`
- `TAK C`
- `TAK D`
- `TAK E`

### TAK 頻道、Slot 與頻率

以下頻率以本分支的台灣 `TW 920–925 MHz` 區域，以及 TAK MODE 自動套用的 `Short_Fast`（250 kHz 頻寬）計算：

| 顯示 | Slot | 中心頻率 |
|------|------|----------|
| 自動 | 保留進入 TAK MODE 前的 channel | 依原本 Slot／頻率設定 |
| TAK A | 5 | 921.125 MHz |
| TAK B | 9 | 922.125 MHz |
| TAK C | 17 | 924.125 MHz |
| TAK D | 19 | 924.625 MHz |
| TAK E | 3 | 920.625 MHz |

頻率計算方式與韌體一致：`920.0 + 0.125 + ((Slot - 1) × 0.250)` MHz。若裝置改用其他區域、其他 LoRa 頻寬或設定 `frequency_offset`，實際頻率會跟著改變；上表不是跨區域通用頻率表。

當選擇固定 TAK slot 時，TAK MODE 會設定 `config.lora.channel_num`，並清除 `override_frequency`。退出 TAK MODE 時會還原進入 TAK 前的 `channel_num` 與 `override_frequency`。

頻道 slot 只控制固定頻道位置；TAK MODE 的 LoRa preset 會另外自動切成 `Short_Fast`，退出時再還原。

`頻道選擇` 中的 `返回`、Cancel 或 Back 都會直接回 TAK 智慧功率主頁，不會回到 TAK popup。

## GROUP 配對入口

TAK 選單中的 `GROUP設定` 直接共用既有 GROUP 設定流程，不另建一套 TAK 專用配對資料。

可操作項目包含：

- `GROUP PIN A`
- `GROUP PIN B`
- `查看GROUP PIN`
- `EMINFO設定`

從 TAK 選單進入 `GROUP設定` 後，返回會回到 GROUP 菜單。這樣現場可以設定或確認 GROUP PIN 後，直接進 `節點列表` 檢查目前已配對 / heard 到的 GROUP 節點。

目前的配對語意以 GROUP PIN 為核心：兩台或多台裝置使用相同 GROUP PIN 後，GROUP / Lighthouse 相關流程會把它們收斂為同一個可辨識群組。

## CannedMessage 隔離

TAK MODE 啟用期間會暫時退出並阻擋 CannedMessage menu。這是為了避免 TAK 智慧功率主頁或 TAK popup 的旋鈕事件被罐頭訊息 composer 搶走。

當 TAK MODE 或 TAK overlay 正在主頁上時，輸入會優先交給 TAK MODE 處理。

## 智慧功率

智慧功率會在 TAK / TAK Tracker role 下自動啟用。它會先使用 `智慧功率高` 作為初始發射功率，再依據 ACK、RSSI/SNR、ACK timeout、remote LoRa RX 與 broadcast 封包逐步調整 LoRa `tx_power`。

詳細策略見 `docs/SMART_POWER.md`。

## CIV 版限制

本分支是 CIV 版，EMAC / EM UI 進入被 build 關閉。TAK MODE 內仍可能保留 `EMUI` 字樣或 profile 選項，但這不代表 CIV 版可以啟動 EMAC。

## 驗證重點

實機驗證時建議確認：

- 進入 TAK MODE 會顯示 `進入TAK模式` 並重開機
- 退出 TAK MODE 會顯示 `退出TAK模式` 並重開機
- TAK 啟用後主頁是智慧功率 UI
- TAK Tracker role 啟動後使用相同 UI、選單、功能與頻道選擇，且 role 不會被改成 TAK
- LoRa preset 會切到 `Short_Fast`
- 右轉開 TAK 選單
- 左轉開 `頻道選擇`
- 短按回 HermesX 主選單
- `頻道選擇` 返回會回智慧功率主頁
- `GROUP設定` 會進入既有 GROUP PIN 流程
- TAK 啟用時 CannedMessage 不會搶走旋鈕輸入
