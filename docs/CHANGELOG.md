# HermesX Change Log

本文件為可對外發布版本的更新紀錄，整理 HermesX 韌體的重要功能更新、體驗調整與修正項目。

## 2026-07-14

### 發布：HXB_C0.3.7_20260714_1808

- 正式發布 CIV build `HXB_C0.3.7_20260714_1808`，上一個 GitHub Release 為 `HXB_C0.3.7_20260713_1838`。
- `TAK Tracker` 現在完整共用 TAK MODE 的智慧功率主頁、旋鈕操作、TAK profile、頻道選擇、GROUP 設定、尋人入口與 CannedMessage 輸入隔離。
- 套用 TAK 共用設定時保留 `TAK_TRACKER` role，不會被改寫成 `TAK`；Tracker 的位置送出、省電與 rebroadcast 語意維持不變。
- App 建立的 Primary / Secondary 自訂頻道、名稱、PSK、uplink/downlink 與位置分享設定不會被 TAK 頻道選擇覆寫。
- 收錄本日尚未發布的 TraceRoute 整頁搜尋結果、直接綁定／返回操作，以及旋鈕實際 GPIO 長按來源修正。
- 新增 release note：`docs/RELEASE_HXB_C0.3.7_20260714_1808.md`。

### 最終驗證

- `platformio run -e heltec-wireless-tracker -j 4` 編譯成功，RAM 34.5%，Flash 88.0%。
- 韌體內嵌版本確認為 `HXB_C0.3.7_20260714_1808`。
- Desktop OTA / Factory 產物與 build 輸出的 SHA256 一致。
- OTA SHA256: `131f1d890d1aae36ca8b6adf915f5493a2678a8444b8b4adcdeb064fdee8274f`
- Factory SHA256: `382b2fd360fb83f7e89d206648574f60d1b4c3ad6bbe37dd9c78eb10f2e03b2c`

### 調整

- TraceRoute `搜尋裝置` 的搜尋結果由置中小視窗改為整頁顯示，完整呈現 `ShortName` 與自動換行的 `LongName`。
- 搜尋成功結果頁最下方新增 `綁定`、`返回` 兩個可選操作；`綁定` 會直接加入快速 TraceRoute 綁定清單，`返回` 不變更綁定。
- 從搜尋結果選擇 `綁定`、`返回` 或取消離開後，`綁定節點` 清單游標一律停在 `返回` 選項，不再自動定位到搜尋到的節點。
- 找不到裝置時同樣使用整頁結果，顯示查詢 ShortName，最下方只提供有效的 `返回` 操作。

### 修正

- 修正 TraceRoute 長按來源混用：泛用 `BUTTON_EVENT_LONG_PRESSED` 與所有按鍵共用的 `anyPressed` 不再直接觸發綁定或解除綁定，避免 Home 或其他按鍵的既有按壓在切頁後被誤判成 TraceRoute 長按。
- 依實機 log 修正 shared hold pin 放開後的假長按：`rotEnc1` 已送出短按後，`ButtonThread` 的 OneButton 狀態仍可能延遲滿 1 秒並誤呼叫解除綁定；`ButtonThread` 現在不再執行 TraceRoute 綁定／解除或旋鈕鎖定頁面動作。
- 修正 `RotaryEncoderInterruptBase` 按住輪詢排程：原本先設定 50ms interval 又回傳 `INT32_MAX`，會被 `OSThread::run()` 覆蓋成永久等待，導致真正長按無法成立；按住期間現在直接回傳 50ms 持續讀取實際 GPIO。
- TraceRoute 1 秒長按與 Home 3 秒旋鈕鎖定統一由旋鈕 GPIO hold 時間判定，並在按下當下鎖定頁面資格；其他按鍵、放開後的 OneButton 延遲狀態或跨頁既有按壓均不能觸發解除綁定。
- 保留 TraceRoute 放開時的短按抑制與延後短按完成：短按仍查看節點詳情／送出 TraceRoute，長按後放開不會再補送短按或關閉確認框。

### 驗證

- `platformio run -e heltec-wireless-tracker -j 4` 編譯成功，搜尋結果整頁與返回焦點調整的 CIV build 版本為 `HXB_C0.3.7_20260714_1642`；已完成 Desktop handoff，本次未發布。
- 已搬移 `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260714_1642.bin` 與 `.factory.bin`，來源與目的地 SHA256 一致。
- OTA SHA256: `37992d7cf8fd3fda1a4f5c12cf1767158e82eaa189d7dcd4cd188a036b7585a9`
- Factory SHA256: `962fb543e58efb55081a4315418fdc42eeab02754a9148dbcd346958a5862845`
- `platformio run -e heltec-wireless-tracker -j 4` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260714_1622`。
- 已依 handoff 搬移並驗證韌體產物：
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260714_1622.bin`
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260714_1622.factory.bin`
- OTA SHA256: `6eabd7777671ad2e05c403eb89be48b17e88b2b689fb4fa843f0f133819f02c5`
- Factory SHA256: `9cebf4975e86b480183e9655bc17163f913dd65311fb692d82dfaadd723bd0e5`

### 發布

- 發布 CIV build `HXB_C0.3.7_20260713_1838`，上一個 GitHub Release 為 `HXB_C0.3.7_20260711_0239`。
- 新增 release note：`docs/RELEASE_HXB_C0.3.7_20260713_1838.md`，記錄本版與上一版的完整差異。

### 新增

- TraceRoute `綁定節點` 清單最上方新增 `搜尋裝置`，可使用 ShortName 搜尋目前在線節點；清單順序調整為 `搜尋裝置`、`返回`、在線節點。
- 搜尋裝置沿用 GROUP PIN 的鍵盤配置，並新增獨立 `EXIT` 鍵。
- 搜尋結果新增置中小視窗：找到時顯示 ShortName / LongName，找不到時顯示查詢 ShortName，兩種結果都有可見的 `返回` 按鈕。

### 調整

- ShortName 搜尋使用不分英文字母大小寫的完整比對。
- 成功結果視窗返回後，游標會停在找到的節點，繼續沿用短按查看 `LongName`、role、最近一次聽到，以及長按 1 秒綁定的既有操作。
- 搜尋結果顯示期間會由結果視窗優先攔截輸入，避免底層清單誤觸詳情或長按綁定。

### 與上一版差異

- `HXB_C0.3.7_20260711_0239` 的主要新增是 Direct Home 隨機文案；本版新增的是 TraceRoute 綁定節點 ShortName 搜尋與完整結果互動。
- 上一版已有的 Direct Home 文案、URL 更新 timeout 與 TraceRoute 長按修正均保留，本版未移除既有功能。

### 驗證

- `platformio run -e heltec-wireless-tracker -j 4` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260713_1838`。
- 已依 handoff 搬移並驗證韌體產物：
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260713_1838.bin`
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260713_1838.factory.bin`
- OTA SHA256: `6a3f9a25742804a6a34e0a9c849a9627b1ac92f3aaa36c770f02cbfd1a8369ac`
- Factory SHA256: `6ee31f7620ab61f3b9c658ae933b3c232eb6c4012b147d395d6e1bb337ee0a5d`

