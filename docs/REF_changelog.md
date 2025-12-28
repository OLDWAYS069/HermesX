# HermesX 變更紀錄 (REF_changelog.md)

## 範圍
- 日期：2025-12-27
- 項目：LoBBS 在 Repeater 角色啟用、啟動/收訊日誌補強
- 檔案：
  - docs/CHANGELOG_MINI.md
  - src/modules/Modules.cpp
  - lobbs/src/LoBBSModule.cpp
- 說明：
  - Repeater 角色現在也會建立 TextMessage/LoBBS/Welcome 模組，保留 DM 與歡迎訊息功能。
  - Modules 啟動時輸出角色與 LoBBS/LoDB/LoFS 是否編入的日誌；LoBBS 啟動與收到 DM 時輸出 Info，便於現場確認模組活著。
- 測試：
  - 待執行：Repeater 角色編譯與開機確認 LoBBS 啟動日誌；DM 送達時確認 LoBBS 收訊日誌與回覆正常。

## 範圍
- 日期：2025-12-26
- 項目：LoBBS 整合 / UTF-8 放寬 / HermesX headless 精簡
- 檔案：
  - .gitmodules
  - platformio.ini
  - docs/CHANGELOG_MINI.md
  - src/configuration.h
  - src/modules/Modules.cpp
  - lobbs/src/*
  - lodb/src/*
  - lofs/src/*
  - src/mesh/generated/meshtastic/lobbs.pb.{c,h}
  - src/graphics/fonts/HermesX_zh/HermesX_CN12*.{cpp,h}
  - src/graphics/fonts/OLEDDisplayFontsZH.{cpp,h}
  - src/modules/HermesXInterfaceModule.cpp
  - src/modules/HermesXPowerGuard.cpp
  - src/sleep_hooks.cpp
- 說明：
  - 新增 LoBBS/LoDB/LoFS 子模組並納入編譯（PlatformIO include/build filter）；LoBBS 於 Modules 註冊，nanopb 產物生成在 `src/mesh/generated/meshtastic/`。
  - LoBBS 中文化與 UTF-8 放寬：帳號/密碼不再限 ASCII，訊息截斷改 UTF-8 安全；登入成功自動推送中文指令速查，所有回覆改為中文。
  - HermesX 介面與中文字型改為條件編譯：無螢幕時自動排除 HermesX/UI/字型，減少 headless 佈建負擔。
  - 新增 WelcomeModule：監聽位置更新，預設以本機座標為圓心 20km 內的新節點在頻道廣播歡迎；可用 `/welcome on|off|radius <公里>` 調整。
- 測試：
  - 待執行：LoBBS 註冊/登入（中英混排帳密）、/mail 與 /news 中英文內容收發；WelcomeModule 觸發（位置距離判定、廣播內容）；headless 組態確認 HermesX/字型排除且可編譯。

## 範圍
- 日期：251129
- 版本：0.2.7
- 項目：EMACT @EmergencyActive 授權、SAFE 長按、版號顯示、Lighthouse 改為隨身模式
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
  - Lighthouse 加入 passphrase/白名單授權日誌並支援全形＠前綴；EM 封鎖旗標持久化；SAFE 必須收到 ACK 才解除封鎖。
  - APP_VERSION_DISPLAY 以 branch 語意版號＋ git 短碼呈現，開機畫面顯示 `HXB_<tag><sha>`。
  - 收到 @EmergencyActive 後阻擋 TEXT_MESSAGE_APP（外部/本地）；長按觸發 SAFE Emergency 封包，ACK 後解除封鎖並退出 EM。
  - Lighthouse 預設改為隨身模式：開機不再廣播，改在主螢幕顯示狀態；以宏 `HERMESX_LH_BROADCAST_ON_BOOT` 控制是否仍廣播（給中繼用）。
  - 新增 `docs/REF_3021.md` 描述 302.1 流程與封包對應；`docs/Lighthouse_portable.md` 紀錄隨身模式調整與舊行為保留方法。
  - 已知問題：開機後狀態 banner（EM/輪詢/行動）仍可能未顯示，雖有 log「show status banner」，需再對齊 HermesX face 刷新管線。
- 測試：
  - 待執行：載入 `/prefs/lighthouse_passphrase.txt`，檢查未授權時的 log；EM 封鎖 TEXT_MESSAGE_APP；長按 SAFE 發送 Emergency 封包，收到 ACK 後解除封鎖並退出 EM；確認隨身模式開機只顯示狀態、不廣播。

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
