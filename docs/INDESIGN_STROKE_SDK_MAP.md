# InDesign stroke SDK map

Phase D of the InDesign stroke engine investigation: the InDesign SDK surfaces that matter for text strokes, arranged along the pipeline from story to output, and the points where a plugin could change what reaches the page.

The map cites public SDK headers by file name and line so each claim can be checked. It reproduces no SDK code or documentation. "Verified" means the cited lines were read during this investigation; "SDK census" means the item comes from a broad read-only survey of the SDK and was not re-read line by line.

Takeover qualification, 2026-09-12: reading an interface proves that the declaration exists, not that a proposed intervention works in the host. Source-only architecture claims remain proposals until a controlled host test. The sampled transparency-group observations do not establish universal repaint idempotence; the original glyph-core measurement mask was invalid for that purpose. See [the takeover audit](../PROJECT_TAKEOVER_AUDIT.md).

## Version and tooling

| Item | Value | Source |
| --- | --- | --- |
| SDK version | 21.5.1, build 73 | *BuildNumber.h*, line 33; *CC21_5BuildNumber.h*, lines 75 and 119 (verified) |
| Installed InDesign | 21.5.1.73 | `app.version`; *InDesign.exe* file version |
| Match | Exact | — |
| Solutions | *build/win/prj/SDKSamples.sln*; one *.sdk.vcxproj* per sample with `Debug\|x64` and `Release\|x64` configurations | SDK census; sample project read |
| Toolset the SDK targets | Visual Studio 2022 (`v143`), Windows SDK 10.0.26100 | SDK census |
| Toolset available here | Visual Studio 2022 Community 17.7 with MSVC 14.37 (`v143`); Windows SDK 10.0.22621 | *vswhere* and file system |
| Resource tooling | *devtools/bin* (*Odfrc.exe*, *merge_res.cmd*, *restore_res.cmd*); *devtools/dolly* project generator | SDK census |

### Building against this SDK

Verified on 2026-09-12, first by building the *basictextadornment* sample and then this repository's own plugin, to a linked *.pln* installed under *Plug-Ins* and confirmed loaded: it appears among the running application's mapped modules, alongside the 235 plugins InDesign loads for itself.

- **Compiler.** The SDK's precompiled-header shim accepts only `_MSC_VER` 1938–1939 (Visual Studio 2022 17.9) and 1942 (17.12). Visual Studio 17.14 ships MSVC 14.44, which reports 1944 and stops the build with an explicit error. Installing the MSVC v14.42 (17.12) toolset side by side and passing `/p:VCToolsVersion=14.42.34433` builds cleanly. The Windows SDK the projects target, 10.0.26100, comes with 17.14.
- **Paths.** Build through a path without non-ASCII characters. The sample projects hardcode `ID_SDK_DIR` to `..\..\..`, which only resolves while the linker's working directory is the project folder. Overriding the output directory breaks it, so pass `/p:ID_SDK_DIR=<absolute SDK path>`; every SDK and Boost library property derives from it.
- **Writes outside the output directory.** Even with `OutDir` redirected, a sample project writes its import library to `$(LIB_DIR)` and its PDB to a hardcoded relative path, both inside the SDK tree. A plugin project of our own must redirect `ImportLibrary` and `ProgramDatabaseFile` as well, or the SDK folder collects build artifacts.
- **Build paths have to be stable, not just ASCII.** *Odfrc.exe* records the absolute path of the *.fr* file it compiled into a file called *frFilePath* in the intermediate directory, and refuses to run when a later build offers the same resource under a different path, reporting that ODFRC is being called twice for different *.fr* files. Building through a junction with a randomly generated name therefore succeeds once and fails on every run after it. Deriving the junction name from its target fixes this, and keeps the precompiled header and the object files valid between builds as well.
- **The resource tools need `PATH`, and *merge_res.cmd* needs `call`.** *merge_res.cmd* invokes *cmpmodtime.exe*, *ConcatRes.exe*, and *atouch.exe* by bare name, so *devtools/bin* must be on `PATH` or each one fails with `is not recognized`. It is also a batch file: a pre-link step that starts it without `call` never regains control, so every command chained after it on the same line is skipped in silence and the build still reports success.
- **Where the resources actually go.** The *.fres* file stays empty and *merge_res.cmd* reports that ConcatRes was skipped, which is normal rather than a fault. odfrc writes each compiled resource as *idrc_&lt;type&gt;/&lt;id&gt;.idrc* under the intermediate directory, and InDesign reads them from a folder named *(&lt;plugin&gt; Resources)* beside the plugin, the same layout the installed application's own plugins use. Copy that tree and nothing else: MSBuild keeps its *.tlog* directory in the same intermediate directory, where a recursive copy sweeps it into the installed plugin, so point `TLogLocation` somewhere outside `IntDir`.
- **Extension and install location.** A third-party plugin is a *.pln* installed under *Plug-Ins*, while the shipping application's own plugins are *.apln*. The sample projects and the *dolly* project generator both emit *.pln*, and Adobe's extension-installer manifest for the *LayoutHelper* sample copies *IDHostAdapter.pln* into `$indesign/Plug-Ins`. That folder needs administrator rights to write, so installing into a subfolder of its own keeps removal to a single deletion.

