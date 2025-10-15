# HermesX 變更紀錄 (REF_changelog.md)

## 範圍
- 日期：2025-10-15
- 項目：EM 中文最小字型整合
- 檔案：
  - .gitignore
  - platformio.ini
  - docs/HermesX_EM_FontSubset.md
  - docs/REF_changelog.md
  - src/graphics/Screen.h
  - src/graphics/ScreenFonts.h
  - src/graphics/fonts/OLEDDisplayFontsZH.cpp
  - src/graphics/fonts/OLEDDisplayFontsZH.h
  - tools/fonts/generate_hermesx_em_font.py
  - tools/fonts/hermesx_em_charset.txt
- 說明：
  - 新增 HermesX_EM16_ZH 子集字型並在 OLED_ZH 組態下套用。
  - 自訂 UTF-8 轉碼邏輯，將 EM 所需漢字與標點對應至 0x80–0x9F。
  - 提供字集清單與 Python 產生腳本，文件化流程與驗證項目。
- 測試：
  - 未執行；待以 `-D OLED_ZH=1` 編譯並於裝置上確認 EM 介面顯示。

## 範圍
- 日期：2025-10-10
- 項目：EM UI Rotary → 302.1 Direct Sender
- 檔案：
  - src/modules/HermesXInterfaceModule.cpp
  - src/modules/HermesXInterfaceModule.h
- 說明：
  - 新增 `startSosAnim()`／`cancelSosAnim()`，統一 SOS LED 動畫生命週期。
  - `updateLED()` 新增 Idle 檢查，避免 SOS 動畫與一般顯示互相干擾。
- 測試：
  - 未執行；後續需確認三按觸發與 SOS ACK/NACK 動畫整合風險。