## 2026-07-11

### 調整

- Direct Home 小威右側空白區改為開啟 Home 時隨機顯示一則 HermesX 文案；文字超出欄寬時會拆成兩段，每約 2 秒切換一次再循環。
- 文案切段納入 Direct Home base repaint 判斷，避免 direct TFT skip-ui 路徑讓右側文字停在舊段落。

### 驗證

- `platformio run -e heltec-wireless-tracker -j 4` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260711_0239`。
- 已依 handoff 搬移並驗證韌體產物：
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260711_0239.bin`
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260711_0239.factory.bin`
- OTA SHA256: `31f356e99c9f771949f65afda6ec500bd454f73f50169e165d6590e6d119d2fe`
- Factory SHA256: `16f07d51235b5a28100ada288a376d3c9b8c0ba61c985bd4cad0bd5c36c611ae`

## 2026-07-10

### 驗證

- `platformio run -e heltec-wireless-tracker -j 1` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260710_1912`。
- 已依 handoff 搬移並驗證韌體產物：
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260710_1912.bin`
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260710_1912.factory.bin`
- OTA SHA256: `66d0d4495b18322aac6ce0bd722dd669a00e7c4bae6b758a3ffeda13894cc428`
- Factory SHA256: `17b52641f50f7c8147b9b539784400d8f68a7829fa22c06e112f12623130e9cf`

## 2026-07-09

### 修正

- 放寬 URL 更新檢查與下載的 HTTP / HTTPS timeout：標頭讀取等待由 5 秒提高到 15 秒，下載中單次無資料容忍由 5 秒提高到 30 秒，降低手機熱點、弱 WiFi 或 HTTPS 來源短暫停頓時誤判 `URL 下載逾時` 的機率。
- URL 更新等待遠端標頭資料時會持續更新 UI 並餵 watchdog，避免長一點的網路等待被看成裝置卡死。

## 2026-07-08

### 修正

- 修正旋鈕鎖定長按範圍過大，導致 TraceRoute `綁定節點` / 快速 TraceRoute 的 1 秒長按綁定與解除綁定流程可能被 3 秒旋鈕鎖定邏輯干擾的問題。
- 旋鈕鎖定 3 秒長按現在只允許在固定主頁的 `TAK MODE` / 智慧功率頁生效；TraceRoute、ONLINE、GROUP、Finder、訊息、設定與任何 overlay 顯示期間不會觸發旋鈕鎖定，保留各頁既有長按語意。
- `ButtonThread` 與 `RotaryEncoderInterruptBase` 共用同一個 `shouldAllowRotaryLockLongPress()` 閘門，避免 shared hold pin 與 rotary interrupt 兩條路徑行為不一致。
- 修正 TraceRoute 1 秒長按叫出 `是否綁定？` 後，放開旋鈕時 rotary driver 又補送短按，導致確認框用預設 `否` 立即關閉的問題；TR 頁面中按住超過 1 秒的 rotary release 現在不再產生短按事件。

### 驗證

- `platformio run -e heltec-wireless-tracker -j 4` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260708_0925`。
- 已依 handoff 搬移並驗證韌體產物：
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260708_0925.bin`
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260708_0925.factory.bin`
- OTA SHA256: `dbc8f23817adb94c16d4adaf59d3297319a44c90d8da0ce0b352dc13923acee0`
- Factory SHA256: `7919a4821ce1871cac4ce62f62c1f3595179810a89523c6ae64b6273dca40336`

## 2026-07-06

### 調整

- `TAK MODE` 進入時會自動切換 LoRa preset 到 `Short_Fast`，搭配既有智慧功率降低 TAK / ATAK 封包 airtime；退出 TAK MODE 時會還原進入前的 preset 或 custom LoRa 參數。
- `TAK MODE` 智慧功率主頁在不改動原本 GROUP 與訊號資訊排版的前提下，於 shortName 左側顯示目前 TAK 頻道標籤，例如 `TAK A`。
- 新增旋鈕鎖定：長按旋鈕 3 秒可鎖定 / 解鎖旋鈕輸入，抵達長按秒數時會顯示 `旋鈕鎖定` 彈窗，並以 `解鎖` / `鎖定` 兩格反白目前狀態。
- 修正 Heltec Wireless Tracker 旋鈕長按被 `PowerHold` / EM 快捷路徑攔截的問題；當 rotary press pin 與 ButtonThread 的 hold pin 共用時，現在會以 ButtonThread 累計的 hold elapsed 優先在 3 秒觸發鎖定 / 解鎖，不再跳出 EM 模式進入警告。
- 修正 `旋鈕鎖定` 彈窗顯示期間左右旋事件繼續流到 TAK MODE 主頁，導致原本的 TAK 選單 / 頻道選擇把彈窗操作搶走的問題；彈窗現在會先吃掉左右旋、上下、確認與取消事件。

### 驗證

- `platformio run -e heltec-wireless-tracker -j 4` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260706_1944`。
- 已依 handoff 搬移並驗證韌體產物：
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260706_1944.bin`
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260706_1944.factory.bin`
- OTA SHA256: `eb4965263bef982d015e13d7aa935692c5d32051f30619205a772f6a35dd9445`
- Factory SHA256: `866ef8d7010c5ef90cd89924ffd3b02eaec56a8eda8b6d4bc83e21242b28376c`
- 實機 WiFi OTA 上傳驗證成功；`/upload-update-bin` 需使用 `PUT -T` 搭配 `X-Hermes-Filename`，不可用 multipart `curl -F`。上傳完成後裝置回報 `Streamed 2933216 bytes to OTA partition.`，下一步需在 HermesX `更新模式` 執行 `套用更新`。

## 2026-07-03

### Release

- 發布 CIV build `HXB_C0.3.7_20260703_1702`，包含 TAK MODE 簡化、智慧功率主頁、頻道選擇、GROUP 設定快速入口與 TAK 文件整理。
- 新增 release note：`docs/RELEASE_HXB_C0.3.7_20260703_1702.md`。

### 驗證

