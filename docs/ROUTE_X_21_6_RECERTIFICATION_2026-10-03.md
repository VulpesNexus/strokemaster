# Route X on InDesign 21.6, 2026-10-03

Route X (`1e7c417e…`) was certified on InDesign 21.5.1.73 ([cross-frame research](../CROSS_FRAME_RESEARCH_2026-09-26.md), [route W certification](../ROUTE_W_CERTIFICATION_2026-09-25.md)). The host is now 21.6.0.57. This pass recertifies route X on 21.6 against same-day controls, adds an extension census for the cases the research left untested, fixes the one route X gap that census found, validates the miter-spike model against InDesign's output, corrects the spikes in route W's stroke pass, and takes over plain page-item strokes.

## Verdict

**ROUTE_X `1e7c417e` on 21.6: CERTIFIED against same-day controls.** Session C3 is the candidate session C1 repeated job for job in a fresh 21.6 process with the installed binary (hash read from the loaded module):

- All 104 measured lines (victims, scope, cross-frame, transparency, color, round trips, saved documents, edit batteries, background exports) are identical to C1's.
- All six PNG outputs are byte-identical to C1's; PDF, EPS, and PostScript differ only in bytes that vary per export, with identical damage per frame.
- The cross-frame census passes 22 of 22 against same-day 21.6 native and route W sessions (N3, W3), per case identical to C1 ([*cross_frame_census_21.6.json*](../tests/audit/results/cross_frame_census_21.6.json)); PDF structure differs from route W only in clipping and graphics state.
- The edit identities (undo, redo, reopen, IDML) are exact, and the 13 documents the session saved carry no plugin name or ID prefix ([*C3-state.json*](../tests/audit/results/routex/C3-state.json)).

The native control on 21.6 still loses fill-core pixels wherever the census expects it to, so InDesign 21.6 has not fixed the overlap itself.

**ROUTE_X candidate `301fe06b` (the paints-nothing fix): CERTIFIED on 21.6 against same-day controls; superseded by `8c1d42aa`, which adds the miter correction below.** Session C4 repeats C3 in a fresh process with the candidate (65,536 bytes, `301fe06b6424a07609601811975d5f395fb3c6a620db56e20a8c4f5c6e9e66ba`, hash read from the loaded module), plus the extension census and the five saved suites:

- Its 103 measured lines are identical to C3's, and its six PNG outputs are byte-identical to C3's.
- The cross-frame census passes 22 of 22 against N3 and W3 ([*cross_frame_census_21.6_301fe06b.json*](../tests/audit/results/cross_frame_census_21.6_301fe06b.json)).
- The extension census passes 8 of 8 built cases against N4 and W4 ([*cross_frame_extension_census_301fe06b.json*](../tests/audit/results/cross_frame_extension_census_301fe06b.json)); XE6 goes from 11,206 damaged pixels natively to 0.
- The 13 documents it saved carry no plugin state ([*C4-state.json*](../tests/audit/results/routex/C4-state.json)).

The installer's backups keep the earlier builds: `1e7c417e` in *build/indesign-install/20261003T091703Z-backup* (also *build/Release-RouteX-1e7c417e*), and the research build in *20261003T094017Z-backup*.

Controls: research build `7b178414` (74,752 bytes, *build-plugin.ps1 -RouteX -Research*, the candidate's logic plus the control file), installed by an elevated install the owner approved, run in one process with *ISE_XF_CONTROL* switched between native (N3, N4), route W (W3, W4), and story (K5, K6) modes.

## Extension census

[*cert-X2.indd*](../tests/certification/corpus/cert-X2.indd), built by *tools/jsx/cert-build-xframe2.jsx*, holds one story per case in the cert-X geometry; the census backdrop is on the parent page, beneath every page item. Same-day verdicts ([*cross_frame_extension_census.json*](../tests/audit/results/cross_frame_extension_census.json): N4, W4, and K6, the fixed logic):

| Case | Construction | Native damage (HQP) | Fixed route X | Verdict |
| --- | --- | ---: | ---: | --- |
| XE0 | threaded pair on the parent page | > 0 | 0 | PASS |
| XE1 | path text threaded into a frame | > 0 | 0 | PASS |
| XE2 | threaded anchored frames | — | — | UNTESTED: 21.6 refuses to thread anchored frames by script, inline or custom |
| XE3 | threaded pair rotated 30° | > 0 | 0 | PASS |
| XE4 | three frames threaded in a chain | > 0 | 0 | PASS |
| XE5 | downstream frame scaled 125% in its matrix | > 0 | 0 | PASS |
| XE6 | invisible rectangle (no fill, no stroke) stacked between the frames | > 0 | 0 | PASS |
| XE7 | threaded pair in a nested group | > 0 | 0 | PASS |
| XE8 | the same rectangle with a 1 pt black border | > 0 | between 0 and route W | PASS (partial, like XB3) |

**XE6 and the fix.** The first build of XE6 set the rectangle's stroke to None and then its weight to 0, which turns a 1 pt black stroke back on (a recorded host trap). Route X correctly treated that bordered rectangle as content drawn in between and corrected only around it; it is kept as XE8. For a truly invisible item, route X `1e7c417e` still recorded the item's geometry as content when its shape began drawing, which stops the correction under invisible rectangles, empty frames, and wrap shapes. *plugin/source/IseCrossFrame.cpp* now skips a spline with no fill, no stroke, no children, no text column, and no transparency (`PaintsNothing`); its frame fill and stroke events already report nothing, and anything the check cannot establish keeps today's box. A traced story-mode run confirms it: the invisible rectangle's 27 shape events recorded no content box, and the bordered rectangle's were recorded as before.

Harness fixes found by this census: the story reference groups a story's frames on their own page (XE0 is not on the first document page) and skips path text before reading its layer; *manifest_frames* drops cases the host refused to build; *certify_routex.py summarize* takes the census as an option (a corrected case with no native session is `PASS_NO_CONTROL`, never `PASS`) and reproduces the committed 21.5.1 summary byte for byte.

## Preference handling

Preset exports in 21.6 leave the application's last-used PDF export destination profile changed, and in 21.6 the two PDF profiles are coupled (setting *pdfXProfile* to `USE_NO_PROFILE` resets the destination to `USE_DOCUMENT`; setting the destination to `WORKING` sets *pdfXProfile* to `WORKING`). *tools/jsx/ise-lib.jsx* now restores the closest reachable pair, via the color space set to unchanged and back, and reports `adjusted=[profiles: …]`; two tests model the measured coupling. The owner decided that the last-used PDF export settings need no restoring, since InDesign always saves the last export's settings and the owner exports with saved presets; every other preference is still gated, and stayed unchanged.

## Miter spikes: the model holds on InDesign's output

Route X leaves suite B's miter geometry untouched on 21.6: the export has the stroke widths (2–16 pt) and miter limits (1–500) of the native 21.5.1 export from before any plugin, and its pixels differ only at edges (306 pixels above 64 levels, at most 87, within route W's certified ring).

