# Fixture B: miter-join spikes

Reproduction notes for the miter spikes. The mechanism is in *docs/STROKE_MECHANISMS.md*, M5.

## Files

| Path | What it is |
| --- | --- |
| *../corpus/pristine/fixture-pristine.indd* | The supplied fixture, read-only; fixture B is the "Miter joints create awful spikes…" frame |
| *minimal/B-miter.indd* | Minimal reproducers, built by *tools/jsx/build-B-miter.jsx* |
| *actual/B-miter.pdf*, *actual/B-miter-300ppi.png*, *actual/B-miter.idml* | Exports |
| *actual/B-miter.manifest.json* | Entries with bounds, requested and read-back miter limits, wedge angles, and apex positions |
| *actual/B-miter.probe.md*, *actual/B-miter.probe.json* | Measurements from *tools/stroke_probe.py* |
| *actual/crops/* | Isolated renders of each measured entry |

There is no expected output yet. PDF and PostScript can only miter a join or bevel it, so clamping a long miter at a sane length has no operator equivalent. Choosing the target join behavior is part of the fix design.

## Reproduce

1. With InDesign 2026 running: *powershell -NoProfile -File tools\run-jsx.ps1 build-B-miter.jsx*
2. Measure: *python tools/stroke_probe.py tests/indesign-stroke/B-miter-spikes/actual B-miter*

## Entries

| Section | Entries | What varies |
| --- | --- | --- |
| B1 | "VWAMNvwkyzK", Minion Pro 36 pt, Paper stroke 4 pt Outside, Miter | `miterLimit` on the text: 1, 1.5, 2, 3, 4, 10, 100, 500 |
| B2 | Same glyphs | Center alignment at 8 pt with limits 4 and 1; weights 1, 2, 6, and 8 pt at limit 4 |
| B3 | Isosceles wedges, apex up, 60 pt tall | Apex angle 10°–90°; Outside 4 pt and Center 8 pt; Miter, limit 4 |
| B4 | Create Outlines of the limit-4 text | As converted, and with the paths set to Outside |

## Results

**The text attribute reaches the output.** Every B1 entry emits exactly its `miterLimit` as the PDF `M` operand.

| Limit (B1) | Largest reach of the stroke from the glyph edge | Stroke pixels beyond the 4 pt weight |
| --- | --- | --- |
| 1 | 4.1 pt | 0 |
| 1.5 | 5.66 pt | 7,365 |
| 2 | 6.8 pt | 7,931 |
| 3, 4, 10, 100 | 6.8 pt | 8,299 |
| 500 | 6.8 pt | 8,286 |

**Spikes scale with the weight.** At limit 4, the largest reach is 1.64, 3.36, 6.8, 11.81, and 16.7 pt for weights of 1, 2, 4, 6, and 8 pt.

**Wedges.** With Center strokes, the apex bevels below 29° and miters from 30° up, matching the textbook within 0.15 pt: 0.4 pt at 10° against 0.35 predicted, 15.3 pt at 30° against 15.45, and 7.9 pt at 60° against 8.0. With Outside strokes, InDesign offsets the path by half the weight, fills it, and strokes it at the full weight. Beveled apexes then come out flat-topped at 2.2–2.5 pt, and mitered ones reach the textbook distance.

**Outlines.** Converted as they come, the paths reach 3.36 pt, because the converted stroke is centered. Set to Outside, they reach 4.9 pt, less than the live text's 6.8 pt, because the offset stage bevels first.
