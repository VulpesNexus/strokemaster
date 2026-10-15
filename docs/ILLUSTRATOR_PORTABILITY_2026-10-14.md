# Illustrator portability and InDesign-specific gaps (2026-10-14)

## Current boundary

The last recorded installed Illustrator probe is the region-prep-11 diagnostic, measured on Illustrator 30.7.0; its current installed hash was not rechecked. The Illustrator executable now reports **30.8.2**. No host load, document mutation, or visual test was run in this pass. The source build here is **region-prep-12**, a compile and mock candidate only; it still applies the older topology experiment solely to its marked analytic fixture. It does not expose a general stroke correction to users.

The Illustrator project now compiles and links `IseRegionCore.cpp` and `IseClipper2.cpp` alongside its existing topology experiment. `IseMiterCore.h` is used by a new path adapter. `IllustratorSdkContour.h` reads Illustrator's anchor, incoming handle, and outgoing handle into the shared `ise_miter::Contour` representation. It retains the last-to-first cubic of a closed path and rejects nonfinite or out-of-range coordinates before changing its output. The marked-fixture source reader uses that admission check. This is a source integration, not evidence that Illustrator produces correct glyph art.

The final **complete** hidden build passed 70 registration, bridge, and refusal mock assertions, 2,879 output-backend assertions, and 13,229 stroke-behind assertions; its candidate artifact hash was `1a56e1a90083e38d197a44fb14ade46283884d3c045bfbd33909ec8a078391d7`. The later attempt to link an additional standalone contour test stalled. A narrower attempt to link the real region core into the mock executable failed with **No space left on device** while writing an object. The project was returned to the passing mock-target structure and rebuilt successfully.

**2026-10-15 follow-up:** with space restored, `IllustratorContourTests.cpp` was added to `tests/native-repaint/run.py` and passes its 14 checks under MSVC (/W4 /WX), alongside the existing suites (33 refusal, 65,585 stroke-behind, 43 cross-frame, 299 miter, 51 settings). `build_region_native.py` rebuilt region-prep-12 from the same source hashes and passed the same 70, 2,879, and 13,229 assertions; builds are not byte-reproducible, and the new candidate is `553b30b88b454f8070ed7233e3fb7603630c74c055095c93b1c5ea58ca858a39`. Gate 1 below is therefore met.

**2026-10-15 rebaseline (gate 2 met):** the four host probes on record for region-prep-11 (stroke-behind, region lifecycle, appearance, and region discriminator) ran on Illustrator 30.8.2, each in a fresh, quietly launched process (*tools/run-ai-fresh.ps1*). Every status field, gate, processing census, vertex deviation, and bound matches the 30.7.0 record, and the 600 dpi MuPDF measures are unchanged: fill-core damage 55,568 → 0 under stroke-behind, identical to the outlined control away from the fills, and 72,080 → 0 under the Appearance route, with the PDF text kept. region-prep-12 was then frozen, installed in place of region-prep-11 (kept frozen as *build/region-prep11-frozen.aip*), and reproduced every field and measure except its build name. Record: *tests/audit/results/illustrator-region-rebaseline-30.8.2.json*.

The run found one host change: since Illustrator 2025 (29.8.11) was installed beside 2026, the ProgID `Illustrator.Application` names 2025. A running 2026 is not found under it, and activating it starts a second Illustrator, 2025. Both Illustrator runners now attach with `Illustrator.Application.30`; *plugin/illustrator-probe/capture-region.ps1* still uses the old name and `Start-Process`, and is kept only as the record of its 2026-09 runs.

## Owner decisions and the text-kept effect (2026-10-15)

The owner ruled: uniform text strokes are left to Illustrator for now (its own object-level Appearance stroke beneath the characters), until findings show the plugin should take them too; and effects must keep text live and editable, in the document and in every export, unless the user outlines it beforehand.

The second ruling was out of reach under claim 8, which turned out to be narrower than recorded. A control (*tools/jsx/ai-text-pdf-control2.jsx*) exported the same text under five host effects: only Offset Path, which declares path input, wrote outlines; Drop Shadow, Transform, and Outer Glow kept the PDF text and its font. Illustrator converts text to outlines only for an effect that cannot take text.

region-prep-13 adds `org.strokemaster.proposed.stroke-behind-text.v1` ([TextBehind.h](../plugin/illustrator-probe/TextBehind.h)), which declares text-frame input. Illustrator hands it one copy of the text frame per appearance row (object stroke, Characters, object fill); for each, it puts a stroke-only outline copy beneath and a copy of the frame above with its character strokes cleared (the originals are disposed only once every frame is staged, so a failure leaves the input as it was) through ATE `ReplaceOrAddLocalCharFeatures` (`SetLocalCharFeatures` replaced every override and dropped the text to 12 pt default type). On the mixed-stroke fixture, on Illustrator 30.8.2:

- fill-core damage 55,568 → 0, with the PDF text and font kept, identical to the outlined stroke-behind control away from the fills (130 pixels differ at fill edges: text against outline rasterization);
- it republishes after a live edit (PDF text follows), undo, and save, and after a fresh-process reopen;
- without the plugin the corrected result cached in the file exports unchanged, but an edit made without the plugin is not drawn: the document reads *HH/HHH* and the PDF *HH/HH*. That is a limit of every live effect, not of this one.

