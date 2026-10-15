# Stroke alignment: where it is missing, and the native workarounds (2026-10-16)

The owner asked why Inside and Outside alignment are unavailable on compound paths and in other cases, in Illustrator or InDesign, and for workarounds where possible, along with an opt-in for dashed Outside strokes. This closes the census's open item AI-03 for Illustrator 30.8.2, with region-prep-17 (`7ce361cf…`) and region-prep-18 (`4695d7b6…`), which adds Inside drawn as two fills. Records: *tests/audit/results/illustrator-stroke-alignment-2026-10-16.json* and *illustrator-two-fill-inside-2026-10-16.json*.

## Illustrator: what draws which alignment

*tools/jsx/ai-align-survey.jsx* gives one target per cell, each with a Black fill and a gray 8 pt miter stroke. It sets each target to Center, Inside, and Outside through `AIPaintStyleSuite::SetStrokeAlignment` (the call behind the Stroke panel's *Align Stroke* buttons), reads the alignment back from the art style, and exports the page in each state. *tools/measure_align_survey.py* compares each cell's painted and black areas across the three states: equal areas mean the alignment is not drawn.

| Target | Center, Inside, Outside drawn |
| --- | --- |
| Closed path, ellipse | all three |
| Compound path of closed contours (overlapping squares, outlined type) | all three |
| **Compound path with one open contour** | **Center only: Inside and Outside are dropped for every contour, closed ones included** |
| Open path | Center only (the setting does not stick) |
| Group with its own (Appearance) stroke | all three |
| Type (point, area, on a path): character strokes, or an Appearance stroke on the type object | **Center only: the setting is stored but never drawn** (*tools/jsx/ai-text-align.jsx*) |
| Live Paint group | none: its edges carry no alignment |

So in Illustrator a compound path aligns like any closed shape, until one of its contours is open: then nothing in it aligns, and the Stroke panel shows Center. The census's Outside and Inside rows on compound paths never met this, since every contour there is closed.

## InDesign: measured

*tools/jsx/id-align-survey.jsx* ran in the owner's InDesign 2026 (with the owner's leave), in a document of its own made without a window and closed unsaved. The owner's open documents were checked unchanged afterward, and the active document was put back. It used the same cells, set through `strokeAlignment` and exported in each state, measured by *tools/measure_align_survey.py*:

