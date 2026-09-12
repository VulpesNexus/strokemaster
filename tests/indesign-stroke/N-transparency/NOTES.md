# Fixture N: transparency, blend modes, gradients, and overprint

This fixture does not reproduce a defect. It exists to answer the opposite question: when the plugin repaints a run's fill in the foreground pass, does it change anything it should not?

Takeover correction, 2026-09-12: the original comparison eroded all non-background ink, including the stroke halo and effects. Its “core” was not an independent glyph-fill mask. The numerical observations below are retained as historical ink-mask measurements; their compositing verdicts are withdrawn. Even opaque fills can change at partially covered edges or when differently colored runs overlap.

## Files

| Path | What it is |
| --- | --- |
| *minimal/N-transparency.indd* | The fixture, built by *tools/jsx/build-N-transparency.jsx* |
| *actual/* | Exported with the plugin loaded: PDF, PNG at 300 ppi, IDML, manifest, probe |
| *actual-plugin-replay/* | The same saved document re-exported with the plugin loaded, plus EPS and PostScript |
| *actual-noplugin/* | The same saved document exported with the plugin removed from *Plug-Ins* |
| *actual-noplugin/plugin-vs-noplugin.md* | The per-variant comparison, written by *tools/compare_renders.py* |

## Method

The plugin corrects drawing, not the document, so the only control that isolates it is to render **one saved document twice under two renderers**. Rebuilding the fixture twice would compare two documents. *tools/jsx/ise-reexport.jsx* opens the saved *.indd* without a window, exports, and closes with `SaveOptions.NO`, so the file on disk is never touched.

Three controls make the comparison trustworthy:

- **The control on the control.** The build export and a replay export, both with the plugin loaded, differ by **0 pixels across the whole page**. Re-exporting is therefore a faithful reproduction, and any later difference is attributable to the renderer alone.
- **The plugin really was absent.** The plugin writes a trace line as its library is mapped. With the plugin folder moved aside the trace file was never created, so the "no plugin" numbers are not a mislabeled second run.
- **Variants the adornment must skip.** N20 (Center stroke), N21 (no stroke), and N22 (no stroke, gradient fill) change by **0 pixels**. A Center stroke is painted over its own fill by definition and a run with no stroke has nothing to correct, so any change there would mean the adornment is reaching runs it has no business touching.

Two palettes are used. The *classic* palette is Black on a Paper stroke, comparable with fixture A. The *mid* palette is a 50 % black fill with a 60 % magenta stroke, chosen because Multiply of black over anything is still black: a black fill would hide exactly the defect being looked for.

## Results

Historical measurements from InDesign's PNG export at 300 ppi. The old “Core lum” includes eroded stroke/effect ink and is not a measurement solely of the glyph body. Unchanged averages do not prove unchanged per-pixel fill color. The table's final column records the old interpretation for traceability, not the current conclusion.

| Variant | Historical ink-mask luminance, no plugin → plugin | Withdrawn interpretation |
| --- | --- | --- |
| N01 blend Multiply | 99.11 → 99.07 | No double composite |
| N02 blend Screen | 247.78 → 247.13 | No double composite |
| N03 blend Overlay | 253.00 → 253.00 | Identical |
| N04 blend Darken | 99.38 → 99.38 | Identical |
| N05 blend Lighten | 255.00 → 255.00 | Identical |
| N06 blend Difference | 40.37 → 39.73 | No double composite |
| N07 text opacity 50 % | 166.95 → 166.95 | Identical |
| N08 Multiply at 50 % | 124.26 → 124.21 | No double composite |
| N09 object blend Multiply | 99.11 → 99.07 | No double composite |
| N10 object opacity 50 % | 166.95 → 166.95 | Identical |
| N11 gradient fill | 234.19 → 232.56 | Repaint ring only |
| N12 gradient stroke | 214.94 → 212.74 | Repaint ring only |
| N13 both gradient | 221.71 → 220.06 | Repaint ring only |
| N14 overprint fill | 172.46 → 172.46 | Identical |
| N15 overprint stroke | 172.46 → 172.46 | Identical |
| N16 overprint both | 172.46 → 172.46 | Identical |
| N17 spot color, overprint | 239.06 → 237.35 | Repaint ring only |
| N18 drop shadow on text | 152.41 → 152.41 | Identical |
| N19 feather on text | 172.02 → 172.02 | Identical |
| N20, N21, N22 controls | unchanged | 0 pixels changed |

### Observed transparency grouping and unresolved compositing

The sampled exported PDF places added path fills inside form XObjects */Fm0* to */Fm11*. This grouping is a useful observation. It does not prove idempotence for every fill, antialiased boundary, overlapping color, or effect stack, nor does an unchanged mask average establish it for N07 or N10.

