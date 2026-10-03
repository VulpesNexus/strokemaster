# Miter experiment resumption — September 30, 2026

**Status: BLOCKED at native host execution; the bounded offline experiment is executed.** The highest-value target remains miter spikes. This continuation recovered Claude's final Route X state, prepared the pending native controls, reproduced the cusp failures, and ran calibrated filled-region comparisons on the preserved PDFs' actual embedded fonts. It adds no production miter correction or new host certification.

## Current project state and Claude's handoff

The repository's *main* HEAD is `b737916f8c8a9948b2ebedf6a389a5cb9f5ab4d9`, matching the six supplied cross-frame commits. Before this continuation, the index matched that commit's tree, and all 445 indexed files matched their Git blobs, allowing checkout CRLF normalization. A subsequent Git status showed only this continuation's research changes. Remote push state was not checked.

The installed plugin and *build/Release-RouteX/InDesignStrokeEngine.pln* match SHA-256 `1e7c417e998c4ab8314384d6dc865d81f949b6ed3c7e8d0139da23d5ee3e1ef8`. The retained Route W build hashes to `f31dd2c9c7a4f7bd94ea9c9c1c48b19ba33fec76f2c3332730d1d82d7c4f4215`. The recorded cross-frame census has 22 PASS cases and 132 PASS output verdicts; those Adobe outputs were not regenerated here.

Claude's local *.ai/AI_STATE.md* was stale: it requested Route W installation. The latest section J in *STROKEMASTER_RESEARCH_REVIVAL_2026-09-25.md* correctly names the [miter contract](MITER_GEOMETRY_CONTRACT.md) as the next target. The local handoff has been updated to the measured state and remaining gates.

The currently running applications are **InDesign 21.6** and **Illustrator 30.8.2**, whereas the certification used InDesign 21.5.1.73 and Illustrator 30.7. The InDesign process's module list includes *InDesignStrokeEngine.pln*. Loaded-module presence and matching installed bytes are separate observations; no new draw was measured. These newer host versions must not inherit the old certification without a fresh host run.

## Native connection attempts and authorization

The first phase respected the standing silent-execution restriction and used in-process reads and Node execution. The owner then explicitly authorized tests even if they were not perfectly silent. That superseded the silence restriction for this task and enabled command-based testing.

The existing InDesign probe runner could not find the host in the Running Object Table. Its fallback COM activation started a second *InDesign.exe -Embedding* process, stalled before the probe ran, and returned `80080005 CO_E_SERVER_EXEC_FAILURE`. The extra process exited; the original InDesign and Illustrator processes were left running. No control fixture or native export was produced. An attempted cleanup found that the extra process had already exited and stopped no process.

Read-only, approved diagnostics outside the restricted sandbox confirmed the normal executable registration, loaded scripting modules, and a visible original InDesign window. Both the generic and versioned existing-object queries returned `800401E3 MK_E_UNAVAILABLE`. Automatic approval review rejected a repeat of COM activation because its after-the-fact process guard would not prevent another unintended host. That activation was not retried.

The Computer Use skill's supported *@oai/sky* entry point was also tried. Its native pipe returned “The system cannot find the file specified” (OS error 2), including after the server restart. No Computer Use app input was performed. No custom native-helper protocol or alternate COM launcher was attempted. The workspace guard reports **no pending InDesign snapshot**. No original Adobe session was closed or resized.

## Cusp inventory and pending control fixture

[The preflight evidence](../tests/geometry/miter_host_control_preflight.json) pins the current script and input identities. Fifteen checks pass in Node: specimen geometry, join/cap coverage, exact dyadic arithmetic, existing JSX reversal refusal, census reproduction, and lifecycle doubles covering successful export, host refusal, normalization, style failure, and export failure. Doubles establish script dispatch and cleanup logic, not actual Adobe geometry or preference behavior.

The existing *ise-clip-miter.jsx* reproduced 10,960 corner candidates on the unchanged preserved extraction: 10,781 valid local joins and 179 explicit cusp/unresolved-turn failures across 92 contours. No fallback join or contour removal was introduced.

Exact dyadic cross products identify 47 closed contours whose anchors and handles all lie on one line, each with nonzero extent. Their cubic segments therefore also lie on that line and enclose zero fill area in this extracted representation. Ninety-four of the 179 failures occur on these contours; the other 85 remain separate. Zero fill area does not imply zero stroke ink. Of 128 two-anchor contours, 12 are collinear and 116 are curved; anchor count cannot be a discard rule. This does not resolve glyph-to-extraction mapping or the 216-contour provenance deficit.

