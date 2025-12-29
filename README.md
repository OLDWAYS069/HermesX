## 本分支（HermesBASE_b0.1.0）重點
- Welcome 廣播改為需有 NodeInfo 才觸發，訊息文案更新，走主頻道 0、每節點只歡迎一次。
- Lighthouse 廣播與自我介紹改走主頻道 0，新增 `@戳`、`@HermesBase` 公頻回覆；`@BAT` 仍在 channel 2。
- LoBBS `/welcome` 指令可查詢/開關/調整半徑，配合上述歡迎行為。

## 快速開始
- 環境需求：PlatformIO（VS Code 擴充或 `pio` CLI）、Python 3。
- 預設編譯環境：`platformio.ini` 的 `default_envs = tbeam`。若使用其他板子，改成對應的 env（位於 `arch/*/*.ini` 或 `variants/*/platformio.ini`）。
- 建置：`pio run -e <env>`  
  燒錄：`pio run -t upload -e <env>`
- 序列埠監看：`pio device monitor -e <env>`（預設 115200）。

## 主要特性
- 模組化擴充：Welcome 歡迎訊息、LoBBS 指令、Lighthouse 緊急模式/站台告示等。

## 模組與指令
### WelcomeModule（歡迎訊息）
- 觸發：收到 `POSITION_APP` 且已有該節點的 NodeInfo（`has_user`），距離在設定半徑內（預設 20km，channel 0，hop_limit=3），每個 NodeNum 只歡迎一次。
- 訊息內容：
  ```
  歡迎 <對方暱稱或「新朋友」> 進入台灣妹婿-花蓮分區!

  LoBBS 指令（請私訊我）：
  登入： /hi <帳號> <密碼> 

  公頻指令：
  @BAT： 查看伺服器電量
  ＠戳 ：戳一下我
  ＠HermesBase：有關於HermesBase
  ```
- LoBBS 已登入用戶可用 `/welcome on|off|radius <公里>` 開關或調整半徑。

### Lighthouse
- 廣播管道：狀態/介紹、`@戳` 回覆、`@HermesBase` 回覆走 channel 0；`@BAT` 仍在 channel 2。
- 指令（公頻）：
  - `@Status`：顯示 Lighthouse 狀態（非廣播模式時會在本機顯示）。
  - `@BAT`：回報電池狀態（channel 2）。
  - `@戳`：回覆「討厭><」（channel 0）。
  - `@HermesBase`：回覆「HermesBase是一套可以提供遠端管理、離網布告欄的系統\n更多資訊：連結」（channel 0，連結待補）。
  - `@HiHermes`：廣播自我介紹。
  - `@EmergencyActive:<pass>` 或白名單來源 `@EmergencyActive`：啟動緊急模式；`@GoToSleep` 進入節能輪詢；`@Repeater` 轉固定中繼站。

### LoBBS（帳號登入與訊息）
- 私訊 `/hi <帳號> <密碼>` 登入或註冊；登入後可用 `/welcome ...` 管理歡迎訊息。
- 公頻常用指令：`@BAT`（伺服器電量）、`＠戳`（戳一下）、`＠HermesBase`（系統介紹）。

## 頻道與廣播習慣
- 主頻道（channel 0）：Welcome 廣播、Lighthouse 狀態/介紹與新增的公頻回覆。
- 預設半徑與去重：Welcome 半徑 20km，可調；同一 NodeNum 只歡迎一次，裝置重啟後重計。

## 專案結構速覽
- `src/modules/`：各模組（WelcomeModule、LighthouseModule、LoBBS 等）。
- `graphics/`：面板 UI、動畫與顯示邏輯。
- `arch/`、`variants/`：各板子與組態的 PlatformIO 設定。
- `data/prefs/`：Lighthouse 白名單、passphrase 等預設檔。

## 開發提示
- 編譯旗標集中於 `platformio.ini` 的 `[env]`，Meshtastic/HermesX 的包含/排除宏可由此調整。
- Welcome/Lighthouse 相關常數與訊息可在各自模組檔內修改（`src/modules/WelcomeModule.cpp`、`src/modules/LighthouseModule.cpp`）。
- 建議先以預設 `tbeam` 或目標板子的 env 編譯確認環境 OK，再切換實機上傳。
