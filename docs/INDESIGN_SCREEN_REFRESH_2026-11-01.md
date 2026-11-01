# InDesign screen refresh candidate, November 1, 2026

The earlier results below are retained as history. Current closeout results are recorded in [the November 2 review](REVIEW_2026-11-02.md). Source publication does not promote a binary or extend the older certification to GPU rendering.

## Expanded host results, November 2

The combined receipt is *build/host-final-2026-11-02/candidate-1/measurement.json*. It retains `focusedPassed=true`, `undoRedoGeometryConfirmed=true`, and `storyScopePassed=true`, but correctly reports `extendedScenesPassed=false` and overall `passed=false`.

| Check | Result |
| --- | --- |
| Ten alternating frame moves, edit, restore, separate, return | Zero enclosed white pixels with Close holes enabled |
| Document undo and redo | Frame coordinates return to separated and joined positions respectively; captures contain no stale cavity |
| Nested group, child movement, outer group movement | Zero enclosed white pixels; corresponding PDF closed |
| Two layout windows | Both inspected windows closed after movement and switching; this does not prove repaint of an inactive, uninspected window |
| Story mode, separate stories | Shared cavity remains open, as required |
| Story mode, linked two-parcel story | Shared cavity closes on screen and in PDF, including after movement |
| Ordinary applied parent artwork | Present before and after movement; no cavity regression |
| Intervening artwork | Obscured glyph excluded (`covered=1`), preserving artwork stacking |
| Remove intervening artwork | **FAIL:** 17,190 enclosed white screen pixels and 35,351 black pixels remain, versus 37,188 black pixels in the corrected reference; PDF is closed and a later frame move repairs the screen |

