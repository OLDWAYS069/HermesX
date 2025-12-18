# HermesX 302.1 EM UI v2.1 改版說明書

> 本版本為正式版，並取代舊文件 `HermesX_EM_UI.md`。

## 🎯 改版目的
新版 (v2.1) 以 **Rotary + 按鍵即時互動** 為核心，讓使用者能快速在現場切換訊息並送出符合 **302.1 協議** 的封包。
（完整細節保持原內容）

## 251207EMUI 現況

src/modules/HermesEmergencyUi.{h,cpp} 提供簡單表情動畫狀態機（neutral/send/recv/ack/nack），介面是 setup/onSend/onReceive/onAck/onEmergencyModeChanged/currentFace，但目前沒有被 HermesXInterface 任何地方呼叫，屬於尚未掛上的小工具。
目前實際顯示的 EM UI 是走 HermesXInterfaceModule::showEmergencyBanner()：Lighthouse 進入 EM 時會呼叫，強制疊加紅色 SOS 提示並啟動一次收訊動畫；HermesXInterfaceModule::isEmergencyUiActive() 只會在等待 SAFE 或 banner 還在時回 true。
LED 部分會依 HermesIsEmergencyAwaitingSafe() 把待機/PowerHold 動畫改為 ACK 綠色，讓使用者知道正等待 SAFE（src/modules/HermesXInterfaceModule.cpp around 374, 498, 552）。
EMACT 現況

EM 狀態儲存：src/modules/HermesEmergencyState.* 持久化 gHermesEmergencyAwaitingSafe 到 /prefs/lighthouse_emstate.bin；Lighthouse 啟動時載入並在 EM 開啟時設為 true（src/modules/LighthouseModule.cpp (line 92)），退出時清除（ (line 286)）。
進入 EM：LighthouseModule::handleReceived() 接受 @EmergencyActive，需通過白名單 /prefs/lighthouse_whitelist.txt 或密碼 /prefs/lighthouse_passphrase.txt（全形＠亦可，normalizeAtPrefix），通過後設 emergencyModeActive、調整角色為 Router/禁省電、HermesSetEmergencyAwaitingSafe(true) 並重啟（ (line 466) 之後段）。
封鎖文字：等 SAFE 期間阻擋所有 TEXT_MESSAGE_APP
從手機 → MeshService::handleToRadio() 直接丟棄（src/mesh/MeshService.cpp (line 184)）
本機 canned/free text → CannedMessageModule::sendText() 直接返回（src/modules/CannedMessageModule.cpp (line 541)）
SAFE 長按路徑：電源長按結束時如仍在等待 SAFE，介面模組會用 PORTNUM_HERMESX_EMERGENCY 廣播 SAFE 並要 ACK，記錄 lastSafeRequestId（src/modules/HermesXInterfaceModule.cpp (line 1037)）。
解鎖：收到對應 Routing ACK 後清除等待 SAFE 並呼叫 lighthouseModule->exitEmergencyMode()（ (line 729)）。未收到則維持封鎖/警示狀態。