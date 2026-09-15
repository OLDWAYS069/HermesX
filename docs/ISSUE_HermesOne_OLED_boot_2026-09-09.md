# HermesOne OLED 初始化停住

## 實機回報

USB 診斷最後顯示 `HXDIAG:oled-after-allocate`，未出現
`HXDIAG:oled-after-init-commands`。此標記位於 OLED framebuffer 配置成功後、
`sendInitCommands()` 前；仍需排除標記後的 `Serial.flush()` 阻塞，不能僅憑最後一行判定硬體故障。

## 程式碼追查與待驗證調整

目前 HermesOne variant 使用 `AutoOLEDWire`。其 `connect()` 重新啟動 Wire，
並套用預設 700000 Hz；Adafruit nRF52 `TwoWire::setClock()` 實際選擇 400 kHz。
同一個 Wire 在前面的開機掃描以 100 kHz 運作。
此版本 `TwoWire::endTransmission()` 的硬體事件等待沒有逾時退出，
因此初始化傳輸阻塞是可疑點，但尚未確認根因。

`Screen.cpp` 現在只對 `HERMESX_NRF52840_PROMICRO` 的 AutoOLED 建構子
明確指定 100000 Hz，保留既有探測與初始化命令。
這是待實機驗證的相容性調整，尚不能宣稱已修復開機。

同次 UI 調整將 Canned Message ACK/NACK 圓形結果畫面改用既有吉祥物提示，
文字為「送出成功／送出失敗」，並讓提示期間持續更新動畫。

## 實機驗收

- 診斷版啟動應通過 `oled-after-init-commands`、`screen-setup-end` 並顯示首頁。
- 以真實 ACK/NACK 確認結果文字、吉祥物動畫及提示結束後的畫面。
- 若仍停在原處，下一步需區分 USB flush 與第一筆 I2C 命令，而非直接認定 OLED 損壞。
