## 2026-07-15
- Released CIV build `HXB_C0.3.7_20260715_0142`；TraceRoute 綁定節點改存入持久化空間，重新開機後會自動載入，暫時離線也不會被自動解除綁定。
- TraceRoute request 等待回應上限由 10 秒調整為 30 秒；逾時顯示 `等待回應逾時`，送出前失敗仍顯示 `SEND FAIL`。
- `platformio run -e heltec-wireless-tracker` 編譯成功；OTA SHA256: `6310d9c21c198aedadfc550823d3c276722753fb0168779a0db9d8b506a75ed5`。

## 2026-07-14
- Released CIV build `HXB_C0.3.7_20260714_1808`；TAK Tracker 完整共用 TAK MODE 新 UI、智慧功率、TAK profile、頻道選擇、GROUP、尋人與 CannedMessage 輸入隔離，同時保留 `TAK_TRACKER` role 行為。
- App 建立的 Primary / Secondary 自訂頻道、名稱、PSK、uplink/downlink 與位置分享設定不會被 TAK 頻道選擇覆寫。
- 本版同時收錄 TraceRoute 整頁搜尋結果、直接綁定／返回，以及旋鈕實際 GPIO 長按來源修正。
- `platformio run -e heltec-wireless-tracker -j 4` 編譯成功；OTA SHA256: `131f1d890d1aae36ca8b6adf915f5493a2678a8444b8b4adcdeb064fdee8274f`。
- TraceRoute ShortName 搜尋結果改為整頁顯示，完整呈現 `ShortName` 與自動換行的 `LongName`；成功時最下方提供 `綁定`、`返回`，找不到時提供 `返回`。
- 搜尋結果按下 `綁定`、`返回` 或取消後都回到 `綁定節點` 清單，游標固定停在 `返回` 選項；`HXB_C0.3.7_20260714_1642` 已完成目標板編譯與 Desktop handoff，本次未發布。
- `_1642` OTA SHA256: `37992d7cf8fd3fda1a4f5c12cf1767158e82eaa189d7dcd4cd188a036b7585a9`
- 修正 TraceRoute 長按來源混用：綁定／解除綁定只接受按下當時已位於對應頁面的實際旋鈕長按，Home 或其他按鍵事件不再能跨頁誤觸取消綁定。
- 依實機 log 修正 `rotEnc1` 已放開送出短按後，`ButtonThread` 延遲一秒誤呼叫解除綁定；頁面長按動作不再由 shared OneButton 狀態執行。
- 修正旋鈕按住輪詢的 interval 被 `INT32_MAX` 回傳值覆蓋，現在每 50ms 讀取實際 GPIO，使 TR 1 秒長按與 Home 3 秒鎖定恢復且不受假按壓影響。
- `platformio run -e heltec-wireless-tracker -j 4` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260714_1622`；韌體產物已搬到 `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260714_1622.bin` 與 `.factory.bin`。
- OTA SHA256: `6eabd7777671ad2e05c403eb89be48b17e88b2b689fb4fa843f0f133819f02c5`
- Released CIV build `HXB_C0.3.7_20260713_1838`；相較上一版 `HXB_C0.3.7_20260711_0239`，TraceRoute `綁定節點` 新增 ShortName 搜尋入口、鍵盤 `EXIT` 與置中搜尋結果視窗。
- 搜尋結果找到節點時顯示 ShortName / LongName，按 `返回` 後游標停在該節點，並保留短按詳情、長按綁定；找不到時會在同款小視窗顯示查詢 ShortName。
- `platformio run -e heltec-wireless-tracker -j 4` 編譯成功；韌體產物已搬到 `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260713_1838.bin` 與 `.factory.bin`。
- OTA SHA256: `6a3f9a25742804a6a34e0a9c849a9627b1ac92f3aaa36c770f02cbfd1a8369ac`

## 2026-07-11
- Direct Home 小威右側空白區改為開啟 Home 時隨機顯示一則 HermesX 文案；文字超出欄寬時會拆成兩段，每約 2 秒切換一次再循環。
- `platformio run -e heltec-wireless-tracker -j 4` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260711_0239`；韌體產物已搬到 `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260711_0239.bin` 與 `.factory.bin`。
- OTA SHA256: `31f356e99c9f771949f65afda6ec500bd454f73f50169e165d6590e6d119d2fe`

## 2026-07-10
- `platformio run -e heltec-wireless-tracker -j 1` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260710_1912`；韌體產物已搬到 `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260710_1912.bin` 與 `.factory.bin`。
- OTA SHA256: `66d0d4495b18322aac6ce0bd722dd669a00e7c4bae6b758a3ffeda13894cc428`

## 2026-07-09
- 放寬 URL 更新檢查與下載的 HTTP / HTTPS timeout：標頭讀取等待由 5 秒提高到 15 秒，下載中單次無資料容忍由 5 秒提高到 30 秒，降低手機熱點、弱 WiFi 或 HTTPS 來源短暫停頓時誤判 `URL 下載逾時` 的機率。
- URL 更新等待遠端標頭資料時會持續更新 UI 並餵 watchdog，避免長一點的網路等待被看成裝置卡死。

## 2026-07-08
- 修正旋鈕鎖定 3 秒長按範圍過大，導致 TraceRoute `綁定節點` / 快速 TraceRoute 的 1 秒長按綁定與解除綁定可能被干擾的問題。
- 旋鈕鎖定長按現在只允許在固定主頁的 `TAK MODE` / 智慧功率頁生效；TraceRoute、ONLINE、GROUP、Finder、訊息、設定與 overlay 顯示期間不會觸發旋鈕鎖定。
- `ButtonThread` 與 `RotaryEncoderInterruptBase` 共用 `shouldAllowRotaryLockLongPress()` 閘門，避免 shared hold pin 與 rotary interrupt 路徑不一致。
- 修正 TraceRoute 1 秒長按叫出 `是否綁定？` 後，放開旋鈕時 rotary driver 又補送短按，導致確認框用預設 `否` 立即關閉的問題。
- `platformio run -e heltec-wireless-tracker -j 4` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260708_0925`；韌體產物已搬到 `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260708_0925.bin` 與 `.factory.bin`。
- OTA SHA256: `dbc8f23817adb94c16d4adaf59d3297319a44c90d8da0ce0b352dc13923acee0`

