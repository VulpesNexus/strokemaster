# InDesign native paint interception audit

SDK and source investigation, September 12–13, 2026. The supervisor independently checked the adornment, event-dispatcher, and whole-shape draw contracts against the installed SDK. The report distinguishes those contracts from actual-host observations.

**No inspected public contract establishes a registered, stroke-only veto that leaves Adobe’s existing glyph-fill drawing and composition untouched.** `IGlobalTextAdornment` supplies additional drawing, not a native-render veto. A registered draw-event handler does have a demonstrated stop-drawing path, but the supplied implementation establishes it at whole-shape granularity. `IWaxRunText` is the relevant run renderer; `IWaxRunShape` is not the drawing override its name suggests. A custom run renderer is an SDK construction route for runs the plugin creates, not evidence that Adobe’s composers will instantiate it.

“Not established” is a finite conclusion about the inspected SDK, not proof that Adobe has no other extension or private implementation. The smallest unresolved public lead is the dispatch behavior of `kBeforeFrameText`. Its name alone does not establish that returning true suppresses text, much less only its stroke.

**Resolved 2026-09-26 in the host:** InDesign 21.5.1 never dispatches `kBeforeFrameText` or `kBeforeFrameForeground`, in screen, PDF, PNG, EPS, print, or background export, and returning handled for them changes nothing. `kDrawShapeMessage` does veto a whole page item. See [CROSS_FRAME_RESEARCH_2026-09-26.md](../CROSS_FRAME_RESEARCH_2026-09-26.md#4-candidate-host-apis).

## Evidence boundary

The SDK inspected is the local *Adobe InDesign 2026 SDK* under the sibling *Adobe SDK* directory. References below use exact one-based physical header/source lines. The generated SDK HTML is supplemental; its object-model pages describe implementation assignments but do not expose Adobe’s native text renderer body. Repository HEAD is `fb15301d2b7e2d12998aafc2d112fdb80d95820d`; the audit reads the current modified working tree, not just HEAD. The inspected *IseFillRepaintAdornment.cpp* SHA-256 is `EC036C16EED2277DE8BCBAD671F24DF67745F7E7B48893C2C25F29BFEAEB7948`.

Prior investigation documents were used to locate hypotheses, not as proof of SDK contracts. A supplemental official-domain web search returned community discussions and scripting references; none is used to establish the C++ interception verdict. The primary evidence here is the installed Adobe SDK and repository source. No universal claims about screen, PDF, EPS, printing, composition, or transparency follow from declaration visibility or one output fixture.

## Ranked interfaces and routes

Rank reflects usefulness for the requested requirement and the next discriminating investigation, not chronological draw order. The earliest **proven** stop point in the supplied source is the whole-shape draw filter/event before `DrawShape`; the earliest **text-specific candidate** is the frame-text event. Neither is a proven stroke-only interception.

| Rank | Concrete interface or route | What the evidence establishes | Fit and registration boundary |
| --- | --- | --- | --- |
| 1 | `IDrwEvtHandler` / `IDrwEvtDispatcher`, `kBeforeFrameText` | The dispatcher supports registered handlers and a handled return. The text event IDs are public at [S1], lines 108–112. No dispatch call site for these text events was found in the inspected supplied C++ sources. | Best small investigation for retaining Adobe’s existing composition. Whether this event’s return is honored, its precise drawing scope, and whether native fill can be delegated independently are unresolved. Registered extension, not a factory patch; **not implementation-ready**. |
| 2 | Custom `IWaxRunText`, optionally derived from `CWaxRunText` | [S2], lines 39–52, places text and adornment drawing here. [S3], lines 43–61, documents pass dispatch and requires subclasses to implement the actual text drawing overload. | The closest public implementation boundary to native glyph painting. A new boss can supply its own renderer for its own runs. No inspected registration contract redirects stock composers to that new boss or exposes Adobe’s protected fill-only renderer. Reimplementing glyph drawing is not preservation of the native fill implementation. |
| 3 | Registered whole-shape draw handler: `kFilterCheckMessage` / `kDrawShapeMessage` | [S4], lines 83–119, supplies registration and stop semantics. [S5], lines 192–218, returns before `DrawShape` when the draw filter or shape event handles the operation. | A real coarse veto in the supplied base implementation. Suppressing a frame’s shape can suppress native fills and other shape content too. Calling back into the same draw path risks recursion; selective delegation and every stock frame/export path are not established. Useful diagnostic control, insufficient stroke-only solution. |
| 4 | `IParagraphComposer` / `kTextEngineService` and custom wax construction | [S6], lines 57–61, identifies the service. [S7], lines 93–105, registers the SDK sample composer. [S8], lines 481–519 and 531–558, explicitly creates a run, populates glyphs/render data, attaches adornments, and inserts it into a line. | Legitimate ownership route for custom run bosses, but it takes responsibility for composition/rebuilding. The sample actually creates `kWaxTextRunBoss`, not a custom stroke renderer. Subclassing a boss does not make Adobe’s existing composers select it. A wrapper/delegation design preserving all stock composition remains unproved. |
| 5 | `IGlobalTextAdornment` / `ITextAdornment` / `IAdornmentShape` | [S9], lines 68–113, gates the adornment’s participation; lines 140–164 provide void drawing/notification methods. [S10], lines 44–83, explains additive pass placement. | Established registered additional drawing. No return value here authorizes suppressing native glyph fill/stroke. An earlier priority changes when the adornment draws; it does not remove the native text draw at `kTAPriDrawText`. Current repaint architecture belongs here. |
| 6 | `IWaxRunShape`, `IWaxLineShape`, `IParcelShape`, and pass lists | [S11], lines 35–73, makes `IWaxRunShape` a bounds/pass-reporting interface; its complete declaration ends at line 173 without `Draw`. [S12], line 55, and [S13], lines 61–64, expose line/parcel draw entry points. | These are implementation/dispatch surfaces, not a demonstrated registered selective text-pass filter. Removing a required text pass would also remove fills. `IWaxRunText.h` still says “Called by IWaxRunShape”; that comment does not add a missing virtual method to the current shape interface. |
| 7 | `IDrawMgr`, graphics-port interception, and render-data mutation | [S14], lines 48–66, 128–168, and 212–221, concerns port drawing, clipping, and item disabling. [S15], lines 542–571, provides fill/stroke flags on caller-issued text operations. [S16], lines 70–99, exposes render-data setters and a derived render method. | Useful primitives when a plugin owns a draw pipeline; no inspected callback says “replace only Adobe’s Outside stroke.” Port flags control the call issuing them, not an already scheduled native call. Clipping affects paint spatially, not selectively by stroke versus fill. Cached render-data edits are not an established draw override. |

## Why adornment return values cannot be treated as a veto

`IGlobalTextAdornment::GetIsActive` can avoid an otherwise unnecessary pass and prevents that adornment’s `Draw` from being called; the header says this explicitly at [S9], lines 68–74. `GetCouldDraw` asks whether that adornment wants the run, not whether the run may draw, at lines 92–105. `Draw`, `StartOfParcelDraw`, and `EndOfParcelDraw` all return void, at lines 147, 156, and 163. Their run/render/glyph parameters are const. A `const_cast` followed by changing the shared wax would be a mutation experiment, not use of a documented veto.

The current plugin registers its own service boss in [R1], lines 66–76, using `kGlobalTextAdornmentServiceImpl` and `IID_IGLOBALTEXTADORNMENT`. The SDK declares the service and implementation IDs at [S17], lines 71 and 1424. That is genuine SDK service registration. Returning false from its eligibility methods only removes its repaint; it cannot selectively cancel Adobe’s stroke.

Nor does an early adornment provide a documented way to leave a clip or zero-width stroke state installed for the next native operation. Native state installation, save/restore scopes, run caches, and later adornments can defeat or broaden that trick. [S18], lines 52–74, explicitly describes rendering-object/font optimizations and cache clearing. A plan that temporarily rewrites `IWaxRenderData` before a pass and restores it later needs a proven lifetime, cache, reentrancy, error, and output-mode contract. None was established here.

## Native renderer replacement versus supported extension

`CWaxRunText` is useful infrastructure, but its public four-argument `Draw` draws text and adornments, while its protected three-argument text draw is pure virtual [S3]. It does not expose a callable Adobe implementation of “draw this stock run’s native fill only.” `IGraphicsPort::xshow` permits `kFillText` and carries source-text information needed for searchable PDF [S15], lines 561–575. That could support a replacement renderer using composed glyphs, but the plugin would then own native text metadata, positioning modes, transforms, rendering objects, clipping, decorations, and output semantics. Keeping glyph IDs and advances is weaker than keeping the original native fill/composition pipeline.

The generated object-model page [S19] assigns `IID_IWAXRUNTEXT` on `kWaxTextRunBoss` to `kWaxRunTextImpl`. Declaring that ImplementationID in a header is not a factory replacement hook. The inspected public/open source search found no symbol named `ReplaceFactory`. The actual `IObjectModel` factory registration methods occur inside a section explicitly marked **For Internal Use Only**, [S20], lines 165–177. Likewise, `IClassInfo::AddInterface` is below its internal-use boundary at [S21], lines 98 and 142–157.

Registering a new plugin-owned class, implementation, composer service, or draw-event handler is distinct from overwriting Adobe’s implementation factory, patching a vtable, detouring a binary, or forcing a duplicate interface assignment onto a stock boss. The latter routes have no supported native-text replacement contract established by this audit. Do not describe them as registered SDK stroke extensions. Conversely, this report does not claim all boss inheritance or all AddIn overrides are prohibited: SDK examples support subclassing, and some inherited-interface overrides have documented contexts. No equivalent authorization was found for taking over the existing native text-run implementation.

`ReplaceImplementation` in [S22], line 149, is a schema/file-conversion directive, not a live drawing-factory interception API; the file’s purpose at line 23 and the guide’s schema section distinguish it. `IParagraphComposer::RebuildLineWithOverrides` is also not a text-pass hook: [S6], lines 743–756, describes regenerating composed data and permits refusal. `ITextUtils::GetGlyphsForOverrides`, [S23], lines 1216–1226, returns glyph/index results and explicitly does not modify the composed frame. Neither declaration promises suppressing a paint operation while retaining existing composition.

The similarly named `DrawFilter` helper is explicitly marked “Obsolete - do not use” in [S26], lines 31–43. It should not be confused with the registered draw-event dispatcher or proposed as the supported replacement route.

## Current plugin defects and ordering risks

**P1 — Unstroked-victim coverage hole: source-confirmed and host-demonstrated for the supplied fixture.** [R2], in `WantsRepaint`, requires Outside alignment, positive outline width, and `kFillStroke`. Both eligibility and drawing apply that test. An unstroked filled run is excluded even when a later run's stroke overlaps its fill. The original comment incorrectly said only runs with both paints could suffer. That comment is now corrected; eligibility behavior is unchanged.

The supervisor ran [R3] in InDesign **21.5.1.73**, using opaque RGB PNG at **600 ppi**, antialiasing enabled, and overprint simulation disabled. The probe compares the existing native/plugin route with a split reference and creates a separate white-on-black fill mask. I read its source and the saved [result][R4] and [comparison][R5]; I did not run the host. The supervisor reports matching glyph placement; the probe’s explicit equality check records word horizontal offsets and baselines, not every glyph matrix. A subsequent module inspection found the loaded InDesignStrokeEngine module; its disk SHA-256 is recorded in the committed result.

| Fixture | Eroded fill-core changes | Other changes | Bounded interpretation |
| --- | --- | --- | --- |
| V0: first line unstroked, second line stroked | **3,759 of 11,600** core pixels differ; native mean core luminance **82.57**, split **0.0**, delta **−82.566** | 10,377 outside-core changed pixels; 14,136 total | Demonstrates the victim defect relative to this split reference and the independent core mask. The change lies in the glyph interior, not merely an antialiased edge. |
| V1: both lines stroked | **0** core changes | **3,219** outside-core changes | Positive control for core coverage only; outside-core differences are not established harmless. |
| V2: neither line stroked | **0** core changes | **0** changed pixels | Negative control for this fixture. |

The comparison uses a six-pixel erosion of the external fill mask and explicitly records `fill_compositing_verified: false`. These observations do not certify all fills, fonts, transparency states, decorations, or export backends. Broadening repaint eligibility could address the missing victim, but would also broaden duplicate-fill compositing and ordering exposure; it would still not suppress native stroke generation.

**P1 — Native decoration ordering conflict: contract/source-confirmed order, visual failure not yet measured here.** The plugin chooses foreground `+0.90` at [R2], line 202. SDK priorities at [S10], lines 160–176, place native glyph drawing at text `+0.50`, strikethrough at `+0.55`, kenten at `+0.60`, and ruby at `+0.65`. Paragraph shading, borders, rules, and underline are earlier background operations. [S24], lines 32–41, establishes frame-wide pass traversal and within-run fractional ordering; [S25], lines 90–95, supplies the actual pass ordering. Thus the late fill repaint occurs after native strikethrough/ruby/kenten and can cover their overlapping portions. This is an overlap-dependent rendering risk, not proof that every decorated document visibly changes.

Global show-invisibles and dynamic spelling priorities are foreground `+0.50` and `+0.52`, [S9], lines 180–181. Within the same run, repaint at `+0.90` follows them. Across runs in that same pass, the outer run traversal matters; it is incorrect to claim that this fractional priority draws after every decoration of every run. Page-item adornments at `kAfterTextForeground` occur after text adornment priorities [S10], lines 75–83, so the plugin is not universally the last painter either. An earlier repaint could avoid some overlay conflicts while reintroducing later-stroke coverage: priority choice alone does not solve both constraints.

**P2 — Duplicate-fill compositing remains a separate limitation.** [R2], lines 316 and 352, installs the run fill and emits another path fill. Requesting the same rendering object does not establish that painting twice is equivalent to painting once under antialiasing, opacity, blending, overprint, or overlapping different colors. The new victim measurement establishes a particular core defect and useful controls; it does not close this broader question.

## Cheapest discriminating next checks

These are proposed investigations, not executed snippets or permission requests. No host check was performed during this audit.

1. **Resolve the frame-text event contract before building a replacement.** First request or locate the actual `kBeforeFrameText` dispatch contract. In the next diagnostic plugin, register a no-op handler and then return handled only for one disposable target frame. Log event order and flags; compare screen, PNG, and PDF. Determine whether text, adornments, or the entire content disappear. A handled return stopping only lower-priority handlers is not proof of stopping the native caller.
2. **Inventory live boss assignments without changing them.** `IClassInfo::GetImplementation`, line 64, and `GetOwnerComponent`, line 93 [S21], can distinguish ordinary run classes from specialized runs. This determines whether even a hypothetical single-boss takeover would miss other native text paths. Do not call the internal mutation/factory APIs.
3. **Prove renderer ownership before fidelity work.** A plugin-owned run boss/custom composer diagnostic must demonstrate that its renderer is instantiated through supported registration. Separately verify whether Adobe composition can remain the producer. If this needs a global factory overwrite, classify that route as unsupported and stop calling it a stock-composition extension.
4. **Measure decoration overlap with minimal fixtures.** Reuse a stroked victim case with a conspicuous strikethrough through the glyph interior; test ruby/kenten separately. Compare plugin off/on with fixed word/glyph placement and a decoration-only mask. Check screen-only invisibles/spelling separately from print/export. This discriminates the known priority conflict without assuming every adornment behaves alike.
5. **Retain the new V0/V1/V2 evidence as a regression requirement.** Any stroke-replacement candidate must fix V0 while keeping the original native fill contribution single, preserving decoration order, and explaining V1’s outside-core differences. Then add one translucent overlapping-color fixture and verify PDF text extraction as separate tests; a clean opaque PNG is not proof of either property.

The remaining architecture blocker is a supported, sufficiently selective native-paint interception. The frame-text event’s native dispatch still needs an isolated host probe. This report does not establish a production replacement route.

## Source references

[S1]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/interfaces/graphics/DocumentContextID.h:108>

[S2]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/interfaces/text/IWaxRunText.h:39>

[S3]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/includes/CWaxRunText.h:43>

[S4]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/interfaces/graphics/IDrwEvtDispatcher.h:83>

[S5]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/libs/publiclib/pageitems/basicinterfaces/CShape.cpp:192>

[S6]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/interfaces/text/IParagraphComposer.h:57>

[S7]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/sdksamples/singlelinecomposer/SLC.fr:93>

[S8]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/sdksamples/singlelinecomposer/SLCTextGlyphNode.cpp:481>

[S9]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/interfaces/text/IGlobalTextAdornment.h:68>

[S10]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/interfaces/text/ITextAdornment.h:44>

[S11]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/interfaces/text/IWaxRunShape.h:35>

[S12]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/interfaces/text/IWaxLineShape.h:55>

[S13]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/interfaces/architecture/IParcelShape.h:61>

[S14]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/interfaces/graphics/IDrawMgr.h:48>

[S15]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/interfaces/graphics/IGraphicsPort.h:542>

[S16]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/interfaces/text/IWaxRenderData.h:70>

[S17]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/interfaces/text/TextID.h:71>

[S18]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/includes/TextDrawInfo.h:52>

[S19]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/docs/html/classk_wax_text_run_boss.html>

[S20]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/interfaces/architecture/IObjectModel.h:165>

[S21]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/interfaces/architecture/IClassInfo.h:60>

[S22]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/includes/SchemaTypes.h:23>

[S23]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/interfaces/text/ITextUtils.h:1216>

[S24]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/includes/TextDrawPriority.h:32>

[S25]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/includes/DrawPassInfo.h:90>

[S26]: <../../../Adobe SDK/Adobe InDesign 2026 SDK/source/public/includes/DrawFilter.h:31>

[R1]: <../plugin/source/Ise.fr:66>

[R2]: <../plugin/source/IseFillRepaintAdornment.cpp:240>

[R3]: <../tools/jsx/probe-unstroked-victim.jsx>

[R4]: <../tests/audit/results/unstroked-victim.json>

[R5]: <../tests/audit/results/unstroked-victim.json>
