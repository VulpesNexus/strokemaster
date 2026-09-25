# StrokeMaster research revival, 2026-09-25

This pass resumed StrokeMaster from its repository, history, tests, and host evidence, and pushed the native routes in both primary hosts. Every claim below carries one status: **PROVEN** (demonstrated here by code, host experiment, or reproducible fixture), **ESTABLISHED** (documented by an authoritative SDK or Adobe source), **DERIVED** (follows from proven or established evidence, with the reasoning given), **HYPOTHESIS**, **REJECTED**, or **BLOCKED** (with the exact blocker).

Hosts measured: Illustrator 30.7.0, build 114R, and InDesign 21.5.1.73, on Windows x64. SDKs: Illustrator 2026 and InDesign 2026, MSVC 14.42.34433, Windows SDK 10.0.26100.0.

## A. Recovered baseline

At takeover the repository was on `main` at `c94a156`, four commits after the checkpoint `c459683`, with a clean worktree, remote `origin` (*VulpesNexus/strokemaster*), and no tags. The brief's picture of the project was older than the repository.

| Area | Previous claimed state | Evidence at takeover | Verdict | Action taken |
| --- | --- | --- | --- | --- |
| Pure tests | About 219 | 78 geometry, 10 audit, and 308 Node tests, all passing (396) | Stale count; suite healthy | Grew to 78, 25, 308, plus 33 and 291 native checks |
| Illustrator native load | First clean load never completed | Prep-4 and prep-5 lifecycles already passed (332 assertions, PID-attributed exits) in `12e9506` | Stale; load was **PROVEN** before this pass | Moved on to the rendering route |
| Illustrator region effect | Q20: Go never executes | No raw record retained; only the summary in *OPEN_QUESTIONS.md* | Did not reproduce (see C) | Fixed and host-proven |
| InDesign native plugin | No conclusion on whether one can intervene | A native refusal build (`15b802eb…`) is installed, loads, and matches no-plugin output byte for byte on a 20-case census | Load **PROVEN** earlier; intervention open | Route W built (see D) |
| Unstroked victim | 3,759 wrong interior pixels | *tests/audit/results/unstroked-victim.json* and *host-victim-census.json* record 3,759 core pixels for VC00, VC04, VC07, VC08, and VC17 | Reproduced in the record; not rerun (InDesign host blocked, see I) | Root cause refined (see F) |
| IDML preservation | Verified | *pair-persistence-render.json*: 81 host checks, zero changed pixels from INDD through IDML and back to INDD | **PROVEN** earlier | None |
| Topology core | Reviewed | *tests/geometry/TOPOLOGY_REVIEW.md*, 43 API checks, 80 rectangle unions | **PROVEN** for linear polygons | Linked and executed in Illustrator |

Preserved evidence: the tested binaries are frozen read-only beside their sources (*plugin/illustrator-probe/build/region-prep2-frozen.aip*, `bd6bd27b…`; *region-prep9-frozen.aip*, `a78d4398…`; the prep-4 and prep-5 frozen AIPs are unchanged and hash-checked by every region build). The region-prep-7 binary that produced the committed prep-7 record (`47b6b544…`) was overwritten by later builds; its source is commit `09e1da9`. No earlier negative result was deleted; superseded statements are corrected below and in the canonical documents.

## B. Evidence ledger

