# StrokeMaster

Experimental stroke corrections for InDesign, plus an Illustrator native integration diagnostic. This is not a production release. The [takeover audit](PROJECT_TAKEOVER_AUDIT.md) separates reproduced behavior, corrected implementation errors, and remaining release gates.

## Current support

| Problem | Current result |
| --- | --- |
| Overlap between lines and formatting runs | The InDesign fill-repaint prototype corrects the measured Outside-stroked victims. It still misses unstroked victim runs and has ordering/compositing limits. Supported split-frame cases provide a document-level alternative. |
| Miter spikes | Conventional miter behavior is characterized. The corrected local clipping kernel passes independent tests, but local clipping does not satisfy the original global outer-envelope requirement. No native miter fix is delivered. |
| Create Outlines | The script stages conversion before replacing supported text frames. Thirteen checks pass in InDesign 21.5.1.73; broader fidelity and lifecycle coverage remain open. |
| Editable split pairs | Explicit resync carries text and formatting while preserving a uniform Outside stroke. Contents-only watching is optional. Sixteen host checks include IDML import, page placement, and None/zero-weight top strokes. |
| Illustrator integration | An x64 straight-line stroke-replacement diagnostic compiles and passes mocks. It has not been loaded in Illustrator and is not a font stroker. |

The measured hosts are InDesign 21.5.1.73 and Illustrator 30.7.0, build 114R, on Windows. These observations are not cross-version certification.

## Use the scripts

Copy the *tools/jsx* folder into InDesign's *Scripts Panel* folder: open *Window > Utilities > Scripts*, right-click a folder, and choose *Reveal in Explorer*. Run *strokemaster.jsx* and choose **split**, **outline**, **resync**, **watch**, **unwatch**, or **status**.

Read [the usage guide](docs/USING_STROKEMASTER.md) before choosing a correction. Split frames travel as ordinary document objects, but remain two stories. Automatic updates require the script running, and formatting-only edits require explicit resync. Unsupported backgrounds, effects, threading, and ambiguous pair labels are refused. Keep an original copy while evaluating a new revision.

The top Outside-stroke contribution is explicitly cleared to *None* and weight 0. The bottom uses the stroke color as its fill. Mixed Center-stroked ranges keep their native treatment on top; a frame with no Outside-stroked range is not split.

## Native prototype

The InDesign binary and its resource folder load from *Adobe InDesign 2026/Plug-Ins/InDesignStrokeEngine/* after an InDesign restart. This per-machine drawing correction does not change the document. A recipient without the plugin sees native InDesign behavior; tested PDFs already contain the corrective paths.

The prefix `0x5A200` is unassigned. Do not treat it as a release identity. An Adobe prefix allocation, Illustrator effect identity, package signing, and Creative Cloud publication are separate steps explained in [the distribution audit](docs/PLUGIN_DISTRIBUTION.md). No Marketplace package has been submitted.

Native builds require external Adobe SDKs and Visual Studio/MSVC dependencies. The existing InDesign project is under *plugin/*; the separate compile-only Illustrator diagnostic and its build notes are under *plugin/illustrator-probe/*. SDK files and generated binaries are not redistributed here.

## Verify and investigate

[Validation instructions](tests/README.md) cover the executed pure suites and bounded host records. Python tools require *requirements.txt*; the script mocks require Node 24. Old fixture generators and output tables remain research evidence. Regenerate into fresh output directories and inspect restoration reports; do not overwrite the preserved corpus merely to update a measurement.

The render comparator accepts an independent fill-only mask. Without it, its eroded non-background mask includes stroke/effect ink and cannot measure glyph-fill compositing. Historical transparency interpretations based on the old mask were withdrawn and remeasured.

| Document | Purpose |
| --- | --- |
| [Usage guide](docs/USING_STROKEMASTER.md) | Workflow, supported contexts, installation, and limitations |
| [Takeover audit](PROJECT_TAKEOVER_AUDIT.md) | Initial A–N audit, claim ledger, and subsequent evidence |
| [Open questions](OPEN_QUESTIONS.md) | Remaining discriminating tests and release blockers |
| [Geometry contract](docs/MITER_GEOMETRY_CONTRACT.md) | Corrected local rule, independent oracle, and extreme-font limits |
| [Region experiment](docs/REGION_OFFSET_EXPERIMENT.md) | Why topology and a surrounding envelope are required |
| [Illustrator architecture](docs/ILLUSTRATOR_ARCHITECTURE_AUDIT.md) | Native stroke-filter route and compile-only proof |
| [InDesign interception](docs/INDESIGN_PAINT_INTERCEPTION_AUDIT.md) | Public drawing contracts and unresolved suppression route |
| [Fixture census](docs/STROKE_FIXTURE_CENSUS.md) | Inventory of the supplied InDesign fixture |
| [Mechanisms](docs/STROKE_MECHANISMS.md) | Paint-order, miter, outlining, and comparison evidence |
| [History](docs/MITER_JOIN_HISTORY.md) | Primary-source background and confidence limits |
