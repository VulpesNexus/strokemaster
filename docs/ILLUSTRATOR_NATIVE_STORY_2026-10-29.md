# Illustrator native story fill — October 29, 2026

> **Withdrawn, 2026-10-30.** The region-prep-21 story fill drew no strokes at all. Its group style had no explicit *Contents* entry, so Illustrator never drew the frames themselves. It drew only the group's fill, on the glyph outlines. The fill-core metric below counts only pixels inside the letters, so it read the missing strokes as zero damage. Measured on this report's own PDFs (*build/ai-story-2026-10-29/*, 600 ppi): stroke ink outside the letters is 5,569 to 8,452 pt² natively and **0 pt²** under the story fill, in every font and run. The zero-damage, reopen, and plugin-absent results below therefore do not show a correction. The story route was rebuilt in region-prep-22 with the *Contents* position set explicitly and a check of the stroke halo; see [ILLUSTRATOR_TEXT_ROUTE_2026-10-30.md](ILLUSTRATOR_TEXT_ROUTE_2026-10-30.md). The refusal controls and the reopen-diagnosis correction stand.

Region-prep-21 closes the measured same-story fill damage and duplicate PDF extraction for five synthetic fonts using a native group Appearance fill. The guarded script accepts a complete, adjacent set of linked text frames. This is a narrow research result; intervening artwork and mixed-color stories remain open engineering work.

The [portable record](../tests/audit/results/illustrator-native-story-2026-10-29.json) binds the source, scripts, binary, measurements, refusal controls, and raw evidence. The frozen and installed binary is *plugin/illustrator-probe/build/region-prep21-frozen.aip*, SHA-256 `535241e27b2dd6ebfd4225dd15026782d20b7396e3eb2a154bf12628b0107c39`. Builds are not byte-reproducible. Illustrator 30.8.2 was tested through `Illustrator.Application.30` in owned fresh processes on the isolated hidden desktop.

## Native operation

The new research bridge command `story-fill` accepts one plain group containing every frame of a single story. It writes one ordinary native fill above the group's *Contents*. Illustrator then paints all story fills above all story strokes while retaining editable text and exact PDF extraction. No custom effect, outline, raster, or separate duplicate text frame is added. Character strokes and formatting remain intact; `native-fix` can still choose character miter limits separately.

The experimental script *tools/jsx/strokemaster-fix-story.jsx* checks that the complete story's frames are editable siblings and adjacent in their parent's direct child order, groups them at that position, verifies the flattened UUID order, and requests the native fill. A native refusal restores the original frames and selection. It refuses incomplete stories, independent stories, and intervening artwork. The native command also checks group appearance, opacity, blending, text-only children, story completeness, and uniform solid gray fill/overprint. The existing *strokemaster-fix-selection.jsx* is unchanged; this route remains opt-in.

Select all linked frames before invoking the story script from *File > Scripts > Other Script*. The script supplies the group fill; it does not run `native-fix`. The positive host test invokes this same script quietly, then runs `native-fix` separately. The visible completion alert and menu invocation have not been independently exercised.

## Measured result

The same overlapping two-frame flow fixture used in the [prep-20 investigation](ILLUSTRATOR_TEXT_PARITY_2026-10-29.md) has 24 nonspace characters, local size/tracking overrides, and mixed 7 pt character strokes. Both frames must contain text. The first frame is wider for Great Vibes so a whole word fits. The metric renders PDF at 600 ppi, takes gray <32 as the fill reference, erodes it by three pixels, and counts reference core pixels at gray ≥32 in the tested export as damaged.

| Font | Prep-20 damage (pt²) | Story fill damage (pt²) | Reopened damage (pt²) | Extracted/source characters |
| --- | ---: | ---: | ---: | ---: |
| MinionPro-Regular | 64.43 | 0 | 0 | 24/24 |
| SegoeScript | 103.05 | 0 | 0 | 24/24 |
| KerberosFang | 469.63 | 0 | 0 | 24/24 |
| GermanBeauty | 181.32 | 0 | 0 | 24/24 |
| GreatVibes-Regular | 336.00 | 0 | 0 | 24/24 |

All five cases preserve the recorded character formatting, frame transforms, contents, and two-frame story membership. Corrected and reopened PDFs are pixel-identical. All five remain editable after reopen. The guarded entry point separately passes the same strict Minion measurement. The five-font runs exercise direct grouping plus the native command; only the separate Minion run exercises the guarded grouping script.

Four isolated refusal controls pass: artwork between the frames, partial selection, independent stories, and mixed character fill. Their recorded direct sibling UUIDs/order, group count, selection, character formatting, transforms, contents, and story membership remain unchanged. Mixed-fill refusal exercises grouping rollback; the other three refuse before mutation. These DOM checks do not constitute a visual comparison of every possible refusal.

With only StrokeMaster disabled, a fresh Illustrator opens the saved Minion group, confirms that its bridge is unavailable, retains the exact reopen snapshot and pixels, and appends two nonspace characters. Its edited PDF extracts exactly 26 characters and has zero damaged pixels in the 192,658-pixel edited fill core. This establishes native live editing for that fixture without the plugin. The installed binary was restored and hash-checked afterward.

## Reopen diagnosis corrected

An immediate collection read after `app.open()` can return zero text frames before Illustrator returns to idle. The revised runner opens, inspects/edits, and inspects the edit in separate COM calls with four seconds of idle time. The earlier prep-19 saved-text-loss claim is withdrawn: a fresh, phase-separated read finds two editable linked frames with 20 and 7 characters. Clearing appearance on the temporary outline copy remains isolation, not a proven repair for file corruption.

The prep-18 Minion file still contains two independent one-frame stories after idle. That confirms the destructive linked-story inspection defect repaired by outlining an owned copy. The old raw receipts are retained; the earlier report and portable record now carry the correction.

## Limits and state

This route changes the document tree by adding a native group. Nonadjacent frames remain refused so artwork between them keeps its stacking position. Mixed fills, gradients, patterns, and broader paint models need more work. The group fill is an explicit Appearance color; later character recoloring requires updating or removing that group fill. Child object effects/complex appearances, clipping parents, opacity interactions, and stories larger than the tested two frames are not certified. These are validation and engineering gaps, not proven native impossibilities.

One combined, multiple-document refusal run ended in COM RPC failure `0x800706BE`, without a completed receipt. Its cause is not isolated. All four cases pass in separate fresh hosts. The first interposed-art fixture placed its shape below both frames and incorrectly expected refusal; its raw failed receipt is retained, and the corrected fixture places the shape between them and passes. No general crash/undo robustness claim follows from these focused tests.

Build checks pass: 97 registration/bridge/refusal assertions, 2,879 SDK-output assertions, 13,229 stroke-behind assertions, and 9,255 text-behind assertions. Native command success and refusal behavior are established by the host tests above; the added command mock covers missing-suite refusal. UI/menu identity, broader fonts and exports, transformed/path roundtrips, undo/redo, timer interaction with grouped art, real documents, and release packaging remain open.

Raw owned files are in *build/ai-story-2026-10-29/*. The ordinary text-parity sweep now shares snapshot helpers and supports phase-separated reopen. *tools/run-ai-story-inspect.ps1* provides separate open/read phases for an existing owned fixture. *tools/measure_story_edit.py* checks the plugin-absent edit export. The portable record includes failed controls and stability limits alongside successes.

Illustrator is closed. Preferences match the saved pre-test file, the workspace guard has no pending snapshot, and prep-21 is installed for the next normal launch. Prep-18 and prep-20 remain frozen for rollback. Work remains uncommitted; nothing was pushed. InDesign was not driven. REA guidance was available, but this step needed source/SDK inspection and native host experiments rather than decompilation.
