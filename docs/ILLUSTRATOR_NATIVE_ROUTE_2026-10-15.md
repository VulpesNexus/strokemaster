# Illustrator: the native route (2026-10-15)

The owner asked for three things after the text-kept effect: text that stays editable with its corrections following every edit, as in InDesign; the remaining fixtures tested; and parity with the InDesign plugin. This records how each was met, measured on Illustrator 30.8.2.

## Edits followed: correction as native Illustrator data

The InDesign plugin corrects at draw time and leaves the document native, so an InDesign without the plugin still draws every edit, uncorrected. Illustrator has no draw-time hook that reaches its exports, and a live effect's result is cached in the file, so without the plugin an edit is not drawn at all (the text-kept effect: document *HH/HHH*, PDF *HH/HH*).

The native route writes the correction as ordinary Illustrator attributes instead. Illustrator itself draws them, with or without the plugin, and every edit is followed:

| Case | Native attribute | What it does |
| --- | --- | --- |
| Text, one fill color, any strokes | An object-level fill above *Characters* | Every glyph fill paints over every character stroke |
| Text, one stroke, any fills | An object-level stroke below *Characters*; character strokes cleared | Every stroke paints beneath every fill |
| Text, both mixed | None (left as typed, reported) | Not expressible natively; the text-kept effect covers it with the plugin installed |
| Glyph tips and escaping interior miters | A miter limit per character | Bevels the corners the owner's rulings cut, keeps the rest |
| A shape's whole points | A miter limit per path | Keeps a point between straight edges whole past the default limit |
| Outside strokes on overlapping contours | The fill moved above the stroke in the *Appearance* | Hides the stroke of contour edges inside the filled region |

The research plugin writes them through its bridge (`native-route`, `native-fix`); *tools/jsx/strokemaster-fix-selection.jsx* runs both on the selection from *File > Scripts*. The miter limits come from the InDesign plugin's own rules: *plugin/illustrator-probe/NativeMiter.h* reads each outline with `ise_miter::Corners` and picks the one limit that bevels every corner the rulings cut (`TrimmedTip`, an interior miter that would run out through a thin part) and keeps every miter they keep (`WholePoint` past the limit), or the limit that leaves the fewest corners wrong. Its checks are in *tests/native-repaint/IllustratorContourTests.cpp* (30). An independent read (on the API credit) found that an escaping interior miter sharper than the current limit was ignored when a whole point raised the limit, that the first stroke field below *Characters* was skipped, and that Inside strokes, which Illustrator clips to the fill, were being judged as if their miters could escape; all three are fixed and tested, along with scaled frames (the frame's matrix scales the half width), surrogate pairs, invisible characters, and leaks on error paths.

### Measured

