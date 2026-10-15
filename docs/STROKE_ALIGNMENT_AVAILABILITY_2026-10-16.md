# Stroke alignment: where it is missing, and the native workarounds (2026-10-16)

The owner asked why Center or Inside alignment is unavailable on compound paths and in other cases, in Illustrator or InDesign, and for workarounds where possible, along with an opt-in for dashed Outside strokes. This closes the census's open item AI-03 for Illustrator 30.8.2, with region-prep-17 (`7ce361cf…`). Records: *tests/audit/results/illustrator-stroke-alignment-2026-10-16.json*.

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

## InDesign: what is known

From the repository's own InDesign runs, not re-run today (the only InDesign running was the owner's working session):

- **Text** has Center and Outside only. `TextStrokeAlign` has no Inside value, and InDesign draws Outside as a stroke of twice the weight under each run's fill (*docs/INDESIGN_STROKE_INVESTIGATION.md*).
- **Open paths** keep Outside, with the side taken from the path's direction (*docs/COVERAGE_CENSUS_2026-10-04.md*).
- **Compound paths** take all three. The coverage census drew Inside and Outside on the same compound paths as Illustrator's (outlined type, overlapping squares).
- Not tested in InDesign: a compound path with an open contour, and the other targets above.

## The workarounds (Illustrator, native, opt-in)

`native-align` (bridge command; user script *tools/jsx/strokemaster-align-selection.jsx*) gives Outside or Inside where Illustrator has none, each with the closest construct Illustrator draws itself. Each was measured against Illustrator's own alignment on an equivalent closed shape (*tools/jsx/ai-width-side.jsx*, *ai-align-workarounds.jsx*, *ai-native-align.jsx*):

| Target | Outside | Inside |
| --- | --- | --- |
| Closed paths, compound paths of closed contours, groups with their own stroke | Illustrator's own alignment | Illustrator's own alignment |
| Open path | A one-sided width profile at the full weight: the stroke lies on one side (2,250 pt² there, 31 on the other, which is the antialiased edge), with the convex join mitered | The same on the other side |
| Compound path with an open contour | Center at twice the weight beneath the fill: within 8 pt² of Illustrator's own Outside on the closed contour | Refused: no faithful form |
| Type | A stroke of twice the weight beneath the characters (one stroke), or each character's weight doubled and painted before its own fill (mixed strokes): within 71 pt² of Illustrator's own Outside on the outlined type | Refused: no faithful form |

- **Which side is Inside on an open path:** the side its closing would enclose, so a contour opened at one point keeps its sides.
- **Why the profile is used only on open paths:** on a closed contour it fails at the start point (that corner is cut on a slant, 231 pt² off the square), and its joins ignore the miter limit (the thorn's point comes out short and curved, 573 pt²). On type, a one-sided Inside profile was 1,333 pt² off Illustrator's own Inside, with spikes outside the letters.
- **The empty side gets a thousandth of the weight:** exactly 0 drew a wedge across closed shapes and a star at joins.
- **What the user sees afterward:**
  - a one-sided profile keeps the weight in the Stroke panel, with a custom width profile;
  - the doubled forms show Center and twice the weight.
- **Restore** (the script's third choice) puts every construct back as it was, at half the weight it has then (so a weight changed since is kept: a doubled stroke set to 20 afterward comes back at 10). Measured: open path, compound path, type with one stroke, mixed strokes, and a frame scaled 150%, and the dashed thorn all come back to their original drawing exactly (0 pt²). Items given Illustrator's own alignment keep it, since that is an ordinary Stroke panel setting.
- **A scaled type frame:** the object-level stroke scales with the frame as the character strokes do, so Outside reaches twice what the Center stroke reached outside the letters (measured 1.9, miters and caps included).
- **A failure part-way** rolls the doubled form back to the stroke's own alignment, weight, and paint order, so an item is never left half rewritten; type gets its character strokes back if its style cannot be set.
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
