# HermesX 變更紀錄 (REF_changelog.md)

## 版本比較：0.2.8 vs 0.2.7
- 版本同步：APP_VERSION/SHORT/螢幕顯示皆改為 0.2.8（0.2.7 使用 branch 語意版＋ git 碼顯示）。
- Lighthouse：0.2.8 預設停用（編譯排除 `MESHTASTIC_EXCLUDE_LIGHTHOUSE=1`），0.2.7 仍啟用且隨身模式廣播。
- 罐頭訊息 UX：自動插入 `Cancel` 選項並支援長按約 1 秒退出；ACK/NACK 不再強制搶焦，避免跳回 ACK 臉。0.2.7 需選訊息或被 ACK 彈窗打斷，退出不便。
- ACK/NACK 聲光：僅在 `waitingForAck` 且 request_id/封包 id 符合最後送出的訊息時觸發，避免設定/系統封包誤播（0.2.7 對其他可靠封包較敏感）。
- 其他：集中式 LED 管理、TAK 靜默、EM/SAFE 回復 0.2.6 的行為延續自 0.2.7，未再變更。

## 範圍
- 日期：2025-12-25
- 版本：0.2.8（App/螢幕同步 0.2.8）
- 項目：版號對齊分支、暫停 Lighthouse、罐頭訊息可取消與長按退出、ACK/NACK 不搶焦
- 檔案：
  - version.properties
  - bin/platformio-custom.py
  - platformio.ini
  - src/modules/CannedMessageModule.cpp / .h
  - src/ButtonThread.cpp
  - src/modules/HermesXInterfaceModule.cpp
- 說明：
  - 將 APP_VERSION/SHORT/螢幕顯示調整為 0.2.8；build/顯示一致。
  - 編譯時排除 Lighthouse 模組（`MESHTASTIC_EXCLUDE_LIGHTHOUSE=1`）。
  - 罐頭訊息自動插入 `Cancel` 選項，選取即退出；在選單內按住約 1 秒也可退出；收到 ACK/NAK 不再強制切回該頁，避免打斷。
  - HermesX ACK/NAK 聲光僅在等待自己的封包時觸發，避免設定/系統封包誤觸。
- 測試：未執行（需實機驗證罐頭退出與版本顯示）。

## 範圍
- 日期：2025-12-25
- 版本：App 回報 2.6.11 / 螢幕顯示 0.2.6
- 項目：TAK 角色靜默 HermesX 介面、維持 0.2.6 顯示/EMAC 流程
- 檔案：
  - src/modules/HermesXInterfaceModule.cpp
  - src/modules/HermesXInterfaceModule.h
  - bin/readprops.py
  - bin/platformio-custom.py
  - src/graphics/Screen.cpp
  - src/configuration.h
  - src/modules/AdminModule.cpp
- 說明：
  - 新增 TAK/TAK_TRACKER 角色檢查，介面全域靜默：LED 與蜂鳴器動畫、啟動/關機效果與提示音全部關閉。
  - 版號顯示維持 0.2.6，並將 EMAC/SAFE 行為回復 0.2.6 流程；版本回報仍為 2.6.11。
  - AdminModule LoRa case 收斂作用域避免跳標編譯錯誤。
- 測試：
  - `pio run -e heltec-wireless-tracker`

## 範圍
- 日期：2025-12-05
- 版本：App 回報 2.6.11 / 螢幕顯示 0.2.6
- 項目：版號顯示分離、TFT 喚醒重繪、清理冗長 LED log
- 檔案：
  - bin/platformio-custom.py
  - src/configuration.h
  - src/graphics/Screen.cpp
  - src/graphics/niche/InkHUD/Applets/System/Logo/LogoApplet.cpp
  - src/modules/HermesXInterfaceModule.cpp
  - docs/CHANGELOG_MINI.md
- 說明：
  - 透過 `APP_VERSION_DISPLAY` 將螢幕顯示定為 0.2.6，同時維持對 App 的 2.6.11 回報。
  - ST77xx/ILI9xxx 等 TFT 在 VEXT 斷電後醒來強制 `ui->init()`＋`forceDisplay(true)`，避免亮背光但黑屏。
  - 移除 HermesX LED `selectActiveAnimation` 的冗長狀態列印。
- 測試：
  - 手動驗證螢幕顯示版號、App 連線版本識別；實機檢查 TFT 喚醒是否正常重繪。

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
  - Lighthouse 加入 passphrase/白名單授權日誌並支援全形＠前綴；EM 封鎖旗標持久化；SAFE 必須收到 ACK 才解除封鎖。
  - APP_VERSION_DISPLAY 以 branch 語意版號＋ git 短碼呈現，開機畫面顯示 `HXB_<tag><sha>`。
  - 收到 @EmergencyActive 後阻擋 TEXT_MESSAGE_APP（外部/本地）；長按觸發 SAFE Emergency 封包，ACK 後解除封鎖並退出 EM。
  - Lighthouse 預設改為隨身模式：開機不再廣播，改在主螢幕顯示狀態；`HERMESX_LH_BROADCAST_ON_BOOT` 控制是否仍廣播（中繼用）。
  - 新增 `docs/REF_3021.md` 描述 302.1 流程與封包；`docs/Lighthouse_portable.md` 紀錄隨身模式調整與舊行為保留方法。
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
