# Cross-frame stroke order in InDesign, 2026-09-26

Route W draws a text frame's Outside strokes beneath that frame's fills. This sprint asked whether the same correction can reach across separate and threaded frames, and where InDesign allows it. The answer, measured in the host:

- **Threaded frames, and the columns of one frame, are one text object, and route X corrects them.** It is a production build of route W with one addition. When a later frame or column of the same story strokes over a fill the same drawing pass has already painted, route X withholds the stroke there with an even-odd clip. The damaged fill-core count falls to 0 in every threaded case of the census, in live PDF, PDF/X-4, flattened PDF/X-1a, PNG, EPS, and PostScript, on screen, and in background PDF export. Save, reopen, IDML, reflow, undo, and redo keep it.
- **Independent frames keep InDesign's stacking order, on purpose.** An upper frame's Outside stroke over a lower frame's text is the same geometry as a designer's halo. A rule that removes the first removes the second too, and the census proves it.
- **Two findings correct the route W certification.**
  - Route W's scope is the parcel (a column), not the frame.
  - Its certified build is not loaded on InDesign's background PDF export thread, which File > Export > Adobe PDF uses by default, so that export is drawn natively. Route X is declared a model plugin and runs there too.

The machine-readable matrix is [cross_frame_census.json](tests/audit/results/cross_frame_census.json), built by *tools/certify_routex.py summarize*. The session records are in [tests/audit/results/routex](tests/audit/results/routex).

## 1. Baseline identity

| Item | Value |
| --- | --- |
| Repository | *main* at `611d701` (route W certification), worktree clean; route W commits unchanged |
| Certified route W | *InDesignStrokeEngine.pln* `f31dd2c9…` (27,136 bytes), installed and loaded at the start; backed up in *build/indesign-install/20260925T171203Z-backup* |
| Host and tools | InDesign 21.5.1.73; Adobe InDesign 2026 SDK; MSVC 14.42.34433; Illustrator 30.7; Ghostscript 9.23; MuPDF 1.28.2 |
| Tests at the start | 90 geometry, 44 audit, 313 Node, and 33 plus 65,563 native core checks, all passing |
| Smoke subset (session S0, fresh process) | Victim census 0 damaged fill-core pixels in 8 outputs; cert-E per frame identical to the certified session A2; save, reopen, IDML, package, and place-back 0; PNGs byte-identical to A2; PDF structure identical except font subset tags; preferences unchanged against the sprint start and the pre-certification snapshot |

No baseline regression was found.