[*tools/measure_miter_envelope.py*](../tools/measure_miter_envelope.py) takes the stroke path route W wrote for each suite B line (the line's glyph outlines placed on the page, stroked at twice the Outside weight with InDesign's join and limit), renders that stroke alone with MuPDF, and compares it with two constructions of the frozen topology core: the one-sided outward offset of the filled outlines (the envelope, with miters only at convex exterior corners) and the symmetric stroke. Ink outside the envelope is a spike; symmetric minus envelope is the predicted spike. Record: [*miter_envelope_suite_b_2026-10-03.json*](../tests/geometry/miter_envelope_suite_b_2026-10-03.json); 6 unit tests.

| Suite B line | Spike ink | Predicted | Inside the prediction | Outside both | Glyphs |
| --- | ---: | ---: | ---: | ---: | --- |
| Outside 4 pt, limit 1, 1.5, 2 | 0–0.03 pt² | 0 | — | ≤ 0.03 pt² | — |
| Outside 4 pt, limit 3, 4, 10, 100 | 3.78 pt² | 4.05 pt² | 97–98% | ≤ 0.03 pt² | M, v, w, z |
| Outside 1 or 2 pt, limit 4 | 0 | 0 | — | 0 | — |
| Outside 6 pt, limit 4 | 27.7 pt² | 28.5 pt² | 98.6% | 0.07 pt² | M, v, w (and V, z at the fringe) |
| Outside 8 pt, limit 4 | 78.9 pt² | 81.8 pt² | 98.8% | 0.05 pt² | M, v, w, y |

InDesign's spikes are exactly the interior miter tips of its symmetric stroke that escape the outward offset, on the glyphs the September 30 embedded-font experiment named. The ink outside both constructions stays at the fringe level, so the envelope holds every legitimate exterior join: clipping route W's stroke to the envelope would remove the spikes and change nothing else. Limit 500 is outside the core's accepted range (1–100) and is reported as refused.

## Miter correction: route X candidate `8c1d42aa`

**ROUTE_W_MITER: CERTIFIED for suite B on 21.6, foreground and background PDF; installed now.** Route W strokes a mitered run's outline with bevel joins, then restrokes each exterior corner (a two-segment stub along the corner's own tangents, a fifth of a point at 4 pt) with the run's miter join and limit. A corner is exterior when a point just off it, on its join's side, lies outside the run's fill under the nonzero rule (*plugin/source/IseMiterCore.h*; 24 host-free checks in *tests/native-repaint/MiterCoreTests.cpp*: squares both ways, an L, a square with a hole, a thin chevron whose dropped miter tip is shown to leave the fill, overlapping glyphs, the closed two-anchor contour, collinear and degenerate points, curve tangents, smooth junctions). The renderer's own stroker builds every kept miter and applies the limit, so exterior joins match the native stroke; round- and bevel-joined runs are drawn exactly as before, byte for byte.

A/B in one process (research build `5eb40946`, story mode, *miter=0* then *miter=1*, sessions M0 and M1):

| Suite B line | Spike ink, miter=0 | Spike ink, miter=1 | Ink in the predicted spikes, miter=1 |
| --- | ---: | ---: | ---: |
| Outside 4 pt, limits 3–100 | 3.78 pt² | 0.04 pt² | 0 |
| Outside 6 pt, limit 4 | 27.7 pt² | 0.09 pt² | 0 |
| Outside 8 pt, limit 4 | 78.9 pt² | 0.14 pt² | 0 |
| Outside 1–2 pt, or limits 1–2 | ≤ 0.03 pt² | ≤ 0.05 pt² | 0 |

Every pixel that changed by more than 32 levels got darker: stroke ink removed where the spikes were (2,348 pixels above 64 levels in the 300 ppi PNG). Pixels that got lighter changed by at most 32 levels, along the stroke edge within a stub's length of a corner, where the stub's anti-aliased edge overlaps the bevel stroke's; route W's certified edge ring is up to 106 levels. The remaining 0.04–0.14 pt² outside the envelope is the same fringe. M0 and M1 measure identical fill-core damage on every output of the victims, cross-frame, and extension censuses and the five saved suites ([*M0.json*](../tests/audit/results/routex/M0.json), [*M1.json*](../tests/audit/results/routex/M1.json); corrected envelope record [*miter_envelope_suite_b_corrected_2026-10-03.json*](../tests/geometry/miter_envelope_suite_b_corrected_2026-10-03.json)).

Session C5 repeats C4 in a fresh process with the candidate (78,848 bytes, `8c1d42aa3354fde38c9410d28006d810a1cefbe6d0498ef8f4bc03df89593681`, hash read from the loaded module):

- Every fill-core damage count of its 110 measured lines equals C4's. The lines that differ are the comparisons against renders made under the previous plugin (saved documents against their origin, placed EPS against its direct export), which change by the miter-joined cases' edge pixels.
- Its PNGs equal C4's byte for byte except the two holding miter-joined text that has no spike: VC12, "thin benign miter strokes" (152 pixels, at most 19 levels), and E09, an anchored inline frame (35 pixels, at most 32 levels).
- The cross-frame census passes 22 of 22 and the extension census 8 of 8 built cases against same-day native and route W ([*cross_frame_census_21.6_8c1d42aa.json*](../tests/audit/results/cross_frame_census_21.6_8c1d42aa.json), [*cross_frame_extension_census_8c1d42aa.json*](../tests/audit/results/cross_frame_extension_census_8c1d42aa.json)); the 13 documents it saved carry no plugin state ([*C5-state.json*](../tests/audit/results/routex/C5-state.json)).
- Suite B in its foreground PDF and in a background PDF export (*Document.asynchronousExportFile*, the default *File > Export > Adobe PDF* route) has no ink in any predicted spike.

Not yet measured for the miter correction: EPS, PostScript, and screen drawing of suite B (they draw the same stroke operators), Center strokes (left to InDesign, spikes included), and real documents. Earlier builds are kept: `301fe06b` in *build/Release-RouteX-301fe06b* and the installer backup *20261003T102150Z-backup*, the research builds beside them.

## Center strokes: escaping tips only, route X candidate `8dbde64e`

The owner's look at the miter suite flagged the Center 8 pt line (spikes on v and w), and chose to remove only the escaping tips. A Center stroke lies over its own fill, so an interior miter there is visible and belongs; only its part outside the letter escapes.

