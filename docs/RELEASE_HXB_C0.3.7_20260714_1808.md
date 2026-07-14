# HXB_C0.3.7_20260714_1808 Release Notes

Release date: 2026-07-14
Branch: `HermesX_C0.3.7`
Target: `heltec-wireless-tracker`
Build: `HXB_C0.3.7_20260714_1808`
Previous release: `HXB_C0.3.7_20260713_1838`

## Highlights

- `TAK Tracker` 現在完整共用 TAK MODE 的智慧功率主頁與新 UI。
- TAK Tracker 可直接使用相同的 `TAKMODE設定`、`頻道選擇`、`GROUP設定`、尋人入口與 CannedMessage 輸入隔離。
- TAK profile 與 TAK A-E / `自動` 頻道設定在 TAK、TAK Tracker 之間共用。
- 套用共用設定時保留 `TAK_TRACKER` role，不會被改成 `TAK`，Tracker 的位置送出、省電與 rebroadcast 語意維持不變。
- App 建立的 Primary / Secondary 自訂頻道、名稱、PSK、uplink/downlink 與位置分享設定都會保留。

## TraceRoute Improvements

- ShortName 搜尋結果由置中小視窗改為整頁顯示，完整呈現 `ShortName` 與自動換行的 `LongName`。
- 搜尋成功時新增 `綁定`、`返回` 兩個明確操作；找不到裝置時只顯示有效的 `返回`。
- 離開搜尋結果後，`綁定節點` 清單游標固定回到 `返回`，避免意外操作搜尋到的節點。
- TraceRoute 綁定／解除綁定與 Home 旋鈕鎖定改由旋鈕實際 GPIO hold 時間判定，避免其他按鍵、延遲 OneButton 狀態或跨頁按壓造成假長按。
- 修正 rotary 按住輪詢被 `INT32_MAX` 覆蓋成永久等待，恢復 1 秒 TraceRoute 長按與 3 秒 Home 旋鈕鎖定。
- 保留長按放開後的短按抑制，確認框不會再被 release 補送的短按立即關閉。

## Documentation

- 更新 `docs/TAK_MODE.md`，說明 TAK Tracker 共用範圍與自訂頻道保留規則。
- 更新 `docs/TraceRoute.md`，說明整頁搜尋結果、直接綁定與返回焦點。
- 更新 `docs/CHANGELOG.md` 與 `docs/CHANGELOG_MINI.md`。

## Artifacts

- OTA: `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260714_1808.bin`
- Factory: `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260714_1808.factory.bin`

## SHA256

```text
131f1d890d1aae36ca8b6adf915f5493a2678a8444b8b4adcdeb064fdee8274f  HXB_C0.3.7_20260714_1808.bin
382b2fd360fb83f7e89d206648574f60d1b4c3ad6bbe37dd9c78eb10f2e03b2c  HXB_C0.3.7_20260714_1808.factory.bin
```

## OTA Command

```bash
curl -# -H 'Expect:' -H 'X-Hermes-Filename: HXB_C0.3.7_20260714_1808.bin' -T /Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260714_1808.bin http://192.168.43.21/upload-update-bin
```

## Verification

- `platformio run -e heltec-wireless-tracker -j 4` succeeded.
- RAM usage: 34.5%.
- Flash usage: 88.0%.
- Firmware contains `HXB_C0.3.7_20260714_1808`.
- Desktop OTA / Factory artifacts match the build outputs by SHA256.
- `git diff --check` passed before release.
