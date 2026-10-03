# Using StrokeMaster

Status, 2026-10-03: the InDesign plugin corrects Outside text strokes as InDesign draws them, certified on InDesign 21.6 ([report](ROUTE_X_21_6_RECERTIFICATION_2026-10-03.md)) and confirmed by the owner on *cert-X.indd* and on a real document. The scripts below remain the document-level alternative. [The support matrix](../HOST_SUPPORT_MATRIX.md) records the older tested boundaries. Keep an original copy when evaluating a new revision.

Which correction to use depends on one question: **does the file have to look right somewhere other than this machine?**

| Your situation | Use | Why |
| --- | --- | --- |
| You work and export on a machine with the plugin | The **plugin** | Strokes are drawn beneath the fills of their story on screen and in every tested export (PDF in the foreground and background, PNG, EPS, PostScript), and interior miter spikes are dropped (measured in PDF and PNG); the text stays live and the document is never changed. |
| You export PDF and hand that over | The **plugin**, then inspect the PDF | The correction is written into the PDF as vector paths beneath live text; the PDF looks the same everywhere. |
| Someone else opens the INDD or IDML | The **split** command, within its supported appearance | Without the plugin InDesign draws the document natively, overlaps and spikes included. Split frames are ordinary document objects that need no plugin. |
| You are converting type to outlines | The **outline** command | Native *Type > Create Outlines* loses the fill and halves the halo. |
| You have split frames and need to edit the text | **resync**, or **watch** | Splitting makes two stories; these keep them together. |

## The plugin

**What it does.** For every text run with an Outside stroke, the plugin draws the stroke itself, in a pass before the frame's text, and InDesign then draws the run's fill on top. So a line's stroke no longer covers another line's fill: within a frame, across the columns of a frame, and across the threaded frames of one story. A run with a Miter join is stroked with bevel joins, and only the corners whose join points away from the letter get their miter back, with the run's own miter limit, so no miter tip can poke through a thin part of a letter. A Center stroke stays InDesign's own, in InDesign's order; only the parts of its miters that poke out of a letter are removed, and its miters inside the letter are kept. Nothing is written into the document; the same file opens and draws natively on a machine without the plugin.

**Where it stops, on purpose or for now:**

- **Separate stories are left as InDesign stacks them.** One story's stroke over another story's text looks the same as a deliberate halo, so it is not changed. A correction there would need a switch, and none exists yet.
- **Center strokes keep InDesign's drawing and order**; only their escaping miter tips are removed. Hiding the half of a Center stroke that lies over its letter would need a switch, and none exists yet.
- **Shapes and outlined text are left to InDesign**, spikes and beveled points included; taking over their strokes is planned.
- **Content drawn between two frames of a story** (an unrelated filled or stroked object stacked between them) keeps InDesign's order where it covers a letter; invisible objects, such as empty frames, do not.
- **A translucent upstream frame** is not corrected, and exporting a single page item draws the per-frame correction only.

**Measured hosts:** InDesign 21.5.1.73 and 21.6.0.57 on Windows. A plugin must be rebuilt for each major InDesign version.

**Installing.** Building needs the InDesign 2026 SDK and Visual Studio's MSVC 14.42 or 14.39 toolset:

```powershell
powershell -NoProfile -File plugin\build-plugin.ps1 -SdkPath "<InDesign 2026 SDK folder>" -RouteX
```

Then, with InDesign closed, from an elevated PowerShell (the *Plug-Ins* folder is writable by administrators only):

```powershell
powershell -NoProfile -File tools\install-indesign-plugin.ps1 -Variant Release-RouteX
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
