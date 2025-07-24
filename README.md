
# HermesX Firmware

**HermesX** 是基於 [Meshtastic](https://github.com/meshtastic/Meshtastic-device) 開源韌體延伸開發的自訂分支，專為 LoRa 裝置打造具有互動表情顯示、旋鈕操作、訊息回覆與緊急應變的個人戰術通訊系統。

---

##  特色功能

### HermesXInterfaceModule

> 讓裝置具有互動表情與操作界面。

- 搭配 WS2812 RGB LED 指示燈、被動式蜂鳴器
- Rotary Encoder 控制 canned messages 選單
- 按鍵按壓觸發 canned message 發送
- 整合 `sendCannedMessage()`，自訂訊息發送邏輯
- 更酷的操作介面!



### LighthouseModule

> 建立動態自適應的 LoRa 緊急模式。

- 專門設計給中繼節點用
- 在首次開機時，會自動進入省電模式
- 當接收到 "@EmergencyActive" 時喚醒，並開始廣播及轉發
- "@ResetLighthouse" 則回到省電模式

> 專為花東緊急連線計畫開發




### EmergencyAdaptiveModule

> 將在HemresX Alpha版回歸。








---

##  置方式（Build Instructions）

### 1. 安裝 PlatformIO

建議搭配 VSCode 使用  
 [https://platformio.org/install](https://platformio.org/install)

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

## 專案結構

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

## 支援硬體

目前已於以下板子測試：

- Heltec Wireless Tracker S3
- 其他自定板子請根據原腳位自行修改 `GPIO` 設定

---

## 計劃目標

HermesX 將成為一款適用於：

- 民防通訊（Disaster-Ready）
- 搜救定位（Rescue Beacon）
- 情感互動（LoRa Companion）
- 自主緊急應變網路（Emergency LoRa Mesh）

---

## 作者 / Maintainer

> **OLDWAYS069**  
> HermesTrack 計畫發起人  
> GitHub: [github.com/OLDWAYS069](https://github.com/OLDWAYS069)

---

## License

This project is licensed under the MIT License.
