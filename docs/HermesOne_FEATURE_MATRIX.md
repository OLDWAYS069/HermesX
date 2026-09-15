# HermesOne 0.1.0 功能矩陣

HermesOne 定位為低耗能、輕功能的 nRF52840 野外節點。輕量化集中在不適用硬體、背景模組與重複程式路徑；使用者指定的本機 UI、Meshtastic 核心能力及聲光互動均保留。

## 保留功能

| 類別 | 保留內容 | 驗證狀態 |
|---|---|---|
| 核心通訊 | SX126x/HT-RA62 LoRa Mesh、Routing、BLE、Admin | 已編譯；LoRa/BLE 長時間實機待驗證 |
| 定位 | GPS UART、Position、GPS 本機頁 | 已編譯；GPS 實機待驗證 |
| 尋人 | Finder/Lighthouse、節點清單、詳細頁、尋人脈衝 | 已編譯；端到端待驗證 |
| TAK | ATAK plugin、Position PLI、TAK 設定頁、智慧功率相關 UI | 已編譯；ATAK 實機待驗證 |
| 訊息 | Text Message、Canned Message、收件／詳細、訊息編輯、中文與注音輸入 | 已編譯；逐頁待驗證 |
| 節點 | NodeDB、NodeInfo、ONLINE 清單／詳細 | 已編譯；逐頁待驗證 |
| 路由 | TraceRoute 原生模組、本機搜尋／結果／彈窗 | 已編譯；逐頁待驗證 |
| 群組 | GROUP 清單、詳細、設定與訊息入口 | 已編譯；逐頁待驗證 |
| 系統 UI | HOME、時鐘、Setup、Fast Setup、更新 UI、SLEEP | 已編譯；逐頁待驗證 |
| 聲音 | 一般啟動音、旋鈕音、收發／成功／失敗等事件音 | 已編譯；蜂鳴器已能發聲 |
| 燈光 | HermesX C0.4.0 的 WS2812B 開機、待機呼吸/runner、收發、ACK/NACK、關機及裝飾回饋 | 已編譯；P0.06 實機待驗證 |
| 電源 | 低功耗 profile、螢幕休眠、深度睡眠 | 已編譯；電流待量測 |
| 遙測 | Meshtastic Device Telemetry | 保留 |

## 明確排除或不適用

| 功能 | 狀態 | 原因 |
|---|---|---|
| MQTT | 編譯排除 | 本硬體沒有 Wi-Fi |
| Wi-Fi / URL OTA / ESP32 Factory BIN | 不適用 | nRF52840 以 UF2/HEX/DFU ZIP 更新；更新 UI 本身保留 |
| Waypoint | 編譯排除 | 0.1.0 非核心背景模組 |
| Store & Forward server | 編譯排除 | 0.1.0 非核心背景模組；一般文字訊息不受影響 |
| Range Test | 編譯排除 | 非產品任務需求 |
| Neighbor Info | 編譯排除 | ONLINE/NodeDB/NodeInfo 仍保留 |
| Serial Module | 編譯排除 | 不等同 USB bootloader 或診斷序列埠 |
| Detection Sensor | 編譯排除 | 沒有對應硬體 |
| Accelerometer / Magnetometer | 編譯排除 | 沒有對應硬體；也不再探測或建立執行緒 |
| Environmental / Power sensor telemetry | 編譯排除 | 沒有外接感測器；Device Telemetry 保留 |
| External Notification module | 編譯排除 | 本機蜂鳴器與 WS2812B 回饋仍保留 |
| Remote Hardware、Power Stress、Dropzone、Generic Thread | 編譯排除 | 非 0.1.0 產品需求 |
| Hermes EM 獨立 UI module | 編譯排除 | Finder/Lighthouse 與 GROUP UI 仍保留；不建立額外 EM UI 執行緒 |
| 外接 CardKB I2C 鍵盤、額外 Up/Down 按鍵 | 編譯排除 | 現行硬體以旋鈕輸入，保留訊息編輯與注音軟體鍵盤 |
| Heltec Tracker 彩色 TFT Direct UI | 不適用 | HermesOne 使用 SSD1309 單色 OLED |

## 聲光相容邊界

- WS2812B DATA 使用 P0.06（板上 `006`），數量維持 8 顆；GPS 模組 TX 改接 P1.06/D10。
- 動畫演算法、時序、顏色、呼吸曲線、runner、逐顆啟動與關機淡出均直接沿用 HermesX C0.4.0。
- 啟動、收發、成功、失敗、Node Info 與關機音序沿用 HermesX；只有底層輸出由 ESP32 LEDC 改成 nRF52840 `tone()/noTone()`。

## 實機狀態

已確認 UF2 bootloader 寫入、韌體開機、SSD1309 顯示、區域設定畫面、旋鈕／按鍵基本操作及無源蜂鳴器發聲。其餘標示「待驗證」的項目不可只憑編譯成功宣稱完成。

編譯基準：`HermesOne_0.1.0_20260905_0650`，Flash 793,692 / 815,104 bytes（97.4%），RAM 43,292 / 248,832 bytes（17.4%）。
