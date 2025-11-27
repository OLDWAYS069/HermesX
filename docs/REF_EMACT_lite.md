# HermesX EMACT lite：文字訊息路徑與 EM SAFE 長按規格

> EMACT（Emergency Message Access Control Table）針對 EM 模式下的「文字封鎖、SAFE 長按、@EmergencyActive 白名單」行為，聚焦在最小改動路徑，便於快速落地與測試。

## 🎯 目標與範圍
- 防止 EM 模式時的雜訊文字干擾：收到 `@EmergencyActive` 後，封鎖一般文字流量（App/PC/裝置端）。
- 允許 SAFE 長按回報必達：長按專屬流程需走 Emergency 封包而非 TEXT_MESSAGE_APP。
- `@EmergencyActive` 僅接受白名單節點來源；本地裝置不主動發送 `@EmergencyActive`。
- 保持既有 Emergency 功能與 302.1 封包不變；不改 Meshtastic LoRa 協定。

## 🧭 狀態與旗標
- `emergencyActive`：EM 模式是否啟用（由 `@EmergencyActive` 或本地 SOS 觸發）。
- `emergencyAwaitingSafe`：EM 已啟動且尚未收到 SAFE 回報；決定是否封鎖文字並攔截長按。
- `waitingForAck`：傳送 SAFE 後等待 ACK 的一般可靠傳輸旗標。

## 📡 外部 (手機 App / PC) → Radio 路徑
1) 入口：toRadio (藍牙/USB/HTTP/串口) → `PhoneAPI::handleToRadio` (`src/mesh/PhoneAPI.cpp`)。  
2) 轉送：`PhoneAPI::handleToRadioPacket` → `MeshService::handleToRadio(meshtastic_MeshPacket &p)` (`src/mesh/MeshService.cpp`)。  
3) 判斷：`p.decoded.portnum`；文字為 `meshtastic_PortNum_TEXT_MESSAGE_APP`，不經 `CannedMessageModule`。  
4) EMACT 攔截：  
```cpp
if (emergencyAwaitingSafe && p.decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP) {
    // 拒絕或忽略，回送錯誤碼給 PhoneAPI（選用）
    return ERR_EMERGENCY_TEXT_BLOCKED;
}
```
5) 非文字 port（Emergency / 控制）照舊進入 Mesh，確保 SAFE/STATUS/NEED/RESOURCE/SOS/HEARTBEAT 不被阻擋。

## 📲 裝置端 (UI/按鍵) → Mesh 路徑
- 主發送：`CannedMessageModule::sendText(...)` (`src/modules/CannedMessageModule.cpp`) 由罐頭訊息或自由輸入觸發。
- 介面包裝：`HermesXInterfaceModule::sendText/sendCannedMessage` (`src/modules/HermesXInterfaceModule.h`) 仍呼叫 `CannedMessageModule`。
- 接收顯示：`TextMessageModule` 只負責接收 `TEXT_MESSAGE_APP`，不參與發送。
- EMACT 攔截：在 `CannedMessageModule::sendText` 入口檢查 `emergencyAwaitingSafe`。  
  - 若真，拒絕文字送出，回傳錯誤碼供 UI 顯示「EM 模式封鎖一般文字」。  
  - 若為 Emergency 類（SAFE/STATUS/NEED/RESOURCE/SOS），使用 Emergency 專用發送函式，避開文字封鎖。

## 🚨 EM 模式入口與保持
- 被動：收到 `@EmergencyActive` 且來源節點在白名單時，`LighthouseModule` 觸發 `onEmergencyModeChanged(true)`，啟動警示並設 `emergencyAwaitingSafe = true`。非白名單來源的 `@EmergencyActive` 直接忽略/拒絕。
- 本地不主動送出 `@EmergencyActive`，僅被動接受並進入 EM。
- 醒機/重啟：若儲存狀態為 EM_ACTIVE，開機初始化時即重啟警示與封鎖邏輯。

## ✋ SAFE 長按流程（EMACT 專屬）
1) 進入條件：`emergencyActive && emergencyAwaitingSafe`。  
2) 長按 handler：按住 ≥ 3s（可配置）即觸發 SAFE 發送，不播放關機動畫。  
3) 發送：呼叫 Emergency SAFE 專用 API（例如 `EmergencyAdaptiveModule::sendSafe()`），要求 ACK。  
4) ACK：播放綠閃＋成功音，清除 `emergencyAwaitingSafe`，停止警示並恢復一般按鍵行為。  
5) NACK/超時：保持警示與封鎖，可再次長按重試；UI 顯示「SAFE 未送達，請再試/移動位置」。  
6) Retry 策略：可沿用 ReliableRouter retry，或在長按重試時再啟動一次發送即可。

## 🚧 白名單 / 黑名單
- 允許（EMACT 通過）：Emergency 封包（SAFE/STATUS/NEED/RESOURCE/SOS/HEARTBEAT）、控制/配置必需指令、系統健康回報。  
- 封鎖：`PortNum_TEXT_MESSAGE_APP`（外部與裝置端皆同），以及任何走純文字的自由輸入。  
- `@EmergencyActive` 白名單：僅白名單節點觸發 EM；其他來源的 `@EmergencyActive` 忽略。  
- 白名單檔案：`/prefs/lighthouse_whitelist.txt`，一行一個 NodeNum（可用十六進位 0x 前綴），允許 `@EmergencyActive` 的來源。
- SAFE 傳送：不得走 TEXT_MESSAGE_APP，長按時直接用 Emergency 專用封包，避開文字攔截。

## ✅ 實作 Checklist
- [ ] `MeshService::handleToRadio` 加入 EMACT 判斷與回覆碼（可回傳 PhoneAPI error）。  
- [ ] `CannedMessageModule::sendText` 在入口拒絕文字，並暴露錯誤碼給 UI。  
- [ ] 長按 handler 依 `emergencyAwaitingSafe` 優先觸發 SAFE，阻斷關機動畫。  
- [ ] `emergencyAwaitingSafe` 隨 `@EmergencyActive`、本地 SOS、SAFE ACK 正確設/清。  
- [ ] `@EmergencyActive` 僅接受白名單來源；非白名單應忽略並紀錄。  
- [ ] 測試：外部 App 傳文字被拒；長按 SAFE 收到 ACK 後解除封鎖；NACK/超時可重試；非白名單 `@EmergencyActive` 不觸發 EM。
  
