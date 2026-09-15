# Fixture index

Registry snapshot: September 13, 2026, baseline `c4596837a5e0ea10a68f7b94e2dd368032341334`. This index points to existing files; it does not relocate or regenerate them. All original Adobe fixtures remain preserved. The load report records the user's saved benchmark and preservation-copy hash 7ced363138cca7a1a75f2cfc1cf66acca09f89c4ca6cbe3d5c5d5f77ce2485ee in the native load report; the preserved source remains separate from generated fixtures.

[The census](<../STROKE_DEFECT_AND_LIMITATION_CENSUS.md>) owns issue classification; [the support matrix](<../HOST_SUPPORT_MATRIX.md>) owns bounded support claims. [The JSON registry](<../fixtures/stroke-evidence-registry.json>) contains all fixture IDs, planned cases, raw per-artifact SHA-256 values, optional canonical LF hashes for text, and manifest subcase names/application flags.

**Preservation verified:** *tests/indesign-stroke/corpus/pristine/fixture-pristine.indd* still hashes to `3797ba73746cd6bac8cf55b689f0347e77cb622257651ac4afa5f4430ba3a783`. A current artifact hash identifies bytes; it does not prove the historical tested source or a new host execution. Raw hashes identify locally collected bytes and can differ after a checkout converts text line endings. Optional `canonical_lf_sha256` values decode strict UTF-8, replace CRLF with LF, and re-encode UTF-8; they retain any BOM and all other content. Embedded observed source/output hashes and binary hashes remain raw. The current registry contains 156 artifacts, including 113 text artifacts with this additional portable hash.

Reproduction descriptions below are experiment specifications, not newly tested commands. A generator is an existing source pointer and requires saved user work and an authorized host run before use. Work from owned copies, retain failure artifacts, never overwrite pristine data, and record the exact build/render path on rerun. “Last verified build” is the historical observation's build, not the date this index was written.

## Existing fixture families

