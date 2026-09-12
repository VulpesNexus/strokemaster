# Stroke mechanisms

Phase E of the InDesign stroke engine investigation: what produces each defect, with the controls that establish it. Evidence labels follow the investigation's ladder: PROVEN, STRONGLY SUPPORTED, PLAUSIBLE, DISPROVEN, and UNKNOWN.

Unless stated otherwise, measurements come from *tools/stroke_probe.py* on the minimal reproducers in *tests/indesign-stroke/*. They use MuPDF renders at 720 dpi (10 px per point) of InDesign's *[High Quality Print]* PDF export, after removing every text object and path that doesn't belong to the entry being measured. "Hidden fill" counts pixels that are solid fill when strokes are removed but white in the render being measured. "Strokes first" is the same content with the entry's stroke objects moved before its fill objects and nothing else changed. Re-emitting content in its original order is the control for every edit, and the controls rendered identically to the original.

## M1. Outside text strokes are a doubled centered stroke under each run's own fill

**Claim.** For text with an Outside stroke, InDesign paints a centered stroke of twice the weight and then paints the same glyphs' fill on top. Text with a Center stroke gets a single object that paints the fill first and the stroke over it, at the weight itself.

**Evidence.**

- PDF, Outside: every stroke text object (`Tr 1`) has `w` equal to twice the weight: 14 for the fixture's 7 pt, 8 for 4 pt, 6 for 3 pt, and 16 for the minimal reproducers' 8 pt. It is immediately followed by a fill text object (`Tr 0`) that shows the same glyphs at the same position.
- PostScript (EPS export): the run's outline is built with `charpath`, stroked with `lw` at twice the weight, and then filled with `show` or `xshow`.
- PDF, Center: a single object with `Tr 2` at the weight itself (A04, B2, C09).

**Status.** PROVEN for PDF and PostScript output. It fits the SDK's structure: the run's drawing data carries a stroke alignment, while the graphics port has no alignment and no text rendering mode, so the run drawing has to emulate alignment.

## M2. The paint unit is the wax run, units follow story order within a frame, and frames follow z-order

**Claim.** Each paint unit (stroke, then fill) is one wax run. Within a frame, units are painted in story order. Separate frames are painted in stacking order, even when they are threaded.

**Evidence.**

- Fixture C: 27 style ranges broken across 3 lines give 28 units, one per range per line.
- Line breaks: a paragraph return (A01), a forced line break (A02), and a soft wrap (A03) give identical units and identical hidden fill (1,536 px).
- Threading: A06 and A07 have the same threaded story and the same geometry, and differ only in which frame is in front. The frame in front covers the other: 2,126 px hidden when the upper frame is in front, 1,536 when it is behind. The unthreaded pair A08 matches A06.
- Language control (C19): a language change on one character yields `S S S F F F`: three stroke objects, then three fill objects. That is one run whose stroke call and fill call the PDF exporter splits by the `Lang` marked content. The render shows no seam.
- SDK: text passes visit every wax run in the frame, and the run text drawing is called per run (see *INDESIGN_STROKE_SDK_MAP.md*, stage 4).

**Status.** PROVEN at output level. STRONGLY SUPPORTED that the unit is the wax run: the SDK structure agrees, and the language control separates run boundaries from PDF object boundaries.

## M3. Adjacent-line overlap (fixture A) is M1 and M2 across lines

**Claim.** A line's stroke covers the previous line's fill because each line is its own paint unit, painted after the previous line's fill.

**Evidence.**

| Case | Hidden fill as exported | With strokes first |
| --- | --- | --- |
| Fixture A (600 dpi, whole fixture) | 4,562 px | 0 |
| A01 two paragraphs, 24 pt, 8 pt Outside | 1,536 px | 0 |
| A10 line 1 split into three runs | 3,244 px | 0 |
| A11 frame text at 90 % opacity (transparency group) | 1,506 px | 0 |
| A09a and A09b, 3 pt and 5 pt | 0 | 0 |
| A05, leading 60 pt | 0 | 0 |

Controls from the brief: reversing story order against stacking order (A06 against A07), moving frames while keeping order (A08), and comparing operator order (M2).

**Status.** PROVEN.

**Expected behavior and scope.** For Outside strokes, the union of a frame's strokes should be painted beneath all of that frame's fills. Across separate frames, stacking order should keep deciding, as it does for any overlapping page items. Center strokes are painted over the fill by design: at 16 pt Center on 24 pt text (A04) the stroke covers every fill pixel, and that is the documented meaning of Center, not this defect.

## M4. Run fragmentation (fixture C) is M1 and M2 at run boundaries, and boundaries follow attribute inequality

**Claim.** The seams are a later run's stroke painted over an earlier run's fill. The stroke geometry is not fragmented. A run boundary appears whenever the drawing attributes differ, even when the difference isn't visible.

**Evidence: what splits a run.** Each variant overrides one character in "nnnnnnn" (*C-attribute-sweep.probe.md*).

| Splits the run (interleaved `S F S F S F`) | Keeps one run (`S F`) |
| --- | --- |
| Fill color, fill tint | Tracking, manual kerning, kerning method |
| Stroke color, stroke tint, stroke weight | Stroke weight or fill color set locally to the style's own value |
| Join; miter limit, even under a round join (no visual effect) | No Break (no visual effect) |
| Stroke alignment; stroke or fill overprint | OpenType slashed zero, oldstyle figures, stylistic set 1, ligatures off, discretionary ligatures |
| Font change; point size, horizontal and vertical scale, baseline shift, skew | Empty character style, applied directly, by GREP style, or by nested style |
| Underline, strikethrough, and underline color with underline off (no visual effect) | Hyperlink source; XML tag |
| All Caps, Small Caps, and Lowercase on an already-lowercase letter (no visual effect) | Language (splits PDF objects, not the run) |
| Superscript and subscript | |
| Character style that sets a fill, directly or by GREP style | |
| Applied condition with indicators hidden (no visual effect) | |
| Explicit stroke tint 100 in place of the swatch's tint (no visual effect) | |
| Variable font axis value; variable font named instance | |

Character rotation of 10° drew nothing rotated in this configuration, so that variant is inconclusive.

**Evidence: the halo is continuous.** In fixture C, painting each line's strokes before its fills lowers hidden fill from 6,239 to 124 px; painting the whole frame's strokes first lowers it to 120. The remainder is consistent with the underline path, which is painted before the frame's text and ends up under the strokes. No stroke is missing or offset at any boundary.

**Status.** PROVEN: the seams are paint-order interleaving at wax-run boundaries. PROVEN: boundaries follow attribute inequality, including five differences with no visual effect. DISPROVEN: that stroke geometry is evaluated in fragments. Merging runs for stroke generation is therefore unnecessary; painting a frame's Outside strokes before its fills removes the seams.

**Confirmed from inside the drawing code.** The evidence above reorders an exported file after the fact, which shows what the right order looks like but not that InDesign can be made to produce it. A global text adornment that repaints each Outside-stroked run's fill after the frame's text now does produce it, in InDesign's own drawing, and reaches screen rendering and export alike: the hidden fill of every affected reproducer falls to zero, and on the fixture the three-line frame with 27 style ranges falls from 8,758 covered pixels to 2, measured against the fill core recorded before the plugin existed. See *INDESIGN_STROKE_INVESTIGATION.md*, fixture A, for the results and the costs.

## M5. Miter spikes (fixture B) are standard miter joins governed by the text miter-limit attribute

**Claim.** The spikes are what the PDF and PostScript miter rule produces for the emitted miter limit, which comes straight from the text's miter-limit attribute (default 4). A join is mitered when 1 / sin(θ / 2) ≤ limit and beveled otherwise.

**Evidence.**

- The attribute reaches the output: setting `miterLimit` on text to 1, 1.5, 2, 3, 4, 10, 100, and 500 emits exactly that `M` (B1). The SDK's constant text miter limit of 4.0 is therefore not what reaches output. DISPROVEN as the controlling value.
- The emitted limit alone controls the spikes. Editing only `M` in the fixture's PDF, limits of 3, 4, 10, and 100 render identically, 2 removes the tallest spikes, and 1 equals a bevel join.
- Reach against limit, "VWAMNvwkyzK" at 36 pt with a 4 pt Outside stroke:

  | Limit | Largest stroke reach from the glyph edge | Pixels beyond the weight |
  | --- | --- | --- |
  | 1 | 4.1 pt | 0 |
  | 1.5 | 5.66 pt | 7,365 |
  | 2 | 6.8 pt | 7,931 |
  | 3 to 500 | 6.8 pt | 8,286–8,299 |

- Reach against weight, at limit 4: 1.64, 3.36, 6.8, 11.81, and 16.7 pt for weights 1, 2, 4, 6, and 8 pt. Spikes grow in proportion to the weight, as the fixture's caption says.
- Page-item wedges with Center strokes follow the textbook exactly. Below 29° (ratio above 4) the apex is beveled: 0.4–1.0 pt against 0.35–0.97 predicted. At 30° and above it is mitered: 15.3 pt against 15.45 at 30°, and 7.9 against 8.0 at 60°.
- Page items with Outside strokes use a different construction. InDesign offsets the path outward by half the weight (bevel-truncating acute offset corners), fills that offset path, and strokes it at the full weight. Mitered apexes still reach the textbook distance; beveled ones come out flat-topped at 2.2–2.5 pt.
- For mitered joins, text (a doubled centered stroke) and page items (the offset construction) reach the same distance, weight / sin(θ / 2). The census's suggestion that doubling makes text spikes longer than page items' is DISPROVEN.

**Status.** PROVEN: the mechanism is standard miter joining at the attribute's limit. STRONGLY SUPPORTED: the text miter limit is user-controllable, since it is a document attribute in scripting, IDML, and styles, and the SDK census reports a Stroke panel observer for it; the panel itself is still to be confirmed on screen.

**What is a design question rather than a defect, and how it was answered.** PDF and PostScript can only bevel a join that exceeds the limit. Keeping a corner's miter but cutting it off at a sane length isn't expressible with a line join, because PDF (ISO 32000-1, table 55), PostScript, and SVG all define exactly three. It needs stroke geometry built from the outlines (*INDESIGN_STROKE_SDK_MAP.md*, stage 7).

That behavior is not exotic: it is what Microsoft's graphics stack has done since GDI+, and what FreeType and Qt do by default. What it is not, any more, is a web standard. SVG 2 specified it as `stroke-linejoin: miter-clip` in 2018, no browser implemented it, and the Working Group removed it on 18 March 2026. Describing this project's rule as "SVG 2's miter-clip" would now point at a deleted feature, and the copy of the specification served at *svgwg.org* is stale and still shows it. See *MITER_JOIN_HISTORY.md*.

The design question is now settled. The limit is a **threshold**, and what is wanted is a **clamp**: keep the miter and cut its tip off flat once it would reach further than a chosen multiple of the half-width. The multiple chosen is sqrt(2), the ratio a right angle produces, so the rule reads "no corner may protrude further than a right-angled corner would" and leaves every ordinary corner alone. Sweeping every degree from 1 to 179 at an 8 pt half-width, the native rule changes reach by 23.95 pt between 28 and 29 degrees where it stops mitering, while the clamp's largest step anywhere is 0.0975 pt; reach divided by half-width is identical across half-widths of 4, 8, and 12, so the rule is continuous in the angle and exactly proportional to the weight. It is implemented in *tools/miter_rule.py* and *tools/stroke_outline.py*, whose outlines reproduce Illustrator's own Expand to within 0.11 percent of area. Per-defect consequences and the delivery options are under fixture B in *INDESIGN_STROKE_INVESTIGATION.md*.

## M6. Create Outlines (fixture D) turns the painted stroke into a centered path stroke at half the painted width, over the fill

**Claim.** The conversion keeps the join and miter limit but sets the path's stroke alignment to Center and its weight to half the width InDesign paints for the text. It paints that stroke over the fill and sets fill overprint on.

**Evidence** (*D-outlines.probe.md* and the build log):

| Live text | Resulting path |
| --- | --- |
| Outside 4 pt, Round | Polygon, Center, 4 pt, Round, overprint fill on |
| Center 8 pt, Round | Polygon, Center, 4 pt, Round, overprint fill on |
| Outside 4 pt, Miter, limit 10 | Polygon, Center, 4 pt, Miter, limit 10 |
| Two paragraphs | Group of one polygon per line |
| A text range inside a story | Anchored polygon, Center, 4 pt |

- PDF: each outline is filled (`f`) and then stroked (`S`, `w 4`).
- Pair measurement (D01): the outlines keep 931 black pixels against the live text's 79,915.
- Setting Outside on the converted paths (D05) keeps 38,018 of 79,913 black pixels.
- Contour direction test on "Bog egg" (*D-contours.probe.md*): the converted outer contours and counters run in opposite directions. With Outside applied as converted, 19,030 of 147,417 black pixels remain. With every contour reversed, 19,016 remain, so absolute direction doesn't matter. With only the counters reversed, 116,618 remain, but the counters fill in solid and white rises from 344,420 to 385,626 pixels. Relative contour orientation changes the page-item Outside construction on compound paths (PROVEN). No orientation makes Outside alignment on outlines reproduce the live text (DISPROVEN as a fix).
- A conversion that places a stroke-only copy at the painted width under a fill-only copy reproduces the live text within antialiasing: 1,322 differing edge pixels for Outside 4 pt, 901 for Center 8 pt, and 1,975 for letters with counters (*D-faithful.probe.md*).

**Status.** PROVEN for what the conversion produces. STRONGLY SUPPORTED that it is decided in the conversion's capturing port, from the SDK structure.

## M7. Other Adobe engines split text into the same units, and InDesign 2025 behaves identically

**Claim.** Illustrator breaks stroked text into the same per-line and per-run paint units, but paints each unit's fill first and its stroke over it, centered at the nominal weight. InDesign 2025 produces byte-for-byte equivalent paint structure to 2026 for the same fixture.

**Evidence.**

- Illustrator 2026 (30.7.0), 24 pt text with a white character stroke over a black fill: the exported PDF holds one fill object and one stroke object per line, and a line whose fourth character is magenta splits into three such pairs. The stroke width equals the weight (8 for an 8 pt stroke, not doubled), with `M 4` and the default miter join. Because the stroke is painted over the fill and centered, an 8 pt stroke on 24 pt text covers nearly the whole glyph.
- An object-level stroke on an Illustrator text frame (`stroked = true` on the frame) produced no stroke at all in the exported PDF, so that route is untested here.
- InDesign 2025 (20.5.5.119) opened the fixture's IDML and exported 72 paint objects with the same roles, stroke widths, joins, miter limits, and positions as InDesign 2026's export of the same document. The sequences compare equal.

**Status.** PROVEN for what each application exported. This narrows the search: splitting text into per-line and per-run paint units is shared across Adobe's text engines, so it is not specific to InDesign's compositor. What is specific to InDesign is painting a doubled stroke under each unit's own fill to emulate Outside alignment, which is where A and C come from. The behavior is unchanged between InDesign 2025 and 2026, so this is not a recent regression.

## M8. Text carrying a blend mode or a partial opacity is drawn inside a transparency group, which makes a second fill idempotent

**Claim.** InDesign draws text whose fill carries a blend mode, a partial opacity, a drop shadow, or a feather inside a transparency group, and everything drawn for that text, including a foreground adornment's own painting, lands inside the same group. The group is composited onto the page once, so painting the same opaque ink twice inside it cannot composite twice.

**Evidence** (*tests/indesign-stroke/N-transparency/*, one saved document exported with and without the plugin):

- In the exported PDF the adornment's added path fills appear inside the form XObjects */Fm0* to */Fm11*, one form per blended or partially opaque frame, rather than in the page stream.
- The mean luminance of the glyph core, taken 3 px inside its edge, is unchanged to two decimal places at 50 % text opacity and 50 % object opacity (166.95), Overlay (253.00), Darken (99.38), Lighten (255.00), overprint of fill, stroke, or both (172.46), a drop shadow (152.41), and a feather (172.02). Under Multiply it moves from 99.11 to 99.07, and under Screen from 247.78 to 247.13.
- The three controls the adornment must skip, a Center stroke, a run with no stroke, and an unstroked gradient fill, change by 0 pixels.
- A gradient fill is not emitted as a path fill. Over the whole document `f` rises from 2 to 38 and `sh` from 0 to 4, with `W` and `n` each rising by 4: the two gradient-filled frames at two lines apiece. Text-showing operators stay at 86.

