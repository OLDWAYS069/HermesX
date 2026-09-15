# HermesOne compact UI raster check

Run from the repository after a PlatformIO build has installed the OLED library:

```sh
python3 bin/hermesone_ui_verify/verify.py /tmp/hermesone-ui-preview
```

Requires `clang++` and Python Pillow. Output includes 128×64 PGM frames and a
nearest-neighbor enlarged `preview.png` contact sheet.

The harness compiles the real CN12 renderer/data, text wrapping code and
EM/low-memory/rotary renderers. It extracts the current keyboard functions and
compact update/stealth branches directly from source. The host adapter implements
the OLED pixel operations and decodes the actual Arial bitmap font.

Checks cover all 137 cursor positions in the upper/lower/Bopomofo layouts,
pixel bounds, whole-line scrolling through the last page, candidates, toasts,
and the four-row 128x64 function-list layout (including the bottom row).
Inspect the images for legibility and separation of text and controls.

This does not emulate input events, animation timing on hardware, I2C transfers,
screen transitions or overlay ownership. Those require source review and device testing.