| # | Claim | Status | Evidence |
| --- | --- | --- | --- |
| 1 | Illustrator 30.7.0 loads, initializes, and runs StrokeMaster native effects | PROVEN | Twelve Illustrator processes this pass (ten with the plugin loaded, two with it held out), each quit with zero documents; zero Illustrator or InDesign fault events logged all day |
| 2 | The region effect's Go never executes (Q20) | REJECTED | Region-prep-2, unchanged, reached `goCalls=1` on the first redraw in a fresh process and refused at its own gate `source.paint-count` |
| 3 | A new group has no art style; a parser-built style for it has two None paint fields | PROVEN | Region-prep-3/4: `wrapperBase=2`, `wrapperPaintFields=2`, `ContainsPaint=0` |
| 4 | A post-effect on a styled group receives one child per paint field plus the contents, at the group-contents position | PROVEN | Region-prep-5 census `1(1(None),1(contents),1(None))`, `GetGroupContentsPosition=1` |
| 5 | A post-effect on a filled, stroked path receives the stroke and the fill as separate art, stroke above | PROVEN | Census `1(2[0,1],2[1,0])` |
| 6 | Illustrator hands a native effect declaring path and compound input **glyph outlines of live text**, one compound per glyph with its own fill and stroke, while the source stays a live TextFrame | PROVEN | Region-prep-8 census on one- and two-line text; `liveTextKept=true` in every case |
| 7 | Illustrator's native PDF paints a run's fills, then its strokes (`Tj`, `Tr 1`, `Tj`) | PROVEN | *pdfctl-none.pdf* paint sequence |
| 8 | Under any live effect, Adobe Offset Path at 0 pt included, Illustrator exports text as outlines, with no PDF text or fonts | PROVEN | *pdfctl-adobe.pdf* and *pdfctl-census.pdf*: 0 text characters, 0 fonts |
| 9 | InDesign's PDF paints each run's Outside stroke, then its fill, run after run | PROVEN (earlier) | *tests/audit/results/pdf-operators.json*: text render modes alternate 1, 0 per run |
| 10 | InDesign adornment passes are frame-wide: every run of a frame is visited in one pass before the next pass | ESTABLISHED | *TextDrawPriority.h*, lines 36–42 (InDesign 2026 SDK) |
| 11 | The native region correction publishes a topology-correct vector region in Illustrator | PROVEN (marked analytic fixture) | Region-prep-7: bridge and host `applyEffect` both published; PDF effect minimum x −0.3000 against the control's −0.4708 escape; both pass the measurer |
| 12 | The region correction survives undo, redo, and AI save/close/reopen, and refuses edited input without an alert | PROVEN (fixture) | *illustrator-region-prep7.json*, probe `life1` |
| 13 | Paint order alone removes all stroke-over-fill damage for opaque, Normal text in Illustrator | PROVEN (fixture) | Region-prep-9 stroke-behind: 55,568 → 0 damaged fill-core pixels; ink and off-fill pixels identical to the outlined control |
| 14 | The stroke-behind correction survives live edits, undo, and a fresh-process reopen | PROVEN (fixture) | Probes `sb2`, `xp-present` |
| 15 | Without the plugin, a document keeps drawing the corrected result from its cache, but a text edit leaves the drawn art stale | PROVEN | `xp-absent` (corrected) and `xp-absent-edited` (text HH/HHH, drawn HH/HH); LiveShear recorded the same host behavior |
| 16 | InDesign 2026 can load native plugins | PROVEN (earlier) | Refusal build loaded and byte-identical to no-plugin output on 20 frames |
| 17 | InDesign's Plug-Ins folder cannot be written without elevation, and no per-user plugin folder exists | PROVEN / DERIVED | ACL read and a failed write this pass; the user preferences folder has no plugin location; the SDK's plugin-config bosses enable plugins, they do not add folders |
| 18 | Route W (Background-pass strokes, muted stroke paint in the Text pass) corrects InDesign's overlap | HYPOTHESIS | Built, 291 pure checks; host test BLOCKED on elevation |
| 19 | The unstroked-victim defect needs topology-aware geometry | REJECTED as stated | Claim 13: the victim is a paint-order defect; geometry is needed for escapes that lie outside every fill (claim 11) |
| 20 | One shared Adobe stroke component could be hooked for all hosts | REJECTED | All three hosts ship their own builds of AGM 8, CoolType 11, BIB 5, ACE 7, and AdobePDFL 18 (different hashes); neither SDK exposes AGM's stroker |
| 21 | The earlier Illustrator host exits at `Illustrator.exe+0x18162a7` were caused by StrokeMaster | REJECTED as an attribution | The same offset is Illustrator 30.7.0's document-churn crash, reproduced by the LiveShear and FreeDistort+ sessions with every third-party plugin removed |
| 22 | Illustrator does not return written path coordinates bit for bit | PROVEN | Readback drift up to 7.7e-13 pt |
| 23 | Illustrator writes PDF coordinates to three decimals, and a live effect that returns an error raises a modal alert per redraw | PROVEN / DERIVED | Coordinates in every region PDF; the owner cleared an error dialog after three refusals returned `kBadParameterErr`, and none has appeared since refusals pass through |
| 24 | For a uniform text stroke, one object-level Appearance stroke beneath the characters removes all stroke-over-fill damage **and the PDF keeps its text and font** | PROVEN (fixture) | Region-prep-10: 72,080 → 0 damaged fill-core pixels, coverage identical to native; PDF paints `Tr 1` shows, then `Tr 0` shows ([record](tests/audit/results/illustrator-region-prep10-appearance.json)) |
| 25 | That Appearance renders, exports, and follows edits in Illustrator without the plugin | PROVEN (fixture) | Fresh processes with the plugin held out: corrected as opened; after appending a glyph, the glyph renders and the order holds |
| 26 | Mixed per-character strokes cannot be expressed as one object-level stroke | DERIVED | An object-level paint field applies one stroke to the whole frame; per-character values live in the characters' own attributes, which paint inside the contents |
| 27 | Curved outlines can be flattened to a stated tolerance and offset by the reviewed core within the matching distance band; on curves, legitimate miters are kept and excessive ones are clamped at the limit | PROVEN (analytic fixtures) | *tests/geometry/test_curve_region.py*: circle and annulus bands; lens tips of 30° (full miter) and 20° (clamped at 4h) |
| 28 | Real glyph topology passes the adapter and the core, and every accepted result keeps its fill | PROVEN (five system fonts, 189 glyphs each, two offsets) | *tests/geometry/glyph_region_census.json*: 184–187 of 189 accepted per static font; worst lost fill area 0 |
| 29 | Everything the pipeline cannot handle is refused rather than drawn wrong | PROVEN (census) | Touching composite parts and overlapping variable-font contours are refused by the adapter; Boost buffer failures on closing apertures (Arial Bold `@` and `e` at h = 120) and self-intersecting contours are refused by the core's checks |

