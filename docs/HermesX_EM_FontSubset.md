# HermesX EM 中文子集字型流程

## 目標
- EM 模式必備詞彙：緊急模式／我受困了／需要醫療／需要物資／我在這（我在這裡）／你好／收到／我平安，與標點：：，。！？（）《》。
- 僅內嵌 1bpp、約 16px 的中文字，容量控制在 40 KB 以下（目前約 3.5 KB）。
- 不使用 SPIFFS/LittleFS，直接編入韌體；顯示流程維持 UTF-8。
 - 用途：HermesX_EM16_ZH 僅供 OLED_ZH 下的 EM UI 最小字集；一般介面仍以 HermesX_CN12 為主。

## 步驟
1. **維護字集清單**：編輯 `tools/fonts/hermesx_em_charset.txt`（UTF-8、無 BOM）。每一行放入句子或標點。
2. **產生字型**：
   ```powershell
   py -3 tools/fonts/generate_hermesx_em_font.py -v
   ```
   - 首次執行會下載 `bdfconv.exe` 與 `unifont.bdf` 至 `tools/fonts/bin/`、`tools/fonts/cache/`。
   - 產出 `src/graphics/fonts/OLEDDisplayFontsZH.{h,cpp}`，並在終端顯示容量與字碼對應。
3. **啟用編譯旗標**：在對應環境設定 `-D OLED_ZH=1`（平台預設放於 `platformio.ini` 註解處）。
4. **重建並驗證**：
   - 重新編譯、燒錄。
   - 進入 EM 介面，確認所有詞彙皆可顯示；若出現缺字，將該字加入字集清單並重跑步驟 2。

## 字碼對應
生成流程會將所有漢字映射到 `0x80–0x9F`，`Screen::customFontTableLookup()` 會在 `OLED_ZH` 下轉換 UTF-8 三位元組字元。

| Codepoint | Glyph | 字型索引 |
|-----------|-------|-----------|
| U+7DCA | 緊 | 0x80 |
| U+6025 | 急 | 0x81 |
| U+6A21 | 模 | 0x82 |
| U+5F0F | 式 | 0x83 |
| U+6211 | 我 | 0x84 |
| U+53D7 | 受 | 0x85 |
| U+56F0 | 困 | 0x86 |
| U+4E86 | 了 | 0x87 |
| U+9700 | 需 | 0x88 |
| U+8981 | 要 | 0x89 |
| U+91AB | 醫 | 0x8A |
| U+7642 | 療 | 0x8B |
| U+7269 | 物 | 0x8C |
| U+8CC7 | 資 | 0x8D |
| U+5728 | 在 | 0x8E |
| U+9019 | 這 | 0x8F |
| U+88E1 | 裡 | 0x90 |
| U+4F60 | 你 | 0x91 |
| U+597D | 好 | 0x92 |
| U+6536 | 收 | 0x93 |
| U+5230 | 到 | 0x94 |
| U+5E73 | 平 | 0x95 |
| U+5B89 | 安 | 0x96 |
| U+FF1A | ： | 0x97 |
| U+FF0C | ， | 0x98 |
| U+3002 | 。 | 0x99 |
| U+FF01 | ！ | 0x9A |
| U+FF1F | ？ | 0x9B |
| U+FF08 | （ | 0x9C |
| U+FF09 | ） | 0x9D |
| U+300A | 《 | 0x9E |
| U+300B | 》 | 0x9F |

## 清理
- 產生器會將原始輸出寫入 `tools/fonts/generated/`，部署前可保留以供追蹤，或依 `.gitignore` 設定忽略。
- 若需重建 bdfconv，刪除 `tools/fonts/bin/` 後再執行步驟 2。
