# Route X on InDesign 21.6, 2026-10-03

Route X (`1e7c417e…`) was certified on InDesign 21.5.1.73 ([cross-frame research](../CROSS_FRAME_RESEARCH_2026-09-26.md), [route W certification](../ROUTE_W_CERTIFICATION_2026-09-25.md)). The host is now 21.6.0.57. This pass recertifies route X on 21.6 against same-day controls, adds an extension census for the cases the research left untested, fixes the one route X gap that census found, and validates the miter-spike model against InDesign's output.

## Verdict

**ROUTE_X `1e7c417e` on 21.6: CERTIFIED against same-day controls.** Session C3 is the candidate session C1 repeated job for job in a fresh 21.6 process with the installed binary (hash read from the loaded module):

- All 104 measured lines (victims, scope, cross-frame, transparency, color, round trips, saved documents, edit batteries, background exports) are identical to C1's.
- All six PNG outputs are byte-identical to C1's; PDF, EPS, and PostScript differ only in bytes that vary per export, with identical damage per frame.
- The cross-frame census passes 22 of 22 against same-day 21.6 native and route W sessions (N3, W3), per case identical to C1 ([*cross_frame_census_21.6.json*](../tests/audit/results/cross_frame_census_21.6.json)); PDF structure differs from route W only in clipping and graphics state.
- The edit identities (undo, redo, reopen, IDML) are exact, and the 13 documents the session saved carry no plugin name or ID prefix ([*C3-state.json*](../tests/audit/results/routex/C3-state.json)).

The native control on 21.6 still loses fill-core pixels wherever the census expects it to, so InDesign 21.6 has not fixed the overlap itself.

**ROUTE_X candidate `301fe06b` (the paints-nothing fix): CERTIFIED on 21.6 against same-day controls; installed now.** Session C4 repeats C3 in a fresh process with the candidate (65,536 bytes, `301fe06b6424a07609601811975d5f395fb3c6a620db56e20a8c4f5c6e9e66ba`, hash read from the loaded module), plus the extension census and the five saved suites:

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

## Next

1. Miter correction in route W's stroke pass: clip each run's stroke to its envelope (or fill the envelope), with the core linked into the plugin; certify against suite B and every row of this report.
2. The owner's view of *cert-X.indd* at 400%, and a real multi-page document.
3. XE2 by hand: whether InDesign's interface threads anchored frames at all.
