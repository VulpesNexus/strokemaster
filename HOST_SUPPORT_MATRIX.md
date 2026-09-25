# Host support matrix

Snapshot: September 13, 2026; baseline `c4596837a5e0ea10a68f7b94e2dd368032341334`. This is a finite evidence matrix, not a production compatibility declaration. [The census](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md>) defines classifications, priorities, primary-source notices, and planned experiments. [The fixture index](<fixtures/FIXTURE_INDEX.md>) resolves the evidence IDs.

**Current native capability (2026-09-25):** Illustrator region-prep-7 (`47b6b544…`) publishes the reviewed topology core's correction of the marked analytic fixture, through its bridge and through the host's `applyEffect`, and survives undo, redo, and AI save/reopen; region-prep-9 (`a78d4398…`) adds a stroke-behind correction that removes all fill-core damage from a live two-line text fixture, across live edits and a fresh-process reopen. Q20 is closed: Go always ran, and the plugin refused its own input. InDesign now runs route W (`f31dd2c9…`, installed by the owner with the refusal build `15b802eb…` backed up): on the 20-case census it removes all fill-core damage in PNG, PDF, and EPS export, and the PDF keeps live text; across the 12 saved fixture documents it reaches the strokes-first oracle within every frame, in PDF and print output alike; on screen the owner saw the correction, and a save after interrupted redraws kept every stroke; overlap between separate frames stays native. Details and every status: [research revival](<STROKEMASTER_RESEARCH_REVIVAL_2026-09-25.md>). [FX-ID-REFUSAL], [FX-AI-PREP5]

**Standalone topology: bounded approval.** The [independent review](<tests/geometry/TOPOLOGY_REVIEW.md>) closes the reproduced protected-retention and remote/multipart-envelope failures at snapshot `f92abc24…`. All 24 exact remote-envelope orders refuse before union; all 24 tight controls retain the hole. The 80 seeded rectangle unions pass with 6,415 independent membership checks and zero refusals. Geometry discovery passes 72 tests with six dependency skips; those six separately pass in the repository environment. The core caller passes 43 checks. This establishes finite standalone polygon/API behavior; native topology integration, raw winding, curves, and universal numerical bounds remain unverified. [FX-TOPOLOGY]

**Status vocabulary:** NATIVE OK, PLUGIN FIXED, SCRIPT FIXED, PARTIAL, HOST LIMITATION, PLUGIN LIMITATION, REGRESSION, UNKNOWN. FIXED applies only to the stated measured case and render/model path. REGRESSION requires a known earlier passing result under comparable conditions; an untested version is UNKNOWN. UNEXECUTED is an execution marker, not a failure. Confidence is independent of support status. No broad native-plugin family is release-certified.

## Environment and provenance

| Recorded environment | What is established | What is not established |
| --- | --- | --- |
| InDesign 21.5.1.73, Windows 11 Pro 10.0.26200 | Historical corpus identifies this environment; current audit JSON explicitly records 21.5.1.73. Installed SDK map identifies 21.5.1 build 73. | Retained VC runs are included; not every result records GPU/driver or tested source hash. |
| InDesign 20.5.5.119 | M7's historical narrative reports equivalent PDF paint structure after IDML import. | Matching raw version-pair export is not present in the indexed raw records; confidence STRONGLY SUPPORTED, not a current support certification. |
| Illustrator 30.7.0, build 114R | Region JSON records both version and build. Outline/benchmark attribution comes from the takeover narrative. | Separate FX-AI-LOAD now proves native load and a rejected callback on this build; other builds remain untested. |
| Later versions in Adobe release notes | Refer to UP-ID/UP-AI census rows. | No retained local execution for these versions. First affected versions and applicability to StrokeMaster remain unknown unless stated. |
| PDF rasterizers | Historical A–D analysis reports MuPDF at 720 dpi; extreme comparison records MuPDF/Poppler at 864 dpi. | Historical A-D/extreme records lack exact package versions. FX-PERSISTENCE20 separately records PyMuPDF 1.28.2 at 288 ppi; do not backfill older experiments from it. |

