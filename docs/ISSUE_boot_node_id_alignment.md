# Boot Screen Node ID Alignment

## Summary
On the boot screen, the top-right text block (version + Node ID/short name)
showed a left shift on the second line. The issue was caused by combining
manual right alignment with per-glyph right alignment inside `drawMixed`.

## Symptom
- The Node ID line in the top-right block is shifted left by ~1 glyph width.
- The shift is most visible on the second line (Node ID/short name).

## Root cause
- Boot screen uses `HermesX_zh::drawMixed` for UTF-8 mixed text rendering.
- `drawMixed` draws each glyph with `display.drawString`, which respects the
  active text alignment.
- The boot screen set `TEXT_ALIGN_RIGHT` and also manually positioned the text
  by subtracting the measured width from the screen right edge.
- With right alignment enabled, each glyph is individually right-aligned to the
  same X coordinate, effectively applying the right alignment twice and shifting
  the line left by one glyph width.

## Fix
- Introduce a helper that right-aligns per line while forcing left alignment:
  - Split the text by newline.
  - Measure each line width with `HermesX_zh::stringAdvance`.
  - Draw each line at `rightX - lineWidth` using `drawMixed` with left alignment.
- Replace the boot screen text rendering in:
  - `drawIconScreen`
  - `drawOEMIconScreen`

## Files
- `src/graphics/Screen.cpp`

## Validation
- Boot screen top-right block aligns correctly.
- Node ID/short name line no longer shifts left.