- `platformio run -e heltec-wireless-tracker -j 4` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260703_1702`。
- 已依 handoff 搬移並驗證韌體產物：
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260703_1702.bin`
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260703_1702.factory.bin`
- OTA SHA256: `68b3029477f39c80fe86b926000c2039b695ff1de6a2fc039324b35183d466c6`
- Factory SHA256: `2074899852a65c5dd45694aeb254ab4a41659befe76e30464a7383dddb3ef8d3`

## 2026-06-29

### 調整

- `TAK MODE` 進入與退出改用更新模式同款 transition 動畫，進入顯示 `進入TAK模式`，退出顯示 `退出TAK模式`，再排程重開機。
- `TAK MODE` 啟用期間會暫時退出並阻擋 CannedMessage menu，避免智慧功率 Home 短按叫出 TAK 選單時與罐頭訊息輸入打架。
- 修正 `TAK MODE` 彈窗 / 設定 / 頻道選擇無法被 Rotary 控制的問題；TAK 輸入改採 CannedMessage 同款 `rotEnc1` effective cw/ccw/press 解析。
- `TAK MODE` 智慧功率主頁改為右轉開 TAK 選單、左轉開 `頻道選擇`、短按回原本主選單。
- `頻道選擇` 的 `返回` 與 Cancel/Back 改為直接回 TAK 智慧功率主頁，不再跳回 TAK popup。
- `TAK MODE` 彈窗新增 `GROUP設定` 快速入口，直接共用現有 GROUP PIN A/B 與查看 PIN 流程；從此入口返回時會回到 GROUP 菜單，方便接著檢查已配對節點列表。
- 新增 `docs/TAK_MODE.md`，整理 TAK MODE 進出流程、智慧功率主頁、旋鈕操作、頻道選擇、GROUP 配對入口、CannedMessage 隔離與 CIV 版限制；README 與 docs index 已加入入口。
- Direct Home 原本的 direct clock overlay 改由常駐小威動畫取代，並與 GPS / NEON Clock buffer 分離；Home 小威固定顯示在螢幕左側，避免佔用舊 Home clock overlay buffer。
- 小威支援 `趴著` / `坐著` 姿勢輪替，兩種姿勢都改為 4 幀 sprite 尾巴動畫；尾巴改成水平掃動，避免看起來像上下抖動或分離的棒狀物。
- 小威動畫改為差異像素更新：只有進入 Home、位置或姿勢切換時才重畫整個區域，平常只更新尾巴變動像素，降低 ST7735 實機閃爍。

### 驗證

- `platformio run -e heltec-wireless-tracker` 編譯驗證成功，CIV build 版本為 `HXB_C0.3.7_20260629_2323`。
- 已依 handoff 搬移並驗證韌體產物：
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260629_2323.bin`
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260629_2323.factory.bin`
- `platformio run -e heltec-wireless-tracker` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260629_0439`。
- 已依 handoff 搬移並驗證韌體產物：
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260629_0439.bin`
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260629_0439.factory.bin`
- `platformio run -e heltec-wireless-tracker` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260629_1859`。
- 已依 handoff 搬移並驗證韌體產物：
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260629_1859.bin`
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260629_1859.factory.bin`

## 2026-06-23

### 新增

- `TAK` / `TAK Tracker` 角色自動啟用「智慧功率」：進入 TAK 類角色時會暫存原本 LoRa `tx_power`，先使用設定上限發送，再依 ACK、RSSI/SNR 與逾時狀態在最低/最高 dBm 邊界內保守調整；離開 TAK 類角色時會還原原本功率設定。
- 智慧功率在 TAK 類角色下會取樣所有 remote LoRa RX，包括 `Portnum=300` / EMHB 這類 `WantAck=0` broadcast；連續強訊號 broadcast 也能保守觸發降功率，不再只靠 ACK 流量調整。
- 智慧功率啟用時，Home frame 會切換成「智慧功率」儀表頁，顯示目前 LoRa 功率、最近 SNR/RSSI、RX 時間，以及 heard 到的 GROUP 裝置數。
- 智慧功率儀表頁針對 Heltec Wireless Tracker 窄螢幕重排：GROUP 框改為固定高度，RX 時間移到訊號資訊區，避免文字與框線重疊。
- `TAKMODE設定` 新增 `智慧功率低` / `智慧功率高`，可直接在裝置端調整智慧功率的最低與最高 dBm；此功能與 `聲光靜默` 分離，不會因 Silent 開關改變 LoRa 功率策略。

### 修正

- Home 時鐘授時來源收斂為手機 App 與本機 GPS：不再接受 mesh 其他節點、WiFi/Ethernet NTP 或開機硬體 RTC 回填來更新 Home 時間，避免未連手機/GPS 時被錯誤來源帶到錯時間。
- 修正手動 WiFi / USB 更新檔缺少 `HXB...` 檔名 metadata 時，更新模式把 ESP app descriptor 的版本字串顯示成待更新版本的問題。
- `/upload-update-bin`、USB/XModem 串流與本機 `/update/firmware.bin` 檢查現在都必須從檔名 hint 解析出 `HXB..._YYYYMMDD_HHMM`；缺少時會明確顯示 `更新檔名缺少 HXB 版本`，不再 fallback 到 `esp_app_desc_t.version`。
- 修正 `HXB_C0.3.7_YYYYMMDD_HHMM.bin` 這類版本前綴本身含底線的合法檔名被誤判為缺少 HXB 版本的問題；解析規則改以最後兩個底線切出日期與時間。
- `/update-info` 回報版本改用 `APP_HERMES_VERSION`，讓更新工具看到的裝置版本與 HermesX OTA 檔名規則一致。
- DirectHome neon buffer 配置失敗時改為 10 秒節流記錄，且智慧功率 Home 不再先嘗試配置舊 Home overlay buffer，避免 monitor 被重複 WARN 洗版。
- Heap 保護模式改為連續低水位 3 秒後才觸發，避免 TFT/BLE/Radio 瞬間 heap 碎片低點造成保護頁反覆彈出。
- 智慧功率 Home 啟用時會主動釋放既有 Home/GPS direct neon buffer，避免從舊 Home 進入 TAK 後仍保留高記憶體 UI 緩衝而誤觸 Heap 保護模式。
- 智慧功率無 remote RX/ACK 回饋的補功率等待由 120 秒縮短為約 45 秒，讓附近節點離線或斷開後更快回升發射功率。
- 智慧功率的 remote RX 廣域監聽不再處理本機 LOCAL 封包，避免本機 EMHB 心跳在 TAK 類角色下反覆進 hermesx 模組。
- GROUP presence 的 inactive EMHB 心跳最小間隔改為 60 秒，避免 Emergency UI 未啟用時仍每 5 秒廣播 `active=0`，造成 phone queue / packet history 長時間壓力。
- 智慧功率 Home 顯示條件改為只跟隨 runtime `SmartPower ON` 狀態，不再只因裝置角色值是 TAK / TAK Tracker 就提前取代 Home 頁。
- TAK MODE 退出時會把 role defaults、config、nodedb 與 devicestate 一併還原並保存；若舊狀態已把 TAK / TAK Tracker 誤存為原本角色，會防呆回 `Client`，避免退出後仍停在 `ROLE=TAK`。
- 智慧功率 Home 顯示條件補上 TAK MODE runtime 狀態：TAK MODE ON 後會立刻顯示智慧功率頁，退出時則立刻刷新 SmartPower OFF，避免畫面晚於 hermesx module tick 才切換。
- 智慧功率收訊記錄放寬為 TAK 類 role 也會更新 UI 訊號狀態，避免 monitor 已有 `rxRSSI` 但智慧功率頁仍顯示 `RSSI --`；同時新增 `SmartPower signal ...` DEBUG 便於現場比對。
- 智慧功率 Home 的螢幕右上角新增橫向小電池圖示，位置在 GROUP 視窗上方，沿用 HermesX Home 電池圖示樣式。

### 驗證

- `platformio run -e heltec-wireless-tracker -j 1` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260625_1830`。
- 已依 `docs/AI_UPDATE_HANDOFF.md` 搬移並驗證韌體產物：
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260625_1830.bin`
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260625_1830.factory.bin`

