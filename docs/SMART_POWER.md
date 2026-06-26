# 智慧功率

智慧功率是 HermesX 在 `TAK` / `TAK Tracker` 模式下自動啟用的 LoRa 發射功率調整功能。它的目標是在維持通訊可靠度的前提下，避免裝置長時間使用不必要的高功率發射。

進入 TAK 類角色後，HermesX 會先以設定的最高功率發送，確保初始通訊穩定；接著會持續觀察實際 mesh 收訊狀態，包含 ACK、RSSI/SNR、逾時，以及 TAK / EMHB 常見的 `WantAck=0` broadcast 封包，依現場訊號強弱保守調整 LoRa `tx_power`。

## 運作方式

智慧功率會在以下角色自動啟用：

- `TAK`
- `TAK Tracker`

進入 TAK 類角色時：

- 暫存原本的 LoRa `tx_power`
- 先套用「智慧功率高」作為初始發射功率
- 開始取樣 remote LoRa RX 訊號
- 依現場訊號逐步調整功率

離開 TAK 類角色時：

- 停止智慧功率策略
- 還原進入 TAK 前的原本 LoRa `tx_power`

## 訊號取樣

智慧功率會觀察 remote LoRa RX 封包的收訊品質，包含：

- ACK 成功或失敗
- ACK timeout
- RSSI / SNR
- remote LoRa RX 強度
- `Portnum=300` / EMHB 類型的 `WantAck=0` broadcast

這代表智慧功率不只依賴 ACK。即使是 TAK 現場常見的 broadcast 流量，只要連續出現穩定強訊號，也能成為降功率依據。

## 降功率

當附近裝置訊號明顯穩定，且 HermesX 連續收到強訊號樣本時，智慧功率會逐步降低 LoRa `tx_power`。

降功率的目的：

- 減少不必要的高功率發射
- 降低耗電
- 降低空中占用
- 減少近距離設備之間過強訊號造成的浪費

降功率不是一次降到底，而是依 cooldown 與連續樣本逐步調整，避免因短暫訊號波動造成不穩定。

## 升功率

如果現場訊號變弱，或發生 ACK 失敗、ACK timeout、長時間沒有有效回饋，智慧功率會逐步提高 LoRa `tx_power`。

升功率的目的：

- 維持通訊可靠度
- 補償距離變遠或遮蔽物造成的訊號衰減
- 在現場環境變差時保留傳輸能力

## 裝置設定

在 `TAKMODE設定` 中可以調整：

- `智慧功率低`
- `智慧功率高`

這兩個值是智慧功率自動調整的最低與最高 dBm 邊界。HermesX 只會在這個範圍內調整 LoRa 發射功率。

預設值：

- `智慧功率低`: `14 dBm`
- `智慧功率高`: `22 dBm`


## 驗證方式

進入 `TAK` / `TAK Tracker` 後，monitor 應該先看到：

```text
[HermesX] SmartPower ON role=7 saved=22 min=14 max=22
```

當 HermesX 在 TAK 類角色下收到 remote `Portnum=300` / EMHB broadcast 時，應該能看到 hermesx 模組開始觀察這類封包：

```text
Module 'hermesx' wantsPacket=1
```

如果現場連續出現強 RX 訊號，並且已經過調整 cooldown，會看到類似：

```text
[HermesX] SmartPower set tx_power=21 reason=strong RX signal
```

這表示智慧功率已經依照 remote RX 強訊號，從原本功率逐步降低。

如果後續訊號仍然穩定，功率會繼續在 `智慧功率低` 與 `智慧功率高` 的範圍內保守調整。

