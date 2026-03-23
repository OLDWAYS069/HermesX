# HermesX Auto Flasher

Windows 用自動刷機工具。

## 目錄說明

- `tool_windows/`
  Windows 執行檔與執行所需 runtime。
- `Target/`
  韌體 `.bin` 放置位置。
- `config.yaml`
  預設刷機與 Meshtastic 設定。
- `CLI.md`
  舊版設定來源，需要時可轉成 `config.yaml`。
- `audio/`
  啟動音樂與提示音。

## 使用方式

1. 確認裝置已用 USB 連接到電腦。
2. 確認要刷寫的韌體已放在 `Target/`。
3. 直接執行 `tool_windows/Meshtastic_Auto_Flash.exe`。
4. 依畫面提示等待刷機、重開機、重新枚舉完成。

## 設定檔

預設會優先讀取 `config.yaml`。

如果要沿用舊格式，可保留 `CLI.md` 供工具轉換使用。

## 成功與失敗提示

- 啟動時會播放背景音樂。
- 等待重新枚舉、需要注意操作時會播放提示音。
- 成功與失敗都會播放不同提示音。

## 失敗排查

如果畫面顯示失敗，請先查看同目錄下的 `flash_and_config.log`。

常見位置：

- `auto_flash_tool/flash_and_config.log`
- 或執行檔旁邊的 `tool_windows/flash_and_config.log`

如果裝置長時間沒有重新出現，請依畫面提示按一下 `RESET`，或重新插拔 USB。

## 發布時至少要保留

- `tool_windows/`
- `Target/`
- `config.yaml`

若需要保留舊版設定相容性與音效，再一併保留：

- `CLI.md`
- `audio/`
