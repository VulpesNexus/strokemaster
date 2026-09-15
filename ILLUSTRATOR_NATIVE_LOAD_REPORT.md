# Illustrator native load experiments

**Current host-validated build: native-prep-5**, frozen SHA-256 `201dd19cb25174c7fb9628fd1bf2769aa41e7d4dd4f2c6b77e5e9d7aea0fc9c7`. Its hardened lifecycle on native PID 5324 passed all 332 assertions (27 failed-callback interval checks), with Go/mutations/accepted inputs each 3 and latest error 0. AI save/reopen, source edits, and PDF export passed. Direct COM `Application.Quit` then observed the same PID/start exit after zero documents, in a 4.021051 s recorded attempt. Nested shutdown was exercised only by mocks. This remains a bounded straight-line snapshot result, with no broad stability or corner proof. Portable lifecycle evidence includes both prep-4 history and this prep-5 run.

**native-prep-4 reached `LIFECYCLE_OBSERVED_ONLY` on Illustrator 30.7.0 / 114R.** The frozen binary, SHA-256 `45fab742d70bfb12497b3735a1e3c9d4b14dd8dc9195551f0221f1c4297b1c78`, completed the straight-line control/effect test, AI save/reopen, a subsequent source edit, and PDF export. Portable evidence is [*illustrator-native-lifecycle.json*](tests/audit/results/illustrator-native-lifecycle.json). This is an open, straight, two-anchor **SourceParameterSnapshot** result, not proof of live synchronization of arbitrary stroke styles, topology-aware miter clipping, or missing-plugin rendering.

The actual native PID was **64996**, returned by diagnostics schema 2 and matched to all five external runner snapshots with the same process start time. The runner remained attached to that process throughout the test. The harness's 230 passing assertions include repeated protocol parsing; they are not 230 independent test cases. The successful Go calls correspond to initial effect rendering, the first endpoint edit, and an edit after AI reopen. Go calls, successful replacements, and accepted inputs all ended at **3**, with latest error **0** and stroke origin **3 — parameters**. Reopen redraw itself did not add a callback; the subsequent edit did.

## Second process and later shutdown evidence

A second fresh specimen run in *build/host-census/ai-native-4-after-manual/fresh-host.json* also returned `LIFECYCLE_OBSERVED_ONLY`. Native PID **78916** matched all five runner snapshots; the guard found that process already running, so its launch is not attributed to the supervisor. The run recorded 231 passing assertions, three Go calls, three replacements, three accepted inputs, origin 3, and zero latest error. It repeated AI save/reopen and endpoint reevaluation with newly generated fixtures. This is a second-process repetition, not a cross-process reopening of the first run's files.

Unlike the first run, this run includes one passing preexisting-document check. The harness checked name, saved flag, and path-item count, restored document count and interaction level, and did not close that document. It did not compare all document content. The portable evidence includes both runs without document names or machine paths.

The recovery chronology is separate: an earlier uninstrumented quit had no recovery discard or recorded target PID. Later, the user authorized recovery discard on PID 48928; zero documents were verified, but the JavaScript quit request left that process alive after 30 seconds and controller exit. Manual closure was subsequently reported. Those earlier attempts did not prove clean shutdown. A later direct-COM quit observed PID 38016 exit, as recorded below; this does not explain the earlier non-exit or establish overall stability.

## Stage ledger

| Stage | Evidence | Result and scope |
| --- | --- | --- |
| BUILDS | Native mocks, PiPL, PE, and frozen hash verification | Proven for frozen prep-4 |
| HOST DISCOVERS / LOADS | Exact plugin/build bridge response and matching actual PID | Loading proven; discovery timing not separately measured |
| INITIALIZES / REGISTERS | Checked diagnostics state and registered effect/command handles | Proven in this run |
| CALLBACK EXECUTES / ACCEPTS INPUT | Three successful replacements; parameter stroke origin | Proven for the two-anchor specimen |
| AI SAVE / REOPEN | Both control and effect reopened with exact editable source geometry; effect edit dispatched afterward | Proven within the same host process |
| PDF OUTPUT | Control remained an 8 pt stroked line; effect PDF contained one filled rectangle | Bounded geometry evidence; no general final-render certification |
| OWNED DOCUMENT CLEANUP | Harness restored document count and interaction level | Proven for its specimens; no per-existing-document assertion appears in this record |
| QUIT REQUEST / TARGET EXIT | Direct COM `Application.Quit`, zero documents, PID 38016 and process start matched the hardened lifecycle; quit returned and target exited in a 3.883 s recorded interval | PID-attributed exit observed for this route and session; earlier non-exit unexplained |
| GLOBAL HOST STABILITY | A later PID 48928 appeared without a supervisor launch | Unresolved; P0 gate remains open |

The original user document's preservation was reported separately by the supervisor; the lifecycle record itself has no per-existing-document checks. Later inspection returned an empty document list. No recovery discard is established by that first specimen run. The later authorized discard is recorded separately above; no arbitrary document closure follows from specimen cleanup.

## Export and persistence evidence

Both AI files were saved with PDF compatibility, closed, and reopened in the same process. The reopened effect retained two open anchors and exactly matched the saved geometry. Its subsequent endpoint edit caused a fresh successful replacement using the stored stroke parameters. This proves bounded attachment-parameter usability after same-process file reopen, not cross-process reopening or automatic style resynchronization.

Independent supervisor MuPDF extraction in the local *pdf-geometry.json* reports one ordinary control stroke, width 8 pt, from PDF coordinates (100, 160) to (220, 160), and one effect fill rectangle with bounds (100, 196)–(260, 204). Gray values match. The effect's x extent agrees with the two +20 pt endpoint edits; its height is 8 pt. There is no second stroked effect path in that extraction. The portable summary records the artifact byte lengths and hashes, without local paths, document names, fonts, or outline payloads. Missing-plugin fallback, arbitrary Appearance, transparency, transformed strokes, and full rendering remain unverified.

