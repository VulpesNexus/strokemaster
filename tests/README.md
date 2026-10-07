# StrokeMaster validation

Run from the repository root with *requirements.txt* installed in *.venv/* and Node 24 available. The geometry suite requires both the standalone C++ topology executable and its direct API caller. Their build entry points are *tools/build_topology.py* and *tests/geometry/build_core_api.py*, with explicit Boost-header, MSVC, and Windows SDK directories; the tested experiment uses external Boost.Geometry 1.74. Preserve the reviewed frozen artifacts; a rebuild creates a new identity that needs verification. Missing compilation is an explicit failure, not silently skipped coverage. The following help commands describe the required paths:

```powershell
.\.venv\Scripts\python.exe -B tools/build_topology.py --help
.\.venv\Scripts\python.exe -B tests/geometry/build_core_api.py --help
```

After verifying those dependencies, these commands exercise geometry, evidence parsing, and real JSX against mocks. They do not launch Adobe:

```powershell
.\.venv\Scripts\python.exe -B -m unittest discover -s tests/geometry -v
.\.venv\Scripts\python.exe -B -m unittest discover -s tests/audit -v
node --test --test-isolation=none tests/scripts/*.test.cjs plugin/illustrator-probe/host-lifecycle.test.cjs
```

The geometry tests use an independently derived half-plane oracle and analytic region counterexamples. They explicitly fail unsupported local cusps or invalid numbers. Passing them does not establish a topology-safe font stroker. Embedded-font provenance tests require FontTools and pikepdf; missing dependencies must be reported as unexecuted coverage, not a font pass.

The script tests cover output conflicts, preference snapshots, repeated entry-point invocation, pair identity, scoped synchronization, staged outlining, and diagnostic result reporting. Their host objects are mocks. Host applicability, rendering, undo, and persistence need separate evidence.

The September 13 run of the exact Node selection above records **308 tests passed, zero failures, and zero skips** in [the retained log](../build/host-census/node-final-current.log). This count excludes a future region harness. The [standalone topology review](geometry/TOPOLOGY_REVIEW.md) records the final frozen kernel, 43 API checks, and 80 independently validated rectangle unions; earlier 29-test prototype counts describe only a historical subset.

## Executed host evidence

*audit/results/* contains bounded JSON observations from the takeover. Raw logs and large local benchmark artifacts remain under the ignored *build/takeover/* directory. Proprietary font programs and SDK files are not part of this evidence bundle. Hashes identify inputs and local artifacts; a hash alone does not replace the missing input or prove its semantics.

| Probe | Executed host / result | Important limit |
| --- | --- | --- |
| *ai-probe-outline-dom.jsx* | Illustrator 30.7.0, build 114R; 17 checks | Four outline-access routes and cleanup, not full Appearance |
| *ai-probe-regions.jsx* | Same Illustrator; ten checks | Basic Unite and compound construction, not arbitrary topology |
| *probe-takeover-safety.jsx* | InDesign 21.5.1.73; eleven checks | Readable preference values verified; 15 values unverified across PDF/EPS |
| *probe-takeover-sync.jsx* | Same InDesign; 16 checks | Document-scoped direct tick, not idle-event timing |
| *probe-takeover-outline.jsx* | Same InDesign; 13 checks | Staging, cleanup, paint attributes, and placement; broader rendering remains open |
| *probe-takeover-faults.jsx* | InDesign 21.6.0.57; 58 checks | Split and resync refuse every unsupported constraint and appearance, on either frame, before changing anything. Failures midway cannot be injected in the host (page item methods are read only); the mocks cover them |
| *probe-fill-mask.jsx* | Same InDesign; 46 word positions unchanged | Independent coverage mask for the 23 historical transparency samples |
| *probe-unstroked-victim.jsx* | Same InDesign; V0 fails native coverage, V1/V2 controls hold | A reproduced defect, not a passing plugin release gate |
| *probe-zero-stroke.jsx* | Same InDesign; four text-access routes | None alone retains weight; explicit zero plus None works |
| *probe-outline-paint.jsx* | Same InDesign; four swatch cases | Paper page-item overprint properties are inapplicable |

Host probes use *tools/run-id-probe.ps1* or *tools/run-ai-probe.ps1* through Windows PowerShell 5.1. They require the intended application already running; InDesign's fallback activation is limited to one existing version-21 process. Probe output must contain `passed: true` for the runner to succeed. A successful exploratory census means its collection/checks completed, not that the measured product behavior is correct.

The current Illustrator native-prep-5 record identifies binary `201dd19c…`, host 30.7.0 build 114R, and native PID 5324. All 332 harness assertions pass, including 27 failed-callback interval checks; Go calls, mutations, and accepted inputs each reach 3 with latest error 0. Source edits, same-process AI save/reopen, and PDF export pass. Direct COM `Application.Quit` subsequently observes the same PID/start exit with zero documents in a 4.021051 s recorded attempt. Nested shutdown is tested only by mocks; full stability and native region processing remain unverified. [Portable lifecycle evidence](audit/results/illustrator-native-lifecycle.json) retains prep-4 history separately; [the load report](../ILLUSTRATOR_NATIVE_LOAD_REPORT.md) supplies its context.

The new InDesign census records 20 mixed-stroke, zero/None, color, decoration, transform, and benign-control cases. The old overlay fails no-harm; the current native refusal binary adds no paint and produces byte-identical PNGs to the absent-plugin control. It does not fix the original overlap. Separate hardened host probes check 20-frame native PNG/PDF persistence and two split-pair PNG round-trips through INDD/IDML. The final pair harness correctly expects lower fill to match source stroke color and tint, includes fillTint among sampled fields, and preserves the earlier image hash; this was a harness correction, not a product change. Read the [support matrix](../HOST_SUPPORT_MATRIX.md) for exact versions, routes, and unexecuted cases.

The native refusal test compiles the actual implementation against capability-free stubs with the installed MSVC toolchain:

```powershell
.\.venv\Scripts\python.exe tests/native-repaint/run.py
```

Its passing checks establish refusal behavior against mocks. Actual host no-harm comes from the separate matched-render records. Topology tests likewise certify only their explicit linear-region contract; their native font integration is still pending.

Native load-route testing must respect the no-UAC constraint. The SDK-documented per-user folder is present and empty, but host loading from it remains UNEXECUTED. [Deployment evidence](../fixtures/FIXTURE_INDEX.md#per-user-load-route).