## The pipeline, stage by stage

| # | Stage | Public surface | What a plugin can do there |
| --- | --- | --- | --- |
| 1 | Character attributes | Text attribute bosses for stroke weight, stroke color and tint, stroke overprint, miter limit, join, and stroke alignment. A report implementation hands each value to composition through `IAttrReport::TellComposition`, which calls `IDrawingStyle::SetMiterLimit` (*IDrawingStyle.h*, line 423) and `SetStrokeAlignment` (line 432). | Read or set attributes; add custom attributes. The stock attributes' behavior can't be replaced. |
| 2 | Composition and run segmentation | `IDrawingStyle::CanShareWaxRunWith` (*IDrawingStyle.h*, line 526) decides whether adjacent characters can share a wax run. The implementation is private. | Nothing public changes where runs split. A custom paragraph composer still receives one drawing style per run. |
| 3 | Wax (composed result) | `IWaxRenderData` holds each run's drawing parameters: `RenderMethod` enumerates none, stroke only, fill only, and fill and stroke (*IWaxRenderData.h*, line 46); `SetMiterLimit` (line 87); `SetStrokeAlignment` (line 90). *IWaxRun.h* declares a constant text miter limit of 4.0 (line 35). | Read everything needed to redraw a run: glyphs, matrices, font, colors, stroke settings. |
| 4 | Drawing a frame's text | `IWaxRunText::Draw` takes the graphics data, shape flags, and a draw pass (*IWaxRunText.h*, line 52). The shared base class first checks for a Create Outlines port, then calls every adornment registered for the current pass, and only in the text pass calls the run's own protected text drawing (*CWaxRunText.h*, lines 43–73). Passes are frame-wide: for each distinct pass used by the frame's active adornments, every wax run in the frame is visited (*TextDrawPriority.h*, lines 36–42). The run text implementation is private. | Draw before or after the frame's text in dedicated passes, or bracket each run's native text drawing with adornments just before and just after it in the text pass. |
| 5 | Adornments | `IGlobalTextAdornment`: `Draw` is called per run (*IGlobalTextAdornment.h*, lines 147–150), with `StartOfParcelDraw` and `EndOfParcelDraw` around each pass (lines 156–164); registered through `kGlobalTextAdornmentService` (*TextID.h*, line 71). `ITextAdornment` is the per-run adornment attached through an attribute (SDK census). `IAdornmentShape` is the page-item adornment (SDK census). | The main public hook for A and C. |
| 6 | Graphics port | `IGraphicsPort::setlinejoin` (*IGraphicsPort.h*, line 419), `setmiterlimit` (line 430), text drawing flags for fill and stroke (lines 545–546), `xshow` overloads taking those flags (lines 554 and 561), and `strokepath_clip` (line 615). The port has no text rendering mode and no charpath call (SDK census). | Draw glyphs filled or stroked, or draw arbitrary paths, with explicit join and miter settings. |
| 7 | Path engines | `IPathFinder` with union, intersection, difference, and exclusive-or (*IPathFinder.h*, lines 64 and 92); `IOffsetPath::OffsetPath` with join type and miter limit (*IOffsetPath.h*, line 69); glyph outlines through `IFontInstance::GetOutlineInfo` into path or port callbacks (SDK census). | Build unioned or offset stroke geometry, including a miter that is clipped at a limit instead of beveled. |
| 8 | Create Outlines | Command bosses for text and frames (*charpathid.h*, lines 57–58) and a dedicated capturing port implementation (line 89). `ICreateOutlinesUtils::IsCreateOutlinesPort` (*ICreateOutlinesUtils.h*, line 57) and `CreateCOTextCmd` (line 67). | Detect the conversion port while drawing; act before or after the conversion commands. |
| 9 | Draw events | Frame-level text messages `kBeforeFrameText` and `kAfterFrameForeground` (*DocumentContextID.h*, lines 110 and 112); page-item draw events dispatched from the shape base class (SDK census). No per-line or per-run draw event exists. | Skip or wrap a whole frame's drawing. |
| 10 | PDF export | `IPDFExportSetupProvider::PDFProcessEvent` (*IPDFExportSetupProvider.h*, line 55) and `kAfterPDFExportSignalResponderService` (*PDFID.h*, line 1497). | Adjust or post-process exported PDF. |