## 2026-07-06
- `TAK MODE` 進入時會自動切換 LoRa preset 到 `Short_Fast`，搭配既有智慧功率降低 TAK / ATAK 封包 airtime；退出 TAK MODE 時會還原進入前的 preset 或 custom LoRa 參數。
- `TAK MODE` 智慧功率主頁在不改動原本 GROUP 與訊號資訊排版的前提下，於 shortName 左側顯示目前 TAK 頻道標籤，例如 `TAK A`。
- 新增旋鈕鎖定：長按旋鈕 3 秒可鎖定 / 解鎖，抵達長按秒數時顯示 `旋鈕鎖定` 彈窗並反白目前狀態。
- 修正 Heltec Wireless Tracker 旋鈕長按被 `PowerHold` / EM 快捷路徑攔截的問題；rotary press pin 與 ButtonThread hold pin 共用時，會以 hold elapsed 優先在 3 秒觸發鎖定 / 解鎖。
- 修正 `旋鈕鎖定` 彈窗顯示期間左右旋事件繼續流到 TAK MODE 主頁，導致 TAK 選單 / 頻道選擇把彈窗操作搶走的問題。
- `platformio run -e heltec-wireless-tracker -j 4` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260706_1944`；韌體產物已搬到 `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260706_1944.bin` 與 `.factory.bin`。
- OTA SHA256: `eb4965263bef982d015e13d7aa935692c5d32051f30619205a772f6a35dd9445`
- 實機 WiFi OTA 上傳驗證成功；`/upload-update-bin` 使用 `PUT -T` 與 `X-Hermes-Filename`，不可用 multipart `curl -F`，上傳後需在 HermesX `更新模式` 執行 `套用更新`。

## 2026-07-03
- Released CIV build `HXB_C0.3.7_20260703_1702`，包含 TAK MODE 簡化、智慧功率主頁、頻道選擇、GROUP 設定快速入口與 `docs/TAK_MODE.md`。
- `platformio run -e heltec-wireless-tracker -j 4` 編譯成功；韌體產物已搬到 `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260703_1702.bin` 與 `.factory.bin`。
- OTA SHA256: `68b3029477f39c80fe86b926000c2039b695ff1de6a2fc039324b35183d466c6`

## 2026-06-29
- `TAK MODE` 進入與退出改用更新模式同款 transition 動畫，分別顯示 `進入TAK模式` / `退出TAK模式`，再排程重開機。
- `TAK MODE` 啟用期間會暫時退出並阻擋 CannedMessage menu，避免智慧功率 Home 短按叫出 TAK 選單時與罐頭訊息輸入打架。
- 修正 `TAK MODE` 彈窗 / 設定 / 頻道選擇無法被 Rotary 控制的問題；TAK 輸入改採 CannedMessage 同款 `rotEnc1` effective cw/ccw/press 解析。
- `TAK MODE` 智慧功率主頁改為右轉開 TAK 選單、左轉開 `頻道選擇`、短按回原本主選單。
- `頻道選擇` 的 `返回` 與 Cancel/Back 改為直接回 TAK 智慧功率主頁，不再跳回 TAK popup。
- `TAK MODE` 彈窗新增 `GROUP設定` 快速入口，直接共用 GROUP PIN A/B、查看 PIN 與配對節點檢查流程。
- 新增 `docs/TAK_MODE.md`，整理 TAK MODE 主頁、旋鈕操作、頻道選擇、GROUP 配對入口與 CIV 版限制。
- `platformio run -e heltec-wireless-tracker` 編譯驗證成功，CIV build 版本為 `HXB_C0.3.7_20260629_2323`；韌體產物已搬到 `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260629_2323.bin` 與 `.factory.bin`。
- `platformio run -e heltec-wireless-tracker` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260629_1859`；韌體產物已搬到 `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260629_1859.bin` 與 `.factory.bin`。
- Direct Home 原 direct clock overlay 改由常駐小威動畫取代，並與 GPS / NEON Clock buffer 分離；Home 小威固定在螢幕左側。
- 小威 `趴著` / `坐著` 兩種姿勢都改為 4 幀 sprite 尾巴動畫，尾巴以水平掃動呈現，避免上下抖動或像分離棒狀物。
- 小威動畫改用差異像素更新，平常只更新尾巴變動像素，降低 ST7735 實機閃爍。
- `platformio run -e heltec-wireless-tracker` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260629_0439`；韌體產物已搬到 `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260629_0439.bin` 與 `.factory.bin`。

## 2026-06-23
- Home 時鐘授時來源收斂為手機 App 與本機 GPS：不再接受 mesh 其他節點、WiFi/Ethernet NTP 或開機硬體 RTC 回填來更新 Home 時間，避免未連手機/GPS 時被錯誤來源帶到錯時間。
- `TAK` / `TAK Tracker` 角色自動啟用「智慧功率」：進入時暫存原本 LoRa `tx_power`，先使用設定上限發送，再依 ACK、RSSI/SNR 與逾時在最低/最高 dBm 邊界內保守調整；離開 TAK 類角色時還原原本功率設定。
- 智慧功率在 TAK 類角色下會取樣所有 remote LoRa RX，包括 `Portnum=300` / EMHB 這類 `WantAck=0` broadcast；連續強訊號 broadcast 也能保守觸發降功率，不再只靠 ACK 流量調整。
- 智慧功率啟用時，Home frame 會切換成「智慧功率」儀表頁，顯示目前 LoRa 功率、最近 SNR/RSSI、RX 時間，以及 heard 到的 GROUP 裝置數。
- 智慧功率儀表頁針對 Heltec Wireless Tracker 窄螢幕重排：GROUP 框改為固定高度，RX 時間移到訊號資訊區，避免文字與框線重疊。
- `TAKMODE設定` 新增 `智慧功率低` / `智慧功率高`，可直接調整智慧功率最低與最高 dBm；此功能與 `聲光靜默` 分離。
- 修正手動 WiFi / USB 更新檔缺少 `HXB...` 檔名 metadata 時，更新模式把 ESP app descriptor 的版本字串顯示成待更新版本的問題。
- `/upload-update-bin`、USB/XModem 串流與本機 `/update/firmware.bin` 檢查現在都必須從檔名 hint 解析出 `HXB..._YYYYMMDD_HHMM`；缺少時會明確顯示 `更新檔名缺少 HXB 版本`，不再 fallback 到 `esp_app_desc_t.version`。
- 修正 `HXB_C0.3.7_YYYYMMDD_HHMM.bin` 這類版本前綴本身含底線的合法檔名被誤判為缺少 HXB 版本的問題；解析規則改以最後兩個底線切出日期與時間。
- `/update-info` 回報版本改用 `APP_HERMES_VERSION`，讓更新工具看到的裝置版本與 HermesX OTA 檔名規則一致。
- DirectHome neon buffer 配置失敗時改為 10 秒節流記錄，且智慧功率 Home 不再先嘗試配置舊 Home overlay buffer，避免 monitor 被重複 WARN 洗版。
- Heap 保護模式改為連續低水位 3 秒後才觸發，避免 TFT/BLE/Radio 瞬間 heap 碎片低點造成保護頁反覆彈出。
- 智慧功率 Home 啟用時會主動釋放既有 Home/GPS direct neon buffer，避免從舊 Home 進入 TAK 後仍保留高記憶體 UI 緩衝而誤觸 Heap 保護模式。
- 智慧功率無 remote RX/ACK 回饋的補功率等待由 120 秒縮短為約 45 秒，讓附近節點離線或斷開後更快回升發射功率。
- 智慧功率不再處理本機 LOCAL EMHB 心跳；GROUP presence inactive 心跳最小間隔改為 60 秒，降低 phone queue / packet history 壓力。
- 智慧功率 Home 顯示條件改為只跟隨 runtime `SmartPower ON` 狀態，不再只因角色值是 TAK / TAK Tracker 就提前出現。
- TAK MODE 退出時會同步還原 role defaults 並保存 config / nodedb / devicestate；舊狀態若已把 TAK 誤存為原本 role，會防呆回 `Client`。
- 智慧功率 Home 補看 TAK MODE runtime 狀態，TAK MODE ON 後會立刻顯示，退出時同步刷新 SmartPower OFF。
- 智慧功率收訊記錄放寬為 TAK 類 role 也會更新 UI 訊號狀態，避免 monitor 已有 `rxRSSI` 但 UI 仍顯示 `RSSI --`；新增 `SmartPower signal ...` DEBUG。
- 智慧功率 Home 的螢幕右上角新增橫向小電池圖示，位置在 GROUP 視窗上方，沿用 HermesX Home 電池圖示樣式。
- `platformio run -e heltec-wireless-tracker -j 1` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260625_1830`。
- 韌體產物已依 handoff 搬到 `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260625_1830.bin` 與 `.factory.bin`。

