# Illustrator: the native route's gaps (2026-10-16)

The owner asked for every concern left by [the native route](ILLUSTRATOR_NATIVE_ROUTE_2026-10-15.md) to be addressed, with a known limitation only where Illustrator certainly has no native means. This records what closed, how it was measured on Illustrator 30.8.2 with region-prep-16 (`4b462039…`), and what stays open and why. Records: *tests/audit/results/illustrator-native-gaps-2026-10-16.json* and *illustrator-census-2026-10-16.json*.

## Closed natively

| Concern | Native means | Measured |
| --- | --- | --- |
| Text with fills and strokes both mixed | ATE's per-character *FillFirst* off: each character paints its stroke before its own fill | Fill-core damage 19,142 → 896 px; text kept, through save, reopen, and edit |
| Inside strokes on overlapping contours | Illustrator's own *Effect > Pathfinder > Add* as a pre-effect, after the user's own | Census Inside rows: stray 2,805.6 → 60.0 pt² |
| Dashed whole points | Illustrator's own *Align Dashes to Corners* on Center strokes | Dashed Center thorn: point missing 162.8 → 6.3 pt² |
| Outside strokes on overlapping contours, round and bevel joins | The fill above the stroke, as for miter joins (it was skipped for them) | Stray 2,431.8 → 958.1 pt² across those rows |
| An edit outdating a limit | The automatic re-fix (needs the plugin; see below) | Host-tested: limits re-chosen after an edit, in the edit's undo step |
| *File > New* filled with scripted sizes | The ten entries the probes made removed (a backup is kept outside the repository) | `documents.add` works again (error 129 before) |

- **FillFirst** has no control in Illustrator's interface, but Illustrator keeps it in the file and draws it. `native-route` sets it on frames no object-level paint can serve: both fills and strokes mixed, and frames whose appearance it does not rebuild unless a painting fill already sits above *Characters*. The 896 px left are the second line's strokes over the first line's fills (see *Known limitations*).
- **Pathfinder Add** is written with the parameters Illustrator writes from its own menu (read back with the research command `style-dump`), and renders exactly as the menu command does (60.8 pt² in the probe's measure, all of it the miter join at the united outline's two concave corners, which that measure erodes as round). The census allows miter wedges only at anchors, so it reads 4.6 pt² at Inside 4 and 0 with round or bevel joins. The effect carries a tag in its parameters: a Pathfinder Add the user put there is used as the union and never removed, and the fix's own comes off again when a later edit no longer needs it. Applied to a group instead of the compound path, the same effect fills the interior, so it is never used that way.
- **Dash alignment** is stored on the stroke field as Illustrator's Stroke panel stores it: live-effect parameters with no live effect, holding a Beautiful Strokes dictionary (`BSDashAdj` on) beside a `StrokeStyle` entry. The 2026-10-15 attempt left out `StrokeStyle`, which is likely why its stroke vanished. A collinear anchor does not anchor dashes, as the owner's dash ruling wants.
- The miter limit is now written before these parser writes, so `SetPathStyle` cannot undo them. Where Pathfinder unites the contours, the limit is chosen from the united outline.

### The automatic re-fix

While the plugin runs, art the fix has written to (marked in its dictionary) is fixed again after it changes. A timer looks twice a second, and the fix is appended to the undo step of the edit that caused it, so one Undo takes both back. Host-tested (*tools/jsx/ai-refix-*.jsx*, through *tools/run-ai-fresh.ps1 -Script2*):

- after an edit (a sharper tip, two letters typed), the thorn's limit goes from 11.49 to 19.13 and the new v and w get 2.02 and 2.10 instead of the inherited 4; a manual fix afterward finds nothing to change;
- one undo restores the edit and the limits together;
- art seen for the first time (just fixed, opened, pasted) is taken as fixed, so opening a document changes nothing in it;
- selecting art starts nothing;
- a value the user sets by hand (a miter limit, the fix's Pathfinder Add or dash alignment removed) is kept, and that item is released from the re-fix; running the fix script again takes it back;
- text with spaces stays followed (every run's limit is recorded, spaces included).

It waits while a story has the text focus, and does nothing while redo steps wait, since any change then would discard them. A user's Redo cannot be tested from a script: a script run is one undo step, so a script's `app.undo()` and `app.redo()` lose the redo list with the re-fix paused too.

### Census (71 rows, native → after the fixes)

| Rows | Count | Escaping | Stray | Holes | Point missing |
| --- | ---: | --- | --- | --- | --- |
| Page items, Outside (mitered) | 19 | 27.4 → 27.4 | 6,333.8 → 235.4 | 383.8 → 315.5 | 3,128.2 → 744.7 |
| Page items, Center (mitered) | 13 | 15.8 → 15.7 | 5,639.1 → 5,633.2 | 4,210.3 → 4,209.3 | 2,140.5 → 704.5 |
| Page items, Inside | 11 | 0.0 → 0.0 | 2,805.6 → 60.0 | 481.9 → 261.2 | 0.0 → 0.0 |
| Page items, Outside or Center, round or bevel joins | 8 | 0.0 → 0.0 | 2,431.8 → 958.1 | 24.7 → 7.4 | 1,303.4 → 1,303.4 |
| Live text | 20 | 60.2 → 22.7 | 0.0 → 0.0 | 0.0 → 0.0 | 0.0 → 0.0 |

No row is worse than on 2026-10-15. The Center rows' stray is the inner contour edges, which the owner's rulings keep (the InDesign plugin keeps them too). The round- and bevel-join rows' missing points are by design: those joins draw no point. Row 28 (open, unfilled) is drawn differently from run to run by the census builder; its native and corrected renders change together, and the fix leaves it as drawn.

## Known limitations

Each of these needs something Illustrator does not have, so no native attribute can cover it:

- **Mixed fills and mixed strokes across characters.** Illustrator paints a text object's characters in one layer, *Characters*, and FillFirst orders only each character's own stroke and fill. Only an object-level paint can go beneath or above every character, and it has one color. A later character's stroke can therefore still reach an earlier character's fill: tight leading, as in the fixture, or tight kerning. With the plugin, the text-kept effect covers it.
- **Glyph tips beveled, not cut flush.** Illustrator strokes have three joins, miter, round, and bevel, and no clipped miter. A bevel removes the spike at the vertex where InDesign's plugin cuts it one half width further out.
- **One miter limit per glyph or per path, not per corner.** Illustrator stores one limit per stroke, so where one glyph or path has a corner to keep sharper than a corner to cut, one ruling is not met (24 kept corners beveled and 31 cut corners mitered of 6,311 in the census).
- **Dashed Inside and Outside strokes.** Illustrator aligns dashes on Center strokes only: Beautiful Strokes declines Inside and Outside strokes, and the same option written into their own effect's parameters changed nothing (census 17). Inside strokes lose no point, since they are clipped to the fill. On Outside strokes a dash gap can still fall on a whole point (163 pt² per row). The only native equivalent would rewrite the stroke as Center at twice the weight under the fill, which changes what the Stroke panel shows the user, so it is not done.
- **Dashes at shallow corners.** Illustrator anchors dashes only at corners turning more than 45 degrees (measured: 45 not anchored, 46 anchored). The InDesign plugin anchors them past 30, so corners turning 30 to 45 degrees keep an unfitted pattern.
- **Without the plugin, a new glyph's limit.** A typed character takes its neighbour's limit, and choosing its own needs code: the plugin's re-fix, or the fix script run again. The overlap corrections follow every edit with no plugin.
