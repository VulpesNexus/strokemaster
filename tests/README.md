# StrokeMaster validation

Run from the repository root with *requirements.txt* installed in *.venv/* and Node 24 available. These commands exercise geometry, evidence parsing, and the real JSX against mocks; they do not launch Adobe applications:

```powershell
.\.venv\Scripts\python.exe -m unittest discover -s tests/geometry -v
.\.venv\Scripts\python.exe -m unittest discover -s tests/audit -v
node --test --test-isolation=none tests/scripts/*.test.cjs
```

The geometry tests use an independently derived half-plane oracle and analytic region counterexamples. They explicitly fail unsupported local cusps or invalid numbers. Passing them does not establish a topology-safe font stroker. Embedded-font provenance tests require FontTools and pikepdf; missing dependencies must be reported as unexecuted coverage, not a font pass.

The script tests cover output conflicts, preference snapshots, repeated entry-point invocation, pair identity, scoped synchronization, staged outlining, and diagnostic result reporting. Their host objects are mocks. Host applicability, rendering, undo, and persistence need separate evidence.

## Executed host evidence

*audit/results/* contains bounded JSON observations from the takeover. Raw logs and large local benchmark artifacts remain under the ignored *build/takeover/* directory. Proprietary font programs and SDK files are not part of this evidence bundle. Hashes identify inputs and local artifacts; a hash alone does not replace the missing input or prove its semantics.

| Probe | Executed host / result | Important limit |
| --- | --- | --- |
| *ai-probe-outline-dom.jsx* | Illustrator 30.7.0, build 114R; 17 checks | Four outline-access routes and cleanup, not full Appearance |
| *ai-probe-regions.jsx* | Same Illustrator; ten checks | Basic Unite and compound construction, not arbitrary topology |
| *probe-takeover-safety.jsx* | InDesign 21.5.1.73; eleven checks | Readable preference values verified; 15 values unverified across PDF/EPS |
| *probe-takeover-sync.jsx* | Same InDesign; 16 checks | Document-scoped direct tick, not idle-event timing |
| *probe-takeover-outline.jsx* | Same InDesign; 13 checks | Staging, cleanup, paint attributes, and placement; broader rendering remains open |
| *probe-fill-mask.jsx* | Same InDesign; 46 word positions unchanged | Independent coverage mask for the 23 historical transparency samples |
| *probe-unstroked-victim.jsx* | Same InDesign; V0 fails native coverage, V1/V2 controls hold | A reproduced defect, not a passing plugin release gate |
| *probe-zero-stroke.jsx* | Same InDesign; four text-access routes | None alone retains weight; explicit zero plus None works |
| *probe-outline-paint.jsx* | Same InDesign; four swatch cases | Paper page-item overprint properties are inapplicable |

Host probes use *tools/run-id-probe.ps1* or *tools/run-ai-probe.ps1* through Windows PowerShell 5.1. They require the intended application already running; InDesign's fallback activation is limited to one existing version-21 process. Probe output must contain `passed: true` for the runner to succeed. A successful exploratory census means its collection/checks completed, not that the measured product behavior is correct.

The Illustrator native diagnostic is compiled and mock-tested only. Its scripted host proof must wait until the user's benchmark is saved and the diagnostic is loaded through an Illustrator restart. It is a straight-line ownership/dispatch test, not the miter correction. Release gates remain in [the takeover audit](../PROJECT_TAKEOVER_AUDIT.md) and [open questions](../OPEN_QUESTIONS.md).