Route W leaves a mitered Center run native and in InDesign's own drawing order. In its pass before Text it records the run's join in a second ledger (the same mute-and-recover rules as the stroke paint, *ThreadJoinLedger*) and sets it to bevel in the wax; InDesign then draws the run with bevel joins. In the Foreground pass, before the join is given back, it restrokes the run's corners with the native miter and limit: the exterior ones as they are, the interior ones clipped to the run's glyph outlines (nonzero), so an interior miter is kept inside the letter and only its escaping part is lost. Every stub is a short piece of the outline itself on each side of its corner (*CubicHead*, *CubicTail* in *IseMiterCore.h*): the first build's straight tangent stubs stuck out of the stroke where a letter curves tightly next to a corner, which a Center stroke shows and an Outside stroke hides under the fill. Tests: 39 miter-core checks (interior corners, curve pieces) and 12 join-ledger checks.

A/B in one process (research build `f0d94de3`, *miter=0* then *miter=1*, sessions M4 and M5), measured in MuPDF at 288 dpi and in InDesign's PNG:

| Suite B line | MuPDF: ink lost above 64 levels | Where | Ink gained above 32 levels |
| --- | --- | --- | --- |
| Center 8 pt, limit 4 | 65 px | the two predicted spikes (29 and 34 px), plus two single pixels | 305 px, all within two pixels of the stroke edge |
| Center 8 pt, limit 1 | 0 | — | 329 px, all within two pixels of the stroke edge |
| Outside lines | spikes only, as before | — | none |

The gained edge pixels are the path-against-text-stroke edge difference route W's certification already describes: the stubs are paths beside InDesign's text stroke, rasterized slightly apart. InDesign's own PNG shows the same class (up to 106 levels, within two pixels of an edge, both ways); nothing in it loses more than 128 levels. Inside the letters nothing changes.

**ROUTE_W_MITER for Center strokes: CERTIFIED_WITH_VARIANCE on 21.6 as `8dbde64e`; superseded by `48e27348`, which adds page-item strokes (below).** Session C6 repeats C5 with the candidate (84,480 bytes, `8dbde64e148883a57457081eb70640003c0a65f9ad976515d00fe714f6831c7f`, hash read from the loaded module):