The historical corpus records NVIDIA RTX 4070 SUPER and Intel UHD 770 and captures GPU/CPU/Overprint views, but those old captures are not a fresh plugin-version screen test. Native/plugin filenames such as *native.png* or *actual/* are labels; provenance must establish which plugin was loaded.

## Measured islands of support

| Host / version | Exact case | Native result | Plugin result | Script result | Screen | Export / model route | Editable | Status / confidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| InDesign 21.5.1.73 | A01–A03 [FX-A] | Hidden fill in recorded HQ Print baseline | Historical hidden-fill count becomes zero in these cases | Separate fallback exists; not inferred from plugin result | No current plugin capture | PDF → MuPDF 720 dpi | Live text retained by adornment; full lifecycle unverified | PLUGIN FIXED for this finite metric; PROVEN |
| InDesign 21.5.1.73 | V0 [FX-VICTIM] | No isolated no-plugin control in the old V0 record; new VC controls added | 3,759 incorrect core pixels remain in recorded loaded-plugin path | Split supplies the position-preserving reference | UNEXECUTED | InDesign PNG 600 ppi, fill-only mask | Measured source positions preserved in old V0; separate native persistence now in FX-PERSISTENCE20 | PARTIAL / P1 release blocker; PROVEN |
| InDesign 21.5.1.73 | N00–N22 [FX-FILLMASK] | Historical native exports retained | All sampled eroded fill cores identical; edges outside that claim | Independent mask generation checked 46 word positions | UNEXECUTED | PNG 300 ppi; 3 px erosion | No full editability test | PARTIAL; PROVEN for metric |
| InDesign 21.5.1.73 | Two page-two pairs [FX-SYNC] | IDML retains tested attributes | Not a native-plugin test | Identity/None+zero/resync model checks pass | UNEXECUTED | Historical model-only record; separate two-pair rendered result now in FX-PAIR-RENDER | Live paired text; broader edit cases open | SCRIPT FIXED for measured model behavior; PROVEN |
| InDesign 21.5.1.73 | Staged outline transaction [FX-OUTLINE] | Actual returned art ownership inspected | Not a native-plugin test | 13 transaction/placement/refusal checks pass | UNEXECUTED | Host model, not general PDF fidelity | Successful outlined output is no longer live text | SCRIPT FIXED for bounded transaction; PROVEN |
| InDesign 21.5.1.73 | None plus explicit zero [FX-ZERO] | Stored values are distinct and writable | No separate plugin claim | Required empty/zero top state checked | UNEXECUTED | DOM rows; selected IDML checks in FX-SYNC | Attributes editable | NATIVE OK for these values; PROVEN |
| Illustrator 30.7.0 114R | Basic style/Unite/compound [FX-AI-REGIONS] | Ten finite model checks pass | No native plugin execution | Constructed regions/cleanup verified | UNEXECUTED | DOM signed-area/style assertions only | Editable sample paths; no round-trip | NATIVE OK for measured primitives; PROVEN |
| Illustrator 30.7.0 114R attribution | Outline method [FX-AI-OUTLINE] | Singular method worked in reported samples | No native plugin execution | 17 preservation/access checks reported | UNEXECUTED | No full Appearance/output guarantee | Original measured fields preserved; copies outlined | PARTIAL; STRONGLY SUPPORTED |
| Illustrator 30.7.0 114R | First native diagnostic [FX-AI-LOAD] | Control path created, no callback | Loads/registers; effect callback rejected, mutations 0 | Overall passed=false; collection/cleanup evidence retained | No fidelity proof | Control PDF written; effect export not reached | Two-anchor source retained before rejection; lifecycle not proved | PARTIAL; PROVEN loading, failed processing |
| Illustrator 30.7.0 114R | Marked closed-region fixture, region-prep-7 | Hole miter escapes the 0.2 pt wall to x = −0.4708 | One filled region, hole kept, minimum x −0.3000; bridge and `applyEffect` routes both publish | Not a script test | Not captured | PDF measured by *tools/measure_region_pdf.py*: both effect and control PASS | Source stays editable; undo, redo, AI save/reopen pass; an edited fixture is refused and passes through | PLUGIN FIXED for this fixture; PROVEN ([record](<tests/audit/results/illustrator-region-prep7.json>)) |
| Illustrator 30.7.0 114R | Two lines of live text, mixed 8 pt character strokes, unstroked victim, region-prep-9 stroke-behind | 55,568 of 72,080 fill-core pixels damaged | 0 damaged; ink and off-fill pixels identical to the outlined control | Not a script test | Not captured | PDF, MuPDF 600 dpi; text exported as outlines under any effect | Live text, live edits, undo, fresh-process reopen pass; without the plugin an edit renders stale | PLUGIN FIXED for this fixture; PROVEN ([record](<tests/audit/results/illustrator-region-prep9-stroke-behind.json>)) |
| Illustrator 30.7.0 114R | Two lines of live text, uniform 8 pt stroke, region-prep-10 native Appearance | All 72,080 fill-core pixels damaged | 0 damaged; coverage identical to native; written as plain Appearance, no effect | Not a script test | Not captured | PDF keeps text and font; strokes painted before fills | Live text; renders, exports, and follows edits without the plugin | PLUGIN FIXED (authoring only) for this fixture; PROVEN ([record](<tests/audit/results/illustrator-region-prep10-appearance.json>)) |
| InDesign 21.5.1.73 | VC11/12/13 [FX-VICTIM20] | Benign controls without plugin | Each acquires 2,964 changed pixels outside core | Collection passed, output gate failed | UNEXECUTED | Opaque RGB PNG 600 ppi | Full no-harm lifecycle not proved | PARTIAL / P1 no-harm failure; PROVEN |
| InDesign 21.5.1.73 | VC15 [FX-VICTIM20] | Matched plugin-off control retained | 5,430 core contribution changes; 3,300 wrong core vs intended reference | Independent reference retains glyph positions | UNEXECUTED | PNG 600 ppi | Not an editability proof | PARTIAL / P1 decoration failure; PROVEN |
| InDesign 21.5.1.73 | 20-frame persistence [FX-PERSISTENCE20] | Native INDD → IDML → INDD save/reopen | Plugin absent | Sampled state and positions equal; no script pairs | UNEXECUTED | PNG 300 ppi: 0 changed pixels; MuPDF 288 ppi: 0 channel differences; PDF text equal | Eight sampled character fields; broader editability untested | NATIVE OK; PROVEN finite case |

**Native persistence: PROVEN, finite scope.** Without the plugin, all 20 frames preserve eight sampled character fields through INDD → IDML → INDD save/reopen. PNG at 300 ppi has 0 changed pixels; PDF/X-4:2008 raster comparison has 0 differing channels, and extracted text is equal. The portable record identifies MuPDF via PyMuPDF 1.28.2 at 288 ppi with alpha disabled, PNG at 300 ppi, NumPy 2.5.3, and Pillow 12.3.0. The PDFs have different file hashes, so this is render/text equality, not byte identity. This does not test script-pair creation, metadata, synchronization, or script-pair round-trip. Fourteen PDF preference fields and one EPS field remain unverified by the restoration logger. [FX-PERSISTENCE20]

**Split-pair persistence: PROVEN for two pairs.** On InDesign 21.5.1.73 with safe-refusal binary `15b802ebfb38f567eb3aa586e995c0a1afff47efd078f3e2a0443596b54df4c4`, uniform and varied Outside-stroke pairs preserve `ISE_SPLIT` labels, `ISE3` identities, fill/stroke roles, sampled character positions, sampled paint attributes including fillTint, and None/zero top strokes through INDD save/reopen → IDML → INDD save/reopen. Pre-save and final opaque RGB PNGs at 600 ppi have identical hashes and 0 changed pixels. The final checked rerun verifies that the lower fill matches the source stroke color and tint. The harness had incorrectly required a None lower fill; correcting that assertion changes no product behavior. The final image hash remains `1d79f5a038dc21163b9510a9f32064fb8111d54ef81969967dd9a7a6467e69df`, matching the earlier finite render proof. This separately closes the two-pair rendered persistence gap; arbitrary edits, idle resync, PDF/SVG/print, other builds, and full lifecycle remain unverified. [FX-PAIR-RENDER]

**Safe refusal: PROVEN for the 20-frame PNG census.** The same binary records `correction_active: false`. All three whole-page PNGs—native output, independent reference, and mask—match the absent-plugin hashes at 600 ppi, with 0 changed pixels. This establishes finite no-harm output while correction is disabled. It does not repair the target failure or erase old binary `d476f565…` failures. Screen, PDF/print, active correction, and full document-state/lifecycle safety remain unverified. [FX-ID-REFUSAL]

**Hardened census and persistence: PROVEN for the retained 20-frame rerun.** The [checked record](<tests/audit/results/host-census-checked.json>) identifies InDesign 21.5.1.73 and safe-refusal binary `15b802ebfb38f567eb3aa586e995c0a1afff47efd078f3e2a0443596b54df4c4`. Collection and persistence both pass. Native, reference, and mask PNGs at 600 ppi are byte-identical to the no-plugin control, with 0 changed pixels and a black mask corner `(0, 0, 0)`. The mask tint readback remains −1 and unresolved; exact image equality validates this retained mask only. The persistence rerun preserves the eight listed character fields and frame names, with 0 changed PNG pixels at 300 ppi, 0 differing PDF raster channels at 288 ppi, and equal extracted text. Fourteen PDF preference fields and one EPS field remain unverified. Correction is disabled; this result does not establish active correction, screen equivalence, or complete document-state preservation.

## Complete family matrix

A version marked recorded/target is an observed environment for some evidence, not proof that an unexecuted family ran there. Existing fixture IDs and PLAN IDs make that distinction explicit.

| Host | Version | Case | Native result | Plugin result | Script result | Screen | Export | Editable | Status / confidence | Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| InDesign | 21.5.1.73 recorded/target | [ID-01](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#id-01>): Inter-line Outside stroke paint order | Outside overlap recorded | Historical finite improvement only; current safety binary disables repaint | Fixture generation and export available | Historical corpus GPU/CPU/Overprint/normal evidence; no fresh plugin capture | HQ Print PDF → MuPDF 720 dpi: A01–A03 1,536 → 0 hidden pixels in historical plugin pairs | No general guarantee; see measured fixture checks | PARTIAL; PROVEN | [FX-A], [FX-CORPUS] |
| InDesign | 21.5.1.73 recorded/target | [ID-02](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#id-02>): Unstroked victim among stroked lines | New plugin-off/on controls retained | Release blocker remains for tested legacy binary; no candidate fix proved | Controlled owned-document PNG probe; source word positions checked | UNEXECUTED | New InDesign 21.5.1.73 PNG 600 ppi: old-plugin/reference plus plugin-off/on comparisons; no fresh screen/PDF evidence | No general guarantee; see measured fixture checks | PARTIAL; PROVEN | [FX-VICTIM], [FX-VICTIM20] |
| InDesign | 21.5.1.73 recorded/target | [ID-03](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#id-03>): Cross-frame and threaded-story overlap | Frame z-order preserved | Existing frame repaint cannot certify cross-frame halo preservation | Split/reflow behavior outside supported simple frames unverified | Historical corpus only; case-specific screen unexecuted | HQ Print PDF → 720 dpi; historical cross-frame residuals remain | No general guarantee; see measured fixture checks | PLUGIN LIMITATION; PROVEN | [FX-A] |
| InDesign | 21.5.1.73 recorded/target | [ID-04](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#id-04>): Formatting-run boundaries | Finite paint interleaving recorded | Historical repaint improved finite cases; current safety binary disables all A/C corrections | Applied attribute sweep; full per-attribute map in fixture index | Historical corpus views; individual sweep screen unexecuted | 50-frame HQ Print PDF sweep, 720 dpi operator/raster analysis | No general guarantee; see measured fixture checks | PARTIAL; PROVEN | [FX-C] |
| InDesign | 21.5.1.73 recorded/target | [ID-05](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#id-05>): Miter/round/bevel, limit, weight extremes | Native convention matches controls | Native custom-envelope replacement unexecuted | Text miterLimit reaches PDF | Historical corpus views; sweep screen unexecuted | HQ Print PDF M operator and 720 dpi renders; geometry in B manifest | No general guarantee; see measured fixture checks | PLUGIN LIMITATION; PROVEN | [FX-B] |
| InDesign | 21.5.1.73 recorded/target | [ID-06](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#id-06>): Create Outlines fidelity and compound counters | Conversion mismatch recorded | Finite script improvement; broader rendering and live editability not certified | Hardened staged converter has 13 host transaction checks | Historical corpus D views; fresh conversion screen unexecuted | D PDF/720 dpi and path attributes; finite mismatch reproduced | Successful outlined output is not live text | PARTIAL; PROVEN | [FX-D], [FX-D-CONTOURS], [FX-D-FAITHFUL], [FX-OUTLINE] |
| InDesign | 21.5.1.73 recorded/target | [ID-07](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#id-07>): Tint, opacity, blend, spot color, overprint | Full family not measured | Finite coverage only; do not call all opacity/overprint safe | Page-item overprint applicability measured separately from text | UNEXECUTED | N historical PNG 300 ppi: all 23 eroded fill cores have zero nonzero differences; full-page differences remain | No general guarantee; see measured fixture checks | PARTIAL; STRONGLY SUPPORTED | [FX-N], [FX-FILLMASK], [FX-OVERPRINT], [FX-VICTIM20] |
| InDesign | 21.5.1.73 recorded/target | [ID-08](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#id-08>): None, zero weight, tiny weight, invisible stroke | None and stored width distinct | Script top uses None+zero; hidden/transparent state support unknown | Recorded None/weight assignments and sync round-trip | UNEXECUTED | DOM/IDML observations plus new VC07/08/09/16/17 PNG cases; 0% text opacity is not proof for 0% stroke-only opacity | No general guarantee; see measured fixture checks | PARTIAL; PROVEN | [FX-ZERO], [FX-SYNC], [FX-VICTIM20] |
| InDesign | 21.5.1.73 recorded/target | [ID-09](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#id-09>): Layout: drop caps, columns, threading, overset, anchors, baseline grid | Full family not measured | No blanket support or safe-refusal proof | Fixture APIs available in principle; individual gates unexecuted | UNEXECUTED | UNEXECUTED | No general guarantee; see measured fixture checks | UNKNOWN; UNVERIFIED | [PLAN-ID-LAYOUT] |
| InDesign | 21.5.1.73 recorded/target | [ID-10](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#id-10>): Shaping and font changes | Full family not measured | No general script/font support claim | C's Latin ligature/axis controls are historical only | UNEXECUTED | UNEXECUTED | No general guarantee; see measured fixture checks | UNKNOWN; UNVERIFIED | [FX-C], [PLAN-ID-SHAPING] |
| InDesign | 21.5.1.73 recorded/target | [ID-11](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#id-11>): Transforms and text on path | Full family not measured | Unknown outside recorded ordinary frames | Detection/refusal and transformed placement require per-case tests | UNEXECUTED | VC18 old-plugin/reference PNG 600 ppi: zero core changes, 4,891 total changed pixels | No general guarantee; see measured fixture checks | UNKNOWN; STRONGLY SUPPORTED | [FX-VICTIM20], [PLAN-ID-TRANSFORM] |
| InDesign | 21.5.1.73 | [ID-12](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#id-12>): INDD/IDML, pair identity, resync authority | Finite paired-frame persistence | Finite pair persistence proven; native binary 15b802… has correction disabled; broad resync/edit behavior unknown | Retained model checks plus two-pair rendered persistence; ISE_SPLIT labels and ISE3 fill/stroke roles preserved | UNEXECUTED | New pair PNG 600 ppi: 0 changed pixels and identical hashes; PDF, SVG, screen, and post-edit rendering remain UNEXECUTED | Only stated fields and output route verified | PARTIAL; PROVEN | [FX-SYNC], [FX-T], [FX-PAIR-RENDER] |
| InDesign | 21.5.1.73 recorded/target | [ID-13](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#id-13>): Display, export, print, version boundaries | Full family not measured | Requalify 21.6; do not infer PDF behavior from a display change | Export route must be recorded explicitly | Old corpus views only; 21.6 unexecuted | HQ Print/PDF-X4/PNG/EPS historical; print/distill unexecuted | No general guarantee; see measured fixture checks | UNKNOWN; UNVERIFIED | [FX-CORPUS], [PLAN-ID-OUTPUT] |
| Illustrator | 30.7.0 114R | [AI-01](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#ai-01>): Native discovery, loading, callback, and cleanup | Prep-5 native PID 5324; 332 assertions and 27 interval checks | Three replacements, zero errors; one PID-attributed direct-COM exit | SourceParameterSnapshot; arbitrary style resync unverified | No independent screen/GPU/CPU comparison | Same-process AI reopen, source edits, and PDF export pass | Matched process exit in a 4.021051 s recorded attempt; broad stability unverified | PARTIAL; PROVEN bounded run | [FX-AI-LOAD], [FX-AI-REJECT], [FX-AI-PREP4], [FX-AI-PREP5] |
| Illustrator | 30.7.0 114R recorded/target | [AI-02](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#ai-02>): Basic path stroke, caps/joins, open/closed alignment | Finite DOM setters and successful prep-4 and prep-5 open-line runs | Straight-line replacement passes; broader cap/join/alignment family unexecuted | 30.7.0 114R width=16, limit=2, miter setter checks | UNEXECUTED | Basic DOM setters and open-line effect PDF; no miter correction proof | No general guarantee; see measured fixture checks | PLUGIN LIMITATION; PROVEN | [FX-AI-REGIONS], [FX-AI-PREP5], [PLAN-AI-BASIC] |
| Illustrator | 30.7.0 114R recorded/target | [AI-03](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#ai-03>): Inside/Outside alignment availability | Documented closed-path scope | Requirement/support decision remains scoped | Basic DOM cannot stand in for every Appearance stroke | UNEXECUTED | UNEXECUTED | No general guarantee; see measured fixture checks | HOST LIMITATION; STRONGLY SUPPORTED | [PLAN-AI-ALIGN] |
| Illustrator | 30.7.0 114R recorded/target | [AI-04](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#ai-04>): Multiple fills/strokes and Appearance order | Basic attribute/Appearance mismatch reported | Native full-stack support unexecuted | Basic DOM underreports this benchmark; no claim that all APIs are incapable | Benchmark manual confirmation pending | Existing extreme PDF state only; controlled stack PDF/SVG unexecuted | No general guarantee; see measured fixture checks | UNKNOWN; STRONGLY SUPPORTED | [FX-AI-EXTREME], [PLAN-AI-APPEARANCE] |
| Illustrator | 30.7.0 114R recorded/target | [AI-05](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#ai-05>): Transform, Offset Path, Pathfinder and nested effect order | Full family not measured | Unexecuted; no flatten-everything promise | Actual Unite test is narrower than nested effects | UNEXECUTED | UNEXECUTED | No general guarantee; see measured fixture checks | UNKNOWN; UNVERIFIED | [PLAN-AI-EFFECTS] |
| Illustrator | 30.7.0 114R recorded/target | [AI-06](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#ai-06>): Variable-width profiles | Full family not measured | Support versus explicit refusal unresolved | No executed profile read/write proof | UNEXECUTED | UNEXECUTED | No general guarantee; see measured fixture checks | UNKNOWN; UNVERIFIED | [PLAN-AI-WIDTH] |
| Illustrator | 30.7.0 114R recorded/target | [AI-07](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#ai-07>): Dashes and short dash joins | Full family not measured | Source refusal only | No dispatched dashed fixture | UNEXECUTED | UNEXECUTED | No general guarantee; see measured fixture checks | PLUGIN LIMITATION; STRONGLY SUPPORTED | [PLAN-AI-DASH] |
| Illustrator | 30.7.0 114R recorded/target | [AI-08](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#ai-08>): calligraphic brushes | Full family not measured | Support/refusal decision pending | No executed brush test | UNEXECUTED | UNEXECUTED | No general guarantee; see measured fixture checks | UNKNOWN; UNVERIFIED | [PLAN-AI-BRUSH-CALLIGRAPHIC] |
| Illustrator | 30.7.0 114R recorded/target | [AI-09](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#ai-09>): scatter brushes | Full family not measured | Support/refusal decision pending | No executed brush test | UNEXECUTED | UNEXECUTED | No general guarantee; see measured fixture checks | UNKNOWN; UNVERIFIED | [PLAN-AI-BRUSH-SCATTER] |
| Illustrator | 30.7.0 114R recorded/target | [AI-10](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#ai-10>): art brushes | Full family not measured | Support/refusal decision pending | No executed brush test | UNEXECUTED | UNEXECUTED | No general guarantee; see measured fixture checks | UNKNOWN; UNVERIFIED | [PLAN-AI-BRUSH-ART] |
| Illustrator | 30.7.0 114R recorded/target | [AI-11](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#ai-11>): pattern brushes | Full family not measured | Support/refusal decision pending | No executed brush test | UNEXECUTED | UNEXECUTED | No general guarantee; see measured fixture checks | UNKNOWN; UNVERIFIED | [PLAN-AI-BRUSH-PATTERN] |
| Illustrator | 30.7.0 114R recorded/target | [AI-12](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#ai-12>): bristle brushes | Full family not measured | Support/refusal decision pending | No executed brush test | UNEXECUTED | UNEXECUTED | No general guarantee; see measured fixture checks | UNKNOWN; UNVERIFIED | [PLAN-AI-BRUSH-BRISTLE] |
| Illustrator | 30.7.0 114R | [AI-13](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#ai-13>): Live text, area text, text on path and editing | A native post-effect declaring path and compound input receives glyph outlines of live point text, one compound per glyph with its own fill and stroke, while the source stays a live TextFrame | Stroke-behind fixes the measured two-line fixture across edits and reopen; area text and text on path untested | createOutline() finite host evidence; not the chosen architecture | Not captured | Under any effect, text exports as outlines (no PDF text or fonts) | Live text kept; stale drawing when edited without the plugin | PARTIAL; PROVEN for point text | [FX-AI-OUTLINE], [record](<tests/audit/results/illustrator-region-prep8-text.json>) |
| Illustrator | 30.7.0 114R recorded/target | [AI-14](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#ai-14>): Compound paths, holes, winding and self-intersections | Finite primitive checks pass | Finite primitive capability; general region solver unknown | Recorded host probe performs Add/Expand; hole example is construction | UNEXECUTED | DOM area/path checks only; no PDF/SVG proof | No general guarantee; see measured fixture checks | PARTIAL; PROVEN | [FX-AI-REGIONS], [PLAN-AI-TOPOLOGY] |
| Illustrator | 30.7.0 114R recorded/target | [AI-15](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#ai-15>): Clipping groups, symbols and transformed groups | Full family not measured | Unknown | No executed representative group/symbol case | UNEXECUTED | UNEXECUTED | No general guarantee; see measured fixture checks | UNKNOWN; UNVERIFIED | [PLAN-AI-GROUPS] |
| Illustrator | 30.7.0 114R recorded/target | [AI-16](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#ai-16>): Nonuniform scale, reflection, rotation and shear | Full family not measured | Unknown | No complete transform probe | UNEXECUTED | UNEXECUTED | No general guarantee; see measured fixture checks | UNKNOWN; UNVERIFIED | [PLAN-AI-TRANSFORM] |
| Illustrator | 30.7.0 114R recorded/target | [AI-17](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#ai-17>): GPU/CPU, PDF/SVG and independent renderer disagreement | Rasterizers disagree | Cause and intended result unknown | PDF parser/raster evidence only | Controlled GPU/CPU pair unexecuted | PDF renderer difference proven; renderer versions not recorded in summary; SVG unexecuted | No general guarantee; see measured fixture checks | UNKNOWN; PROVEN | [FX-AI-EXTREME] |
| Illustrator | 30.7.0 114R | [AI-18](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#ai-18>): AI save/reopen and missing-plugin fallback | Same-process reopen in prep-4 and prep-5 specimens | Bounded persistence passes; fallback and cross-process reopen unverified | Reopened endpoint edit dispatches replacement | UNEXECUTED | AI/PDF outputs retained; first prep-4 PDF geometry independently inspected | Exact source geometry in the stated specimen lifecycle; broader editing unverified | PARTIAL; PROVEN same-process specimen reopen | [FX-AI-PREP4], [FX-AI-PREP5], [PLAN-AI-PERSIST] |
| Both | ID 21.5.1.73 / AI 30.7.0 114R targets; pure cases have no host | [CORE-01](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#core-01>): Custom global outer envelope versus local clipped join | Full family not measured | Local-only sufficiency disproven; host topology integration remains unverified | Prototype does not prove native takeover | UNEXECUTED | Pure vector/analytic data; not an Adobe render proof | No general guarantee; see measured fixture checks | PLUGIN LIMITATION; PROVEN | [FX-GLOBAL] |
| Both | ID 21.5.1.73 / AI 30.7.0 114R targets; pure cases have no host | [CORE-02](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#core-02>): Adversarial geometry and tolerance | Full family not measured | Bounded standalone kernel approved at f92abc24...; native topology, raw winding, curves, and universal numerical bounds remain unverified. | Independently reviewed standalone polygon/API experiment passes finite analytic, refusal, and rectangle-union controls; no host support inferred. | UNEXECUTED | UNEXECUTED | No general guarantee; see measured fixture checks | UNKNOWN; UNVERIFIED | [FX-TOPOLOGY], [PLAN-CORE-ADVERSARIAL] |
| Both | ID 21.5.1.73 / AI 30.7.0 114R targets; pure cases have no host | [SAFE-01](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#safe-01>): Lifecycle: ownership, undo/redo, cancellation and recovery | Full family not measured | No comprehensive no-harm certification | Finite InDesign preservation checks exist | UNEXECUTED | UNEXECUTED | No general guarantee; see measured fixture checks | UNKNOWN; UNVERIFIED | [FX-PRESERVE], [PLAN-SAFETY] |
| Both | ID 21.5.1.73 / AI 30.7.0 114R targets; pure cases have no host | [PERF-01](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#perf-01>): Incremental invalidation and performance | Full family not measured | Correctness first; no measured performance claim | Direct sync tick is not event-timing evidence | UNEXECUTED | UNEXECUTED | No general guarantee; see measured fixture checks | UNKNOWN; UNVERIFIED | [PLAN-PERFORMANCE] |
| InDesign | 21.6 documented; UNEXECUTED | [UP-ID-01](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#up-id-01>): 21.6 rendering boundary | Adobe notice only; local applicability untested | Do not extrapolate historical support | Compatibility unexecuted | UNEXECUTED | UNEXECUTED | No general guarantee; see measured fixture checks | UNKNOWN; STRONGLY SUPPORTED | [PLAN-ID-OUTPUT] |
| InDesign | 21.5.0 → 21.5.1 documented; UNEXECUTED | [UP-ID-02](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#up-id-02>): Older-SDK table API compatibility | Adobe notice only; local applicability untested | Avoid inferring ABI compatibility from marketing version | Not applicable to DOM-only evidence | UNEXECUTED | UNEXECUTED | No general guarantee; see measured fixture checks | UNKNOWN; STRONGLY SUPPORTED | [PLAN-ID-ABI] |
| Illustrator | 30.7 documented; boundary retest UNEXECUTED | [UP-AI-01](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#up-ai-01>): Aligned-stroke expansion and effect/mask upstream fixes | Adobe notice only; local applicability untested | Retest before implementing an already-fixed workaround | Test candidates, not executed regressions | UNEXECUTED | UNEXECUTED | No general guarantee; see measured fixture checks | UNKNOWN; STRONGLY SUPPORTED | [PLAN-AI-UPSTREAM] |
| Illustrator | 30.8 documented; UNEXECUTED | [UP-AI-02](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#up-ai-02>): 30.8 group Appearance indicator fix | Adobe notice only; local applicability untested | Do not trust panel badge alone as correction trigger | Not tested | UNEXECUTED | UNEXECUTED | No general guarantee; see measured fixture checks | UNKNOWN; STRONGLY SUPPORTED | [PLAN-AI-UPSTREAM] |
| Illustrator | 30.7.0 114R recorded/target | [AI-19](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#ai-19>): Absent, None, zero-weight, zero-opacity, hidden and disabled strokes | Full family not measured | Support versus explicit refusal unresolved | No executed full-state census | UNEXECUTED | UNEXECUTED | No general guarantee; see measured fixture checks | UNKNOWN; UNVERIFIED | [PLAN-AI-EMPTY] |
| Both | ID 21.5.1.73 / AI 30.7.0 114R targets; pure cases have no host | [SAFE-02](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#safe-02>): No-harm output and document-state controls | Benign no-plugin controls retained | P1 no-harm failure proven for legacy binary; do not dismiss non-core changes as harmless | Finite InDesign preservation checks exist | UNEXECUTED | InDesign RGB PNG 600 ppi; no-plugin native.png versus legacy-plugin native.png. VC03 no stroke and VC09 0% text opacity unchanged. | No general guarantee; see measured fixture checks | PARTIAL; PROVEN | [FX-VICTIM20], [PLAN-NOHARM] |
| InDesign | 21.5.1.73 recorded/target | [ID-14](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#id-14>): Decorations: strikethrough and underline | Matched plugin-off decoration control | P1 output-fidelity failure; causal contribution still under investigation | Controlled owned-document PNG probe; source word positions checked | UNEXECUTED | New InDesign 21.5.1.73 PNG 600 ppi: old-plugin/reference plus plugin-off/on comparisons; no fresh screen/PDF evidence | No general guarantee; see measured fixture checks | PARTIAL; PROVEN | [FX-VICTIM20] |
| InDesign | 21.5.1.73 | [ID-15](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#id-15>): Native INDD → IDML → INDD persistence without plugin | 20-frame native persistence measured | Plugin absent; no script-pair creation, metadata, synchronization, or missing-plugin fallback tested | Probe checks contents, horizontalOffset, baseline, strokeColor.name, strokeWeight, fillColor.name, strokeTint, and endJoin for each sampled character. | UNEXECUTED | RGB opaque PNG 300 ppi: 0 changed pixels; PDF/X-4:2008 → MuPDF 288 ppi: 0 channel differences; extracted text equal. PyMuPDF package 1.28.2; PNG 300 ppi and PDF raster 288 ppi recorded. | Only explicitly sampled fields checked | NATIVE OK; PROVEN | [FX-PERSISTENCE20] |
| Illustrator | 30.7.0 114R | [AI-20](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#ai-20>): Unexpected host exits around first native load | Earlier crashes and JavaScript non-exit; later matched COM exits | P0 broad stability gate remains despite bounded successes | Nested shutdown host-forcing UNEXECUTED; mocks only | No independent screen attribution | Straight-line PDF outputs separate from shutdown evidence | Prep-4 PID 38016 and prep-5 PID 5324 exits observed; earlier causes unknown | UNKNOWN; PROVEN bounded observations | [FX-AI-LOAD], [FX-AI-PREP4], [FX-AI-PREP5] |
| InDesign | 21.5.1.73 | [SAFE-03](<STROKE_DEFECT_AND_LIMITATION_CENSUS.md#safe-03>): Safe-refusal binary preserves 20-frame output | Whole-page absent-plugin control retained | Finite no-harm refusal passes; target victim/overlap correction remains unavailable | The same generator completes; native correction is inactive | UNEXECUTED | InDesign 21.5.1.73, RGB PNG 600 ppi: identical hashes and 0 changed pixels for all three whole-page images. | Only stated fields and output route verified | PLUGIN LIMITATION; PROVEN | [FX-ID-REFUSAL] |

## Historical prep-2 native load stage ledger

| Stage | InDesign legacy adornment / later safe refusal | Illustrator native-prep-2 |
| --- | --- | --- |
| BUILDS | Historical build and tested legacy hash retained | PROVEN; installed hash begins 4eb883, replacing the older compile-only artifact |
| HOST DISCOVERS / HOST LOADS | Prior module observation plus recorded enable/disable controls | PROVEN by native bridge response; discovery timing not separately instrumented |
| INITIALIZES / REGISTERS | Historical functioning adornment; separate startup timestamps unavailable | PROVEN: initialization and registration events/handles |
| CALLBACK EXECUTES | Legacy contribution measured; later 15b802… correction disabled, whole-page output unchanged | PROVEN: effect 0 → 1; control remains 0 |
| TEST ART REACHES CALLBACK | Victim and no-harm contribution measured | PARTIAL: invocation observed, input acceptance failed; 0 replacements, origin 0, error 1346458189 |
| HOST CLOSES CLEANLY | Not proved by specimen cleanup | FAILED observation: later Windows access violations; pre-install events also exist. P0; attribution UNKNOWN (AI-20). |

[The native load report](<ILLUSTRATOR_NATIVE_LOAD_REPORT.md>) and [FX-AI-LOAD] retain the first run. Binary SHA-256 is `4eb883eebc07c21acc1c8e13a366475713f7501969b61309b51cbbcbe5f0b5fc`; the old `4e595bfa…` report [FX-AI-BUILD] remains historical compile-only evidence. The actual run returned `passed: false`. A harness stage label that was not updated after rejection cannot erase the recorded callback counter. These prep-2 measurements predate the bounded prep-4 success below. Full Appearance, topology correction, cross-process persistence, and clean shutdown remain unproved.

**Prep-3 named rejection:** Illustrator 30.7.0 build 114R, binary `3cbb7fda5b259d75256cfa2d72b3e838521a3b3cc82a86994b91ab39f03be796`, records `stroke.keys-missing` with error 1346458189, one Go call, zero mutations, and origin zero. The processing path has two open anchors, fill=0, and stroke=0; Art 21, Path 13, Path Style 14, Dictionary 10, and Entry 7 acquire successfully. Both instance and parameter dictionaries exist. This identifies the observed rejection gate; successful replacement, effect export, and the broader cause remain unproved. [FX-AI-REJECT]

**Historical prep-4 evidence: PROVEN for the first two fresh specimen runs.** The [portable lifecycle record](<tests/audit/results/illustrator-native-lifecycle.json>) identifies Illustrator 30.7.0 build 114R and frozen binary `45fab742d70bfb12497b3735a1e3c9d4b14dd8dc9195551f0221f1c4297b1c78`. Native PIDs 64996 and 78916 each match five runner snapshots. Each run ends with three Go calls, three successful replacements, three accepted inputs, zero latest error, and stroke origin 3 (parameters). Both runs preserve the open two-anchor source through same-process AI save/reopen and reevaluate an endpoint edit afterward. Reopen redraw alone adds no callback. The first run records 230 passing assertions and the second records 231; repeated protocol checks are not independent cases. Independent MuPDF extraction of the first run finds one 8 pt control stroke and one effect fill rectangle, with bounds (100, 196)–(260, 204) in PDF coordinates. The second run writes new AI/PDF fixtures; it is not cross-process reopening of the first run’s files. Full rendering, arbitrary style synchronization, transformed strokes, closed paths, glyphs, topology correction, and missing-plugin fallback remain unverified. [FX-AI-PREP4]

**Historical shutdown failures remain unexplained.** The first quit helper did not record a target PID, and the later JavaScript request left PID 48928 alive after 30 seconds and controller exit despite zero documents. A later hardened prep-4 lifecycle on PID 38016 passes 332 assertions, including 27 interval checks, followed by a PID/start-matched direct-COM exit in a 3.882651 s recorded attempt. Prep-5 subsequently records the separate exit below. These observations establish those COM-route exits without explaining earlier non-exit or access violations. Broad stability remains a P0 concern; no recovered-document permission is pending. [FX-AI-PREP4], [FX-AI-PREP5]

**Current prep-5 native evidence: PROVEN for one bounded straight-line run and its direct-COM exit.** The [portable lifecycle record](<tests/audit/results/illustrator-native-lifecycle.json>) identifies Illustrator 30.7.0 build 114R, binary `201dd19cb25174c7fb9628fd1bf2769aa41e7d4dd4f2c6b77e5e9d7aea0fc9c7`, and native PID 5324 matched to five external snapshots. All 332 harness assertions pass, including 27 failed-callback interval checks; these include repeated protocol checks, not 332 independent cases. Go calls, mutations, and accepted inputs each reach 3, with latest error 0 and parameter stroke origin 3. Source edits, same-process AI save/reopen, and PDF export pass. A subsequent direct COM `Application.Quit` observes the same PID/start exit after zero documents, in a 4.021051 s recorded attempt. Nested shutdown was not forced in the host and has mock evidence only. This is not native region processing, general stroke support, or broad stability certification. [FX-AI-PREP5]

## Promotion and regression rules

A new result must name the immutable input, host build, plugin/source identity, output path, settings, oracle, controls, and artifacts. Keep the older result and explain any version/renderer/font change. Do not promote a whole family from a single favorable sample. Missing expected host evidence is UNKNOWN/UNEXECUTED, not NATIVE OK. A safe refusal needs proof that the relevant document state stayed unchanged. Pixel equality does not prove topology, and model equality does not prove exported appearance.

[S-ID-STROKE]: https://helpx.adobe.com/indesign/desktop/create-lines-and-shapes/edit-and-style-paths/line-stroke-options-and-settings.html

[S-ID-JOIN]: https://developer.adobe.com/indesign/uxp/dom/api/e/end-join/

[S-AI-STROKE]: https://helpx.adobe.com/illustrator/using/stroke-object.html

[S-AI-APPEARANCE]: https://helpx.adobe.com/illustrator/desktop/paint-and-fill/learn-painting-basics/create-multiple-fills-and-strokes.html

[S-AI-BRUSH]: https://helpx.adobe.com/illustrator/desktop/paint-and-fill/apply-and-edit-strokes/about-brushes.html

[S-ID-RELEASE]: https://helpx.adobe.com/indesign/desktop/whats-new/release-notes.html

[S-AI-RELEASE]: https://helpx.adobe.com/illustrator/desktop/new-features/release-notes.html

[S-ID-KNOWN]: https://helpx.adobe.com/indesign/desktop/troubleshoot/known-and-fixed-issues.html

[SDK-ID]: <STROKE_DEFECT_AND_LIMITATION_CENSUS.md#sdk-id>

[SDK-AI]: <STROKE_DEFECT_AND_LIMITATION_CENSUS.md#sdk-ai>

[FX-CORPUS]: <fixtures/FIXTURE_INDEX.md#fx-corpus>

[FX-A]: <fixtures/FIXTURE_INDEX.md#fx-a>

[FX-B]: <fixtures/FIXTURE_INDEX.md#fx-b>

[FX-C]: <fixtures/FIXTURE_INDEX.md#fx-c>

[FX-D]: <fixtures/FIXTURE_INDEX.md#fx-d>

[FX-D-CONTOURS]: <fixtures/FIXTURE_INDEX.md#fx-d-contours>

[FX-D-FAITHFUL]: <fixtures/FIXTURE_INDEX.md#fx-d-faithful>

[FX-D-HOOK]: <fixtures/FIXTURE_INDEX.md#fx-d-hook>

[FX-N]: <fixtures/FIXTURE_INDEX.md#fx-n>

[FX-S]: <fixtures/FIXTURE_INDEX.md#fx-s>

[FX-T]: <fixtures/FIXTURE_INDEX.md#fx-t>

[FX-VICTIM]: <fixtures/FIXTURE_INDEX.md#fx-victim>

[FX-FILLMASK]: <fixtures/FIXTURE_INDEX.md#fx-fillmask>

[FX-PRESERVE]: <fixtures/FIXTURE_INDEX.md#fx-preserve>

[FX-SYNC]: <fixtures/FIXTURE_INDEX.md#fx-sync>

[FX-OUTLINE]: <fixtures/FIXTURE_INDEX.md#fx-outline>

[FX-ZERO]: <fixtures/FIXTURE_INDEX.md#fx-zero>

[FX-OVERPRINT]: <fixtures/FIXTURE_INDEX.md#fx-overprint>

[FX-AI-OUTLINE]: <fixtures/FIXTURE_INDEX.md#fx-ai-outline>

[FX-AI-REGIONS]: <fixtures/FIXTURE_INDEX.md#fx-ai-regions>

[FX-AI-BUILD]: <fixtures/FIXTURE_INDEX.md#fx-ai-build>

[FX-AI-EXTREME]: <fixtures/FIXTURE_INDEX.md#fx-ai-extreme>

[FX-GLOBAL]: <fixtures/FIXTURE_INDEX.md#fx-global>

[FX-VICTIM20]: <fixtures/FIXTURE_INDEX.md#fx-victim20>

[FX-AI-LOAD]: <fixtures/FIXTURE_INDEX.md#fx-ai-load>

[PLAN-ID-LAYOUT]: <fixtures/FIXTURE_INDEX.md#plan-id-layout>

[PLAN-ID-SHAPING]: <fixtures/FIXTURE_INDEX.md#plan-id-shaping>

[PLAN-ID-TRANSFORM]: <fixtures/FIXTURE_INDEX.md#plan-id-transform>

[PLAN-ID-OUTPUT]: <fixtures/FIXTURE_INDEX.md#plan-id-output>

[PLAN-AI-BASIC]: <fixtures/FIXTURE_INDEX.md#plan-ai-basic>

[PLAN-AI-ALIGN]: <fixtures/FIXTURE_INDEX.md#plan-ai-align>

[PLAN-AI-APPEARANCE]: <fixtures/FIXTURE_INDEX.md#plan-ai-appearance>

[PLAN-AI-EFFECTS]: <fixtures/FIXTURE_INDEX.md#plan-ai-effects>

[PLAN-AI-WIDTH]: <fixtures/FIXTURE_INDEX.md#plan-ai-width>

[PLAN-AI-DASH]: <fixtures/FIXTURE_INDEX.md#plan-ai-dash>

[PLAN-AI-BRUSH-CALLIGRAPHIC]: <fixtures/FIXTURE_INDEX.md#plan-ai-brush-calligraphic>

[PLAN-AI-BRUSH-SCATTER]: <fixtures/FIXTURE_INDEX.md#plan-ai-brush-scatter>

[PLAN-AI-BRUSH-ART]: <fixtures/FIXTURE_INDEX.md#plan-ai-brush-art>

[PLAN-AI-BRUSH-PATTERN]: <fixtures/FIXTURE_INDEX.md#plan-ai-brush-pattern>

[PLAN-AI-BRUSH-BRISTLE]: <fixtures/FIXTURE_INDEX.md#plan-ai-brush-bristle>

[PLAN-AI-TEXT]: <fixtures/FIXTURE_INDEX.md#plan-ai-text>

[PLAN-AI-TOPOLOGY]: <fixtures/FIXTURE_INDEX.md#plan-ai-topology>

[PLAN-AI-GROUPS]: <fixtures/FIXTURE_INDEX.md#plan-ai-groups>

[PLAN-AI-TRANSFORM]: <fixtures/FIXTURE_INDEX.md#plan-ai-transform>

[PLAN-AI-PERSIST]: <fixtures/FIXTURE_INDEX.md#plan-ai-persist>

[PLAN-CORE-ADVERSARIAL]: <fixtures/FIXTURE_INDEX.md#plan-core-adversarial>

[PLAN-SAFETY]: <fixtures/FIXTURE_INDEX.md#plan-safety>

[PLAN-PERFORMANCE]: <fixtures/FIXTURE_INDEX.md#plan-performance>

[PLAN-ID-ABI]: <fixtures/FIXTURE_INDEX.md#plan-id-abi>

[PLAN-AI-UPSTREAM]: <fixtures/FIXTURE_INDEX.md#plan-ai-upstream>

[PLAN-AI-EMPTY]: <fixtures/FIXTURE_INDEX.md#plan-ai-empty>

[PLAN-NOHARM]: <fixtures/FIXTURE_INDEX.md#plan-noharm>

[FX-PERSISTENCE20]: <fixtures/FIXTURE_INDEX.md#fx-persistence20>

[FX-PAIR-RENDER]: <fixtures/FIXTURE_INDEX.md#fx-pair-render>

[FX-ID-REFUSAL]: <fixtures/FIXTURE_INDEX.md#fx-id-refusal>

[FX-AI-REJECT]: <fixtures/FIXTURE_INDEX.md#fx-ai-reject>

[FX-AI-PREP4]: <fixtures/FIXTURE_INDEX.md#fx-ai-prep4>

[FX-TOPOLOGY]: <fixtures/FIXTURE_INDEX.md#fx-topology>

[FX-AI-PREP5]: <fixtures/FIXTURE_INDEX.md#fx-ai-prep5>
