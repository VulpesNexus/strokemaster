# Fixture A: adjacent-line stroke overlap

Reproduction notes for the adjacent-line overlap. The mechanism is in *docs/STROKE_MECHANISMS.md*, M1–M3.

## Files

| Path | What it is |
| --- | --- |
| *../corpus/pristine/fixture-pristine.indd* | The supplied fixture, read-only; fixture A is the two coincident "Text on separate lines…" frames |
| *minimal/A-minimal.indd* | Minimal reproducers, one variation per frame, built by *tools/jsx/build-A-minimal.jsx* |
| *actual/A-minimal.pdf* | InDesign's *[High Quality Print]* export |
| *actual/A-minimal-300ppi.png* | InDesign's own raster export |
| *actual/A-minimal.idml* | IDML export |
| *actual/A-minimal.manifest.json* | Each frame's name, variation, bounds, and line baselines, written by the build script |
| *actual/A-minimal.probe.md*, *actual/A-minimal.probe.json* | Measurements from *tools/stroke_probe.py* |
| *actual/crops/* | Isolated renders per measured frame: as exported on top, strokes first below |
| *actual/probe/* | The edited PDFs behind each measurement |
| *expected/A-minimal-expected.pdf*, *expected/A-minimal-expected-300ppi.png* | The export with each frame's Outside strokes moved before that frame's fills, written by *tools/make_expected.py* |

## Reproduce

1. With InDesign 2026 running, build the reproducers: *powershell -NoProfile -File tools\run-jsx.ps1 build-A-minimal.jsx*
2. Measure: *python tools/stroke_probe.py tests/indesign-stroke/A-adjacent-line-overlap/actual A-minimal*
3. Write the expected output: *python tools/make_expected.py tests/indesign-stroke/A-adjacent-line-overlap A-minimal*

The Python tools need pikepdf, PyMuPDF, NumPy, Pillow, and SciPy.

## Frames and results

Unless a row says otherwise, text is Minion Pro Regular 24 pt with auto leading (28.8 pt), fill Black, and an 8 pt Paper stroke, Outside, Round join, over an opaque 50 % Black backdrop. Pixel counts are at 720 dpi (10 px per point). "Objects" lists the PDF text objects on each line in paint order: S for stroke, F for fill, FS for both in one object.

| Frame | Varies | Objects per line | Hidden fill | With strokes first |
| --- | --- | --- | --- | --- |
| A01 | Two paragraphs | S F / S F | 1,536 px | 0 |
| A02 | Forced line break | S S F F / S F (one run, split by the exporter) | 1,536 px | 0 |
| A03 | Soft wrap | S F / S F | 1,536 px | 0 |
| A04 | Center alignment at 16 pt | FS / FS | 62,497 px, all of the fill | Not applicable |
| A05 | Leading 60 pt | S F / S F | 0 | 0 |
| A09a | Stroke 3 pt | S F / S F | 0 | 0 |
| A09b | Stroke 5 pt | S F / S F | 0 | 0 |
| A08 | Unthreaded pair, upper frame in front | S F each | 2,126 px | 0 |
| A06 | Threaded pair, story starts in the lower frame, upper frame in front | S F each | 2,126 px | 0 |
| A07 | Threaded pair, upper frame behind | S F each | 1,536 px | 0 |
| A10 | Line 1 split into three runs by a fill color | S F S F S F / S F | 3,244 px | 0 |
| A11 | Frame text opacity 90 % (transparency group) | S F / S F | 1,506 px | 0 |

## Reading the results

- Inside a frame, each wax run is painted as its own stroke and then its own fill, in story order. A06 against A07 shows that separate frames follow stacking order even when they are threaded.
- Painting the Outside strokes first removes the damage in every case without moving a glyph, so that is the expected output.
- A04 is not this defect: a Center stroke is painted over its own fill by definition.

## With the plugin installed

The run before the plugin existed is preserved in *actual-before-plugin/*, and both runs are measured with the same library versions, so the comparison has no confound. Rebuild the reproducers with the plugin loaded and measure again to reproduce this.

| Frame | Hidden fill before | After |
| --- | --- | --- |
| A01, A02, A03 | 1,536 px | 0 |
| A10 | 3,244 px | 0 |
| A11 | 1,506 px | 0 |
| A04 Center alignment | 62,497 px | 62,497 px |
| A05, A09a, A09b | 0 | 0 |
| A06, A08 | 2,126 px | 2,163 px |
| A07 | 1,536 px | 1,657 px |

A04 and the three clean frames are the controls: a Center stroke is painted over its own fill deliberately, so the adornment skips it, and frames with no overlap must not acquire any. The cross-frame pairs are outside what a text adornment can reach, because it is scoped to one frame; their small rise is the enlarged fill core meeting the unchanged stroke of the frame in front, not new damage. Every `hidden_fill_px_strokes_first` reference is unchanged, so the measurement's own baseline did not move.