[*probe-miter-host-controls.jsx*](../tools/jsx/probe-miter-host-controls.jsx) loads the shared helpers and [specimen definitions](../tools/jsx/miter-host-cases.jsx) from their repository folder. It creates one hidden disposable 42-page document and a new *build/miter-controls-<timestamp>/* evidence directory, closes only its owned document, and uses the existing preference snapshot/restore helper. Adobe documents [the hidden-document parameter](https://developer.adobe.com/indesign/uxp/dom/api/d/documents/), [cap identifiers](https://developer.adobe.com/indesign/uxp/dom/api/e/end-cap/), and [path input representation](https://developer.adobe.com/indesign/uxp/dom/api/p/path/); those declarations are not new ExtendScript host proof.

| Control family | Cases |
| --- | ---: |
| Open segment, three joins × three caps | 9 |
| Exact open U-turn, three joins × three caps | 9 |
| Closed two-anchor contour, three joins × three caps | 9 |
| Ordinary right-angle corner, three joins, butt cap | 3 |
| Near U-turn, both signs at 0.001 and 0.01 pt, three joins | 12 |

Each control requests a centered 3 pt stroke and miter limit 4. The manifest records requested and read-back geometry, refusal, or normalization. Style/export failure fails the harness. `HOST_RECORDED` is only a collection status; `nativeGeometryVerdict` remains `NOT_MEASURED` until independent vector and raster comparison. This probe remains unexecuted in Adobe and has not received independent review.

## Executed embedded-font experiment

[*compare_embedded_region.py*](../tools/compare_embedded_region.py) reads the actual embedded TrueType or Type1C font in a one-page PDF, converts closed glyph contours through the existing bounded curve adapter, and invokes the frozen topology core with `compare_boundary`. It compares a positive filled-region buffer with the union of fill and symmetric boundary stroke. Glyphs remain separate authorized cohorts. No font program, glyph coordinates, or result vector geometry is written to retained evidence. Unsupported encoding, font matrices, open contours, degenerate rings, and unresolved topology remain explicit failures or per-glyph refusals.

[*measure_miter_calibration.py*](../tools/measure_miter_calibration.py) independently parses the matching limit-4 stroke-text run in each preserved PDF. It confirms identity stroke CTM, miter join, and these calibrations:

| PDF fixture | Effective font size | PDF stroke width | Half-width used |
| --- | ---: | ---: | ---: |
| Suite B, embedded Minion Pro | 36 pt | 8 pt | 4 pt |
| Takeover extreme, embedded KerberosFang | 12 pt | 3 pt | 1.5 pt |

Both PDFs use `Tf = 1`; the text matrix supplies the 36 pt or 12 pt scale. Calibration selects the limit-4 row rather than the first suite B row, which has limit 1. This resolves units for this bounded experiment, not every affine text transform. Font mapping and coordinate-level provenance remain separate acceptance gates.

The [aggregate evidence](../tests/geometry/miter_region_experiment_2026-09-30.json) embeds the individual measurement records and pins their source, PDF, font, and frozen-core hashes. Suite B's embedded Type1C stream hashes to `04026ca2e330fa954f0a648fb1ec9286e6be9db47b684c1853ad94ec108817dd`; KerberosFang's embedded TrueType stream matches the previously recorded `58dd2f9e…` identity. The core still hashes to `07acb688…`.

All 11 distinct suite B glyphs were accepted at six initial flattening tolerances from 0.01 to 0.0003125 pt. Every accepted response reported zero fill-loss residual. The symmetric comparator has excess regions on **M, v, w, and z**; the other seven reported zero excess. This is a comparison between two Boost constructions on flattened embedded outlines, not a corrected Adobe output or independent proof of native miter semantics.

For KerberosFang, all nine distinct nonempty glyphs in “Lorem ipsum” were refused: L has crossing/touching contours, and the others encounter fewer than three distinct points. Space is recorded as an empty glyph with no region request. No troublesome contour was dropped to obtain a pass. The earlier exploratory record that submitted space to the core and obtained an empty-source refusal remains under *build/*; the final reader labels emptiness separately.

## Refinement result and new performance limit

[*measure_miter_refinement.py*](../tools/measure_miter_refinement.py) checks the four changed Minion glyphs with declared successive-run thresholds: excess-area delta ≤ 0.01 pt² and bounds delta ≤ 0.005 pt. It retains every successful checkpoint, persists timeouts, and does not repeat a failed checkpoint on resume.

Five further runs completed, from 0.00015625 to 0.000009765625 pt flattening tolerance. None met both thresholds. At the finest completed run:

| Glyph | Symmetric excess area | Material bounds difference |
| --- | ---: | --- |
| M | 0.089180 pt² | Lower bound extends 0.188783 pt farther |
| v | 2.092392 pt² | Lower bound extends 2.253691 pt farther |
| w | 2.121740 pt² | Upper bound extends 2.189676 pt farther |
| z | 0.109705 pt² | Bounds agree; difference is inside the bounds |

The next batch, at 0.0000048828125 pt, exceeded the core's **60-second timeout**. The summary is explicitly `REFINEMENT_NOT_CONVERGED`, and the driver returns failure. Bounds and area vary with chord refinement even while the flattening position bound shrinks; no universal stroke-error bound or stable exact silhouette is claimed. The result exposes a precision/performance limitation of this prototype rather than justifying a relaxed criterion.

## Verification and unchanged production scope

| Verification | Result |
| --- | --- |
| Existing geometry suite | 90 passed, no skips |
| New reader, calibration, and resume tests | 13 passed |
| Existing audit suite | 56 passed, no skips, outside restricted process sandbox |
| Existing Node selection | 313 passed, no skips |
| Miter control preflight | 15 passed; no Adobe execution |
| Native refusal core | 33 checks passed |
| Stroke-behind core | 65,563 checks passed |
| Cross-frame core | 43 checks passed |

These are separate invocations, not one combined geometry run. Audit attempts inside the restricted process sandbox failed on temporary-file permissions, including in a workspace temp directory; the approved outside-sandbox run passed. Those execution failures were not hidden or attributed to the plugin. Native tests rebuilt only ignored test artifacts and did not install or load Adobe.

Production plugin source, installed Route X, and certification records were not changed. No commit, push, installation, release, or original-host shutdown occurred. New research tools and evidence remain uncommitted and lack independent review. No relaunch is needed for a plugin change, because none was made.

## Exact next step

Restore a supported connection to the existing/new InDesign 21.6 host and run the 42 native controls. An owner-session relaunch is not authorized by merely knowing a process exists; preserve open documents and obtain the necessary owner-session permission before closing it. Measure PDF paths/operators and native PNG against two PDF renderers, with cap, closure, and actual stored geometry recorded separately. A native refusal or normalization does not authorize deleting a font contour.

After those controls, address the unresolved filled-region topology and curve offset construction using the measured glyph cohort. Native stroke replacement must preserve legitimate exterior miters, live text, vector output, fonts, and every Route W and Route X regression row. The observed core timeout is also a performance gate. Owner inspection at 400%, a real document, screen pixels, and the previously untested host scenarios remain open.

## Backend restoration attempt

After the owner explicitly requested backend restoration and probe execution, the supported Computer Use client was reset and reinitialized. Both the initial and post-reset window-list calls failed with native pipe OS error 2. A fresh existing-host COM attachment also returned `800401E3 MK_E_UNAVAILABLE`; InDesign remained responsive. No fallback COM activation, host shutdown, or probe dispatch occurred. The supported API provides no native-helper start or repair operation. Backend restoration requires the application runtime to provision the missing native helper; restarting InDesign alone is not established as a remedy.

## Host run — October 3, 2026

**Status: the 42 native controls are recorded in InDesign 21.6.0.57; the geometry verdict is still `NOT_MEASURED`.** InDesign was not running. A tool session launched it, non-elevated and without activation, inside *.workspace/tools/adobe-workspace-guard.ps1* Save/Restore, ran *tools/run-id-probe.ps1 -Script probe-miter-host-controls.jsx*, and quit it with no documents open; the guard restored one workspace file and reports no pending snapshot. The installed plugin was not changed.

A normally launched InDesign does not register in the Running Object Table either, but COM class activation attached to that one process without starting another. The September 30 activation failure therefore came from that host session, not from the route; an elevated launch after the owner's elevated install is a plausible, unverified cause.

The first two runs exposed three harness defects, fixed in the probe with regression checks in *tests/geometry/test_miter_host_controls.js* (18 checks pass; the new ones fail against the previous comparison and gate):

| Defect | Effect | Fix |
| --- | --- | --- |
| Exact string comparison of read-back anchors | InDesign reads 50.01 back as 50.0100000000002; all 12 near-U-turn controls were marked normalized and removed before drawing | Per-case `readbackDeviation`, accepted within 1e-9 pt; the maximum observed was 2.2e-13 pt |
| Gate failed on any `unverified=` count | 19 PDF security and sampling preferences are unreadable while security is off, in the snapshot already | Snapshot-unreadable preferences the probe never writes are recorded in `unreadablePreferences`; a written or verification-failed one still fails |
| Restore-line parser used a capture group | InDesign 21.6's ExtendScript returned `undefined` for the first group of `/^(\d+)(?: \[(.*?)\])?…/` | Parsed without capture groups |

The passing run is *build/miter-controls-1791013114285/* (ignored): *controls.indd*, *controls.pdf*, *controls.json*, and one 1200 ppi PNG per page. InDesign names page 1 *controls.png* and page N *controlsN.png* from 2 to 42. All 42 cases are `CREATED`. The [preflight record](../tests/geometry/miter_host_control_preflight.json) still pins the September 30 probe; it is history, not this run's identity.

A first raster look, from InDesign's own PNG only (ink bounding boxes, specimen frame in points, segment 0 to 30, 3 pt stroke, limit 4):

- Miter and bevel joins at exact and near U-turns (both signs, 0.001 and 0.01 pt) add no ink beyond x = 30: no spike, and no miter at all, since the limit is exceeded.
- Round joins at the same turns add a round end reaching x = 31.48.
- Closed two-anchor contours paint the full 30 × 3 pt bar: flush ends with miter or bevel joins, round ends with a round join; caps do not apply. Zero fill area is not zero stroke ink, so these contours cannot be dropped.
- Caps at the open ends behave as specified.

This is one renderer and bounding boxes, not the independent PDF-operator and two-renderer measurement the next step requires.

## PDF measurement — October 3, 2026

[*tools/measure_miter_controls.py*](../tools/measure_miter_controls.py) reads the run's *controls.pdf*, renders every page in MuPDF 1.28.2, Poppler 20.08.0 (*pdftoppm*), and Ghostscript 9.23 at 1200 dpi, adds InDesign's own PNGs, and compares each with an analytic area and bounds derived from the PDF stroking rules (a join whose miter ratio exceeds the limit is beveled). The tolerances were fixed before measuring: 0.5 pt² of area and 0.12 pt (two pixels) of bounds. The record is [*miter_host_controls_pdf_2026-10-03.json*](../tests/geometry/miter_host_controls_pdf_2026-10-03.json); 13 unit tests cover the expectations (one against an independent capsule oracle), the operator parser, and the coverage measurement. Its status is `PDF_MEASURED_WITH_FAILURES`, for the reason below.

**Operators: 42 of 42 match.** Each page has one stroke of one subpath with width 3, miter limit 4, no dash, and the requested vertices exactly; join and cap are set in the page's */GS0* ExtGState, not in the content stream. A closed contour is written with an explicit return to its first vertex followed by `h`, so the closed two-anchor specimen reaches renderers as 0 → 30 → 0, closed.

