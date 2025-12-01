# HermesX LED 動畫集中管理說明

## 架構概覽
- 管理模組：`src/modules/HermesXInterfaceModule.*`
- 集中列舉：`LEDAnimation`（PowerHoldProgress/Fade/LatchedRed、StartupEffect、ShutdownEffect、AckFlash、NackFlash、SendL2R、ReceiveR2L、InfoR2L、IdleBreath、IdleRunner）
- 狀態容器：`LEDState`，經 `startLEDAnimation()` / `stopLEDAnimation()` / `tickLEDAnimation()` 管理。
- 開關：`useCentralLedManager` 預設為 `true`，可改為 `false` 回退 legacy `animState`。

## 動畫與觸發來源
- PowerHold 進度/收尾/鎖紅：ButtonThread 長按（開/關機）呼叫，長按開始立即啟動進度並累計 elapsed，滿 `BUTTON_LONGPRESS_MS` 觸發 fade → latchedRed。
- StartupEffect：啟動時由 HermesXInterfaceModule 建構流程觸發；集中渲染一次開機動畫後退回 Idle。
- ShutdownEffect：長按關機時啟動，等待動畫時間後才 `power->shutdown()`；關機前會 `forceAllLedsOff()` 清燈。
- AckFlash / NackFlash：訊息 ACK/NACK 回饋或路由結果；集中模式啟動時同步播放成功/失敗音效。
- SendL2R / ReceiveR2L / InfoR2L：文字/罐頭送出、收到訊息、NodeInfo 收到；跑馬點疊在 Idle 呼吸上。
- IdleBreath + IdleRunner：無高優先動畫時的背景；呼吸為底色，runner 依設定開啟。

## 優先順序（高→低）
1. PowerHoldProgress → PowerHoldFade → PowerHoldLatchedRed
2. ShutdownEffect
3. StartupEffect
4. AckFlash / NackFlash
5. SendL2R / ReceiveR2L / InfoR2L
6. IdleBreath（可同時渲染 IdleRunner）

## 關鍵行為
- 長按開/關機：按下即啟動 PowerHold 進度；達門檻後（預設 5s）才算長按事件。關機會先跑 ShutdownEffect，動畫時間到才關機並清燈。
- 呼吸底色：訊息跑馬與 Info 跑馬不會清除呼吸背景，點亮跑馬點後仍保留底色。
- 音效同步：Ack/Nack 在集中模式啟動時即刻播放對應音效，避免聲光錯位。

## Fallback（Legacy）
- 若需回退舊路徑，可將 `useCentralLedManager` 設為 `false`，所有動畫邏輯會回到 legacy `animState` / 既有函式。
