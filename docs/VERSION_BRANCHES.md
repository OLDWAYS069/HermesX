# HermesX 0.3.2 版本分支

本分支由 `HermesX_0.2.9` 原樣升版為 `HermesX_G0.3.2`。除版本標記與本文件外，功能基底維持 0.2.9 內容。

## 目前分支

- 分支：`HermesX_G0.3.2`
- 顯示版號：`HXB_G0.3.2`
- OTA build 版號：`HXB_G0.3.2_YYYYMMDD_HHMM`
- 類型：`GOV`

## GOV / CIV 差異

| 分支 | 類型 | EMAC / EM UI | Lighthouse / GROUP 尋人 | build flags |
|------|------|---------------|---------------------------|-------------|
| `HermesX_G0.3.2` | `GOV` | 啟用 | 啟用 | `MESHTASTIC_EXCLUDE_LIGHTHOUSE=0`, `HERMESX_CIV_DISABLE_EMAC=0` |
| `HermesX_C0.3.2` | `CIV` | 關閉 | 編譯排除 | `MESHTASTIC_EXCLUDE_LIGHTHOUSE=1`, `HERMESX_CIV_DISABLE_EMAC=1` |

## 切換版本號時要同步的位置

1. `bin/platformio-custom.py` 的 `display_short`
2. `src/graphics/Screen.cpp` 的 `kHermesXBuildTag`
3. `README.md` 與 `docs/README.md` 的分支說明
4. 本檔的目前分支、顯示版號與 OTA build 版號

`APP_VERSION` 仍保留 Meshtastic 相容版本；HermesX 的更新比對與檔名使用 `APP_HERMES_VERSION`。
