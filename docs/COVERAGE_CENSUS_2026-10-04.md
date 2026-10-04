# Coverage census: what InDesign draws where the plugin steps aside (2026-10-04)

The owner asked whether full coverage is needed to remove every defect, or whether some strokes can stay InDesign's. The answer depends on whether InDesign's own drawing of each uncovered case shows the defects the plugin removes. This census measured that on InDesign 21.6.0.57 with research build `d8ee50f2`. The plugin's code is unchanged; production `3298f50c` was reinstalled afterward.

## Method

*tools/jsx/build-coverage-census.jsx* builds one document with 64 rows, one case per row, and exports it three times in one InDesign process: natively (*mode=native*), corrected (*mode=story miter=1 shapes=1*), and its geometry layer alone.

- **Page-item rows** carry a base set: outlined text ("AMVWkz", 48 pt), a 10-degree thorn (sides 20 pt), and two overlapping squares. Most rows use Minion; nine use German Beauty, which is pointy and whose outlines overlap.
- **Text rows** use live text set as suite B sets it: Minion 36 pt, "VWAMNvwkyzK", Outside 4 pt, miter limit 4. Six rows use German Beauty instead.
- **Geometry layer:** every item is drawn again, filled Black with no stroke, effect, or transparency. Text is converted to outlines; corner effects and transforms are kept. This is the outline each stroke follows.
- **Twins:** an item that keeps its transform gets a twin one row down: the same outline, untransformed, at the weight the case should show.

*tools/measure_coverage_census.py* renders each row at 8 px/pt in MuPDF and counts the following in square points, per part (outlined text, thorn, squares). Its tests are in *tests/audit/test_measure_coverage_census.py*.

