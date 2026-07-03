# HXB_C0.3.7_20260703_1702 Release Notes

Release date: 2026-07-03
Branch: `HermesX_C0.3.7`
Target: `heltec-wireless-tracker`
Build: `HXB_C0.3.7_20260703_1702`

## Highlights

- TAK MODE now uses update-mode style transition pages for enter and exit, showing `進入TAK模式` / `退出TAK模式` before reboot.
- TAK MODE uses the SmartPower home as its main page after activation.
- TAK SmartPower home controls are now:
  - right rotate: open TAK menu
  - left rotate: open `頻道選擇`
  - short press: return to the normal HermesX main menu
- TAK menu adds `GROUP設定`, reusing existing GROUP PIN A/B and PIN display flows for pairing-style field setup.
- `頻道選擇` adds fixed TAK mission slots for avoiding known congested LoRa channel slots.
- TAK input routing now uses the same rotary interpretation path as CannedMessage, and TAK active state blocks CannedMessage from stealing the encoder.
- New `docs/TAK_MODE.md` documents the current TAK MODE behavior, controls, channel selection, GROUP pairing entry, SmartPower relation, and CIV limitations.

## Artifacts

- OTA: `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260703_1702.bin`
- Factory: `/Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260703_1702.factory.bin`

## SHA256

```text
68b3029477f39c80fe86b926000c2039b695ff1de6a2fc039324b35183d466c6  HXB_C0.3.7_20260703_1702.bin
2074899852a65c5dd45694aeb254ab4a41659befe76e30464a7383dddb3ef8d3  HXB_C0.3.7_20260703_1702.factory.bin
```

## OTA Command

```bash
curl -# -H 'Expect:' -H 'X-Hermes-Filename: HXB_C0.3.7_20260703_1702.bin' -T /Users/oldways/Desktop/HermesX韌體/HXB_C0.3.7_20260703_1702.bin http://192.168.43.21/upload-update-bin
```

## Verification

- `platformio run -e heltec-wireless-tracker -j 4` succeeded.
- Firmware string contains `HXB_C0.3.7_20260703_1702`.
- `git diff --check` passed.