Record: *tests/audit/results/illustrator-text-kept-stroke-behind-2026-10-15.json*. Not yet covered: the miter and overlap geometry of step 3 (the strokes are still Illustrator's own outline strokes), text with an object-level stroke or fill in its appearance, area and path text, threaded frames, other fonts, and EPS and SVG exports. *TextBehindTests.cpp* checks the transform against an in-memory art tree with every SDK call failing once in turn (9,255 assertions, 186 runs): a failure leaves the input untouched, or, at the final disposal of the originals, reports a failed rollback. An independent read (on the API credit) found the in-place version's rollback defect that the staged copies fix; the installed build is region-prep-13 `71d273e3`.

## The native route (2026-10-15, later)

The owner then asked for corrections that follow every edit, as InDesign's do, the remaining fixtures tested, and parity with InDesign. The answer is the native route: the corrections written as ordinary Illustrator attributes (an object-level fill above or stroke below *Characters*, miter limits chosen per character and per path by InDesign's own rulings, and fill above stroke for Outside strokes on overlapping contours). It needs no plugin to draw, follows edits, keeps text in every export, and on a 71-row port of the InDesign coverage census removes the stray contour edges, the beveled whole points, and the escaping glyph miters. What it cannot express (text with both fills and strokes mixed, Inside strokes on overlapping contours, dashed points) is listed there: [ILLUSTRATOR_NATIVE_ROUTE_2026-10-15.md](ILLUSTRATOR_NATIVE_ROUTE_2026-10-15.md).

## Transfer map

| Source | Reuse in Illustrator | Remaining integration |
| --- | --- | --- |
| `IseMiterCore.h` | Direct geometry reuse; the new adapter produces its contour type | Read real path and glyph outlines; match winding, joins, limits, and alignment to Illustrator's Appearance and PDF output |
| `IseRegionCore.{h,cpp}` and `IseClipper2.cpp` | Compiled and linked by region-prep-12; the old fixture output still uses the historical core | Resolve overlapping contours and real glyph curves, then emit bounded path art transactionally with readback and rollback |
| `IsePatternCore.h` | Portable length, corner, dash, and wave algorithms | Illustrator brushes, dashes, and variable-width profiles have different semantics; admit one tested style at a time |
| `IseStrokeBehindCore.h` | Portable paint-order model | The current Illustrator effect has its own host-facing `StrokeBehind.h`; reconcile the model and host tree only after per-paint evidence |
| `IsePageItemStroke.cpp` `Prepare` and `PrepareWord` | Their contour planning and tolerance rules can be extracted after host-neutral inputs are defined | Replace InDesign's `IOutlineInfo`, stroker, graphics-port, swatch, transparency, and page-item calls with Illustrator suite adapters; preserve explicit refusal gates |
| `IseCrossFrame*`, `IseStripeProbe.h`, adornments, and `IseSettings.h` | Little direct source reuse | Stories, native strokers, draw priorities, and panel settings are host-specific |

The first useful Illustrator product split remains: use a native object-level Appearance stroke for uniform text when it preserves editable PDF text; use the live effect for mixed per-character strokes only with an explicit PDF-outline and missing-plugin contract. The existing records prove those behaviors only on bounded fixtures.

## Next Illustrator gates

1. **Met 2026-10-15.** Restore build headroom, rerun the new contour CMake test and the region-probe build, and compare source and artifact identities. Do not replace the installed AIP as a side effect of compilation.
2. **Met 2026-10-15.** Rebaseline on Illustrator 30.8.2 with fresh-process controls and the documented `ai-stroke-behind-proof.jsx` and `ai-region-lifecycle.jsx` specimens. Preserve other Adobe work and use the host coordination and quiet-launch procedures.
3. Feed actual outlined glyph contours through the new adapter and shared region core. Test nonzero/even-odd rules, touching contours, curves, transforms, miter limits, undo, save/reopen, screen, PDF, and missing-plugin edits. The current SDK output backend accepts only one shell and one hole, with straight segments; general curved multi-component output needs its own bounded ownership and rollback tests.
4. Port page-item strokes and only then pattern/Appearance families, refusing brushes and variable-width profiles until a measured model exists.

## InDesign-specific work after the shared port

The certified InDesign 21.6.0.57 Route X engine already handles the measured story overlap, miter, pattern, opacity, and vertical-text cases. Its remaining work is distinct from shared geometry:

- **Live candidate acceptance:** the newer panel is installed and hash-verified but needs a live visual and resize check; the metadata-sanitized package copies need a host load and export check before they inherit C32 behavior.
- **Version and distribution:** other host builds and InDesign Server are unmeasured. An Adobe-assigned prefix, rebuild, signing, and clean-machine install/update/restore/ remove checks remain release gates.
- **Story policy:** independent stories intentionally keep host stacking; the experimental across-stories switch can remove a deliberate halo. A product policy and separate-story corpus would precede making that behavior broader.
- **Host-owned effects:** arrowheads, stroke-specific drop shadows, vertical text, and certain open-path strokes remain native by design. The anchored-object group bound, rare doubled gap-color refusal, and slight inward-band differences are recorded limitations, not evidence of a general geometry regression.
- **Script workflows:** split/resync and Create Outlines have narrower supported envelopes than Route X; broader clipboard, arbitrary resync, undo, and fidelity cases need separate host transactions if those workflows are to become release features.

Sources: `docs/RELEASE_CANDIDATE_CHECKS_2026-10-06.md`, `docs/INDESIGN_LATEST_UI_CANDIDATE.md`, `docs/INDESIGN_RELEASE_CANDIDATE.md`, `OPEN_QUESTIONS.md`, `plugin/source/IsePageItemStroke.cpp`, and `.workspace/handoffs/STROKEMASTER-ILLUSTRATOR.md` in the parent workspace.