| Count | What it is | The defect behind it |
| --- | --- | --- |
| Escaping | Stroke outside the shape, beyond the stroke's own band and its exterior miters | Miter tips leaving the shape; an Inside stroke spilling out |
| Stray | Stroke inside the fill, beyond the band | A broken Outside offset fill showing the stroke through the letters; contour edges stroked inside an outline |
| Holes | Backdrop inside the fill | Fill that is not drawn |
| Point missing | The part of a point past the limit, out to *PointReach*, that is not drawn | A beveled point (the owner's whole-points rule) |
| Plugin changes | Area where native and corrected differ by more than 64 levels | Zero means the plugin left the row to InDesign |

The record is [*coverage_census_21.6.json*](../tests/audit/results/coverage_census_21.6.json). Holes up to about 30 pt² appear in covered rows too, so treat them as noise from anti-aliasing at narrow features. The documents and PDFs are in *build/look/coverage-census/* (ignored).

## Page items InDesign draws (native, Minion unless marked GB)

| Case | Escaping | Stray in letters | Holes | Point missing | Inner edges | Plugin changes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Center 8 miter (covered control) | 0 | 0 | 24 | 101 | 476 | 122 |
| Outside 4 miter (covered control) | 0 | 144 | 14 | 101 | 343 | 755 |
| Inside 4 miter | **614** | 0 | **215** | 0 | 225 | 0 |
| Inside 8 miter | **3,684** | 0 | **104** | 0 | 278 | 0 |
| GB Inside 4 miter | **85** | 0 | 32 | 0 | 228 | 0 |
| Outside 4 round | 0 | **128** | 2 | n/a | 341 | 0 |
| Outside 4 bevel | 0 | **124** | 4 | n/a | 338 | 0 |
| GB Outside 4 round | 0 | **378** | 21 | n/a | 336 | 0 |
| Center 8 round | 0 | 0 | 6 | n/a | 479 | 0 |
| Center 8 bevel | 0 | 0 | 1 | n/a | 479 | 0 |
| Outside 4 dashed 12/4 | 0 | **122** | 17 | **101** | 266 | 0 |
| Center 8 dashed 12/4 | 0 | 0 | 29 | **101** | 283 | 0 |
| Outside 8 thick-thin | 0 | **267** | 43 | **294** | 508 | 0 |
| Center 8 thin-thin | 0 | 0 | 103 | **101** | 147 | 0 |
| Outside 4 fancy corners | **20** | **904** | **357** | **87** | 1,156 | 0 |
| Center 8 fancy corners | **18** | 0 | 25 | **88** | 299 | 0 |
| Outside 4 inset corners | 4 | **830** | **193** | **90** | 348 | 0 |
| Outside 4 rounded corners | 0 | **613** | **139** | 0 | 326 | 0 |
| Outside 4 opacity 50% | 0 | **144** | 14 | **101** | 343 | 0 |
| Outside 4 drop shadow | 0 | **144** | 14 | **101** | 343 | 0 |
| GB Center 8 opacity 50% | **7** | 0 | 16 | **101** | 479 | 0 |
| Outside 4 scaled 50%, kept as a percentage | 0 | **144** | 14 | **101** | 343 | 0 |
| Outside 4 sheared 20° | 19 | **143** | 9 | **107** | 345 | 0 |
| Center 8 open, filled | 0 | 0 | 0 | **103** | 369 | 0 |
| Outside 4 open path | 0 | 1,220 | 103 | 101 | 250 | 0 |

**Inner edges** counts the squares' contour edges that InDesign strokes inside the filled shape. For Outside, the plugin draws around the filled shape only (owner confirmation pending, Q29); a Center stroke keeps them in both drawings. **n/a**: a round or bevel join has no point to keep.

What the rows show:

- **Inside strokes are broken.** InDesign draws Inside as a stroke of the path offset inward, and that offset fails wherever the shape is thinner than the stroke. The stroke then spills outside the shape (thin letters come out bolder than their own outline), and thin parts vanish: the thorn's spike is drawn neither filled nor stroked. This is the most visible defect in the census, and it happens on ordinary fonts.
- **The Outside offset fill breaks whatever the join, dash, effect, or transform.** On outlined Minion, white stroke shows through the A, M, k, and z (the stray column). The plugin's refill removes this only on the plain miter strokes it covers. Round and bevel joins, dashes, stripes, corner effects, transparency, a scale kept as a percentage, and shear all keep it. German Beauty roughly triples it.
- **Corner effects make it severe.** Fancy, inset, and rounded corners break the Outside fill over much of each letter (stray 613 to 904, holes 139 to 357). Fancy corners also make escaping tips in Center.
- **Beveled points** (the whole-points rule) come back with dashes, stripes, corner effects, transparency, kept scaling, shear, and open paths.
- **Center escaping tips** come back with corner effects, transparency (German Beauty), and shear.
- **Nothing to fix:** Center strokes with round or bevel joins. Their counts sit at the noise floor and they have no points.
- **Scaling:** scaling by script, and by default in the interface, applies the scale to the path and the weight. Such items reach the plugin unscaled and are covered. Only a scale kept as a percentage (*Preferences > General > When Scaling*) and shear stay on the item. With a kept uniform scale, InDesign draws exactly what it draws for the untransformed twin (1 pt² apart), so the stroke simply scales.
- **Shear:** InDesign strokes a sheared item in the item's own space, so its width varies with direction (343 pt² from the twin). The 19 pt² escaping is that varying width, not spikes.
- **Open paths:** InDesign keeps Outside alignment on an open path and picks the side from the path's direction. Glyph contours then come out stroked on their inner side (stray 1,220). This looks like InDesign's design rather than a defect.

## Covered cases: three problems found

1. **Text in a sheared frame** (route W): the plugin strokes the glyphs in the frame's skewed space, so the stroke is up to about 6% thinner or wider depending on direction. InDesign strokes text in page space at an even width. Escaping goes from 7.4 pt² natively to 29.3 pt², and 197 pt² differ. Rotated frames, character skew, horizontal scale, scaled frames, and type on a path are fine.
2. **`FillRulesAgree` probes only edge midpoints** (*IseMiterCore.h*): on two overlapping parallelograms (the sheared squares' untransformed twins), every probe point falls almost exactly on the other shape's edge. The check then reports that the fill rules agree, `OutsideClip` built from the contours fails its own check, and the item falls back to InDesign (plugin changes 0 on the squares, reproduced offline). The fallback kept the drawing from going wrong here, but the same blind spot could in principle let a wrong clip through. The axis-aligned squares in the core tests pass only because their probe lands exactly on an edge.
3. **A gradient fill under an Outside stroke** is not refilled. Where InDesign's offset fill breaks, the corrected drawing shows the backdrop through the letter (holes 101 against 32 natively in the letters), where InDesign showed white stroke.

## Text InDesign draws

The text route covers more than the page-item route. In every text case except one, InDesign's own drawing shows suite B's escaping tips (3 to 5 pt² for Minion, about 1 for German Beauty) and the plugin's drawing removes them (0.1 to 0.7 pt²). That holds in frames at 50% opacity, with a drop shadow, scaled as a percentage, rotated, with skew and horizontal scale, on a path, with gradient fills, and with gradient strokes. The exception is the sheared frame above. The native-versus-corrected count reads 0 at 50% opacity only because the change there falls below its 64-level threshold.

## Where the damage is

- **Outside page items:** every uncovered variant keeps InDesign's broken offset fill, plus beveled points where it has miters. The refill and outside clip don't depend on the join, dash, or effect, so extending the Outside route is the widest single gain. Each variant still has to keep InDesign's look: round or bevel joins are cheap, transparency needs the stroke inside the item's transparency group, corner effects are already in the recorded outline, and dashes and stripes need their pattern rebuilt.
- **Inside strokes:** always broken where the shape is thinner than the stroke; a new alignment for the route (fill, then the stroke at twice the weight clipped to the fill).
- **Center page items:** only the points and the escaping tips at corner effects, transparency, and shear.
- **Leave to InDesign:** Center strokes with round or bevel joins, and Outside on open paths.
