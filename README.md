
# HermesX Firmware

🎯 **HermesX** 是基於 [Meshtastic](https://github.com/meshtastic/Meshtastic-device) 開源韌體延伸開發的自訂分支，專為 LoRa 裝置打造具有互動表情顯示、旋鈕操作、訊息回覆與緊急應變的個人戰術通訊系統。

---

## ✨ 特色功能

### HermesXInterfaceModule

> 讓裝置像「LoRa 寵物機」一樣具有互動表情與操作界面。

- 支援 ST7789 OLED 表情顯示（`drawFace()` / `updateFace()`）
- 搭配 WS2812 RGB LED 指示燈、被動式蜂鳴器（GPIO 可設定）
- Rotary Encoder 控制 canned messages 選單
- 按鍵按壓觸發 canned message 發送
- 整合 `sendCannedMessage()`，自訂訊息發送邏輯

### EmergencyAdaptiveModule

> 建立動態自適應的 LoRa 緊急模式。

- 監聽網路訊息觸發關鍵字（例如 `@EmergencyActive`）
- 切換 LoRa 頻率、功率、節點優先權（僅特定節點可發訊）
- 減少一般節點位置回報頻率
- 支援「我有困難」快速求救訊號（多次按鍵觸發）

---

## 🔧 建置方式（Build Instructions）

### 1. 安裝 PlatformIO

建議搭配 VSCode 使用  
🔗 [https://platformio.org/install](https://platformio.org/install)

### 2. Clone 此專案

```bash
git clone https://github.com/OLDWAYS069/HermesX.git
cd HermesX
```

### 3. 開啟 VSCode 並選擇 `master` 分支

（非預設的 `main`）

### 4. 編譯

```bash
platformio run -e heltec-wireless-tracker-s3
```

---

## 🗂 專案結構

```plaintext
HermesX/
├── platformio.ini
├── src/
│   ├── modules/
│   │   ├── HermesXInterfaceModule.cpp/h
│   │   ├── EmergencyAdaptiveModule.cpp/h
│   ├── HermesXPacketUtils.h
├── protos/
│   └── custom_protos/
│       └── emergency.proto
```

---

## 📌 支援硬體

目前已於以下板子測試：

- Heltec Wireless Tracker S3
- HTIT LoRa Tracker V1 (ESP32-S2)
- 其他自定板子請根據原腳位自行修改 `GPIO` 設定

---

## 🧠 計劃目標

HermesX 將成為一款適用於：

- 民防通訊（Disaster-Ready）
- 搜救定位（Rescue Beacon）
- 情感互動（LoRa Companion）
- 自主緊急應變網路（Emergency LoRa Mesh）

---

## 👤 作者 / Maintainer

> **OLDWAYS069**  
> 台灣 Meshtastic 開發者｜HermesTrack 計畫發起人  
> GitHub: [github.com/OLDWAYS069](https://github.com/OLDWAYS069)

---

## 📜 License

This project is licensed under the MIT License.