## 2026-06-22

### 調整

- 開機 Hermes 歡迎畫面改為新 neon logo 流程：四個節點依序出現，冷白紅藍外框線延伸完成後，淡入由參考圖轉出的黃色光暈 `Hermes` logo。
- Heltec Wireless Tracker BootHold 改走 RGB565 direct-draw：長按期間依序顯示四點、以即時計算端點平滑延伸線條，最後顯示黃色光暈 Hermes，並保留完成後進入原本 Meshtastic boot logo 的流程。
- Hermes BootHold TFT 線段不再用 15 張 keyframe 跳格播放，線條階段改用 runtime vector drawing；最後 Hermes 字樣仍使用由參考圖轉出的 RGB565 bitmap，避免字樣比例與顏色再次偏離。
- 冷開機、系統重開機與非 BootHold 開機的 Hermes welcome boot screen 強制走同一套 TFT direct renderer，自動以時間播放四點、線段、Hermes 動畫；BootHold gate 則仍由長按進度推動同一套動畫。
- 自動 Hermes welcome 改為獨立狀態機 `hermesXBootWelcomeActive`，不再共用 BootHold gate 的 `hermesXBootHoldActive` / nodeDB 收尾流程；自動 welcome 期間跳過 setup 初始 `ui->update()`，避免白/藍閃與舊 UI frame 蓋掉 direct 動畫。
- 開機動畫路徑新增 `[HermesBootAnim]` 診斷 Log，會輸出 setup 判斷、TFT direct renderer 入口、BootHold progress/reveal/finish，以及切回 Meshtastic boot logo 的時間點，方便比對實機序列埠與畫面差異。
- 依實機 Log 修正自動 Hermes welcome 的計時起點：`Screen::setup()` 只先畫第 0 幀，3 秒動畫改到 `Screen::runOnce()` 第一次進入 TFT direct renderer 時才開始，避免主程式初始化期間消耗動畫時間造成冷開機線段跳格。
- 自動 Hermes welcome 完成 3 秒動畫後保留最終 Hermes 畫面 1.2 秒再切回 Meshtastic boot logo，避免排程延後時最後一幀尚未完整顯示就被切走。
- 自動 Hermes welcome 改用 render-driven elapsed：每次實際畫面重繪最多只推進 50ms 動畫時間，避免開機初始化阻塞時直接從空白跳到線段完成或 Hermes 字樣。
- 自動 Hermes welcome 改為在 `Screen::setup()` 內先完成 TFT direct blocking playback，播完再進入後續模組初始化，避免 WS2812B startup animation 啟動時讓 Hermes 動畫中途停頓。
- 修正 blocking welcome 播完後同一輪 `Screen::setup()` 立即執行初始 `ui->update()`，造成 Meshtastic boot logo 疊到 Hermes 最終字樣上的閃爍。
- Heltec Wireless Tracker / V1.0 variant 的 `SCREEN_TRANSITION_FRAMERATE` 從 3fps 提高到 60fps，讓 Hermes welcome direct renderer、BootHold 與更新模式進入/退出 transition 不再被板級設定強制降到 3fps。
- HermesX 更新模式進入/退出 transition bar 改為依 elapsed 直接計算像素填充，減少進出更新模式時的跳格感。
- 修正 `TFTDisplay::writeRow565()` 寫入 RGB565 frame array 時未啟用 LovyanGFX byte swap，造成實機黃色 Hermes 顯示成藍紫色的問題。

### 驗證

- `platformio run -e heltec-wireless-tracker` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260623_1943`。
- 已依 `docs/AI_UPDATE_HANDOFF.md` 搬移並驗證韌體產物：
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260623_1943.bin`
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260623_1943.factory.bin`

## 2026-06-18

### 發布

- 建立 `HermesX_C0.3.7` CIV 分支，顯示版號更新為 `HXB_C0.3.7`。
- `platformio run -e heltec-wireless-tracker` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260618_1807`。
- 已依 `docs/AI_UPDATE_HANDOFF.md` 搬移並驗證韌體產物：
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260618_1807.bin`
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260618_1807.factory.bin`

### 修正

- `功能` 子頁面新增可見的 `退出` 項目，選取後會回到主選單的 `功能` 入口，避免只能靠返回鍵離開子頁。
- 主選單新增 `功能` 子頁面，將 `TAK MODE`、`MSG`、`ONLINE`、`TRACE`、`GROUP`、`尋人模組` 收進同一層功能頁；從 Home 短按進主選單仍預設停在第 6 項 `Home`。
- 修正 `HEAP 保護模式` 原本只顯示提醒、沒有真正停用高記憶體 UI 路徑的問題；低 heap 觸發後會立即進入 runtime 保護狀態，釋放 Home/GPS direct neon buffer，並阻止保護期間重新配置。
- 保護期間會停用 Home direct clock、自訂霓虹時鐘、dog overlay、Home footer 快捷入口、主快捷選單入口與 Home 旋鈕開啟罐頭選單，避免低記憶體狀態下繼續進入較重的 UI 路徑。
- `退出` 只會暫時壓低提醒，不會解除保護；保護狀態會等 free heap 與 largest block 回到安全水位後自動解除。
- 修正 `旋鈕對調` 只有設定頁狀態、重開後實體方向未真正反轉的問題；rotary driver 現在會依 HermesX 對調狀態交換 CW/CCW 事件，設定後也會即時套用。