**Status.** PROVEN for what was exported and measured. This removes the main predicted risk of a foreground fill repaint. What remains is not compositing but antialiasing: the repaint redraws the fill's outer edge, leaving a one-pixel ring whose visibility tracks the contrast between fill and stroke, largest where a dark fill meets a white stroke and near zero where a mid fill meets a mid stroke.

## Summary

| Defect | Mechanism | Layer | Status |
| --- | --- | --- | --- |
| A, adjacent-line overlap | M1 + M2: per-run stroke-then-fill, runs in story order | Text drawing (wax run drawing), upstream of every output backend | PROVEN |
| B, miter spikes | M5: miter rule at the text's miter-limit attribute | Attribute value carried to the port; the join rule is PDF and PostScript's own | PROVEN |
| C, run fragmentation | M1 + M2 at wax-run boundaries; boundaries follow attribute inequality | Text drawing, with run segmentation in composition | PROVEN |
| D, Create Outlines | M6: centered stroke at half the painted width, over the fill | Create Outlines conversion | PROVEN |

One implementation choice links A, C, and D: Outside text strokes are emulated per run by painting a doubled stroke under that run's fill (M1). A and C are that emulation meeting run and line boundaries. D is a conversion that has no path equivalent of "stroke under fill" and falls back to a centered stroke over it. B is a separate mechanism.

## Still open

- The distilled PDF path. A print to a PostScript file is measured in fixture N and carries the correction.
- Photoshop and Acrobat comparisons of the same geometry (InDesign 2025 and Illustrator are covered in M7).
- Glyph-level corner statistics for Minion Pro, to relate the spike reach to specific corners.
