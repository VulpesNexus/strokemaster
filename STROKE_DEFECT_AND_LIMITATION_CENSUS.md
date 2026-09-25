# Stroke defect and limitation census

Evidence inventory as of September 13, 2026, anchored to checkpoint `c4596837a5e0ea10a68f7b94e2dd368032341334`. Findings draw on primary Adobe documentation, SDK declarations, retained host records, and fixture metadata. Each later result retains its own binary identity, host build, and output route.

**Current native capability (2026-09-25):** InDesign runs route W (`f31dd2c9…`), which removes all fill-core damage on the 20-case census in PNG and PDF export and keeps PDF text; its stroke edges differ from native by a sub-pixel ring, and screen rendering is unmeasured. Illustrator region-prep-7 publishes the topology correction of the marked analytic fixture, and region-prep-9 a stroke-behind correction on live text; both are bounded fixture results. See the [research revival](STROKEMASTER_RESEARCH_REVIVAL_2026-09-25.md). [FX-ID-REFUSAL], [FX-AI-PREP5]

**Standalone topology: bounded approval.** The [independent review](<tests/geometry/TOPOLOGY_REVIEW.md>) closes the reproduced protected-retention and remote/multipart-envelope failures at snapshot `f92abc24…`. All 24 exact remote-envelope orders refuse before union; all 24 tight controls retain the hole. The 80 seeded rectangle unions pass with 6,415 independent membership checks and zero refusals. Geometry discovery passes 72 tests with six dependency skips; those six separately pass in the repository environment. The core caller passes 43 checks. This establishes finite standalone polygon/API behavior; native topology integration, raw winding, curves, and universal numerical bounds remain unverified. [FX-TOPOLOGY]

This is the issue and experiment catalog. [The host matrix](<HOST_SUPPORT_MATRIX.md>) records support boundaries; [the fixture index](<fixtures/FIXTURE_INDEX.md>) records reproduction inputs and oracles. [The machine-readable registry](<fixtures/stroke-evidence-registry.json>) contains case IDs, planned fixture IDs, artifact locations, artifact hashes, and manifest subcases. It is an inventory snapshot, not a test runner. Historical user-guide claims do not define the desired support envelope.

## Evidence rules

Confidence applies to the precise observation in a row. **PROVEN** requires retained controlled measurement or an explicit source-only counterexample, with its limits stated. **STRONGLY SUPPORTED** includes Adobe's own documented semantics/fix notice or a historical report with incomplete raw provenance. **PLAUSIBLE** is a testable hypothesis. **UNVERIFIED** is not executed or has insufficient evidence. **DISPROVEN** applies only to the named rejected claim. A family can contain a proven observation and still have **UNKNOWN** product support.

Classifications are separate: **HOST DEFECT** is a demonstrated faulty behavior or an Adobe-acknowledged defect, bounded by version and evidence; **HOST LIMITATION** is missing behavior in the observed host model; **CONVENTIONAL VECTOR BEHAVIOR** is expected geometry or paint order; **API LIMITATION** is a gap in a particular inspected access route; **STROKEMASTER DEFECT** is wrong project behavior; **STROKEMASTER LIMITATION** is an unsupported implementation envelope; **UNKNOWN** has insufficient evidence for attribution.

Priorities follow the brief: **P0** data loss/crash/corruption; **P1** incorrect project output or harm to unaffected art; **P2** important unresolved target workflow; **P3** unsupported edge case with safe refusal as the intended outcome; **P4** cosmetic/documentation/usability. For unexecuted rows, priority ranks the gate's potential consequence, not a claim that a crash or damage occurred. Source-level refusal is not a proved safe host refusal.

**Native miter semantics remain a control.** Adobe documents miter-to-bevel fallback and distinct round/bevel joins [S-ID-STROKE], [S-ID-JOIN], [S-AI-STROKE]. The desired topology-aware outer envelope is a separate StrokeMaster invariant; local clipping is insufficient for that invariant [FX-GLOBAL]. Cross-frame z-order also remains conventional unless a separately defined correction explicitly changes it. A/C rows record the observed lack of frame-wide halo ordering as a host limitation; they do not claim Adobe violated PDF or PostScript semantics. D's reproduced conversion mismatch is classified separately.

**Evidence is not interchangeable.** A script `passed: true` can mean successful collection while measured output fails. Header declarations prove access surfaces, not host success. Historical corpus screenshots are native observations, not current plugin screen proof. PNG, PDF, EPS, SVG, print, model round-trip, and native lifecycle stages retain separate execution states. Renderer/version omissions remain omissions.

## Primary Adobe sources