## 2026-06-04

### 新增

- `設定 > UI設定` 新增新訊息提示開關，可控制 HermesX 新訊息大提示框是否顯示。

### 調整

- HermesX 新訊息 popup 改為大提示框格式，顯示 `NEW MSG`、來源短 ID、訊息摘要與 `查看 / 略過` 操作，並維持約 3 秒提示時間。
- `MSG / Recent Send` 詳細訊息頁只放大正文內容；Recent Send 列表維持原本小字體與原本版面。
- 詳細訊息正文支援自動換行與上下捲動，長訊息可像 TraceRoute 詳細內容一樣往下看。

### 修正

- 修正新訊息 popup 顯示時底層 TFT palette 色彩區域殘留，導致籃色或其他底層色塊卡在提示框中的問題。
- 修正 popup 按下 `查看` 後固定開啟 Recent Send 最新索引、與實際 popup 訊息不一致的問題；現在會依 popup 綁定封包尋找對應訊息。
- 修正 popup 查看路徑和 CannedMessage / Recent Send 輸入擁有權打架，導致進入詳細頁後滾動、返回或按鍵操作異常的問題。
- 修正 `MSG / Recent Send` 詳細訊息頁按下 Press / Select 無法退出的問題；現在會回到 Recent Send 列表。
- 修正詳細訊息頁中文與英文正文大小不一致的問題；中文 glyph 改依英文正文高度重採樣，不再硬套固定 1x / 2x。
- 修正詳細訊息 payload 未依長度限制讀取的風險，改以 `payload.size` 安全複製並補上結尾字元。
- 修正停留在 Recent Send 列表或詳細頁時仍可能被 Footer 快捷鍵或自動輪播帶走的問題。

### 驗證

- `git diff --check -- src/graphics/Screen.cpp docs/ISSUE_msg_popup_recent_send_2026-06-03.md` 通過。
- `platformio run -e heltec-wireless-tracker -j 4` 編譯成功，CIV build 版本為 `HXB_C0.3.2_20260604_1946`。
- 已依 `docs/AI_UPDATE_HANDOFF.md` 搬移並驗證韌體產物：
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.2_20260604_1946.bin`
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.2_20260604_1946.factory.bin`
- 尚待實機最終確認 popup 查看操作、詳細訊息捲動與中英文字級一致性。

## 2026-06-01

### 新增

- 主快捷頁新增獨立 `TraceRoute` 功能頁；進入後顯示類似 ONLINE 的節點列表，選取節點後進入 TraceRoute 專用 detail，焦點預設停在 `開始TraceRoute`，可直接送出路由測試。
- `裝置管理 > 更新模式` dedicated update environment 第一層新增 `WiFi設定`，讓 URL 更新檢查與 WiFi 手動更新共用同一份 WiFi 設定入口。

### 調整

- `ONLINE` / `GROUP` 節點 detail 的 `MSG` 改為 Screen-native 直接訊息鍵盤，不再跳到 CannedMessage composer；支援畫面鍵盤、實體鍵盤字元輸入、刪除與送出。
- `WiFi更新` 子頁移除 `WiFi設定`，改為專注顯示目前版本、連線狀態與 `開始更新`。
- TraceRoute 結果 popup 改用更清楚的訊號文案：最上方顯示 `本機收到: RSSI ... / SNR ...`，去程與回程每一跳顯示 `訊號SNR`。

### 修正

- 修正 ONLINE / TraceRoute 節點列表只顯示已收到 NodeInfo/User 的節點，導致對方已傳訊息但仍顯示「沒有在線節點」的問題；現在有最近 last_heard 的節點會以 Node ID fallback 顯示。
- 修正裝置尚未取得有效網路/GPS 時間時，收到對方 NodeInfo 會觸發綠色提示但 `last_heard` 仍為 0，導致 ONLINE / TraceRoute 看不到該節點的問題。
- 修正 TraceRoute tile 與 TraceRoute 清單 / detail 的長文字可能換行超出框線的問題；長節點名稱現在只顯示框內第一行。
- 修正 TraceRoute 在主快捷選單未被選取時可能顯示成 GROUP 縮寫的問題；未選取狀態固定顯示 `TR`。
- 修正 ONLINE / GROUP detail 的 MSG 編輯器無法從畫面鍵盤退出、在小螢幕顯示超出，以及私訊固定走 primary channel 的問題；現在有 `EXIT` 鍵並使用目標節點記錄的 channel。
- 修正 ONLINE / GROUP detail 的 MSG 畫面鍵盤文字未置中與 `EXIT` 後可能停在空白畫面的問題；MSG 現在沿用 Group PIN 設定頁的鍵盤繪製方式，退出後會強制重繪原 detail 頁。

### 驗證

- `platformio run -e heltec-wireless-tracker` 編譯成功，CIV build 版本為 `HXB_C0.3.2_20260602_1411`。
- 已依 `docs/AI_UPDATE_HANDOFF.md` 搬移韌體產物：
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.2_20260602_1411.bin`
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.2_20260602_1411.factory.bin`

## 2026-05-29

### 修正

- 修正 CIV build 的 GROUP 節點清單永遠顯示 `沒有已配對節點` 的問題；GROUP Heartbeat 現在只要已設定 GROUP PIN 就會在非 EMAC 狀態下維持同組 presence，CIV 不需要進入 EMAC 也能讓同組裝置出現在 `GROUP > 節點列表`。
- 修正 `GROUP設定 > EMINFO設定 > EMINFO廣播` 切換開/關後跳到黑底全螢幕提示且無法退出的問題；切換提示改回設定頁內 toast，不再觸發 Screen alert frame。
- 修正尋人模式收到同組 `POSITION: OK` ack 但對方未立即重送 position 時，12 秒 timeout 會清空既有有效位置節點的問題；現在授權 ack 搭配 NodeDB 既有有效位置也會完成本次尋人結果。

### 驗證

- `platformio run -e heltec-wireless-tracker` 編譯成功，CIV build 版本為 `HXB_C0.3.2_20260529_1559`。
- 已依 `docs/AI_UPDATE_HANDOFF.md` 搬移韌體產物：
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.2_20260529_1559.bin`
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.2_20260529_1559.factory.bin`

