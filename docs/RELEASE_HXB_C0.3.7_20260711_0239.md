# HXB_C0.3.7_20260711_0239 Release Notes

Release date: 2026-07-11
Branch: `HermesX_C0.3.7`
Target: `heltec-wireless-tracker`
Build: `HXB_C0.3.7_20260711_0239`

## Highlights

- Direct Home 小威右側空白區改為隨機 HermesX 文案。
- Home 文案超出右側欄寬時會拆成兩段，每約 2 秒切換一次再循環。
- Direct Home skip-ui 路徑會把文案切段納入 base repaint 判斷，避免右側文字停在舊段落。
- URL 更新檢查與下載 timeout 放寬，降低手機熱點、弱 WiFi 或 HTTPS 來源短暫停頓造成的誤判。
- TraceRoute 1 秒長按綁定 / 解除綁定不再被 3 秒旋鈕鎖定邏輯干擾。
- 旋鈕鎖定長按範圍收斂到固定主頁的 `TAK MODE` / 智慧功率頁。

## Artifacts

- OTA: `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260711_0239.bin`
- Factory: `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260711_0239.factory.bin`

## SHA256

```text
31f356e99c9f771949f65afda6ec500bd454f73f50169e165d6590e6d119d2fe  HXB_C0.3.7_20260711_0239.bin
16f07d51235b5a28100ada288a376d3c9b8c0ba61c985bd4cad0bd5c36c611ae  HXB_C0.3.7_20260711_0239.factory.bin
```

## OTA Command

```bash
curl -# -H 'Expect:' -H 'X-Hermes-Filename: HXB_C0.3.7_20260711_0239.bin' -T /Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260711_0239.bin http://192.168.43.21/upload-update-bin
```

## Verification

- `platformio run -e heltec-wireless-tracker -j 4` succeeded.
- Firmware string contains `HXB_C0.3.7_20260711_0239`.
- `git diff --check` passed.
