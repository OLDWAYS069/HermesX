# HermesX 302.1.1 Status Matrix

> 此文件用於追蹤 `302.1 Core` 與 `HermesX Extension` 的實作狀態。  
> 狀態分類：
> - `已實作`
> - `部分實作`
> - `待實作`

## 302.1 Core

| 項目 | 規格要求 | 目前狀態 | 實作位置 | 備註 |
|------|----------|----------|----------|------|
| Emergency 專用 port | EM UI / 自動 SOS 均走 `PORTNUM_HERMESX_EMERGENCY = 300` | 已實作 | `src/modules/HermesEmUiModule.cpp`, `src/modules/LighthouseModule.cpp` | 核心封包已走 port 300 |
| EM UI 主回報封包 | `STATUS: TRAPPED / NEED: MEDICAL / NEED: SUPPLIES / STATUS: OK` | 已實作 | `src/modules/HermesEmUiModule.cpp` | ACK 規則已依文件區分 |
| Emergency OK | 非手機來源 `@EmergencyActive` 後回送 `STATUS: OK`，ACK 後啟用鎖 | 已實作 | `src/modules/LighthouseModule.cpp` | 與文件一致 |
| Lighthouse 自動 SOS | 60 秒寬限後送 `SOS`，之後每 60 秒重送 | 已實作 | `src/modules/LighthouseModule.cpp` | SAFE 後停止 |
| TEXT_MESSAGE_APP 僅承載控制指令 | 一般文字不得承載 EM 狀態封包 | 部分實作 | `src/mesh/MeshService.cpp`, `src/modules/LighthouseModule.cpp` | 協定面已收斂為只放行白名單 `@` 指令，仍需實機確認 phone/local 外送全數被擋 |
| EM Tx lock | 啟用後只允許 port 300 與白名單控制指令 | 部分實作 | `src/mesh/MeshService.cpp` | 放行條件已縮緊；待複測一般文字、手機文字與控制指令三條路徑 |

## HermesX Extension

| 項目 | 定位 | 目前狀態 | 實作位置 | 備註 |
|------|------|----------|----------|------|
| EMINFO | EM 狀態同步 | 部分實作 | `src/modules/HermesEmUiModule.cpp` | 已改為 Hermes 私有二進位 payload；仍跑在 mesh transport 上，尚未做到完全隔離 |
| 回報統計 | UI 對主回報封包的本地聚合 | 已實作 | `src/modules/HermesEmUiModule.cpp` | 統計 `TRAPPED / MEDICAL / SUPPLIES / SAFE` |
| 各裝置狀態 | UI 對 EMINFO 的本地呈現 | 部分實作 | `src/modules/HermesEmUiModule.cpp` | 只列出已送過 EMINFO 的節點 |
| EMINFO廣播 開關 | 控制 EMINFO 定期廣播 | 已實作 | `src/modules/HermesEmUiModule.cpp`, `src/modules/HermesEmUiModule.h` | 目前可開關並保存到 prefs |
| EM Heartbeat | EM 模式下短週期在線確認 | 待實作 | 文件草案於 `docs/REF_3021.md` | 尚未落地實作 |

## EMAC 設定

| 項目 | 目標 | 目前狀態 | 預定位置 | 備註 |
|------|------|----------|----------|------|
| `EMINFO廣播` | 可在 EMAC設定 中調整 | 部分實作 | `Screen.cpp`, `Screen.h`, `HermesEmUiModule.cpp` | 邏輯存在，尚未正式整合進 `EMAC設定` UI |
| `EMINFO週期` | 調整狀態同步頻率 | 待實作 | `HermesEmUiModule.cpp`, `Screen.cpp` | 目前沿用 `node_info_broadcast_secs` |
| `Heartbeat週期` | 調整 heartbeat 週期 | 待實作 | 待新增 | 依 Heartbeat 實作而定 |
| `離線判定門檻` | 控制 stale/offline timeout | 待實作 | 待新增 | 依 Heartbeat 實作而定 |
| `是否附帶電量` | 控制 EMINFO/Heartbeat 是否帶 battery | 待實作 | `HermesEmUiModule.cpp`, `Screen.cpp` | 目前固定附帶電量 |

## 目前最關鍵的待收斂項

1. `EM Tx lock` 的實機驗證  
   確保 EM 期間一般文字封包無法再外送。

2. `EMINFO` 的協議定位  
   目前已定義為 `HermesX Extension`，但仍需決定最終的可見性與兼容策略。

3. `EM Heartbeat` 正式落地  
   目前只有文件草案，尚未進入 code。

4. `EMAC設定` UI 整合  
   目前文件已定義參數項，但設定頁尚未完整提供調整入口。
