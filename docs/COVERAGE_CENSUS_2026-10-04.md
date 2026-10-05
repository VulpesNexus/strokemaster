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

## Follow-up: extended coverage (2026-10-04 to 10-05)

The owner asked for every fix the census pointed to, with essentially no regressions. Research build `9b924925` covered all of them. *build-coverage-census.jsx* grew to 103 rows: Inside joins and limits, dashes and stripes, corner effects, stroke-level and fill-level transparency, shear and uneven kept scale, and filled text frames with frame strokes. It was exported five ways: native, corrected, *xp=0*, *probe=native*, and geometry.

*measure_coverage_census.py* gained two counts:

- **Spill:** fill-colored pixels outside the outline.
- **Off:** pixels far from every expected gray, such as a stroke drawn opaque that should be 50%.

The certified build's census was measured again with the same tool, so that every row could be compared with what `3298f50c` drew. *tools/compare_census_runs.py* compares two exports row by row. The record is [*coverage_census_2_21.6.json*](../tests/audit/results/coverage_census_2_21.6.json). An independent review of the code found the same problems.

**What the census build fixed** (all counts in pt², native → corrected):

- **Outside, every join and dash:**
  - Round, bevel, dashed, dotted, gap color, thick-thin, and limit 2 lose all stray stroke through the letters and all holes, on Minion and German Beauty. For example, round 468 → 0 and dashed 388 → 0.
  - The pattern stays InDesign's: dashes are InDesign's own stroke, drawn under a clip.
- **Outside with object-level opacity or a drop shadow, a kept scale, or shear:** the same gains, with whole points.
- **Outside with corner effects on squares and the thorn:** the offset fill is whole again. On fancy corners, stray goes 2,478 → 0 and holes 357 → 0.
- **Gradient fills are refilled:** the hole InDesign's offset left in the A is gone. Inside the letters, the gradient matches InDesign's to within 0.17 levels.
- **Inside strokes:** escaping 614 → 0 and holes 215 → 18 on Inside 4 miter; the same for round, bevel, and limit 2. The thorn's spike is drawn again.
- **Text:**
  - In a frame scaled 150% × 100% (kept), escaping goes 677 → 0.9.
  - In a sheared frame it goes 7.4 → 0.3, with the stroke now even in page space as InDesign draws it.
  - A 200% kept scale and German Beauty sheared are fixed as well.
- **Covered rows are unchanged from `3298f50c`** on every count, beyond anti-aliasing.

**What the census build broke, and what the source now does instead** (production `410689fd`, research `ebd06435`; measured on the host below):

| Census build's defect | Measured | Now |
| --- | --- | --- |
| A stroke with its own opacity drew opaque | Off 67 → 4,333 (Outside), 18 → 5,946 (Center) | Stroke-level and fill-level transparency and effects are left to InDesign. Object-level ones stay covered. |
| A stroke's own drop shadow was lost | Shadow 1,607 → 0 | The same refusal |
| A fill's own 50% opacity was refilled opaque | Off 272 → 5,530 | The same refusal |
| Inside on curved outlines showed InDesign's inward offset fill spilling past the letters | Spill 60 → 754 (Inside 8), 55 → 1,736 (fancy corners) | Inside is taken over only for straight-segment outlines without corner effects or content (squares, the thorn, frames without content). Elsewhere it stays InDesign's. |
| Corner effects on outlined text threw 16 pt blocks and spikes from loop pinches | Escaping 4.6 → 352 (inset), 20 → 330 (fancy) | Corners of the resolved region that the outline lacks keep the stroke's bevel past the limit. |
| Inside on a filled text frame painted one device pixel past the frame | Spill +46 | Inside needs an item without content. |

How the crossing-corner rule was checked: an offline simulation reproduces the plugin's Outside drawing from its own core code. Over every Outside page-item outline of the census, it changes only the 15 pinch corners on the fancy and inset letters, and nothing on German Beauty or any other row. Beveling pinches within the limit too would have changed 6 to 10 of German Beauty's certified corners, so the rule stops at the limit.

What is left:

- **Rounded and bevel corner effects on outlined text:** escaping goes 0.1 → 26 and 0.9 → 6. These are thin loops the corner effect adds to the path; InDesign's own stroke of the same path draws them too.
- **Open paths:** never reached in the census, and interior miters would be cut by the closing line, so they stay InDesign's.
- **Text in a frame both sheared and rotated:** 94 → 88. Both drawings escape alike, so the cause is not found yet.
- **Not tested:** vertical text, which now stays in its own coordinates; type on a path in a sheared frame; radial gradients.

## Host run of the candidate (2026-10-05)

