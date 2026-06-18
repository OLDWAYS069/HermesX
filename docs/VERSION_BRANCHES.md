# HermesX 0.3.2 版本分支

本分支由 `HermesX_C0.3.2` 升版為 `HermesX_C0.3.7`。除版本標記、CIV build flags 與本文件外，功能基底維持 C0.3.x CIV 內容；CIV 關閉 EMAC，但保留 GROUP 與尋人模組。

## 目前分支

- 分支：`HermesX_C0.3.7`
- 顯示版號：`HXB_C0.3.7`
- OTA build 版號：`HXB_C0.3.7_YYYYMMDD_HHMM`
- 類型：`CIV`

## GOV / CIV 差異

| 分支 | 類型 | EMAC / EM UI | Lighthouse / GROUP 尋人 | build flags |
|------|------|---------------|---------------------------|-------------|
| `HermesX_G0.3.2` | `GOV` | 啟用 | 啟用 | `MESHTASTIC_EXCLUDE_LIGHTHOUSE=0`, `HERMESX_CIV_DISABLE_EMAC=0` |
| `HermesX_C0.3.7` | `CIV` | 關閉 | 啟用 | `MESHTASTIC_EXCLUDE_LIGHTHOUSE=0`, `HERMESX_CIV_DISABLE_EMAC=1` |

`CIV` 的 `HERMESX_CIV_DISABLE_EMAC=1` 只關閉 EMAC 入口、EM UI 進入、遠端 `ACTIVATE: EMAC` / `@EmergencyActive` / `STATUS: LOST` 與 Lighthouse sleep 控制；`GROUP PIN`、GROUP 節點資料與 `REQUEST: POS` 尋人位置脈衝仍保留。

## 切換版本號時要同步的位置

1. `bin/platformio-custom.py` 的 `display_short`
2. `src/graphics/Screen.cpp` 的 `kHermesXBuildTag`
3. `README.md` 與 `docs/README.md` 的分支說明
4. 本檔的目前分支、顯示版號與 OTA build 版號

`APP_VERSION` 仍保留 Meshtastic 相容版本；HermesX 的更新比對與檔名使用 `APP_HERMES_VERSION`。
