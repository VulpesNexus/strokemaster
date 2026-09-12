# InDesign stroke investigation

The per-defect report of the InDesign stroke engine investigation, for InDesign 2026 (21.5.1.73) on Windows 11. The supporting documents are *STROKE_FIXTURE_CENSUS.md* (Phase A), *INDESIGN_STROKE_SDK_MAP.md* (Phase D), and *STROKE_MECHANISMS.md* (Phase E). Reproducers, measurements, and expected outputs are under *tests/indesign-stroke/*.

This report reflects the state after phases A through G. Fixture D has a Phase F script prototype, and the native plugin for fixtures A and C is built, installed into InDesign 2026, and measured: interception candidate I1 as a global text adornment, described under fixture A. Its results and its costs are recorded there rather than summarized here.

Fixture N (*tests/indesign-stroke/N-transparency/*) is not a defect reproducer. It asks the opposite question, whether the plugin harms output that carries a blend mode, a partial opacity, a gradient, or an overprint, by rendering one saved document twice under two renderers. Its results are folded into fixture A's risks below.

## Summary

| Fixture | Root cause | Layer | Verdict |
| --- | --- | --- | --- |
| A, adjacent-line overlap | Outside strokes are painted per wax run as a doubled stroke under that run's own fill, so a later line's stroke covers an earlier line's fill | Text drawing, upstream of screen, PDF, PostScript, and raster output | ROOT CAUSE PROVEN, SDK FIX IMPLEMENTED AND VERIFIED within a frame; cross-frame overlap remains |
| B, miter spikes | Standard miter joins at the text's miter-limit attribute (default 4) | Attribute value passed to the graphics port; the join rule is PDF and PostScript's own | ROOT CAUSE PROVEN, FIX NOT YET IMPLEMENTED |
| C, run fragmentation | The same per-run painting as A, at run boundaries; runs split on any drawing-attribute inequality, visible or not | Text drawing, with run segmentation in composition | ROOT CAUSE PROVEN, SDK FIX IMPLEMENTED AND VERIFIED by the same plugin, unchanged |
| D, Create Outlines | The conversion writes a centered stroke at half the painted width, over the fill | Create Outlines conversion | AUTOMATIC CORRECTION PROTOTYPED |

A and C have one root cause. A fix that paints a frame's Outside strokes before that frame's fills removes both, and the expected outputs in *tests/indesign-stroke/A-adjacent-line-overlap/expected* and *…/C-run-fragmentation/expected* show exactly that result.

Two comparisons place the behavior (*STROKE_MECHANISMS.md*, M7). InDesign 2025 exports the same fixture with an identical paint structure, so none of this is a recent regression. Illustrator splits stroked text into the same per-line and per-run units, but paints each unit's stroke over its fill at the nominal weight, and has no Outside alignment for text. The shared part is the unit structure; what is InDesign's own is emulating Outside alignment with a doubled stroke under each unit's fill.

## Fixture A: adjacent-line stroke overlap

### Symptom

With an Outside text stroke wide enough to reach the neighboring line, the next line's stroke covers the previous line's descenders. In the fixture (Minion Pro 12 pt, auto leading, 7 pt stroke), 4,562 fill pixels at 600 dpi are covered. The minimal reproducer (24 pt, 8 pt stroke) covers 1,536 pixels at 720 dpi.

### Root cause

For Outside alignment, each wax run is painted as a centered stroke of twice the weight followed by the same run's fill. Runs are painted in story order within a frame, and separate frames are painted in stacking order. The stroke-under-fill guarantee therefore holds only inside one run (*STROKE_MECHANISMS.md*, M1–M3). Controls: moving the strokes before the fills removes every covered pixel while nothing else changes; A06 against A07 changes only stacking order and flips which frame is damaged; line breaks of every kind behave the same; weights of 3 and 5 pt and leading of 60 pt cover nothing.

### Layer

Text drawing, in the private wax run text implementation. The order is identical in PDF export, PostScript export, InDesign's own raster export, and after IDML round-trip. Composition geometry is correct.

### Existing Adobe capability

None found that changes the order. No text, paragraph, story, or frame attribute controls it, and none appears in scripting or IDML.

### Workaround

Keep an Outside stroke narrower than the gap between lines (A09a and A09b), or increase leading. For a stroke that must stay wide: a stroke-only copy of the story placed behind a fill-only copy. *tools/jsx/ise-split-stroked-text.jsx* builds that pair from a selection, per text style range, and it is measured in *tests/indesign-stroke/S-split/NOTES.md*. With the plugin removed, it takes 1,010, 2,763, and 3,260 covered pixels to zero, and the split frames then render identically whether the plugin is loaded or not, so the correction is carried by the document and needs nothing installed. The cost is that the story then lives in two frames, and InDesign has no native way to keep them in sync.

### Script-level fix

None that keeps a single live story, and that limit is now measured rather than assumed. *tools/jsx/ise-split-stroked-text.jsx* builds the shadow pair above and corrects the appearance completely, but the two frames hold two stories from then on. InDesign's linked stories synchronize the text and then overwrite the child's formatting with the parent's, paragraph styles included, and no style-mapping table is reachable from scripting to prevent it (*tools/jsx/probe-linked-stories.jsx* and *tools/jsx/probe-style-mapping.jsx*). So a script can build the pair, and nothing native can keep it live. A script can, and *tools/jsx/ise-live-sync.jsx* does: it rebuilds the bottom frame from the top, which survives an arbitrary edit where re-applying settings by character offset would not. Measured, a resync carried the edited text across while leaving the stroke at weight 8, Paper, Outside, and Round, and an idle task at 700 ms noticed an edit and resynced the pair within ten seconds without being asked (*tests/indesign-stroke/S-split/NOTES.md*). Polling is the only trigger available, because no InDesign event reports a text edit.

### SDK/native-plugin fix

Two designs, both public-API based (*INDESIGN_STROKE_SDK_MAP.md*, I1 and I2):

- **I1.** A global text adornment in the foreground pass repaints each stroked run's fill after the whole frame's text. Fills painted from glyph outlines avoid duplicate searchable text in PDF.
- **I2.** A global text adornment in the background pass paints every run's stroke first, and the native stroke is suppressed while each run draws its text.

**I1 is built, installed, and measured** (*plugin/*, boss `kIseFillRepaintAdornmentBoss`). It registers through `kGlobalTextAdornmentService`, is called for every wax run in every frame, and repaints the fill of each run whose stroke is aligned Outside, drawing the run's glyph outlines through `IGraphicsPort::fill` with the run's own rendering object installed. I2 remains unbuilt.

### Engine-level fix

Inside InDesign's run text drawing, paint all of a frame's Outside strokes in one pass before its text pass. That is the ordering the expected output demonstrates.

### Risks

Three of these are no longer predictions. Measured on *A-minimal*, comparing the same reproducers before and after the plugin:

- **Fills are painted twice, and the second pass darkens antialiased glyph edges.** Measured, not estimated: the fill core grows by a nearly constant amount per document, 62,497 to 64,347 px on A01 to A03, 56,442 to 57,979 px on A10, and 61,368 to 63,300 px on A11, while the fill area itself grows by 4,118 px on every B1 entry. The amount tracks glyph perimeter rather than stroke weight, so it is a one-pixel ring along the fill boundary. It is invisible at reading size, but it is a real change to output and it is the price of this design.
- **Every Outside-stroked run in the document is repainted, not only damaged ones.** Fixture B, an unrelated defect, shifts measurably as a result: `white_px` falls by about 1,490 on each entry and `spike_px_beyond_weight` by 2 to 10 %. The miter geometry is untouched and the spikes remain, 8,109 px beyond weight against 8,299 before, but the control did not hold cleanly. That shift is the edge ring alone and nothing else: B's entries are single-run lines, and re-measuring them after the run-placement defect below was fixed reproduced all thirteen entries to the pixel.
- **Cross-frame overlap is not corrected, by construction.** An adornment is scoped to one parcel, so A06, A07, and A08 keep their damage. Their measured hidden fill rises slightly, 2,126 to 2,163 and 1,536 to 1,657, because the enlarged fill core intersects the unchanged stroke of the frame in front. Changing the order across frames would change page-item stacking and must not be done.
- **Transparency, blend modes, gradients, and overprint do not composite twice.** This was the largest outstanding prediction against I1, and it is now measured rather than assumed (fixture N). InDesign draws text carrying a blend mode or a partial opacity inside a transparency group, and the repaint lands inside that same group, so the group is still composited onto the page once and painting opaque ink twice inside it is idempotent (*STROKE_MECHANISMS.md*, M8). The mean luminance of the glyph core, taken 3 px inside its edge so the repaint's own ring is excluded, is unchanged to two decimal places under 50 % text opacity and 50 % object opacity (166.95), Overlay (253.00), Darken (99.38), Lighten (255.00), overprint of fill, stroke, or both (172.46), a drop shadow (152.41), and a feather (172.02); under Multiply it moves from 99.11 to 99.07. Gradient-filled runs are repainted too, emitted as a clip and a shading rather than a path fill. A11, at 90 % frame text opacity, corrects fully.
- **Runtime cost is small on these documents, and screen redraw is still not timed directly.** Seven repetitions each, re-exporting the corpus document: PDF export 1,171 ms without the plugin against 1,181 ms with it, and 300 ppi raster export 1,093 ms against 1,120 ms, about 0.9 % and 2.5 %. Raster export drives the same drawing code as the screen and is the closest available proxy, but it is a proxy.
- I2 depends on the undocumented timing of port use inside the run draw.

### Verification

*A-minimal.probe.md* (12 entries: hidden fill as exported and with strokes first), *A-minimal-expected.pdf*, and the preserved pre-plugin run in *actual-before-plugin/*, measured under the same library versions so the comparison has no confound.

| Entry | Before | After |
| --- | --- | --- |
| A01 two paragraphs, A02 forced break, A03 soft wrap | 1,536 px | **0** |
| A10 line split into three runs | 3,244 px | **0** |
| A11 frame text opacity 90 % | 1,506 px | **0** |
| A04 Center alignment (must not change) | 62,497 px | 62,497 px |
| A05, A09a, A09b (must stay clean) | 0 | 0 |
| A06, A07, A08 cross-frame pairs | 2,126 / 1,536 px | 2,163 / 1,657 px |

The fixture itself is corrected too, which matters more than the reproducers. Its exports before and after the plugin are measured against one shared manifest, which is legitimate because the two files hold the same text at the same positions: 175 words, and no word displaced by as much as a hundredth of a point. Counting covered pixels against the unchanged pre-plugin fill core, so that the repaint cannot flatter itself by enlarging what is measured:

| Fixture frame | Before | After |
| --- | --- | --- |
| Three lines, 27 style ranges | 8,758 px | **2** |
| Three lines, 2 runs per line | 6,557 px | **8** |
| Two lines, single run | 2 px | **0** |
| Text already converted to outlines | 110,647 px | 110,647 px |

The last row is fixture D's defect, not this one: its damage is baked into converted paths, and its strokes-first reference equals its hidden fill exactly, so no change of paint order can reach it. The adornment correctly leaves it alone.

Live text survives intact: 346 characters, 92 words, and 48 text-showing operators before and after, with extracted text identical, while path-fill operators rise from 1 to 23. The correction reaches the display path as well as export: the 300 ppi raster differs by 36,713 pixels, where an inert build differed by none. Fixture C behaves the same way, 1 path fill becoming 106 across 50 variants with its 215 text-showing operators unchanged.

### Verdict

ROOT CAUSE PROVEN. SDK FIX IMPLEMENTED AND VERIFIED FOR OVERLAP WITHIN A FRAME; cross-frame overlap remains, and the doubled fill darkens glyph edges by a one-pixel ring.

## Fixture B: miter-join spikes

### Symptom

Pointed spikes project from glyph corners on text with a Miter join, growing with the stroke weight: 1.64, 3.36, 6.8, 11.81, and 16.7 pt of reach at weights 1, 2, 4, 6, and 8 pt (limit 4).

### Root cause

These are standard miter joins. A join is mitered when 1 / sin(θ / 2) is at most the limit and beveled otherwise, and the limit InDesign emits is exactly the text's miter-limit attribute (*STROKE_MECHANISMS.md*, M5). Controls: every requested limit from 1 to 500 is emitted verbatim; editing only `M` in the exported PDF changes the spikes and nothing else; page-item wedges match the textbook miter and bevel distances within 0.15 pt.

### Layer

The attribute's value is carried unchanged to the graphics port. The spike shape is the output format's own join rule applied to the doubled width.

### Existing Adobe capability

The miter limit on text is a real, working attribute: in scripting, in IDML, and in paragraph and character styles. Its default of 4 comes from *[No Paragraph Style]*. With Minion Pro at a 4 pt Outside stroke, a limit of 1.5 cuts the largest reach from 6.8 to 5.66 pt, and a limit of 1 gives a pure bevel (4.1 pt). Limits from 3 to 500 change nothing for that glyph set. SDK evidence points to the *Stroke* panel exposing it for text; this is still to be confirmed on screen.

### Workaround

Round join, or a lower miter limit set in the paragraph style.

### Script-level fix

Set the miter limit in the document's text styles to a value chosen from the measurements (1.5–2 for text halos). That is a real document setting, not a trick, but it bevels corners that exceed the limit instead of shortening them.

### SDK/native-plugin fix

Shortening long miters to a fixed length while keeping every other miter (the `miter-clip` behavior in SVG 2) has no PDF or PostScript operator, so it has to be drawn as geometry. That means stroke geometry built with the SDK's offset-path engine, which accepts a join type and a miter limit, drawn from the adornment in I2.

### Engine-level fix

Offer a clipped-miter join for text strokes, implemented as geometry at output.

### Risks

A clipped-miter implementation replaces native stroking for the affected text, with the same risks as I2. Lowering the limit changes the look of legitimately sharp corners.

### Verification

*B-miter.probe.md*: limit sweep, weight sweep, wedge apex extensions against theory, and outline comparisons.

### Verdict

ROOT CAUSE PROVEN, FIX NOT YET IMPLEMENTED. The mechanism isn't a defect in the join math. What is missing is a join rule that shortens instead of beveling, and that target behavior is a design decision.

## Fixture C: character-run stroke fragmentation

### Symptom

Formatting one character leaves visible notches: the next run's stroke covers the right edge of the previous run's glyphs. The fixture covers 6,239 fill pixels at 600 dpi.

### Root cause

The painting order from A, at run boundaries (*STROKE_MECHANISMS.md*, M4). A new wax run starts whenever the drawing attributes differ. That includes five differences with no visual effect: underline color with underline off, Lowercase on a lowercase letter, an applied condition with hidden indicators, a miter limit under a round join, and an explicit stroke tint of 100. Tracking, kerning, OpenType features, empty character styles, hyperlinks, XML tags, and language don't split runs. The halo's geometry is continuous; painting strokes first removes the notches entirely.

### Layer

Run segmentation happens in composition (private), and the damage happens in run text drawing.

### Existing Adobe capability

None that merges runs or changes their order.

### Workaround

Avoid local overrides on stroked text, including invisible ones. Where formatting is required, keep the stroke narrow enough not to reach the neighboring glyph.

### Script-level fix

No fix that keeps one live story, but the same split as A works here (*tools/jsx/ise-split-stroked-text.jsx*), because it operates per text style range rather than per frame. Measured without the plugin: one magenta character mid-line takes 2,763 covered pixels to zero, and one character at a different stroke weight and color takes 3,260 to zero (*tests/indesign-stroke/S-split/NOTES.md*, S02 and S03).

### SDK/native-plugin fix

The same designs as A (I1 or I2). No run merging is needed, because order alone removes the damage.

I1 is built and corrects this fixture as well, without a line of code specific to it, which is the strongest evidence that A and C are one defect rather than two. The adornment repaints the fill of every Outside-stroked run after the frame's text, and a notch at a run boundary is the same damage as a notch at a line boundary.

### Engine-level fix

The same as A.

### Risks

The same as A. One of them lands harder here: this fixture is nothing but local overrides, so almost every run is repainted, and the one-pixel edge ring therefore covers almost every glyph.

### Verification

*C-attribute-sweep.probe.md* (50 variants) and *C-attribute-sweep-expected.pdf*, with the run taken before the plugin existed preserved in *actual-before-plugin/*.

This fixture asks for no pixel measurements of its own, because it was built to answer which attributes split a run rather than how much damage results, so the evidence is the paint record and the render. With the plugin loaded, the 50 variants gain 105 path fills, 1 becoming 106, and the page render differs by 85,215 pixels. Live text is untouched: 215 text-showing operators before and after. Painting the fills as outlines rather than as text is what keeps that number fixed.

### Verdict

ROOT CAUSE PROVEN. SDK FIX IMPLEMENTED AND VERIFIED: the same adornment built for A corrects C unchanged.

## Fixture D: Create Outlines

### Symptom

Outlined stroked text shows no fill and a halo of the wrong size. Converted with the native command, the outlines of a 48 pt Outside 4 pt "Hog" keep 931 of the live text's 79,915 black pixels.

### Root cause

The conversion writes every stroke as a centered path stroke at half the width InDesign paints for the text, over the fill, and turns fill overprint on. Outside 4 pt text becomes Center 4 pt; Center 8 pt text also becomes Center 4 pt (*STROKE_MECHANISMS.md*, M6).

### Layer

The Create Outlines conversion (its capturing port and commands, private).

### Existing Adobe capability

Stroke alignment on paths. Setting Outside on the converted paths isn't faithful: counters come out wrong whatever the contour orientation.

### Workaround

Before converting, set the stroke weight to twice the intended weight and the alignment to Center, then send a fill-only copy to the front. Done by hand, this is what the faithful converter automates.

### Script-level fix

*tools/jsx/ise-faithful-outlines.jsx* converts a text frame and rebuilds each path as a stroke-only copy at the painted width plus a fill-only copy. Outside strokes go under every fill and Center strokes over them. Measured against live text, it matches within antialiasing: 1,322 differing edge pixels for Outside 4 pt, 901 for Center 8 pt, 1,350 for a Miter join at limit 10, and 1,975 for letters with counters. Because all of a frame's strokes end up under all of its fills, it also removes the fixture A overlap within the outlined frame.

### SDK/native-plugin fix

A responder that runs after the Create Outlines commands and applies the same rebuild (I5). It would cover text ranges, mixed formatting, and a single undo step, which the script prototype doesn't.

### Engine-level fix

In the conversion, keep the painted width and the paint order: emit the stroke as its own path under the fill.

### Risks

- The prototype handles whole text frames with uniform stroke and fill settings; everything else is left to the native command.
- After the canceled native command, the group lost its name; the contents were correct.
- The rebuild doubles the number of paths.
- Overprint fill is set from the text's own setting, not the conversion's.

### Verification

- *D-faithful.probe.md*: five pairs against live text.
- *D-hook.probe.md*: the real *Create Outlines* menu command with the hook installed matches live text within 1,324 pixels, and without the hook reproduces the native result.
- *D-outlines.probe.md* and *D-contours.probe.md*: the native behavior and the orientation test.

### Verdict

AUTOMATIC CORRECTION PROTOTYPED. *tools/jsx/ise-create-outlines-hook.jsx* intercepts the menu command in an InDesign session and was verified through the real menu action. It isn't installed permanently; installing it would mean placing a startup script.

## Candidate fixes

Scores: ● strong, ◐ partial, ○ weak or absent; for risk-type criteria, ● means low risk.

### A and C

| Candidate | Engine correctness | Visual fidelity | Text editable | Reflow | PDF and print | SDK support | Independence from internals | Version stability | Runtime cost | Simplicity | Reversible |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| I2 background strokes, native stroke suppressed | ● | ● | ● | ● | ● | ◐ | ○ | ◐ | ◐ | ○ | ● |
| I1 foreground fill repaint (built) | ● | ◐ | ● | ● | ● | ● | ● | ● | ◐ | ◐ | ● |
| I3 frame redraw from a draw event | ● | ◐ | ● | ● | ◐ | ● | ● | ◐ | ◐ | ○ | ● |
| I4 PDF post-processing | ◐ | ● | ● | ● | ◐ (PDF only) | ● | ● | ● | ● | ◐ | ● |
| Shadow stroke-only story behind a fill-only story (built) | ○ | ● | ◐ | ◐ | ● | ● | ● | ● | ● | ◐ | ◐ |
| Narrower stroke or more leading | ○ | ○ | ● | ● | ● | ● | ● | ● | ● | ● | ● |

Two rows are now scored from measurement, I1 and the shadow story; the rest are judgments. The shadow row's visual fidelity is full because its fills are painted once, natively, so it carries none of I1's edge ring, and its runtime cost is nil because the result is document content rather than work done at draw time. Its text editability and reflow stay partial for one reason, measured in *tests/indesign-stroke/S-split/NOTES.md*: the story then lives in two frames and nothing native keeps them in sync. Its engine correctness, text editability, reflow, and PDF and print columns are results from the fixture and the reproducers, and its PDF and print column is now measured on the PostScript path as well as the PDF one. Its visual fidelity stays partial because of the one-pixel edge ring, and its runtime cost stays partial because export costs about 1 to 3 % more and screen redraw itself was never timed directly. The other rows are estimates and should be read as such.

### B

| Candidate | Engine correctness | Visual fidelity | Text editable | Reflow | PDF and print | SDK support | Independence from internals | Version stability | Runtime cost | Simplicity | Reversible |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Miter limit 1.5–2 in text styles | ◐ | ◐ | ● | ● | ● | ● | ● | ● | ● | ● | ● |
| Clipped-miter geometry drawn by a plugin | ● | ● | ● | ● | ● | ◐ | ◐ | ◐ | ○ | ○ | ● |
| Round join | ○ | ◐ | ● | ● | ● | ● | ● | ● | ● | ● | ● |

### D

| Candidate | Engine correctness | Visual fidelity | Text editable | Reflow | PDF and print | SDK support | Independence from internals | Version stability | Runtime cost | Simplicity | Reversible |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Menu hook with faithful converter | ◐ | ● | Not applicable (outlines) | Not applicable | ● | ● | ● | ● | ● | ● | ● |
| Command responder plugin (I5) | ● | ● | Not applicable | Not applicable | ● | ● | ● | ◐ | ● | ◐ | ● |
| Outside alignment on converted paths | ○ | ○ | Not applicable | Not applicable | ● | ● | ● | ● | ● | ● | ● |

## Blocked on decisions

1. **Plugin toolchain: settled.** Visual Studio 2022 was updated and the MSVC v14.42 toolset added, which the SDK's precompiled-header shim accepts (`_MSC_VER` 1942). The plugin builds from *plugin/build-plugin.ps1*; everything that had to be worked out along the way is in *INDESIGN_STROKE_SDK_MAP.md* under "Building against this SDK".
2. **Plugin testing: settled.** The plugin is installed in *Plug-Ins/InDesignStrokeEngine/*, a folder of its own, so removing it is one deletion. Writing there needs administrator rights, so each install raises a prompt. InDesign was restarted for it, which is safe here because the author's document is saved and copied twice, to *corpus/pristine/* and *corpus/actual/*.
3. **On-screen checks: settled.** The author's screenshots show all four defects rendering identically in the GPU preview, the CPU preview, *Overprint Preview*, and with GPU Performance off. The display path is not involved, and the images are in *tests/indesign-stroke/corpus/actual/screen*.
4. **Startup hook.** Installing *ise-create-outlines-hook.jsx* as a startup script would make the D correction permanent.

## Still open

- Photoshop and Acrobat comparisons of the same geometry. InDesign 2025 and Illustrator are covered in *STROKE_MECHANISMS.md*, M7.
- The distilled PDF path. Printing to a PostScript file is measured in fixture N and carries the correction; distilling that PostScript back to PDF is not tested.
- InDesign Server, which is not installed on this machine, so the plugin has never been loaded outside the desktop application.

The duplicated fixture A frame was a mistake by the fixture's author and means nothing.
