# StrokeMaster

Corrections for the way InDesign paints text strokes, and the investigation that established why they are needed. There is a native plugin that corrects the drawing, scripts that build a correction into the document itself, and a measured record of what InDesign actually does.

For InDesign 2026 (21.5.1) on Windows.

## What it corrects

| Defect | What you see | Status |
| --- | --- | --- |
| Adjacent-line overlap | A wide Outside stroke on one line covers the descenders of the line above | Root cause proven; corrected by the plugin and by the split |
| Run fragmentation | Formatting one character leaves notches where the next run's stroke covers the previous run's glyphs | Same root cause, corrected by the same fix |
| Miter spikes | Long spikes shoot from glyph corners on a Miter join | Root cause proven; no fix implemented, because the target behavior is a design decision |
| Create Outlines | Converting stroked type loses the fill and halves the halo | Corrected by the `outline` command |

The first two are one defect. For Outside alignment InDesign paints, per wax run, a centered stroke of twice the weight and then that same run's fill, and runs are painted in story order, so a later run's stroke lands on an earlier run's fill.

## Choosing a correction

The question that decides it is whether the file has to look right anywhere other than your own machine. *docs/USING_STROKEMASTER.md* has the full guide; in short:

- **Only you open it** and you want the fix invisible: use the **plugin**. It changes the drawing and never the document.
- **Anyone else opens it**, or it goes to a printer, or you hand over an INDD or IDML: use the **split** command. The correction is built out of real frames, so it survives with nothing installed.
- **You export PDF and nothing else**: either. A PDF exported with the plugin loaded is already correct everywhere.

## Quick start

**Scripts.** Copy the *tools/jsx* folder into InDesign's *Scripts Panel* folder (*Window > Utilities > Scripts*, right-click a folder, *Reveal in Explorer*), then double-click *strokemaster.jsx* and choose a command: split, outline, resync, watch, unwatch, or status.

**Plugin.** Copy *InDesignStrokeEngine.pln* and its *(InDesignStrokeEngine Resources)* folder into *Adobe InDesign 2026\Plug-Ins\InDesignStrokeEngine\* and restart InDesign. Administrator rights are needed; removing it is deleting that folder.

**Before distributing the plugin to anyone else, read the warning below.**

## Requirements

- Adobe InDesign 2026 (21.5.1) running, on Windows
- Python 3 with the packages in *requirements.txt*, for the measurement tools: *python -m pip install -r requirements.txt*
- To build the plugin: Visual Studio 2022 with MSVC v14.42 or v14.39, and the Adobe InDesign 2026 SDK. Build with *plugin\build-plugin.ps1 -SdkPath "\<sdk\>"*.

## A warning about the plugin's ID prefix

The plugin uses prefix `0x5A200`, which is a development placeholder and **was not assigned by Adobe**. Adobe allocates a 24-bit prefix on request, and its own documentation is explicit that two plugins sharing a prefix cannot be loaded at the same time. InDesign has no registry and no validation, so a collision only ever appears on someone else's machine. Change `kIsePrefixNumber` in *plugin/source/IseID.h* before distributing anything. It is a one-line change today because the plugin stores no data in documents.

## Build the reproducers

Each build script creates its own document without touching open ones. It saves the document under *tests/indesign-stroke/\<issue\>/minimal/*, exports PDF, PNG, and IDML under *…/actual/*, closes it, and restores InDesign's export preferences.

| Command | Builds |
| --- | --- |
| *powershell -NoProfile -File tools\run-jsx.ps1 build-A-minimal.jsx* | Adjacent-line overlap variations |
| *powershell -NoProfile -File tools\run-jsx.ps1 build-B-miter.jsx* | Miter-limit, weight, and wedge-angle variations |
| *powershell -NoProfile -File tools\run-jsx.ps1 build-C-attribute-sweep.jsx* | One frame per character attribute, to see which ones split a run |
| *powershell -NoProfile -File tools\run-jsx.ps1 build-D-outlines.jsx* | Native *Type > Create Outlines* results |
| *powershell -NoProfile -File tools\run-jsx.ps1 build-D-faithful.jsx -With ise-faithful-outlines.jsx* | Live text next to faithful outline conversions |
| *powershell -NoProfile -File tools\run-jsx.ps1 build-N-transparency.jsx* | Blend modes, opacity, gradients, and overprint, to test whether the plugin harms them |
| *powershell -NoProfile -File tools\run-jsx.ps1 build-S-split.jsx -With ise-split-stroked-text.jsx* | Split pairs beside unsplit controls |
| *powershell -NoProfile -File tools\run-jsx.ps1 build-T-livesync.jsx -With ise-split-stroked-text.jsx,ise-live-sync.jsx* | A split pair, edited and resynced |

## Measure

- *python tools/stroke_probe.py tests/indesign-stroke/\<issue\>/actual \<stem\>* lists the paint objects InDesign exported for every frame and writes the measurements the manifest asks for into *\<stem\>.probe.md*.
- *python tools/compare_renders.py \<a.png\> \<b.png\> \<manifest.json\> \<out-stem\>* compares two renders of one document frame by frame, separating a repaint's antialiased edge from a real change of color.
- *python tools/stroke_outline.py render \<paths.json\> \<out.png\>* builds a stroke's outline from a path under each join rule, and *validate* compares a plain miter against Adobe's own expansion of the same stroke.
- *python tools/miter_geometry.py \<out-dir\>* checks the join formulas against fixture B's measured wedges and draws the rules side by side. Both tools take the rule itself from *tools/miter_rule.py*, which is the only place it is defined.
- *powershell -NoProfile -File tools\run-ai-jsx.ps1 ai-dump-paths.jsx -ArgJson "{\\"out\\":\\"C:/tmp/p.json\\"}"* writes Illustrator path geometry to JSON for the two tools above. Illustrator is a separate application from InDesign, so it has its own runner.
- *python tools/make_expected.py tests/indesign-stroke/\<issue\> \<stem\>* writes the expected output: the same PDF with each frame's Outside strokes painted before its fills.
- *python tools/scan_identifiable.py \<path\>* checks files for this machine's user name, computer name, and home directory before they are added to the repository.

The per-frame crops and the edited PDFs these tools write are not committed; they are regenerated from the PDFs that are.

## Documents

| Document | Contents |
| --- | --- |
| *docs/USING_STROKEMASTER.md* | Which correction to use, how to install it, and what to know before relying on it |
| *docs/STROKE_FIXTURE_CENSUS.md* | Inventory of the supplied fixture and where each defect appears |
| *docs/INDESIGN_STROKE_SDK_MAP.md* | InDesign SDK surfaces for text strokes and where a plugin could act |
| *docs/STROKE_MECHANISMS.md* | What produces each defect, with controls |
| *docs/INDESIGN_STROKE_INVESTIGATION.md* | Per-defect report: root cause, layer, fixes, risks, and verdicts |
| *docs/LINKED_TEXT_UTILITY.md* | Notes toward a separate tool for mirroring one text into several frames |

Each fixture under *tests/indesign-stroke/* carries its own *NOTES.md* with the method and the numbers.
