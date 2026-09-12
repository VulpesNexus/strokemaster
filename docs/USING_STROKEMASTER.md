# Using StrokeMaster

Which correction to use depends on one question: **does the file have to look right somewhere other than this machine?**

| Your situation | Use | Why |
| --- | --- | --- |
| Only you open the file, and you want the fix to be invisible | The **plugin** | Corrects drawing with no change to the document. Nothing to maintain, and text stays one story. |
| Someone else opens the file, or it goes to a printer, or you hand over an INDD or IDML | The **split** command | The correction is built out of real frames, so it survives anywhere with nothing installed. |
| You export PDF and nothing else | Either | A PDF exported with the plugin loaded is already correct everywhere. |
| You are converting type to outlines | The **outline** command | Native *Type > Create Outlines* loses the fill and halves the halo. |
| You have split frames and need to edit the text | **resync**, or **watch** | Splitting makes two stories; these keep them together. |

## Installing

**The plugin** (per machine, InDesign 2026 only): copy *InDesignStrokeEngine.pln* and its *(InDesignStrokeEngine Resources)* folder into *C:\Program Files\Adobe\Adobe InDesign 2026\Plug-Ins\InDesignStrokeEngine\*, then restart InDesign. Needs administrator rights. Remove it by deleting that folder and restarting.

**The scripts**: copy the whole *tools/jsx* folder into InDesign's *Scripts Panel* folder (*Window > Utilities > Scripts*, right-click a folder, *Reveal in Explorer*). Then double-click *strokemaster.jsx* in the panel and pick a command.

Headless, for scripted pipelines:

    powershell -NoProfile -File tools\run-jsx.ps1 strokemaster.jsx -ArgJson "{\"command\":\"split\"}"

There is no need to pass `-Engine`. Measured: a headless run reports its engine as `main`, so the `#targetengine` line in the script is ignored on that path, because the directive applies only to the script InDesign compiles itself and *run-jsx.ps1* reaches the file through `$.evalFile`. The watcher survives there regardless, because `main` is persistent: a watch started in one invocation reported `watching=true` from the next. Running the script from the Scripts panel does honor the directive, giving it an engine of its own.

## The commands

**split** — select one or more stroked text frames and run it. Each becomes two frames in the same place: a bottom frame keeping the stroke, with the stroke color as its fill, and a top frame keeping the fill, with no stroke. Every stroke is then painted before every fill, which is the whole correction.

It works per text style range, so a story with a differently colored word or a heavier stroke on one character splits correctly. It refuses, and says so, when there is no stroke, when the stroke is Center-aligned, and when the frame is threaded. A Center stroke is painted over its own fill by design, so moving it underneath would change how the text looks rather than repair it.

**outline** — select text frames and run it instead of *Type > Create Outlines*. It reads the stroke settings first, converts, then rebuilds each path as a stroke copy at the width InDesign actually paints plus a fill copy, grouped in the right order. Frames with mixed stroke or fill settings are left to InDesign and reported.

**resync** — after editing the text in the top frame of a split pair, this rebuilds the bottom frame from it. The text comes across and the stroke stays as it was.

**watch** / **unwatch** — the same thing, automatically, about once a second. InDesign raises no event when text changes, so this polls; it is the only route available.

## What to know before relying on it

- **The plugin is per machine and per version.** It is compiled against the InDesign 2026 SDK and declares host 21.5; it will not load in 2025 or in a future 2027. Documents are unaffected, and render the original way wherever it is absent.
- **The split is the only correction that travels.** With it, the file is correct on a machine with nothing installed. This is the one to use for handoff.
- **A split story lives in two frames.** InDesign cannot keep them together itself: linked stories synchronize the text and then overwrite the child's formatting with the parent's, paragraph styles included. That is why **resync** and **watch** exist, and why they need the scripts installed.
- **resync needs the stroke to be uniform** across the bottom frame. A pair whose stroke varies within the story is reported and skipped rather than guessed at, because character positions recorded before an edit no longer mean anything after one.
- **The plugin leaves a one-pixel ring** along every repainted fill, from redrawing an antialiased edge. It is invisible at reading size. The split does not have it, because its fills are painted once.
- **Neither reaches text that is already outlined.** That damage is baked into the paths; convert with **outline** in the first place.
- **Cross-frame overlap is not corrected** by the plugin, which is scoped to one frame at a time.

## If something looks wrong

- *Nothing happened.* Check that a text frame is selected, and that its stroke is Outside rather than Center.
- *The halo disappeared after converting to outlines.* That is the native command, not this one. Undo and use **outline**.
- *The two frames of a pair show different text.* Run **resync**, or **watch** to keep it from recurring.
- *A collaborator sees notches and you do not.* They do not have the plugin. Use **split**, which does not depend on it.