Remeasure using an independently generated fill-only mask and a single-pass reference. The revised comparison tool explicitly distinguishes external fill coverage from its fallback ink heuristic and has a regression showing the original mask's false classification of changed halo pixels. Renderer, export, and compositing conclusions remain separate.

### Gradients are corrected, and are easy to miss

A gradient fill is not emitted as a path fill. InDesign writes a clip followed by a shading, so a probe that records only `f` and `S` sees nothing and will wrongly report that gradient text is skipped. Counting every operator instead:

| Operator | No plugin | Plugin | Delta |
| --- | --- | --- | --- |
| `f` path fill | 2 | 38 | +36 |
| `sh` shading | 0 | 4 | +4 |
| `W` clip | 19 | 23 | +4 |
| `TJ` show text | 86 | 86 | **0** |

The 36 added path fills are the 18 solid-filled stroked frames at two lines each, and the 4 shadings are N11 and N13 at two lines each: 40 repaints for the 20 stroked frames, and none for the 3 controls. Live text is untouched.

## Export, EPS, and print

The correction is not specific to PDF export. Re-rendered through Ghostscript at 300 ppi, the EPS export and a print to a PostScript file both carry it, with the same per-variant pattern.

InDesign writes EPS as a DOS EPS: a binary wrapper whose first four bytes are `C5 D0 D3 C6`, with a TIFF preview ahead of the PostScript. Ghostscript must be handed the PostScript section, whose offset and length are in the wrapper. Ghostscript also cannot open a path containing this repository's en dash, so it is run from an ASCII working directory.

The printed page is cropped to US Letter while the fixture page is 900 pt tall, which removes only N21 and N22, both zero-change controls. Both PostScript comparisons therefore report the same whole-page count, 29,960 changed pixels, because they contain the same set of changed pixels.

The EPS and PostScript files themselves are not committed. InDesign stamps a `%%For:` line naming the machine's user into the DSC header of both, which *tools/scan_identifiable.py* reports, and it cannot be edited out without rewriting the DOS EPS section offsets that follow it. The exported PDF, PNG, and IDML carry no such stamp and are kept. Regenerate the PostScript with *tools/jsx/ise-reexport.jsx* and *tools/jsx/ise-print-ps.jsx*.

## Performance

Seven repetitions each, re-exporting the corpus fixture, median milliseconds:

| Export | No plugin | Plugin | Overhead |
| --- | --- | --- | --- |
| PDF | 1171 | 1181 | +10 ms, about 0.9 % |
| PNG at 300 ppi | 1093 | 1120 | +27 ms, about 2.5 % |

Screen redraw was not timed directly. PNG export rasterizes through the same drawing code, so it is the closest available proxy.

## Reproduce

1. Build: *powershell -NoProfile -File tools\run-jsx.ps1 build-N-transparency.jsx*
2. Re-export the saved document, with and without the plugin, using *tools/jsx/ise-reexport.jsx* through *run-jsx.ps1 -ArgJson*.
3. Compare: *python tools/compare_renders.py &lt;noplugin.png&gt; &lt;plugin.png&gt; &lt;manifest.json&gt; &lt;out-stem&gt;*

The comparison crops each frame with 12 pt of padding. Do not widen it: the rows are 24 pt apart and every frame's Outside stroke reaches 8 pt beyond it, so a 24 pt pad takes in part of the next row and reports that neighbor's repaint as this frame's change.
