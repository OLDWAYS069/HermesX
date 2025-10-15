# CODEX 工作硬規則

## 命名與結構
- 新增類別/模組一律以 `HermesX` 為前綴。
- 不更動檔名大小寫與路徑（避免 Windows/Unix 造成 delete+add）。
- 模組註冊：新增模組時更新 `HermesX/src/modules/Modules.cpp` 的註冊序列。

## 行為與協議
- 302.1 微協議：型別/SOS/SAFE/NEED/RESOURCE/STATUS/HEARTBEAT 文串構成不得更動。
- 可靠傳輸：沿用 ReliableRouter 機制；若需擴充，走 hook 而非硬改核心。
- UI/LED：HermesXInterfaceModule 的公開 API 不得破壞（詳見 AGENTS.md）。

## 禁區（除非 PR 說明且標註 @owner）
- `src/mesh/`、`src/sleep*`、`meshtastic/` 第三方核心
- `emergency.proto` 與其生成檔
- `docs/REF_*.md`（只能追加，不得移除關鍵章節）

## 產出
- **永遠輸出 unified diff（patch）**，格式與範例見 `CODEX_PATCH_FORMAT.md`。
- 單一 PR/patch 僅解決單一議題；避免 > 200 行大改（可拆 patch）。