## C. Illustrator native status

**Load: PROVEN.** Already proven before this pass, and repeated in ten plugin-loaded processes today with no faults.

**Rendering intervention: PROVEN, for two bounded fixtures.** The usable intervention point is the **live effect** (`AILiveEffectSuite`, post-effect) on the source object's art style, not a stroke-generator replacement. Illustrator builds the styled art, hands it to Go, and draws and exports whatever Go leaves. The source stays editable, save/reopen keeps the effect and its cached result, and undo works.

- **Q20 resolved.** Four plugin-side causes, each measured and fixed: the wrapper gate counted None paint fields (fix: `ContainsPaint`); the output stage assumed one processing child (fix: locate the contents by group-contents position and require every other child to be demonstrably unpainted); readback was compared bit for bit (fix: 1e-4 pt tolerance, deltas reported); and refusals returned an error that raised a modal alert (fix: pass the processing copy through, report only a failed rollback).
- **Region correction (topology core, marked analytic fixture).** Published through both the plugin's own attachment and the host's `PageItem.applyEffect`. PDF: one filled vector region, the hole kept, minimum x −0.3000; the native control's hole miter escapes the 0.2 pt wall to −0.4708. Both PDFs pass *tools/measure_region_pdf.py*.
- **Stroke-behind (live text, paint order).** Two lines of *HH* on 34 pt leading, 8 pt mixed per-character strokes, and an unstroked victim glyph: fill-core damage 55,568 of 72,080 pixels natively, 0 corrected (MuPDF, 600 dpi). Against the outlined pass-through control, which shares the export route, ink coverage and every pixel away from the fills are identical. It republishes after live edits and undo, and after a fresh-process reopen.

- **Native Appearance (uniform strokes).** Region-prep-10's bridge writes one object-level stroke beneath a text frame's characters, a plain style with no effect. On the uniform-stroke fixture it removes all damage, keeps PDF text and fonts, and needs no plugin to render, export, or follow edits (claims 24 and 25).

**Constraints, all measured:** under any effect the PDF carries text as outlines (claim 8); without the plugin, edited text under an effect renders stale (claim 15), which the Appearance route avoids; effect order relative to other Appearance entries, transforms, gradients, and transparency are refused or untested.

## D. InDesign native status