| ID | Primary source | Page updated | Bounded finding |
| --- | --- | --- | --- |
| S-ID-STROKE | [Adobe InDesign stroke settings](https://helpx.adobe.com/indesign/desktop/create-lines-and-shapes/edit-and-style-paths/line-stroke-options-and-settings.html) | 2026-06-02 | Miter limit selects bevel fallback; caps, joins, alignment, and patterned strokes are distinct controls. |
| S-ID-JOIN | [Adobe InDesign EndJoin API](https://developer.adobe.com/indesign/uxp/dom/api/e/end-join/) | — | The enumerated joins are miter, round, and bevel. This is a bounded API statement, not an inventory of every possible extension. |
| S-AI-STROKE | [Adobe Illustrator stroke controls](https://helpx.adobe.com/illustrator/using/stroke-object.html) | 2025-01-03 | Documents ordinary stroke controls and limits alignment choices to closed paths excluding Live Paint groups. |
| S-AI-APPEARANCE | [Adobe multiple fills and strokes](https://helpx.adobe.com/illustrator/desktop/paint-and-fill/learn-painting-basics/create-multiple-fills-and-strokes.html) | 2025-10-27 | Objects/groups can have multiple fills and strokes, including duplicated entries; a single basic stroke value is not a full stack census. |
| S-AI-BRUSH | [Adobe brush families](https://helpx.adobe.com/illustrator/desktop/paint-and-fill/apply-and-edit-strokes/about-brushes.html) | 2025-10-27 | Separates calligraphic, scatter, art, bristle, and pattern brushes. Their geometries must not be treated as one uniform-stroke case. |
| S-ID-RELEASE | [Adobe InDesign release notes](https://helpx.adobe.com/indesign/desktop/whats-new/release-notes.html) | 2026-09-11 | Version boundaries are recorded in UP-ID-01/02. A display-engine update does not prove an export fix. |
| S-AI-RELEASE | [Adobe Illustrator release notes](https://helpx.adobe.com/illustrator/desktop/new-features/release-notes.html) | 2026-08-31 | Version boundaries are recorded in UP-AI-01/02. An Adobe-listed fix is not a locally verified fix. |
| S-ID-KNOWN | [Adobe InDesign known/fixed issues](https://helpx.adobe.com/indesign/desktop/troubleshoot/known-and-fixed-issues.html) | 2026-07-28 | Reviewed as a discovery source; no entry promoted to local StrokeMaster support. |

The release dates above are page/section metadata, not proof of the first affected build. Only the explicitly labeled version sections in UP-ID/UP-AI rows are used. Other unlabelled/accordion-collapsed sections were not assigned guessed fix dates. Adobe release notes may link to UserVoice; their own summary is the primary notice. Unreproduced community complaints are **UNVERIFIED leads**, never local support evidence. Community-only assertions remain unverified.

## Installed SDK access, bounded to declarations

<a id="sdk-id"></a>

The installed InDesign 2026 SDK declares per-run drawing plus parcel start/end hooks in *IGlobalTextAdornment.h* (Adobe InDesign 2026 SDK, *source/public/interfaces/text/IGlobalTextAdornment.h, line 147*), and render mode, miter, and alignment state in *IWaxRenderData.h* (Adobe InDesign 2026 SDK, *source/public/interfaces/text/IWaxRenderData.h, line 46*). These declarations establish inspection/intervention candidates, not a verified full-frame paint replacement, segmentation override, or transparency guarantee.

<a id="sdk-ai"></a>

The installed Illustrator 2026 SDK exposes *AIArtStyle.h* (Adobe Illustrator 2026 SDK, *illustratorapi/illustrator/AIArtStyle.h, line 297*) for style access, *AILiveEffect.h* (Adobe Illustrator 2026 SDK, *illustratorapi/illustrator/AILiveEffect.h*) for effect processing, and *AIPathfinder.h* (Adobe Illustrator 2026 SDK, *illustratorapi/illustrator/AIPathfinder.h, line 336*) for region operations. Declarations establish access, not execution. The current diagnostic's straight-line restrictions are not a statement that Illustrator lacks richer stroke capabilities.

## InDesign cases

| ID | Host / feature | Symptom / classification / confidence | Minimal repro | Screen | Export | SDK access | Script access | StrokeMaster status | Priority | Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="id-01"></a>ID-01 | InDesign: Inter-line Outside stroke paint order | Later run strokes obscure earlier fills; frame-wide halo ordering is the project requirement, not a proven violation of Adobe's documented join semantics. **HOST LIMITATION; PROVEN.** | A01–A03: two lines, 8 pt Outside Round; compare original/re-emitted/strokes-first PDF. | Historical corpus GPU/CPU/Overprint/normal evidence; no fresh plugin capture | HQ Print PDF → MuPDF 720 dpi: A01–A03 1,536 → 0 hidden pixels in historical plugin pairs | IGlobalTextAdornment draw/parcel hooks declared | Fixture generation and export available | Historical finite improvement only; current safety binary disables repaint | P2 | [FX-A], [FX-CORPUS]; [SDK-ID] |
| <a id="id-02"></a>ID-02 | InDesign: Unstroked victim among stroked lines | VC00 reproduces the 3,759 wrong-core-pixel victim failure against independent reference. Plugin-on/off controls now exist; their 10 core changed pixels are not the total victim failure. **STROKEMASTER DEFECT; PROVEN.** | VC00 first HH line None, second stroked; use VC01 reverse direction, VC02 both strokes, VC03 both None plus the remaining 16 cases. | UNEXECUTED | New InDesign 21.5.1.73 PNG 600 ppi: old-plugin/reference plus plugin-off/on comparisons; no fresh screen/PDF evidence | Victim selection/repaint mechanism under investigation | Controlled owned-document PNG probe; source word positions checked | Release blocker remains for tested legacy binary; no candidate fix proved | P1 | [FX-VICTIM], [FX-VICTIM20] |
| <a id="id-03"></a>ID-03 | InDesign: Cross-frame and threaded-story overlap | Separate frames follow z-order even when threaded. Global halo merging would require an explicit cross-object policy. **CONVENTIONAL VECTOR BEHAVIOR; PROVEN.** | A06/A07 reverse story order versus stacking; A08 unthreaded. | Historical corpus only; case-specific screen unexecuted | HQ Print PDF → 720 dpi; historical cross-frame residuals remain | Per-parcel hook is not a proven page-wide replacement | Split/reflow behavior outside supported simple frames unverified | Existing frame repaint cannot certify cross-frame halo preservation | P2 | [FX-A] |
| <a id="id-04"></a>ID-04 | InDesign: Formatting-run boundaries | Run paint interleaving causes seams; measured halo geometry is continuous. Do not diagnose missing geometry from the seam alone. **HOST LIMITATION; PROVEN.** | C00–C46 plus controls: one changed character in seven glyphs; compare paint order and unchanged re-emission. | Historical corpus views; individual sweep screen unexecuted | 50-frame HQ Print PDF sweep, 720 dpi operator/raster analysis | IWaxRenderData exposes drawing state; public segmentation override not proven | Applied attribute sweep; full per-attribute map in fixture index | Historical repaint improved finite cases; current safety binary disables all A/C corrections | P2 | [FX-C]; [SDK-ID] |
| <a id="id-05"></a>ID-05 | InDesign: Miter/round/bevel, limit, weight extremes | Ordinary miter spikes and bevel fallback follow native semantics. Custom bounded outer-envelope clipping is a separate product requirement. **CONVENTIONAL VECTOR BEHAVIOR; PROVEN.** | B1 limit 1–500; B2 alignment; B3 wedges; B4 weights; preserve benign native controls. | Historical corpus views; sweep screen unexecuted | HQ Print PDF M operator and 720 dpi renders; geometry in B manifest | Miter/join/alignment declarations exist | Text miterLimit reaches PDF | Native custom-envelope replacement unexecuted | P3 | [FX-B]; [S-ID-STROKE], [SDK-ID], [S-ID-JOIN] |
| <a id="id-06"></a>ID-06 | InDesign: Create Outlines fidelity and compound counters | Measured conversion changes painted width/alignment and overprint; simply reversing contours or choosing Outside is not a fidelity repair. **HOST DEFECT; PROVEN.** | D01 live/outlined pair; D05 alignment; D-contours reverse outer/counter directions. | Historical corpus D views; fresh conversion screen unexecuted | D PDF/720 dpi and path attributes; finite mismatch reproduced | Capture/replacement proposals only | Hardened staged converter has 13 host transaction checks | Finite script improvement; broader rendering and live editability not certified | P2 | [FX-D], [FX-D-CONTOURS], [FX-D-FAITHFUL], [FX-OUTLINE] |
| <a id="id-07"></a>ID-07 | InDesign: Tint, opacity, blend, spot color, overprint | Finite 23-sample cores agree, but new benign controls prove output changes outside the fill core; decoration/color cases need independent interpretation. **STROKEMASTER LIMITATION; STRONGLY SUPPORTED.** | N00–N22; add independent fill-only coverage, edge/alpha/separation oracles and mixed neighboring colors. | UNEXECUTED | N historical PNG 300 ppi: all 23 eroded fill cores have zero nonzero differences; full-page differences remain | Draw state available; inherited transparency correctness needs execution | Page-item overprint applicability measured separately from text | Finite coverage only; do not call all opacity/overprint safe | P1 | [FX-N], [FX-FILLMASK], [FX-OVERPRINT], [FX-VICTIM20] |
| <a id="id-08"></a>ID-08 | InDesign: None, zero weight, tiny weight, invisible stroke | Measured None color can retain a nonzero stored weight; identical-looking states must not be merged without checking semantics. **UNKNOWN; PROVEN.** | Four text access routes; None only, None+zero, 0.001 pt; extend to 0% opacity and style-present transparent strokes. | UNEXECUTED | DOM/IDML observations plus new VC07/08/09/16/17 PNG cases; 0% text opacity is not proof for 0% stroke-only opacity | RenderMethod distinguishes fill/stroke modes | Recorded None/weight assignments and sync round-trip | Script top uses None+zero; hidden/transparent state support unknown | P3 | [FX-ZERO], [FX-SYNC], [FX-VICTIM20]; [SDK-ID] |
| <a id="id-09"></a>ID-09 | InDesign: Layout: drop caps, columns, threading, overset, anchors, baseline grid | Coverage gap beyond A's threaded pairs and historical anchored outlines. **UNKNOWN; UNVERIFIED.** | Independent minimal cases: drop cap with next-line overlap; two columns; overset transition; inline object; baseline-grid reflow. | UNEXECUTED | UNEXECUTED | Composition/wax access does not certify these cases | Fixture APIs available in principle; individual gates unexecuted | No blanket support or safe-refusal proof | P2 | [PLAN-ID-LAYOUT] |
| <a id="id-10"></a>ID-10 | InDesign: Shaping and font changes | Latin sweep does not cover RTL/shaping-heavy or vertical text; C21 rotation was inconclusive. **UNKNOWN; UNVERIFIED.** | Vary ligatures/OpenType, optical versus metric kerning, variable axes, missing/substituted fonts, RTL and vertical text separately. | UNEXECUTED | UNEXECUTED | Glyph/run access declared; international correctness unexecuted | C's Latin ligature/axis controls are historical only | No general script/font support claim | P2 | [FX-C], [PLAN-ID-SHAPING] |
| <a id="id-11"></a>ID-11 | InDesign: Transforms and text on path | VC18 tests one 8-degree rotated frame; one favorable fill-core result does not cover all transforms or text on path. **UNKNOWN; STRONGLY SUPPORTED.** | Rotate/shear/reflect/nonuniformly scale a frame; compare text on path against untransformed source. | UNEXECUTED | VC18 old-plugin/reference PNG 600 ppi: zero core changes, 4,891 total changed pixels | Matrix/outline access is not a tested correction | Detection/refusal and transformed placement require per-case tests | Unknown outside recorded ordinary frames | P2 | [FX-VICTIM20], [PLAN-ID-TRANSFORM] |
| <a id="id-12"></a>ID-12 | InDesign: INDD/IDML, pair identity, resync authority | Two uniform/varied split pairs preserve identity, both roles, sampled character positions, sampled paint attributes, and None/zero top strokes through INDD reopen → IDML → INDD reopen; PNG 600 ppi hashes match. Arbitrary edits and idle timing remain unverified. **STROKEMASTER LIMITATION; PROVEN.** | Duplicate/delete/move/restyle one member; copy to another document; edit generated art and source separately; retain authority decisions. | UNEXECUTED | New pair PNG 600 ppi: 0 changed pixels and identical hashes; PDF, SVG, screen, and post-edit rendering remain UNEXECUTED | Native observer path remains unverified | Retained model checks plus two-pair rendered persistence; ISE_SPLIT labels and ISE3 fill/stroke roles preserved | Finite pair persistence proven; native binary 15b802… has correction disabled; broad resync/edit behavior unknown | P1 | [FX-SYNC], [FX-T], [FX-PAIR-RENDER] |
| <a id="id-13"></a>ID-13 | InDesign: Display, export, print, version boundaries | Historical screen parity cannot certify a new rendering engine or a new plugin build. **UNKNOWN; UNVERIFIED.** | Compare layout/normal/Preview, GPU/CPU, overprint, PDF presets, PNG, actual print/distilled output on identical saved inputs. | Old corpus views only; 21.6 unexecuted | HQ Print/PDF-X4/PNG/EPS historical; print/distill unexecuted | Each render path needs separate callback evidence | Export route must be recorded explicitly | Requalify 21.6; do not infer PDF behavior from a display change | P2 | [FX-CORPUS], [PLAN-ID-OUTPUT]; [S-ID-RELEASE] |
| <a id="id-14"></a>ID-14 | InDesign: Decorations: strikethrough and underline | VC15 has 5,430 core changes in plugin-on/off comparison, combining intended repair and damage; independent reference leaves 3,300 wrong core pixels. The 5,430 is not a damage-only count. **STROKEMASTER DEFECT; PROVEN.** | VC14 underline and VC15 strikethrough with same two-line overlap; preserve decoration color/position in independent fill/reference layers. | UNEXECUTED | New InDesign 21.5.1.73 PNG 600 ppi: old-plugin/reference plus plugin-off/on comparisons; no fresh screen/PDF evidence | Victim selection/repaint mechanism under investigation | Controlled owned-document PNG probe; source word positions checked | P1 output-fidelity failure; causal contribution still under investigation | P1 | [FX-VICTIM20] |
| <a id="id-15"></a>ID-15 | InDesign: Native INDD → IDML → INDD persistence without plugin | All 20 census frames preserve sampled character state and positions through IDML and INDD save/reopen, with matching rendered outputs. This is finite native preservation, not script-pair persistence. **UNKNOWN; PROVEN.** | Export generated no-plugin census, reopen IDML, save/reopen INDD, compare sampled fields, PNG, PDF raster, and PDF text. | UNEXECUTED | RGB opaque PNG 300 ppi: 0 changed pixels; PDF/X-4:2008 → MuPDF 288 ppi: 0 channel differences; extracted text equal. PyMuPDF package 1.28.2; PNG 300 ppi and PDF raster 288 ppi recorded. | No custom native processing exercised | Probe checks contents, horizontalOffset, baseline, strokeColor.name, strokeWeight, fillColor.name, strokeTint, and endJoin for each sampled character. | Plugin absent; no script-pair creation, metadata, synchronization, or missing-plugin fallback tested | P2 | [FX-PERSISTENCE20] |

## Illustrator cases

| ID | Host / feature | Symptom / classification / confidence | Minimal repro | Screen | Export | SDK access | Script access | StrokeMaster status | Priority | Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="ai-01"></a>AI-01 | Illustrator: Native discovery, loading, callback, and cleanup | Prep-5 completes the bounded lifecycle and PID-attributed COM exit; earlier runs remain historical. **STROKEMASTER LIMITATION; PROVEN.** | Open straight two-anchor control/effect specimens in native PID 5324; retain source edits, same-process reopen, counters, exports, and separate quit telemetry. | No independent screen/GPU/CPU comparison | AI save/reopen and PDF export pass; full rendering remains unverified. | SourceParameterSnapshot supplied through parameters; arbitrary style synchronization remains unverified. | 332 assertions including 27 interval checks; Go/mutations/accepted inputs each 3; latest error 0. | Straight-line processing and one COM-route exit pass; native region processing and broad stability unverified. | P2 | [FX-AI-BUILD], [FX-AI-LOAD], [FX-AI-REJECT], [FX-AI-PREP4], [FX-AI-PREP5]; [SDK-AI] |
| <a id="ai-02"></a>AI-02 | Illustrator: Basic path stroke, caps/joins, open/closed alignment | Basic setters work in one host probe. Diagnostic supports only a straight open two-anchor gray butt-cap case; not miter correction. **STROKEMASTER LIMITATION; PROVEN.** | Open/closed wedges; each cap/join/alignment and miter threshold; keep native and custom expectations separate. | UNEXECUTED | Basic DOM setters plus prep-5 open-line effect PDF; full cap/join/alignment family unexecuted | AIPathStyle/AILiveEffect available | 30.7.0 114R width=16, limit=2, miter setter checks | Prep-5 straight-line replacement passes; no miter or general stroke support claim | P3 | [FX-AI-REGIONS], [FX-AI-PREP5], [PLAN-AI-BASIC]; [S-AI-STROKE], [SDK-AI] |
| <a id="ai-03"></a>AI-03 | Illustrator: Inside/Outside alignment availability | Adobe documents alignment choices for closed paths excluding Live Paint groups; do not promise the same control for every path. **HOST LIMITATION; STRONGLY SUPPORTED.** | Compare open path, closed path, Live Paint edge, and outlined stroke. | UNEXECUTED | UNEXECUTED | Per-target representability not tested | Basic DOM cannot stand in for every Appearance stroke | Requirement/support decision remains scoped | P3 | [PLAN-AI-ALIGN]; [S-AI-STROKE] |
| <a id="ai-04"></a>AI-04 | Illustrator: Multiple fills/strokes and Appearance order | Benchmark basic character value 1 pt differs from exported 3 pt stroke; zero leaf stroke after outlining does not describe visible Appearance. **API LIMITATION; STRONGLY SUPPORTED.** | Object/group/layer/nested group; duplicate strokes; stroke above/below fill; inspect effective contribution before/after effects. | Benchmark manual confirmation pending | Existing extreme PDF state only; controlled stack PDF/SVG unexecuted | AIArtStyle and contribution Live Effect are candidates | Basic DOM underreports this benchmark; no claim that all APIs are incapable | Native full-stack support unexecuted | P2 | [FX-AI-EXTREME], [PLAN-AI-APPEARANCE]; [S-AI-APPEARANCE], [SDK-AI] |
| <a id="ai-05"></a>AI-05 | Illustrator: Transform, Offset Path, Pathfinder and nested effect order | A valid callback alone cannot establish correct stack position. **UNKNOWN; UNVERIFIED.** | Place correction before/after Transform, Offset Path, Pathfinder, opacity/blend; retain other strokes/fills. | UNEXECUTED | UNEXECUTED | AIArtStyle/AILiveEffect declarations only | Actual Unite test is narrower than nested effects | Unexecuted; no flatten-everything promise | P2 | [PLAN-AI-EFFECTS]; [SDK-AI] |
| <a id="ai-06"></a>AI-06 | Illustrator: Variable-width profiles | Uniform-width envelope semantics do not establish width-profile behavior. **UNKNOWN; UNVERIFIED.** | Taper, asymmetric width and abrupt width point; compare custom envelope and native expansion. | UNEXECUTED | UNEXECUTED | No inspected width-profile processing proof | No executed profile read/write proof | Support versus explicit refusal unresolved | P3 | [PLAN-AI-WIDTH]; [S-AI-STROKE] |
| <a id="ai-07"></a>AI-07 | Illustrator: Dashes and short dash joins | Diagnostic source refuses dashes; successful host refusal is unexecuted. **STROKEMASTER LIMITATION; STRONGLY SUPPORTED.** | Dash across acute corner; very short segment; round/butt cap; varying phase. | UNEXECUTED | UNEXECUTED | Diagnostic checks dash length | No dispatched dashed fixture | Source refusal only | P3 | [PLAN-AI-DASH]; [SDK-AI] |
| <a id="ai-08"></a>AI-08 | Illustrator: calligraphic brushes | Brush-specific output is not covered by the ordinary-stroke diagnostic. **UNKNOWN; UNVERIFIED.** | One calligraphic brush at an acute turn; expand a disposable copy and inspect output regions, opacity, and corner behavior. | UNEXECUTED | UNEXECUTED | Brush-specific processing contract not audited | No executed brush test | Support/refusal decision pending | P3 | [PLAN-AI-BRUSH-CALLIGRAPHIC]; [S-AI-BRUSH] |
| <a id="ai-09"></a>AI-09 | Illustrator: scatter brushes | Brush-specific output is not covered by the ordinary-stroke diagnostic. **UNKNOWN; UNVERIFIED.** | One scatter brush at an acute turn; expand a disposable copy and inspect output regions, opacity, and corner behavior. | UNEXECUTED | UNEXECUTED | Brush-specific processing contract not audited | No executed brush test | Support/refusal decision pending | P3 | [PLAN-AI-BRUSH-SCATTER]; [S-AI-BRUSH] |
| <a id="ai-10"></a>AI-10 | Illustrator: art brushes | Brush-specific output is not covered by the ordinary-stroke diagnostic. **UNKNOWN; UNVERIFIED.** | One art brush at an acute turn; expand a disposable copy and inspect output regions, opacity, and corner behavior. | UNEXECUTED | UNEXECUTED | Brush-specific processing contract not audited | No executed brush test | Support/refusal decision pending | P3 | [PLAN-AI-BRUSH-ART]; [S-AI-BRUSH] |
| <a id="ai-11"></a>AI-11 | Illustrator: pattern brushes | Brush-specific output is not covered by the ordinary-stroke diagnostic. **UNKNOWN; UNVERIFIED.** | One pattern brush at an acute turn; expand a disposable copy and inspect output regions, opacity, and corner behavior. | UNEXECUTED | UNEXECUTED | Brush-specific processing contract not audited | No executed brush test | Support/refusal decision pending | P3 | [PLAN-AI-BRUSH-PATTERN]; [S-AI-BRUSH] |
| <a id="ai-12"></a>AI-12 | Illustrator: bristle brushes | Brush-specific output is not covered by the ordinary-stroke diagnostic. **UNKNOWN; UNVERIFIED.** | One bristle brush at an acute turn; expand a disposable copy and inspect output regions, opacity, and corner behavior. | UNEXECUTED | UNEXECUTED | Brush-specific processing contract not audited | No executed brush test | Support/refusal decision pending | P3 | [PLAN-AI-BRUSH-BRISTLE]; [S-AI-BRUSH] |
| <a id="ai-13"></a>AI-13 | Illustrator: Live text, area text, text on path and editing | A native post-effect declaring path and compound input receives glyph outlines of live point text, one compound per glyph with its own fill and stroke; Illustrator's own PDF paints each run's fills, then its strokes. **HOST LIMITATION (paint order); PROVEN.** | Compare character stroke versus Appearance stroke, multiple strokes, area/path text, and edits without expanding originals. | UNEXECUTED | Stroke-behind fixture PDF: 55,568 → 0 damaged fill-core pixels; text is outlined under any effect | Live effect over styled art (region-prep-8/9) | createOutline() finite host evidence; not final destructive architecture | Stroke-behind proven for a two-line point-text fixture, edits, undo, and reopen; area text and text on path untested | P2 | [FX-AI-OUTLINE], [PLAN-AI-TEXT]; [SDK-AI], [prep-9](tests/audit/results/illustrator-region-prep9-stroke-behind.json) |
| <a id="ai-14"></a>AI-14 | Illustrator: Compound paths, holes, winding and self-intersections | Simple Unite and compound construction succeed; arbitrary Boolean holes and self-intersections were not tested. **UNKNOWN; PROVEN.** | Two squares: Unite area 15,000 pt²; square-with-hole area 7,500 pt². Add reversed winding and self-intersection fixtures. | UNEXECUTED | DOM area/path checks only; no PDF/SVG proof | AIPathfinder operations declared | Recorded host probe performs Add/Expand; hole example is construction | Finite primitive capability; general region solver unknown | P2 | [FX-AI-REGIONS], [PLAN-AI-TOPOLOGY]; [SDK-AI] |
| <a id="ai-15"></a>AI-15 | Illustrator: Clipping groups, symbols and transformed groups | Individual-path tests do not establish group/layer/symbol ownership. **UNKNOWN; UNVERIFIED.** | Clipping group, symbol instance, nested group and layer appearance; preserve hierarchy and unrelated members. | UNEXECUTED | UNEXECUTED | Native ownership path pending | No executed representative group/symbol case | Unknown | P2 | [PLAN-AI-GROUPS] |
| <a id="ai-16"></a>AI-16 | Illustrator: Nonuniform scale, reflection, rotation and shear | Envelope distance and stroke/effect scaling need an explicit coordinate-space policy. **UNKNOWN; UNVERIFIED.** | Apply each transform independently; toggle Scale Strokes & Effects; transform parent and child separately. | UNEXECUTED | UNEXECUTED | Style scale factors declared; correction not executed | No complete transform probe | Unknown | P2 | [PLAN-AI-TRANSFORM]; [SDK-AI] |
| <a id="ai-17"></a>AI-17 | Illustrator: GPU/CPU, PDF/SVG and independent renderer disagreement | Extreme PDF rasterizers disagree by 23,454 thresholded pixels in the recorded crop; neither is declared the geometry oracle. **UNKNOWN; PROVEN.** | Same extreme PDF/crop at 864 dpi in MuPDF and Poppler; add controlled CPU/GPU, PDF and SVG comparisons. | Controlled GPU/CPU pair unexecuted | PDF renderer difference proven; renderer versions not recorded in summary; SVG unexecuted | Native render callback path unexecuted | PDF parser/raster evidence only | Cause and intended result unknown | P2 | [FX-AI-EXTREME] |
| <a id="ai-18"></a>AI-18 | Illustrator: AI save/reopen and missing-plugin fallback | Prep-5 repeats the successful prep-4 same-process source-edit and reopen lifecycle; cross-process reopen and fallback remain unverified. **STROKEMASTER LIMITATION; PROVEN.** | Save/reopen each new control/effect AI in the same host process, then edit its endpoint. | UNEXECUTED | AI/PDF specimens written in the recorded prep-4 and prep-5 runs. | Stored parameter snapshot remains usable after same-process reopen; general serialization untested. | Reopen redraw adds no callback; subsequent endpoint edit triggers replacement. | Bounded persistence passes; arbitrary edits, cross-process reopen, and fallback remain open. | P0 | [FX-AI-PREP4], [FX-AI-PREP5], [PLAN-AI-PERSIST] |
| <a id="ai-19"></a>AI-19 | Illustrator: Absent, None, zero-weight, zero-opacity, hidden and disabled strokes | Visually empty Appearance entries can retain different editable state; no equivalence census has executed. **UNKNOWN; UNVERIFIED.** | Compare no entry, None paint, zero width, 0% stroke opacity, transparent color where representable, hidden entry and disabled effect/entry; inspect PDF/SVG and saved AI. | UNEXECUTED | UNEXECUTED | Basic path and full Appearance access must be distinguished | No executed full-state census | Support versus explicit refusal unresolved | P2 | [PLAN-AI-EMPTY] |
| <a id="ai-20"></a>AI-20 | Illustrator: Unexpected host exits around first native load | Earlier access violations and JavaScript non-exit remain unexplained; later prep-4 and prep-5 direct-COM exits are PID-attributed. **UNKNOWN; PROVEN.** | Retain matched PID/start, empty-document state, quit route, and observed process exit separately for each run. | Independent screen attribution unexecuted | Successful straight-line exports do not establish global host stability. | Direct-COM exit observed for prep-4 PID 38016 and prep-5 PID 5324. | Nested shutdown tested only by mocks; the host run did not force it. | P0 broad stability gate remains; no plugin-causality claim. | P0 | [FX-AI-LOAD], [FX-AI-PREP4], [FX-AI-PREP5] |

## Shared correctness and safety

| ID | Host / feature | Symptom / classification / confidence | Minimal repro | Screen | Export | SDK access | Script access | StrokeMaster status | Priority | Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="core-01"></a>CORE-01 | Both: Custom global outer envelope versus local clipped join | Correct local axial clipping can still exceed the required global envelope around holes/neighboring boundaries. **STROKEMASTER LIMITATION; PROVEN.** | Analytic rectangle/hole gap cases; compare native miter, local clip and region-aware target; retain winding and unchanged boundaries. | UNEXECUTED | Pure vector/analytic data; not an Adobe render proof | Shared geometry integration unexecuted | Prototype does not prove native takeover | Local-only sufficiency disproven; host topology integration remains unverified | P1 | [FX-GLOBAL] |
| <a id="core-02"></a>CORE-02 | Both: Adversarial geometry and tolerance | No host-certified topology pipeline for every degeneracy or font. **UNKNOWN; UNVERIFIED.** | Acute/nearly parallel, concave/convex, cusp, zero length, repeated anchors, loops/self-intersections, holes/nested holes, touching/overlapping glyphs, tiny counters/thin stems and huge stroke; save deterministic seeds. | UNEXECUTED | UNEXECUTED | Boolean declarations are not robustness proof | Independently reviewed standalone polygon/API experiment passes finite analytic, refusal, and rectangle-union controls; no host support inferred. | Bounded standalone kernel approved at f92abc24...; native topology, raw winding, curves, and universal numerical bounds remain unverified. | P1 | [FX-TOPOLOGY], [PLAN-CORE-ADVERSARIAL] |
| <a id="safe-01"></a>SAFE-01 | Both: Lifecycle: ownership, undo/redo, cancellation and recovery | Finite preservation probes do not certify undo, redo, cancellation, corruption recovery or all untouched art. **UNKNOWN; UNVERIFIED.** | Inject failure/cancel in owned test documents; exercise undo/redo, save/close, and missing-plugin recovery without touching user fixtures. | UNEXECUTED | UNEXECUTED | Host clean-close and rollback proof pending | Finite InDesign preservation checks exist | No comprehensive no-harm certification | P0 | [FX-PRESERVE], [PLAN-SAFETY] |
| <a id="perf-01"></a>PERF-01 | Both: Incremental invalidation and performance | No timing distribution or complete dependency graph for a correct native geometry candidate. **UNKNOWN; UNVERIFIED.** | Ordinary/pathological text, long paragraph/large document/path-heavy AI; initial/edit/reflow/redraw/export. Invalidate on text/font/glyph/size/stroke/leading/tracking/transform/frame/Appearance/parent/order changes. | UNEXECUTED | UNEXECUTED | Precise native notifications need proof | Direct sync tick is not event-timing evidence | Correctness first; no measured performance claim | P3 | [PLAN-PERFORMANCE] |
| <a id="safe-02"></a>SAFE-02 | Both: No-harm output and document-state controls | Actual legacy plugin-on/off control changes VC11 thin Round, VC12 thin Miter and VC13 thick nonoverlap by 2,964 pixels each, all outside the eroded fill core. **STROKEMASTER DEFECT; PROVEN.** | No strokes anywhere, benign thin strokes, round joins, ordinary miters below threshold, unrelated paths/text frames, differently colored neighbors, and a complex nontriggering document; require vector/pixel and document-state identity except permitted metadata. | UNEXECUTED | InDesign RGB PNG 600 ppi; no-plugin native.png versus legacy-plugin native.png. VC03 no stroke and VC09 0% text opacity unchanged. | Host clean-close and rollback proof pending | Finite InDesign preservation checks exist | P1 no-harm failure proven for legacy binary; do not dismiss non-core changes as harmless | P1 | [FX-VICTIM20], [PLAN-NOHARM] |
| <a id="safe-03"></a>SAFE-03 | InDesign: Safe-refusal binary preserves 20-frame output | Binary 15b802eb… has correction_active=false. Native/reference/mask PNG hashes match the plugin-absent control across the full 20-frame census; historical old-binary failures remain separate. **STROKEMASTER LIMITATION; PROVEN.** | Same 20-case input with plugin absent versus safe-refusal binary; compare entire native, reference, and mask PNGs. | UNEXECUTED | InDesign 21.5.1.73, RGB PNG 600 ppi: identical hashes and 0 changed pixels for all three whole-page images. | Output evidence does not prove active correction or universal lifecycle safety | The same generator completes; native correction is inactive | Finite no-harm refusal passes; target victim/overlap correction remains unavailable | P2 | [FX-ID-REFUSAL] |

## Upstream version-bound candidates

| ID | Host / feature | Symptom / classification / confidence | Minimal repro | Screen | Export | SDK access | Script access | StrokeMaster status | Priority | Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="up-id-01"></a>UP-ID-01 | InDesign: 21.6 rendering boundary | Adobe announces updated display rendering in 21.6; this does not establish an A/C correction or PDF change. **UNKNOWN; STRONGLY SUPPORTED.** | Repeat A/C/V/N and benign controls across 21.5.1.73 and legitimate 21.6 install when available. | UNEXECUTED | UNEXECUTED | Compatibility unexecuted | Compatibility unexecuted | Do not extrapolate historical support | P2 | [PLAN-ID-OUTPUT]; [S-ID-RELEASE] |
| <a id="up-id-02"></a>UP-ID-02 | InDesign: Older-SDK table API compatibility | Adobe lists a Windows 21.5.0 customer-plugin/table-API crash fixed in 21.5.1; this is not a reproduced StrokeMaster crash. **HOST DEFECT; STRONGLY SUPPORTED.** | Only if table APIs are used: legitimate version-pair diagnostic; record SDK and binary identity. | UNEXECUTED | UNEXECUTED | Adobe version-bound fix notice, not local load proof | Not applicable to DOM-only evidence | Avoid inferring ABI compatibility from marketing version | P0 | [PLAN-ID-ABI]; [S-ID-RELEASE] |
| <a id="up-ai-01"></a>UP-AI-01 | Illustrator: Aligned-stroke expansion and effect/mask upstream fixes | Adobe lists aligned-stroke expansion defects and live-effect/opacity-mask drawing fixes in 30.7. Neither is reproduced by this registry. **HOST DEFECT; STRONGLY SUPPORTED.** | Closed inside/outside stroke expansion: inspect closure/duplicate anchors; separately move live effect with opacity mask. | UNEXECUTED | UNEXECUTED | Use future native path inspection | Test candidates, not executed regressions | Retest before implementing an already-fixed workaround | P2 | [PLAN-AI-UPSTREAM]; [S-AI-RELEASE] |
| <a id="up-ai-02"></a>UP-AI-02 | Illustrator: 30.8 group Appearance indicator fix | Adobe lists a fix for groups falsely marked as having custom Appearance in 30.8. **HOST DEFECT; STRONGLY SUPPORTED.** | Attribute-free group versus genuinely styled group; inspect panel and model on version pair. | UNEXECUTED | UNEXECUTED | Indicator is not effective-style proof | Not tested | Do not trust panel badge alone as correction trigger | P4 | [PLAN-AI-UPSTREAM]; [S-AI-RELEASE] |

## Attribute-family and coverage boundary

The historical C manifest explicitly covers fill/color/tint, font, size, tracking, manual kerning, optical kerning, baseline shift, horizontal/vertical scale, skew, OpenType features, ligatures, character/GREP/nested styles, stroke color/weight/join/limit/alignment/overprint, language, conditions, XML/hyperlink metadata, variable axis, and named instance. All 50 entries are retained individually in the registry. A manifest's `applied` flag means the assignment succeeded, not that its visual effect or correction was verified.

The historical operator analysis distinguishes interleaved paints from exporter-only object splits; language is an example of the latter. Empty character/GREP/nested styles did not behave like styles that changed paint. Rotation C21 was inconclusive. Those are finite Minion Pro/Bahnschrift observations, not universal shaping rules. See [the historical mechanism table](<docs/STROKE_MECHANISMS.md#m4-run-fragmentation-fixture-c-is-m1-and-m2-at-run-boundaries-and-boundaries-follow-attribute-inequality>) and [FX-C].

Required missing families are explicitly planned: layout/shaping/transforms, all five brush kinds, variable width/dashes, effective Appearance at object/group/layer/nested-group levels, effect ordering, live text/area/path text, clipping/symbols, transform/scaling policy, empty/hidden stroke states, output paths, persistence/missing-plugin behavior, adversarial topology, invalidation/performance, and no-harm controls. No Cartesian product is implied; each plan calls for a minimal discriminating case.

## September 13 executed-result intake

The portable candidate records are [*tests/audit/results/host-victim-census.json*](<tests/audit/results/host-victim-census.json>), [*tests/audit/results/host-census-persistence.json*](<tests/audit/results/host-census-persistence.json>), and [*tests/audit/results/illustrator-first-native-load.json*](<tests/audit/results/illustrator-first-native-load.json>). Local build artifacts provide supporting provenance. The native stage labels are stale after the callback rejection; retained counters establish the invocation, not successful replacement.

The retained two 20-case InDesign runs both record `passed: true`, version `21.5.1.73`, and unchanged glyph positions. Their manifests compare equal. Successful collection does not mean correct output. Both export opaque RGB PNG at 600 ppi. The old-plugin and no-plugin files share the basename *native.png*, so the registry records full paths and input hashes.

| Case | No-plugin → old-plugin changed core / total pixels | Old-plugin → intended-reference wrong core | Interpretation |
| --- | --- | --- | --- |
| VC00 unstroked victim | 10 / 1,668 | 3,759 | Target failure remains; plugin contribution is not the total failure count. |
| VC03 no stroke | 0 / 0 | 0 | Finite no-harm control passes. |
| VC09 0% text opacity | 0 / 0 | 0 (empty fill core) | Output unchanged; does not test stroke-only opacity. |
| VC11 thin Round | 0 / 2,964 | 0 | P1 no-harm failure outside the core. |
| VC12 thin Miter | 0 / 2,964 | 0 | P1 no-harm failure outside the core. |
| VC13 thick, nonoverlapping | 0 / 2,964 | 0 | P1 no-harm failure outside the core. |
| VC15 strikethrough | 5,430 / 19,983 | 3,300 | Contribution mixes intended repair and damage; 5,430 is not a damage-only count. |

The independent reference uses separate centered doubled strokes behind original fills and checks character positions. Its applicability is bounded to these cases; decorations, rotated bounds, and other oracles still need scrutiny. Do not dismiss all non-core differences as harmless antialiasing. [FX-VICTIM20]

Illustrator `30.7.0 / 114R`, binary `4eb883eebc07c21acc1c8e13a366475713f7501969b61309b51cbbcbe5f0b5fc`, diagnostic `native-prep-2`, now has actual initialization, registration, and callback evidence. The effect advances `goCalls` to 1 but has `mutations = 0`, `strokeOrigin = 0`, and `lastGoError = 1346458189`. The control PDF exists; effect export was not reached. A failed callback with origin zero does not by itself locate the failing guard or prove the style key was missing. That first-run counter did not name the gate; the separate prep-3 record below does. Disposable specimens closed and measured state was restored; later unexpected host exits are recorded with unknown attribution (AI-20). See [the native load report](<ILLUSTRATOR_NATIVE_LOAD_REPORT.md>) and [FX-AI-LOAD].

**Native persistence: PROVEN, finite scope.** Without the plugin, all 20 frames preserve eight sampled character fields through INDD → IDML → INDD save/reopen. PNG at 300 ppi has 0 changed pixels; PDF/X-4:2008 raster comparison has 0 differing channels, and extracted text is equal. The portable record identifies MuPDF via PyMuPDF 1.28.2 at 288 ppi with alpha disabled, PNG at 300 ppi, NumPy 2.5.3, and Pillow 12.3.0. The PDFs have different file hashes, so this is render/text equality, not byte identity. This does not test script-pair creation, metadata, synchronization, or script-pair round-trip. Fourteen PDF preference fields and one EPS field remain unverified by the restoration logger. [FX-PERSISTENCE20]

**Unexpected Illustrator exits: P0, attribution UNKNOWN.** The native load report records Windows `0xc0000005` events for Illustrator `30.7.0.114` at local times 00:52:20 (offset `0x849dee`) and 00:58:07 (`0x18162a7`). Events at 00:16:00 and 00:38:17, before installation, also name `0x18162a7`. No per-invocation process-ID correlation or controlled reproduction establishes causality. The faulting executable name neither blames nor clears the plugin. Specimen cleanup does not establish a clean host exit. [FX-AI-LOAD]

**Split-pair persistence: PROVEN for two pairs.** On InDesign 21.5.1.73 with safe-refusal binary `15b802ebfb38f567eb3aa586e995c0a1afff47efd078f3e2a0443596b54df4c4`, uniform and varied Outside-stroke pairs preserve `ISE_SPLIT` labels, `ISE3` identities, fill/stroke roles, sampled character positions, sampled paint attributes including fillTint, and None/zero top strokes through INDD save/reopen → IDML → INDD save/reopen. Pre-save and final opaque RGB PNGs at 600 ppi have identical hashes and 0 changed pixels. The final checked rerun verifies that the lower fill matches the source stroke color and tint. The harness had incorrectly required a None lower fill; correcting that assertion changes no product behavior. The final image hash remains `1d79f5a038dc21163b9510a9f32064fb8111d54ef81969967dd9a7a6467e69df`, matching the earlier finite render proof. This separately closes the two-pair rendered persistence gap; arbitrary edits, idle resync, PDF/SVG/print, other builds, and full lifecycle remain unverified. [FX-PAIR-RENDER]

**Safe refusal: PROVEN for the 20-frame PNG census.** The same binary records `correction_active: false`. All three whole-page PNGs—native output, independent reference, and mask—match the absent-plugin hashes at 600 ppi, with 0 changed pixels. This establishes finite no-harm output while correction is disabled. It does not repair the target failure or erase old binary `d476f565…` failures. Screen, PDF/print, active correction, and full document-state/lifecycle safety remain unverified. [FX-ID-REFUSAL]

**Hardened census and persistence: PROVEN for the retained 20-frame rerun.** The [checked record](<tests/audit/results/host-census-checked.json>) identifies InDesign 21.5.1.73 and safe-refusal binary `15b802ebfb38f567eb3aa586e995c0a1afff47efd078f3e2a0443596b54df4c4`. Collection and persistence both pass. Native, reference, and mask PNGs at 600 ppi are byte-identical to the no-plugin control, with 0 changed pixels and a black mask corner `(0, 0, 0)`. The mask tint readback remains −1 and unresolved; exact image equality validates this retained mask only. The persistence rerun preserves the eight listed character fields and frame names, with 0 changed PNG pixels at 300 ppi, 0 differing PDF raster channels at 288 ppi, and equal extracted text. Fourteen PDF preference fields and one EPS field remain unverified. Correction is disabled; this result does not establish active correction, screen equivalence, or complete document-state preservation.

**Prep-3 named rejection:** Illustrator 30.7.0 build 114R, binary `3cbb7fda5b259d75256cfa2d72b3e838521a3b3cc82a86994b91ab39f03be796`, records `stroke.keys-missing` with error 1346458189, one Go call, zero mutations, and origin zero. The processing path has two open anchors, fill=0, and stroke=0; Art 21, Path 13, Path Style 14, Dictionary 10, and Entry 7 acquire successfully. Both instance and parameter dictionaries exist. This identifies the observed rejection gate; successful replacement, effect export, and the broader cause remain unproved. [FX-AI-REJECT]

**Historical prep-4 evidence: PROVEN for the first two fresh specimen runs.** The [portable lifecycle record](<tests/audit/results/illustrator-native-lifecycle.json>) identifies Illustrator 30.7.0 build 114R and frozen binary `45fab742d70bfb12497b3735a1e3c9d4b14dd8dc9195551f0221f1c4297b1c78`. Native PIDs 64996 and 78916 each match five runner snapshots. Each run ends with three Go calls, three successful replacements, three accepted inputs, zero latest error, and stroke origin 3 (parameters). Both runs preserve the open two-anchor source through same-process AI save/reopen and reevaluate an endpoint edit afterward. Reopen redraw alone adds no callback. The first run records 230 passing assertions and the second records 231; repeated protocol checks are not independent cases. Independent MuPDF extraction of the first run finds one 8 pt control stroke and one effect fill rectangle, with bounds (100, 196)–(260, 204) in PDF coordinates. The second run writes new AI/PDF fixtures; it is not cross-process reopening of the first run’s files. Full rendering, arbitrary style synchronization, transformed strokes, closed paths, glyphs, topology correction, and missing-plugin fallback remain unverified. [FX-AI-PREP4]

**Historical shutdown failures remain unexplained.** The first quit helper did not record a target PID, and the later JavaScript request left PID 48928 alive after 30 seconds and controller exit despite zero documents. A later hardened prep-4 lifecycle on PID 38016 passes 332 assertions, including 27 interval checks, followed by a PID/start-matched direct-COM exit in a 3.882651 s recorded attempt. Prep-5 subsequently records the separate exit below. These observations establish those COM-route exits without explaining earlier non-exit or access violations. Broad stability remains a P0 concern; no recovered-document permission is pending. [FX-AI-PREP4], [FX-AI-PREP5]

**Current prep-5 native evidence: PROVEN for one bounded straight-line run and its direct-COM exit.** The [portable lifecycle record](<tests/audit/results/illustrator-native-lifecycle.json>) identifies Illustrator 30.7.0 build 114R, binary `201dd19cb25174c7fb9628fd1bf2769aa41e7d4dd4f2c6b77e5e9d7aea0fc9c7`, and native PID 5324 matched to five external snapshots. All 332 harness assertions pass, including 27 failed-callback interval checks; these include repeated protocol checks, not 332 independent cases. Go calls, mutations, and accepted inputs each reach 3, with latest error 0 and parameter stroke origin 3. Source edits, same-process AI save/reopen, and PDF export pass. A subsequent direct COM `Application.Quit` observes the same PID/start exit after zero documents, in a 4.021051 s recorded attempt. Nested shutdown was not forced in the host and has mock evidence only. This is not native region processing, general stroke support, or broad stability certification. [FX-AI-PREP5]

## Claim corrections and intake gates

- **DISPROVEN:** ordinary native miter spikes establish a broken miter implementation; the B evidence instead follows conventional joins. The custom envelope is a new requirement.
- **DISPROVEN:** a corrected local axial clip guarantees the global outer envelope; retained analytic counterexamples distinguish them [FX-GLOBAL].
- **DISPROVEN:** a second fill pass is universally equivalent to painting fills once after all strokes. The existing opaque-core agreement does not certify edges, alpha, or overlapping paint [FX-FILLMASK].
- **DISPROVEN:** all nearby victim fills are fixed by the legacy plugin; V0 remains wrong. The module was observed after the test with disk SHA-256 `d476f565ae57fa9257111d5d86f4e8a313c9efb7bc85d8912e9b5842ec80f342`. That older observation alone has no no-plugin control [FX-VICTIM]. The new 20-case plugin-off/on comparison now isolates the legacy binary contribution [FX-VICTIM20].
- **UNVERIFIED:** a new host version, newly built binary, passing pure-test count, or successful DOM probe certifies native takeover. The old build report records x64/PiPL only [FX-AI-BUILD]; the new host result separately proves loading and a rejected callback [FX-AI-LOAD].

For each new retained result, record the fixture/subcase ID, exact host version/build, OS/architecture, source checkpoint, plugin SHA-256, observed load stage, render/export route, export settings, renderer/version, font provenance, input/output hashes, controls, measured result, and unresolved fields. Preserve failing artifacts and earlier observations. Promote a status only for that tested combination. A new V0 fix must include both unmodified and all-stroked/no-stroke controls; a native Illustrator result must distinguish discovery, load, initialization, registration, callback, actual-art delivery, and clean close.

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
