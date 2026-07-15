# HXB_C0.3.7_20260715_0142 Release Notes

Release date: 2026-07-15
Branch: `HermesX_C0.3.7`
Target: `heltec-wireless-tracker`
Build: `HXB_C0.3.7_20260715_0142`
Previous release: `HXB_C0.3.7_20260714_1808`

## TraceRoute Persistence

- `綁定節點` 清單現在儲存在裝置的 `/prefs/hermesx_tr_bound_nodes.txt`。
- 進入 TraceRoute 時會載入既有 Node ID，重新開機不再清空綁定。
- 新增綁定或長按解除綁定時會立即同步更新持久化資料。
- 節點暫時離線或不符合目前 ONLINE 時效條件時仍會保留綁定，不會被背景整理自動解除。

## TraceRoute Timeout

- TraceRoute request 等待回應上限由 10 秒調整為 30 秒，降低較慢多跳路由被過早判定失敗的機率。
- 等待逾時顯示 `等待回應逾時`；只有服務、路由器或封包配置等送出前失敗才顯示 `SEND FAIL`。

## Documentation

- 更新 `docs/TraceRoute.md`，補充重新開機與節點離線時的綁定保留規則，以及 30 秒回應 timeout。
- 更新 `docs/online.md`，區分 TraceRoute 等待逾時與送出失敗。
- 更新 `docs/CHANGELOG.md` 與 `docs/CHANGELOG_MINI.md`。

## Artifacts

- OTA: `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260715_0142.bin`
- Factory: `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260715_0142.factory.bin`

## SHA256

```text
6310d9c21c198aedadfc550823d3c276722753fb0168779a0db9d8b506a75ed5  HXB_C0.3.7_20260715_0142.bin
a9a8424a75f2e7cf778c4d64f68cce4a1affb5bdf7c5c8412058400929776937  HXB_C0.3.7_20260715_0142.factory.bin
```

## OTA Command

```bash
curl -# -H 'Expect:' -H 'X-Hermes-Filename: HXB_C0.3.7_20260715_0142.bin' -T /Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260715_0142.bin http://192.168.43.21/upload-update-bin
```

## Verification

- `platformio run -e heltec-wireless-tracker` succeeded.
- RAM usage: 34.5%.
- Flash usage: 88.0%.
- Firmware contains `HXB_C0.3.7_20260715_0142`.
- Desktop OTA / Factory artifacts match the build outputs by SHA256.
- `git diff --check` passed before release.