The focused removal reproduction is *invalidation-diag/* under the same build evidence directory. Research-only entry traces were placed before eligibility checks for both after-spread and after-layer invalidation callbacks. The reproduction produced ordinary drawing traces but no entry for either invalidation callback. Relaxing the existing screen-port gate alone therefore has no demonstrated way to repair this edit path. A supported change notification or dependency invalidation path covering ordinary item removal must be established and regression-tested before promotion. Temporary diagnostic subscriptions and traces were removed afterward, and the research variant rebuilt successfully.

The diagnostic engine was `9a990f39860747b57aa27c2c84a7a22cd781bd17f3edba78b52c1fc3baa1ffcb`; the main host matrix used `2c743b4a326b56a80d2e7354b188839ce34e894a69daa25b1636d01a2286bd10`. The final rebuild without diagnostics is `a9529f4cd5f1cbdeaedd199eb04bd9aa0e5e7ff169764a8577ea40bdffab3120` and was not installed. Host jobs peaked at 3.04–3.06 GB under the 4 GB cap with the unchanged 4 GB reserve.

Harness corrections were required: InDesign groups use `constructor.name`, window creation uses the [Windows collection](https://developer.adobe.com/indesign/uxp/dom/api/w/windows/), and undo/redo must address the [Document's stack](https://developer.adobe.com/indesign/uxp/dom/api/d/document/). Application undo affected the global GPU-preference command rather than document geometry. The five-argument COM call also requires an explicit missing optional argument instead of null. Failed harness attempts remain recorded and are not counted as product failures.

GPU capture, cross-parent text correction, inactive-window invalidation delivery, and host-scale enumeration truncation remain uncertified. Cross-parent text is explicitly skipped by the current snapshot traversal; ordinary parent artwork passing is not evidence for that capability. Native bounded-enumeration checks remain separate from those host limits.

## Measured retry result

The previous engine starts successfully under a 4 GB job cap and peaks at 2.88 GB. The earlier 2.5–2.6 GB limits were therefore insufficient even for the baseline. The corrected candidate's final CPU-preview run peaked at 2.97 GB. All runs retained the 4 GB free-memory reserve and used an isolated desktop, private settings, and workspace restoration.

Host diagnostics exposed a real candidate defect: `ScreenOrder` queried `IWaxStrand` on the text-model boss, where that interface was absent. Every snapshot consequently held zero neighbor records. Adobe's SDK identifies the frame-list boss as the wax strand. Querying `IWaxStrand` from the existing counted `IFrameList` fixes the lookup; an independent review confirmed ownership and lifetime. Temporary diagnostic statements were removed.

The executed fixture uses explicit 400 pt page dimensions, CPU preview, glyph-bound placement, and no frame paint. Stroke weight must be set before applying None; the harness records and checks the resulting frame style. GPU preview captures omitted the page and were rejected as evidence. The final receipt is *build/resume-2026-11-01/screen-fixed-clean/measurement.json*:

| Check | Result |
| --- | --- |
| Close holes off | One cavity: 17,634 screen pixels; 1,769.242 pt² in PDF |
| Corrected screen path, initial and ten alternating moves | Zero enclosed white pixels throughout |
| Separate, return, edit U to C, restore U | Zero enclosed white pixels; no stale closed cavity |
| Glyph fills | Initial black area matches the off control; all ten moves stay within 1% |
| Screen path disabled, after moving | Old defect reproduced: 17,190 enclosed white pixels |
| PDF with Close holes enabled, including screen-disabled control | Zero enclosed white area |
| Undo/redo | Commands returned, but frame geometry did not change; not certified |

The visual regression flags are true, while the combined receipt deliberately reports `passed=false` because undo/redo geometry was not confirmed. No `H screen-inval` trace was observed, so the new invalidation handler and descendant-cache behavior cannot be certified from this run. The tested research binary is `0338084a4ea3cf2e2fb0e59524142faaafd28de9352d2abb69f60be2ec44312e`, retained in the installation backup and build snapshots. Later all-variant rebuilds have different binary hashes and were not reinstalled; source hashes are recorded separately.

## Recovered stopping point

Claude completed the Illustrator prep-51 triangle and separate-live-story fixes and host checks, then stopped while reviewing the InDesign screen-cache candidate. The older AI state and CC answers did not reflect that stopping point. This continuation did not rebuild or reinstall Illustrator.

## Changes

The screen path snapshots earlier visible text in layout order rather than relying on cached neighbors to repeat adornment callbacks. It invalidates overlapping frames and their cached ancestors after spread invalidation, remembers prior overlap participants so moving apart clears old ink, releases snapshot font references at parcel boundaries, and supplies conservative Close holes ink bounds. Print and PDF retain their traversal records.

Independent source review required three corrections before integration:

- Require non-null graphics data and an actual layout view, as well as a nonprinting, non-PDF port. Bitmap and other offscreen devices must not enter the screen snapshot path.
- Preserve previous dependencies when frame enumeration is truncated. Conservatively invalidate the spread cache instead of installing incomplete dependencies.
- Store overlap participants once, with a one-million-comparison budget. On exhaustion, invalidate all valid enumerated frames conservatively, avoiding quadratic storage and unbounded pair scanning.

The cross-frame tests include the candidate's ten cases plus three for dense-layout budget exhaustion, duplicate identities, and invalid identities. Executed results: 33 native refusal, 65,585 stroke-behind, 65 cross-frame, 335 miter, 51 Illustrator contour, and 60 settings checks passed. These do not establish host rendering behavior.

The first build logs exposed an unquoted PATH assignment in *InDesignStrokeEngine.vcxproj*: the workspace ampersand was parsed as a command separator. Quoting the assignment removed that diagnostic from all three rebuilt plugin variants. The native test runner still emits the same command-not-found diagnostic during toolchain initialization, before all six test executables compile and pass; that separate initialization warning remains unresolved.

Final build SHA-256 prefixes: Route W `54cef32ac19c`, Route X `d5497ff81793`, and Route X Research `94dd47e39eed`. Full hashes and successful exit statuses are in *build/resume-2026-11-01/validation.json*. The final complete validation peaked at 0.70 GB under a 1 GB cap, with the 4 GB reserve unchanged.

## Verification and execution boundary

Source signatures were checked against the installed InDesign 2026 SDK. Its ink-bounds callback may leave the initially empty rectangle untouched. Snapshot sources copy glyph IDs and positions and hold counted font references instead of iterator-owned wax pointers. The bounded dependency algorithm received an independent read; its final null guard was corrected afterward.

In-process readback, source hashes, and ASCII checks passed. These are not compiler or runtime evidence. A direct Node child-process request for Git status failed with `spawn EPERM` before output was produced. On the owner's follow-up, a narrow request through the supported approval mechanism succeeded: stdout, stderr, and intentional exit 17 were preserved. The approved PowerShell outer route starts Python with `UseShellExecute=false` and `CreateNoWindow=true`, then uses the isolated-desktop and heavy-run wrappers. No global sandbox setting was changed. The queued job uses a 2 GB cap and the unchanged 4 GB reserve; other sessions' workers were left alone. The earlier candidate's split-writable-root error remains historical evidence.

Previous file contents and a before/after SHA-256 manifest are in ignored *build/resume-2026-11-01/*. Five existing source/test files were integrated: *IseCrossFrame.cpp*, *IseCrossFrame.h*, *IseCrossFrameCore.h*, *IseStrokeBehindAdornment.cpp*, and *CrossFrameCoreTests.cpp*. Existing Illustrator fixes and other work were preserved.

## Earlier failed attempts

The first isolated host attempt (*screen-on-1*) exited before creating a fixture: `GetActiveObject` returned `0x800401E3`; job peak was 2.50 GB at the 2.50 GB cap. A 3 GB attempt remained queued and was canceled while childless. Subsequent attempts used a 2.6 GB cap with the unchanged 4 GB reserve. The harness was corrected to inspect the underlying COM exception, use the established two-argument `DoScript` call, and retain detailed failures. The final attempt refused COM activation when a second InDesign process appeared; no owner process was attached or altered.

Windows events for test processes 95292 and 86936 report `0xc00000fd` (stack overflow) in *ntdll.dll*. Both jobs peaked at the 2.6 GB cap. A raw pointer scan of process 95292's dump contains Character Panel, CoolType, PMRuntime, and Windows runtime addresses. It is not an unwound call stack. The later successful baseline and candidate runs establish that a larger allowance resolves the startup barrier; they do not prove the precise exception chain. The task-owned dump was moved into ignored *build/resume-2026-11-01/* with a SHA-256 manifest; event records, launch logs, and restoration receipts are there too.

## Remaining certification gates

This list records the November 1 stopping point. The subsequent observer fixes and current acceptance limits are in the [November 2 closeout review](REVIEW_2026-11-02.md).

1. Fix the reproduced stale screen after intervening-artwork removal through a supported notification/dependency path, then demonstrate repaint without another edit, export, or forced redraw. Spread and layer callbacks did not enter in the isolated reproduction.
2. Verify GPU/live-display behavior separately. Current image evidence is CPU preview captured before export through PrintWindow.
3. Verify inactive-window invalidation delivery without relying on switching windows to redraw. The inspected window-switching cases passed.
4. Resolve the unsupported cross-parent text scope and certify host-scale truncated enumeration. Ordinary parent artwork, nested groups, document undo/redo, and the tested story-scope cases no longer belong on the untested list.
5. Only after the applicable gates pass should a candidate be promoted and CC1 handed back for an owner retest. The restored previous engine remains the production installation.
