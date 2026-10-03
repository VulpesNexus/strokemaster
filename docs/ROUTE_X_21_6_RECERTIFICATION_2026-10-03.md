# Route X on InDesign 21.6, 2026-10-03

Route X (`1e7c417e…`) was certified on InDesign 21.5.1.73 ([cross-frame research](../CROSS_FRAME_RESEARCH_2026-09-26.md), [route W certification](../ROUTE_W_CERTIFICATION_2026-09-25.md)). The host is now 21.6.0.57. This pass repeats the route X candidate session on 21.6, adds an extension census for the cases the research left untested, and records a route X defect that census found.

## Verdict

**ROUTE_X on 21.6: CERTIFIED against the 21.5.1 references; same-day native control pending.** Session C3 is the candidate session C1 repeated job for job in a fresh 21.6 process with the installed `1e7c417e` binary (hash read from the loaded module):

- All 104 measured lines (victims, scope, cross-frame, transparency, color, round trips, saved documents, edit batteries, background exports) are identical to C1's.
- All six PNG outputs are byte-identical to C1's; PDF, EPS, and PostScript differ only in bytes that vary per export, with identical damage per frame.
- The cross-frame census passes 22 of 22 against the 21.5.1 native and route W references, per case identical to C1 ([*cross_frame_census_21.6.json*](../tests/audit/results/cross_frame_census_21.6.json)); PDF structure differs from route W only in clipping and graphics state.
- The edit identities (undo, redo, reopen, IDML) are exact, and the 13 documents the session saved carry no plugin name or ID prefix ([*C3-state.json*](../tests/audit/results/routex/C3-state.json)).

The independent-story cases still show native damage under route X (for example 41,495 pixels in the cross-frame census's High Quality Print PDF), so InDesign 21.6 has not fixed the overlap itself. A same-day native control and route W baseline need the research build installed, which needs an elevated install.

## Preference incident and fix

Preset exports in 21.6 left the application's PDF export destination profile at `USE_DOCUMENT`; the shared restore could not put back the saved `WORKING`. In 21.6 the two PDF profiles are coupled: setting *pdfXProfile* to `USE_NO_PROFILE` resets the destination to `USE_DOCUMENT`, and setting the destination to `WORKING` sets *pdfXProfile* to `WORKING`. No assignment order, combined assignment, preset, or temporary PDF/X standard reached the starting pair by script. Setting the color space to unchanged and back makes InDesign pick the working profile by name: *sRGB IEC61966-2.1* with no PDF/X profile, the same effective profiles as the start, stored by name rather than as the working space. That is the state left in the owner's preferences; *File > Export > Adobe PDF*, *Output*, *Destination: Working RGB* puts back the original choice.

*tools/jsx/ise-lib.jsx* now tries the orders that can reach a saved pair after its normal passes and reports which one worked (`adjusted=[profiles: …]`), or that none did, in which case the difference is still reported. Two tests in *tests/scripts/safety.test.cjs* model the measured coupling; both fail against the previous restore. Every later job of this pass restored with `fails=0 diff=0`.

## Extension census

[*cert-X2.indd*](../tests/certification/corpus/cert-X2.indd), built by *tools/jsx/cert-build-xframe2.jsx*, holds one story per case in the cert-X geometry; the census backdrop is on the parent page, beneath every page item. Results under route X `1e7c417e` ([*cross_frame_extension_census.json*](../tests/audit/results/cross_frame_extension_census.json), session C3Z): a corrected case with no native session is `PASS_NO_CONTROL`, never `PASS`.

| Case | Construction | Route X damage, six outputs | Verdict |
| --- | --- | --- | --- |
| XE0 | threaded pair on the parent page | 0 | PASS_NO_CONTROL |
| XE1 | path text threaded into a frame | 0 | PASS_NO_CONTROL |
| XE2 | threaded anchored frames | — | UNTESTED: 21.6 refuses to thread anchored frames by script, inline or custom |
| XE3 | threaded pair rotated 30° | 0 | PASS_NO_CONTROL |
| XE4 | three frames threaded in a chain | 0 | PASS_NO_CONTROL |
| XE5 | downstream frame scaled 125% in its matrix | 0 | PASS_NO_CONTROL |
| XE6 | invisible rectangle stacked between the frames | 3,282–3,322 | FAILED |
| XE7 | threaded pair in a nested group | 0 | PASS_NO_CONTROL |

**XE6 is a route X defect.** When any page item starts drawing between two frames of a story, route X records its geometry as content that may cover a fill and stops correcting the glyphs under it. A spline with no fill, no stroke, no children, no text column, and no transparency paints nothing; its frame fill and stroke events already report nothing. Invisible rectangles, empty frames, and wrap shapes are common in real layouts. *plugin/source/IseCrossFrame.cpp* now skips such an item (`PaintsNothing`); anything it cannot establish keeps today's cautious box, because a missed cover would let content show through a stroke instead. Built, not installed: route X candidate `301fe06b6424a07609601811975d5f395fb3c6a620db56e20a8c4f5c6e9e66ba` (65,536 bytes, *build/Release-RouteX*) and research build `7b178414315b3716c5ac1fe2634b92ea4e344aea6b00402ebca741c35e6ef1af` (74,752 bytes). The certified `1e7c417e` build is kept in *build/Release-RouteX-1e7c417e*.

Harness fixes found by this census: the story reference groups a story's frames on their own page (XE0 is not on the first document page) and skips path text before reading its layer; *manifest_frames* drops cases the host refused to build. The generalized *certify_routex.py summarize* reproduces the committed 21.5.1 summary byte for byte.

## Miter spikes: first host measurement

Suite B exported from 21.6 with route X installed has the stroke widths (2–16 pt) and miter limits (1–500) of the native 21.5.1 export from before any plugin. Its pixels differ only at edges (306 pixels above 64 levels, at most 87, within route W's certified ring of up to 106), because route W draws the stroke as a path. Route X therefore leaves miter geometry untouched, the spikes persist in 21.6, and route W's stroke pass is where a miter correction can act: a clip or a region fill limited to the one-sided outward offset of the fill, per the [miter contract](MITER_GEOMETRY_CONTRACT.md).

## Next

1. One elevated install of the research build `7b178414` (InDesign closed), then one process switching *ISE_XF_CONTROL* modes: native, route W, and story (the fixed logic) on the victims, cert-X, and cert-X2. That is the same-day 21.6 control for both censuses and the first host run of the fix.
2. An elevated install of the candidate `301fe06b`, then session C4 repeating C3 and the extension census: every C3 row unchanged, XE6 corrected.
3. The owner's view of *cert-X.indd* at 400%, and a real multi-page document.