Production `410689fd` was certified first (session C11, [report](ROUTE_X_21_6_RECERTIFICATION_2026-10-03.md#extended-coverage-route-x-candidate-410689fd)). The census then ran on its research twin `ebd06435` in *build/look/coverage-census-3/* (ignored), with the same five exports. The record is [*coverage_census_3_21.6.json*](../tests/audit/results/coverage_census_3_21.6.json): the corrected export against native, and the *xp=0* export.

**Every defect of the census build is gone:**

- Stroke-level and fill-level opacity, and the stroke's own drop shadow (rows 60, 61, 62, 64, and 75), measure the same as native in every part.
- On every Inside row the text measures the same as native. So do the fancy corners Inside (row 56) in every part, and the filled frame's Inside stroke (row 101).
- Corner effects on outlined text no longer throw blocks: escaping is 33.8 pt² with fancy corners (row 12, census build 330) and 19.6 with inset corners (row 14, census build 352).

**Every other row measures as on the census build**, within 1 pt² (off-level pixels within 2.5), so its gains hold: Outside with every join and dash, Inside on squares and the thorn, gradient refills, kept transforms, corner effects on squares and the thorn, and sheared text.

**One gap against the design.** Object-level opacity and the object's drop shadow (rows 16, 17, 18, 41, 42, and 63) are left to InDesign as well, text included. The certified `3298f50c` left them to InDesign too, so this is no regression, but the census build's gain on them is lost. The *xp=0* export, which refuses all transparency, measures the same as the corrected export on all 103 rows: the check for a stroke's or fill's own transparency (*StrokeOrFillTransparency*) reports every transparent item. A read of an attribute the item does not carry fails, and the check counted that as transparency; `aca2a6c0` fixes it (below).

**Still worse than native, as before:**

- Corner effects on outlined text keep a few long points at the corners the effect adds: escaping is 33.8 against native's 19.9 (fancy, row 12) and 19.6 against 4.6 (inset, row 14). Native draws white blocks over the letters there, so the rows are cleaner overall. Removed by `bc1b850d` (below).
- Rounded and bevel corner effects on outlined text (rows 15 and 58), as listed above. Also fixed by `bc1b850d`.
- Off-level pixels on corner-effect text rise by 40 to 100 pt² (rows 12, 14, 57, 58, and 59), and a sheared bevel line escapes 11.3 pt² against native's 9.8 (row 71). Both measure the same on the census build.

## Object-level transparency covered again (2026-10-05)

Research build `4418348c` traced every transparency attribute read on the census ([*trace-xp-attributes.txt*](../tests/audit/results/routex/trace-xp-attributes.txt)). Items with only object-level opacity or an object drop shadow carry no stroke or fill transparency attribute at all, and every read of an absent attribute through the style descriptor failed. Production `aca2a6c0` and its research twin read an absent attribute as its hard-coded default instead ([report](ROUTE_X_21_6_RECERTIFICATION_2026-10-03.md#object-level-transparency-route-x-candidate-aca2a6c0)).

The census on `4418348c` (*build/look/coverage-census-4/*, ignored; record [*coverage_census_4_21.6.json*](../tests/audit/results/coverage_census_4_21.6.json)) changes rows 16, 17, 18, 41, 42, and 63 only, each to the census build's numbers to the hundredth. Every other row measures as on `ebd06435`. With the object's drop shadow (row 18), spill and off-level pixels rise slightly against native: the thorn's spill from 259.5 to 297.2 pt², and its off-level pixels from 399.7 to 446.2, now that its point is drawn whole; the census build measured the same.

## Corner-effect corners (2026-10-05)

The long points on corner-effect text came from corners the effect adds, none of them on the item's own path. Past the limit, each drew a point at least the limit distance long where InDesign bevels. Within it, the fancy and inset effects' zigzags of segments under half a point drew full miters 7 to 10 pt long, as InDesign's own stroke does. Production `bc1b850d` bevels the first and trims the second to the corner's shorter edge past the stroke ([report](ROUTE_X_21_6_RECERTIFICATION_2026-10-03.md#corner-effect-corners-route-x-candidate-bc1b850d)).

The census on research twin `2b85d65a` (*build/look/coverage-census-6/*, ignored; record [*coverage_census_5_21.6.json*](../tests/audit/results/coverage_census_5_21.6.json)) changes the corner-effect rows only. Escaping text ink falls to 1.6 pt² (fancy, row 12, native 19.9), 0.5 (Center fancy, row 13), 3.0 (inset, row 14, native 4.6), 0.7 (rounded, row 15, native 0.1), and 0.1 (bevel, row 58). Off-level pixels on those rows stay 60 to 90 pt² above native's, at the anti-aliased edges of the effect's zigzags.