## Earlier failures and correction

| Build / experiment | Measured outcome | Interpretation |
| --- | --- | --- |
| prep-2, `4eb883…0b5fc` | One Go call, zero replacements, origin zero, `PARM`; ordinary control exported | Loaded and dispatched; failing gate initially unknown |
| prep-3, `3cbb7f…be796` | Named rejection `stroke.keys-missing`; two-anchor unfilled path; all five Go suites acquired | Both dictionaries existed, but supplied no stroke entry |
| prep-4 intermediate, `4d7385…46d76a` | Build before native PID/schema change; install hash guard rejected the mutable-target mismatch | Superseded; not the tested deployment snapshot |
| prep-4 frozen, `45fab742…7b1c78` | Typed captured stroke parameter plus native PID; successful lifecycle result | Tested snapshot; retain immutable deployment path |

Prep-4 captures the actual simple native stroke before merging and stores it under the documented `StrokeStyle` parameter key. `FromStrokeStyle` creates an entry reference consumed by dictionary `Set`; `Get` creates a reference consumed by `ToStrokeStyle`. Missing keys still fail closed. No processing-style guess or automatic merge population is assumed. Detailed SDK citations and ownership review are in [*PREPARATION_AUDIT.md*](plugin/illustrator-probe/PREPARATION_AUDIT.md).

## Host-exit observations

Windows recorded Illustrator access violations before and after the first diagnostic installation. The 00:16:00 and 00:38:17 pre-install events shared offset `0x18162a7` with the later 00:58:07 event; the 00:52:20 event used `0x849dee`. These are crashes, not plugin attribution. Earlier launcher/PID mismatches and unavailable COM attachments cannot be treated as evidence of this later successful run.

After the first lifecycle run, an empty-document helper returned normally without recording its target PID. Exit of PID 64996 remains unverified. PID 48928 later appeared without a supervisor launch and remained after a subsequent JavaScript quit attempt. The newer direct-COM result proves exit of PID 38016 only. No overall host-stability, earlier-crash attribution, or explanation of the intervening processes follows.

## Preservation and deployment

Baseline commit: `c4596837a5e0ea10a68f7b94e2dd368032341334`. The saved benchmark and its local preservation copy have SHA-256 `7ced363138cca7a1a75f2cfc1cf66acca09f89c4ca6cbe3d5c5d5f77ce2485ee`. No font program or proprietary outlines are redistributed. The hash-verified prep-4 deployment source is *plugin/illustrator-probe/build/native-prep4-frozen.aip*, 52,224 bytes. Installation evidence is separate from host execution evidence and remains in ignored local build records. No rebuild was performed while preparing this report.

## Smallest next adapter, proposed only

First resolve and independently verify the topology kernel's P1 protected-region-loss and P2 distant-envelope-precision findings, then freeze its reusable API and evidence. The current kernel returns a filled region containing the source fill; that result is not directly interchangeable with this hook's stroke-only contribution.

The approved adapter scaffold keeps the straight-line hook unchanged and reserves a separate identity for one generated closed linear shell with one explicit analytic triangular hole and equal gray fill/stroke. Call the reviewed kernel API in process, without launching the research executable or reimplementing its Boolean geometry. Stage and validate the complete returned single contour in memory, then create only new owned filled diagnostic art with rollback on SDK failure. Compare coordinates, area, envelope containment, and PDF paint type against the standalone oracle. Allow the one explicit analytic hole; reject additional holes, multiple output components, unresolved fill rules, curves, mixed paint, and arbitrary selected artwork in this first scaffold.

The separate closed-region design uses a post-effect on a neutral generated group, with the editable compound as its child and equal gray fill/stroke. Candidate real-core tests pass 104 assertions for the benign triangle and the corrected left-facing spike triangle. A separate SDK-declaration target passes 1,404 mocked assertions, including 52 stage/commit failures. These are isolated tests, not native region execution. Whole-object registration, source/provenance validation, and actual host compound output remain integration gates. See *plugin/illustrator-probe/TOPOLOGY_ADAPTER_PLAN.md*; the broader kernel review remains independent.

## Hardened third run and measured COM exit

The hardened prep-4 harness passed all 332 assertions on native PID 38016, including 27 failed-callback interval assertions. Control dispatch stayed at zero; effect Go/mutations/accepted inputs each reached 3 with latest error 0. Same-process AI save/reopen, source edits, and PDF export passed again. The native PID and process start match all five external snapshots. Raw local evidence is *build/host-census/ai-native-4-hardened/guarded-host.json* and its process/stage journals. This is another bounded straight-line snapshot proof.

A subsequent direct COM `Application.Quit` call recorded zero documents, returned successfully, and observed that same target PID 38016 exit. The full recorded attempt lasted 3.882651 seconds. *build/host-census/ai-prep4-com-quit.json* records the PID/start and exit; the preceding lifecycle establishes its native PID. Portable evidence includes sanitized fields and all source hashes. This establishes normal exit through this COM route in this session, without attributing the earlier JavaScript quit behavior or clearing general stability concerns.

Prep-5 is a separate compile-only lifetime fix, frozen at *plugin/illustrator-probe/build/native-prep5-frozen.aip*, SHA-256 `201dd19cb25174c7fb9628fd1bf2769aa41e7d4dd4f2c6b77e5e9d7aea0fc9c7`. It passes nine nested-shutdown mock scenarios plus the existing native suite. Review details are in *plugin/illustrator-probe/PREP5_LIFETIME_AUDIT.md*. Prep-4 remains the host-evidence artifact; no region adapter is linked into prep-5.
