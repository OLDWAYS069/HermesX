# HermesX 302.1 EM UI v2.1（現行實作）

> 本文件為現行版本行為；取代舊文件 `HermesX_EM_UI.md`。

## 版面
- **標頭列**：⚠️ +「人員尋回模式啟動」。
- **左側清單**：`受困 / 醫療 / 物資 / 安全`（反白高亮選取）。
- **右側欄**：
  - 上半：倒數（00–99）。
  - 下半：狀態字樣（固定位置）。
- **底部**：banner 文字（預設「請在60秒內回復」）。

## 狀態文字規則
- 尚未送出：`等待中`
- 送出後（未出現 ACK/NACK）：`已傳送`
- ACK（2 秒內）：`傳送成功\n請原地待命`
- NACK：`傳送失敗`（固定顯示直到下一次送出）

## 輸入與互動
- 旋鈕 CW/CCW 切換選項。
- 必須先旋轉導覽（5 秒內 armed），Press 才會送出。
- 送出冷卻 1.5 秒。
- Cancel/Back：退出 EM UI。
- Rotary 三擊：本地觸發 EM UI，並廣播 `@EmergencyActive`。

## 送出封包
使用 `PORTNUM_HERMESX_EMERGENCY`（300）Broadcast：
- 受困 → `STATUS: TRAPPED`（want_ack=true）
- 醫療 → `NEED: MEDICAL`（want_ack=true）
- 物資 → `NEED: SUPPLIES`（want_ack=true）
- 安全 → `STATUS: OK`（want_ack=false，並停止自動 SOS）

## 聲音與提示
- 進入 EM UI：啟動蜂鳴器（siren）。
- **第一次送出即停止蜂鳴器**。
- ACK/NACK 會播放成功/失敗回饋音。

## 實作位置
- `src/modules/HermesEmUiModule.cpp`
