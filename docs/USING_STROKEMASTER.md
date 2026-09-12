# Using StrokeMaster

Takeover status, 2026-09-13: these are prototype workflows. Conflict handling and pair identity have been repaired and tested in InDesign 21.5.1.73, including IDML import and resync. Rendering and native-plugin support remain bounded by [the audit](../PROJECT_TAKEOVER_AUDIT.md). Keep an original copy when evaluating a new script revision.

Which correction to use depends on one question: **does the file have to look right somewhere other than this machine?**

| Your situation | Use | Why |
| --- | --- | --- |
| Only you open the file, and you want the fix to be invisible | The **plugin** | Corrects drawing with no change to the document. Nothing to maintain, and text stays one story. |
| Someone else opens the file, or you hand over an INDD or IDML | The **split** command, within its supported appearance | Uses ordinary document frames; recipients need no plugin to render them. Fonts, composition, and synchronization still matter. |
| You export PDF and nothing else | Either, within the tested envelope | Corrective paths are embedded in the tested exports; recipients do not need the plugin to render those paths. |
| You are converting type to outlines | The **outline** command | Native *Type > Create Outlines* loses the fill and halves the halo. |
| You have split frames and need to edit the text | **resync**, or **watch** | Splitting makes two stories; these keep them together. |

## Installing

**The plugin** (per machine, InDesign 2026 only): copy *InDesignStrokeEngine.pln* and its *(InDesignStrokeEngine Resources)* folder into *C:\Program Files\Adobe\Adobe InDesign 2026\Plug-Ins\InDesignStrokeEngine\*, then restart InDesign. Needs administrator rights. Remove it by deleting that folder and restarting.

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

- **The plugin is installed per machine.** The only recorded native target is InDesign 2026, build 21.5.1.73. Other builds require explicit ABI/load/edit/export validation; the version string alone does not prove their loader behavior. Its drawing correction is absent when the plugin is absent.
- **Split frames are ordinary document objects.** They travel without a plugin, but remain two independent stories. Unsupported frame paint/effects are refused. After sending the file, edits require another resync by someone running the script, or manual maintenance of both stories.
- **A split story lives in two frames.** The investigated linked-story configuration overwrote the child's formatting. That finite result does not rule out every native mapping or SDK route. The implemented **resync** and **watch** commands require the scripts to run.
- **resync needs the stroke to be uniform** across the bottom frame. A pair whose stroke varies within the story is reported and skipped rather than guessed at, because character positions recorded before an edit no longer mean anything after one.
- **The plugin changes antialiased fill edges** in the recorded raster tests. Visibility depends on scale and colors; a one-pixel result at one raster scale is not a general bound. Splitting avoids this particular second fill pass but introduces other appearance and synchronization risks.
- **Neither reaches text that is already outlined.** That damage is baked into the paths; convert with **outline** in the first place.
- **Cross-frame overlap is not corrected** by the plugin, which is scoped to one frame at a time.
- **Unstroked victim runs are also missed by the current plugin.** A later stroked line can still cover an earlier unstroked line. This is reproduced by the takeover regression; a supported split-frame reference corrects that example.
- **The native prefix is still a development placeholder.** Prefix allocation, signing, and Creative Cloud listing are separate steps described in [the distribution notes](PLUGIN_DISTRIBUTION.md); the current build is not a Marketplace release.

After replacing the script files, invoke *strokemaster.jsx* again to load the revised libraries. If watching is enabled, run **unwatch**, then **watch** to recreate its listener. Updating these JSX files does not require an InDesign restart. Loading a new native plugin binary does.

## If something looks wrong

- *Nothing happened.* Check that a text frame is selected, and that its stroke is Outside rather than Center.
- *The halo disappeared after converting to outlines.* That is the native command, not this one. Undo and use **outline**.
- *The two frames of a pair show different text.* Run **resync**, or **watch** to keep it from recurring.
- *A collaborator sees notches and you do not.* They do not have the plugin. Use **split**, which does not depend on it.