### Registering a global text adornment

The SDK ships no sample that registers an `IGlobalTextAdornment`: *basictextadornment* and *hiddentext* both include *IGlobalTextAdornment.h* only for its draw-priority constants, and implement `ITextAdornment`, which is attached through a text attribute instead. The pattern below was recovered from the installed application, where 19 classes across the *Text*, *UI*, *XMedia*, and *Required* plugins expose the interface. Every one of them has the same shape:

```
Class
{
    <your boss>,
    kInvalidClass,
    {
        IID_IK2SERVICEPROVIDER,    kGlobalTextAdornmentServiceImpl,   // API-provided
        IID_IGLOBALTEXTADORNMENT,  <your implementation>,
    }
},
```

Two details are worth recording because they are easy to get wrong from the headers alone. The parent is `kInvalidClass`, not a service-provider base boss. And a `kUIPlugIn` may host one: one of the nineteen lives in the *UI* plugin, so `kModelPlugIn` is not required, and in the samples it marks persistence and server plugins rather than anything to do with load order.

A compiled `ClassDescriptionTable` can be read directly, which is the quickest way to confirm what odfrc emitted. The layout is a class count, then per class a two-byte marker, the class ID, the parent ID, an interface count, and that many interface and implementation ID pairs, all little-endian 32-bit:

```
count=1  marker=0001  class=0x0005A200  parent=0x00000000  interfaces=2
   0x00000124 -> 0x0000CA00     IID_IK2SERVICEPROVIDER    -> kGlobalTextAdornmentServiceImpl
   0x00000279 -> 0x0005A200     IID_IGLOBALTEXTADORNMENT  -> our implementation
```

`0x279` is `kTextPrefix + 121` (*TextID.h*, line 832) with `kTextPrefix` = `0x200`, and `0x0000CA00` is `kTextPrefix2 + 0` (line 1424).

**A class description table is not enough on its own.** The plugin also needs a `FactoryList` resource, or InDesign reads the class table, knows the boss and its interfaces, and still has no way to manufacture the implementation. The symptom is quiet and misleading: the library loads and appears among the process's mapped modules, and nothing else happens at all. Of the 262 plugins installed with InDesign, 260 carry one.

```
resource FactoryList (kSDKDefFactoryListResourceID)
{
    kImplementationIDSpace,
    {
#include "<your>FactoryList.h"
    }
};
```

That header is read twice, which is the part worth remembering: `REGISTER_PMINTERFACE` expands to `BUILD_PMINTERFACE(id##_)` for the C++ compiler and to the bare implementation ID for odfrc (*InterfaceFactory.h*, lines 292 to 298). Including it only from the dead-strip file satisfies the linker and leaves the resource side empty. The compiled result is small enough to check by eye: `idrc_FACT/550.idrc` holds `0x00000103` (the implementation ID space), a count, and one ID per entry.

Proven by control on 2026-09-12: the same sources and the same class table, with and without this resource, gave no trace output at all versus 21 boss constructions and 67 `Draw` calls on the same fixture.

**Reaching the document from a wax run.** An adornment that wants the run's own colors has to resolve `IWaxRenderData::GetColorUID`, which returns a bare `UID` and therefore needs a database to resolve against. Wax is not a persistent UID-based object, so asking a wax run for `IPMPersist` returns nil, and the free `GetDataBase` helper fails in exactly the same way, because *PersistUtils.cpp* implements it as that same persistence query. `IWaxRun::GetDataBase` (*IWaxRun.h*, line 168) returns the database directly. The failure mode is quiet and easy to misread: the adornment registers, `Draw` is called for every run, and it simply cannot install a fill, so nothing is drawn and nothing is reported.