| ID | Application / host version | Issue | Minimal reproduction | Expected native/reference result | Expected StrokeMaster result | Oracle type | Last verified build / confidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="fx-corpus"></a>FX-CORPUS | InDesign / 21.5.1.73 | Original A/B/C/D corpus | Inspect a disposable working copy; preserve duplicate A frames as historical input. | Retain historical native output; classify B as conventional miter. | Only intentionally corrected contributions may change. | Historical screen images + PDF/PS operators + raster + IDML comparison | 21.5.1.73; PROVEN |
| <a id="fx-a"></a>FX-A | InDesign / 21.5.1.73 | Inter-line, cross-frame and benign controls | Use the A manifest variant in an owned copy; compare original-order control and strokes-first PDF. | Preserve Center/z-order semantics; measured Outside overlap remains the baseline. | Fix selected same-frame halo without claiming cross-frame or unstroked-victim coverage. | HQ Print PDF → MuPDF 720 dpi and 300 ppi InDesign PNG | 21.5.1.73; PROVEN |
| <a id="fx-b"></a>FX-B | InDesign / 21.5.1.73 | Native joins, limit and weight sweep | Vary only manifest parameter; retain native conventional-miter control. | Miter within limit; bevel beyond it; round/bevel separate. | Protected native joins remain; custom envelope tested separately. | PDF operators + synthetic analytic wedge + raster | 21.5.1.73; PROVEN |
| <a id="fx-c"></a>FX-C | InDesign / 21.5.1.73 | 50 run-attribute variants including controls | One fourth-character attribute differs; use named fi/st/Bahnschrift controls. | Record actual paint interleaving; language object splitting is not automatically run splitting. | Preserve independent formatting and glyph placement; eliminate targeted occlusion only. | PDF paint order + reordered control + raster | 21.5.1.73; PROVEN |
| <a id="fx-d"></a>FX-D | InDesign / 21.5.1.73 | Native Create Outlines width/alignment | Use the named D variant; outline disposable source copies only. | Record finite conversion output, not an inferred untouched original. | Preserve fill/width/holes; fidelity and editability checked separately. | Path attributes + PDF/raster | 21.5.1.73; PROVEN |
| <a id="fx-d-contours"></a>FX-D-CONTOURS | InDesign / 21.5.1.73 | Counter orientation | Use the named D variant; outline disposable source copies only. | Record finite conversion output, not an inferred untouched original. | Preserve fill/width/holes; fidelity and editability checked separately. | Path attributes + PDF/raster | 21.5.1.73; PROVEN |
| <a id="fx-d-faithful"></a>FX-D-FAITHFUL | InDesign / 21.5.1.73 | Historical separate fill/stroke outline repair | Use the named D variant; outline disposable source copies only. | Record finite conversion output, not an inferred untouched original. | Preserve fill/width/holes; fidelity and editability checked separately. | Path attributes + PDF/raster | 21.5.1.73; PROVEN |
| <a id="fx-d-hook"></a>FX-D-HOOK | InDesign / 21.5.1.73 | Historical outline-hook experiment | Use the named D variant; outline disposable source copies only. | Record finite conversion output, not an inferred untouched original. | Preserve fill/width/holes; fidelity and editability checked separately. | Path attributes + PDF/raster | 21.5.1.73; PROVEN |
| <a id="fx-n"></a>FX-N | InDesign / 21.5.1.73 | 23 transparency/gradient/blend/overprint controls | Export the same saved N document with/without plugin, retaining all preferences. | Reference native export; no presumption that all ink is fill. | No unintended core/edge/color/separation change. | Historical PDF + PNG; original ink mask superseded | 21.5.1.73; PROVEN |
| <a id="fx-s"></a>FX-S | InDesign / 21.5.1.73 | Historical paired fill/stroke fallback | Identical control and split text placed 300 pt apart; preserve subpixel phase. | Unsplit control keeps native ordering. | Selected supported split restores halo; refusal leaves source unchanged. | No-plugin/plugin PDF/PNG pairs | 21.5.1.73; PROVEN |
| <a id="fx-t"></a>FX-T | InDesign / 21.5.1.73 | Historical live-sync fixture | Use saved single T sample; do not promote it to idle-event timing proof. | Native text reflows independently of script pairing. | Scoped resync preserves authority; broader edits remain pending. | Manifest and historical fixture | 21.5.1.73; STRONGLY SUPPORTED |
| <a id="fx-victim"></a>FX-VICTIM | InDesign / 21.5.1.73 | P1 unstroked victim | V0 first HH paragraph None; V1 both stroked; V2 both None. 30 pt/14 pt leading/12 pt stroke. | Record loaded-plugin legacy PNG, not a proved no-plugin baseline. | V0 core must equal position-preserving split reference; V1/V2 remain controls. | InDesign PNG 600 ppi, independent mask, per-input SHA-256 | 21.5.1.73; PROVEN |
| <a id="fx-fillmask"></a>FX-FILLMASK | InDesign / 21.5.1.73 | Corrected N fill-only measurement | Retain 46 word positions; make independent fill-only coverage for 23 frames. | Retain native N export as comparison input. | No eroded core change; edge/alpha checks are distinct. | PNG 300 ppi; 3 px erosion; 23×1,114 interior pixels | 21.5.1.73; PROVEN |
| <a id="fx-preserve"></a>FX-PRESERVE | InDesign / 21.5.1.73 | Owned-document preservation | Conflict with modified sentinel; repeated headless invocation; cleanup/preferences checks. | Existing user document remains valid and unchanged in measured fields. | Refuse conflict without closing it; restore readable preferences. | 11 host assertions, not render/undo proof | 21.5.1.73; PROVEN |
| <a id="fx-sync"></a>FX-SYNC | InDesign / 21.5.1.73 | Pair identity and IDML semantics | Two coincident-bound page-two pairs; edit formatting; direct tick; duplicate token; IDML reopen. | Native IDML preserves recorded labels/attributes. | Refuse ambiguous identity, preserve None+zero, resync correct pair only. | 16 host checks; model round-trip, not output fidelity | 21.5.1.73; PROVEN |
| <a id="fx-outline"></a>FX-OUTLINE | InDesign / 21.5.1.73 | Staging and paint ownership | Inject logger failure after staging; then successful page-two conversion and background refusal. | Capture actual outline/spread-sibling behavior. | Roll back owned temporary art; replace source only after checks. | 13 host assertions; no universal raster equivalence | 21.5.1.73; PROVEN |
| <a id="fx-zero"></a>FX-ZERO | InDesign / 21.5.1.73 | None versus weight | Apply six methods on four text-access routes. | None-only can retain 3 pt; zero must be assigned separately. | Top must have both empty stroke color and zero weight when required. | 24 recorded DOM rows | 21.5.1.73; PROVEN |
| <a id="fx-overprint"></a>FX-OVERPRINT | InDesign / 21.5.1.73 | Page-item Paper/None overprint applicability | Compare the probe's four swatch cases; do not infer text from path properties. | Record applicability/read failures as well as values. | Accept/refuse only under explicit swatch rule. | Host property observations; separation output unexecuted | 21.5.1.73; PROVEN |
| <a id="fx-ai-outline"></a>FX-AI-OUTLINE | Illustrator / 30.7.0 114R (historical narrative) | Singular outline access and cleanup | Four original/collection/duplicate outline routes and injected-failure cleanup, in disposable document. | createOutline() works on measured target; plural call not a general API. | Never certify full Appearance from leaf attributes. | 17 host checks reported in takeover narrative; compact raw check record not located | 30.7.0 114R (historical narrative); STRONGLY SUPPORTED |
| <a id="fx-ai-regions"></a>FX-AI-REGIONS | Illustrator / 30.7.0 114R | Basic path and finite region primitives | Two overlapping squares; separate square with hole. | Unite area magnitude 15,000 pt²; constructed-hole area 7,500 pt². | No arbitrary-hole/self-intersection/transform promise. | 10 host assertions; signed area/basic style values | 30.7.0 114R; PROVEN |
| <a id="fx-ai-build"></a>FX-AI-BUILD | Illustrator / No host execution; x64 build only | Native diagnostic build identity | Inspect build report; later host experiment requires independent manual gate. | No native output observed. | Progress must move through discovery/load/init/registration/callback/art/clean-close separately. | Binary SHA-256 + embedded PiPL; no host oracle | No host execution; x64 build only; PROVEN |
| <a id="fx-ai-extreme"></a>FX-AI-EXTREME | Illustrator / 30.7.0 114R (takeover attribution) | Appearance/font provenance/renderer disagreement | Keep original benchmark preserved; compare existing extracted contours and embedded font; do not install/copy font binaries. | Existing PDF uses width 3 and limit 4; expected intended geometry unresolved. | Preserve ordinary exterior joins; resolve custom envelope with vector oracle. | Embedded-font census + two rasterizers at 864 dpi; renderer versions missing | 30.7.0 114R (takeover attribution); STRONGLY SUPPORTED |
| <a id="fx-global"></a>FX-GLOBAL | Neither; pure geometry / No host execution | Local clip/global-envelope counterexample | Analytic outer rectangle/inner triangular hole gap; preserve admitted exterior miters. | Conventional native miter is a control, not the custom target. | Retain fill and protected joins while satisfying chosen global envelope. | Analytic containment/counterexamples, not host or font proof | No host execution; PROVEN |
| <a id="fx-victim20"></a>FX-VICTIM20 | InDesign / 21.5.1.73 | 20-case victim/no-harm/decoration census with plugin-off/on controls | Owned HH frame grid, 30 pt/14 pt/12 pt default; one property varied per VC00–VC19. Old-plugin and no-plugin runs both complete with glyphPositionsUnchanged. | No-plugin native.png is the controlled native reference; old-plugin native.png is not native-only. Comparison direction is explicit in intake. | Repair victims and preserve benign/decoration output; old binary fails VC00, VC11/12/13 and VC15 against the stated oracles. | InDesign RGB opaque PNG 600 ppi; 6 px erosion; independent stroke-before-fill reference and separate plugin-off/on comparison | 21.5.1.73; legacy plugin d476f565ae57fa9257111d5d86f4e8a313c9efb7bc85d8912e9b5842ec80f342; PROVEN |
| <a id="fx-ai-load"></a>FX-AI-LOAD | Illustrator / 30.7.0 114R | First actual native load, rejected callback, and later unattributed host crashes | Installed native-prep-2 after user-preserved benchmark; strict bridge; control then effect specimen. | Control creates one editable open two-anchor path without effect callback. | Effect callback must accept input and replace its stroke; actual first attempt rejects with 1346458189, mutations 0, origin 0. | Native event journal, bridge counters, control PDF, and retained Windows event report; no effect export; later crashes with unknown attribution | 30.7.0 114R; native-prep-2 / 4eb883eebc07c21acc1c8e13a366475713f7501969b61309b51cbbcbe5f0b5fc; PROVEN |
| <a id="fx-persistence20"></a>FX-PERSISTENCE20 | InDesign / 21.5.1.73 | Native persistence of 20 victim-census frames without plugin | Export generated no-plugin census, reopen IDML, save/reopen INDD, compare sampled fields, PNG, PDF raster, and PDF text. | Sampled state and positions, direct/reopened PNG, PDF raster, and PDF text remain equal. | Not exercised: plugin absent and no script-pair round-trip. | RGB opaque PNG 300 ppi: 0 changed pixels; PDF/X-4:2008 → MuPDF 288 ppi: 0 channel differences; extracted text equal. PyMuPDF package 1.28.2; PNG 300 ppi and PDF raster 288 ppi recorded. | 21.5.1.73, plugin absent; retained host execution; PROVEN |
| <a id="fx-pair-render"></a>FX-PAIR-RENDER | InDesign / 21.5.1.73 | Two split pairs: rendered INDD/IDML persistence | Uniform and varied Outside strokes; split, INDD save/reopen, IDML export/import, then INDD save/reopen. | Saved editable paired frames retain their labels and sampled state. | ISE_SPLIT/ISE3 identity and fill/stroke roles persist; top stroke is None and zero; lower fill matches source stroke color and tint; sampled fields include fillTint; final PNG equals pre-save PNG. | Two pairs, four frames; repeated per-character host assertions; RGB opaque PNG 600 ppi, exact matching SHA-256 and 0 changed pixels. | 21.5.1.73; safe-refusal binary 15b802ebfb38f567eb3aa586e995c0a1afff47efd078f3e2a0443596b54df4c4; PROVEN |
| <a id="fx-id-refusal"></a>FX-ID-REFUSAL | InDesign / 21.5.1.73 | Safe-refusal binary preserves 20-frame output | Same 20-case input with plugin absent versus safe-refusal binary; compare entire native, reference, and mask PNGs. | Plugin-absent whole-page PNGs define unchanged output. | Correction disabled; native, reference, and mask PNGs unchanged. No corrected output claim. | InDesign 21.5.1.73, RGB PNG 600 ppi: identical hashes and 0 changed pixels for all three whole-page images. | 21.5.1.73; safe-refusal binary 15b802ebfb38f567eb3aa586e995c0a1afff47efd078f3e2a0443596b54df4c4; PROVEN |
| <a id="fx-ai-reject"></a>FX-AI-REJECT | Illustrator / 30.7.0 114R | Prep-3 named callback rejection | Control and effect specimens; retain processing path/style and named rejection event. | Control has an editable two-anchor path and no effect callback. | Replacement required; actual input reaches Go but fails stroke.keys-missing with zero mutations. | Native journal: two open anchors, fill=0, stroke=0; five Go suites acquired with error 0; no effect export. | native-prep-3 / 3cbb7fda5b259d75256cfa2d72b3e838521a3b3cc82a86994b91ab39f03be796; PROVEN |
| <a id="fx-ai-prep4"></a>FX-AI-PREP4 | Illustrator / 30.7.0 114R; PIDs 64996, 78916, and 38016 | Three historical prep-4 native lifecycle runs | New open, straight, two-anchor specimens; endpoint edits and same-process AI save/reopen in each run. | Control remains an ordinary stroked line. | Three successful replacements per run, error 0; no arbitrary style, miter, or missing-plugin claim. | PID/counter and editable-source checks; first-run independent MuPDF geometry; direct-COM exit observed for PID 38016 only; broad stability unverified. | native-prep-4 / 45fab742d70bfb12497b3735a1e3c9d4b14dd8dc9195551f0221f1c4297b1c78; PROVEN for bounded specimen operations |
| <a id="fx-topology"></a>FX-TOPOLOGY | Standalone C++ / Boost 1.74; no Adobe host | Bounded filled-region and union-precision experiment | Known polygons, protected witnesses, remote/tight multipart envelopes, and seeded orthogonal unions. | Native integration UNEXECUTED. | Preserve admitted polygon geometry or refuse unsupported/coarse input without output. | Independent area and membership, 24-order controls, 80 fuzz cases, and 43 API checks. | Snapshot f92abc24…; APPROVED for bounded standalone scope |
| <a id="fx-ai-prep5"></a>FX-AI-PREP5 | Illustrator / 30.7.0 114R; PID 5324 | Current straight-line lifecycle and COM exit | New two-anchor specimens; source edits, same-process AI reopen, and exports; separate zero-document COM quit. | Ordinary control stroke retained. | Three replacements; latest error 0; no native region claim. | 332 assertions including 27 interval checks; PID/start-matched exit in 4.021051 s. | native-prep-5 / 201dd19cb25174c7fb9628fd1bf2769aa41e7d4dd4f2c6b77e5e9d7aea0fc9c7; PROVEN bounded run |

## Artifact locations and manifest subcases