## 2026-06-22
- 開機 Hermes 歡迎畫面改為新 neon logo 流程：四個節點依序出現，冷白紅藍外框線延伸完成後淡入黃色光暈 `Hermes` logo。
- Heltec Wireless Tracker BootHold 改走 RGB565 direct-draw：長按期間依序顯示四點、以 runtime vector drawing 平滑延伸線條，最後顯示黃色光暈 Hermes，並保留完成後進入原本 Meshtastic boot logo 的流程。
- Hermes BootHold TFT 線段不再用 15 張 keyframe 跳格播放；最後 Hermes 字樣仍使用由參考圖轉出的 RGB565 bitmap，避免字樣比例與顏色再次偏離。
- 冷開機、系統重開機與非 BootHold 開機的 Hermes welcome boot screen 強制走同一套 TFT direct renderer，自動以時間播放四點、線段、Hermes 動畫；BootHold gate 則仍由長按進度推動同一套動畫。
- 自動 Hermes welcome 改為獨立狀態機，不再共用 BootHold gate 的 nodeDB 收尾流程；自動 welcome 期間跳過 setup 初始 `ui->update()`，避免白/藍閃與舊 UI frame 蓋掉 direct 動畫。
- 開機動畫路徑新增 `[HermesBootAnim]` 診斷 Log，方便從序列埠確認 setup、TFT direct renderer、BootHold progress/reveal/finish 與切回 Meshtastic boot logo 的實際順序。
- 依實機 Log 修正自動 Hermes welcome 的計時起點：`setup` 只先畫第 0 幀，3 秒動畫改到 `runOnce` 第一次進入 TFT direct renderer 時才開始，避免冷開機線段跳格。
- 自動 Hermes welcome 完成 3 秒動畫後保留最終 Hermes 畫面 1.2 秒再切回 Meshtastic boot logo，避免最後一幀被提早切走。
- 自動 Hermes welcome 改用 render-driven elapsed：每次實際重繪最多推進 50ms，避免開機初始化阻塞時直接跳到線段完成或 Hermes 字樣。
- 自動 Hermes welcome 改為先在 `Screen::setup()` 內完成 TFT direct blocking playback，播完後才進入後續模組初始化，避免 WS2812B startup animation 讓 Hermes 動畫中途停頓。
- 修正 blocking welcome 播完後同一輪 setup 立即 `ui->update()`，導致 Meshtastic boot logo 疊到 Hermes 最終字樣上的閃爍。
- Heltec Wireless Tracker / V1.0 variant 的 `SCREEN_TRANSITION_FRAMERATE` 從 3fps 提高到 60fps，讓 Hermes welcome、BootHold 與更新模式進入/退出 transition 不再被板級設定強制降到 3fps。
- 更新模式進入/退出 transition bar 改為依 elapsed 直接計算像素填充，減少跳格感。
- 修正 `TFTDisplay::writeRow565()` RGB565 byte swap，避免實機黃色 Hermes 顯示成藍紫色。
- `heltec-wireless-tracker` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260623_1943`。
- 韌體產物已依 handoff 搬到 `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260623_1943.bin` 與 `.factory.bin`。

## 2026-06-18
- 建立 `HermesX_C0.3.7` CIV 分支，顯示版號更新為 `HXB_C0.3.7`；`heltec-wireless-tracker` 編譯成功，CIV build 版本為 `HXB_C0.3.7_20260618_1807`。
- 韌體產物已依 handoff 搬到 `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260618_1807.bin` 與 `.factory.bin`。
- `功能` 子頁面新增可見的 `退出` 項目，選取後會回到主選單的 `功能` 入口，避免只能靠返回鍵離開子頁。
- 主選單新增 `功能` 子頁面，將 `TAK MODE`、`MSG`、`ONLINE`、`TRACE`、`GROUP`、`尋人模組` 收進同一層功能頁；從 Home 短按進主選單仍預設停在第 6 項 `Home`。
- 修正 `HEAP 保護模式` 只顯示提示、沒有真正停用高記憶體 UI 路徑的問題；低 heap 觸發後會立即釋放 Home/GPS direct neon buffer，並阻止保護期間重新配置。
- 保護期間會關閉 Home footer 快捷入口、主快捷選單入口與 Home 旋鈕開啟罐頭選單，避免低記憶體狀態下繼續進入較重的互動頁。
- `退出` 只暫時壓低提示，不會解除保護；當 free heap 與 largest block 回到安全水位後才會自動解除。
- 修正 `旋鈕對調` 只有設定頁狀態、重開後實體方向未真正反轉的問題；rotary driver 現在會依 HermesX 對調狀態交換 CW/CCW 事件，設定後也會即時套用。

## 2026-06-04
- `設定 > UI設定` 新增新訊息提示開關，可控制 HermesX 新訊息大提示框是否顯示。
- 新訊息 popup 改為大提示框格式，顯示 `NEW MSG`、來源短 ID、訊息摘要與 `查看 / 略過`，提示時間約 3 秒。
- 修正新訊息 popup 顯示時底層 TFT palette 色彩區域殘留，導致籃色或其他底層色塊卡在提示框中的問題。
- 修正 popup 按下 `查看` 後固定開啟 Recent Send 最新索引、與實際 popup 訊息不一致的問題；現在會依 popup 綁定封包尋找對應訊息。
- 修正 popup 查看路徑和 CannedMessage / Recent Send 輸入擁有權打架，導致進入詳細頁後滾動、返回或按鍵操作異常的問題。
- `MSG / Recent Send` 列表維持原本小字體與原本版面；只有詳細訊息頁正文放大。
- 詳細訊息正文支援自動換行與上下捲動，長訊息可像 TraceRoute 詳細內容一樣往下看。
- 修正詳細訊息頁按下 Press / Select 無法退出的問題；現在會回到 Recent Send 列表。
- 修正詳細訊息頁中文與英文正文大小不一致的問題；中文 glyph 改依英文正文高度重採樣，不再硬套固定 1x / 2x。
- `platformio run -e heltec-wireless-tracker -j 4` 編譯成功，CIV build 版本為 `HXB_C0.3.2_20260604_1946`。
- 韌體產物已依 handoff 搬到 `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.2_20260604_1946.bin` 與 `.factory.bin`。
- 尚待實機最終確認 popup 查看操作、詳細訊息捲動與中英文字級一致性。

## 2026-06-01
- `ONLINE` / `GROUP` 節點 detail 的 `MSG` 改為 Screen-native 直接訊息鍵盤，不再跳到 CannedMessage composer；可用畫面鍵盤或實體鍵盤輸入並直接送出私訊。
- `裝置管理 > 更新模式` dedicated update environment 第一層新增 `WiFi設定`，供 URL 更新與 WiFi 手動更新共用；`WiFi更新` 子頁改為只保留版本、連線狀態與開始更新。
- 主快捷頁新增獨立 `TraceRoute` 功能：進入後顯示類似 ONLINE 的節點列表，選取節點後可直接按 `開始TraceRoute`，並保留 MQTT-only 節點的 `LORA ONLY` 防呆。
- TraceRoute 結果 popup 文案改為明確顯示 `本機收到: RSSI ... / SNR ...`、`去程訊號`、`回程訊號` 與每一跳 `訊號SNR`，避免誤解為 NodeDB 資訊或 per-hop RSSI。
- 修正 ONLINE / TraceRoute 節點列表要求節點必須有 NodeInfo/User 的問題；現在只要 NodeDB 有 2 小時內的 last_heard，即使對方只傳過訊息也會用 Node ID 顯示。
- 修正裝置尚未取得有效網路/GPS 時間時，收到對方 NodeInfo 會觸發綠色提示但 `last_heard` 仍為 0，導致 ONLINE / TraceRoute 看不到該節點的問題。
- 修正 TraceRoute tile 與 TraceRoute 清單 / detail 的長文字可能換行超出框線的問題；長節點名稱現在只顯示框內第一行。
- 修正 TraceRoute 在主快捷選單未被選取時可能顯示成 GROUP 縮寫的問題；未選取狀態固定顯示 `TR`。
- 修正 ONLINE / GROUP detail 的 MSG 編輯器無法從畫面鍵盤退出、在小螢幕顯示超出，以及私訊固定走 primary channel 的問題；現在有 `EXIT` 鍵並使用目標節點記錄的 channel。
- 修正 ONLINE / GROUP detail 的 MSG 畫面鍵盤文字未置中與 `EXIT` 後可能停在空白畫面的問題；MSG 現在沿用 Group PIN 設定頁的鍵盤繪製方式，退出後會強制重繪原 detail 頁。
- `heltec-wireless-tracker` 編譯成功，CIV build 版本為 `HXB_C0.3.2_20260602_1411`。
- 韌體產物已依 handoff 搬到 `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.2_20260602_1411.bin` 與 `.factory.bin`。

## 2026-05-29
- 修正 CIV build 的 GROUP 節點清單永遠顯示 `沒有已配對節點`；GROUP Heartbeat 現在只要有 GROUP PIN，就會在非 EMAC 狀態下維持同組 presence。
- 修正 `GROUP設定 > EMINFO設定 > EMINFO廣播` 切換後跳到黑底提示且無法退出的問題。
- 修正尋人模式收到同組 `POSITION: OK` ack 但對方未立即重送 position 時，timeout 會清空既有有效位置節點的問題。
- `heltec-wireless-tracker` 編譯成功，CIV build 版本為 `HXB_C0.3.2_20260529_1559`。
- 韌體產物已依 handoff 搬到 `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.2_20260529_1559.bin` 與 `.factory.bin`。

## 2026-05-26
- 從 GOV 同步 BLE node-only config 修正：手機只要求節點資訊時會跳過 file manifest rebuild，避免不必要的檔案系統掃描造成 PANIC 重開機。
- `heltec-wireless-tracker` 編譯成功，CIV build 版本為 `HXB_C0.3.2_20260526_2006`。
- 韌體產物已依 handoff 搬到 `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.2_20260526_2006.bin` 與 `.factory.bin`。

## 2026-05-21
- 從 GOV 同步 `CannedMessage` / Screen input ownership 修正：`TAK MODE` 等 HermesX 專用頁不再被 CannedMessage 搶走 rotary/input，並修正 frame 位置預設為 `0` 造成 Home frame 0 被誤判成 Recent detail 的根因。
- `heltec-wireless-tracker` 編譯成功，CIV build 版本為 `HXB_C0.3.2_20260521_1341`。

## 2026-05-15
- 修正 `尋人模式` 收到對方 `POSITION` 回傳後可能跳到黑畫面的問題；現在尋人模組有自己的清單與明細 frame，不再共用 `ONLINE` frame/input，成功收到同 GROUP/EM 密碼授權且有座標的節點後，會進入尋人模組自己的 `尋人清單` 並提供明確 `離開` 選項。
- 修正 `尋人清單` / `尋人模式` frame 已切換且 TFT overlay 已執行，但畫面仍可能全黑的問題；現在尋人 frame 每次繪製都會明確清黑底並重設白色前景，避免沿用前一頁留下的 BLACK 繪圖狀態。
- 修正旁聽到別人的 TraceRoute 回覆、或被其他節點 TraceRoute 時會誤跳出 TraceRoute 結果頁的問題；現在只顯示本機主動送出的 TraceRoute 回覆。
- `heltec-wireless-tracker` 編譯成功，韌體產物已依 handoff 搬到 `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.2_20260515_1702.bin` 與 `.factory.bin`。

## 2026-05-09
- `TAK MODE` 入口改為獨立盾牌 icon 頁：短按會叫出彈窗操作頁，可切換 TAK ON/OFF、進入 `TAKMODE設定`、啟動 EMUI / 尋人模組，並從彈窗選擇返回主選單；開啟後離開頁面仍維持 TAK，重開機後也會自動恢復，`TAKMODE設定` 可調整裝置資訊廣播、GPS 刷新、位置廣播、SmartPosition 門檻與聲光靜默。
- 修正 `TAK MODE` 彈窗選單在小螢幕上不會捲動的問題；現在只繪製可視列，選取項超出畫面時會跟著捲到後續選項。
- 修正退出 `TAK MODE` 後聲光靜默可能殘留的問題；現在會記錄 TAK 是否真的套用過靜默，關閉 TAK 或在 TAK 中關閉靜默時會還原狀態 LED、緊急燈、EMUI siren、heartbeat 與 buzzer enable 腳。
- 修正 TAK 靜默期間 `noTone()` 釋放 buzzer 後沒有重新接回 HermesX 音效通道的問題；退出 TAK 靜默與 emergency tone 自動停止後會重新初始化 buzzer 輸出，避免音效仍像被關閉。
- 修正退出 `TAK MODE` 後一般音效仍被 `outputsDisabled` 擋住的問題；現在 TAK role 進出會立即重跑 output policy，退出後一般 HermesX 音效會跟著恢復，不必等下一輪狀態刷新或重開機。
- 停用殘留的 Rotary 三擊本地 EMAC 入口；本機進入 EMAC 改以旋鈕長按 3 秒倒數確認為準，避免開機後快速連按誤觸。
- 修正 EM UI 收到未授權 `RESET: EMAC` 也會退出本機 EMAC / EMUI 的問題；現在解除也會走 GROUP 授權檢查。
- 修正低記憶體保護頁預設選中 `清除節點` 的危險行為；彈出時改為預設 `退出`，避免誤清 NodeDB。
- GROUP Heartbeat 現在可建立最小節點狀態，避免只送 Heartbeat 的同組裝置不出現在 `節點列表`。
- EMINFO / Heartbeat 接收端相容 v1 舊格式，v2 仍保留 GROUP fingerprint 檢查，降低新舊韌體混跑斷層。
- GROUP PIN A/B 都設定時會分別送出對應 fingerprint，避免 B-only 同組裝置看不到 A+B 裝置。
- EMAC active 開機時若抑制 Stealth restore，會清掉殘留 stealth state，避免解除 EMAC 後下次開機又套回 Stealth。
- `heltec-wireless-tracker` 編譯成功，韌體產物已依 handoff 搬到 `/Users/oldways/Desktop/HermesX韌體/HXB0.2.9_20260509_0505.bin` 與 `.factory.bin`。

## 2026-05-07
- `裝置管理 > 更新模式` 入口改為按下後直接切到獨立更新動畫頁，中央顯示 `重開中` 並排程重開機，不再停留在原本設定頁只顯示底部提示。
- 更新模式進出轉場統一：進入顯示 `進入更新模式`，退出 dedicated update environment 顯示 `退出更新模式中`。
- WiFi / USB 手動更新頁的條狀進度改為甜甜圈式圓形進度動畫，降低小螢幕上的擁擠感。
- 修正從 Home 短按進入 HermesX 主選單時預設停在 `潛行模式` 的問題；現在進選單會直接停在 `Home`。
- 修正主選單側邊英文項目可能被擠到換行的問題；`ONLINE`、`GROUP`、`MSG`、`Home` 等純 ASCII label 改為不換行置中顯示。
- 修正 EMAC 進入後可能沒有蜂鳴器警報聲的問題；舊 `/prefs/hermesx_emui_buzzer.txt` 會被清除，Stealth/TAK 也只暫時靜音，不再持久關閉 EM siren。
- `heltec-wireless-tracker` 編譯成功，韌體產物已依 handoff 搬到 `/Users/oldways/Desktop/HermesX韌體/HXB0.2.9_20260507_1428.bin` 與 `.factory.bin`。

## 2026-05-06
- 主選單新增 `GROUP` 入口；進入後先顯示 `GROUP設定 / 節點列表`，把設定與配對節點狀態收斂在同一個 GROUP 區。
- `節點列表` 顯示已配對且送出 `EMINFO/Heartbeat` 的節點，包含節點名稱、EM 狀態與 `在線 / 延遲 / 離線`；在線判斷沿用 `EM Heartbeat` 的最近收到時間。
- `GROUP DETAIL` 可查看節點 EM 狀態、LastHB、電量、地/物、座標與 Node ID，並提供 `MSG` 與 `TraceRoute` 操作；MQTT 節點維持 `LORA ONLY` 限制。

## 2026-05-01
- `尋人模式`、`EMAC` 與 `EMINFO/Heartbeat` 授權收斂為 `GROUP`：裝置需使用同一組 `GROUP PIN`，EMINFO/Heartbeat 也會用 GROUP fingerprint 過濾不同群組。
- `GROUP設定` 取代原 `EMAC設定` 內的密碼語意，設定項改為 `GROUP PIN A/B`、`查看GROUP PIN` 與 `解除EMAC`；舊 `/prefs/lighthouse_passphrase.txt` 仍沿用，避免既有裝置遺失設定。
- EM 專用封包送出格式改為 `ACTIVATE: EMAC GROUP <pin>`、`RESET: EMAC GROUP <pin>`、`REQUEST: POS GROUP <pin>`，接收端仍相容舊的 `<pass>` 格式。
- `尋人模式` 選單補齊：入口 icon 改為雷達/準星風格，文案調整為 `發送尋人訊號 / 位置訊息`，不再沿用舊的 `尋人模組` 命名。
- 修正 `發送尋人訊號` 確認 popup 的收尾流程：`取消`、手動發送與 3 秒倒數自動發送都會正確回到有效頁面，不再掉入黑屏。
- `發送尋人訊號` 新增前景雷達掃描動畫，並改為持續顯示直到真的收到任一 `POSITION` 回報才切進 `位置訊息`；若 `12s` 內都沒有位置回報，則顯示失敗提示。
- `位置訊息` / Finder 節點列表沿用 `ONLINE` 節點瀏覽架構，可直接查看節點位置與相關資訊，避免另開一套獨立列表流程。

## 2026-04-15
- 新增 `尋人模式` 入口；進入後分為 `發送尋人訊號` 與 `位置訊息` 兩層，不再直接覆蓋原本的 `ONLINE`。
- `發送尋人訊號` 走 `REQUEST: POS <pass>` 輕量位置點名；收到且授權通過的同頻裝置會各自廣播一次自己的位置封包。
- `發送尋人訊號` 確認 popup 改為沿用既有確認框流程，提供 `取消 / 廣播` 選項；`廣播` 需等 `3 秒` 倒數完成後才可選取。
- `位置訊息` 清單顯示最近在線且有有效位置的節點；點入明細後只顯示 `LongName / LastHeard / location / 經 / 緯 / 高度 / 相對位置`

## 2026-04-30
- 更新模式完成 WiFi / USB / URL 三條更新路徑整合：手動更新分為 `WiFi更新` 與 `USB更新`，URL 更新支援檢查、下載、進度與套用更新。
- 更新 UI 統一為流程式頁面：狀態、版本資訊、條狀進度與底部操作按鈕一致化；詳細資訊改為小彈窗，避免整頁切換與文字重疊。
- dedicated update environment 啟動後新增 `UPDATE CORE / syncing...` 進場動畫，重開機進入更新環境時先顯示短暫同步畫面，再進入更新模式。
- 版本顯示拆分為 HermesX build 版本：`APP_VERSION` 保持 Meshtastic 相容版本，更新流程與韌體檔名使用 `HXB0.2.9_YYYYMMDD_HHMM`。
- WiFi 更新支援 `X-Hermes-Filename`，可從檔名取得待更新版本；URL 更新可從 `Content-Disposition` / URL 檔名推導版本。
- 新增更新輔助工具：`tools/hermesx_usb_update.py`、`tools/hermesx_wifi_update.py`，並建立固定交接規則：每輪編譯成功後需搬移韌體產物到桌面韌體資料夾，並提供 WiFi 更新指令。
- 修正 WiFi 更新接收時 UI 沒有先切到接收狀態的問題；現在收到上傳請求會先進行 UI preflight，再開始讀取韌體資料。
- 修正 OTA 完成後因 partition description 讀取失敗導致流程誤判失敗的問題；`esp_ota_end` 成功時允許繼續完成接收。
- 修正 URL 更新頁與手動更新頁在 160x80 小螢幕上的換行、底部按鈕、進度文字重疊與殘留 toast 問題。

## 補記（先前已完成但漏記）
- Fast Setup `裝置管理` 新增 `更新模式`，可在裝置端檢查 `/update/firmware.bin`、開始寫入 OTA、套用或取消更新。
- 新增 `HermesXUpdateManager` 管理 `Idle / AwaitingImage / Writing / ReadyToApply / Failed` 狀態與 OTA 寫入進度。
- 更新模式頁面可顯示 `目前版本 / 待寫入版本 / 來源狀態 / 寫入進度 / 最後錯誤`，並在重開後依 `/prefs/hermesx_update_pending.bin` 判斷是否已成功套用。

## 2026-04-11
- EM 新增 `五線回報`；`受困 / 醫療 / 物資` 先進表單，填完 `人 / 事 / 時 / 地 / 物` 後再送出。
- `各裝置狀態` 改為兩層：上層只顯示 `人 / 事 / 多久以前聽到`，點入後可看完整明細與 `相對位置`。
- 單一裝置明細新增 `經度 / 緯度 / 高度 / 相對位置`；若本機與對方都有有效 GPS，可顯示相對方位、距離與角度。
- `EM Heartbeat` 落地，用於 EM 狀態下的短週期在線同步。
- `EMAC設定` 新增集中式 `EMINFO設定` 子頁。
- 修正 `五線回報` 頁面後續欄位無法捲動顯示的問題。
- 修正 `EMUI` 內快速連按會再次觸發本地 EM 入口的問題。
- 修正本機執行 `EMAC解除` 時不會同步退出 EMUI 的問題。
- 修正 EM 流程仍可能把角色卡在 `Router` 的問題；現在不再自動強制切成 `Router`。
- `EMAC` 啟動改走 `port 300` 的 `ACTIVATE: EMAC <pass>`，`EMAC解除` 改走 `RESET: EMAC <pass>`，兩者共用同一組密碼。
- `EMAC` emergency port 新增輕量位置脈衝指令 `REQUEST: POS <pass>`；同頻道裝置收到且授權通過後，各自廣播一次自己的位置封包。
- 週期保護從 `60 秒` 改為 `90 秒`，逾時後改送 `STATUS: LOST`；未進入 EM 的裝置會被帶入 EM，已在 EM 中的裝置不再被強制拉回主頁。
- 若上次關機前仍處於 EM，開機後會自動恢復並重新叫出 EMUI。
- `EMUI` 顯示期間，`WS2812B` 改為 `60 BPM` 紅燈閃爍。

## 2026-04-06
- 修正 Fast Setup 從 `設定` 退出後誤跳 `QRCODE` 的問題；現在會正確回 HermesX 操作頁。
- 修正停留在 `UI設定` 時若收到 `NACK` 會被踢回 `Home` 的問題；現在會回到原本頁面。
- `UI設定` 命名調整：`WS2812` 改為 `Hermes狀態條`，板載燈改為 `板載RGB燈`。
- Fast Setup `UI設定` 新增 `時區` 設定，可直接保存固定時區偏移。
- Home 在尚未取得有效時間時，改為顯示德牧像素動畫與 `請連接手機` 提示，不再顯示 `--:--:--`。
- 德牧 fallback 動畫改為參考圖 sprite，支援 `趴著 / 坐著` 姿勢輪替、尾巴搖動，並修正殘影、雙重疊圖、背景白點與腳部裁切問題。

## 2026-03-27
- 修正 HermesX 多個滾動式選單的 redraw 不同步問題：內部選取索引變更後，畫面現在會即時跟上，不再出現「log/狀態已切換，但顯示停在前一項」。
- 修正 `CannedMessage` 選單移動時 `UIFrameEvent` 未穩定帶 action 的問題，避免某些上下移動路徑沒有正確要求 redraw。
- 調整 `Screen::setFastFramerate()`：固定畫面互動時會強制下一次 UI tick 立即生效，不再被 `OLEDDisplayUi` 的 FPS budget 延後一幀。
- Fast Setup `裝置管理 > 電源管理` 新增唯讀 `當前電壓` 顯示，可直接查看目前電池電壓。

## 2026-03-26
- Fast Setup `裝置管理 > LoRa` 新增 `Role` 入口，可直接在裝置上切換 `Client / Client Mute / Client Hidden / Tracker / Sensor / TAK / TAK Tracker / Lost&Found`。
- `Home` / `GPS` 頁改為只在 `Client`、`Client Mute`、`Client Hidden` 存在；`Tracker` 等非 client 類角色保留選單與 Fast Setup，但不再載入這兩頁。
- Home/GPS direct neon 相關大塊 buffer 改為 role-aware 的 lazy allocation；切到非 client 類角色時會釋放，不再常駐佔用 RAM。
- 現場驗證顯示：`TAK` 模式 free heap 顯著高於 `Client`，表示這批 UI/neon 記憶體已不再於非 client 類角色常駐。
- 現場量測參考值：重開後 `TAK` 約 `147872` bytes free heap、`139252` largest block；重開後 `Client` 進 Home 約 `45548` bytes free heap、`36852` largest block。

## 2026-03-20
- Home 與 GPS neon 改為共用 scratch buffer，減少 direct-TFT 霓虹快取的重複常駐 RAM。
- Home 時鐘 region 上限縮為實際 `152x37`，GPS title neon 再少一塊 `coreMask` 常駐陣列。
- 修正共用 scratch 初版過小造成的 Home 時鐘右側殘影/錯位。
- 修正 InkHUD 訊息 Banner 短按時無法直接進入 `Recent Send / All Messages`，現在會優先跳到訊息頁。
- HermesX 新訊息 popup 顯示時間延長為 `5s`。
- `Recent Send` 列表摘要改為 UTF-8 安全渲染，修正中文/混合文字顯示異常。
- `Recent Send` detail 頁新增上下滾動與右側 scrollbar，版面同步壓緊以減少空白。
- HermesX popup 的 `Press` 直開最新訊息 detail 仍有已知時序問題，這輪尚未完全修復。
- 現場量測顯示 free heap 明顯回升；先前頻繁 panic 高度懷疑與記憶體壓力過高有關。

## 2026-03-23
- Fast Setup `裝置設定` 改名為 `裝置管理`，新增 `節點資料庫 > 重設資料庫`，可手動清除 `12hr / 24hr / 48hr` 未更新節點。
- 新增低記憶體提醒 popup：當 `free < 6KB` 或 `largest < 4KB` 時，會提醒使用者前往 `裝置管理 > 節點資料庫` 清理，但不自動刪除資料。
- 低記憶體提醒改為搭配三連高音蜂鳴；進入 Stealth 時也同步使用較明顯的蜂鳴提示。
- `UI設定` 新增 `旋鈕對調`，`rotEnc1` 與 `CannedMessage` 已同步改為吃設定，不再硬編碼方向。
- 新增 `tools/capture_monitor_log.sh`，可將 `platformio device monitor` 輸出同步保存到 `logs/monitor/`，便於長時間追 log 與 crash 前後比對。

## 2026-03-19
- 修正 BLE 配對後 HermesX 分支在設定同步/收包時較易異常的問題，`getFiles()` 已同步回官方版實作。
- `狀態燈亮度 = 0` 現在只關 LED，不再把蜂鳴器一起靜音。
- 修正 `狀態燈亮度` 重開機後不保留的問題，改為獨立存到 `/prefs/hermesx_ui_led_brightness.txt`。
- 修正亮度為 `0` 時，開機動畫、一般 LED 動畫與長按電源提示仍可能亮起的問題。
- UI 文案由 `WS2812亮度` 改為 `狀態燈亮度`。

## 2026-03-12
- GPS 座標顯示微調：改為「只縮整體、不壓字距」，座標維持半尺寸顯示（`coordHalfScale=true`）且字距回復正常（`coordTracking=0`），避免數字擠壓。
- 註記 `Neon effect` 繪製邏輯：`renderDirectNeonPattanakarnText()` 先合併字形 layer map（逐像素取最大層級），再依層級 palette 以 scanline run 方式輸出，形成「外暈 -> 內暈 -> 核心」。
- 註記 `Bloom effect` 繪製邏輯（GPS 球體）：`renderDirectGpsGlobeBloom()` 以 40 層同心填圓堆疊，亮度由外圈 `1%` 遞增至內圈 `72%`，外層先畫、內層後畫。
- 註記 `Bloom effect` 繪製邏輯（Home 右下球體）：`renderDirectHomeOrangeMesh()` 以 20 層橘色同心 Bloom 堆疊，亮度由外圈 `5%` 遞增至內圈 `34%`，再疊加線框與節點。

## 2026-03-07
- GPS 頁視覺改版：TFT Hero 版強化 `GPS ON` 標題與座標霓虹層次，維持 `Pattanakarn` 座標字型與動態小數位自適應。
- GPS 霓虹補強：半尺寸座標改為核心外描 + 雙層 glow，避免字體核心覆蓋光暈；同步提升標題/座標 glow 色階可見度。
- GPS 座標霓虹改為「Home Timer 同款 direct-TFT 演算法」：改在 `ui->update()` 後直繪，多層 glow 與層級色帶邏輯完整沿用 Home。
- 右下地圖新增迷紅光暈：保留藍色地球本體與藍色內暈，外層加入紅色 halo（藍地球 + 紅外暈）。
- 非 TFT fallback 新增黑白近似霓虹：關鍵文字改為雙層白色描邊 + 核心字，並在版面足夠時加入右下角地圖外圈描邊。

## 2026-03-06
- 新增電池過放保護：偵測到電池電壓連續低於 `3.5V` 時，自動進入電池保護並深睡眠。
- 新增 USB 例外：若目前為 USB 供電，則不觸發過放保護。
- FastSetup 新增「`節點設定 -> 電源管理 -> 過放保護`」開關，支援現場直接切換並保留設定。
- 修正 `CannedMessage` 送出後誤跳 `Recent Send`：送出/ACK 清場與逾時退場改為固定回 `Home`。
- 修正 `FOCUS_PRESERVE` 在 module frame 移除後的索引映射偏移，避免焦點落到 `Recent Send`。
- 新增 `CannedMessage` 返回路徑診斷 log：`captureReturnTarget` / `restoreReturnTarget`。

## 2026-03-05
- 修正 Home Timer 每秒整頁重刷：改為時鐘差分更新，降低閃爍。
- 修正返回 Home 殘影與只剩 Timer 問題：加強進出 Home 清區與首次完整重繪。
- 修正 `CannedMessage` 的 `Cancel` 誤跳 `Recent Send`：預設改回 Home，並加入 `restoreReturnTarget default -> home` log。
- 修正 `CannedMessage` 旋鈕誤退出：旋轉改為優先做訊息選單導航，不再誤觸 `Cancel/Back` 導致回 Home。

## 2026-03-03
- HermesX 主頁改版：Home 改為大時鐘主頁，整合日期 / Role / 橫向電池 / 右下衛星角標，並加入 `Pattanakarn` 數字字型資產。
- TFT Home 時鐘改為 direct-TFT 霓虹渲染：新增 `fillRect565()/drawPixel565()` 直繪 API，改用多層藍光暈 + 青白管壁 + 白色核心，不再只靠單色 buffer 假發光。
- Home 時鐘改為局部差分更新：秒數變化只更新變動字形與光暈層，降低每秒整頁重刷感。
- 新增 `Recent Send` 訊息流程：最近訊息清單與詳頁分離，`MSG` 入口整合到 HermesX 自訂 UI。
- 新訊息提示改為 3 秒彈出：只在「訊息喚醒螢幕」時顯示；短按可直進該則訊息，旋鈕可關閉。
- 修正訊息彈窗時序：`EVENT_RECEIVED_MSG` 先喚醒螢幕導致 popup 不觸發的問題，改為先由 `Screen` 判斷是否 arm popup 再喚醒。
- CannedMessage 與 Recent Send 隔離：Recent list / detail 頁不再被罐頭選單搶事件；取消時優先關閉 CannedMessage，避免誤退回訊息列表。
- 旋鈕輸入補 fallback：若未明確設定 rotary 的 `cw/ccw/press` 事件，預設回退為 `UP/DOWN/SELECT`，減少自訂頁面「有 raw log 但 UI 沒反應」情況。

## 2026-02-22
- 建立分支 `HermesX_0.2.9_Civ`。
- Civ 版已排除 EMAC 功能（FastSetup 隱藏 EMAC 設定、停用 EM 緊急 UI 觸發）。
- Lighthouse 模組改為關閉（不建立模組實例）。

## 2026-02-18
- Fast Setup：新增多層快速設定流程，整合 EMAC/UI/節點/GPS/罐頭訊息。
- UI設定：`LED亮度` 改為控制 **WS2812 使用者燈**（不再改 TFT 背光），新增檔位 `關閉 / 低 / 中 / 高 / 最大`。
- 潛行模式：新增一鍵關閉 UI LED(2812)/蜂鳴器，並停用 GPS、BLE、LoRa TX 與 radio 介面。
- TFT：Fast Setup 新增分區彩色（header/選中列/toast），頁層切換才全重繪，旋鈕移動不再每步整頁刷新。
- 體感修正：退出 FAST Setup 改為局部 palette 區塊失效重繪，降低卡頓。
- 版本差異（vs `HXB_0.2.8`）：新增 Fast Setup、WS2812 亮度設定、潛行模式與 TFT 彩色分區/刷新優化；App 回報版本仍為 2.6.11.x。

## 2026-02-10
- EM UI：新增 HermesXEmUi 模組，`@EmergencyActive` 彈出中文緊急選單（⚠️ + 倒數 + 受困/醫療/物資/安全），旋鈕送出後停止蜂鳴器。
- EM UI 右欄：上半倒數、下半狀態（已傳送／傳送成功請原地待命／傳送失敗），NACK 會停留直到下一次送出。
- Lighthouse EMAC：白名單/密碼授權；SAFE 60s 寬限與 60s 週期自動 SOS；Emergency OK ACK 後啟用 EM Tx lock；手機觸發 EMAC 直接啟用鎖。
- EM Tx lock：鎖定時僅允許 emergency port 與手機的 `@EmergencyActive/@ResetLighthouse/@GoToSleep/@HiHermes/@Status`。
- Rotary 待機恢復：light sleep 前後 detach/attach interrupts，並增加 Press 去彈跳與低電位檢查。

## 2026-01-26
- BootHold：gate 提前到 NodeDB 之前，僅在睡眠喚醒時啟動；改為 GPIO 直讀 + baseline 判定，按鍵門檻前不放行主系統，並輸出 gate log。
- BootHold UI：短按喚醒後顯示「. .. ... ....」長按點點動畫，避免與 Resuming 互相覆蓋；等待期內不播放 LED 啟動動畫。
- Prefs 保護：設定讀取失敗時先嘗試還原 `/backups/backup.proto`，成功即不覆寫預設；正常啟動會自動建立/更新備份，避免設定被洗掉。

## 2026-02-04
- 螢幕顯示版號更新為 `HXB_0.2.9`（App 版本仍為 2.6.11）。

## 2026-01-25
- BootHold：短按喚醒後進入 5 秒等待期，期間需長按才放行開機；等待期改為 setup 內阻塞式 gate，未達門檻前不初始化 MeshService/模組；新增長按期間由暗轉亮的進度動畫與「.」循環提示，喚醒時以點點提示覆蓋 Resuming；開機時記錄喚醒原因（timer/ext/reset）。

## 2026-01-09
- 修正開機頁面右上角 Node ID/短名顯示向左偏移，改為逐行右對齊計算後繪製。

## 2025-12-25 (b0.2.8)
- App 回報改為官方格式 2.6.11.x，螢幕顯示固定 `HXB_0.2.8`。
- Lighthouse 模組暫時排除編譯。
- CannedMessage 增加內建 Cancel 項，長按約 1 秒可退出選單，ACK/NACK 不再強制搶焦。

## 2025-12-25
- TAK/TAK_TRACKER 角色靜默 HermesX 介面：關閉 LED 與蜂鳴器，包含啟動/關機動畫與各種提示音/動畫。
- 版號維持 0.2.6 顯示，EMAC/SAFE 行為回復 0.2.6 流程。

## 2025-12-05
- 顯示版號分離：對 App 回報 2.6.11（APP_VERSION/SHORT），螢幕顯示 0.2.6（APP_VERSION_DISPLAY），便於相容與現場辨識。
- TFT 喚醒重繪：ST77xx/ILI9xxx 等面板在 VEXT 斷電後會遺失 GRAM，醒來時強制 `ui->init()` + `forceDisplay(true)`，避免亮背光但黑屏。
- 清理冗長 log：移除 HermesX LED `selectActiveAnimation` 的大量狀態列印。

## 2025-11-30
- 分支 `hermesX_b0.2.6` 紀錄 Heltec Tracker 電源鍵長按喚醒異常：按住反覆重啟、無 ButtonThread 事件與 power-hold 動畫，文件化環境與待查方向。

## 2025-11-29
- Lighthouse EMACT：支援全形＠的 `@EmergencyActive:<pass>`，並在拒絕/通過時寫 log（密碼/白名單狀態）。
- 開機版號：改用 `HXB_<語意版號><git>` 的 APP_VERSION_DISPLAY，從分支名推語意版號，方便現場辨識。

## 2025-10-30
- 恢復 HermesX CN12 混排的 ASCII 分流，英數回到半形寬度，中文仍交由 CN12 繪製。
- CannedMessage 清單與輸入框改用混排渲染，完整保留 UTF-8 中文並原樣傳送。

## 2025-10-27
- 紀錄 HermesX BootHold 行為：若畫面出現 `Resuming...`，表示由 EXT1/RTC 喚醒，仍須持續按住電源鍵超過 `BUTTON_LONGPRESS_MS` 才會開機；提前放開仍會依設計回到深睡眠。
- 目前已知問題：雖可喚醒開機，但約 5 秒後會再度關機，初判為門檻設定異常。

## 2025-10-29
- 恢復 TFT CN12 中文快路徑，改善彩色面板上的字元性能與顏色一致性。
- Removed the custom TFT fast-path, reverting to the stock drawing pipeline; fixes compass overlay corruption.
