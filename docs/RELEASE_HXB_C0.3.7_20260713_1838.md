# HXB_C0.3.7_20260713_1838 Release Notes

Release date: 2026-07-14
Branch: `HermesX_C0.3.7`
Target: `heltec-wireless-tracker`
Build: `HXB_C0.3.7_20260713_1838`
Previous release: `HXB_C0.3.7_20260711_0239`

## Highlights

- TraceRoute `綁定節點` 清單最上方新增 `搜尋裝置`，`返回` 移到其下方。
- 搜尋裝置沿用 GROUP PIN 鍵盤配置，並新增獨立 `EXIT` 鍵。
- 輸入 ShortName 後按 `OK` 會以不分大小寫的完整比對搜尋目前在線節點。
- 搜尋結果改用置中小視窗顯示，不再把提示疊在鍵盤右下角。
- 找到節點時顯示 ShortName / LongName；找不到時顯示查詢 ShortName，兩種結果都有 `返回` 按鈕。
- 關閉成功結果視窗後，游標會停在找到的節點，保留短按查看詳情與長按綁定的既有操作。

## Changes From Previous Release

相較 `HXB_C0.3.7_20260711_0239`：

- 新增 TraceRoute 綁定清單的 ShortName 搜尋流程。
- 新增搜尋鍵盤 `EXIT` 與獨立搜尋結果視窗。
- 調整 TR 綁定清單排序為 `搜尋裝置`、`返回`、在線節點。
- 補齊搜尋結果 modal 的輸入攔截，避免結果顯示期間誤觸節點詳情或長按綁定。
- 更新 `docs/TraceRoute.md`，記錄搜尋、結果視窗與返回後操作。
- 上一版的 Direct Home 隨機文案、URL timeout 與 TraceRoute 長按修正均完整保留。

## Artifacts

- OTA: `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260713_1838.bin`
- Factory: `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260713_1838.factory.bin`

## SHA256

```text
6a3f9a25742804a6a34e0a9c849a9627b1ac92f3aaa36c770f02cbfd1a8369ac  HXB_C0.3.7_20260713_1838.bin
6ee31f7620ab61f3b9c658ae933b3c232eb6c4012b147d395d6e1bb337ee0a5d  HXB_C0.3.7_20260713_1838.factory.bin
```

## OTA Command

```bash
curl -# -H 'Expect:' -H 'X-Hermes-Filename: HXB_C0.3.7_20260713_1838.bin' -T /Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260713_1838.bin http://192.168.43.21/upload-update-bin
```

## Verification

- `platformio run -e heltec-wireless-tracker -j 4` succeeded.
- Firmware string contains `HXB_C0.3.7_20260713_1838`.
- Desktop OTA / Factory artifacts match the build outputs by SHA256.
- `git diff --check` passed before release.