Builds made in this sprint, all into the ignored *build/* folder:

| Build | SHA-256 | Bytes | What it was for |
| --- | --- | --- | --- |
| Research 1 | `2da2d236…` | 59,904 | Trace of draw events and adornment passes; modes native, route W, story, overlap; suppression and deferred-emission probes. Its frame fill and frame stroke events were not gated on real paint, so its story mode never withheld anything |
| Research 2 | `a05acd09…` | 64,512 | The same, gated: the first working prototype |
| Research 3 | `baf0d4e5…` | 71,680 | A model plugin with thread-safe services, and lazy fill records |
| **Route X candidate** | `1e7c417e998c4ab8314384d6dc865d81f949b6ed3c7e8d0139da23d5ee3e1ef8` | 63,488 | No research code; mode fixed; deterministic content bounds. *plugin/build-plugin.ps1 -RouteX* into *build/Release-RouteX*; installed now |

The research builds read a control file and write a trace only when InDesign is launched with *ISE_XF_CONTROL* and *ISE_XF_TRACE* set; without them they behave as route W. The owner ran all four elevated installs.

## 2. Cross-frame fixture census

*tests/certification/corpus/cert-X.indd* (`29237a41…`), built by *tools/jsx/cert-build-xframe.jsx*, uses the certification victim: Minion Pro 30 pt, 14 pt leading, a 12 pt Paper Outside stroke over Black, on a Black 50% backdrop. Native damage and certified route W damage are from the same research binary in native and route W modes. Native mode reproduces the refusal build exactly (48,176–48,778 on the victim census, output for output), and route W mode reproduces the certified build (PNGs byte-identical). Figures are damaged fill-core pixels in the 600 ppi PNG.

| Case | What it is | Relation | Native | Route W | Route X | Expected |
| --- | --- | --- | ---: | ---: | ---: | --- |
| XA0 | Upper frame's stroke over lower frame's fill, both stroked | independent | 2,706 | 2,706 | 2,706 | unchanged |
| XA1 | Stroked upper frame over unstroked lower frame | independent | 2,706 | 2,706 | 2,706 | unchanged |
| XA2 | Unstroked upper frame over stroked lower frame | independent | 0 | 0 | 0 | control |
| XA3 | Two stories in touching frames (the XB0 geometry) | independent | 11,074 | 4,214 | 4,214 | unchanged |
| XA4 | Two stories on two layers | independent | 2,706 | 2,706 | 2,706 | unchanged |
| XA5 | Headline with a Paper halo over another frame's text | independent | 273 | 273 | 273 | unchanged |
| XB0 | Threaded; downstream frame in front (E00) | same story | 11,074 | 4,214 | **0** | corrected |
| XB1 | Threaded; upstream frame in front | same story | 6,932 | 4,196 | **0** | corrected |
| XB2 | Threaded frames overlapping by 10 pt | same story | 9,898 | 4,774 | **0** | corrected |
| XB3 | Threaded, an unrelated rectangle stacked between | same story | 11,074 | 4,204 | 3,272 | partial |
| XB4 | Threaded, downstream frame with a 20% frame fill | same story | 11,074 | 4,214 | **0** | corrected |
| XB5 | Threaded, both frames with a 1 pt border | same story | 10,988 | 4,222 | 206 | partial |
| XB6 | One frame, two columns, no gutter | same story | 7,195 | 2,461 | **0** | corrected |
| XB7 | Threaded pair split across layers | same story | 11,074 | 4,214 | **0** | corrected |
| XD0 | Threaded pair in a group | same story | 11,074 | 4,214 | **0** | corrected |
| XD1 | Threaded, upstream frame at 50% opacity | same story | 11,074 | 4,148 | 4,148 | unchanged |
| XD2 | Threaded, upstream frame on a hidden layer | same story | 3,762 | 0 | 0 | control |
| XD3 | Two table rows (cell text does not overlap) | same story | 0 | 0 | 0 | control |
| XN0 | Stroked text over a rectangle | single frame | 0 | 0 | 0 | control |
| XN1 | Rectangle over stroked text | single frame | 2,703 | 0 | 0 | control |
| XN2 | Threaded, Center strokes | same story | 21,056 | 21,056 | 21,056 | center |
| XN3 | Threaded, nonprinting upstream frame | same story | 3,762 | 0 | 0 | control |

Route W's remaining damage in a threaded case is exactly the part that crosses the frame boundary: the same four lines in one frame reach 0 (E01). XB6 shows that the columns of one frame are separate too. XN2's count is the Center stroke over its own fill, by definition, and is the same in every mode.

## 3. Callback and draw-order traces

The research build logged every draw event it registered for (all of *DocumentContextID.h*'s draw messages), every adornment question and visit, and every record and decision. The condensed traces are in [tests/audit/results/routex](tests/audit/results/routex) (*trace-\*.json*, from *tools/routex_trace.py*). In every output:

1. **The traversal is a spread draw.** A traversal opens with `kBeginSpreadMessage`, visits layers and page items in stacking order, and closes with `kEndSpreadMessage`, all into one graphics port. Route W's adornments draw into the same port. The export kinds:
   - A PDF export makes two traversals: the PDF itself, and a screen-kind one with flags 0x1000.
   - A PNG export makes screen-kind traversals.
   - An EPS or PostScript export makes a print traversal and a screen-kind one.
   - A document window draws with flags 0x800.
   - A background PDF export draws on a worker thread, with its own brackets and port.
2. **A text frame is a nest of page items.** For each frame, `kBeginShapeMessage` arrives for the spline, then the multicolumn item, then each column; frame fill events come first, then the frame hierarchy, and finally the frame stroke. A column's text is drawn between `kBeforeFrameBackground` and `kAfterFrameForeground`. `kBeforeFrameText` and `kBeforeFrameForeground` are never dispatched: 0 of each in every trace. That resolves the open lead in *docs/INDESIGN_PAINT_INTERCEPTION_AUDIT.md*.
3. **A parcel is drawn whole before the next one starts.** Route W's adornments are asked about the parcel (`GetIsActive` receives its `IParcelShape`). They then visit all its runs in the stroke pass, and all of them again in the restore pass. No parcel's passes resumed after another parcel began, in any traversal of any output.
4. **Frames of one story are independent page items.** XB0's upstream frame finishes all its passes before its downstream frame begins its own, and XB1 draws them in the opposite order because it is stacked that way. No callback spans a story.

## 4. Candidate host APIs

| Candidate | Scope | Can observe frames? | Can suppress native output? | Can emit later? | Screen | PDF | Print | Risk |
| --- | --- | :---: | :---: | :---: | :---: | :---: | :---: | --- |
| C1 Global text adornment passes (route W) | one parcel | yes: the parcel gives frame, story, and parcel count | a run's stroke, by muting it in the wax until the Text pass | no; the passes of one parcel run in a fixed order | yes | yes | yes | certified; background only as a model plugin |
| C2 Draw events: `kDrawShapeMessage` | one page item | yes: every item, and spread, layer, page, and group brackets | the whole item only: returning handled removed XB0b entirely (7,084 pixels) | no | yes | yes | yes | coarse; the brackets are what route X uses |
| C3 Draw events: `kBeforeFrameText`, `kBeforeFrameForeground` | frame text | never dispatched | no: returning handled changed nothing | no | no | no | no | rejected |
| C4 Spread callback (`kAfterContentMessage`) emitting buffered geometry | spread | through records | no | yes: recorded outlines landed on the painted glyphs (415,963 pixels, 0.4% more than a pixel off the fill mask) | yes | yes | yes | draws above every item on the spread; rejected as a correction point |
| C5 Graphics state kept past its callback | traversal | n/a | n/a | n/a | n/a | n/a | n/a | ports are per traversal; not attempted, by rule |
| C6 Earlier frame draws a later frame's strokes in advance | spread | would need prediction | n/a | n/a | n/a | n/a | n/a | rejected on evidence (section 5) |
| C7 Knockout across any overlapping text (research overlap mode) | traversal | yes | no; withholds strokes | no | yes | yes | yes | fixes XA0–XA4 and removes the halo; rejected as a default |
| **C8 Knockout within a story (route X)** | traversal and parcel | yes | no; withholds strokes | no | yes | yes | yes | chosen |
| C9 Custom run renderer, composer, or factory replacement | run | n/a | n/a | n/a | n/a | n/a | n/a | not reopened: no supported registration (*docs/INDESIGN_PAINT_INTERCEPTION_AUDIT.md*) |

Every candidate except C9 was run in the host.

## 5. Rejected approaches

- **Moving the stroke earlier.** Nothing drawn later can go beneath something already painted, and the earlier frame's pass ends before the later frame exists in the traversal (section 3). A later frame's stroke could only be drawn early (C6) if the earlier frame's pass predicted that the later frame will be drawn. InDesign decides that item by item, and the trace shows the decision varies by output:
  - The nonprinting upstream frame of XN3 is drawn in a window, and not in any export.
  - XD2's frame on a hidden layer is drawn nowhere.
  - A single-item export draws one frame with no spread bracket at all.
  
  A prediction would have to replicate InDesign's filtering, other plugins' included.
- **Holding host drawing state.** Graphics ports are per traversal (distinct pointers per export, per window draw, per thread). Retaining one, a port's clip, or a wax pointer past its callback has no documented lifetime.
- **Redrawing the earlier frame's fills after the later stroke.** This paints each fill twice (the refusal build's history), duplicates it under antialiasing and transparency, and puts it above content stacked in between.
- **Emitting at the end of the spread** (C4). It is geometrically exact, but everything emitted there lands above every page item.
- **Suppressing whole items and redrawing them** (C2). The veto is item-granular: it can only reorder whole items, which is a z-order change.
- **Overlap as the compositing domain** (C7). It corrects the independent pairs and destroys the halo: XA5's body text reappears through the headline's Paper stroke (273 fill-core pixels no longer covered). XA0 and XA5 are the same geometry, and nothing InDesign knows about either separates a defect from a design.

## 6. Compositing-domain findings

The unit is the **story**: the parcels of one frame list. Its frames and columns are one text object to a reader. The E00/E01 control (identical text, one frame boundary apart) shows InDesign treating them inconsistently; the refusal build's own history shows the within-frame case to be a defect. Independent stories are separate page items, and InDesign's stacking between them is kept, halo included.

Within a story, route X withholds a later stroke only from fills that are actually painted in the same traversal, into the same port. A fill that has content drawn over it in the meantime is left to InDesign's order, because that content is above it in any order. That content can be:

- an unrelated item stacked between (XB3, second line only);
- the frame's own border (XB5, the glyphs the border crosses);
- the stroking frame's own frame fill.

A translucent or blended upstream frame is not recorded (XD1): withholding a stroke beneath a fill that is not opaque would show the backdrop where the stroke should show through.

## 7. Threaded-story findings

- **Two threaded frames are painted independently.** Their parcels share a frame list, which the adornment's parcel exposes as the story's identity. Membership is read at every draw, so reflow changes it as it happens:
  - ED4 pushed a line from one frame into the next (XB0), and pulled one back (XB1). Both reach 0.
  - Threading two independent frames made them one story, and XA3 went from 4,214 to 0.
  - Undoing made them two stories again, and 4,214 returned.
- **Nothing outlives a traversal.** Fill records belong to one spread draw and one port, and are dropped when the bracket closes. A frame invalidated alone is redrawn with no records from other frames, so its strokes are route W's.
- **There is no stale geometry.** For all 8 ED4 edits:
  - Undoing all of them is pixel-identical to the original.
  - Redoing them, reopening the saved copy, and taking it through IDML are each pixel-identical to the live edit.

## 8. Screen and export differences

| Path | Brackets | Route X | Evidence |
| --- | --- | --- | --- |
| Document window | spread, flags 0x800 | applied | trace: the same decisions as export; XN3's nonprinting upstream frame is drawn and corrected on screen only |
| PDF (foreground), PNG, EPS, print to PostScript | spread | applied | outputs, section 9 |
| Background PDF export (File > Export > Adobe PDF) | spread, on a worker thread | applied only because route X is a model plugin | the certified build's output is pixel-identical to native; route X's is pixel-identical to its foreground export |
| Single-item export (*PageItem.exportFile*) | none | route W only | trace: no spread bracket, so nothing is recorded; the other frame is not in the image either |

One adapter serves every path. What differed was the host's plugin loading and one host query:

- **Plugin loading.** A UI plugin (*kUIPlugIn*) is not loaded on worker threads, so neither route W nor any other adornment runs there. A model plugin (*kModelPlugIn*) whose services return `kMultipleThreads` gets one instance per thread session, and its state is thread-local.
- **Painted bounds.** `IShape::GetPaintedBBox` pads its result by about 9 pt on the main thread and 1 pt on a worker thread. Research build 3, which used it, withheld XB3's first line in background export but not in the foreground. The candidate uses geometry instead: path and stroke bounds, a column's parcel ink bounds, and painted bounds only for items with transparency effects. That makes the two paths agree (section 9).

## 9. Prototype results

Research build 2 in story mode passed all 22 cases; research build 3 reproduced it exactly. The candidate, in two fresh processes (C1 and C2):

- **Cross-frame census:** 22 of 22 cases meet their expected outcome in all six outputs (HQP, PDF/X-4, PDF/X-1a, 600 ppi PNG, EPS, PostScript).
- **Background PDF export:** HQP, PDF/X-4, and PDF/X-1a of the victim census and the cross-frame census are pixel-identical to the foreground exports of the same session (maximum difference 0). Route W's code as certified, in a UI plugin as the certified build is, exports the victim census natively in the background. The research build ran it in its route W mode: pixel-identical to native, and 14,157 pixels differ from its foreground export at 150 dpi.
- **Reproducibility:** C1 and C2 PNGs are byte-identical, and their PDFs render identically.
- **Edges:** against a story reference (the threaded story duplicated behind itself with Center 2w strokes and no fill, the original keeping fills only), all differences above 16 levels lie within one pixel of a glyph edge, and none above 64 lies beyond two pixels. The maximum difference is larger than route W's own ring:
  - 102–195 in MuPDF;
  - 223 in Ghostscript;
  - up to 255 in InDesign's PNG, at the seam where the clip meets a glyph edge.
  
  For comparison, the same comparison on single frames gives 53–140.
- **Structure:** in every PDF, text, text render modes, images, and fonts are the same as route W's. Only the clip (`W`) and graphics-state operators increase.
- **Illustrator 30.7:** it opened the HQP and PDF/X-1a exports and saved them with editing capabilities on and off, and the editable copy reopened. Every corrected case is 0 in all three, the text is kept (192 and 206 characters), and no pixel differs from the InDesign export by more than 64 levels.

## 10. Regression results

- **Victim census:** 0 damage in all 8 control outputs, per frame identical to the certified session A2, PNGs byte-identical.
- **Transparency (cert-T) and color (cert-C):** per frame identical to A2, PNGs byte-identical.
- **Scope (cert-E):** identical to A2 except E00, the threaded case, which went from 4,198 to 0.
- **Round trips:** INDD save and reopen, IDML, IDML reopened, package, and PDF and EPS placed back all 0.
- **Documents saved by route W:** saved, from IDML, and edited, they reopen under route X unchanged.
- **Edit batteries:** ED1–ED3, the certification's, are exact again: 23 and 13 undo and redo steps, and five save and reopen cycles.
- **No document state:** no saved INDD or IDML, including ED4's edited, IDML, and reopened documents, contains the plugin's name or its ID prefix (*tools/certify_routex.py state-scan*).
- **Preferences:** unchanged across every session (*certify_routew.py prefs-diff*).
- **Screen stress:** the certification's redraw stress (60 window actions, 4 saves mid-redraw) is exact again.
- **Stability:** no crash or hang in any of the six InDesign processes (S0, three research builds, C1, C2) and the one Illustrator process.

## 11. Performance

Seven repetitions, interleaved by mode in one process (*tools/jsx/cert-xbench.jsx*, research build 3, whose logic the candidate shares except for its content bounds). Figures are the median against route W:

| Document | PDF | PNG 150 ppi |
| --- | ---: | ---: |
| Victim census (no threaded story) | −1% | +1% |
| Cross-frame census | −7% | +3% |
| 20 pages, one unstroked story threaded through 20 frames | −1% | −1% |
| 20 pages, one stroked story threaded through 160 frames that all overlap (worst case) | +28% | +21% |

In the worst case the PDF is 2.6 times route W's size (1.47 MB against 0.57 MB): each withheld glyph's outline is written into the clip of every stroke that meets it. Research build 2, which built every recorded fill's outlines at once, cost 29% on the unstroked threaded document's PNG. The lazy records in build 3 and the candidate build outlines only when a stroke meets a fill, which removed that cost.

## 12. Architecture decision and remaining boundary

**Decision: a temporary per-traversal buffer (family B) holding host-independent geometry (family C), with one adapter for every output.** Route W stays as it was certified: frame-local passes, stroke muted in the wax. Route X adds the following:

- a `kDrawEventService` handler that brackets each traversal and reports content drawn in it;
- records of painted fills, keyed by story, frame, parcel, and port, with outlines built lazily and a counted font reference;
- even-odd clips that withhold a stroke from those fills.

The plugin is declared a model plugin so that it also runs on the background export thread. Nothing is persistent, and nothing crosses a traversal.

Remaining boundary:

- **Independent frames:** unchanged, by design (section 5).
- **Content in between:** correction stops glyph by glyph where content drawn in between covers a fill. That includes the frame's own border, and a partly covered glyph is left whole.
- **Translucent upstream frames:** not corrected.
- **Single-item exports:** draw route W only.
- **Seam variance:** larger than route W's ring, within one device pixel of an edge.
- **Worst-case cost:** time and PDF size as in section 11.
- **Screen:** judged by trace only; there is still no pixel capture of the screen.
- **Untested:**
  - threaded frames on master pages;
  - text on a path threaded to a frame;
  - anchored threaded frames;
  - rotated threaded pairs (a rotated two-column frame was tested).
- **Unchanged:** Center strokes and miter joins.

## 13. Exact next experiment

The miter-spike correction, which is independent of stroke order and now the largest open StrokeMaster defect in InDesign. Its next bounded experiment is the one *docs/MITER_GEOMETRY_CONTRACT.md* names:

- compare a one-sided outward offset of the filled glyph region, with joins built only at its convex exterior corners, against symmetric stroking that also builds inward miter tips;
- run the pending host controls first: an exact U-turn under miter, bevel, and round joins, then a closed two-anchor contour.

Measure it against suite B (*tests/indesign-stroke/B-miter-spikes*) with the certification harness. It passes only if every row of this census and of the route W matrix stays unchanged. Before any release, route X also needs a pixel-level screen check (the owner's view of *cert-X.indd* at 400% would do) and a real multi-page document.
