# StrokeMaster

Experimental stroke scripts, native Adobe integration, and topology research. This is not a production release. The [support matrix](HOST_SUPPORT_MATRIX.md) separates current behavior, historical prototype results, and unresolved release gates.

## Current support

| Problem | Current result |
| --- | --- |
| Overlap between lines and formatting runs | Illustrator: an experimental native effect moves every stroke of live text beneath every fill; on the measured two-line fixture it removes all stroke-over-fill damage and survives edits, undo, and reopen, but the PDF carries the text as outlines. InDesign: the installed build refuses all repaint and leaves native output unchanged; route W, which moves native Outside strokes beneath the frame's fills, is built but not yet host-tested (installing it needs administrator rights). Split frames remain the document-level alternative. |
| Miter spikes | The reviewed topology core, linked into an Illustrator effect, removes a hole's miter escaping through a thin wall and keeps the ordinary outer miters, on screen-independent PDF measurement of a marked polygon fixture. It handles straight segments only; curves and real glyph outlines remain open. |
| Create Outlines | The script stages conversion before replacing supported text frames. Thirteen checks pass in InDesign 21.5.1.73; broader fidelity and lifecycle coverage remain open. |
| Editable split pairs | Explicit resync carries text and formatting while preserving a uniform Outside stroke. Contents-only watching is optional. Uniform and varied pairs preserve their rendered appearance through the tested INDD/IDML round-trip; resync still requires a uniform bottom stroke. |
| Illustrator integration | The diagnostic plugins load in Illustrator 30.7.0 and run three experimental effects: a straight-line stroke replacement, a topology region correction on a marked fixture, and a stroke-behind correction on live text. Documents keep drawing the corrected result without the plugin, but text edited there draws stale. Nothing here is a release. |

The measured hosts are InDesign 21.5.1.73 and Illustrator 30.7.0, build 114R, on Windows. These observations are not cross-version certification.

## Use the scripts

Copy the *tools/jsx* folder into InDesign's *Scripts Panel* folder: open *Window > Utilities > Scripts*, right-click a folder, and choose *Reveal in Explorer*. Run *strokemaster.jsx* and choose **split**, **outline**, **resync**, **watch**, **unwatch**, or **status**.

Read [the usage guide](docs/USING_STROKEMASTER.md) before choosing a correction. Split frames travel as ordinary document objects, but remain two stories. Automatic updates require the script running, and formatting-only edits require explicit resync. Unsupported backgrounds, effects, threading, and ambiguous pair labels are refused. Keep an original copy while evaluating a new revision.

The top Outside-stroke contribution is explicitly cleared to *None* and weight 0. The bottom uses the source stroke color and tint as its fill. Mixed Center-stroked ranges keep their native treatment on top; a frame with no Outside-stroked range is not split.

## Native prototype

The installed InDesign binary performs no corrective drawing and leaves native output unchanged in the 20-case PNG test. A second variant, built with *build-plugin.ps1 -RouteW*, adds the experimental route W adornments; *tools/install-indesign-plugin.ps1* installs either variant with a backup and hash check, from an elevated PowerShell with InDesign closed. Replacing a loaded binary requires an InDesign restart. The [root-cause report](UNSTROKED_VICTIM_ROOT_CAUSE.md) explains why the earlier repaint was withdrawn. PDFs already exported with an older correction retain their recorded paths.

The prefix `0x5A200` is unassigned. Do not treat it as a release identity. An Adobe prefix allocation, Illustrator effect identity, package signing, and Creative Cloud publication are separate steps explained in [the distribution audit](docs/PLUGIN_DISTRIBUTION.md). No Marketplace package has been submitted.

Illustrator plugins load without elevation from the shared per-generation folder *%LOCALAPPDATA%\Adobe Illustrator Plug-ins\30*, shared with other plugins; the region probe has loaded from there in every run since 2026-09-15. InDesign has no such folder: its *Plug-Ins* folder is writable by administrators only, so installing or replacing the InDesign binary needs one elevated copy. [Route evidence](fixtures/FIXTURE_INDEX.md#per-user-load-route).

Native builds require external Adobe SDKs and Visual Studio/MSVC dependencies. The InDesign project is under *plugin/*; the Illustrator diagnostic and its build notes are under *plugin/illustrator-probe/*. Read the [native load report](ILLUSTRATOR_NATIVE_LOAD_REPORT.md) before testing it. SDK files and generated binaries are not redistributed here.

## Verify and investigate

[Validation instructions](tests/README.md) cover the executed pure suites and bounded host records. Python tools require *requirements.txt*; the script mocks require Node 24. Old fixture generators and output tables remain research evidence. Regenerate into fresh output directories and inspect restoration reports; do not overwrite the preserved corpus merely to update a measurement.

The render comparator accepts an independent fill-only mask. Without it, its eroded non-background mask includes stroke/effect ink and cannot measure glyph-fill compositing. Historical transparency interpretations based on the old mask were withdrawn and remeasured.

| Document | Purpose |
| --- | --- |
| [Research revival, 2026-09-25](STROKEMASTER_RESEARCH_REVIVAL_2026-09-25.md) | Current evidence ledger, root-cause map, architecture decision, and next experiment |
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