**Renders:**

| Renderer | Pass at the fixed tolerances | Edge offset from the butt segment (MC00) | Largest residual after that offset |
| --- | ---: | ---: | ---: |
| MuPDF | 42 | +0.0001 pt | 0.10 pt² |
| Poppler | 42 | 0.0000 pt | 0.26 pt² |
| Ghostscript | 3 | +0.0080 pt | 0.17 pt² |
| InDesign PNG | 2 | −0.0088 pt | 0.16 pt² |

Every bounds error over all 168 renders is at most 0.04 pt. Ghostscript and InDesign fail on area only: Ghostscript grows and InDesign thins every edge by about 0.14 px, which changes the area in proportion to the outline length; that offset is a diagnostic computed from MC00 alone, not a relaxed verdict. The residual patterns are renderer traits: MuPDF draws round ends about 0.1 pt² short (inscribed polygons), and Poppler draws right-angle corners about 0.25 pt² large.

**What this establishes for the miter contract:**

- No renderer draws a miter at an exact or near reversal: miter joins at 180° and at 0.001–0.01 pt from it end flush, as the PDF rule requires, and round joins add the half disc.
- The closed two-anchor contour is inked as a full 30 × 3 pt bar in every renderer, with flush or round ends by join and no caps. Such contours carry stroke ink and cannot be dropped from glyph outlines.
- InDesign's PDF carries the requested stroke parameters unchanged, so its PDF and the independent renderers agree with the rule, and InDesign's own raster differs only by its edge offset.

**Not resolved:** the near-reversal sliver (15|eps| pt², at most 0.17 px tall at 1200 dpi) is below these renders' resolution. Its residual flips sign with eps in Poppler, Ghostscript, and InDesign, by about the sliver's own area; only MuPDF stays within 0.03 pt². A cropped high-resolution render would settle it; it does not affect the spike question.
