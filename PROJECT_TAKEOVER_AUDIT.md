# StrokeMaster takeover audit

Audit started 2026-09-12 against `fb15301`. This is a working evidence ledger, not a release certification. “Reproduced now” means executed during this takeover; inherited measurements remain historical until independently checked. Large task: native host integration, geometry, rendering, and compatibility require separate validation.

## A. Repository state

Initial checkout: clean `main`, tracking `origin/main`; remote is the private *VulpesNexus/strokemaster* repository. Three commits: `a512b61` (initial InDesign implementation and evidence), `55e8289` (geometry experiments), and `fb15301` (history and Illustrator probes). No other local branches or release tags. Inventory: 202 tracked files, including 147 test artifacts, 36 tools, nine plugin files, and seven documents. No CI configuration or pure automated regression suite was found in the initial tracked inventory. Existing ignored *build/* and *.venv/* are local dependencies/artifacts, not releases.

The preserved InDesign fixture hash was reproduced: SHA-256 `3797BA73746CD6BAC8CF55B689F0347E77CB622257651AC4AFA5F4430BA3A783`. Its copy under *tests/indesign-stroke/corpus/pristine/* is the authority. Generated fixtures cover A (line overlap), B (miters), C (run boundaries), D (outlining), N (appearance), S (split frames), and T (sync). User documents must never be closed to make a fixture export succeed.

## B. Existing implementations

- *plugin/* contains one native InDesign global text adornment. It redraws eligible glyph fills in a foreground pass. It does not replace native strokes, suppress their spikes, merge runs, or implement a custom join.
- *tools/jsx/strokemaster.jsx* exposes split, outline, resync, watch, unwatch, and status. Companion scripts duplicate frames or geometry; these are document reconstruction, not native engine correction.
- *tools/stroke_outline.py*, *tools/miter_geometry.py*, and *tools/miter_rule.py* are geometry experiments. *tools/jsx/ise-clip-miter.jsx* duplicates their geometry in ExtendScript. There is no shared native geometry library.
- Illustrator has diagnostic JSX and a COM runner only. No Illustrator native plugin source, project, or binary exists in this checkout. SDK examples outside the repository are not an implementation.

## C. Verified findings

The pristine hash and initial repository inventory were independently reproduced before implementation. The execution record below contains subsequent host tests, PDF parsing, and independent geometry checks. Counts are tied to their test scope; none certifies a production plugin.

## D. Unverified findings

Native Illustrator loading, complete Appearance preservation, topology-safe joins, InDesign stroke suppression, undo/redo, and cross-version release compatibility remain unverified. Historical render observations do not establish all-host or all-font behavior.

## E. Incorrect and stale findings: initial claim ledger

This table records the initial audit, before the corrective experiments. The execution record at the end supersedes its pending entries.

| Claim | Source | Evidence | Reproduced now? | Confidence | Action |
| --- | --- | --- | --- | --- | --- |
| Original fixture is preserved | Census; pristine INDD | SHA-256 above | Yes, hash only | PROVEN | Keep original read-only; work on copies |
| Outside text paints doubled centered stroke, then fill per output run | Mechanisms M1; A/C PDFs and manifests | Committed output and analysis tool | Pending operator re-read | STRONGLY SUPPORTED | Reparse PDFs independently; distinguish output operations from internal wax |
| A and C are paint ordering symptoms | M2–M4; surgery outputs | Controlled historical exports and measurements | Not yet | STRONGLY SUPPORTED | Reproduce ordering intervention and unchanged-order control |
| Repainting is equivalent to painting fills once after strokes | README; plugin comments | Native source adds a second fill pass | No; source contradicts strict equivalence | DISPROVEN | Explicitly measure double coverage, decorations, overlapping colors, and AA |
| All opacity/blend risks are eliminated | Mechanisms M8; SDK map | Finite PDF samples, some nonzero luminance differences | Not yet | UNVERIFIED | Bound claim to tested cases; inspect oracle masks and compositing |
| InDesign miter limit is fixed internally | Earlier hypothesis, corrected M5 | B variants emit differing `M` | Pending reparse | STRONGLY SUPPORTED that hypothesis is false | Verify serialized parameter, not UI inference |
| Clipping plane is at ratio × half-width | Geometry docstrings and code | Code uses `(cap - hw)/(tip - hw)` although edge projection is not `hw` | Source inspected; oracle pending | DISPROVEN as implemented | Independent half-plane oracle before repair |
| No corner extends farther radially than a right angle | Geometry prose | A flat bisector clip also has sideways extent | Analytical contradiction | DISPROVEN for a flat clip | Distinguish axial cap from radial bound; document policy precisely |
| Geometry works for any font, weight, and angle | Outline docstring | One raster comparison; 24 fixed subdivisions and angle cutoff | No | UNVERIFIED | Remove universal claim; adaptive error bounds and adversarial corpus |
| 95 differing pixels validates corrected joins | Mechanisms M5; outline validator | Validator calls ordinary `miter`, not `clip` | Yes, source inspection | DISPROVEN | Retain only as historical native-miter comparison |
| 0.15 pt wedge test validates clipping | Geometry check | Hardcoded historical B3 values, conventional bevel/miter formulas | Yes, source inspection | DISPROVEN | Independent clipping tests required |
| Geometry uses exact anchor tangents for joins | Outline comments | Tangents classify corners, but actual join uses flattened adjacent chords | Yes, source inspection | DISPROVEN | Add curved-anchor regression before claiming fidelity |
| Open contours, holes, transforms, and arbitrary scale are supported | Implied universal geometry prose | Loops always close contours; fixed absolute tolerances | No | UNVERIFIED | Declare experiment envelope; test degeneracies and topology |
| Illustrator `createOutlines()` is the method | Outline extraction script | Committed code failed first host execution historically | Pending disposable-host test | UNVERIFIED | Probe singular and plural methods on original, collection, and duplicate |
| Outlining exposes actual Appearance stroke | Extractor header | Leaf path style does not establish full Appearance stack | No | UNVERIFIED | Separate basic path, character, group, and multiple-stroke state |
| P3 region resolution works | Capability probe framing | Only tests method/collection presence | Yes, source inspection | DISPROVEN as an executed claim | Execute Unite/Expand with area, holes, and failure controls |
| Removing temporary art leaves document unchanged | Both Illustrator probes | Mutation can change dirty flag, undo, and selection | No | UNVERIFIED | Use disposable documents; compare source state before/after |
| Fixture generator touches only owned documents | *ise-lib.jsx* header | `saveExport` closes another open target with `SaveOptions.NO` | Yes, source inspection | DISPROVEN | Fail on conflicts; regression covering modified open document |
| Version string proves exact loader compatibility | User guide | SDK resource fields are not runtime compatibility tests | No | UNVERIFIED | Record tested build only; inspect ABI and actual load gates |
| Polling is the only live-sync route | User guide | SDK observers/commands not excluded by a DOM event search | No | UNVERIFIED | Limit claim to implemented script; investigate native observer route |
| Withdrawn SVG feature and industry history are correctly cited | History document and code comments | Document has quotations but no source URLs | Browser verification started | UNVERIFIED | Verify primary sources; shorten quotations; keep history out of geometry code |

## F. Current host capability matrix

| Surface | Evidence available at takeover | Status |
| --- | --- | --- |
| InDesign 2026 foreground adornment | Source, local build artifacts, historical PDF/raster pairs | Historical host verification; current session pending |
| InDesign miter stroke replacement | SDK map proposals only | PROPOSED |
| Illustrator path construction | Historical minimal point-array probe | Needs isolated reproduction |
| Illustrator outline extraction | Failed plural-method call | EXPERIMENTAL |
| Illustrator region resolution | Menu existence only | UNVERIFIED |
| Illustrator live effect / stroke filter | SDK headers and TwirlFilter sample | PROPOSED, not host-verified |
| Cross-version native plugins | No alternative build/load matrix | UNVERIFIED |

## G. Existing test coverage

Tracked evidence includes INDD/IDML/PDF/PNG fixtures, JSON manifests, JSON/Markdown measurements, native/no-plugin pairs, transparency variants, and script generators. These are valuable finite observations. They do not constitute an automated release gate. The outline validator compares one simple filled contour plus its stroke against one Adobe expanded contour at a fixed raster scale. The wedge check embeds earlier measurements rather than regenerating a host oracle. Neither validates clipped-miter geometry.

## H. Missing coverage

Independent vector geometry oracle; clipping-boundary regressions; winding, holes, self-intersection, open paths/caps, collapsed handles, cusps, invalid numbers, and extreme scales; undo/redo and error cleanup; multiple Appearance strokes, brushes, variable width, and nested effects; live editing and metadata persistence; international/vertical text; missing fonts; cancellations; long-session leaks; performance distributions; clean builds and host-version gates. Screen, PDF, print, SVG, save/reopen, and IDML need separate records. A pathological font cannot replace synthetic tests.

## I. Architecture risks

The InDesign implementation changes every eligible legacy frame merely by installation, and cannot suppress native miter spikes. Foreground repaint can also change ordering relative to native decorations and unrelated adornments. Its trace writes from library initialization and the draw callback. No opt-in metadata, unsupported-case detector, or release mode is present. Script pair synchronization supports uniform stroke only and must preserve all source semantics when rebuilding.

The geometry prototype rasterizes overlapping polygons; that is not a vector Boolean engine. A native port would inherit inaccurate clipping and flattening unless corrected first. A local flat clipping policy is not necessarily the desired global glyph-envelope clipping policy. Illustrator Appearance must be handled at the specific stroke contribution, not reconstructed from a character default or a single basic path style.

## J. Documentation inconsistencies

README still calls the miter policy an undecided design choice although the user approved delivery. It says all geometry rules live in one file although three files implement clipping independently. “Correct everywhere,” “invisible,” “only route,” and “no change” exceed their evidence. The SDK version/toolset table is stale relative to its own later build notes. The history document names sources without links. Earlier screen observations are not current plugin screen tests. Research status needs explicit separation from user instructions.

## K. Build/release state

One InDesign MSBuild project and build script exist, with local ignored build output. SDKs for both hosts exist outside the repository. No Illustrator project, CI, release tag, release archive, license file, signing record, or assigned InDesign prefix is present. Prefix `0x5A200` remains a development placeholder. Registration identity, ABI compatibility, packaging, and Creative Cloud distribution are separate questions. No deployment, signing, or marketplace submission is implied by this audit.

## L. Recommended next architecture

First establish a host-independent clipping policy and oracle. Prefer native host Boolean facilities after measuring them; do not build a Boolean engine from raster union experiments. For Illustrator, compare a stroke contribution live effect against a post-effect on outlined input using the actual ownership and style contracts. For InDesign, retain native composition and investigate suppression/replacement of the faulty paint contribution; the existing repaint remains a bounded prototype. Behavior-changing joins should be opt-in. Share tested geometry, not speculative host abstractions.

## M. Ordered next experiments

1. Isolated Illustrator DOM probe: original/collection/duplicate, singular/plural outline methods, success/failure cleanup, and source-state preservation.
2. Independent clipping-plane oracle, ordinary-corner negative controls, deterministic orientation/scale samples, and explicit axial-versus-radial measurements.
3. Repair the two demonstrated probe/core failures and dangerous fixture-conflict behavior, with regression tests.
4. Extract the extreme font through a disposable copy; inspect actual Appearance via SDK or exported output; never equate a character stroke weight with Appearance.
5. Execute native region operations on adversarial regions; trace the full live-effect sample; implement the smallest justified SDK proof.
6. Independently parse A–D PDF evidence, then repeat host editing/export checks on isolated fixtures.
7. Establish native host support envelopes, lifecycle tests, and migration semantics before UI, optimization, and release packaging.

## N. Definition of done

Production claims require independent geometry checks plus live host tests covering edits, styles/Appearance, transforms, undo/redo, save/reopen, export, and safe failure within an explicit supported envelope. Both native plugins, optional scripts, clean reproducible builds, installation/removal instructions, licensing/identity, bounded performance, and negative controls must pass. No production release is certified by this initial audit. Outstanding discriminating experiments are tracked in *OPEN_QUESTIONS.md*.

## Execution record after the initial audit

### Illustrator host probes and benchmark

Executed in Illustrator 30.7.0, build 114R. The four-route outline-method probe passed 17 checks, including injected-failure cleanup. The working method is the singular `createOutline()`. The extractor now uses a disposable document and returns explicit success/failure; both the success and injected-failure paths preserved the measured source contents, bounds, item count, saved state, and document count. This is preservation of those measured fields, not a proof covering all hidden host state.

The region probe passed ten checks: basic stroke setters, point-array assignment, path cleanup, actual Pathfinder Add and Expand on overlapping squares (area 15,000 pt²), and a compound square with a hole (area 7,500 pt²). The latter tests compound construction, not Boolean operations on arbitrary holes. Self-intersections, transforms, degenerate geometry, and undo remain open.

The extreme example produced 3,336 closed contours and 20,308 anchors. Its PDF emits a 3 pt miter stroke with limit 4, despite a 1 pt character-level DOM value. Basic leaf paths report zero stroke after outlining; neither reading identifies the complete Appearance stack. A native stroke-contribution test is required.

Font provenance is materially significant: two KerberosFang versions are installed. Version 1.000 has 3,552 contours for this text; version 1.004 has 820. The embedded PDF agrees with version 1.000's contour census. The outlined extraction has 216 fewer contours, an unexplained difference. There are 215 two-point contours in the embedded font, but subtracting them does not account for all 216, and the extraction still contains 128 two-anchor contours. No discard mapping is proven. See [the reproducible font census](tests/geometry/extreme_font_provenance.json).

Two independent PDF renderers also differ materially on the native extreme-font PDF: at 864 dpi, the same 1,056 × 480 crop has 91,295 thresholded ink pixels in MuPDF and 104,555 in Poppler, with XOR 23,454. Both were visually inspected. This exceeds a demonstrated antialiasing-only explanation; its cause and which result matches the intended geometry remain unresolved. Local artifacts are retained under *build/takeover/*; font binaries are not redistributed.

### Geometry corrections and their limits

The inherited clipping interpolation was wrong. At a 30° corner, half-width 8, and ratio √2, it returned a bisector extent of approximately 6.2419 instead of 11.3137. Python and JSX now intersect the actual offset lines with the clipping plane. The independent decimal half-plane oracle made 12,984 coordinate comparisons; maximum normalized error was approximately 1.78 × 10⁻¹⁵ on that corpus. Ordinary retained miters have negative controls. This verifies local straight-segment join mathematics, not complete stroked glyphs.

The extreme extraction yields 10,781 valid local joins and 179 explicit cusp/unresolved-turn failures across 92 contours. Of the valid joins, 5,756 ordinary miters are unchanged, 4,121 are shortened relative to retained native miters, and 904 flat clips are longer than native bevel fallback. No full-font pass is claimed. See [the geometry contract](docs/MITER_GEOMETRY_CONTRACT.md) and [the measured corpus](tests/geometry/extreme_join_statistics.json).

A synthetic narrow counter disproves equivalence between local √2 clipping and the original outer-envelope requirement: the local cap protrudes 0.321320 pt beyond the defined neighboring envelope. A flat axial cap also does not bound radial distance. The original requirement remains the acceptance criterion; it has not been silently replaced by this local rule.

### InDesign safety and evidence tools

Executed in InDesign 21.5.1.73: eleven preservation checks pass, including rejection of an open output conflict containing an unsaved sentinel, refusal to adopt a preexisting document for cleanup, injected build failure, repeated headless entry-point invocation, and UI/document-state restoration. Export preferences now use value snapshots and report unsupported/unreadable state explicitly: PDF had 14 unverified properties, EPS one, and PNG zero. Zero differences among readable values does not mean all preferences were verified.

Independent PDF parsing reproduces the sampled Outside stroke/fill operator ordering and varying miter limits. The inspected *hqprint.pdf* and *fixture-plugin.pdf* each contain 72 text-showing operations, with path fills increasing from four to 40. Earlier counts from different artifacts must not be mixed with these. The parser has independent graphics-state, form-inheritance, and unbalanced-stack regressions.

The original render comparison's “glyph core” mask was actually eroded non-background ink. A synthetic regression reproduces 25 changed halo pixels falsely classified as fill-interior changes. The tool now labels the heuristic honestly, accepts an independent fill-only coverage mask, and refuses a silent uneroded fallback when SciPy is missing. The same test reports zero fill-core changes with the correct mask. Historical transparency safety claims are withdrawn pending remeasurement with valid references.

Subsequent remeasurement generated a separate white fill-only mask from an owned copy of fixture N, with all strokes and sampled effects disabled. All 46 word positions were unchanged. Against the historical plugin/absent PNG pair at 300 dpi, every one of the 23 frames has exactly identical RGB values on its 1,114-pixel eroded fill core: zero nonzero differences, not merely a mean below a threshold. Changes elsewhere total 43,052 pixels. This supports unchanged core color for these finite samples; it is not a general compositing proof. [Measured result](tests/audit/results/fill-mask.json).

The unstroked-victim gap is now reproduced in the current InDesign host. With the installed plugin, the first unstroked line loses 3,759 eroded fill-core pixels beneath the next line's stroke at 600 dpi. A split-frame reference restores them without moving glyphs. The all-stroked control has zero core differences, with 3,219 changed pixels elsewhere; the no-stroke control has zero differences anywhere. These controls distinguish the victim-selection defect from the normal edge difference. [Measured result](tests/audit/results/unstroked-victim.json).

The revised split/sync libraries passed 16 actual-host checks: distinct same-bounds pairs on page two, initial stale-text reconciliation, explicit formatting-only resync, fill/stroke roles, stacking, duplicate/legacy-label refusal, token/page preservation through IDML, post-import resync, and user-document/UI preservation. The top stroke is explicitly None with zero weight, both before and after IDML. The inherited restriction against zeroing text stroke weight was disproven across Text, TextStyleRange, and Character access routes. Automatic polling remains contents-only; the test invokes a document-scoped tick directly and does not certify idle-event timing. [Host record](tests/audit/results/indesign-sync.json).

The outline transaction now passes 13 actual-host checks, including an injected logger failure after complete staging, cleanup to the original item count, successful replacement, page-two placement, doubled Outside stroke width, and refusal of frame background paint. Host execution disproved two mock assumptions: outlines and duplicated paths can be returned as spread siblings, and mixed `pageItems` collections are type-ordered rather than a reliable stacking oracle. The converter tracks newly returned handles, regroups them without changing the measured coordinates, and checks direct sibling order through `allPageItems`. Unclaimed art after an ambiguous host failure is reported for recovery, not silently deleted. [Host record](tests/audit/results/indesign-outline.json).

Page-item overprint is inapplicable for Paper in the measured host; text and page-item properties must not be conflated. Paper's documented knockout semantics and None's lack of ink allow the converter to accept a false source overprint state for those swatches; true is refused. Other colors still require readable, verified assignments. [Host applicability census](tests/audit/results/page-item-overprint.json), [Adobe swatch semantics](https://helpx.adobe.com/lv/indesign/using/swatches.html). This transaction validation does not certify all rendering, save/reopen, or undo behavior.

Pure suites currently pass 170 JavaScript tests, 39 geometry/provenance tests, and ten evidence-parser/mask tests, with zero failures or skips. Native C++ mocks pass separately. These totals describe different test layers and do not substitute for host coverage. See [validation instructions](tests/README.md).

### Native architecture and distribution

The new Illustrator diagnostic compiles as an x64 native plugin with embedded PiPL and an exported entry point. Suite, registration, protocol, ownership, and rollback mocks run; they do not execute Adobe suites. It replaces only a supported straight-line stroke with a filled rectangle. Native loading and live editing are pending preservation of the user's unsaved benchmark and a host restart. It is not a miter implementation. See [the architecture audit](docs/ILLUSTRATOR_ARCHITECTURE_AUDIT.md).

[The distribution audit](docs/PLUGIN_DISTRIBUTION.md) separates InDesign prefix allocation, Illustrator effect identity, package identity, signing, and Marketplace review. The installed InDesign SDK supplies a prefix-request procedure; the project's prefix is still unassigned. Obtaining a prefix alone does not publish or install the plugin through Creative Cloud. No prefix request or listing submission was sent.
