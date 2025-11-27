# HermesX EMACT lite：文字訊息路徑與 EM SAFE 長按概念

## 外部 (手機 App / PC) 傳入
- 入口：toRadio (藍牙/USB/HTTP/串口) → `PhoneAPI::handleToRadio` (`src/mesh/PhoneAPI.cpp`).
- 轉送：`PhoneAPI::handleToRadioPacket` → `MeshService::handleToRadio(meshtastic_MeshPacket &p)` (`src/mesh/MeshService.cpp`).
- 類型判斷：`p.decoded.portnum`；文字為 `meshtastic_PortNum_TEXT_MESSAGE_APP`，直接送入 mesh，不經 CannedMessageModule。
- 封鎖外部文字：在 `MeshService::handleToRadio` 對 `TEXT_MESSAGE_APP` 做條件判斷，保留 Emergency/控制類 port。

## 裝置端發出 (UI/按鍵)
- 主路徑：`CannedMessageModule::sendText(...)` (`src/modules/CannedMessageModule.cpp`)，由罐頭訊息或自由輸入觸發。
- 介面包裝：`HermesXInterfaceModule::sendText/sendCannedMessage` 宣告於 `src/modules/HermesXInterfaceModule.h`，底層仍呼叫 CannedMessageModule。
- 接收顯示：`TextMessageModule` 只接收 `TEXT_MESSAGE_APP` 封包並通知觀察者，不參與發送。
- 封鎖裝置端文字：在 `CannedMessageModule::sendText` 先檢查 EM/SAFE 旗標，拒絕或提示 UI；不影響外部 ToRadio 入口。

## EM 模式入口（沿用現行規劃）
- 被動：收到 `@EmergencyActive` 時切入 EM 並重啟/啟動警示（目前在 `LighthouseModule` 處理，開機後應觸發 `onEmergencyModeChanged(true)`）。

## EM 模式長按「SAFE」回報 (概念)
- 進入 EM_ACTIVE（如收到 `@EmergencyActive`）時：啟動警示閃爍/鳴叫，暫停電源長按動畫，設定 `emergencyAwaitingSafe = true`。
- 長按接管：在長按 handler 中，`emergencyAwaitingSafe == true` 且按住達秒數（例 3s）→ 送 Emergency SAFE/STATUS:OK 封包（要求 ACK），成功才解除警示。
- ACK/NACK 回饋：ACK → 綠閃＋成功音並清除 `emergencyAwaitingSafe`；NACK/超時 → 保持警示，允許再長按重試，並提示「定位/連線不佳，SAFE 未送達」。
- 封鎖其他傳送：`emergencyAwaitingSafe` 為真時，可在 CannedMessageModule/`MeshService::handleToRadio` 拒絕非 Emergency 封包，避免一般文字流量。

## 白名單建議（封鎖一般文字時仍需放行）
- 允許的 port：Emergency 類型（SAFE/STATUS/NEED/RESOURCE/SOS/HEARTBEAT）、控制/配置必需的指令。
- 封鎖的 port：一般文字 `PortNum_TEXT_MESSAGE_APP`（無論 App/PC 或裝置端）。
- SAFE 傳送：使用 Emergency 專用封包流程（不要走 TEXT_MESSAGE_APP），長按時直接呼叫 Emergency 發送函式，避免被文字封鎖規則擋下。
  