# StrokeMaster

Experimental stroke scripts, native Adobe integration, and topology research. This is not a production release. The [support matrix](HOST_SUPPORT_MATRIX.md) separates current behavior, historical prototype results, and unresolved release gates.

## Current support

| Problem | Current result |
| --- | --- |
| Overlap between lines and formatting runs | The current InDesign native safety build refuses all repaint. The earlier overlay missed unstroked victims and changed benign edges and decorations. Supported split-frame cases remain the document-level alternative. |
| Miter spikes | A filled-region buffer prototype removes inward excursions in controlled polygon examples while retaining exterior miters. It is not yet a native font stroker; curves, raw font topology, and the real benchmark remain open. |
| Create Outlines | The script stages conversion before replacing supported text frames. Thirteen checks pass in InDesign 21.5.1.73; broader fidelity and lifecycle coverage remain open. |
| Editable split pairs | Explicit resync carries text and formatting while preserving a uniform Outside stroke. Contents-only watching is optional. Uniform and varied pairs preserve their rendered appearance through the tested INDD/IDML round-trip; resync still requires a uniform bottom stroke. |
| Illustrator integration | Native prep-5 passes the open straight-line diagnostic, source edits, same-process AI save/reopen, and PDF export. Region-prep-2 links the reviewed topology core into a second plugin that loads, registers, creates the marked analytic fixture, and reports over its bridge; its live effect has not executed in the host yet — the append-to-style attachment does not fire, and one proof attempt recorded an unexplained host exit. Native region correction, arbitrary style synchronization, and broad stability remain unverified. |

The measured hosts are InDesign 21.5.1.73 and Illustrator 30.7.0, build 114R, on Windows. These observations are not cross-version certification.

## Use the scripts

Copy the *tools/jsx* folder into InDesign's *Scripts Panel* folder: open *Window > Utilities > Scripts*, right-click a folder, and choose *Reveal in Explorer*. Run *strokemaster.jsx* and choose **split**, **outline**, **resync**, **watch**, **unwatch**, or **status**.

Read [the usage guide](docs/USING_STROKEMASTER.md) before choosing a correction. Split frames travel as ordinary document objects, but remain two stories. Automatic updates require the script running, and formatting-only edits require explicit resync. Unsupported backgrounds, effects, threading, and ambiguous pair labels are refused. Keep an original copy while evaluating a new revision.

The top Outside-stroke contribution is explicitly cleared to *None* and weight 0. The bottom uses the source stroke color and tint as its fill. Mixed Center-stroked ranges keep their native treatment on top; a frame with no Outside-stroked range is not split.

## Native prototype

The current InDesign binary performs no corrective drawing and leaves native output unchanged in the 20-case PNG test. It requires an InDesign restart to replace an older loaded binary. This safety change removes all earlier A/C native repaint corrections; it does not fix the original overlap. The [root-cause report](UNSTROKED_VICTIM_ROOT_CAUSE.md) explains the replacement work required. PDFs already exported with an older correction retain their recorded paths.

The prefix `0x5A200` is unassigned. Do not treat it as a release identity. An Adobe prefix allocation, Illustrator effect identity, package signing, and Creative Cloud publication are separate steps explained in [the distribution audit](docs/PLUGIN_DISTRIBUTION.md). No Marketplace package has been submitted.

Native deployment currently requires no UAC prompts or elevation. The documented Illustrator per-user plugin location has an existing empty local folder, but loading this plugin from it has not been tested. Treat it as a proposed deployment route, not a verified installation recipe. [Route evidence and remaining check](fixtures/FIXTURE_INDEX.md#per-user-load-route).

Native builds require external Adobe SDKs and Visual Studio/MSVC dependencies. The InDesign project is under *plugin/*; the Illustrator diagnostic and its build notes are under *plugin/illustrator-probe/*. Read the [native load report](ILLUSTRATOR_NATIVE_LOAD_REPORT.md) before testing it. SDK files and generated binaries are not redistributed here.

## Verify and investigate

[Validation instructions](tests/README.md) cover the executed pure suites and bounded host records. Python tools require *requirements.txt*; the script mocks require Node 24. Old fixture generators and output tables remain research evidence. Regenerate into fresh output directories and inspect restoration reports; do not overwrite the preserved corpus merely to update a measurement.

The render comparator accepts an independent fill-only mask. Without it, its eroded non-background mask includes stroke/effect ink and cannot measure glyph-fill compositing. Historical transparency interpretations based on the old mask were withdrawn and remeasured.

| Document | Purpose |
| --- | --- |
| [Usage guide](docs/USING_STROKEMASTER.md) | Workflow, supported contexts, installation, and limitations |
| [Takeover audit](PROJECT_TAKEOVER_AUDIT.md) | Initial A–N audit, claim ledger, and subsequent evidence |
| [Open questions](OPEN_QUESTIONS.md) | Remaining discriminating tests and release blockers |
| [Host census](STROKE_DEFECT_AND_LIMITATION_CENSUS.md) | Host defects, conventions, API limits, and project defects |
| [Fixture index](fixtures/FIXTURE_INDEX.md) | Named reproductions and their evidence |
| [Topology review](tests/geometry/TOPOLOGY_REVIEW.md) | Final bounded standalone approval; supersedes the frozen design’s pending-review wording |
| [Topology design](TOPOLOGY_GEOMETRY_DESIGN.md) | Frozen filled-region experiment and numerical limits |
| [Geometry contract](docs/MITER_GEOMETRY_CONTRACT.md) | Corrected local rule, independent oracle, and extreme-font limits |
| [Region experiment](docs/REGION_OFFSET_EXPERIMENT.md) | Why topology and a surrounding envelope are required |
| [Illustrator architecture](docs/ILLUSTRATOR_ARCHITECTURE_AUDIT.md) | SDK stroke-filter contracts; current host results are in the load report |
| [InDesign interception](docs/INDESIGN_PAINT_INTERCEPTION_AUDIT.md) | Public drawing contracts and unresolved suppression route |
| [Fixture census](docs/STROKE_FIXTURE_CENSUS.md) | Inventory of the supplied InDesign fixture |
| [Mechanisms](docs/STROKE_MECHANISMS.md) | Paint-order, miter, outlining, and comparison evidence |
| [History](docs/MITER_JOIN_HISTORY.md) | Primary-source background and confidence limits |
