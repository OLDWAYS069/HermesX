# HermesX Firmware

HermesX 是一款建立在 Meshtastic 協作網路上的定製韌體，專注於讓離線通訊更直覺。這一代的核心任務是強化人機介面：即使使用者不拿出手機，也能透過裝置本體的旋鈕、LED 與音效快速掌握狀態並完成訊息傳遞。

## 功能亮點
- **專注的操作體驗**：強調「抬手即用」的交互，不需手機即可瀏覽罐頭訊息並完成發送。
- **視覺 + 聽覺雙通知**：LED 狀態條與對應音效共同回饋，讓訊息狀態一目了然、耳聞即知。
- **HermesX 品牌化 UI**：面板表情、動畫與命名全面統一，打造一致的介面識別。
- **安全喚醒流程**：短按喚醒後進入 5 秒長按等待期，進度條由暗轉亮並搭配點點提示，避免誤觸開機。

## 外觀設計
外殼預留勾槽，可搭配 D 扣或掛繩將 HermesX 固定於背包、胸掛、皮帶或褲子，真正做到隨身攜帶、隨時使用。

## LED 狀態條行為
| 狀態 | 顏色與動畫 | 說明 |
| --- | --- | --- |
| 待機 | 橘色燈條、有一顆亮點來回移動 | 裝置處於待命但可立即操作。 |
| 發送訊息 | 白色亮點自下而上流動 | 目前正在發送使用者選定的訊息。 |
| 接收訊息 | 白色亮點自上而下流動 | 收到其他節點的訊息。 |
| 收到節點資訊 | 綠色亮點自上而下流動 | 發現或更新網路節點資訊。 |
| 傳訊成功 | 綠燈閃爍三次 | 訊息已獲確認。 |
| 傳訊失敗 | 紅燈閃爍三次 | 訊息未成功送達，請重試。 |

同時搭配對應音效通知，使用者無需盯著燈條也能即時掌握狀態。

## 旋鈕操作
- **旋轉**：瀏覽並選擇欲發送的罐頭訊息。
- **按下**：立即發送目前選定的訊息。
- **長按**：控制開機與關機。

## 其他特點
- 支援 18650 電池快速更換，延長外勤續航。
- 防潑水設計（請勿浸泡；若不慎泡水導致損壞，可寄回更換電路板 ??）。

## 售價
先行者套件價格為 3000 元 / 台，含完整保固服務。

這是 HermesX 的第一步，我們期待把它帶到真實場域與每一個日常場景。

## HermesX Agents 指南
- **核心命名習慣**：以 HermesX 為主要前綴，涵蓋類別（如 `HermesXInterfaceModule`、`HermesFace`）、工具（`HermesXPacketUtils`）與記錄（`HermesXLog`）；功能掛鉤採語意化命名（`setNextSleepPreHookParams`、`runPreDeepSleepHook`）；以大寫宏 `MESHTASTIC_EXCLUDE_HERMESX` 控制編譯範圍。
- **Hermes 介面 Agent** (`src/modules/HermesXInterfaceModule.*`、`HermesFace*`、`TinyScheduler.h`)：處理表情動畫、旋鈕交互與電源提示，公共 API 包含 `startPowerHoldAnimation`/`updatePowerHoldAnimation`/`stopPowerHoldAnimation`，資源依 `HermesFaceMode` enum 命名。
- **按鍵與輸入 Agent** (`ButtonThread.*`、`input/RotaryEncoderInterruptBase.*`)：擴充 `HermesOneButton` 型別別名、`HoldAnimationMode` 狀態與備援 `BUTTON_PIN_ALT` 喚醒；事件函式統一為 `userButtonPressedLongStart/Stop`、`rotaryStateCW` 命名，並透過 Hermes 介面更新動畫。
- **通信可靠度 Agent** (`mesh/ReliableRouter.*`)：新增 `ReliableEventType` enum、`ReliableEvent` 結構與 `setNotify`/`emit` 回呼，命名以用途為主（`ImplicitAck`、`GiveUp`），`hermesXCallback` 事件橋接 ACK/NACK。
- **模組註冊 Agent** (`modules/Modules.cpp`)：維持 Hermes 模組建立序列，命名遵循 `moduleName = new Hermes...`，加入 `LighthouseModule`、`MusicModule` 等依功能命名的模組。
- **睡眠控制 Agent** (`sleep.*`、`sleep_hooks.*`、`Power.cpp`、`platform/esp32/main-esp32.cpp`)：調整喚醒路徑，公開變數 `g_ext1WakeMask`/`g_ext1WakeMode`，函式以動作描述命名（`setNextSleepPreHookParams`、`consumeSleepPreHookParams`），`BUTTON_PIN_ALT` 判斷邏輯獨立。
- **UI 與資源 Agent** (`graphics/Screen.cpp`、`graphics/img/icon.xbm`、`modules/CannedMessageModule.*`)：統一 Hermes 面板、圖示與提示文字命名（`HermesX_DrawFace`、`HermesFaceMode::Sending`），訊息發送改走 `RX_SRC_USER`，臨時訊息以 `temporaryMessage` 命名。
- **外部通知 Agent** (`modules/ExternalNotificationModule.cpp`)：保留 `hermesXCallback` 呼叫點與 `setExternalState` 命名，確保與 Hermes UI/LED 同步。
- **設定與總覽** (`platformio.ini`、`.vscode/settings.json`、`README.md`)：命名以環境或品牌為核心（`default_envs = heltec-wireless-tracker`、README 標題 `HermesX Firmware`），新增旗標時使用 `BUTTON_PIN_ALT`、`LIGHTHOUSE_DEBUG` 等突顯用途的名稱。
- 後續延伸時，保持上述前綴、語意化函式與枚舉的命名模式，即可維持 HermesX 分支的一致性。

