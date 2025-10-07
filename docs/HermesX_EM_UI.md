# HermesX EM UI 對照表

## 控制旗標與狀態

- **`emergencyUiActive`**
  - 來源：`applyEmergencyTheme(active)`。
  - 為 `true` 時 `drawFace()` 才會依據 EM overlay 高度呼叫 `drawEmergencyOverlay()`。
  - `onEmergencyModeChanged(true/false)` 會透過 `applyEmergencyTheme()` 開關；一般傳送流程不會變更它。

- **`showEmergencyBanner(on, text, color)`**
  - 常見呼叫點：
    - `playSOSFeedback()` → `showEmergencyBanner(true, "EM SENDING")`
    - `onEmergencyTxResult()` 根據 SOS/SAFE/NEED/RESOURCE/STATUS 成功與否更新文案；SAFE 成功時 `showEmergencyBanner(false)`。
    - `handleAckNotification(false)` 在 NACK 時會呼叫 `onEmergencyTxResult(..., false)` → 顯示失敗橫幅。
  - 若 `emergencyUiActive == false`，即使設定 `showEmergencyBanner(true)`，`drawFace()` 也不會畫 overlay（但文字會保留，重新進 EM mode 時可沿用）。

- **`emergencyMenuStates[]` / `updateEmergencyMenuState()`**
  - SOS：`onTripleClick()` → `Pending`；`onEmergencyTxResult(SOS, ok)` → `Success`/`Failed`。
  - SAFE：`onDoubleClickWithin3s()` → `Pending`；`onEmergencyTxResult(SAFE, ok)` → `Success`/`Failed`。
  - NEED/RESOURCE/STATUS：目前僅 `onEmergencyTxResult()` 會更新為 `Success`/`Failed`（若後續加入 UI 觸發需記得補上 `Pending` 狀態）。
  - `decayEmergencyMenuStates()` 在 `runOnce()` 執行，`Success/Failed` 狀態維持 8 秒 (`kEmergencyMenuStateHoldMs`) 後自動回 `Idle`。

- **`pendingEmergencyType` / `hasPendingEmergencyType`**
  - 設定：`onTripleClick()`、`onDoubleClickWithin3s()`；若 `sendSOS/sendSafe` 失敗會立即標記 `Failed` 並清旗標。
  - 清除：`onEmergencyTxResult()`（比對 type）、`handleAckNotification()`（ACK/NACK 會觸發 `onEmergencyTxResult()`）；`onEmergencyModeChanged(false)` 亦會 `resetEmergencyMenuStates()` 並清旗標。

- **`applyEmergencyTheme(active)` 與 LED 主題**
  - 開啟：複製 `defaultTheme` 並改為紅色呼吸 (`colorIdleBreathBase = 0xF800`)、綠色 Ack (`0x07E0`)、紅色 Failed (`0xF800`)，同時設 `emergencyUiActive = true`。
  - 關閉：還原 `defaultTheme`、設 `emergencyUiActive = false`，並在 `showEmergencyBanner(false)` 時清除所有 EM 橫幅。

## 回歸檢查建議

1. **一般傳送**：檢查 `playSendFeedback()` 不會意外顯示 EM overlay（應只啟動 LED 動畫）。
2. **NACK 流程**：`handleAckNotification(false)` 需透過 `onEmergencyTxResult()` 顯示 `SOS FAILED` / `SAFE FAILED`，同時更新 menu 狀態。
3. **EM Mode 切換**：`onEmergencyModeChanged(true/false)` 應正確切換 LED 主題、清旗標及 overlay，離開 EM 後一般訊息不應殘留 EM UI。
## 顯示整合
- EM 模式啟用時，HermesXInterfaceModule 會宣告需要專用的 UI frame，並在 applyEmergencyTheme(true) 時透過 UIFrameEvent 重新建立畫面，將螢幕切換成專屬的 EM UI。
- 退出 EM 後則釋出該 frame，回復原本的主畫面與 overlay 配置。
## 待辦需求
- EM 模式啟動時，主畫面需進入專屬的 EM 頁面，並內嵌目前規劃好的 Canned Message 選單，取代原本的 Canned Message 主選單版面。
- SEND / ACK / NACK 等臉部動畫觸發期間，暫時隱藏 EM 介面，優先顯示原有動畫；動畫結束後再恢復 EM UI。
