# HermesOne 修改與交付格式

- 修改前確認目前分支是 `HermesOne`。
- 保留使用者工作樹中既有修改，不回復無關檔案。
- 程式修改後至少執行 `git diff --check`。
- 韌體修改應使用 `platformio run -e nrf52_promicro_diy_tcxo_hermesx -j 4` 驗證。
- 回覆需提供實際 `APP_HERMES_VERSION`、RAM/Flash、產物路徑與未完成實機項目。
- HermesOne 交付物是 UF2/HEX/ZIP，不是 ESP32 OTA/Factory BIN。
