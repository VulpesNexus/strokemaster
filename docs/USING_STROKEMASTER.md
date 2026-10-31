# Using StrokeMaster

Before relying on either plugin, read [what to know](WHAT_TO_KNOW.md): one page on what each does to your files, and its limits.

Status, 2026-10-03: the InDesign plugin corrects Outside text strokes as InDesign draws them, certified on InDesign 21.6 ([report](ROUTE_X_21_6_RECERTIFICATION_2026-10-03.md)) and confirmed by the owner on *cert-X.indd* and on a real document. Since 2026-10-05 it also covers more page-item strokes (`e2c96147`, certified and installed with the StrokeMaster panel `5f4f5bab` on 2026-10-06; [census follow-up](COVERAGE_CENSUS_2026-10-04.md#follow-up-extended-coverage-2026-10-04-to-10-05)). The limits below describe that revision. The scripts below remain the document-level alternative. [The support matrix](../HOST_SUPPORT_MATRIX.md) records the older tested boundaries. Keep an original copy when evaluating a new revision.

Which correction to use depends on one question: **does the file have to look right somewhere other than this machine?**

| Your situation | Use | Why |
| --- | --- | --- |
| You work and export on a machine with the plugin | The **plugin** | Strokes are drawn beneath the fills of their story on screen and in every tested export (PDF in the foreground and background, PNG, EPS, PostScript), and interior miter spikes are dropped (measured in PDF and PNG); the text stays live and the document is never changed. |
| You export PDF and hand that over | The **plugin**, then inspect the PDF | The correction is written into the PDF as vector paths beneath live text; the PDF looks the same everywhere. |
| Someone else opens the INDD or IDML | The **split** command, within its supported appearance | Without the plugin InDesign draws the document natively, overlaps and spikes included. Split frames are ordinary document objects that need no plugin. |
| You are converting type to outlines | The **outline** command | Native *Type > Create Outlines* loses the fill and halves the halo. |
| You have split frames and need to edit the text | **resync**, or **watch** | Splitting makes two stories; these keep them together. |

## The plugin

**What it does.** For every text run with an Outside stroke, the plugin draws the stroke itself, in a pass before the frame's text, and InDesign then draws the run's fill on top. So a line's stroke no longer covers another line's fill: within a frame, across the columns of a frame, and across the threaded frames of one story. A run with a Miter join is stroked with bevel joins, and only the corners whose join points away from the letter get their miter back, with the run's own miter limit, so no miter tip can poke through a thin part of a letter. A Center stroke stays InDesign's own, in InDesign's order; only the parts of its miters that poke out of a letter are removed, and its miters inside the letter are kept. Shapes, frames, and outlined text with a plain mitered stroke (Center or Outside) get the same rule, and a corner sharper than the miter limit keeps its whole point instead of being flattened at its tip as InDesign does when it is a shape's own point: both edges straight, the shorter at least the stroke's reach long, and the point at most five times that edge (a triangle's apex, a thorn's tip). Every other corner past the limit, such as the tapered tips of script letters and the needles of decorative fonts, is beveled as InDesign bevels it. An Outside stroke is drawn around the shape's own outline, so outlined text keeps its fill. Where an outline's contours overlap, as *Create Outlines* leaves them for decorative fonts, an Outside stroke goes around the filled shape only: InDesign also strokes the contour edges that lie inside the letters, and its offset fill fails on such outlines. Overprint settings are kept. Nothing is written into the document; the same file opens and draws natively on a machine without the plugin.

**Where it stops, on purpose or for now:**

- **Separate stories are left as InDesign stacks them.** One story's stroke over another story's text looks the same as a deliberate halo, so it is not changed. A correction there would need a switch, and none exists yet.
- **Center strokes keep InDesign's drawing and order**; only their escaping miter tips are removed. Hiding the half of a Center stroke that lies over its letter would need a switch, and none exists yet.
- **Shape strokes** (closed paths) are taken over as follows:
  - Center with a Miter join. Where contours touch or overlap, its dashes, dots, and stripes follow one outline of the filled shape.
  - Outside with any join, including dashes, dots, and stripes. Patterns follow one outline around each word, so letters that touch share their pattern instead of crossing it. Dashes and dots keep InDesign's own pattern, gap color, and corner adjustment; the adjustment applies at the outline's real corners only, so outlined script stays dashed. Stripes, built-in and custom, are drawn as bands at their distance from the letters. Past the miter limit, a pattern's corner gets a solid whole point, without the pattern.
  - Inside, any join, on any outline, corner effects and frames with content included. The plugin fills the outline itself in place of InDesign's inward offset fill, which spills past curves. On overlapping contours, its dashes and stripes follow one outline inside the filled shape.
  - Also covered: gradient fills, corner effects (Outside), overlapping contours (an Outside or Inside stroke goes around the filled shape only; InDesign also strokes the contour edges inside it), overprint, opacity and blend modes (on the object, the stroke, or the fill), object-level effects, scale kept as a percentage, and shear. A stroke with its own opacity is drawn as one piece, so its overlapping parts do not show darker.

  These stay InDesign's own, and InDesign's drawing keeps its defects ([census](COVERAGE_CENSUS_2026-10-04.md)):
  - **An effect set on the stroke or the fill itself** (a drop shadow, a feather, a glow), not on the object. InDesign applies these around its own stroke and fill.
  - **Open paths** with Outside or Inside alignment, arrowheads, or dashes, and Center strokes with round or bevel joins (nothing to correct). Open paths with a Center Miter stroke are taken over: corners past the limit keep whole points, as on closed shapes.
- **Corner effects on outlined text** add thin loops and small zigzags to the path. The stroke follows them, as InDesign's own stroke does, but the corners the effect adds get no long points: past the miter limit they are beveled, as InDesign bevels them, and within it their miters are trimmed to the size of the zigzag.
- **Vertical text stays InDesign's.** Its strokes are drawn as without the plugin; its outlines are covered as any shape.
- **Whole points under object opacity** keep within the item's bounds in PDF, where InDesign's transparency group would cut them flat; they are shortened along their own axis.
- **Outlined decorative fonts make large PDFs.** Every corner's miter is a small filled shape in the PDF, and the outside of an overlapping outline is a clip as detailed as the outline itself. One line of outlined Kerberos Fang at 48 pt, stroked both ways, took the test page from 4.3 MB to 12.4 MB.
- **Content drawn between two frames of a story** (an unrelated filled or stroked object stacked between them) keeps InDesign's order where it covers a letter; invisible objects, such as empty frames, do not.
- **A translucent upstream frame** is not corrected, and exporting a single page item draws the per-frame correction only.

**The StrokeMaster panel.** *Window > StrokeMaster* opens a panel with one checkbox per correction; a click redraws every open document at once, so a correction can be compared on and off in place. Hovering a checkbox shows what it does when on and when off. Without the engine installed, every switch is grayed out and the panel says *Engine not installed*. The panel menu holds *Restore Defaults* and *About StrokeMaster*, which shows the version, a link to this repository, and the license. Collapsed to an icon, the panel shows a crown, light or dark to suit InDesign's interface brightness. The switches apply to every document, to the screen and to every export, and are kept between sessions (in *%APPDATA%\StrokeMaster\settings.txt*). *Restore Defaults* (the button, or the panel menu) puts them all back. A switch whose parent is off is shown grayed out, its own state kept. The panel is under *Window*, not *Plug-Ins*. An edit to the settings file by hand applies to the next export at once, but the screen keeps each item's last rendering until the panel redraws or a new window opens.

| Switch | On (default unless noted) | Off |
| --- | --- | --- |
| *StrokeMaster* | Every correction below | InDesign draws everything itself |
| *Strokes behind other letters' fills* | Text strokes drawn beneath the fills of their frame | InDesign's order |
| *Across a story's threaded frames* | Also across the frames and columns of one story | Within each frame only |
| *Across separate stories* | Also across separate stories that overlap; off by default, because it also removes a deliberate halo over another frame | Separate stories keep InDesign's stacking |
| *No miter spikes* | Miters at live text's inner corners that would poke out through a letter's thin parts removed | InDesign's miters |
| *Draw their strokes* | The plugin draws the strokes of shapes, frames, and outlined text | InDesign does, with all the defects below |
| *Whole points past the miter limit* | A shape's own sharp points kept whole | Beveled, as InDesign bevels them |
| *Also on dashed and striped strokes* | Dashes and dots: a solid whole point beyond InDesign's patterned bevel. Stripes along one outline per word: each stripe follows the point | The patterned bevel |
| *Trim glyph tips* | Sharp glyph tips (beside a curve or a short edge) cut flush with the stroke's edge, within the miter limit, and past it on outside and center strokes | Whole miters within the limit, spikes included; InDesign's bevel past it |
| *Trim corner-effect corners* | Corners a corner effect adds beveled past the limit and trimmed within it | Treated like the outline's own corners |
| *One outline per word (dashes, stripes)* | Outside patterns follow one outline around touching letters, and keep their pattern in the letters' counters: dashes, dots, and hashes laid between its real corners (dashes with the notches between letters filled); Wavy laid as whole waves round each outline, lower round small counters and tight bends; White Diamond drawn by InDesign's own stroker along it, with InDesign's corner diamonds; built-in stripes, with or without a gap color, filled as regions by distance from the letters; custom stripes, measured from their own stroker, filled as built-in stripes are | InDesign's pattern on each contour, clipped outside the fill |
| *Overlapping contours get one outline* | Outside and Inside strokes go around the filled shape | Such outlines are left to InDesign |
| *Inside strokes*, *Open paths* | Drawn by the plugin | Left to InDesign |
| *Objects with opacity or effects* | Opacity and blend modes (object, stroke, fill) and object-level effects covered | Such objects are left to InDesign |

A showcase of what each correction changes, and of the forms handled with the least certainty, is built by *tools/jsx/build-showcase.jsx* and set side by side by *tools/compose_showcase.py*.

**Measured hosts:** InDesign 21.5.1.73 and 21.6.0.57 on Windows. A plugin must be rebuilt for each major InDesign version.

**Installing.** Building needs the InDesign 2026 SDK and Visual Studio's MSVC 14.42 or 14.39 toolset:

```powershell
powershell -NoProfile -File plugin\build-plugin.ps1 -SdkPath "<InDesign 2026 SDK folder>" -RouteX
powershell -NoProfile -File plugin\build-plugin.ps1 -SdkPath "<InDesign 2026 SDK folder>" -UI
```

The first builds the engine; the second the StrokeMaster panel, a separate plugin, since the engine is a model plugin (so that background PDF export runs it) and a model plugin carries no interface.

Then, with InDesign closed, from an elevated PowerShell (the *Plug-Ins* folder is writable by administrators only):

```powershell
powershell -NoProfile -File tools\install-indesign-plugin.ps1 -Variant Release-RouteX
powershell -NoProfile -File tools\install-indesign-plugin.ps1 -Variant Release-UI -Name StrokeMasterUI
```

It backs up the installed plugin first, verifies the copy by hash, and prints the backup folder; `-Restore <that folder>` puts the previous plugin back, and `-ValidateOnly` with either form shows the plan without writing anything. Start InDesign to load the new plugin. The ID prefix is a development placeholder, so this build is for your own machine, not for distribution ([distribution notes](PLUGIN_DISTRIBUTION.md)).

Native development details are in [the load report](../ILLUSTRATOR_NATIVE_LOAD_REPORT.md), [the InDesign root-cause report](../UNSTROKED_VICTIM_ROOT_CAUSE.md), and [the cross-frame research](../CROSS_FRAME_RESEARCH_2026-09-26.md). Illustrator's plugin is a separate diagnostic, not a release.

## Installing the scripts

**The scripts**: copy the whole *tools/jsx* folder into InDesign's *Scripts Panel* folder (*Window > Utilities > Scripts*, right-click a folder, *Reveal in Explorer*). Then double-click *strokemaster.jsx* in the panel and pick a command.

Headless, for scripted pipelines:

The headless runner is *tools/run-jsx.ps1*. Until its revised commands have been host-tested, use the existing Scripts panel entry only on disposable copies. Do not copy the earlier backslash-escaped PowerShell JSON example; it was not portable shell quoting.

There is no need to pass `-Engine`. Measured: a headless run reports its engine as `main`, so the `#targetengine` line in the script is ignored on that path, because the directive applies only to the script InDesign compiles itself and *run-jsx.ps1* reaches the file through `$.evalFile`. The watcher survives there regardless, because `main` is persistent: a watch started in one invocation reported `watching=true` from the next. Running the script from the Scripts panel does honor the directive, giving it an engine of its own.

## The commands

**split** — select one or more stroked text frames and run it. Each becomes two frames in the same place: a bottom frame keeping the stroke, with the stroke color as its fill, and a top frame keeping the fill, with no stroke. Every stroke is then painted before every fill, which is the whole correction.

It assigns paint per text style range, including varied strokes. Only Outside strokes move underneath; Center-stroked ranges retain their original treatment on top. It refuses threaded, locked, grouped, or unsupported frames, including frame backgrounds, borders, transparency, and effects. These refusals prevent duplication from silently changing the frame's appearance. Each pair receives a persistent token; frames are never matched merely because they share a position.

**outline** — select text frames and run it instead of *Type > Create Outlines*. It stages conversion on duplicates, validates the resulting paths and stacking, then replaces the original. An error before replacement preserves the original and attempts to remove all temporary art; any incomplete cleanup is explicitly reported. The host transaction test covers a page-two frame, failure after staging, and successful replacement. Outlined text loses text editability by definition.

This conservative converter accepts uniform solid text paint in an unthreaded, non-overset, untransformed frame directly on a page/spread. It refuses mixed paint, frame backgrounds or borders, effects, decorations, text wrap, anchored objects, tables, footnotes, and automatic lists. An unsupported frame stays untouched; it is not silently handed to the native command. Paper/None have inapplicable page-item overprint controls in the tested host; only a false source overprint state is accepted for those swatches. Broader rendering fidelity, undo, and output routes still need regression coverage.

**resync** — after editing the top frame, rebuild the bottom from it while retaining its uniform Outside stroke. Explicit resync also propagates formatting-only changes. Pair discovery includes all document pages. The measured page-two pairs survived IDML import and could be edited and resynchronized afterward.

**watch** / **unwatch** — poll for contents changes, including a mismatch already present on the first tick. Automatic watching does not detect formatting-only changes; use **resync** for those. Watching requires the script running in the current InDesign session. It has no save/export event backstop, so explicitly resync and inspect the pair before delivery or export. Direct tick behavior is host-tested; precise idle-event timing is not certified by the new tests.

Duplicating a labeled pair within the same document creates ambiguous tokens. The script reports and skips those frames rather than choosing a mate. Older pair labels are also refused. Preserve the old pair and create a fresh split from an original stroked frame to obtain current labels; do not simply relabel arbitrary superposed frames. IDML retains the new labels in the measured round-trip.

## What to know before relying on it

- **The plugin's correction exists only where the plugin runs.** A recipient without it sees InDesign's own drawing. Hand over a PDF exported with the plugin, or use **split**.
- **Native compatibility requires per-host validation.** The plugin is measured on InDesign 21.5.1.73 and 21.6.0.57; other builds need load, edit, and export checks.
- **Split frames are ordinary document objects.** They travel without a plugin, but remain two independent stories. Unsupported frame paint/effects are refused. After sending the file, edits require another resync by someone running the script, or manual maintenance of both stories.
- **A split story lives in two frames.** The investigated linked-story configuration overwrote the child's formatting. That finite result does not rule out every native mapping or SDK route. The implemented **resync** and **watch** commands require the scripts to run.
- **resync needs the stroke to be uniform** across the bottom frame. A pair whose stroke varies within the story is reported and skipped rather than guessed at, because character positions recorded before an edit no longer mean anything after one.
- **The plugin's stroke edges are drawn as paths, not as text strokes.** They match InDesign's geometry exactly, but anti-aliased edge pixels can differ from a native stroke's, within two device pixels of an edge.
- **Nothing reaches text that is already outlined.** That damage is baked into the paths; convert with **outline** in the first place.
- **The native prefix is still a development placeholder.** Prefix allocation, signing, and Creative Cloud listing are separate steps described in [the distribution notes](PLUGIN_DISTRIBUTION.md); the current build is not a Marketplace release.

After replacing the script files, invoke *strokemaster.jsx* again to load the revised libraries. If watching is enabled, run **unwatch**, then **watch** to recreate its listener. Updating these JSX files does not require an InDesign restart. Loading a new native plugin binary does.

## If something looks wrong

- *The plugin changed nothing.* Check that the stroke is Outside rather than Center, and that InDesign was restarted after installing. Overlap between two separate stories is left alone on purpose.
- *A stroke still covers letters in another frame of the same story.* Something drawn between the frames (a filled or stroked object stacked between them) covers those letters; InDesign's order is kept there.
- *Nothing happened.* For the scripts, check that a text frame is selected, and that its stroke is Outside rather than Center.
- *The halo disappeared after converting to outlines.* That is the native command, not this one. Undo and use **outline**.
- *The two frames of a pair show different text.* Run **resync**, or **watch** to keep it from recurring.