## 2026-05-26

### 修正

- 從 GOV 同步 BLE node-only config 修正：手機只要求節點資訊時不再重建整個 file manifest，避免不必要的檔案系統掃描造成 log 停在 `SPI lock acquired for file manifest` 後 PANIC 重開機。

### 驗證

- `git diff --check -- docs/CHANGELOG.md docs/CHANGELOG_MINI.md src/mesh/PhoneAPI.cpp` 通過。
- `platformio run -e heltec-wireless-tracker` 編譯成功，CIV build 版本為 `HXB_C0.3.2_20260526_2006`。
- 已依 `docs/AI_UPDATE_HANDOFF.md` 搬移韌體產物：
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.2_20260526_2006.bin`
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.2_20260526_2006.factory.bin`

## 2026-05-21

### 修正

- 從 GOV 同步 `CannedMessage` / Screen input ownership 修正：`TAK MODE`、GROUP/ONLINE/Finder/Recent 等 HermesX 專用頁不再被 CannedMessage 的 rotary/input observer 搶走輸入；CannedMessage 只在合法私訊 composer 狀態接管輸入，其他 Screen-owned 頁面會直接阻擋或退出 CannedMessage。
- 修正 frame 位置預設值為 `0` 導致 Home frame 0 被誤判成 Recent detail / 其他不存在頁面的問題；所有不存在的 frame position 改以 `0xFF` 表示，並在 active-page 判斷時檢查 `frameCount`，避免再次出現 `captureReturnTarget=3 frame=0` 這類誤判。

### 驗證

- `git diff --check` 通過。
- `platformio run -e heltec-wireless-tracker` 編譯成功，CIV build 版本為 `HXB_C0.3.2_20260521_1341`。

## 2026-05-15

### 修正

- 修正 `尋人模式` 收到對方 `POSITION` 定位回傳後可能跳到黑畫面的問題；現在尋人模組有自己的清單與明細 frame，不再共用 `ONLINE` frame/input，成功收到同 GROUP/EM 密碼授權且有座標的節點後，會進入尋人模組自己的 `尋人清單` 並提供明確 `離開` 選項。
- 修正 `尋人清單` / `尋人模式` frame 已切換且 TFT overlay 已執行，但畫面仍可能全黑的問題；現在尋人 frame 每次繪製都會明確清黑底並重設白色前景，避免沿用前一頁留下的 BLACK 繪圖狀態。
- 修正旁聽到別人的 TraceRoute 回覆、或被其他節點 TraceRoute 時，HermesX 會誤跳出 TraceRoute 結果頁的問題；現在只會顯示本機剛主動送出的 TraceRoute 回覆。

### 驗證

