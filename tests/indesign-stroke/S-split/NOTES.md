# Fixture S: the fill-over-stroke split

The document-level correction for fixtures A and C, for anyone who cannot install the plugin. Each stroked frame becomes two superposed frames: a bottom frame that keeps the stroke and takes the stroke color as its fill, and a top frame that keeps the fill and has no stroke. Painting every stroke before every fill is what the plugin does at draw time; here the same ordering is built out of real page items, so it is carried by the document itself.

Built by *tools/jsx/build-S-split.jsx* on top of the library in *tools/jsx/ise-split-stroked-text.jsx*.

## Method

Every variant exists twice on one page, identical, with the left frame left alone as a control and the right frame split. The two columns are exactly 300 pt apart, which at 300 ppi is exactly 1,250 pixels, so the two crops are rasterized at the same sub-pixel phase and can be compared directly. That detail is load-bearing: at an arbitrary 290 pt spacing, two frames with byte-identical content differed by 1,134 pixels along their glyph edges, a floor under every measurement taken that way. With 300 pt, a refused frame and its control differ by **0** pixels, worst channel **0**.

The plugin corrects the control frames too, so the fixture is measured twice: once with the plugin loaded and once with it removed from *Plug-Ins*, from the same saved document.

## Results without the plugin

This is the measurement that matters, because it is the situation the split exists for.

| Entry | Variant | Control | Split |
| --- | --- | --- | --- |
| S01 | uniform: black fill, Paper 8 pt Outside | 1,010 px covered | **0** |
| S02 | varied fill: one magenta character on line one | 2,763 px | **0** |
| S03 | varied stroke: one character at 5 pt magenta | 3,260 px | **0** |
| S04 | no stroke at all | 0 | 0, refused, identical to the control |
| S05 | Center alignment | 54,702 px | 54,702 px, refused, identical to the control |

S02 and S03 are fixture C's defect rather than fixture A's, and the same split corrects both without a line of code specific to either, which is the same evidence the plugin gives: A and C are one defect.

## The split needs nothing installed

Rendering the same document with and without the plugin, per frame, at 300 ppi:

| Entry | Control frame | Split frame |
| --- | --- | --- |
| S01 | 3,112 px change | **0** |
| S02 | 3,845 px | **0** |
| S03 | 3,561 px | 232 px |
| S04, S05 | 0 | 0 |

The control frames change because the plugin corrects them. The split frames do not change, because there is nothing left for the plugin to correct. S03 is the one exception and the reason is visible in its structure: its bottom frame carries three different stroke settings, so it is still a fill-and-stroke Outside run that the adornment repaints, idempotently. Its glyph core moves by 0.008 of a luminance level.

The split is also cleaner than the plugin. With the plugin loaded, a control's fill core measures 56,679 px against the split's 53,829, a difference of 2,850 that matches the frames' whole measured difference: that is the plugin's one-pixel repaint ring, which the split does not have because its fills are painted once, natively.

## Paint order

The structural proof, independent of any renderer. Text objects in paint order, S for stroke and F for fill:

| Entry | Control | Split |
| --- | --- | --- |
| S01 | `S F / S F` | `S F F / S F F` |
| S02 | `S F S F S F / S F` | `S F F F F / S F F` |
| S03 | `S F S F S F / S F` | `S F S F S F F F F / S F F` |

Every fill that belongs to the visible text is painted after every stroke of that pair. S03 still interleaves inside its bottom frame, where the ink is the stroke color painting over the stroke color, which cannot damage anything the reader sees.

## What the split refuses, and why

- **No stroked text.** Nothing to correct.
- **A Center-aligned stroke.** A Center stroke is painted over its own fill by definition, so moving it underneath is a redesign of the text and not a repair: splitting S05 took its covered fill from the whole glyph core to zero. The plugin skips Center runs for exactly this reason, and the two corrections have to agree or a document would look different depending on which was applied.
- **Threaded frames.** A duplicate of one frame in a thread does not carry the thread.

Registration is sound: adding an Outside stroke moves no glyph. Word offsets are identical between a stroked and an unstroked copy of the same text, and the baseline difference between two frames equals their frame offset exactly (*tools/jsx/probe-linked-stories.jsx*, Q7).

## What it costs: the two frames cannot be kept in sync natively

The split turns one story into two, and InDesign has no built-in way to keep them together while they carry different formatting. Measured in *tools/jsx/probe-linked-stories.jsx* and *tools/jsx/probe-style-mapping.jsx*:

- `contentPlace` does create a linked story on the same page, and `Link.linkType` reports `Internal Linked Story`. It is document data, so it survives on any machine.
- Updating is never live. Editing the parent leaves the child stale at `LINK_OUT_OF_DATE` until `update()` runs, or until a save with `updateWhileSaving`.
- An update then overwrites the child's formatting with the parent's, **paragraph styles included**: a child styled stroke-only came back at weight 1, stroke None, and fill Black.
- Custom Style Mapping does not rescue it from scripting. `linkedStoryOptions` exposes only `updateWhileSaving`, `warnOnUpdateOfEditedStory`, `removeForcedLineBreaks`, and `applyStyleMappings`, with no mapping table; the only mapping objects in the DOM are the XML ones. With `applyStyleMappings` on and `mapStyles` on, the child still lost its formatting, and `mapStyles` generated no style groups. Whether the *Custom Style Mapping* dialog can define a pair that survives is untested, because nothing reaches it from a script.
- Linked content cannot be placed into an existing frame at all; the call fails with "Content Place Failed".

So the split is a real workaround and not a fix: it is correct, it is portable, and from then on the text lives in two places.

## A script can keep them in sync, and does

What InDesign will not do, *tools/jsx/ise-live-sync.jsx* does, and it is measured rather than asserted. It rebuilds the bottom frame from the top rather than patching it, because no character offset recorded before an edit still means anything afterward.

- **A resync keeps both the text and the formatting** (*tools/jsx/build-T-livesync.jsx*). Editing only the top frame left the bottom stale, and after a resync the bottom read the edited text with its stroke untouched: weight 8, Paper, Outside, Round join, fill still painted in the stroke color, and still behind the top frame. That second point is the one InDesign's own linked stories get wrong.
- **A watcher notices an edit on its own** (*tools/jsx/probe-live-sync-watch.jsx*). With an idle task at 700 ms, an edit to the top frame left the pair mismatched, and ten seconds of idle time later the two frames matched again with the resync counter at 1, without anything being asked to run.
- Idle tasks are the only route, because no InDesign event reports a text edit: the whole `DocumentEvent` family is the save and close pair (*tools/jsx/probe-live-sync.jsx*).

The limit to state plainly: the bottom frame's stroke settings have to be uniform across its story, because they are read back from the frame rather than from a record indexed by character positions an edit invalidates. A pair whose stroke varies within the story is reported and skipped rather than guessed at. And all of this needs the script installed, which is the trade: the split's **output** is portable to a machine with nothing installed, while **editing** it afterward is not.

## Reproduce

1. *powershell -NoProfile -File tools\run-jsx.ps1 build-S-split.jsx -With ise-split-stroked-text.jsx*
2. *python tools/stroke_probe.py tests/indesign-stroke/S-split/actual S-split*
3. For the numbers that matter, export the saved document again with the plugin removed from *Plug-Ins* and probe *actual-noplugin* the same way.
