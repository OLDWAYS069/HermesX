# Patch 輸出規範

- 使用 unified diff，可被 `git apply` 或 `git am` 直接套用。
- Header 範例：
  ```
  --- a/HermesX/src/modules/HermesXInterfaceModule.cpp
  +++ b/HermesX/src/modules/HermesXInterfaceModule.cpp
  @@ -12,6 +12,10 @@
  ```
- 新增檔案：`--- a/dev/null` → `+++ b/path/to/file`.
- 若無變更：輸出空白（不要任何文字）。

**禁止**
- 不得輸出說明文字、Markdown、或第二個 code block。
- 不得覆蓋整個檔案內容（應呈現差異）。