- `git diff --check` 通過。
- `platformio run -e heltec-wireless-tracker` 編譯成功。
- 已依 `docs/AI_UPDATE_HANDOFF.md` 搬移韌體產物：
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.2_20260515_1702.bin`
  - `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.2_20260515_1702.factory.bin`

## 2026-05-09

### 修正

- 修正 EM UI 收到 `RESET: EMAC` 時未重新檢查 GROUP 授權的問題；未授權封包現在不會讓本機退出 EMAC / EMUI。
- 修正低記憶體保護頁預設選中 `清除節點` 的危險行為；現在彈出時預設選 `退出`，避免誤按清空 NodeDB。
- 修正 GROUP 節點清單可能漏掉只送出 Heartbeat 的同組裝置；Heartbeat 現在可建立最小節點狀態並更新 LastHB。
- EMINFO / Heartbeat 接收端恢復相容 v1 舊格式，同時保留 v2 GROUP fingerprint 檢查，避免新舊韌體混跑時互相看不到。
- 修正 GROUP PIN A/B fingerprint 發送不對稱；A/B 兩組 PIN 都有設定時會分別送出對應 fingerprint，避免 B-only 同組裝置看不到 A+B 裝置。
- 修正 EMAC active 開機時跳過 Stealth restore 後仍留下 persisted stealth state 的問題；現在會清掉殘留，避免解除 EMAC 後下次開機又套回 Stealth。
- 修正 Lighthouse persisted state 檔案太短時未關閉檔案 handle 的問題。

### 驗證

- `git diff --check` 通過。
- `platformio run -e heltec-wireless-tracker` 編譯成功。
- 已依 `docs/AI_UPDATE_HANDOFF.md` 搬移韌體產物：
  - `/Users/oldways/Desktop/HermesX韌體/HXB0.2.9_20260508_2026.bin`
  - `/Users/oldways/Desktop/HermesX韌體/HXB0.2.9_20260508_2026.factory.bin`

## 2026-05-07

### 改進

- `裝置管理 > 更新模式` 入口改為按下後立即切到獨立更新動畫頁並排程重開機；畫面中央顯示 `重開中`，不再停留在原本選單頁只於底部顯示提示。
- 更新模式進出動畫統一使用同一套更新轉場頁：進入更新模式顯示 `進入更新模式`，退出 dedicated update environment 時顯示 `退出更新模式中`。
- WiFi / USB 手動更新頁的底部條狀進度改為甜甜圈式圓形進度動畫，保留百分比資訊並降低小螢幕上的橫向擁擠。

### 修正

- 修正從 Home 短按進入 HermesX 主選單時，預設焦點停在第一項 `潛行模式` 的問題；現在會直接停在 `Home` 項目，符合從 Home 進選單的操作預期。
- 修正主選單側邊 action tile 顯示英文項目時可能被 `drawMixedBounded()` 擠到換行的問題；`ONLINE`、`GROUP`、`MSG`、`Home` 等純 ASCII label 現在改用不換行的置中繪製。
- 修正更新模式進入/退出等待重開機時仍停在上一層設定頁的問題；等待期間現在會留在專用更新轉場頁，避免使用者誤以為還能操作原本選單。
- 修正 EMAC 進入後可能沒有蜂鳴器警報聲的問題；舊版內部偏好檔 `/prefs/hermesx_emui_buzzer.txt` 若曾被寫成 `0`，會導致 EM UI siren 被持久化靜音。
- EM UI 啟動時會移除舊的 siren 偏好檔，讓 EMAC 蜂鳴器只受 `UI設定 > 全域蜂鳴器` 控制。
- Stealth / TAK 進入時改為只暫時關閉 EM siren runtime 狀態，退出時恢復原狀，不再寫入 EMAC siren 偏好。
- Heltec Wireless Tracker 的蜂鳴器腳位維持既有 GPIO17 路徑，未改動硬體腳位設定。

### 驗證

- `platformio run -e heltec-wireless-tracker` 編譯成功。
- 已依 `docs/AI_UPDATE_HANDOFF.md` 搬移韌體產物：
  - `/Users/oldways/Desktop/HermesX韌體/HXB0.2.9_20260507_1428.bin`
  - `/Users/oldways/Desktop/HermesX韌體/HXB0.2.9_20260507_1428.factory.bin`

## 2026-05-06

### 新增

- 主選單新增 `GROUP` 入口；進入後先顯示 `GROUP設定 / 節點列表`，將設定與已配對節點狀態收斂到同一個 GROUP 區。
- `節點列表` 顯示已配對節點、目前 EM 狀態與 `在線 / 延遲 / 離線`，在線判斷沿用 EMINFO Heartbeat。
- `GROUP DETAIL` 新增節點明細，可查看 EM 狀態、LastHB、電量、地/物、座標、高度與 Node ID。
- `GROUP DETAIL` 提供 `MSG` 與 `TraceRoute` 操作，操作語意與 `ONLINE DETAIL` 對齊。

### 修正

- `GROUP DETAIL` 對 MQTT 來源節點維持 `TraceRoute: --` / `LORA ONLY` 限制，避免送出不合理的 mesh route 請求。
- 從 `GROUP > GROUP設定` 進入既有 GROUP 設定頁時，返回會回到 GROUP 子選單，不會落到 Fast Setup 根目錄。
- `Screen` frame 容量同步擴充，避免新增 GROUP/ONLINE 等 HermesX action destination frame 後超出原先額外 frame 配置。

## 2026-05-01

### 新增

- `ONLINE DETAIL` 重新補回節點操作項目：
  - `MSG`：可直接從 ONLINE 節點明細開啟對該節點的私訊編輯。
  - `TraceRoute`：可直接從 ONLINE 節點明細對 LoRa 節點送出路由追蹤。
- `ONLINE DETAIL` 新增 `Link: LORA / MQTT` 顯示，讓使用者可分辨節點最近來源。
- `設定 > 裝置管理 > 節點資料庫 > 重設資料庫` 新增 `全部清除`，可保留本機節點並清空其他節點資料。

### 修正

- 修正 `ONLINE DETAIL` 中 TraceRoute helper 已存在但沒有任何可操作入口的問題；現在可從節點明細直接觸發。
- 修正 `ONLINE DETAIL` 畫面列數與輸入 handler 可選列數不一致，導致游標可能移到空白 action 位置的問題。
- TraceRoute 現在只允許對 LoRa/local 節點送出；若節點最近來源為 MQTT，畫面顯示 `TraceRoute: --`，按下時提示 `LORA ONLY` 並播放失敗回饋，避免送出不合理的 mesh route 請求。
- `節點資料庫 > 重設資料庫` 的清理選項數量已同步更新，`全部清除` 可正常被游標選取與執行。

### 驗證

- `platformio run -e heltec-wireless-tracker` 編譯成功。
- 已依 `docs/AI_UPDATE_HANDOFF.md` 搬移韌體產物：
  - `/Users/oldways/Desktop/HermesX韌體/HXB0.2.9_20260501_0609.bin`
  - `/Users/oldways/Desktop/HermesX韌體/HXB0.2.9_20260501_0609.factory.bin`

## 2026-04-30

### 新增

- 更新模式完成 WiFi / USB / URL 三條更新路徑整合：
  - `檢查更新` 走 URL 來源，可檢查、下載、顯示候選版本與套用更新。
  - `手動更新 > WiFi更新` 可啟動裝置端上傳服務，透過電腦傳送韌體。
  - `手動更新 > USB更新` 可使用 USB-C 線傳送韌體。
- dedicated update environment 啟動後新增短暫進場動畫：`UPDATE CORE / syncing...`，動畫結束後自動進入更新模式頁，任意輸入可跳過。
- 新增 HermesX build 版本命名：更新流程與韌體檔名使用 `HXB0.2.9_YYYYMMDD_HHMM`；`APP_VERSION` 仍保持 Meshtastic 官方相容版本。
- WiFi 更新支援 `X-Hermes-Filename` header，裝置可從檔名取得待更新版本。
- URL 更新支援從 `Content-Disposition` 或 URL 檔名推導 HermesX 待更新版本。
- 新增更新工具：
  - `tools/hermesx_wifi_update.py`：自動尋找 WiFi 更新裝置並上傳韌體。
  - `tools/hermesx_usb_update.py`：透過 USB 傳送韌體。
- 建立固定交接規則：每輪編譯成功後需搬移韌體產物到桌面韌體資料夾，並提供 WiFi 更新指令，方便後續 AI 或操作者直接刷寫測試。

### 改進

- 更新 UI 改為流程式設計，WiFi / USB / URL 更新頁統一使用狀態文字、版本資訊、條狀進度與底部操作按鈕。
- `目前版本`、`更新來源`、`目前狀態`、`待更新版本`、`下載進度`、`最後錯誤` 等詳細資訊改用小彈窗顯示，不再整頁切換。
- `目前版本` 詳細資訊改顯示完整 HermesX build 版本，而不只顯示 `HXB0.2.9`。
- WiFi 更新收到電腦上傳請求時，先執行 UI preflight 顯示接收狀態，再開始讀取韌體資料，避免畫面停在等待傳送。
- URL 下載流程拆成檢查與下載狀態，降低在小螢幕上狀態文字互相覆蓋的機率。

### 修正

- 修正 WiFi / USB / URL 更新頁在 160x80 螢幕上的換行、底部按鈕與進度文字重疊問題。
- 修正更新詳細資訊頁看起來像整頁切換的問題，改為保留背景頁並覆蓋小彈窗。
- 修正 WiFi 更新完成後因 partition description 讀取失敗而誤判流程失敗的問題；當 `esp_ota_end` 成功時允許接收流程完成。
- 修正更新進度曾因上一輪更新殘留狀態而首次進入就顯示 100% 的問題。

## 補記（先前已完成但漏記）

### 新增

- Fast Setup `裝置管理` 新增 `更新模式` 頁面，可在裝置端直接執行 `檢查更新檔`、`開始更新`、`套用更新` 與 `取消更新`。
- 新增 `HermesXUpdateManager`，統一管理本機 OTA 流程與狀態切換：`Idle / AwaitingImage / Writing / ReadyToApply / Failed`。
- 更新模式頁面新增唯讀狀態資訊：`目前版本`、`待寫入版本`、`來源狀態`、`寫入進度`、`最後錯誤`。

### 改進

- OTA 更新流程改為先檢查 `/update/firmware.bin`，驗證 image header、app descriptor、版本資訊與 OTA 槽位容量後，再分段寫入目標 partition。
- 套用更新前會保存待套用狀態到 `/prefs/hermesx_update_pending.bin`，裝置重開後可判斷新版本是否已成功接手開機。

## 2026-04-11

### 新增

- Emergency Mode 新增 `五線回報` 流程；選擇 `受困 / 醫療 / 物資` 後不再直接送出，改為進入表單頁，填寫 `人 / 事 / 時 / 地 / 物` 後再送出。
- `各裝置狀態` 新增兩層資訊架構：
  - 上層清單僅顯示 `人 / 事 / 多久以前聽到`
  - 點進單一裝置可查看完整明細
- 單一裝置明細新增完整欄位：
  - `人`
  - `事`
  - `時`
  - `地`
  - `物`
  - `經度`
  - `緯度`
  - `高度`
  - `相對位置`
- `相對位置` 新增獨立頁面，可在本機與對方都有有效 GPS 時顯示：
  - 相對方位
  - 大約距離
  - 方位角度
- `EM Heartbeat` 正式落地，作為 EM 模式下的短週期在線同步機制，用於支援裝置狀態頁的最近聽到時間與在線資訊。
- `EMAC設定` 新增 `EMINFO設定` 子頁入口，將 EMINFO 相關設定集中管理，不再散落顯示。

### 修正

- 修正 `五線回報` 頁面在小螢幕上固定只顯示前四個項目、後續欄位無法出現的問題；現在改為真正可捲動的列表。
- 修正 `EMUI` 內快速連按時，外層按鍵事件與 rotary raw press 三擊仍可能再次觸發本地 EM 入口的問題。
- 修正 `EMAC解除` 僅遠端裝置會退出、本機不會同步退出 EMUI 的問題；現在本機執行解除時也會立刻退出 EMUI。
- 修正 `EMAC解除` 仍會把裝置卡在 `Router` 類角色的問題；EM 流程不再自動強制切為 `Router`。
- 修正 `EMUI` 顯示期間 `WS2812B` 仍沿用平時呼吸燈動畫的問題；現在會正確切入 Emergency lamp 狀態。
- 修正 `各裝置狀態` 清單將狀態字樣誤當成時間資訊顯示的問題；時間欄位現在優先顯示回報當下的 time code，沒有有效 time code 時退回顯示 `多久以前`。

### 改進

- `EMAC` 啟動主流程改為走 `PORTNUM_HERMESX_EMERGENCY (300)`，不再只依賴 `TEXT_MESSAGE_APP` 的 `@EmergencyActive`。
- `EMAC解除` 改為與啟動流程共用相同的 EM 專用封包與同一組 `GROUP PIN`；啟動與解除現在分別使用：
  - `ACTIVATE: EMAC GROUP <pin>`
  - `RESET: EMAC GROUP <pin>`
- `尋人模式`、`EMAC`、`EMINFO/Heartbeat` 授權收斂為 `GROUP`：同 PIN 裝置才會接受控制封包，狀態同步也會以 GROUP fingerprint 過濾不同群組。
- 舊版 `@EmergencyActive` 文字控制路徑暫時保留為相容模式，避免新舊韌體混跑時完全失去互通。
- `EMINFO` 與 `EM Heartbeat` 收斂為 Hermes 私有 payload，並與 `回報統計`、`各裝置狀態` 分離：
  - `回報統計` 僅統計主回報封包
  - `各裝置狀態` 則使用 `EMINFO / EMHB / 五線回報`
- `EMAC` 週期保護從 `60 秒` 調整為 `90 秒`；逾時後送出的週期訊號改為 `STATUS: LOST`。
- `STATUS: LOST` 現在會讓尚未進入 EM 的裝置進入 EM mode，但不會把已經在 EMUI 內操作的使用者強制拉回主頁。
- 若裝置在上次關機前仍處於 `EMAC active`，開機後會自動恢復並重新叫出 EMUI。
- `EMUI` 顯示期間的 `WS2812B` 改為 `60 BPM` 紅燈閃爍，用於明確區分 EM 模式與一般待機燈效。

### 備註

- 目前 EM 啟動/解除主流程已走 Hermes 自定義 EM 風包，但最底層 routing ACK 仍由 Meshtastic 原生 routing 機制處理，屬正常行為。
- 目前 `EMINFO`、`EM Heartbeat` 與 `EMAC設定` 已納入 `302.1.1` extension 規劃，詳見：
  - [REF_3021.md](/Users/oldways/HermesX/docs/REF_3021.md)
  - [REF_30211_status.md](/Users/oldways/HermesX/docs/REF_30211_status.md)

## 2026-04-06

### 修正

- 修正 Fast Setup 從 `設定` 退出後誤跳到 `QRCODE` 分享頁的問題；現在會正確返回 HermesX 操作頁。
- 修正停留在 `UI設定` 期間若收到 `NACK` 通知，畫面會被強制拉回 `Home` 的問題；現在會回到原本所在的設定頁。
- 修正無有效時間來源時，Home 頁德牧動畫在 TFT direct-render 路徑中可能出現整頁重繪、殘影、雙重疊圖與腳部被裁切的問題。
- 修正德牧 sprite 切幀時尾巴附近誤帶入背景亮點，導致看起來像白底殘留的問題。

### 新增

- Fast Setup `UI設定` 新增 `時區` 設定，可直接在裝置端選擇固定時區偏移並保存到裝置設定。
- Home 頁在尚未取得任何有效時間來源時，新增德牧像素動畫與 `請連接手機` 提示文字，取代原本的 `--:--:--` 無效時間顯示。

### 改進

- 調整 `UI設定` 的燈光命名，避免兩個不同功能都顯示為「狀態燈設定」：
  - `WS2812` 相關項目改名為 `Hermes狀態條`
  - 板載燈改名為 `板載RGB燈`
- `Hermes狀態條` 相關子頁標題與提示文案同步調整，避免不同頁面使用舊稱呼。
- 無授時狀態下的德牧動畫改為依參考圖轉換的 sprite 呈現，不再使用程式化簡化圖形。
- 德牧動畫調整為每次重新進入 Home 時輪替 `趴著` / `坐著` 兩種姿勢，並持續執行尾巴搖動動畫。
- 德牧動畫版位改為固定靠左，優先避開右側衛星與電量資訊區，並將本體尺寸放大至 `48x48`。

### 備註

- `時區` 設定僅影響時間顯示方式，不提供新的授時來源；若裝置尚未從 GPS / 手機 / 其他來源取得有效時間，Home 頁仍會顯示無授時 fallback 動畫與提示。

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
