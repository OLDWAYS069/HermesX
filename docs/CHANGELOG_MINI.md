## 2026-05-01
- `ONLINE DETAIL` 重新補回 `MSG` 與 `TraceRoute` 操作；`MSG` 可直接對節點開私訊，`TraceRoute` 可直接對 LoRa 節點送出路由追蹤。
- `ONLINE DETAIL` 新增 `Link: LORA / MQTT` 顯示；MQTT 節點的 TraceRoute 會顯示為 `TraceRoute: --`，按下時提示 `LORA ONLY`，避免對 MQTT-only 節點送出不合理的 mesh route 請求。
- 修正 `ONLINE DETAIL` 畫面列數與 input handler 可選列數不一致，游標可能移到空白 action 位的問題。
- `設定 > 裝置管理 > 節點資料庫 > 重設資料庫` 新增 `全部清除`，可保留本機節點並清空其他節點資料。
- 本輪已通過 `platformio run -e heltec-wireless-tracker`，並依 `AI_UPDATE_HANDOFF` 搬移產物到 `~/Desktop/HermesX韌體/HXB0.2.9_20260501_0609.*`。
- `尋人模式` 選單補齊：入口 icon 改為雷達/準星風格，文案調整為 `發送尋人訊號 / 位置訊息`，不再沿用舊的 `尋人模組` 命名。
- 修正 `發送尋人訊號` 確認 popup 的收尾流程：`取消`、手動發送與 3 秒倒數自動發送都會正確回到有效頁面，不再掉入黑屏。
- `發送尋人訊號` 新增前景雷達掃描動畫，並改為持續顯示直到真的收到任一 `POSITION` 回報才切進 `位置訊息`；若 `12s` 內都沒有位置回報，則顯示失敗提示。
- `位置訊息` / Finder 節點列表沿用 `ONLINE` 節點瀏覽架構，可直接查看節點位置與相關資訊，避免另開一套獨立列表流程。

## 2026-04-15
- 新增 `尋人模式` 入口；進入後分為 `發送尋人訊號` 與 `位置訊息` 兩層，不再直接覆蓋原本的 `ONLINE`。
- `發送尋人訊號` 走 `REQUEST: POS <pass>` 輕量位置點名；收到且授權通過的同頻裝置會各自廣播一次自己的位置封包。
- `發送尋人訊號` 確認 popup 改為沿用既有確認框流程，提供 `取消 / 廣播` 選項；`廣播` 需等 `3 秒` 倒數完成後才可選取。
- `位置訊息` 清單顯示最近在線且有有效位置的節點；點入明細後只顯示 `LongName / LastHeard / location / 經 / 緯 / 高度 / 相對位置`。

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
