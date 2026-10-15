# Illustrator portability and InDesign-specific gaps (2026-10-14)

## Current boundary

The last recorded installed Illustrator probe is the region-prep-11 diagnostic, measured on Illustrator 30.7.0; its current installed hash was not rechecked. The Illustrator executable now reports **30.8.2**. No host load, document mutation, or visual test was run in this pass. The source build here is **region-prep-12**, a compile and mock candidate only; it still applies the older topology experiment solely to its marked analytic fixture. It does not expose a general stroke correction to users.

The Illustrator project now compiles and links `IseRegionCore.cpp` and `IseClipper2.cpp` alongside its existing topology experiment. `IseMiterCore.h` is used by a new path adapter. `IllustratorSdkContour.h` reads Illustrator's anchor, incoming handle, and outgoing handle into the shared `ise_miter::Contour` representation. It retains the last-to-first cubic of a closed path and rejects nonfinite or out-of-range coordinates before changing its output. The marked-fixture source reader uses that admission check. This is a source integration, not evidence that Illustrator produces correct glyph art.

The final **complete** hidden build passed 70 registration, bridge, and refusal mock assertions, 2,879 output-backend assertions, and 13,229 stroke-behind assertions; its candidate artifact hash was `1a56e1a90083e38d197a44fb14ade46283884d3c045bfbd33909ec8a078391d7`. The later attempt to link an additional standalone contour test stalled. A narrower attempt to link the real region core into the mock executable failed with **No space left on device** while writing an object. The project was returned to the passing mock-target structure and rebuilt successfully.

**2026-10-15 follow-up:** with space restored, `IllustratorContourTests.cpp` was added to `tests/native-repaint/run.py` and passes its 14 checks under MSVC (/W4 /WX), alongside the existing suites (33 refusal, 65,585 stroke-behind, 43 cross-frame, 299 miter, 51 settings). `build_region_native.py` rebuilt region-prep-12 from the same source hashes and passed the same 70, 2,879, and 13,229 assertions; builds are not byte-reproducible, and the new candidate is `553b30b88b454f8070ed7233e3fb7603630c74c055095c93b1c5ea58ca858a39`. Gate 1 below is therefore met; nothing was installed.

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

1. Restore build headroom, rerun the new contour CMake test and the region-probe build, and compare source and artifact identities. Do not replace the installed AIP as a side effect of compilation.
2. Rebaseline on Illustrator 30.8.2 with fresh-process controls and the documented `ai-stroke-behind-proof.jsx` and `ai-region-lifecycle.jsx` specimens. Preserve other Adobe work and use the host coordination and quiet-launch procedures.
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