A family is the saved repro/document or diagnostic transaction; a manifest entry is a subcase. Registry subcase IDs use `FAMILY/manifest-key`. For FX-VICTIM20, old-plugin and no-plugin manifests compare equal; both source files are inventoried. Missing manifest application-status fields do not mean a test passed. Paths under *build/* are local ignored evidence, not guaranteed available in another checkout. The indexed September 13 records under *tests/audit/results/* are the portable evidence entry points; copying summaries does not make the raw images portable.

### FX-CORPUS artifacts

- [*tests/indesign-stroke/corpus/pristine/fixture-pristine.indd*](<../tests/indesign-stroke/corpus/pristine/fixture-pristine.indd>)
- [*tests/indesign-stroke/corpus/actual/fixture-exports/hqprint.pdf*](<../tests/indesign-stroke/corpus/actual/fixture-exports/hqprint.pdf>)
- [*tests/indesign-stroke/corpus/actual/screen/normal-view.jpg*](<../tests/indesign-stroke/corpus/actual/screen/normal-view.jpg>)
- [*tests/indesign-stroke/corpus/actual/screen/cpu-preview.jpg*](<../tests/indesign-stroke/corpus/actual/screen/cpu-preview.jpg>)
- [*tests/indesign-stroke/corpus/actual/screen/overprint-preview.jpg*](<../tests/indesign-stroke/corpus/actual/screen/overprint-preview.jpg>)
- [*tests/indesign-stroke/corpus/actual/screen/gpu-perf-off.jpg*](<../tests/indesign-stroke/corpus/actual/screen/gpu-perf-off.jpg>)
- [*tests/indesign-stroke/corpus/actual/idml-roundtrip/idml-hqprint.pdf*](<../tests/indesign-stroke/corpus/actual/idml-roundtrip/idml-hqprint.pdf>)
- [*docs/STROKE_FIXTURE_CENSUS.md*](<../docs/STROKE_FIXTURE_CENSUS.md>)

Manifest: [*tests/indesign-stroke/corpus/actual/fixture-exports/hqprint.manifest.json*](<../tests/indesign-stroke/corpus/actual/fixture-exports/hqprint.manifest.json>). 5 subcases; manifest host field 21.5.1.73.

`FX-CORPUS/`: ; `FX-CORPUS/`: ; `FX-CORPUS/`: ; `FX-CORPUS/`: ; `FX-CORPUS/`: .

### FX-A artifacts

- [*tests/indesign-stroke/A-adjacent-line-overlap/minimal/A-minimal.indd*](<../tests/indesign-stroke/A-adjacent-line-overlap/minimal/A-minimal.indd>)
- [*tests/indesign-stroke/A-adjacent-line-overlap/actual-before-plugin/A-minimal.probe.json*](<../tests/indesign-stroke/A-adjacent-line-overlap/actual-before-plugin/A-minimal.probe.json>)
- [*tests/indesign-stroke/A-adjacent-line-overlap/actual/A-minimal.probe.json*](<../tests/indesign-stroke/A-adjacent-line-overlap/actual/A-minimal.probe.json>)

Existing generator/probe: [*tools/jsx/build-A-minimal.jsx*](<../tools/jsx/build-A-minimal.jsx>). This source snapshot is not automatically the historical tested revision.

Manifest: [*tests/indesign-stroke/A-adjacent-line-overlap/actual-before-plugin/A-minimal.manifest.json*](<../tests/indesign-stroke/A-adjacent-line-overlap/actual-before-plugin/A-minimal.manifest.json>). 15 subcases; manifest host field 21.5.1.73.

`FX-A/A01 two paragraphs, stroke 8 outside`: A01 two paragraphs, stroke 8 outside; `FX-A/A02 forced line break`: A02 forced line break; `FX-A/A03 soft wrap`: A03 soft wrap; `FX-A/A04 center alignment, stroke 16`: A04 center alignment, stroke 16; `FX-A/A05 control, leading 60`: A05 control, leading 60; `FX-A/A09a stroke 3`: A09a stroke 3; `FX-A/A09b stroke 5`: A09b stroke 5; `FX-A/A08 lower (created first)`: A08 lower (created first); `FX-A/A08 upper (created second, in front)`: A08 upper (created second, in front); `FX-A/A06 lower, story start`: A06 lower, story start; `FX-A/A06 upper, story continues (in front)`: A06 upper, story continues (in front); `FX-A/A07 lower, story start (in front)`: A07 lower, story start (in front); `FX-A/A07 upper, story continues (behind)`: A07 upper, story continues (behind); `FX-A/A10 two runs on line 1`: A10 two runs on line 1; `FX-A/A11 frame text opacity 90`: A11 frame text opacity 90.

### FX-B artifacts

- [*tests/indesign-stroke/B-miter-spikes/minimal/B-miter.indd*](<../tests/indesign-stroke/B-miter-spikes/minimal/B-miter.indd>)
- [*tests/indesign-stroke/B-miter-spikes/actual-before-plugin/B-miter.probe.json*](<../tests/indesign-stroke/B-miter-spikes/actual-before-plugin/B-miter.probe.json>)

Existing generator/probe: [*tools/jsx/build-B-miter.jsx*](<../tools/jsx/build-B-miter.jsx>). This source snapshot is not automatically the historical tested revision.

Manifest: [*tests/indesign-stroke/B-miter-spikes/actual-before-plugin/B-miter.manifest.json*](<../tests/indesign-stroke/B-miter-spikes/actual-before-plugin/B-miter.manifest.json>). 39 subcases; manifest host field 21.5.1.73.

`FX-B/B1 text miter limit 1`: B1 text miter limit 1; `FX-B/B1 text miter limit 1.5`: B1 text miter limit 1.5; `FX-B/B1 text miter limit 2`: B1 text miter limit 2; `FX-B/B1 text miter limit 3`: B1 text miter limit 3; `FX-B/B1 text miter limit 4`: B1 text miter limit 4; `FX-B/B1 text miter limit 10`: B1 text miter limit 10; `FX-B/B1 text miter limit 100`: B1 text miter limit 100; `FX-B/B1 text miter limit 500`: B1 text miter limit 500; `FX-B/B2 center alignment 8 pt, limit 4`: B2 center alignment 8 pt, limit 4; `FX-B/B2 center alignment 8 pt, limit 1`: B2 center alignment 8 pt, limit 1; `FX-B/B2 weight 1, limit 4`: B2 weight 1, limit 4; `FX-B/B2 weight 2, limit 4`: B2 weight 2, limit 4; `FX-B/B2 weight 6, limit 4`: B2 weight 6, limit 4; `FX-B/B2 weight 8, limit 4`: B2 weight 8, limit 4; `FX-B/B3 wedge 10 deg outside`: B3 wedge 10 deg outside; `FX-B/B3 wedge 15 deg outside`: B3 wedge 15 deg outside; `FX-B/B3 wedge 20 deg outside`: B3 wedge 20 deg outside; `FX-B/B3 wedge 25 deg outside`: B3 wedge 25 deg outside; `FX-B/B3 wedge 28 deg outside`: B3 wedge 28 deg outside; `FX-B/B3 wedge 30 deg outside`: B3 wedge 30 deg outside; `FX-B/B3 wedge 35 deg outside`: B3 wedge 35 deg outside; `FX-B/B3 wedge 40 deg outside`: B3 wedge 40 deg outside; `FX-B/B3 wedge 50 deg outside`: B3 wedge 50 deg outside; `FX-B/B3 wedge 60 deg outside`: B3 wedge 60 deg outside; `FX-B/B3 wedge 90 deg outside`: B3 wedge 90 deg outside; `FX-B/B3 wedge 10 deg center`: B3 wedge 10 deg center; `FX-B/B3 wedge 15 deg center`: B3 wedge 15 deg center; `FX-B/B3 wedge 20 deg center`: B3 wedge 20 deg center; `FX-B/B3 wedge 25 deg center`: B3 wedge 25 deg center; `FX-B/B3 wedge 28 deg center`: B3 wedge 28 deg center; `FX-B/B3 wedge 30 deg center`: B3 wedge 30 deg center; `FX-B/B3 wedge 35 deg center`: B3 wedge 35 deg center; `FX-B/B3 wedge 40 deg center`: B3 wedge 40 deg center; `FX-B/B3 wedge 50 deg center`: B3 wedge 50 deg center; `FX-B/B3 wedge 60 deg center`: B3 wedge 60 deg center; `FX-B/B3 wedge 90 deg center`: B3 wedge 90 deg center; `FX-B/B4 source text, limit 4`: B4 source text, limit 4; `FX-B/`: ; `FX-B/`: .

### FX-C artifacts

- [*tests/indesign-stroke/C-run-fragmentation/minimal/C-attribute-sweep.indd*](<../tests/indesign-stroke/C-run-fragmentation/minimal/C-attribute-sweep.indd>)
- [*tests/indesign-stroke/C-run-fragmentation/actual-before-plugin/C-attribute-sweep.probe.json*](<../tests/indesign-stroke/C-run-fragmentation/actual-before-plugin/C-attribute-sweep.probe.json>)
- [*tests/indesign-stroke/C-run-fragmentation/actual/C-attribute-sweep.probe.json*](<../tests/indesign-stroke/C-run-fragmentation/actual/C-attribute-sweep.probe.json>)

Existing generator/probe: [*tools/jsx/build-C-attribute-sweep.jsx*](<../tools/jsx/build-C-attribute-sweep.jsx>). This source snapshot is not automatically the historical tested revision.

Manifest: [*tests/indesign-stroke/C-run-fragmentation/actual-before-plugin/C-attribute-sweep.manifest.json*](<../tests/indesign-stroke/C-run-fragmentation/actual-before-plugin/C-attribute-sweep.manifest.json>). 50 subcases; manifest host field 21.5.1.73.

`FX-C/C00`: C00 control: no override (applied); `FX-C/C01`: C01 fill color Magenta (applied); `FX-C/C02`: C02 fill tint 50 (applied); `FX-C/C03`: C03 stroke color Magenta (applied); `FX-C/C04`: C04 stroke tint 50 (applied); `FX-C/C05`: C05 stroke weight 5 (applied); `FX-C/C06`: C06 stroke weight 4 set locally (same value as style) (applied); `FX-C/C07`: C07 join Miter (applied); `FX-C/C08`: C08 miter limit 10 with round join (no visual effect) (applied); `FX-C/C09`: C09 stroke alignment Center (applied); `FX-C/C10`: C10 overprint stroke (applied); `FX-C/C11`: C11 overprint fill (applied); `FX-C/C12`: C12 font change (Minion Pro Bold) (applied); `FX-C/C13`: C13 point size 23.9 (applied); `FX-C/C14`: C14 horizontal scale 99 % (applied); `FX-C/C15`: C15 vertical scale 101 % (applied); `FX-C/C16`: C16 baseline shift 0.5 pt (applied); `FX-C/C17`: C17 tracking 20 (applied); `FX-C/C18`: C18 manual kerning -50 before character 3 (applied); `FX-C/C19`: C19 language English: UK (applied); `FX-C/C20`: C20 skew -10 (applied); `FX-C/C21`: C21 character rotation 10 (applied); `FX-C/C22`: C22 empty character style (applied); `FX-C/C23`: C23 character style with fill Magenta (applied); `FX-C/C24`: C24 underline (applied); `FX-C/C25`: C25 underline color set, underline off (no visual effect) (applied); `FX-C/C26`: C26 strikethrough (applied); `FX-C/C27`: C27 Lowercase on a lowercase letter (no visual effect) (applied); `FX-C/C28`: C28 Small Caps (applied); `FX-C/C29`: C29 Superscript (applied); `FX-C/C30`: C30 No Break (no visual effect) (applied); `FX-C/C31`: C31 OpenType slashed zero on a letter (no visual effect) (applied); `FX-C/C32`: C32 OpenType oldstyle figures on a letter (no visual effect) (applied); `FX-C/C33c`: C33c control for C33: text with fi (applied); `FX-C/C33`: C33 ligatures off on fi (applied); `FX-C/C34c`: C34c control for C34: text with st (applied); `FX-C/C34`: C34 discretionary ligatures on st (applied); `FX-C/C35`: C35 GREP style applying the empty character style to character 3 (applied); `FX-C/C36`: C36 GREP style applying the magenta character style to character 3 (applied); `FX-C/C37`: C37 nested style applying the empty character style through 4 characters (applied); `FX-C/C38`: C38 conditional text condition, indicators hidden (applied); `FX-C/C39`: C39 hyperlink text source on character 3 (applied); `FX-C/C40`: C40 XML tag on character 3 (applied); `FX-C/C41`: C41 OpenType stylistic set 1 (applied); `FX-C/C42c`: C42c control for C42: variable font Bahnschrift (applied); `FX-C/C42`: C42 variable font axis on character 3 (Bahnschrift) (applied); `FX-C/C43`: C43 fill color Black set locally (same value as style) (applied); `FX-C/C44`: C44 kerning method Optical on character 3 (applied); `FX-C/C45`: C45 stroke tint 100 set explicitly (style uses the swatch tint) (applied); `FX-C/C46`: C46 variable font named instance Bold on character 3 (Bahnschrift) (applied).

### FX-D artifacts

- [*tests/indesign-stroke/D-create-outlines/actual/D-outlines.probe.json*](<../tests/indesign-stroke/D-create-outlines/actual/D-outlines.probe.json>)
- [*tests/indesign-stroke/D-create-outlines/NOTES.md*](<../tests/indesign-stroke/D-create-outlines/NOTES.md>)

Manifest: [*tests/indesign-stroke/D-create-outlines/actual/D-outlines.manifest.json*](<../tests/indesign-stroke/D-create-outlines/actual/D-outlines.manifest.json>). 11 subcases; manifest host field 21.5.1.73.

`FX-D/D01 Outside 4 pt round`: D01 Outside 4 pt round; `FX-D/`: ; `FX-D/D02 Center 8 pt round`: D02 Center 8 pt round; `FX-D/`: ; `FX-D/D03 Outside 4 pt miter limit 10`: D03 Outside 4 pt miter limit 10; `FX-D/`: ; `FX-D/D04 two lines`: D04 two lines; `FX-D/`: ; `FX-D/D05 Outside 4 pt, outlines set to Outside`: D05 Outside 4 pt, outlines set to Outside; `FX-D/`: ; `FX-D/`: .

### FX-D-CONTOURS artifacts

- [*tests/indesign-stroke/D-create-outlines/actual/D-contours.probe.json*](<../tests/indesign-stroke/D-create-outlines/actual/D-contours.probe.json>)
- [*tests/indesign-stroke/D-create-outlines/NOTES.md*](<../tests/indesign-stroke/D-create-outlines/NOTES.md>)

Manifest: [*tests/indesign-stroke/D-create-outlines/actual/D-contours.manifest.json*](<../tests/indesign-stroke/D-create-outlines/actual/D-contours.manifest.json>). 6 subcases; manifest host field 21.5.1.73.

`FX-D-CONTOURS/K01 live text`: K01 live text; `FX-D-CONTOURS/K01 outlines, Outside, contours as converted`: K01 outlines, Outside, contours as converted; `FX-D-CONTOURS/K02 live text`: K02 live text; `FX-D-CONTOURS/K02 outlines, Outside, inner contours reversed`: K02 outlines, Outside, inner contours reversed; `FX-D-CONTOURS/K03 live text`: K03 live text; `FX-D-CONTOURS/K03 outlines, Outside, all contours reversed`: K03 outlines, Outside, all contours reversed.

### FX-D-FAITHFUL artifacts

- [*tests/indesign-stroke/D-create-outlines/actual/D-faithful.probe.json*](<../tests/indesign-stroke/D-create-outlines/actual/D-faithful.probe.json>)
- [*tests/indesign-stroke/D-create-outlines/NOTES.md*](<../tests/indesign-stroke/D-create-outlines/NOTES.md>)

Manifest: [*tests/indesign-stroke/D-create-outlines/actual/D-faithful.manifest.json*](<../tests/indesign-stroke/D-create-outlines/actual/D-faithful.manifest.json>). 10 subcases; manifest host field 21.5.1.73.

`FX-D-FAITHFUL/F01 Outside 4 pt round`: F01 Outside 4 pt round; `FX-D-FAITHFUL/F01 Outside 4 pt round (converted copy) (faithful outlines)`: F01 Outside 4 pt round (converted copy) (faithful outlines); `FX-D-FAITHFUL/F02 Center 8 pt round`: F02 Center 8 pt round; `FX-D-FAITHFUL/F02 Center 8 pt round (converted copy) (faithful outlines)`: F02 Center 8 pt round (converted copy) (faithful outlines); `FX-D-FAITHFUL/F03 Outside 4 pt miter limit 10`: F03 Outside 4 pt miter limit 10; `FX-D-FAITHFUL/F03 Outside 4 pt miter limit 10 (converted copy) (faithful outlines)`: F03 Outside 4 pt miter limit 10 (converted copy) (faithful outlines); `FX-D-FAITHFUL/F04 Outside 8 pt two lines at 24 pt`: F04 Outside 8 pt two lines at 24 pt; `FX-D-FAITHFUL/F04 Outside 8 pt two lines at 24 pt (converted copy) (faithful outlines)`: F04 Outside 8 pt two lines at 24 pt (converted copy) (faithful outlines); `FX-D-FAITHFUL/F05 Outside 4 pt counters`: F05 Outside 4 pt counters; `FX-D-FAITHFUL/F05 Outside 4 pt counters (converted copy) (faithful outlines)`: F05 Outside 4 pt counters (converted copy) (faithful outlines).

### FX-D-HOOK artifacts

- [*tests/indesign-stroke/D-create-outlines/actual/D-hook.probe.json*](<../tests/indesign-stroke/D-create-outlines/actual/D-hook.probe.json>)
- [*tests/indesign-stroke/D-create-outlines/NOTES.md*](<../tests/indesign-stroke/D-create-outlines/NOTES.md>)

Manifest: [*tests/indesign-stroke/D-create-outlines/actual/D-hook.manifest.json*](<../tests/indesign-stroke/D-create-outlines/actual/D-hook.manifest.json>). 4 subcases; manifest host field 21.5.1.73.

`FX-D-HOOK/H01 live text`: H01 live text; `FX-D-HOOK/H02 live text`: H02 live text; `FX-D-HOOK/`: ; `FX-D-HOOK/`: .

### FX-N artifacts

- [*tests/indesign-stroke/N-transparency/minimal/N-transparency.indd*](<../tests/indesign-stroke/N-transparency/minimal/N-transparency.indd>)
- [*tests/indesign-stroke/N-transparency/actual/N-transparency.pdf*](<../tests/indesign-stroke/N-transparency/actual/N-transparency.pdf>)
- [*tests/indesign-stroke/N-transparency/actual-noplugin/N-transparency.pdf*](<../tests/indesign-stroke/N-transparency/actual-noplugin/N-transparency.pdf>)

Existing generator/probe: [*tools/jsx/build-N-transparency.jsx*](<../tools/jsx/build-N-transparency.jsx>). This source snapshot is not automatically the historical tested revision.

Manifest: [*tests/indesign-stroke/N-transparency/actual/N-transparency.manifest.json*](<../tests/indesign-stroke/N-transparency/actual/N-transparency.manifest.json>). 23 subcases; manifest host field 21.5.1.73.

`FX-N/N00`: N00 control: Normal, opaque, solid (applied); `FX-N/N01`: N01 text blend Multiply (applied); `FX-N/N02`: N02 text blend Screen (applied); `FX-N/N03`: N03 text blend Overlay (applied); `FX-N/N04`: N04 text blend Darken (applied); `FX-N/N05`: N05 text blend Lighten (applied); `FX-N/N06`: N06 text blend Difference (applied); `FX-N/N07`: N07 text opacity 50 % (applied); `FX-N/N08`: N08 text blend Multiply at 50 % (applied); `FX-N/N09`: N09 object blend Multiply (applied); `FX-N/N10`: N10 object opacity 50 % (applied); `FX-N/N11`: N11 gradient fill, solid stroke (applied); `FX-N/N12`: N12 solid fill, gradient stroke (applied); `FX-N/N13`: N13 gradient fill and gradient stroke (applied); `FX-N/N14`: N14 overprint fill (applied); `FX-N/N15`: N15 overprint stroke (applied); `FX-N/N16`: N16 overprint fill and stroke (applied); `FX-N/N17`: N17 spot color fill, overprint (applied); `FX-N/N18`: N18 drop shadow on text (applied); `FX-N/N19`: N19 feather on text (applied); `FX-N/N20`: N20 CONTROL: Center stroke, blend Multiply (applied); `FX-N/N21`: N21 CONTROL: no stroke, blend Multiply (applied); `FX-N/N22`: N22 CONTROL: no stroke, gradient fill (applied).

### FX-S artifacts

- [*tests/indesign-stroke/S-split/actual-noplugin/S-split.probe.json*](<../tests/indesign-stroke/S-split/actual-noplugin/S-split.probe.json>)
- [*tests/indesign-stroke/S-split/actual/S-split.probe.json*](<../tests/indesign-stroke/S-split/actual/S-split.probe.json>)
- [*tests/indesign-stroke/S-split/NOTES.md*](<../tests/indesign-stroke/S-split/NOTES.md>)

Existing generator/probe: [*tools/jsx/build-S-split.jsx*](<../tools/jsx/build-S-split.jsx>). This source snapshot is not automatically the historical tested revision.

Manifest: [*tests/indesign-stroke/S-split/actual/S-split.manifest.json*](<../tests/indesign-stroke/S-split/actual/S-split.manifest.json>). 10 subcases; manifest host field 21.5.1.73.

`FX-S/S01c`: S01c uniform: black fill, Paper 8 pt Outside [control]; `FX-S/S01t`: S01t uniform: black fill, Paper 8 pt Outside [split]; `FX-S/S02c`: S02c varied fill: one magenta character on line one [control]; `FX-S/S02t`: S02t varied fill: one magenta character on line one [split]; `FX-S/S03c`: S03c varied stroke: one character at 5 pt magenta stroke [control]; `FX-S/S03t`: S03t varied stroke: one character at 5 pt magenta stroke [split]; `FX-S/S04c`: S04c CONTROL: no stroke at all (split must refuse) [control]; `FX-S/S04t`: S04t CONTROL: no stroke at all (split must refuse) [split]; `FX-S/S05c`: S05c Center alignment (split acts; records what it does) [control]; `FX-S/S05t`: S05t Center alignment (split acts; records what it does) [split].

### FX-T artifacts

- [*tests/indesign-stroke/T-livesync/actual/T-livesync.manifest.json*](<../tests/indesign-stroke/T-livesync/actual/T-livesync.manifest.json>)

Manifest: [*tests/indesign-stroke/T-livesync/actual/T-livesync.manifest.json*](<../tests/indesign-stroke/T-livesync/actual/T-livesync.manifest.json>). 1 subcases; manifest host field 21.5.1.73.

`FX-T/T01f`: T01 live sync [ISE fill].

### FX-VICTIM artifacts

- [*tests/audit/results/unstroked-victim.json*](<../tests/audit/results/unstroked-victim.json>)
- [*build/takeover/unstroked-victim/native.png*](<../build/takeover/unstroked-victim/native.png>) — local ignored artifact.
- [*build/takeover/unstroked-victim/split.png*](<../build/takeover/unstroked-victim/split.png>) — local ignored artifact.
- [*build/takeover/unstroked-victim/mask.png*](<../build/takeover/unstroked-victim/mask.png>) — local ignored artifact.

Existing generator/probe: [*tools/jsx/probe-unstroked-victim.jsx*](<../tools/jsx/probe-unstroked-victim.jsx>). This source snapshot is not automatically the historical tested revision.

Manifest: [*build/takeover/unstroked-victim/manifest.json*](<../build/takeover/unstroked-victim/manifest.json>). 3 subcases; manifest host field absent; use linked host result.

`FX-VICTIM/V0`: V0; `FX-VICTIM/V1`: V1; `FX-VICTIM/V2`: V2.

### FX-FILLMASK artifacts

- [*tests/audit/results/fill-mask.json*](<../tests/audit/results/fill-mask.json>)

Existing generator/probe: [*tools/jsx/probe-fill-mask.jsx*](<../tools/jsx/probe-fill-mask.jsx>). This source snapshot is not automatically the historical tested revision.

### FX-PRESERVE artifacts

- [*tests/audit/results/indesign-preservation.json*](<../tests/audit/results/indesign-preservation.json>)

Existing generator/probe: [*tools/jsx/probe-takeover-safety.jsx*](<../tools/jsx/probe-takeover-safety.jsx>). This source snapshot is not automatically the historical tested revision.

### FX-SYNC artifacts

- [*tests/audit/results/indesign-sync.json*](<../tests/audit/results/indesign-sync.json>)

Existing generator/probe: [*tools/jsx/probe-takeover-sync.jsx*](<../tools/jsx/probe-takeover-sync.jsx>). This source snapshot is not automatically the historical tested revision.

### FX-OUTLINE artifacts

- [*tests/audit/results/indesign-outline.json*](<../tests/audit/results/indesign-outline.json>)

Existing generator/probe: [*tools/jsx/probe-takeover-outline.jsx*](<../tools/jsx/probe-takeover-outline.jsx>). This source snapshot is not automatically the historical tested revision.

### FX-ZERO artifacts

- [*tests/audit/results/text-zero-stroke.json*](<../tests/audit/results/text-zero-stroke.json>)

Existing generator/probe: [*tools/jsx/probe-zero-stroke.jsx*](<../tools/jsx/probe-zero-stroke.jsx>). This source snapshot is not automatically the historical tested revision.

### FX-OVERPRINT artifacts

- [*tests/audit/results/page-item-overprint.json*](<../tests/audit/results/page-item-overprint.json>)

Existing generator/probe: [*tools/jsx/probe-outline-paint.jsx*](<../tools/jsx/probe-outline-paint.jsx>). This source snapshot is not automatically the historical tested revision.

### FX-AI-OUTLINE artifacts

- [*PROJECT_TAKEOVER_AUDIT.md*](<../PROJECT_TAKEOVER_AUDIT.md>)
- [*tests/README.md*](<../tests/README.md>)

Existing generator/probe: [*tools/jsx/ai-probe-outline-dom.jsx*](<../tools/jsx/ai-probe-outline-dom.jsx>). This source snapshot is not automatically the historical tested revision.

### FX-AI-REGIONS artifacts

- [*tests/audit/results/illustrator-regions.json*](<../tests/audit/results/illustrator-regions.json>)

Existing generator/probe: [*tools/jsx/ai-probe-regions.jsx*](<../tools/jsx/ai-probe-regions.jsx>). This source snapshot is not automatically the historical tested revision.

### FX-AI-BUILD artifacts

- [*tests/audit/results/illustrator-build.json*](<../tests/audit/results/illustrator-build.json>)
- [*plugin/illustrator-probe/host-proof.jsx*](<../plugin/illustrator-probe/host-proof.jsx>)

### FX-AI-EXTREME artifacts

- [*tests/geometry/extreme_font_provenance.json*](<../tests/geometry/extreme_font_provenance.json>)
- [*tests/geometry/extreme_join_statistics.json*](<../tests/geometry/extreme_join_statistics.json>)
- [*tests/audit/results/extreme-renderer-difference.json*](<../tests/audit/results/extreme-renderer-difference.json>)
- [*build/takeover/extreme.pdf*](<../build/takeover/extreme.pdf>) — local ignored artifact.

### FX-GLOBAL artifacts

- [*tests/geometry/local_vs_global_envelope.json*](<../tests/geometry/local_vs_global_envelope.json>)
- [*docs/MITER_GEOMETRY_CONTRACT.md*](<../docs/MITER_GEOMETRY_CONTRACT.md>)
- [*docs/REGION_OFFSET_EXPERIMENT.md*](<../docs/REGION_OFFSET_EXPERIMENT.md>)

### FX-VICTIM20 artifacts

- [*tests/audit/results/host-victim-census.json*](<../tests/audit/results/host-victim-census.json>) — portable candidate evidence.

- [*build/host-census/victims-baseline/host.json*](<../build/host-census/victims-baseline/host.json>) — local ignored artifact.
- [*build/host-census/victims-noplugin/host.json*](<../build/host-census/victims-noplugin/host.json>) — local ignored artifact.
- [*build/host-census/victims-baseline/fixture.indd*](<../build/host-census/victims-baseline/fixture.indd>) — local ignored artifact.
- [*build/host-census/victims-noplugin/fixture.indd*](<../build/host-census/victims-noplugin/fixture.indd>) — local ignored artifact.
- [*build/host-census/victims-baseline/native.png*](<../build/host-census/victims-baseline/native.png>) — local ignored artifact.
- [*build/host-census/victims-noplugin/native.png*](<../build/host-census/victims-noplugin/native.png>) — local ignored artifact.
- [*build/host-census/victims-baseline/reference.png*](<../build/host-census/victims-baseline/reference.png>) — local ignored artifact.
- [*build/host-census/victims-baseline/mask.png*](<../build/host-census/victims-baseline/mask.png>) — local ignored artifact.
- [*build/host-census/victims-baseline/comparison.json*](<../build/host-census/victims-baseline/comparison.json>) — local ignored artifact.
- [*build/host-census/victims-noplugin/manifest.json*](<../build/host-census/victims-noplugin/manifest.json>) — local ignored artifact.
- [*build/host-census/plugin-contribution.json*](<../build/host-census/plugin-contribution.json>) — local ignored artifact.
- [*build/host-census/restart-Disable.json*](<../build/host-census/restart-Disable.json>) — local ignored artifact.

Existing generator/probe: [*tools/jsx/probe-victim-census.jsx*](<../tools/jsx/probe-victim-census.jsx>). This source snapshot is not automatically the historical tested revision.

Manifest: [*build/host-census/victims-baseline/manifest.json*](<../build/host-census/victims-baseline/manifest.json>). 20 subcases; manifest host field absent; use linked host result.

`FX-VICTIM20/VC00`: unstroked first line, stroked second; `FX-VICTIM20/VC01`: stroked first line, unstroked second; `FX-VICTIM20/VC02`: both stroked; `FX-VICTIM20/VC03`: both unstroked; `FX-VICTIM20/VC04`: alternating three lines; `FX-VICTIM20/VC05`: different stroke weights; `FX-VICTIM20/VC06`: different stroke and fill colors; `FX-VICTIM20/VC07`: None with positive stored weight; `FX-VICTIM20/VC08`: zero weight with Paper stroke color; `FX-VICTIM20/VC09`: 0 percent text opacity; `FX-VICTIM20/VC10`: character mixed stroke states; `FX-VICTIM20/VC11`: thin benign round strokes; `FX-VICTIM20/VC12`: thin benign miter strokes; `FX-VICTIM20/VC13`: thick nonoverlapping lines; `FX-VICTIM20/VC14`: underline decoration; `FX-VICTIM20/VC15`: strikethrough decoration; `FX-VICTIM20/VC16`: stroke tint zero; `FX-VICTIM20/VC17`: None plus zero weight; `FX-VICTIM20/VC18`: rotated frame; `FX-VICTIM20/VC19`: baseline shift boundary.

### FX-AI-LOAD artifacts

- [*tests/audit/results/illustrator-first-native-load.json*](<../tests/audit/results/illustrator-first-native-load.json>) — portable candidate evidence; read counters alongside retained stage/crash interpretation.

- [*ILLUSTRATOR_NATIVE_LOAD_REPORT.md*](<../ILLUSTRATOR_NATIVE_LOAD_REPORT.md>)
- [*build/host-census/native-install.json*](<../build/host-census/native-install.json>) — local ignored artifact.
- [*build/host-census/ai-native-1/host.json*](<../build/host-census/ai-native-1/host.json>) — local ignored artifact.
- [*build/host-census/ai-native-1/first-load-stages.jsonl*](<../build/host-census/ai-native-1/first-load-stages.jsonl>) — local ignored artifact.
- [*build/host-census/ai-native-1/first-load-control.pdf*](<../build/host-census/ai-native-1/first-load-control.pdf>) — local ignored artifact.

Existing generator/probe: [*tools/jsx/ai-probe-native-stroke.jsx*](<../tools/jsx/ai-probe-native-stroke.jsx>). This source snapshot is not automatically the historical tested revision.

### FX-PERSISTENCE20 artifacts

- [*tests/audit/results/host-census-persistence.json*](<../tests/audit/results/host-census-persistence.json>) — portable candidate evidence.
- [*build/host-census/persistence-noplugin/host.json*](<../build/host-census/persistence-noplugin/host.json>) — local supporting artifact.
- [*build/host-census/persistence-noplugin/render-comparison.json*](<../build/host-census/persistence-noplugin/render-comparison.json>) — local supporting artifact.
- [*build/host-census/persistence-noplugin/direct.png*](<../build/host-census/persistence-noplugin/direct.png>) — local supporting artifact.
- [*build/host-census/persistence-noplugin/reopened.png*](<../build/host-census/persistence-noplugin/reopened.png>) — local supporting artifact.
- [*build/host-census/persistence-noplugin/direct.pdf*](<../build/host-census/persistence-noplugin/direct.pdf>) — local supporting artifact.
- [*build/host-census/persistence-noplugin/reopened.pdf*](<../build/host-census/persistence-noplugin/reopened.pdf>) — local supporting artifact.
- [*build/host-census/persistence-noplugin/roundtrip.idml*](<../build/host-census/persistence-noplugin/roundtrip.idml>) — local supporting artifact.
- [*build/host-census/persistence-noplugin/reopened.indd*](<../build/host-census/persistence-noplugin/reopened.indd>) — local supporting artifact.

Existing probe: [*tools/jsx/probe-census-persistence.jsx*](<../tools/jsx/probe-census-persistence.jsx>). **Native persistence: PROVEN, finite scope.** Without the plugin, all 20 frames preserve eight sampled character fields through INDD → IDML → INDD save/reopen. PNG at 300 ppi has 0 changed pixels; PDF/X-4:2008 raster comparison has 0 differing channels, and extracted text is equal. The portable record identifies MuPDF via PyMuPDF 1.28.2 at 288 ppi with alpha disabled, PNG at 300 ppi, NumPy 2.5.3, and Pillow 12.3.0. The PDFs have different file hashes, so this is render/text equality, not byte identity. This does not test script-pair creation, metadata, synchronization, or script-pair round-trip. Fourteen PDF preference fields and one EPS field remain unverified by the restoration logger.

### FX-PAIR-RENDER artifacts

- [*tests/audit/results/pair-persistence-render.json*](<../tests/audit/results/pair-persistence-render.json>)
- [*build/host-census/pairs-render/before.png*](<../build/host-census/pairs-render/before.png>)
- [*build/host-census/pairs-render/after.png*](<../build/host-census/pairs-render/after.png>)
- [*build/host-census/pairs-render/host.json*](<../build/host-census/pairs-render/host.json>)
- [*build/host-census/pairs-render/pairs.indd*](<../build/host-census/pairs-render/pairs.indd>)
- [*build/host-census/pairs-render/pairs.idml*](<../build/host-census/pairs-render/pairs.idml>)
- [*build/host-census/pairs-render/reopened.indd*](<../build/host-census/pairs-render/reopened.indd>)

Existing probe: [*tools/jsx/probe-pair-persistence-render.jsx*](<../tools/jsx/probe-pair-persistence-render.jsx>).

### FX-ID-REFUSAL artifacts

- [*tests/audit/results/indesign-native-refusal.json*](<../tests/audit/results/indesign-native-refusal.json>)
- [*build/host-census/victims-refusal/host.json*](<../build/host-census/victims-refusal/host.json>)
- [*build/host-census/victims-refusal/fixture.indd*](<../build/host-census/victims-refusal/fixture.indd>)
- [*build/host-census/victims-refusal/native.png*](<../build/host-census/victims-refusal/native.png>)
- [*build/host-census/victims-refusal/reference.png*](<../build/host-census/victims-refusal/reference.png>)
- [*build/host-census/victims-refusal/mask.png*](<../build/host-census/victims-refusal/mask.png>)

Existing probe: [*tools/jsx/probe-victim-census.jsx*](<../tools/jsx/probe-victim-census.jsx>).

**Split-pair persistence: PROVEN for two pairs.** On InDesign 21.5.1.73 with safe-refusal binary `15b802ebfb38f567eb3aa586e995c0a1afff47efd078f3e2a0443596b54df4c4`, uniform and varied Outside-stroke pairs preserve `ISE_SPLIT` labels, `ISE3` identities, fill/stroke roles, sampled character positions, sampled paint attributes including fillTint, and None/zero top strokes through INDD save/reopen → IDML → INDD save/reopen. Pre-save and final opaque RGB PNGs at 600 ppi have identical hashes and 0 changed pixels. The final checked rerun verifies that the lower fill matches the source stroke color and tint. The harness had incorrectly required a None lower fill; correcting that assertion changes no product behavior. The final image hash remains `1d79f5a038dc21163b9510a9f32064fb8111d54ef81969967dd9a7a6467e69df`, matching the earlier finite render proof. This separately closes the two-pair rendered persistence gap; arbitrary edits, idle resync, PDF/SVG/print, other builds, and full lifecycle remain unverified. [FX-PAIR-RENDER]

**Safe refusal: PROVEN for the 20-frame PNG census.** The same binary records `correction_active: false`. All three whole-page PNGs—native output, independent reference, and mask—match the absent-plugin hashes at 600 ppi, with 0 changed pixels. This establishes finite no-harm output while correction is disabled. It does not repair the target failure or erase old binary `d476f565…` failures. Screen, PDF/print, active correction, and full document-state/lifecycle safety remain unverified. [FX-ID-REFUSAL]

**Hardened census and persistence: PROVEN for the retained 20-frame rerun.** The [checked record](<../tests/audit/results/host-census-checked.json>) identifies InDesign 21.5.1.73 and safe-refusal binary `15b802ebfb38f567eb3aa586e995c0a1afff47efd078f3e2a0443596b54df4c4`. Collection and persistence both pass. Native, reference, and mask PNGs at 600 ppi are byte-identical to the no-plugin control, with 0 changed pixels and a black mask corner `(0, 0, 0)`. The mask tint readback remains −1 and unresolved; exact image equality validates this retained mask only. The persistence rerun preserves the eight listed character fields and frame names, with 0 changed PNG pixels at 300 ppi, 0 differing PDF raster channels at 288 ppi, and equal extracted text. Fourteen PDF preference fields and one EPS field remain unverified. Correction is disabled; this result does not establish active correction, screen equivalence, or complete document-state preservation.

### FX-AI-REJECT artifacts

- [*tests/audit/results/illustrator-native-rejection.json*](<../tests/audit/results/illustrator-native-rejection.json>)
- [*build/host-census/ai-native-3/host.json*](<../build/host-census/ai-native-3/host.json>)

### FX-AI-PREP4 artifacts

- [*tests/audit/results/illustrator-native-lifecycle.json*](<../tests/audit/results/illustrator-native-lifecycle.json>) — portable evidence for both runs and later shutdown observations.

- [*plugin/illustrator-probe/PREPARATION_AUDIT.md*](<../plugin/illustrator-probe/PREPARATION_AUDIT.md>)
- [*plugin/illustrator-probe/README.md*](<../plugin/illustrator-probe/README.md>)
- [*build/host-census/ai-prep4-frozen-install.json*](<../build/host-census/ai-prep4-frozen-install.json>)
- [*plugin/illustrator-probe/build/native-prep4-frozen.aip*](<../plugin/illustrator-probe/build/native-prep4-frozen.aip>)

**Historical prep-4 evidence: PROVEN for the first two fresh specimen runs.** The [portable lifecycle record](<../tests/audit/results/illustrator-native-lifecycle.json>) identifies Illustrator 30.7.0 build 114R and frozen binary `45fab742d70bfb12497b3735a1e3c9d4b14dd8dc9195551f0221f1c4297b1c78`. Native PIDs 64996 and 78916 each match five runner snapshots. Each run ends with three Go calls, three successful replacements, three accepted inputs, zero latest error, and stroke origin 3 (parameters). Both runs preserve the open two-anchor source through same-process AI save/reopen and reevaluate an endpoint edit afterward. Reopen redraw alone adds no callback. The first run records 230 passing assertions and the second records 231; repeated protocol checks are not independent cases. Independent MuPDF extraction of the first run finds one 8 pt control stroke and one effect fill rectangle, with bounds (100, 196)–(260, 204) in PDF coordinates. The second run writes new AI/PDF fixtures; it is not cross-process reopening of the first run’s files. Full rendering, arbitrary style synchronization, transformed strokes, closed paths, glyphs, topology correction, and missing-plugin fallback remain unverified. [FX-AI-PREP4]

**Historical shutdown failures remain unexplained.** The first quit helper did not record a target PID, and the later JavaScript request left PID 48928 alive after 30 seconds and controller exit despite zero documents. A later hardened prep-4 lifecycle on PID 38016 passes 332 assertions, including 27 interval checks, followed by a PID/start-matched direct-COM exit in a 3.882651 s recorded attempt. Prep-5 subsequently records the separate exit below. These observations establish those COM-route exits without explaining earlier non-exit or access violations. Broad stability remains a P0 concern; no recovered-document permission is pending. [FX-AI-PREP4], [FX-AI-PREP5]

**Current prep-5 native evidence: PROVEN for one bounded straight-line run and its direct-COM exit.** The [portable lifecycle record](<../tests/audit/results/illustrator-native-lifecycle.json>) identifies Illustrator 30.7.0 build 114R, binary `201dd19cb25174c7fb9628fd1bf2769aa41e7d4dd4f2c6b77e5e9d7aea0fc9c7`, and native PID 5324 matched to five external snapshots. All 332 harness assertions pass, including 27 failed-callback interval checks; these include repeated protocol checks, not 332 independent cases. Go calls, mutations, and accepted inputs each reach 3, with latest error 0 and parameter stroke origin 3. Source edits, same-process AI save/reopen, and PDF export pass. A subsequent direct COM `Application.Quit` observes the same PID/start exit after zero documents, in a 4.021051 s recorded attempt. Nested shutdown was not forced in the host and has mock evidence only. This is not native region processing, general stroke support, or broad stability certification. [FX-AI-PREP5]

## Proposed fixtures: explicitly unexecuted

Each planned ID has no saved fixture artifact, no verified host build, and confidence **UNVERIFIED**. Expected native behavior must be recorded from the chosen host version or a cited documented control; expected StrokeMaster behavior is preservation plus the explicitly defined correction or safe refusal. Use vector/document-state oracles and independent render comparisons appropriate to the case. No family becomes supported merely because a plan exists.

| ID | Application | Issue IDs | Minimal discriminating experiment | Host build / execution |
| --- | --- | --- | --- | --- |
| <a id="plan-id-layout"></a>PLAN-ID-LAYOUT | InDesign | ID-09 | Independent minimal cases: drop cap with next-line overlap; two columns; overset transition; inline object; baseline-grid reflow. | Not yet recorded; UNEXECUTED |
| <a id="plan-id-shaping"></a>PLAN-ID-SHAPING | InDesign | ID-10 | Vary ligatures/OpenType, optical versus metric kerning, variable axes, missing/substituted fonts, RTL and vertical text separately. | Not yet recorded; UNEXECUTED |
| <a id="plan-id-transform"></a>PLAN-ID-TRANSFORM | InDesign | ID-11 | Rotate/shear/reflect/nonuniformly scale a frame; compare text on path against untransformed source. | Not yet recorded; UNEXECUTED |
| <a id="plan-id-output"></a>PLAN-ID-OUTPUT | InDesign | ID-13, UP-ID-01 | Compare layout/normal/Preview, GPU/CPU, overprint, PDF presets, PNG, actual print/distilled output on identical saved inputs. Repeat A/C/V/N and benign controls across 21.5.1.73 and legitimate 21.6 install when available. | Not yet recorded; UNEXECUTED |
| <a id="plan-ai-basic"></a>PLAN-AI-BASIC | Illustrator | AI-02 | Open/closed wedges; each cap/join/alignment and miter threshold; keep native and custom expectations separate. | Not yet recorded; UNEXECUTED |
| <a id="plan-ai-align"></a>PLAN-AI-ALIGN | Illustrator | AI-03 | Compare open path, closed path, Live Paint edge, and outlined stroke. | Not yet recorded; UNEXECUTED |
| <a id="plan-ai-appearance"></a>PLAN-AI-APPEARANCE | Illustrator | AI-04 | Object/group/layer/nested group; duplicate strokes; stroke above/below fill; inspect effective contribution before/after effects. | Not yet recorded; UNEXECUTED |
| <a id="plan-ai-effects"></a>PLAN-AI-EFFECTS | Illustrator | AI-05 | Place correction before/after Transform, Offset Path, Pathfinder, opacity/blend; retain other strokes/fills. | Not yet recorded; UNEXECUTED |
| <a id="plan-ai-width"></a>PLAN-AI-WIDTH | Illustrator | AI-06 | Taper, asymmetric width and abrupt width point; compare custom envelope and native expansion. | Not yet recorded; UNEXECUTED |
| <a id="plan-ai-dash"></a>PLAN-AI-DASH | Illustrator | AI-07 | Dash across acute corner; very short segment; round/butt cap; varying phase. | Not yet recorded; UNEXECUTED |
| <a id="plan-ai-brush-calligraphic"></a>PLAN-AI-BRUSH-CALLIGRAPHIC | Illustrator | AI-08 | One calligraphic brush at an acute turn; expand a disposable copy and inspect output regions, opacity, and corner behavior. | Not yet recorded; UNEXECUTED |
| <a id="plan-ai-brush-scatter"></a>PLAN-AI-BRUSH-SCATTER | Illustrator | AI-09 | One scatter brush at an acute turn; expand a disposable copy and inspect output regions, opacity, and corner behavior. | Not yet recorded; UNEXECUTED |
| <a id="plan-ai-brush-art"></a>PLAN-AI-BRUSH-ART | Illustrator | AI-10 | One art brush at an acute turn; expand a disposable copy and inspect output regions, opacity, and corner behavior. | Not yet recorded; UNEXECUTED |
| <a id="plan-ai-brush-pattern"></a>PLAN-AI-BRUSH-PATTERN | Illustrator | AI-11 | One pattern brush at an acute turn; expand a disposable copy and inspect output regions, opacity, and corner behavior. | Not yet recorded; UNEXECUTED |
| <a id="plan-ai-brush-bristle"></a>PLAN-AI-BRUSH-BRISTLE | Illustrator | AI-12 | One bristle brush at an acute turn; expand a disposable copy and inspect output regions, opacity, and corner behavior. | Not yet recorded; UNEXECUTED |
| <a id="plan-ai-text"></a>PLAN-AI-TEXT | Illustrator | AI-13 | Compare character stroke versus Appearance stroke, multiple strokes, area/path text, and edits without expanding originals. | Not yet recorded; UNEXECUTED |
| <a id="plan-ai-topology"></a>PLAN-AI-TOPOLOGY | Illustrator | AI-14 | Two squares: Unite area 15,000 pt²; square-with-hole area 7,500 pt². Add reversed winding and self-intersection fixtures. | Not yet recorded; UNEXECUTED |
| <a id="plan-ai-groups"></a>PLAN-AI-GROUPS | Illustrator | AI-15 | Clipping group, symbol instance, nested group and layer appearance; preserve hierarchy and unrelated members. | Not yet recorded; UNEXECUTED |
| <a id="plan-ai-transform"></a>PLAN-AI-TRANSFORM | Illustrator | AI-16 | Apply each transform independently; toggle Scale Strokes & Effects; transform parent and child separately. | Not yet recorded; UNEXECUTED |
| <a id="plan-ai-persist"></a>PLAN-AI-PERSIST | Illustrator | AI-18 | Owned AI/PDF fixture: reopen with/without plugin; check warnings, fallback paths/raster/private data, edits and exports. | Not yet recorded; UNEXECUTED |
| <a id="plan-core-adversarial"></a>PLAN-CORE-ADVERSARIAL | Both | CORE-02 | Acute/nearly parallel, concave/convex, cusp, zero length, repeated anchors, loops/self-intersections, holes/nested holes, touching/overlapping glyphs, tiny counters/thin stems and huge stroke; save deterministic seeds. | Not yet recorded; UNEXECUTED |
| <a id="plan-safety"></a>PLAN-SAFETY | Both | SAFE-01 | Inject failure/cancel in owned test documents; exercise undo/redo, save/close, and missing-plugin recovery without touching user fixtures. | Not yet recorded; UNEXECUTED |
| <a id="plan-performance"></a>PLAN-PERFORMANCE | Both | PERF-01 | Ordinary/pathological text, long paragraph/large document/path-heavy AI; initial/edit/reflow/redraw/export. Invalidate on text/font/glyph/size/stroke/leading/tracking/transform/frame/Appearance/parent/order changes. | Not yet recorded; UNEXECUTED |
| <a id="plan-id-abi"></a>PLAN-ID-ABI | InDesign | UP-ID-02 | Only if table APIs are used: legitimate version-pair diagnostic; record SDK and binary identity. | Not yet recorded; UNEXECUTED |
| <a id="plan-ai-upstream"></a>PLAN-AI-UPSTREAM | Illustrator | UP-AI-01, UP-AI-02 | Closed inside/outside stroke expansion: inspect closure/duplicate anchors; separately move live effect with opacity mask. Attribute-free group versus genuinely styled group; inspect panel and model on version pair. | Not yet recorded; UNEXECUTED |
| <a id="plan-ai-empty"></a>PLAN-AI-EMPTY | Illustrator | AI-19 | Compare no entry, None paint, zero width, 0% stroke opacity, transparent color where representable, hidden entry and disabled effect/entry; inspect PDF/SVG and saved AI. | Not yet recorded; UNEXECUTED |
| <a id="plan-noharm"></a>PLAN-NOHARM | Both | SAFE-02 | No strokes anywhere, benign thin strokes, round joins, ordinary miters below threshold, unrelated paths/text frames, differently colored neighbors, and a complex nontriggering document; require vector/pixel and document-state identity except permitted metadata. | Not yet recorded; UNEXECUTED |


The new FX-VICTIM20 covers reversed offender/victim direction, alternating lines, mixed weights/colors, None versus zero, text opacity zero, mixed characters, benign joins, decorations, tint zero, one rotation, and one baseline shift. These are now executed finite cases; remaining opacity/hidden-entry, broader transform, layout, and shaping plans stay unexecuted. The two comparison directions are stored explicitly in the JSON registry.

## Evidence gaps and acceptance

- FX-AI-OUTLINE is backed by the takeover narrative and source, but a compact retained 17-check raw record is not present in the indexed raw records. Keep that distinction from the explicit ten-check region JSON.
- The 20.5.5.119 InDesign version comparison is historical narrative in [*STROKE_MECHANISMS.md*](<../docs/STROKE_MECHANISMS.md>); its raw paired exports were not located. It does not certify the current plugin on 2025.
- The extreme font/render artifacts are evidence, not a redistributable font package or an oracle for every glyph. Embedded-font and outline contour counts differ; neither missing-contour cause nor rasterizer correctness is established.
- The victim's `native.png` name does not mean “without plugin.” Old V0 had only a post-test module observation; new VC plugin-off/on controls now exist. A candidate fix still needs its own matched controls and separate screen/PDF evidence.
- Further save/reopen, script-pair IDML, undo, missing-plugin, and host-close tests must distinguish model assertions from rendered/output equality. FX-AI-LOAD retains the first loading failure; FX-AI-PREP4 separately proves two bounded open-line runs with same-process AI reopen and PDF export. Cross-process reopening, missing-plugin fallback, full rendering, and clean shutdown remain unproven.

For a new result, append the fixture/subcase ID, host version/build, source/binary hash, input/output hashes, export settings, render route, expected/actual measurements, preservation checks, and unresolved fields. Do not rewrite an earlier failing result into a passing one.


[FX-PAIR-RENDER]: <../fixtures/FIXTURE_INDEX.md#fx-pair-render>

[FX-ID-REFUSAL]: <../fixtures/FIXTURE_INDEX.md#fx-id-refusal>

[FX-AI-REJECT]: <../fixtures/FIXTURE_INDEX.md#fx-ai-reject>

[FX-AI-PREP4]: <../fixtures/FIXTURE_INDEX.md#fx-ai-prep4>


### FX-TOPOLOGY artifacts

**Standalone topology: bounded approval.** The [independent review](<../tests/geometry/TOPOLOGY_REVIEW.md>) closes the reproduced protected-retention and remote/multipart-envelope failures at snapshot `f92abc24…`. All 24 exact remote-envelope orders refuse before union; all 24 tight controls retain the hole. The 80 seeded rectangle unions pass with 6,415 independent membership checks and zero refusals. Geometry discovery passes 72 tests with six dependency skips; those six separately pass in the repository environment. The core caller passes 43 checks. This establishes finite standalone polygon/API behavior; native topology integration, raw winding, curves, and universal numerical bounds remain unverified.

- [*tools/topology_region.cpp*](<../tools/topology_region.cpp>)
- [*tools/build_topology.py*](<../tools/build_topology.py>)
- [*tools/topology_region_core.h*](<../tools/topology_region_core.h>)
- [*tests/geometry/test_topology_region.py*](<../tests/geometry/test_topology_region.py>)
- [*tests/geometry/measure_topology_region.py*](<../tests/geometry/measure_topology_region.py>)
- [*tests/geometry/test_topology_retention.py*](<../tests/geometry/test_topology_retention.py>)
- [*tests/geometry/test_topology_core.py*](<../tests/geometry/test_topology_core.py>)
- [*tests/geometry/topology_core_smoke.cpp*](<../tests/geometry/topology_core_smoke.cpp>)
- [*tests/geometry/build_core_api.py*](<../tests/geometry/build_core_api.py>)
- [*tests/geometry/TOPOLOGY_CORE_API.md*](<../tests/geometry/TOPOLOGY_CORE_API.md>)
- [*tests/geometry/topology_api_equivalence.json*](<../tests/geometry/topology_api_equivalence.json>)
- [*tests/geometry/topology_union_regression.json*](<../tests/geometry/topology_union_regression.json>)
- [*tests/geometry/topology_freeze_rejected_6e7d8faf.json*](<../tests/geometry/topology_freeze_rejected_6e7d8faf.json>)
- [*TOPOLOGY_GEOMETRY_DESIGN.md*](<../TOPOLOGY_GEOMETRY_DESIGN.md>)
- [*tests/geometry/freeze_topology.py*](<../tests/geometry/freeze_topology.py>)
- [*tests/geometry/topology_evidence.json*](<../tests/geometry/topology_evidence.json>)
- [*tests/geometry/topology_oracle.svg*](<../tests/geometry/topology_oracle.svg>)
- [*tests/geometry/topology_oracle.png*](<../tests/geometry/topology_oracle.png>)
- [*tests/geometry/topology_build/.gitignore*](<../tests/geometry/topology_build/.gitignore>)
- [*tests/geometry/topology_freeze.json*](<../tests/geometry/topology_freeze.json>)
- [*tests/geometry/TOPOLOGY_REVIEW.md*](<../tests/geometry/TOPOLOGY_REVIEW.md>)
- [*tests/geometry/test_topology_rect_fuzz.py*](<../tests/geometry/test_topology_rect_fuzz.py>)
- [*tests/geometry/topology_build/topology_region.exe*](<../tests/geometry/topology_build/topology_region.exe>)
- [*tests/geometry/topology_build/compile.log*](<../tests/geometry/topology_build/compile.log>)
- [*tests/geometry/topology_build/core_api/topology_core_smoke.exe*](<../tests/geometry/topology_build/core_api/topology_core_smoke.exe>)
- [*tests/geometry/topology_build/core_api/compile.log*](<../tests/geometry/topology_build/core_api/compile.log>)

The rejected freeze remains historical evidence. The independent review and fuzz test were added after the final core freeze and are inventoried separately. Generated executables and compiler logs are local ignored artifacts; no SDK source is redistributed by this index.

### FX-AI-PREP5 artifacts

- [*illustrator-native-lifecycle.json*](<../tests/audit/results/illustrator-native-lifecycle.json>)
- [Native load report](<../ILLUSTRATOR_NATIVE_LOAD_REPORT.md>)
- [Prep-5 lifetime audit](<../plugin/illustrator-probe/PREP5_LIFETIME_AUDIT.md>)
- [Frozen prep-5 binary, local only](<../plugin/illustrator-probe/build/native-prep5-frozen.aip>)

## Per-user load route

The external Adobe Illustrator 2026 SDK [programmer’s guide, PDF page 9](<../../../Adobe SDK/Adobe Illustrator 2026 SDK/docs/guides/programmers-guide.pdf#page=9>) documents a user-specific plugin folder and an optional Additional Plug-ins Folder preference. Its Windows example retains a legacy Windows 7 heading; documentation alone does not validate the current host route. Read-only local inspection found *%APPDATA%/Adobe/Adobe Illustrator 30 Settings/en_US/x64/Plug-ins/* present with zero entries. No plugin was installed there or loaded from it during this audit.

**UNEXECUTED:** load, initialization, registration, and callback behavior from that folder. Native deployment must respect the no-UAC constraint; no elevated installation or preference change is implied. The bounded next check is a separately authorized owned-host load from the per-user location with binary identity and actual source path recorded. Prefix allocation, effect identity, package signing, and Marketplace acceptance remain separate distribution questions. The SDK stays external and is not redistributed.

[FX-AI-PREP5]: <../fixtures/FIXTURE_INDEX.md#fx-ai-prep5>
