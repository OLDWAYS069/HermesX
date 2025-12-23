# HermesX 電源鍵長按喚醒異常紀錄

-## 環境
- 硬體：Heltec Wireless Tracker (ESP32-S3)，主鍵 GPIO04、預設 active-low/pullup。
- 分支/韌體：`hermesX_b0.2.6`，版本字串 `HXB_0.2.64158fb2`，最近 commit `4158fb2 add password for lighthouse module, but not test yet`。
- 相關功能：HermesXInterfaceModule (LED/蜂鳴器)、ButtonThread 喚醒閘門（3 秒）、powerFSM。

## 問題現象
- 關機或 Resuming 畫面時長按主鍵，設備持續重啟，直到鬆手才停止。
- 按住期間未看到 power-hold/開機動畫，也未放行 3 秒喚醒閘門。
- Log 僅有系統啟動與初始化訊息，完全沒有 ButtonThread “press/long press” 或 `startPowerHoldAnimation()` 的輸出。

## 可能原因（待驗證）
- 按鍵未被判定為按下：`userButton.debouncedValue()` 可能始終為 false（腳位/active-low 設定、接線或硬體差異）。
- powerFSM 狀態：達 3 秒門檻時可能不在 `stateDARK`，導致 `EVENT_PRESS` 未放行。
- HermesX 初始化時序：`HermesXInterfaceModule::instance` 尚未就緒或被編譯排除（`MESHTASTIC_EXCLUDE_HERMESX` / `HERMESX_GUARD_POWER_ANIMATIONS`）。
- BootHold/EXT1 喚醒流程干擾長按判定。

## 目前修改（概述）
- 長按 3 秒才放行開機，並在按住期間呼叫 `startPowerHoldAnimation(PowerOn, 3000ms)` 進度動畫；喚醒長按不再觸發關機。
- 只有在 powerFSM `stateDARK` 時才觸發 `EVENT_PRESS`，避免已開機狀態被長按關機。

## 待處理 / 建議排查
1. 在 `processWakeHoldGate()` 加 log（anyPressed/pressedMs/active/triggered/animStarted）確認按鍵狀態有無偵測到。
2. 確認 `HermesXInterfaceModule::instance` 建立時機，必要時延後喚醒閘門判斷或加重試。
3. 檢查按鍵腳位/電氣設定是否與板件一致，必要時調整 activeLow 或換腳。
4. 確認 `powerFSM.getState()` 在 Resuming/關機時是否為 `stateDARK`；若不是，需調整放行條件或喚醒流程。
