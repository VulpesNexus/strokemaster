# Fixture D: Create Outlines on stroked text

Reproduction and correction notes for *Type > Create Outlines*. The mechanism is in *docs/STROKE_MECHANISMS.md*, M6.

## Files

| Path | What it is |
| --- | --- |
| *../corpus/pristine/fixture-pristine.indd* | The supplied fixture, read-only; fixture D is the frame holding three anchored outline polygons |
| *minimal/D-outlines.indd* | Native conversions of live text, built by *tools/jsx/build-D-outlines.jsx* |
| *minimal/D-faithful.indd* | Live text next to the faithful conversion, built by *tools/jsx/build-D-faithful.jsx* |
| *minimal/D-hook.indd* | The real *Create Outlines* menu command with the faithful hook installed and removed, built by *tools/jsx/build-D-hook-test.jsx* |
| *minimal/D-contours.indd* | Outside alignment on outlines with contours as converted, counters reversed, and all contours reversed, built by *tools/jsx/build-D-contours.jsx* |
| *actual/\<stem\>.pdf*, *-300ppi.png*, *.idml*, *.manifest.json* | Exports and manifests for each document |
| *actual/\<stem\>.probe.md*, *.probe.json* | Pair measurements from *tools/stroke_probe.py*: live text against the converted copy 290 pt to its right |
| *actual/crops/* | Each pair, live text above and converted copy below |

The expected output for every pair is its live text, the left half of the pair.

## Reproduce

1. With InDesign 2026 running:
   - *powershell -NoProfile -File tools\run-jsx.ps1 build-D-outlines.jsx*
   - *powershell -NoProfile -File tools\run-jsx.ps1 build-D-faithful.jsx -With ise-faithful-outlines.jsx*
   - *powershell -NoProfile -File tools\run-jsx.ps1 build-D-hook-test.jsx -With ise-faithful-outlines.jsx,ise-create-outlines-hook.jsx -Engine iseHookTest*
   - *powershell -NoProfile -File tools\run-jsx.ps1 build-D-contours.jsx*
2. Measure each stem: *python tools/stroke_probe.py tests/indesign-stroke/D-create-outlines/actual \<stem\>*

The hook test opens a temporary document with a window, only invokes the menu command while that document is active, removes the hook before returning, and closes the document without saving.

## Results

All pairs use Minion Pro Regular 48 pt, fill Black, Paper stroke, over an opaque 50 % Black backdrop. Black and white pixel counts are live text first, converted copy second, at 720 dpi.

| Stem | Pair | Differing pixels | Black pixels | Result |
| --- | --- | --- | --- | --- |
| D-outlines | D01 Outside 4 pt, native conversion | 179,527 | 79,915 / 931 | Center 4 pt stroke over the fill; fill invisible |
| D-outlines | D02 Center 8 pt, native conversion | 98,781 | 0 / 931 | Converted to Center 4 pt: half the weight |
| D-outlines | D05 native conversion, then Outside | 79,149 | 79,913 / 38,018 | Counters wrong |
| D-contours | K01 Outside as converted, "Bog egg" | 229,196 | 147,417 / 19,030 | Wrong |
| D-contours | K02 Outside with counters reversed | 237,022 | 147,417 / 116,618 | Counters fill in solid; still wrong (white 344,420 / 385,626) |
| D-contours | K03 Outside with every contour reversed | 229,199 | 147,416 / 19,016 | Same as K01 |
| D-faithful | F01 Outside 4 pt | 1,322 | 79,914 / 79,948 | Matches within antialiasing |
| D-faithful | F02 Center 8 pt | 901 | 0 / 0 | Matches |
| D-faithful | F03 Outside 4 pt, Miter, limit 10 | 1,350 | 79,914 / 79,948 | Matches |
| D-faithful | F04 two lines, 24 pt, Outside 8 pt | 10,859 | 60,270 / 61,833 | Matches, and restores the descenders the live text hides (fixture A) |
| D-faithful | F05 Outside 4 pt, "Bog egg" | 1,975 | 147,416 / 147,444 | Matches |
| D-hook | H01 menu command with the hook | 1,324 | 79,913 / 79,948 | Matches |
| D-hook | H02 menu command without the hook | 179,529 | 79,914 / 931 | Native result |

## What the conversion produces

| Live text | Native result |
| --- | --- |
| Outside 4 pt, Round | Polygon, Center 4 pt, Round, overprint fill on |
| Center 8 pt, Round | Polygon, Center 4 pt, Round, overprint fill on |
| Outside 4 pt, Miter, limit 10 | Polygon, Center 4 pt, Miter, limit 10 |
| Two paragraphs | Group of one polygon per line |
| A range inside a story | Anchored polygon, Center 4 pt |

## Notes

- The faithful converter handles whole text frames with uniform stroke and fill settings. Other selections are left to InDesign.
- When the hook cancels the native command, a scripted `invoke()` reports "User canceled this action". A menu click doesn't show an error.
- After the canceled command, the group the hook created no longer had the name the converter gave it; its contents and appearance were correct. Keeping names in this path still needs work.
- In InDesign 21.5.1 scripting, setting `strokeWeight = 0` on a path whose stroke color is already None turns on a 1 pt black stroke. The converter sets only the stroke color.