**Two positions, not one.** Glyph geometry from `IWaxGlyphs` is relative to the start of its own wax run (*IWaxGlyphs.h*, lines 39 to 42), and the run is placed relative to its wax line by `IWaxRun::GetXPosition` and `GetYPosition` (*IWaxRun.h*, lines 98 to 114). Drawing a run's glyphs therefore needs both, and an adornment that uses only `GetGlyphDrawPosition` paints every run as though it began at the start of its line. Measured on the fixture: of 200 runs, 145 sat at a non-zero position on their line, and every one of them reported its first glyph at x = 0. The failure is quiet again, and worse than a blank: the first run of each line is correct, so output looks broadly right while later runs are painted over their neighbors. It also hides in simple tests, because a line with one run has an offset of zero.

**Installing the run's rendering object also reaches gradients in the sampled exports.** `IRenderingObject::Install` with `kInstallFill` requests the run's fill state. A gradient may reach PDF as clipping and shading, so counting only `f` and `S` misses that contribution. The historical sample showed an additional `sh` per gradient-filled run. Placement inside the sampled transparency groups does not establish correct compositing for all blend modes, opacity, overlapping colors, or decorations. The previous claim that these need no handling is withdrawn pending independent fill-only masks and single-pass controls (*STROKE_MECHANISMS.md*, M8).

## Where each defect becomes inevitable

The mechanisms are established in *STROKE_MECHANISMS.md*; this section only places them on the pipeline.

**A, adjacent-line overlap, and C, run fragmentation.** Composition geometry is correct, so neither defect exists before stage 4. It becomes inevitable inside the private run text drawing, which paints each run's doubled Outside stroke and then that run's fill before the next run is visited. Nothing in stages 1–3 exposes an order. The earliest public point that can change the outcome is stage 5.

**B, miter spikes.** The spike geometry follows from the miter limit that stage 1 carries to stage 4, and the PDF miter rule applied to the doubled width. The attribute is public and it works (see *STROKE_MECHANISMS.md*). A different join rule, such as clipping a long miter at the limit instead of beveling it, doesn't exist in PDF or PostScript and needs stage 7 geometry.

**D, Create Outlines.** The conversion runs the wax through the capturing port in stage 8. The path's stroke alignment, width, and paint order are decided there, privately.

## Interception candidates

| Id | Mechanism | Stages | Defects | Output paths it reaches | Main risk |
| --- | --- | --- | --- | --- | --- |
| I1 | Global text adornment in the foreground pass that repaints each stroked run's fill after the whole frame's text | 5, 6 | A, C | Screen, print, PDF (all three measured, EPS and a PostScript print included) | Fills are painted twice. Measured: transparency, blend modes, gradients, and overprint are safe, because the repaint lands inside the same transparency group, and glyph outlines avoid duplicate searchable text. What remains is a one-pixel antialiased ring along every repainted fill, and the added path geometry in the file |
| I2 | Global text adornment in the background pass that paints every run's stroke first, plus suppression of the native stroke while each run draws its text | 4, 5, 6 | A, C, and B if the adornment strokes stage 7 geometry | Screen, print, PDF | Suppression relies on port substitution or transient render-data changes around the native text draw, and that timing isn't documented |
| I3 | Draw event handler that skips a text frame and redraws it: all strokes, then all fills | 4, 6, 9 | A, B, C | Screen, print, PDF | Reimplements run drawing, including colors, gradients, overprint, tagging, and underline adornments |
| I4 | PDF post-processing that reorders each frame's text objects | 10 | A, C | PDF only | Export only; frame identity has to be recovered from the document |
| I5 | Responder after the Create Outlines commands that rebuilds the paths as a stroke copy under a fill copy | 8 | D | All, since the result is document content | Needs the text's stroke alignment from before the conversion |

## Samples to start from

| Sample | What it provides |
| --- | --- |
| *basictextadornment* | A custom text attribute that attaches an adornment and draws behind each run (SDK census) |
| *spellpanel* (open source) | The one public `IGlobalTextAdornment` implementation and its service registration (SDK census) |
| *basicdrwevthandler*, *printselection* | Draw event registration and telling screen, print, and PDF apart (SDK census) |
| *framelabel* | Drawing text through a font instance from an adornment (SDK census) |
| *pdfvt* | A PDF export setup provider (SDK census) |
| *codesnippets* | Wax iteration, path geometry, and glyph lookup examples (SDK census) |

## Not in the SDK

- The run text drawing implementation, so the exact port calls for fill and stroke and where the miter limit is set.
- Any public way to replace `IWaxRunText` on the text run boss or to override run sharing.
- Per-line or per-run draw events, and any draw event that separates text fill from text stroke.
- A text rendering mode or charpath call on the port.
- The dispatch site for the frame-level text draw messages.
- A usable IDML schema (the bundled archive is empty).
