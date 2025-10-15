# HermesX 技術規範 (REF_techspec.md)

## 命名規範
HermesX Agents 指南

核心命名習慣 HermesX 為主要前綴：類別（HermesXInterfaceModule、HermesFace）、工具（HermesXPacketUtils）、記錄（HermesXLog）皆維持此一致性；功能掛鉤使用 setNextSleepPreHookParams、runPreDeepSleepHook 等語意化名稱；宏以大寫 MESHTASTIC_EXCLUDE_HERMESX 控制編譯範圍。
Hermes 介面 Agent (src/modules/HermesXInterfaceModule.*, HermesFace*, TinyScheduler.h) 處理表情動畫、旋鈕交互與電源提示；公共 API 包含 startPowerHoldAnimation/updatePowerHoldAnimation/stopPowerHoldAnimation；資源依 HermesFaceMode enum 命名。
按鍵與輸入 Agent (ButtonThread.*, input/RotaryEncoderInterruptBase.*) 擴充 HermesOneButton 型別別名、HoldAnimationMode 狀態與備援 BUTTON_PIN_ALT 喚醒；事件函式統一 userButtonPressedLongStart/Stop、rotaryStateCW 命名，並透過 Hermes 介面更新動畫。
通信可靠度 Agent (mesh/ReliableRouter.*) 新增 ReliableEventType enum、ReliableEvent 結構與 setNotify/emit 回呼，命名以用途為主（ImplicitAck、GiveUp）；hermesXCallback 事件橋接 ACK/NACK。
模組註冊 Agent (modules/Modules.cpp) 保留 Hermes 模組建立序列，命名遵循 moduleName = new Hermes...；加入 LighthouseModule、MusicModule 等依功能命名的模組。
睡眠控制 Agent (sleep.*, sleep_hooks.*, Power.cpp, platform/esp32/main-esp32.cpp) 調整喚醒路徑，公開變數 g_ext1WakeMask/g_ext1WakeMode；函式命名偏描述動作 (setNextSleepPreHookParams, consumeSleepPreHookParams)；BUTTON_PIN_ALT 對應邏輯獨立判斷。
UI 與資源 Agent (graphics/Screen.cpp, graphics/img/icon.xbm, modules/CannedMessageModule.*) 將 Hermes 面板、圖示與提示文字統一以品牌命名 (HermesX_DrawFace、HermesFaceMode::Sending)；傳訊改走 RX_SRC_USER；臨時訊息以 temporaryMessage 命名。
外部通知 Agent (modules/ExternalNotificationModule.cpp) 保留 hermesXCallback 呼叫點與 setExternalState 命名，與 Hermes UI/LED 同步。
設定與總覽 (platformio.ini, .vscode/settings.json, README.md) 以環境或品牌為命名主軸 (default_envs = heltec-wireless-tracker、README 標題 HermesX Firmware)；新增旗標以 BUTTON_PIN_ALT、LIGHTHOUSE_DEBUG 命名突顯用途。
後續若要延伸，維持以上命名模式（前綴、語意化函式與枚舉）即可保持 HermesX 分支的一致性。

## 硬體腳位
- GPIO5 → 單鍵輸入（長按/雙擊/三擊）
- GPIO17 → 蜂鳴器
- GPIO6 → NeoPixel LED (8 顆)
- BUTTON_PIN_ALT 定義：可用於 EXT1 喚醒，須獨立邏輯判斷

## 模組邊界
- EmergencyAdaptiveModule
  - 關鍵詞觸發 @EmergencyActive
  - 僅來自授權節點active訊息可被觸發
  - 設定檔：`emergency_config.proto`
  - ReliableRouter
  - 禁止修改內部 ACK/NACK 邏輯
  - 新行為透過 callback (`hermesXCallback`)
- ExternalNotificationModule
  - 狀態需與 HermesX LED 同步

## UI / LED 規範
- Idle：橘色燈條，上有一個亮點左右來回橫移
- SEND/RECV/ACK/NACK → 動畫期間抑制 Idle
- 動畫結束必須做 300ms 淡出
