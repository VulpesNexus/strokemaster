# StrokeMaster

Experimental stroke scripts, native Adobe integration, and topology research. InDesign route X `73fb1805` was certified on 2026-10-07 ([status](docs/RELEASE_CANDIDATE_CHECKS_2026-10-06.md#status)). Later switches and fixes have narrower host coverage; see the [review](docs/REVIEW_2026-11-01.md), [screen refresh tests](docs/INDESIGN_SCREEN_REFRESH_2026-11-01.md), and [small-hole regression](docs/INDESIGN_SMALL_HOLES_2026-11-02.md). Those results do not recertify every rendering route. Illustrator remains experimental. The [support matrix](HOST_SUPPORT_MATRIX.md) separates current behavior, historical prototype results, and unresolved release gates.

## Current support

The [November 2 review](docs/REVIEW_2026-11-02.md) records the latest fixes and verification limits. Installation claims in dated reports describe that run, not the binary currently installed on another machine.

| Problem | Current result |
| --- | --- |
| Overlap between lines and formatting runs | Illustrator: for a uniform text stroke, an object-level Appearance stroke beneath the characters removes all measured damage, keeps PDF text, and needs no plugin to render or edit; the probe plugin can write it. For mixed per-character strokes, an experimental native effect moves every stroke beneath every fill and survives edits, undo, and reopen, but the PDF carries the text as outlines. InDesign: route W, a native plugin build, moves native Outside strokes beneath the fills of the same frame at draw time; it is [certified](ROUTE_W_CERTIFICATION_2026-09-25.md) for live and flattened PDF, EPS, PostScript, PNG, save, reopen, and IDML, and keeps PDF text live. Route X extends it across the threaded frames and columns of one story, in background PDF export too, and, by default, leaves overlap between independent frames as InDesign stacks it; Across separate stories enables the optional cross-story correction ([cross-frame research](CROSS_FRAME_RESEARCH_2026-09-26.md)). Recertified on InDesign 21.6 against same-day controls, extended to parent pages, path text, rotated, scaled, chained, and grouped stories, and confirmed by the owner on *cert-X.indd* and on a real document ([report](docs/ROUTE_X_21_6_RECERTIFICATION_2026-10-03.md)); that report certifies its dated build. The document itself is never changed. Split frames remain the document-level alternative. |
| Miter spikes | InDesign: route X's candidate build draws a mitered Outside stroke with bevel joins and gives back, within the run's limit, only the miter joins of corners whose join points away from the fill, filled from the corner's exact tangents, so no interior miter tip can leave a thin glyph; a mitered Center stroke stays native with bevel joins, and its exterior miters, and its interior miters clipped to the letter, are filled in after the text. On the miter suite every measured spike is gone in foreground and background PDF export, legitimate exterior miters are kept, and nothing else in the certification changes beyond edge-level variance ([report](docs/ROUTE_X_21_6_RECERTIFICATION_2026-10-03.md)). Plain mitered strokes on shapes, frames, and outlined text get the same rule, overlapping contours included, and a corner past the miter limit keeps its whole point when it is a shape's own point between straight edges, and is beveled as InDesign bevels it elsewhere; Inside strokes, open Center paths, corner effects, and kept transforms are covered too ([release-candidate checks](docs/RELEASE_CANDIDATE_CHECKS_2026-10-05.md)). Illustrator: the reviewed topology core, linked into an effect, removes a hole's miter on a marked polygon fixture; later native routing handles live text and outlined glyphs; the triangle and uniform-stroke two-story cases have bounded host evidence in the current review. Mixed-stroke fill-route grouping is refused whenever Close holes or live-text miter correction is on. |
| Create Outlines | The script stages conversion before replacing supported text frames. Thirteen checks pass in InDesign 21.5.1.73; broader fidelity and lifecycle coverage remain open. |
| Editable split pairs | Explicit resync carries text and formatting while preserving a uniform Outside stroke. Contents-only watching is optional. Uniform and varied pairs preserve their rendered appearance through the tested INDD/IDML round-trip; resync still requires a uniform bottom stroke. |
| Illustrator integration | The diagnostic plugins load in Illustrator 30.7.0 and run experimental effects for stroke replacement, region correction, stroke ordering, enclosed holes, and flat tips. Documents keep drawing the corrected result without the plugin, but text edited there draws stale. Nothing here is a release. |

The measured hosts include InDesign 21.5.1.73 and 21.6.0.57, and Illustrator 30.7.0 (build 114R) and 30.8.2 on Windows. These observations are not cross-version certification. To install and use the InDesign plugin and its StrokeMaster panel (one switch per correction), read [the usage guide](docs/USING_STROKEMASTER.md#the-plugin).

## Use the scripts

Copy the *tools/jsx* folder into InDesign's *Scripts Panel* folder: open *Window > Utilities > Scripts*, right-click a folder, and choose *Reveal in Explorer*. Run *strokemaster.jsx* and choose **split**, **outline**, **resync**, **watch**, **unwatch**, or **status**.

Read [the usage guide](docs/USING_STROKEMASTER.md) before choosing a correction. Split frames travel as ordinary document objects, but remain two stories. Automatic updates require the script running, and formatting-only edits require explicit resync. Unsupported backgrounds, effects, threading, and ambiguous pair labels are refused. Keep an original copy while evaluating a new revision.

The top Outside-stroke contribution is explicitly cleared to *None* and weight 0. The bottom uses the source stroke color and tint as its fill. Mixed Center-stroked ranges keep their native treatment on top; a frame with no Outside-stroked range is not split.

## Native prototype

The default InDesign build performs no corrective drawing and leaves native output unchanged in the 20-case PNG test. A second variant, built with *build-plugin.ps1 -RouteW*, adds the experimental route W adornments and is the one measured in the [research revival](STROKEMASTER_RESEARCH_REVIVAL_2026-09-25.md#route-w-host-test). A third, *build-plugin.ps1 -RouteX*, adds route X (the correction across the frames of one story) and is also loaded by InDesign's background PDF export; *-RouteX -Research* adds the trace and probes used to build it. *tools/install-indesign-plugin.ps1 -Variant Release-RouteX* (or *Release-RouteW*) installs a variant with a backup and hash check, from an elevated PowerShell with InDesign closed. Replacing a loaded binary requires an InDesign restart. The [root-cause report](UNSTROKED_VICTIM_ROOT_CAUSE.md) explains why the earlier repaint was withdrawn. PDFs already exported with an older correction retain their recorded paths.

The prefix `0x5A200` is unassigned. Do not treat it as a release identity. An Adobe prefix allocation, Illustrator effect identity, package signing, and Creative Cloud publication are separate steps explained in [the distribution audit](docs/PLUGIN_DISTRIBUTION.md). No Marketplace package has been submitted.

Illustrator plugins load without elevation from the shared per-generation folder *%LOCALAPPDATA%\Adobe Illustrator Plug-ins\30*, shared with other plugins; the region probe has loaded from there in every run since 2026-09-15. InDesign has no such folder: its *Plug-Ins* folder is writable by administrators only, so installing or replacing the InDesign binary needs one elevated copy. [Route evidence](fixtures/FIXTURE_INDEX.md#per-user-load-route).

Native builds require external Adobe SDKs and Visual Studio/MSVC dependencies. The InDesign project is under *plugin/*; the Illustrator diagnostic and its build notes are under *plugin/illustrator-probe/*. Read the [native load report](ILLUSTRATOR_NATIVE_LOAD_REPORT.md) before testing it. SDK files and generated binaries are not redistributed here.

## Verify and investigate

[Validation instructions](tests/README.md) cover the executed pure suites and bounded host records. Python tools require *requirements.txt*; the script mocks require Node 24. Old fixture generators and output tables remain research evidence. Regenerate into fresh output directories and inspect restoration reports; do not overwrite the preserved corpus merely to update a measurement.

The host-independent geometry cores also build and test with g++ or clang; see [portable geometry cores](tests/README.md#portable-geometry-cores).

The render comparator accepts an independent fill-only mask. Without it, its eroded non-background mask includes stroke/effect ink and cannot measure glyph-fill compositing. Historical transparency interpretations based on the old mask were withdrawn and remeasured.

| Document | Purpose |
| --- | --- |
| [Research revival, 2026-09-25](STROKEMASTER_RESEARCH_REVIVAL_2026-09-25.md) | Historical evidence ledger, root-cause map, architecture decision, and next experiment |
| [Route X on InDesign 21.6, 2026-10-03](docs/ROUTE_X_21_6_RECERTIFICATION_2026-10-03.md) | 21.6 recertification, the extension census, the installed candidate, and the miter-spike model checked against InDesign |
| [What to know](docs/WHAT_TO_KNOW.md) | What each plugin does to your files, and its limits, in one page |
| [What each switch fixes](docs/FEATURES.md) | One example per switch, off and on, rendered by InDesign |
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

## License

**GPL-3.0-or-later**, with an **Adobe SDK linking exception** for each plugin: the InDesign SDK for the InDesign plugins, the Illustrator SDK for the Illustrator plugin ([LICENSE](LICENSE), [LICENSE-EXCEPTION](LICENSE-EXCEPTION)).

The exception is load-bearing, not boilerplate: both InDesign plugins compile three of Adobe's SDK source files into themselves, under terms GPL section 10 forbids passing on. The repository itself vendors no Adobe code, and the scripts in *tools/jsx/* link nothing.

Vibecoded by [Vixen420](https://github.com/VulpesNexus) in October 2026. Copyright © 2026 Vixen420.
