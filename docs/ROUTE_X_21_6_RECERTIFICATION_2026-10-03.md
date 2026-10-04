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

**ROUTE_X whole points: CERTIFIED on 21.6; installed now as `787656a3`.** Session C9 repeats C7 in a fresh process with the candidate (103,424 bytes, `787656a3da6dda9b794b0e10cac3fc7587d066adc6dd9fc534b8c5d8f1333390`, hash read from the loaded module):

- All 110 measured lines equal C7's, and every PNG equals C7's byte for byte ([*C9.json*](../tests/audit/results/routex/C9.json); C8's record is [*C8.json*](../tests/audit/results/routex/C8.json)).
- The cross-frame census passes 22 of 22 and the extension census 8 of 8 built cases ([*cross_frame_census_21.6_787656a3.json*](../tests/audit/results/cross_frame_census_21.6_787656a3.json), [*cross_frame_extension_census_787656a3.json*](../tests/audit/results/cross_frame_extension_census_787656a3.json)); the 13 documents it saved carry no plugin state ([*C9-state.json*](../tests/audit/results/routex/C9-state.json)).
- Suite B, foreground and background PDF, is identical to C8's (no pixel apart) and differs from C7's only at the triangles: 3,743 pixels gained stroke and 6 lost. Every triangle keeps its whole point: 45.56, 30.44, 22.94, 18.44, and 16.44 pt for 10–28° Outside at 16 px/pt, and 45.90, 30.65, 23.03, 18.48, and 16.53 pt for Center, read from the PDF's geometry because the 10° Center point touches the triangle above it.

Core tests: 80 miter-core checks (the point kept whole, cut at the edge, at the limit distance, and at the longest limit). Retired: `d5214983` and `f756b100` (an earlier build of the edge rule, before the clip fix) with their research builds `a6e363bd` and `cd9ea7d6`. Their sources are this commit's parent states, and the installer's backups (*20261004T055717Z*, *20261004T061919Z*, *20261004T062606Z*, *20261004T062825Z*) hold the binaries they replaced. `48e27348` is in *build/Release-RouteX-48e27348*.

## Owner verification and decisions

- **Overlap: confirmed.** The owner checked *cert-X.indd* and a document of their own set in Times New Roman with the plugin installed; the overlap correction works on both. The InDesign overlap work is complete for this release scope (*OPEN_QUESTIONS.md* Q25), and [the usage guide](USING_STROKEMASTER.md#the-plugin) now describes the plugin.
- **The inner half of Center strokes: only behind a switch.** The half of a Center stroke painted over its own letter is sometimes wanted and sometimes not; hiding it would need an explicit switch (Q28).
- **Separate stories: a switch would be useful.** Captions, callouts, and split headlines are often separate unthreaded frames whose strokes cover a neighbor's letters by accident, but a halo over another frame is deliberate; a per-document switch, off by default, fits both (Q26). Until one is designed, separate stories stay as InDesign stacks them.
- **Miter suite findings.** The Center line's spikes are now corrected (above). The flat-topped triangles are page items, beveled by InDesign past the miter limit; the outlined bottom line is InDesign's own *Create Outlines* defect. The owner chose to bring page-item strokes under the plugin, with the same interior rule and, past the limit, a point cut at the limit distance instead of a bevel at the vertex (Q29).
- **Miter suite for the owner's look:** *build/look/miter-suite/* (ignored) holds a copy of suite B (*B-miter.indd*, to view in InDesign with the plugin) and its PDFs: before (session M0), after the Outside correction (C5), after the Center correction (C6), *B-miter - after (route X 48e27348, text and page items).pdf* (C7), and *B-miter - after (route X 787656a3, whole points).pdf* (C9). *build/look/font-points/* (ignored) holds the font and thorn test: its document and its native and corrected PDFs.
- **Whole points (2026-10-04).** On `48e27348`'s suite B the owner asked for the triangles' points all the way, without regressions elsewhere, and suggested German Beauty and Kerberos Fang as pointy and extreme tests. Points are now kept whole up to the corner's shorter edge (above).
- **Center miter correction: confirmed.** The owner looked at the C6 files: the last character spikes are gone. The outlined bottom line was the remaining outlier, which page-item strokes now draw whole.

## Next

1. The owner's look at the whole points: suite B's triangles, the font and thorn test, and shapes in their own documents.
2. Page-item coverage: Inside strokes, open paths and caps, corner effects, scaled items, and outlines whose fill rules disagree (outlined Kerberos Fang, and both test fonts' outlines set to Outside, are refused for that today).
3. EPS, PostScript, and screen checks of the miter correction, text and page items; a font sweep beyond Minion, German Beauty, and Kerberos Fang (sans, script, a variable font) at large Outside weights.
4. XE2 by hand: whether InDesign's interface threads anchored frames at all.