- Every fill-core damage count of its 110 measured lines equals C5's; three comparisons against renders made under an earlier plugin moved by 2 to 9 pixels.
- Its PNGs equal C5's byte for byte except the scope fixture's (5 pixels, at most 7 levels: E09's mitered anchored frame, now with curve-following stubs).
- The cross-frame census passes 22 of 22 and the extension census 8 of 8 built cases against same-day native and route W ([*cross_frame_census_21.6_8dbde64e.json*](../tests/audit/results/cross_frame_census_21.6_8dbde64e.json), [*cross_frame_extension_census_8dbde64e.json*](../tests/audit/results/cross_frame_extension_census_8dbde64e.json)).
- Suite B in its foreground and background PDF has no ink in any predicted spike, Outside or Center; the Outside fringe outside the envelope fell to 0.01–0.09 pt² with the curve-following stubs. The Center lines lose ink only at the two spikes in both exports.
- The 13 documents it saved carry no plugin state ([*C6-state.json*](../tests/audit/results/routex/C6-state.json)).

## Page-item strokes: route X candidate `48e27348`

The owner chose to bring page-item strokes (shapes, frames, outlined text) under the plugin with the text rule, and, past the miter limit, to keep a corner's point cut flat at the limit distance instead of InDesign's bevel at the vertex (Q29). InDesign stroke-draws a page item after asking the draw-event dispatcher (*kBeginFrameStrokeDrawMessage*); route X's handler now draws the stroke itself at that moment, in the item's inner coordinates, and reports the event handled, so InDesign skips its own stroke for that item and drawing order is kept (*plugin/source/IsePageItemStroke.cpp*).

**What it takes.** Only strokes it can draw exactly: closed paths; a solid stroke type; a Miter join; Center or Outside alignment; no corner effects; no transparency on the item; no scaling (rotation and translation only); an outline whose nonzero and even-odd regions agree wherever a clip is needed (Outside, or Center with an interior corner); no stroke overprint when a corner needs a cut. Everything else, Inside strokes, open paths, dashes, stroke styles, and arrowheads included, stays InDesign's.

**What it draws** (*IseMiterCore.h*): the outline from *IPathPageItem::GetOutlineInfo* (corner effects included), stroked with bevel joins, then each exterior corner restroked with the miter as for text, and for Center each interior corner restroked under a clip to the item's fill. An exterior corner past the limit is filled as its miter cut at the limit distance (*CutMiter*; the join is defined by the tangents at the vertex, so the polygon is exact on curves too). For Outside, the stroke is drawn at twice the weight under a clip to outside the fill.

Three host facts shaped it, each found in the first A/B and measured in its PDF operators:

- **InDesign's PDF port rewrites clips.** It merged the cut clip of an Outside wedge with the outside clip, and dropped it from a Center wedge, whose tip then drew in full (P1: 16.44 to more than 30 pt). The cut is therefore filled, not clipped.
- **It also changes a clip's rule and frame.** An even-odd clip came out as `W` (nonzero), and its rectangular frame as `re`, with the port's own winding; the result was right only because suite B's contours happen to wind against `re`. The outside clip is now an octagon frame with each contour wound explicitly by nesting depth (*OutsideClip*), and probe points off every edge confirm that its nonzero reading is exactly the outside of the fill; where it is not, the item stays InDesign's.
- **For an Outside stroke, InDesign fills an offset of the outline, not the outline,** by half the weight, and strokes that; *IPathPageItem::Fill* fills the same offset. Its offset breaks on the outlined bottom line's overlapping and reversed contours: that is the line's "missing fill" on N, painted over by the offset stroke, and the gaps in A and N once the stroke was ours. The outline is now filled again in the item's paint before its stroke (identical pixels where the offset fill is whole; gradient fills are not refilled).

Core tests: 75 miter-core checks (the cut miter, reversal with curves, the outside clip under every winding of a holed square, opposite windings side by side, the same-direction hole it refuses).

A/B in one process (research build `43a9b310`, *miter=1*, *shapes=0* then *shapes=1*, sessions P4 and P5), suite B's wedges in MuPDF at 16 px/pt (apex height above the vertex, pt):

| Wedges, limit 4 | Native (P4) | Page-item strokes (P5) | Expected |
| --- | ---: | ---: | ---: |
| 10–28°, Outside 4 pt | 2.19–2.50, flat top | 16.00 | 16.00, cut at the limit |
| 10–28°, Center 8 pt | 0.38–1.00, flat top | 16.00 | 16.00, cut at the limit |
| 30–60°, both | 8.00–15.38 | 7.94–15.31 | 8.00–15.45, the full miter |

The outlined bottom line (Outside) draws whole: every letter filled, every stroke clean; the outlined line as converted (Center) lost 151 pixels and gained none, at its spikes. P4 and P5 measure identical fill-core damage on every output of the victims, scope, cross-frame, transparency, color, and extension censuses and the saved suites ([*P4.json*](../tests/audit/results/routex/P4.json), [*P5.json*](../tests/audit/results/routex/P5.json)); outside suite B, one pixel of one PNG changed. In suite B, only the wedges past the limit and the two outlined lines changed by more than 32 levels.

**ROUTE_X page-item strokes: CERTIFIED on 21.6 as `48e27348`; superseded by `787656a3`, which keeps whole points and fixes the inner miters of Center shapes (below).** Session C7 repeats C6 in a fresh process with the candidate (103,424 bytes, `48e27348e268609331bfd4dd52db1a76d209aa758c45c670c2f4fe8223702a72`, hash read from the loaded module):

- All 110 measured lines equal C6's.
- Its PNGs equal C6's byte for byte except the two holding bordered rectangles, cert-X and cert-X2: 5 to 11 corner pixels each, at most 29 levels (one pixel at 43), the bevel-and-stub drawing of a right-angle corner against InDesign's own miter.
- The cross-frame census passes 22 of 22 and the extension census 8 of 8 built cases against same-day native and route W ([*cross_frame_census_21.6_48e27348.json*](../tests/audit/results/cross_frame_census_21.6_48e27348.json), [*cross_frame_extension_census_48e27348.json*](../tests/audit/results/cross_frame_extension_census_48e27348.json)); the 13 documents it saved carry no plugin state ([*C7-state.json*](../tests/audit/results/routex/C7-state.json)).
- Suite B in its foreground and background PDF matches P5 (no pixel above 32 levels apart), its wedges as in the table, and no ink in any predicted text spike.

Not yet covered: Inside strokes, open paths and their caps, corner effects, scaled items, dashes and stroke styles, and outlines whose fill rules disagree; EPS, PostScript, and screen drawing of the wedges are unmeasured. Earlier builds stay in the installer's backups (`8dbde64e` in *20261003T160232Z-backup*).

## Whole points: route X candidate `787656a3`

The owner looked at `48e27348`'s suite B and asked for the triangles' points all the way, with no regression elsewhere. A point past the limit is now kept whole unless it would reach farther past its corner than the corner's shorter edge is long; then it is cut where that edge ends, and never closer than the limit distance (*ise_miter::PointReach*; *Stub::edge* is the shorter of the two segments meeting at the corner). A sharp corner between long edges is the shape's own point. One between short edges, such as a decorative font's notches or a near-reversal, would throw a point many times longer than the detail it comes from.

**Why not uncapped.** A first candidate, `d5214983`, kept every point whole up to InDesign's highest limit, 500 half widths. Its session C8 equals C7 on all 110 measured lines and on every PNG, byte for byte, and gives suite B's triangles their whole points: no page item outside suite B has a corner past its limit. The owner then suggested two fonts to test: German Beauty, which is pointy, and Kerberos Fang, which is extreme. Measured on their outlines (*tools/measure_font_corners.py*, 72 pt, Outside 4 pt):

| Font | Exterior corners past limit 4 | Uncapped point, 90% / max | Edge-capped point, max |
| --- | ---: | ---: | ---: |
| German Beauty | 0 (sharpest 1.9 half widths) | n/a | n/a |
| Kerberos Fang 1.000 (the copy InDesign loads) | 2,236 of 29,688 | 56 / 2,000 pt | 16 pt, the limit distance |
| Kerberos Fang 1.004 | 2,833 of 25,129 | 56 / 2,000 pt | 16 pt, the limit distance |

Every Kerberos Fang corner past the limit sits between edges shorter than its point (median 0.23 pt), so under the edge cap it keeps `48e27348`'s cut; at 300 pt, 2 of them reach past the limit distance, the longer by 27.3 pt. Uncapped, outlined Kerberos Fang would have grown 2,000 points longer than 16 pt.

**A defect found on the way, also in `48e27348`.** The font test adds thorns: a 10° spike on a 40 pt square, its sides 60, 30, or 10 pt long (*tools/jsx/build-B-font-points.jsx*, research builds in one process, *mode=native* then *mode=story miter=1 shapes=1*). On the Center thorns the plugin left black notches at the spike's base, where InDesign's stroke is white. The inner miters at the concave shoulder corners are restroked under a clip to the shape's fill, and *IPathPageItem::Clip* did not survive the PDF port. On the translated thorns, the port dropped the clip and the clipped stubs altogether. On suite B's outlined line, which has no transform, it wrote the clip as `q W n` with no path (C7: one such clip). That clip is now built from the recorded outline, as the Outside clip is; both fill rules agree on it, which eligibility already requires. C9 has no pathless clip, and its suite B renders identically to C7 apart from the triangles.

Thorns, apex height above the tip (pt, MuPDF at 16 px/pt; research build `72897365`, the candidate's logic):

| Thorn sides | Native | Corrected | Expected |
| --- | ---: | ---: | ---: |
| 60 pt, Outside 4 pt / Center 8 pt | 2.19 / 0.38 | 45.56 / 45.56 | 45.89, whole |
| 30 pt, both | 2.19 / 0.38 | 30.00 / 30.00 | 30.00, cut at the edge |
| 10 pt, both | 2.19 / 0.38 | 16.00 / 16.00 | 16.00, cut at the limit distance |

The thorn bases now match InDesign's drawing. In the same document, Kerberos Fang's outlines and both fonts' outlines set to Outside are refused, because their fill rules disagree, so InDesign keeps drawing them unchanged. German Beauty's outlined Center line differs from native by 33 and 34 edge pixels. Live text in both fonts gets the certified text correction, unchanged here: long spikes through letters removed, the font's own jagged exterior corners kept.

**ROUTE_X whole points: CERTIFIED on 21.6 as `787656a3`; superseded by `3298f50c`, which covers overlapping outlines (below).** Session C9 repeats C7 in a fresh process with the candidate (103,424 bytes, `787656a3da6dda9b794b0e10cac3fc7587d066adc6dd9fc534b8c5d8f1333390`, hash read from the loaded module):

- All 110 measured lines equal C7's, and every PNG equals C7's byte for byte ([*C9.json*](../tests/audit/results/routex/C9.json); C8's record is [*C8.json*](../tests/audit/results/routex/C8.json)).
- The cross-frame census passes 22 of 22 and the extension census 8 of 8 built cases ([*cross_frame_census_21.6_787656a3.json*](../tests/audit/results/cross_frame_census_21.6_787656a3.json), [*cross_frame_extension_census_787656a3.json*](../tests/audit/results/cross_frame_extension_census_787656a3.json)); the 13 documents it saved carry no plugin state ([*C9-state.json*](../tests/audit/results/routex/C9-state.json)).
- Suite B, foreground and background PDF, is identical to C8's (no pixel apart) and differs from C7's only at the triangles: 3,743 pixels gained stroke and 6 lost. Every triangle keeps its whole point: 45.56, 30.44, 22.94, 18.44, and 16.44 pt for 10–28° Outside at 16 px/pt, and 45.90, 30.65, 23.03, 18.48, and 16.53 pt for Center, read from the PDF's geometry because the 10° Center point touches the triangle above it.

Core tests: 80 miter-core checks (the point kept whole, cut at the edge, at the limit distance, and at the longest limit). Retired: `d5214983` and `f756b100` (an earlier build of the edge rule, before the clip fix) with their research builds `a6e363bd` and `cd9ea7d6`. Their sources are this commit's parent states, and the installer's backups (*20261004T055717Z*, *20261004T061919Z*, *20261004T062606Z*, *20261004T062825Z*) hold the binaries they replaced. `48e27348` is in *build/Release-RouteX-48e27348*.

## Overlapping outlines: route X candidate `3298f50c`

The owner confirmed `787656a3` and asked for outlined Kerberos Fang to be covered too: full coverage, so that nothing surprises the user. `787656a3` refused every outline whose nonzero and even-odd regions disagree wherever it needed a clip, which is every outline with overlapping contours or a hole wound the fill's way: Kerberos Fang's outlines, both test fonts' outlines set to Outside, and overlapping compound shapes. InDesign drew those unchanged; on outlined Kerberos Fang set to Outside, that is a solid white blob, its offset fill broken.

**What InDesign does.** It fills such an outline under the nonzero rule (`f`, never `f*`, also where contours overlap). So the Center case needs no change: the clip that keeps an interior miter inside the fill is a nonzero clip of the outline, which reads the fill exactly whatever the overlaps. Only the outside of the fill, which an Outside stroke is clipped to, cannot be written as a winding of the contours themselves.

**The region.** *IseRegionCore.h* resolves the outline into the region it fills: Clipper2's union under the nonzero rule (Boost Software License, vendored in *plugin/third_party/clipper2*, release 1.5.4), run on the flattened outline with every vertex carrying its place on the outline (a Z value), then rebuilt from the outline's own segments. Each run of the boundary along one segment becomes that segment's exact piece, curve and all; only at crossings does the boundary depend on the flattening, by at most its tolerance (0.0005 pt). Where a vertex's place came out wrong (Clipper2's callback put a crossing on German Beauty a tenth of a segment off), the vertex itself, projected onto its segment, decides; a run that still strays from Clipper2's polygon is drawn as that polygon. On one line of outlined Kerberos Fang at 48 pt (5,725 contours, 34,191 segments) the region takes 0.4 s, its outside clip 0.2 s, and on a 0.1 pt grid it matches the outline's own nonzero fill everywhere.

For an Outside stroke on such an outline, the region's boundary is what is stroked, and its corners get the joins. Stroking the contours inside the fill as well, then clipping, missed joins where two contours share a corner: each corner's probe landed in the other contour's fill, so both read as interior. As a result an Outside stroke goes around the filled shape only, where InDesign strokes every contour, its internal edges included.

**Joins are filled now.** The test also showed that a restroke along a corner, the way the plugin gave a corner its miter back, is fragile. InDesign's PDF port writes coordinates to a thousandth of a point, so a restroke piece a few thousandths long, beside a tiny segment, came out bent: Ghostscript beveled such a corner, PDFium notched it, and MuPDF drew the point. Each join is now one filled polygon, in the stroke's paint: the miter from the corner's exact tangents (or the whole or cut point past the limit, as before), together with the outer half of the stroke along both sides of the corner for up to one half width (*JoinPolygon*). That band covers the exact stroke beside the corner whatever a renderer approximates there: PDFium stopped short of the offset where a curve bends hard just past its corner. Text gets the same joins, within its limit, as before.

**Overprint.** *IRenderingObject::Install* does not carry overprint: the PDF port wrote the plugin's strokes with `/OP false` where InDesign's own have `/OP true`. Text in route W has drawn overprinting strokes without overprint since it began. Every stroke, join, and refill now sets the item's or run's overprint explicitly, and an overprinting page-item stroke is no longer refused. A refill in 100% black already overprinted, as InDesign's own does.

**Font and shape test** (*tools/jsx/build-B-font-points.jsx*, research build `d8ee50f2`, *mode=native* then *mode=story miter=1 shapes=1* in one process). New rows: both fonts' outlines set to Outside with no fill, overlapping squares and circles, holes wound both ways, and an overprinting shape and text. Pixel check of each Outside item at 8 px/pt (*tools/check_outside_stroke.py*: inside the fill more than 0.3 pt in, the fill or backdrop; outside it within the stroke weight, the stroke):

| Outside item | Inside, wrong of all | Band, missing (Ghostscript / PDFium / MuPDF) |
| --- | ---: | ---: |
| Squares, circles, same-direction hole, rectangular hole | 0 | 0 / 0 / 0 |
| German Beauty, outlined, filled or not | 0 of 264,427 | 48 / 111 / 17 of 303,369 |
| Kerberos Fang, outlined, filled or not | 437 of 294,847 (reference) | 0 / 9,266 / 7,154 of 452,637 |

InDesign's own drawing fails the same check on 293,193 inside pixels for Kerberos Fang and 33,195 for German Beauty, its offset fill broken. The remaining pixels are not the plugin's. Kerberos Fang's inside count is the reference's: it guesses the winding of the rectangles the PDF writes as `re`, and every flagged pixel lies in a hole the outline itself draws. Its band misses in PDFium and MuPDF are those renderers' strokes around contours far smaller than the stroke: MuPDF strokes a 1 pt speck at 8 pt as a ring with a hole, where Ghostscript draws the full disk, and InDesign's own Center stroke shows the same hole in MuPDF. German Beauty's are one-pixel seams where a renderer's flattened stroke meets the joins' band on a convex curve.

The cost is file size: every corner's join and the region's clip go into the PDF. The test page, with one line of outlined Kerberos Fang in each alignment, grows from InDesign's 4.3 MB to 12.4 MB.

**ROUTE_X overlapping outlines: CERTIFIED on 21.6; installed now as `3298f50c`.** Session C10 repeats C9 in a fresh process with the candidate (196,096 bytes, `3298f50c22d62c972afb0e9d6e36b7965a1a3ed5d7ef5c35b24da719b0a26fce`, hash read from the loaded module):

- All 110 measured lines keep C9's damage counts. Seven reference counts move by at most 111 pixels (*SAVED* 174 to 264 against the origin), and PNGs differ only at corners: at most 212 pixels each, by at most 117 levels, none by more than 128 ([*C10.json*](../tests/audit/results/routex/C10.json)).
- The cross-frame census passes 22 of 22 and the extension census 8 of 8 built cases ([*cross_frame_census_21.6_3298f50c.json*](../tests/audit/results/cross_frame_census_21.6_3298f50c.json), [*cross_frame_extension_census_3298f50c.json*](../tests/audit/results/cross_frame_extension_census_3298f50c.json)); the 13 documents it saved carry no plugin state ([*C10-state.json*](../tests/audit/results/routex/C10-state.json)).
- Suite B renders the same in foreground and background PDF. Against C9: the triangles keep their whole points to the hundredth; points within the limit come out at their exact length (30°: 15.38 pt, where C9 drew 15.31 and the miter is 15.45); text corners shift by less than a pixel; and three inner corners of the outlined Center line, which C9 beveled, are sharp again, as InDesign draws them.

Core tests: 118 checks (the region of overlapping squares, holes both ways, and overlapping circles rebuilt as curves; join polygons and their bands). Retired: five intermediate research builds of this work, `764d8779`, `07a85e7a`, `e1ee659a`, `e661d8ff`, and `3a07f283` (uncommitted states between the fixes above), with their installer backups. `787656a3` stays in the installer's backup *20261004T073206Z*.

## Extended coverage: route X candidate `410689fd`

The owner asked for every fix the coverage census pointed to (2026-10-04). They are:

- Exact fill-rule and outside-clip checks.
- Text in sheared and unevenly scaled frames stroked evenly in page space.
- Gradient refills, and no refill over a frame's content.
- Outside with every join and dash.
- Inside strokes.
- Object-level transparency, kept transforms, and corner effects.

Research build `9b924925` ran the extended census on 21.6, and an independent review read the code. Both found regressions: stroke-level and fill-level transparency lost, Inside spilling InDesign's offset fill on curves, and corner-effect pinches throwing blocks. Candidate `410689fd` (research twin `ebd06435`) leaves those cases to InDesign or bevels them; the [census follow-up](COVERAGE_CENSUS_2026-10-04.md#follow-up-extended-coverage-2026-10-04-to-10-05) has the measurements.

Core tests: 194 miter-core checks and 65,585 stroke-behind checks. The crossing-corner rule was checked by an offline simulation over every Outside outline of the census.

**ROUTE_X extended coverage: CERTIFIED on 21.6; superseded by `aca2a6c0`, which covers object-level transparency again (below).** Session C11 (2026-10-05) repeats C10 in a fresh process with the candidate (260,096 bytes, `410689fd7512efc49136d8757881da80fc4d14f66357d597f5bbddfcdaaa2e97`, hash read from the loaded module):

- All 110 measured lines keep C10's damage counts ([*C11.json*](../tests/audit/results/routex/C11.json)). The changes are all in sheared frames, where text is now stroked evenly in page space:
  - The scope document's PNG differs from C10's only inside E07, the frame sheared 20°: 13,747 pixels, where C10 drew the stroke skewed with the frame and C11 draws it round and even.
  - The edited scope document (ED2, whose edits shear E05 and rotate E07, so both are sheared and rotated) changes in those two frames only. Its two reference counts against the September route W session move from 14,420 to 37,066 pixels (PDF) and from 13,825 to 36,485 (PNG), the same frames' new stroke; its damage counts are unchanged.
  - The other six PNGs are byte-identical to C10's.
- Suite B and the four other saved suites render pixel-identical to C10 in foreground and background PDF (MuPDF, 300 dpi).
- The cross-frame census passes 22 of 22 and the extension census 8 of 8 built cases ([*cross_frame_census_21.6_410689fd.json*](../tests/audit/results/cross_frame_census_21.6_410689fd.json), [*cross_frame_extension_census_410689fd.json*](../tests/audit/results/cross_frame_extension_census_410689fd.json)); every measurement in them equals C10's, only the output files' hashes differ. The 13 documents the session saved carry no plugin state ([*C11-state.json*](../tests/audit/results/routex/C11-state.json)).

The extended census then ran on the research twin `ebd06435` (270,848 bytes), installed with the owner's approval for this run only ([census follow-up](COVERAGE_CENSUS_2026-10-04.md#host-run-of-the-candidate-2026-10-05), record [*coverage_census_3_21.6.json*](../tests/audit/results/coverage_census_3_21.6.json)). Every defect of the census build is gone, and every other row keeps the census build's gains. One gap remains against the design: items with object-level opacity or a drop shadow are left to InDesign too, as `3298f50c` left them, so nothing regresses, but the census build's gain on them is not kept. The owner chose to install `410689fd` and fix that in the next cycle, which the same day did (below).

Installer backups: `410689fd` in *build/indesign-install/20261005T092856Z-backup*, `ebd06435` in *20261005T094118Z-backup*, and `3298f50c` in *20261004T161417Z-backup*.

## Object-level transparency: route X candidate `aca2a6c0`

`410689fd` left every transparent page item to InDesign, where only a stroke's or a fill's own transparency was meant to be. A trace of each attribute read on the census, from research build `4418348c` ([*trace-xp-attributes.txt*](../tests/audit/results/routex/trace-xp-attributes.txt)), shows why. An item carries a stroke or fill transparency attribute only once that attribute is set. Reading one through the item's style descriptor (`IXPAttributeUtils::GetAttributeValue`) fails when it is absent, and the check counted a failed read as transparency. Of 507 reads, all 489 of absent attributes failed that way; the 18 attributes set on items (stroke opacity 50%, fill opacity 50%, the stroke's drop shadow) read correctly.

The check now asks the descriptor for each attribute itself (`QueryAttribute`). An absent attribute takes its hard-coded default, which is opaque, Normal, and no effect; a present one is read from the attribute; a read that still fails refuses, as before.

**Census on the research twin** (*build/look/coverage-census-4*, record [*coverage_census_4_21.6.json*](../tests/audit/results/coverage_census_4_21.6.json)): only the six rows with object-level opacity or the object's drop shadow change (16, 17, 18, 41, 42, and 63), each to the census build's numbers to the hundredth. Stray stroke through the letters and squares goes to 0 (342.6 pt² on squares, 143.8 on text, 398.4 on German Beauty), and the thorn's point comes back (points missing 101 → 2.4 with the shadow). Opacity and the shadow are kept. The rows that must stay InDesign's (stroke opacity, fill opacity, the stroke's own drop shadow: 60, 61, 62, 64, and 75) are unchanged, and so is the curved Inside text of row 63. The *xp=0* export now differs from the corrected one in those six rows only.

**ROUTE_X object-level transparency: CERTIFIED on 21.6; superseded by `bc1b850d`, which adds the corner-effect rule below.** Session C12 repeats C11 in a fresh process with the candidate (261,632 bytes, `aca2a6c03b6c969f16a89c6e5347b9ad28d429816ea208aa2b9f1c864410ca38`, hash read from the loaded module):

- All 110 measured lines are identical to C11's, and all seven PNGs are byte-identical ([*C12.json*](../tests/audit/results/routex/C12.json)). The corpus has no page item with object-level transparency, so nothing there was expected to change.
- Suite B and the four other saved suites render pixel-identical to C11 in foreground and background PDF.
- The cross-frame census passes 22 of 22 and the extension census 8 of 8 built cases ([*cross_frame_census_21.6_aca2a6c0.json*](../tests/audit/results/cross_frame_census_21.6_aca2a6c0.json), [*cross_frame_extension_census_aca2a6c0.json*](../tests/audit/results/cross_frame_extension_census_aca2a6c0.json)); the 13 documents it saved carry no plugin state ([*C12-state.json*](../tests/audit/results/routex/C12-state.json)).

Core tests unchanged: 194 miter-core, 43 cross-frame, and 65,585 stroke-behind checks. Installer backups: `410689fd` in *build/indesign-install/20261005T095858Z-backup*, and the research build `4418348c` in *20261005T101144Z-backup*.

## Corner-effect corners: route X candidate `bc1b850d`

On outlined text with fancy, inset, or inverse rounded corner effects, `aca2a6c0` drew long points: above and below the M, by the k and z, and at the thorn's tip. The owner asked for them to be narrowed down and removed. They came from two kinds of corners, both of which the corner effect adds, none of them on the path the user drew:

- **Past the miter limit:** the effect replaces each corner of the path with its own small shape, and on a glyph's acute corners (the M's apexes) that shape turns sharper than the limit. The whole-point rule drew each such corner out to at least the limit distance, 16 pt at Outside 4 pt, where InDesign bevels it. In the census, all 8 past-limit corners of the fancy row, 6 of the inset row, and 5 of the inverse rounded row are the effect's (*CornersNotAtAnchors*: no anchor of the item's own path within 0.001 pt).
- **Within the limit:** the fancy and inset effects cut corners into zigzags of segments 0.06 to 0.5 pt long, whose full miters reach 7 to 10 pt from the vertex. InDesign's own stroke draws these points too. The owner asked for them trimmed as well.

`bc1b850d` reads the item's own anchors (*IPathGeometry*) when the item has a corner effect. A corner off them keeps the stroke's bevel past the limit, as the crossing corners of a resolved region already do. Within the limit its miter reaches past the stroke's edge no farther than the corner's shorter edge (*EffectMiterReach*), so an effect's own square notch, with edges as long as the stroke is wide, keeps its whole miter. Items without corner effects, the triangles' whole points among them, are unchanged.

**Census on the research twin** `2b85d65a` (*build/look/coverage-census-6*, record [*coverage_census_5_21.6.json*](../tests/audit/results/coverage_census_5_21.6.json)): only the corner-effect rows change (12, 13, 14, 15, 55, 58, and 59). Escaping ink on their text, in pt²:

| Row | Native | `aca2a6c0` | `bc1b850d` |
| --- | ---: | ---: | ---: |
| 12, Outside, fancy corners | 19.9 | 33.8 | 1.6 |
| 13, Center, fancy corners | 18.0 | 18.0 | 0.5 |
| 14, Outside, inset corners | 4.6 | 19.6 | 3.0 |
| 15, Outside, rounded corners | 0.1 | 26.5 | 0.7 |
| 58, Outside, bevel corners | 0.9 | 5.9 | 0.1 |

The census counts points past the limit at the effect's corners as missing; those counts return to native's, as InDesign bevels them. An intermediate research build with the bevel rule alone (`a2d09a70`, *build/look/coverage-census-5*) removed the long points and fixed rows 15 and 58; the trim then removed the zigzag points of rows 12, 13, and 14.

**ROUTE_X corner-effect corners: CERTIFIED on 21.6; superseded by `e367f134`, which adds Inside on curves and open paths below.** Session C13 repeats C12 in a fresh process with the candidate (266,752 bytes, `bc1b850d7fd0ec50e2d683157f7005541d38128fb6e88175c612a83162aa3790`, hash read from the loaded module):

- All 110 measured lines are identical to C12's, and all seven PNGs are byte-identical ([*C13.json*](../tests/audit/results/routex/C13.json)). The corpus has no corner effects.
- Suite B and the four other saved suites render pixel-identical to C12 in foreground and background PDF, the triangles' whole points included.
- The cross-frame census passes 22 of 22 and the extension census 8 of 8 built cases ([*cross_frame_census_21.6_bc1b850d.json*](../tests/audit/results/cross_frame_census_21.6_bc1b850d.json), [*cross_frame_extension_census_bc1b850d.json*](../tests/audit/results/cross_frame_extension_census_bc1b850d.json)); the 13 documents it saved carry no plugin state ([*C13-state.json*](../tests/audit/results/routex/C13-state.json)).

Core tests: 213 miter-core checks (corners off the anchors, the effect's trimmed reach). Installer backups: `aca2a6c0` in *build/indesign-install/20261005T103911Z-backup*, `a2d09a70` in *20261005T110027Z-backup*, and `2b85d65a` in *20261005T110806Z-backup*.

## Inside on curves, open paths: route X candidate `e367f134`

The owner asked for the two remaining code items: Inside strokes on curved outlines, and open paths.

**Inside: the fill drawn at the fill event.** InDesign fills an Inside-stroked item's outline offset inward by half the weight, and that offset spills past curved outlines, corner effects, and thin parts. The census build showed that a stroke clipped to the fill cannot cover the spill, because InDesign's fill is drawn before the stroke event. `e367f134` claims the fill event too (*kBeginFrameFillDrawMessage*, *DrawFill*) for every item whose Inside stroke it takes. It fills the outline itself, gradients included, in the fill's place in the drawing order, beneath any content, and InDesign skips its offset fill. The stroke is then drawn at twice the weight, clipped to the outline, as for straight outlines before. Both events run the same analysis of the item (*Analyze*), so they always agree. Curves, corner effects, and frames with content are taken over now; stroke-level and fill-level transparency still refuse, since claiming either event drops InDesign's adornments.

With corner effects on overlapping contours, the effect's loops leave holes about 1.3 pt across in the filled region; stroked Inside, each became a white block about 9 pt wide. Such holes, smaller than the stroke's reach in both directions, are not stroked (*SmallHoles*); the fill keeps them as InDesign's does.

**Open paths: Center, Miter, solid, without arrowheads.** InDesign strokes an open path with caps at its ends and no closing segment, and bevels corners past the limit. The plugin draws the open path itself with the item's caps: every corner's miter as InDesign draws it within the limit, and past it the whole point of closed shapes (*PointReach*). An open path's corners are not told apart by the fill the path closes. Arrowheads (a claimed stroke draws none), dashes, and Outside or Inside on an open path stay InDesign's. InDesign keeps Outside on an open path and offsets the path to one side by its direction, which on outlined text puts the stroke inside many letters (census row 34).

**Census on the research twin** `03d0bd9a` (*build/look/coverage-census-8*, record [*coverage_census_6_21.6.json*](../tests/audit/results/coverage_census_6_21.6.json)); two rows appended: an open path with arrowheads (76), and the thorn opened at its base so its 10° tip stays a corner (77):

| Inside row, text | Escaping, native → `e367f134` | Holes |
| --- | ---: | ---: |
| 4 pt, miter | 614.2 → 0 | 166.0 → 0 |
| 8 pt, miter | 3,684.5 → 0 | 79.4 → 0 |
| 4 pt, round / bevel / limit 2 | 553.9 / 463.9 / 467.9 → 0 | 234.4 / 309.3 / 259.6 → 0 |
| 4 pt, thick-thin | 2,314.5 → 0 | 131.6 → 4.4 |
| 4 pt, fancy corners | 3,814.0 → 0 | 25.2 → 0 |
| 4 pt, opacity 50 / gradient fill / sheared 20 | 614.0 / 614.2 / 603.8 → 0 | 166.1 / 166.1 / 163.5 → 0 |
| German Beauty, miter / round | 84.8 / 78.8 → 0 | 6.9 / 45.9 → 3.6 / 3.9 |

Spill stays at the 55 pt² anti-aliasing baseline on every Inside text row. The opened thorn's point goes from 102.9 pt² missing to 3.0, and the arrowhead row is identical to native. Inside with fancy corners on the overlapping squares: holes 7.2 pt² against native's 10.8, stray stroke 157 → 5. Against `bc1b850d`, nothing else changes beyond anti-aliasing: the two appended rows make the page taller, which moves the PDF port's 0.001 pt rounding of the plugin's coordinates (up to 0.003 pt) while InDesign's own drawing stays put.

Measured and left as they are:

- The Inside-stroked filled text frame (row 103) spills 52 pt² more than native: one device pixel along the frame's top edge. The PDF port writes the stroke's clip as `16 145.001 368 50.001 re`, 0.002 pt past the frame, and MuPDF and Ghostscript rasterize clips without anti-aliasing, so that sliver claims a whole pixel. InDesign's own Inside stroke has no clip.
- Inside with fancy corners bevels the thorn's effect corners past the limit (points missing 6.0 against native's 0.5), by the corner-effect rule above.

**ROUTE_X Inside on curves and open paths: CERTIFIED on 21.6; installed now as `e367f134`.** Session C14 repeats C13 in a fresh process with the candidate (272,896 bytes, `e367f134fe34340e7b51073be554562decd8a18b227ddf23c7c119f07c02813e`, hash read from the loaded module):

- All 110 measured lines are identical to C13's, and all seven PNGs are byte-identical ([*C14.json*](../tests/audit/results/routex/C14.json)). The corpus has no Inside-stroked or open page items.
- Suite B and the four other saved suites render pixel-identical to C13 in foreground and background PDF.
- The cross-frame census passes 22 of 22 and the extension census 8 of 8 built cases ([*cross_frame_census_21.6_e367f134.json*](../tests/audit/results/cross_frame_census_21.6_e367f134.json), [*cross_frame_extension_census_e367f134.json*](../tests/audit/results/cross_frame_extension_census_e367f134.json)); the 13 documents it saved carry no plugin state ([*C14-state.json*](../tests/audit/results/routex/C14-state.json)).

Core tests: 216 miter-core checks (*SmallHoles*). Installer backups: `bc1b850d` in *build/indesign-install/20261005T114246Z-backup*, the first research build `53b91b17` (census 7, without the small-hole rule) in *20261005T115505Z-backup*, and `03d0bd9a` in *20261005T123315Z-backup*.

## Owner verification and decisions

- **Overlap: confirmed.** The owner checked *cert-X.indd* and a document of their own set in Times New Roman with the plugin installed; the overlap correction works on both. The InDesign overlap work is complete for this release scope (*OPEN_QUESTIONS.md* Q25), and [the usage guide](USING_STROKEMASTER.md#the-plugin) now describes the plugin.
- **The inner half of Center strokes: only behind a switch.** The half of a Center stroke painted over its own letter is sometimes wanted and sometimes not; hiding it would need an explicit switch (Q28).
- **Separate stories: a switch would be useful.** Captions, callouts, and split headlines are often separate unthreaded frames whose strokes cover a neighbor's letters by accident, but a halo over another frame is deliberate; a per-document switch, off by default, fits both (Q26). Until one is designed, separate stories stay as InDesign stacks them.
- **Miter suite findings.** The Center line's spikes are now corrected (above). The flat-topped triangles are page items, beveled by InDesign past the miter limit; the outlined bottom line is InDesign's own *Create Outlines* defect. The owner chose to bring page-item strokes under the plugin, with the same interior rule and, past the limit, a point cut at the limit distance instead of a bevel at the vertex (Q29).
- **Miter suite for the owner's look:** *build/look/miter-suite/* (ignored) holds a copy of suite B (*B-miter.indd*, to view in InDesign with the plugin) and its PDFs: before (session M0), after the Outside correction (C5), after the Center correction (C6), *B-miter - after (route X 48e27348, text and page items).pdf* (C7), *B-miter - after (route X 787656a3, whole points).pdf* (C9), and *B-miter - after (route X 3298f50c, overlapping outlines).pdf* (C10). *build/look/font-points/* (ignored) holds the font, thorn, and shape test: its document and its native and corrected PDFs (research build `d8ee50f2`).
- **Whole points (2026-10-04).** On `48e27348`'s suite B the owner asked for the triangles' points all the way, without regressions elsewhere, and suggested German Beauty and Kerberos Fang as pointy and extreme tests. Points are now kept whole up to the corner's shorter edge (above).
- **Whole points: confirmed; full coverage (2026-10-04).** The owner confirmed `787656a3` and asked for outlined Kerberos Fang to be covered, so that nothing surprises the user. Outlines with overlapping contours are now covered (above). An Outside stroke on them goes around the filled shape only, where InDesign also strokes the contour edges inside the letters. **Confirmed by the owner on 2026-10-05**, after a comparison of overlapping squares, circles, and outlined Kerberos Fang: keep the stroke around the filled shape only.
- **Corner-effect points: removed (2026-10-05).** The owner asked for the long points on corner-effect text to be narrowed down and removed, then for the shorter zigzag points InDesign also draws to be trimmed (above).
- **Center miter correction: confirmed.** The owner looked at the C6 files: the last character spikes are gone. The outlined bottom line was the remaining outlier, which page-item strokes now draw whole.

## Next

1. The owner's look at overlapping outlines: the font, thorn, and shape test (outlined Kerberos Fang and German Beauty, filled and not; overlapping shapes), and shapes in their own documents.
2. Done: open paths (Center, Miter) and Inside on curved outlines (`e367f134`, above).
3. EPS, PostScript, and screen checks of the miter correction, text and page items; a font sweep beyond Minion, German Beauty, and Kerberos Fang (sans, script, a variable font) at large Outside weights.
4. XE2 by hand: whether InDesign's interface threads anchored frames at all.