**Can InDesign 21.5.1.73 load native compiled plugins? PROVEN (earlier), confirmed installed today.** SDK 2026, MSVC 14.42, a *.pln* in *Plug-Ins\InDesignStrokeEngine* with its *(InDesignStrokeEngine Resources)* folder; the installed refusal build `15b802eb…` loads and changes no pixel.

**Can such a plugin influence the relevant output? ESTABLISHED at the pass level; the route is built but not host-tested.** InDesign's own text drawing order is the fault (claim 9), and the SDK's adornment passes are frame-wide (claim 10). Route W (*plugin/source/IseStrokeBehindAdornment.cpp*, opt-in with *build-plugin.ps1 -RouteW*) strokes each Outside-stroked run in the Background pass with the run's own stroke rendering object at twice the weight, sets the run's stroke paint to [None] in the wax until the Foreground pass, and restores it there. The document's attributes are never changed, so without the plugin the same document draws natively and InDesign's own stroke controls remain the user interface. The fills remain native text, so PDF keeps searchable text and fonts, unlike Illustrator's effect route.

**Can it correct strokes while preserving live text? HYPOTHESIS, BLOCKED.** Route W is built (30,720 bytes, `c4ef7c33…`) with 291 pure checks of its ledger. Installing it needs one elevated copy into the Plug-Ins folder (claim 17). The fallback, if the Text pass ignores a muted wax paint, is **route A**: a plugin character attribute (the SDK's *basictextadornment* sample shows attribute, adornment, scripting, and root-style responders) holding the stroke, with the native stroke set to None.

**Scripts as a front end.** The existing split, resync, and outline scripts stay valid document-level alternatives. With route W they are not needed for correction, because route W reads InDesign's own stroke attributes; they remain migration and diagnostic tools.

## E. Rendering root-cause map

| Defect | Host | Document model | Paint-order generation | PDF operators | External renderer | Fault domain |
| --- | --- | --- | --- | --- | --- | --- |
| A. Neighboring stroke over fill | Illustrator | Correct (per-character attributes) | Run fills then run strokes; under an effect, glyph by glyph | Wrong order written (claim 7) | Faithful | Host text drawing order, before serialization |
| A. Same | InDesign | Correct | Per run: stroke, then fill | Wrong order written (claim 9) | Faithful | Same |
| B. Miter spikes | Both | Correct | Stroker applies native miter limit per join | Stroke written as center line; spike produced by any conforming renderer | Faithful | Stroke geometry semantics; topology decides whether a spike escapes a fill |
| C. Run fragmentation | InDesign | Formatting splits wax runs | Each run strokes and fills independently | Per run | Faithful | Run segmentation plus defect A's order |
| D. Unstroked victim | Both | Correct | Later strokes painted after earlier fills | Written | Faithful | A paint-order defect (claim 13) |
| E. Live text and reflow | Illustrator | Live | Effects rerun on every edit (proven) | Outlines under an effect | — | Host export policy for effected text |

## F. Geometry findings

- **The unstroked victim is paint order, not geometry (claim 19).** Moving every stroke beneath every fill removed all fill-core damage without any Boolean operation, while leaving coverage identical.
- **Topology is needed where a stroke escapes every fill.** The region fixture's hole miter pierces the outer wall and lands outside the shape, where no fill order can hide it; the linked topology core removes it and keeps the ordinary outer miters (PDF measurement, claim 11).
- **Clamped rather than rounded joins.** The published region keeps miter joins inside the limit and removes only the inward excursion; the measurer's `ordinary_outer_corner_preserved` check passes.
- **Formatting runs.** The stroke-behind transform keeps each glyph's own stroke paint and relative order, so mixed per-character strokes are handled without merging formatting (fixture: three stroke states across two lines).
- **Curves and real glyphs.** *tools/curve_region.py* flattens cubic contours (both control points within the tolerance of the chord) and assigns ring roles by containment depth, refusing crossing or touching contours. On analytic curves the core's result stays within the band the tolerance allows, and the miter policy carries over: a 30° lens tip keeps its full miter, a 20° tip is clamped flat at 4h (claim 27). On real glyphs, five system fonts pass at 97–99% per static font with no lost fill (claims 28 and 29).
- **Still open:** fill-rule resolution for touching or overlapping contours (a reviewed Boolean pre-pass before the core), and applying the core to glyph outlines inside the Illustrator stroke-behind route.

## G. Support matrix (this pass)

| Host | Version | Screen | PDF export | Save/reopen | Undo | Live edit | Reflow | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Illustrator, region effect | 30.7.0 114R | Not captured | PASS (measured) | PASS, same process | PASS | Refuses edited fixture | n/a | Marked fixture only |
| Illustrator, stroke-behind | 30.7.0 114R | Not captured | PASS (measured; text outlined) | PASS, fresh process | PASS | PASS | PASS (added glyph) | Opaque Normal text fixture |
| Illustrator, without plugin | 30.7.0 114R | Cached result | PASS (cached) | Opens | — | **Stale drawing** | Stale | Host behavior |
| Illustrator, native Appearance (uniform stroke) | 30.7.0 114R | Not captured | PASS (measured; **text kept**) | PASS, fresh process, plugin absent | Not measured | PASS without the plugin | PASS (added glyph) | Uniform-stroke fixture; authoring needs the plugin, rendering does not |
| InDesign, refusal build | 21.5.1.73 | Earlier PNG census | Unchanged | PASS (earlier) | — | Native | Native | Installed; no correction |
| InDesign, route W | 21.5.1.73 | BLOCKED | BLOCKED | BLOCKED | BLOCKED | BLOCKED | BLOCKED | Built; install needs elevation |

Screen captures were not taken: the probes ran over COM in a window shown without activation, and screen rendering was not independently compared against export.

## H. Architecture decision

The evidence supports **a shared StrokeMaster core with host-specific native adapters**, and nothing larger:

- No shared Adobe component can be hooked (claim 20), and the two hosts expose unrelated extension points: Illustrator a post-effect over styled art, InDesign frame-wide text adornment passes.
- What is shared is decided above both hosts: the paint-order rule (all strokes of a text object beneath all its fills), the topology rule for escaping joins (the reviewed core), and the measurement contracts.
- Illustrator adapter, two tiers. **Uniform strokes:** author a native object-level Appearance stroke beneath the characters (and clear the character stroke); the result is ordinary document data, keeps PDF text, and needs no plugin at render time (claims 24 and 25). **Mixed per-character strokes:** the live effect declaring path and compound input, applying stroke-behind to glyph outlines and, where a stroke escapes a fill, the topology core; its costs are outlined text in PDF and stale drawing when the plugin is missing.
- InDesign adapter: route W if the host test passes; route A otherwise.

## I. Remaining blockers

1. **InDesign host test (BLOCKED on elevation).** Needs one elevated copy of *build\Release-RouteW* into *Plug-Ins\InDesignStrokeEngine*, with InDesign closed: `powershell -NoProfile -File tools\install-indesign-plugin.ps1 -Variant Release-RouteW`, run as administrator. It backs up the installed refusal build and verifies hashes; `-Restore <backup folder>` puts it back.
2. **Fill-rule resolution.** The curve adapter and core now handle curved glyph outlines (claims 27–29), but touching composite parts and overlapping contours are refused. A reviewed Boolean pre-pass (nonzero winding into explicit shells and holes) would admit them; the Boost buffer failure on closing apertures remains a backend limit that the core refuses.
3. **Screen versus export in Illustrator.** No screen capture comparison was made; both fixtures were measured in PDF only.
4. **Illustrator text with mixed per-character strokes.** The Appearance route keeps PDF text for uniform strokes (claim 24); mixed strokes still need the live effect, whose PDF outlines the text. Untested: splitting a frame's strokes into per-value object-level fields by another means.

## J. Next experiment

**Install route W and run the existing 20-case InDesign victim census against it.** After the elevated install and an InDesign restart, run *tools/run-id-probe.ps1* with *probe-victim-census.jsx* into a fresh folder, then compare with *tools/compare_renders.py* and the fill-only mask against the recorded no-plugin collection (*build/host-census/victims-noplugin*). Pass criteria: VC00, VC04, VC07, VC08, and VC17 fall from 3,759 changed core pixels to zero against the split reference; the benign controls VC03, VC09, VC11, VC12, and VC13 stay within the no-plugin comparison; the PDF keeps its text and shows every stroke before every fill of a frame. Any Text-pass stroke that survives the mute rejects route W in favor of route A.
