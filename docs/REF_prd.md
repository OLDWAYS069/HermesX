# HermesX 需求規格 (REF_prd.md)

## 核心驗收條件
1. **Emergency 302.1協議**
   - 支援 SOS/SAFE/NEED/RESOURCE/STATUS/HEARTBEAT 六類型
   - 觸發 `@EmergencyActive` 僅接受白名單節點
2. **輸入控制**
   - GPIO5 長按 → 開關機
   - GPIO5 三擊 → 觸發 SOS
   - GPIO5 雙擊 → 取消 SOS
3. **LED/聲音回饋**
   - Idle：橘色呼吸
   - SEND → 光點由右向左   螢幕顯示表情 
    {">_>", kColorWhite, 160},
    {">.>", kColorWhite, 160},
    {">o>", kColorWhite, 160},
    {">.>", kColorWhite, 160},
   
   - RECEIVE → 光點由左向右 螢幕顯示表情  
    {"<_<", kColorWhite, 160},
    {"<.<", kColorWhite, 160},
    {"<o<", kColorWhite, 160},
    {"<.<", kColorWhite, 160},
   
   - ACK → 綠色閃光
    {"^_^", kColorGreen, 220},
    {"^o^", kColorGreen, 220},
   
   - NACK → 紅色閃光
    {">_<", kColorRed, 260},
    {"x_x", kColorRed, 260},
   
   - SOS → 特殊閃爍（抑制 Idle）螢幕顯示表情
   - 所有動畫結束後需淡出並恢復 Idle

4. **睡眠控制**
   - 進入深睡前播放關機動畫＋音效
   - 長按時應有動態進度條逐步關閉led燈
   - 最終 LED 全熄
   - 進入深睡時應透過gpio 4(將在未來提供)來長按喚醒
5. **可靠傳輸**
   - 支援 ACK/NACK 機制
   - Retry 直到放棄 → 事件 callback
   - sos訊息將無限制retry直到ack

6. **EM模式下的CANNEDMESSAGE**
   -我受困了

   -需要醫療

   -需要物資
   
   -我在這裡

7.**進入EM**
-三擊ROTARY SWITCH
-切換整體UI，LED也會更新



## 不可違反的限制
- 不得修改 Meshtastic 核心 LoRa 協定
- Emergency 配置獨立存於 `emergency_config.proto`
- `BUTTON_PIN_ALT` 僅能擴充，不可覆蓋 `BUTTON_PIN`
-非必要時務必不要更動除要求之程式碼

