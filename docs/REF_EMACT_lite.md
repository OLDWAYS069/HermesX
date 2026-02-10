# HermesX EMACT lite（現行實作：EM Tx lock）

> 目標：在 EM 模式下避免一般文字封包佔用網路，保留 Emergency 封包可通行。

## 入口與觸發
- `@EmergencyActive` 通過授權後進入 EM（Lighthouse）。
- **來源為手機（from==0）**：立即啟用 EM Tx lock。
- **來源為節點**：Lighthouse 回送 Emergency OK，routing ACK 收到後啟用 EM Tx lock。

## 封鎖規則（MeshService::sendToMesh）
- **允許**：`PORTNUM_HERMESX_EMERGENCY`（300）。
- **允許（手機文字指令）**：
  - `@EmergencyActive`
  - `@ResetLighthouse`
  - `@GoToSleep`
  - `@HiHermes`
  - `@Status`
- 其他 outbound 封包一律丟棄並記錄 log。

## 解除條件
- `@ResetLighthouse` / `@GoToSleep` / `@HiHermes` 會清除狀態並關閉 EM Tx lock。

## 實作位置
- `src/mesh/MeshService.cpp`：`sendToMesh()` 與 `isEmergencyTxLockAllowedText()`
- `src/modules/LighthouseModule.cpp`：觸發/解除 EM Tx lock