The mixed-stroke fixture (point text *HH*/*HH*, 48 pt, black fills, mixed 8 pt strokes) and its mixed-fill twin, MuPDF at 600 dpi (*tools/jsx/ai-native-route-census.jsx*):

| Fixture | Route | Fill-core damage | Off the fills vs native | PDF text |
| --- | --- | ---: | --- | --- |
| Mixed strokes | Fill above | 55,568 → 0 | identical (0 / 0 px) | kept |
| Mixed fills | Stroke below | 72,080 → 0 | identical | kept |
| Both mixed | none | 16,512 (unchanged) | — | kept |
| Mixed strokes, edited without the plugin | Fill above | 0, edit drawn (*HH/HHH*) | — | kept |
| Mixed fills, edited without the plugin | Stroke below | 0, edit drawn | — | kept |

Every export format keeps the correction (*tools/jsx/ai-text-gaps.jsx*, *tools/measure_text_gaps.py*): PDF, EPS (artboard-cropped), PNG, and SVG all measure 0 damage for the native route, against 53,653 to 62,940 natively. The text-kept effect also measures 0 in PDF, EPS, and PNG, but 11,648 in SVG, where its outline strokes and the viewer's substitute font for the live text disagree; the native route draws both from the same text and cannot. Threaded area frames and text that already carries an object-level stroke measure 0 under both routes.

## The census

*tools/jsx/ai-coverage-census.jsx* ports the InDesign coverage census (*docs/COVERAGE_CENSUS_2026-10-04.md*) to Illustrator: the same base set (outlined *AMVWkz*, the 10-degree thorn, two overlapping squares) in Minion Pro, German Beauty, Segoe Script, and Great Vibes, every join, alignment, dash, opacity, effect, gradient, transform, and open-path variant Illustrator has, and live text under each route; 71 rows, 160 pt high so a whole point stays in its row. It exports the native drawing, the drawing after `native-route` and `native-fix` on every item, and the geometry layer, measured by *tools/measure_coverage_census.py* unchanged (record *tests/audit/results/illustrator-census-2026-10-15.json*).

Totals in square points, native → after the native fixes:

| Rows | Count | Escaping | Stray | Holes | Point missing |
| --- | ---: | --- | --- | --- | --- |
| Page items, Outside, mitered | 19 | 27.4 → 27.4 | 6,333.8 → 235.4 | 383.8 → 315.5 | 3,128.2 → 744.7 |
| Page items, Center, mitered | 13 | 15.8 → 15.7 | 5,639.1 → 5,639.1 | 4,210.3 → 4,210.3 | 2,140.5 → 861.0 |
| Page items, Inside | 11 | 0.0 → 0.0 | 2,805.6 → 2,805.6 | 481.9 → 481.9 | 0.0 → 0.0 |
| Page items, round or bevel joins | 8 | 0.0 → 0.0 | 2,431.8 → 2,431.8 | 24.7 → 24.7 | 1,303.4 → 1,303.4 |
| Live text | 20 | 60.2 → 22.7 | 0.0 → 0.0 | 0.0 → 0.0 | 0.0 → 0.0 |

- **Whole points**: every mitered row's thorn tip, beveled past the limit natively (about 163 pt² missing), is drawn whole (about 6 pt², the anti-aliased edge). What stays missing is in the dashed rows, where a gap can fall on the point, and in the open-path rows.
- **Outside on overlapping contours**: the stroke of inner contour edges (about 252 pt² per row on the squares, 294 on joined script letters) is gone on every Outside row, under opacity, a drop shadow, gradients, transforms, and Round Corners. The open-path row drops from 1,983 to 209 pt².
- **Text**: Minion's escaping miters drop from 3.8 to 0.3 pt² per row, within the InDesign plugin's corrected range (0.1 to 0.7); they came from the crotches of v and w, whose interior miters ran out through the letters' thin bottoms. Segoe Script drops from 2.7 to 0. Rotated text and text on a path keep about 2 pt².
- **Unchanged by design**: a Center stroke's inner contour edges (the InDesign plugin keeps them too), round and bevel joins (no miter to keep whole), and Inside rows (see *What remains*). The gradient-fill rows' *holes* rise because the measurement reads the light end of the gradient, now drawn over the inner edges, as backdrop.

## What Illustrator already gets right

Several defects the InDesign plugin exists to remove do not occur in Illustrator: Inside strokes never escape the shape (InDesign: 614 to 3,684 pt² at Inside 4 and 8), Outside strokes have no broken offset fill showing through the letters, and corner effects and transforms are applied to the art, so no stroke is sheared or scaled anisotropically. Nothing is needed there.

## What remains

- **Text with both fills and strokes mixed** (rare: a recolored word that also has its own stroke): no native attribute expresses it. The text-kept effect corrects it and keeps the text, but needs the plugin for an edit to be drawn.
- **Inside strokes on overlapping contours** (247 pt² of stroke along inner contour edges on the squares): no native paint order confines an Inside stroke to the region's boundary. Correcting it means drawing the stroke, with the effect's trade-off.
- **Dashed whole points**: a dash gap can fall on a point. Illustrator's own *Align Dashes to Corners* option fixes it by hand; writing that option as Beautiful Strokes parameters made a Center stroke vanish, so `native-fix` leaves dashes alone.
- **Glyph tips are beveled, not cut flush**: InDesign's plugin cuts a trimmed tip one half width past its vertex; a native bevel cuts it at the vertex. Both remove the spike.
- **A per-character limit is per glyph, not per corner**: where one glyph has a corner to keep sharper than a corner to cut, one ruling is not met (24 kept corners beveled and 31 cut corners mitered of 6,446 in the census).
- **An edit can outdate a limit**: a limit chosen for one outline stays when the text changes; running the fix again re-chooses it.

## Harness notes

*tools/run-ai-fresh.ps1* runs every probe in a fresh, quietly launched Illustrator inside the workspace guard, and puts Illustrator's preferences back after the quit: scripted documents had filled the owner's *File > New* recent sizes, after which `documents.add` failed with error 129 (the probes fall back to `addDocument`). The census found several Illustrator scripting traps, recorded in the workspace memory: a lazy selection clear, transforms that default to the document origin, and a compound-path matrix transform that corrupts the next one (page items are sheared through their points).
