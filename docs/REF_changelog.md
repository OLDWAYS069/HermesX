# HermesX 變更紀錄 (REF_changelog.md)

## 範圍
- 日期：2025-11-30
- 項目：電源鍵長按喚醒異常紀錄
- 檔案：
  - docs/REF_status.md
  - docs/ISSUE_powerhold_longpress.md
- 說明：
  - 在分支 `hermesX_b0.2.6` 紀錄 Heltec Wireless Tracker (GPIO04) 上長按喚醒失效：按住會反覆重啟、無 ButtonThread 事件與 power-hold 動畫，3 秒喚醒閘門未生效。列出環境、症狀、可能原因與排查方向。
- 測試：
  - 無（純文件更新）。

## 範圍
- 日期：2025-11-29
- 版本：0.2.7
- 項目：EMACT 授權強化、SAFE 長按、版號顯示、Lighthouse 改為隨身模式
- 檔案：
  - bin/readprops.py
  - bin/platformio-custom.py
  - src/graphics/Screen.cpp
  - src/modules/HermesEmergencyState.*
  - src/modules/LighthouseModule.cpp
  - src/modules/LighthouseModule.h
  - src/mesh/MeshService.cpp
  - src/modules/CannedMessageModule.cpp
  - src/modules/Modules.cpp
  - src/modules/HermesXInterfaceModule.cpp
  - docs/REF_3021.md
  - docs/Lighthouse_portable.md
- 說明：
  - Lighthouse 加入 passphrase/白名單授權日誌，支援全形＠前綴；EM 封鎖旗標持久化；SAFE 必須收到 ACK 才解除封鎖。
  - APP_VERSION_DISPLAY 以 branch 語意版號＋ git 短碼呈現，開機畫面顯示 `HXB_<tag><sha>`。
  - 收到 @EmergencyActive 後阻擋 TEXT_MESSAGE_APP（外部/本地）；長按觸發 SAFE Emergency 封包，ACK 後解除封鎖並退出 EM。
  - Lighthouse 預設改為隨身模式：開機不再廣播，改在主螢幕顯示狀態；`HERMESX_LH_BROADCAST_ON_BOOT` 控制是否仍廣播（中繼用）。
  - 新增 `docs/REF_3021.md` 描述 302.1 流程與封包；`docs/Lighthouse_portable.md` 紀錄隨身模式調整與舊行為保留方法。
- 測試：
  - 待執行：載入 `/prefs/lighthouse_passphrase.txt`，檢查未授權時的 log；EM 封鎖 TEXT_MESSAGE_APP；長按 SAFE 發送 Emergency 封包後 ACK 解封；隨身模式開機只顯示狀態、不廣播。

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
