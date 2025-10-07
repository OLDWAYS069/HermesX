# HermesX 變更日誌 (REF_changelog.md)

## 範例格式
- 日期：2025-09-27
- 任務：GPIO5 三擊觸發 SOS
- 檔案：
  - src/modules/HermesXInterfaceModule.cpp
  - src/modules/HermesXInterfaceModule.h
- 重點：
  - 新增 `startSosAnim() / cancelSosAnim()`
  - 修改 updateLED() → 支援 SOS 動畫抑制 Idle
- 測試：
  - 單擊 → 無事
  - 雙擊 → 取消
  - 三擊 → 觸發 SOS 並播放動畫
- 風險：
  - Idle LED 被抑制後可能未恢復
  - SOS 與 ACK/NACK 動畫衝突需再測