| Target | Center, Inside, Outside drawn |
| --- | --- |
| Closed path, ellipse, text frame's own stroke | all three |
| Open path | all three (the side taken from the path's direction) |
| Compound path of closed contours, overlapping squares | all three |
| **Compound path with an open contour** | **all three** |
| Group of stroked paths | all three (set on the group, passed to its paths) |
| Text (character strokes) | **Center and Outside only**: `TextStrokeAlign` has no Inside value (`CENTER_ALIGNMENT`, `OUTSIDE_ALIGNMENT`), and reading `TextStrokeAlign.INSIDE_ALIGNMENT` raises an error that `try` does not catch |

So the compound-path restriction is Illustrator's alone: InDesign aligns a compound path's contours whatever they are. Both applications lack Inside on text. (The survey's outlined-text cell lost its stroke in the conversion, so it shows nothing; the compound-path cells cover the same object.)

## The workarounds (Illustrator, native, opt-in)

`native-align` (bridge command; user script *tools/jsx/strokemaster-align-selection.jsx*) gives Outside or Inside where Illustrator has none, each with the closest construct Illustrator draws itself. Each was measured against Illustrator's own alignment on an equivalent closed shape (*tools/jsx/ai-width-side.jsx*, *ai-align-workarounds.jsx*, *ai-native-align.jsx*):

| Target | Outside | Inside |
| --- | --- | --- |
| Closed paths, compound paths of closed contours, groups with their own stroke | Illustrator's own alignment | Illustrator's own alignment |
| Open path | A one-sided width profile at the full weight: the stroke lies on one side (2,250 pt² there, 31 on the other, which is the antialiased edge), with the convex join mitered | The same on the other side |
| Compound path with an open contour | Center at twice the weight beneath the fill: within 8 pt² of Illustrator's own Outside on the closed contour | Two fills (below), where each open contour is open by no more than the weight: within 16.8 pt² of Illustrator's own Inside on the closed contour, and 30.2 pt² on a triangle open by 4 pt (most of it the gap) |
| Type | A stroke of twice the weight beneath the characters (one stroke), or each character's weight doubled and painted before its own fill (mixed strokes): within 71 pt² of Illustrator's own Outside on the outlined type | Two fills (below), with one fill and one stroke throughout: within 49 pt² of Illustrator's own Inside on the outlined letters |

- **Which side is Inside on an open path:** the side its closing would enclose, so a contour opened at one point keeps its sides.
- **Inside drawn as two fills:** a fill of the stroke's color beneath, and the item's own fill on top, shrunk by the weight with Illustrator's live *Effect > Path > Offset Path* (`ofst`, `jntp`, and `mlim` as Illustrator writes them, with the stroke's join and limit). What shows between them is the Inside band, and both follow every edit of the outline, with or without the plugin. The stroke itself stays in the style, hidden, so restore brings it back as it was.
  - Offset Path erodes correctly, a thin part disappearing as it should (a square with a 6 pt bar, inset 4: the bar goes, 2,687 pt² left of 2,704 expected). But when the inset leaves nothing, it gives the shape back unshrunk (Minion *Ax* at 96 pt: insets of 1, 2, and 4 leave 997, 460, and 9 pt², an inset of 8 gives back 1,602 of 1,614). Then every part is thinner than the stroke, and Inside should paint it all in the stroke's color.
  - So the shrunk fill is hidden whenever the inset is empty (`native_region::InsetEmpty`, the shared core's inset). `native-fix`, and so the automatic re-fix, shows or hides it again after an edit: letters enlarged from 72 to 300 pt get their shrunk fill back.
  - The inset judged is the effect's own offset, so a change made in the Appearance panel or by *Scale Strokes & Effects* counts. On type it is scaled by the frame, as Offset Path is: in a frame scaled to 50%, 4 pt letters drawn at 2 pt come within 29.8 pt² of Illustrator's own Inside at 2 pt on the outlined letters.
  - The fix's Offset Path carries a tag in its parameters, as its Pathfinder Add does, so a user's own Offset Path fills are never shown, hidden, or removed. Restore leaves the item as it is when the two fills are no longer there as made (removed, reordered, or given another effect): then the stroke's color would cover the item.
  - An open contour's fill ends along its closing, where its stroke never runs, and the band runs there too (237.6 pt² of a 30 × 8 pt strip along a 60 pt closing). So Inside is refused (`inside-open-closing`) when a closing is longer than the weight, and the script says that *Object > Path > Join* closes the contour, after which Illustrator aligns the compound path itself. A contour left open by a hair keeps the two fills.
  - The band is one flat fill, so a stroke it cannot carry is refused and left as it is: dashes (`inside-dashed`), a gradient or pattern (`inside-stroke-paint`), effects of the stroke's own, a brush, a width profile, or arrowheads.
  - The fix's own effects are found by name and tag. In a document saved as PDF and then edited, the parsed Offset Path effect once reported another handle than the one Illustrator gives for its name, though its name and tag were intact, and restore refused. The same steps alone matched.
  - Not drawn: a scripted export made in the same script call as the change can show the previous state (a restored compound path came out 2,009 pt² off in the same call, exact in the next one). Illustrator redraws once a script ends, so a user never sees it.
- **Why the profile is used only on open paths:** on a closed contour it fails at the start point (that corner is cut on a slant, 231 pt² off the square), and its joins ignore the miter limit (the thorn's point comes out short and curved, 573 pt²). On type, a one-sided Inside profile was 1,333 pt² off Illustrator's own Inside, with spikes outside the letters; the two fills come within 49.
- **The empty side gets a thousandth of the weight:** exactly 0 drew a wedge across closed shapes and a star at joins.
- **What the user sees afterward:**
  - a one-sided profile keeps the weight in the Stroke panel, with a custom width profile;
  - the doubled forms show Center and twice the weight.
- **Restore** (the script's third choice) puts every construct back as it was, at half the weight it has then (so a weight changed since is kept: a doubled stroke set to 20 afterward comes back at 10). Measured: open path, compound path (Outside and Inside), type with one stroke (Outside and Inside), mixed strokes, a frame scaled 150% or 50%, and the dashed thorn all come back to their original drawing exactly (0 pt²). Items given Illustrator's own alignment keep it, since that is an ordinary Stroke panel setting.
- **A scaled type frame:** the object-level stroke scales with the frame as the character strokes do, so Outside reaches twice what the Center stroke reached outside the letters (measured 1.9, miters and caps included).
- **A failure part-way** rolls the doubled form back to the stroke's own alignment, weight, and paint order, so an item is never left half rewritten; type gets its character strokes back if its style cannot be set. For the two fills, the record is written and read back before the first change, so a rollback always knows what to undo.
- The automatic re-fix and `native-fix` read these forms correctly:
  - a Center stroke beneath its fill counts as Outside, with its interior miters hidden unless they escape through a thin part;
  - a one-sided profile counts as its side at the full weight.

## Independent review

Two reads on the API credit, the second after the fixes. They found, and the code now handles:
- a partial failure of the doubled form, and of type Outside;
- restore halving unstroked runs, losing the earlier paint order, or picking the wrong stroke field;
- a stale profile record;
- a Center stroke under an even-odd fill;
- a profile counted where none was written.

Two findings did not hold on the host: the scaled frame (above), and the reference counts. Arrays and entries are consumed as in the SDK's StrokeFilter sample.

A third read covered the two fills. Its findings, now handled:
- restore went on when the two fills were not found, leaving the band over the item;
- a user's own Offset Path fill could be taken for the fix's;
- the inset was judged from the recorded weight, not the effect's own offset;
- a type failure part-way could leave a stuck record;
- `native-fix` rewrote the hidden stroke's miter limit, so restore was not exact.

Two findings were accepted as they are. A failure of the region core counts as an empty inset, which shows the whole item in the stroke's color, the more visible error. And on type, `native-fix` outlines the letters once more to check the inset.

A fourth read covered the refusals and the lookup by name. Its findings, now handled: a stroke with effects of its own (Roughen, Zig Zag) was not refused; one failure path did not release a dictionary; and a long list of reasons could cut off the one the script's hint looks for (each reason is now listed once). It also took `ContainsParams` to read only an effect's outer dictionary, which the SDK header contradicts; the test was dropped anyway, since the getters give their defaults.

The census run with the opt-in also showed that a second fix judged the miter rulings against the limit the first had written. Every fix now keeps the user's own limit in its record and judges against that, so the automatic re-fix is stable too.

## The opt-in for dashed Outside strokes

*tools/jsx/strokemaster-outside-dashes.jsx* (`native-align` with `dashes`) rewrites each selected dashed Outside stroke as Center at twice the weight beneath its fill, then aligns its dashes to corners (Illustrator does that on Center strokes only). It needs one opaque fill and one stroke, since the fill hides the inner half.

Census (71 rows, *tools/jsx/ai-coverage-census.jsx* with the opt-in pass after the usual fix, then *native-fix* again; region-prep-17 `7ce361cf…`), point missing in pt²:

| Row | After the fix | With the opt-in |
| --- | ---: | ---: |
| Outside 4 miter, dashed 12/4 (Minion) | 163.3 | 6.2 |
| German Beauty, Outside 4 miter, dashed | 163.3 | 6.3 |
| Script, Outside 4 miter, dashed | 163.3 | 6.3 |

Every other row is unchanged. Two side effects:
- **German Beauty:** 8.1 pt² of escaping, against 0.06. The doubled stroke has an inner half, and on its spiky letters one limit per path cannot both keep a whole point and cut an interior miter that runs out through a thin part (one more such corner, 32 against 31).
- **Script:** escaping drops from 3.1 to 0.

The rewrite pays where a point matters more than that trade-off, which is why it stays opt-in.

## Known limits

What no native construct covers, measured or checked on the host:
- **Inside on type with mixed fills or strokes** (`type-inside-needs-one-fill-one-stroke`): the two fills are object-level, one color across the frame, and outlining the letters is the user's call.
- **Inside on text in InDesign:** `TextStrokeAlign` has no Inside value (checked in scripting).
- **Inside on a compound path with a contour open by more than the weight:** the band would run along the closing (above). Joining the contour gives Illustrator's own Inside.
- **Dashed, gradient, pattern, brushed, width-profiled, or arrowheaded strokes, or strokes with effects of their own, for Inside where Illustrator has none:** the band cannot carry them. Dashes and gradients were refused on the host; the other refusals were not host-tested, since scripting cannot make them.
- **The Stroke panel on an Inside drawn as two fills:** the stroke is hidden in the style, so a weight or color changed there shows only after restore. The band's width is the Offset Path effect's offset, which the Appearance panel edits.
- **Dashed Outside (the opt-in):** the German Beauty trade-off above.
