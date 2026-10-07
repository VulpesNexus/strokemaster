# Release-candidate checks, InDesign plugin (2026-10-06)

After the third showcase review the owner asked for the open checks: the failing certification step, the output formats not yet measured, a new census and font sweep, the screen, performance on heavy material, and tests sorting the known limitations into what InDesign can keep and what the plugin should take over. The engine under test is route X `c2f0cdc2`; the panel changed to `5f4f5bab` (below).

## Certification

C23 (`c2f0cdc2`) passed in full: 110 lines identical to C20, the coverage census at 22 of 22 and suite B at 8 of 8, the host state clean, InDesign quitting in 9 s ([report](ROUTE_X_21_6_RECERTIFICATION_2026-10-03.md)). The placed-EPS round trip that came out empty in C21 and C22 wrote 239,070 bytes in C23: a transient fault of the host, not of the plugin.

## Output formats

The census, the corner adjustment study, the showcase, and suite B were exported with `c2f0cdc2` to *[High Quality Print]*, *[Press Quality]*, *[Smallest File Size]*, *[PDF/X-4:2008]*, *[PDF/X-3:2002]*, *[PDF/X-1a:2001]*, Interactive PDF, EPS, PostScript, PNG, and JPEG (*cert-lib.jsx* gained the Interactive PDF and JPEG writers), and compared with *[High Quality Print]*.

- *Press Quality*, *Smallest File Size*, and *X-4* are identical to it on every document.
- *X-3* and *X-1a* (flattened) differ only on the showcase's Kerberos Fang cell, where MuPDF draws a flattened speck as a ring.
- Interactive PDF, EPS, PostScript, PNG, and JPEG differ within anti-aliasing once rasters are compared with a one-pixel tolerance.
- Measured with the census itself, every format keeps the corrections. Summed over the census, escaping ink, stray ink, holes, and missing points stay within about 1% of the *High Quality Print* figures in X-1a, X-3, Interactive PDF, EPS, and PostScript. Natively the census has 64,634 pt² of escaping ink, 42,761 of stray ink, and 8,931 of missing points; corrected, 48,330, 12,526, and 3,783 (the rest is drawn natively by design, below).

**One defect found.** With object-level opacity, the live-transparency exports (*High Quality Print*, *X-4*) clip a whole point flat at the bounds of InDesign's transparency group. The point reaches past the stroke bounds InDesign computes for the item. Fixed in `e2c96147` (below).

## Census and font sweep

Census 9 against census 8: 17 moves, all explained.

- Rows 10 and 80 (thick-thin, whole points past the limit): striped whole points now leave the pattern's gap, by design.
- Rows 52 and 78 (gap colors): the gap color counts as "off" ink in the measure.
- Row 40 (German Beauty, dashed): escaping ink from 0.1 to 11.5 pt², slivers where creases are rounded between contours. Minor.
- Rows 82 to 84 (new): the thorn's whole point reaches into the neighboring rows, a layout artifact.

The 14-font sweep did not move.

## Performance

Four pages of outlined Kerberos Fang, outlined script, and live text, stroked Outside solid, dashed, and striped (*tools/jsx/build-performance.jsx*), exported as PDF twice each way:

| | Time | PDF size |
|---|---|---|
| StrokeMaster off | 14.8 s | 19.0 MB |
| StrokeMaster on | 16.1 s | 30.1 MB |

The plugin costs 9% more time; the PDF grows by 58%, since corrected strokes are written as filled regions and joins as geometry.

## On screen

**Found and fixed: switch changes did not reach the screen.** A layout window keeps each page item's rendering and repaints from it on zooms and page turns. A research trace showed the window's spreads drawn with empty layers after the first draw, and the plugin's corrections did draw into the window's ports on that first draw. So whatever the switches were when an item was first drawn stayed on screen. Re-rendering happens only for a new window, a model change, or overprint preview (which keeps its own rendering). The panel invalidated the views only. It now first invalidates each open document's shape graphics cache (*ILayoutUIUtils::InvalidateShapeGraphixCache*).

Measured in the background with *PrintWindow* (no focus taken):

1. A document first drawn with the switches off.
2. The switches back at their defaults through the file: the screen stays native.
3. The panel's Restore Defaults invoked as a menu action: the window shows the corrected strokes.

The panel is under Window > StrokeMaster, not under Plug-Ins.

## The limitations, sorted

The census gained the forms the plugin leaves to InDesign: Inside and Outside open paths with arrowheads, radial gradients on a fill, a stroke, and text, and type on a sheared path. The owner's comments on the interactive census point at the rest. Vertical text has no row: a script cannot set or read a story's orientation; the owner placed a vertical frame by hand (below).

**Safe to leave to InDesign:**

- **Inside strokes on open paths:** InDesign's drawing has no side to stay inside, and the corrected and native rows are identical.
- **Arrowheads:** drawn by InDesign, unchanged by the plugin.
- **Center inset corners:** the effect's own geometry.
- **Straight Hash and Japanese Dots on script:** the ticks are perpendicular to the path, so they fan on tight curves, as InDesign draws them.
- **Radial gradients:** they already go through the plugin and lose their stray ink (row 83: 359 to 11 pt² on the squares).

**Should be fixed by the plugin** (all fixed in `e2c96147`, below):

- **Stroke- and fill-level opacity and blending** (rows 60, 61, and 75): InDesign doubles the overlaps and strokes the inner edges. The plugin could draw these strokes inside a transparency group of its own.
- **Whole points under object opacity** in live-transparency PDF, as above.
- **Inside patterned strokes on overlapping shapes** (rows 49 and 50): the squares' stripes and dashes cross in the overlap. Distance bands inward, as for Outside, would fix it.
- **Center patterned strokes on overlaps** (row 11): the Thin - Thin stripes overlap.

**Left to InDesign by design:** a drop shadow set on the stroke (row 64). The effect is InDesign's own and applies around InDesign's stroke.

## Corner adjustment, the owner's review

The owner's notes on the corner adjustment study (with StrokeMaster on) fall into two groups.

- **The None column:** dots and dashes not aligned to corners, lost spikes at the star's tips, overlaps at concave corners. This is what None means, and InDesign's own drawing shows the same. The star's tips (36°) are within the miter limit, so a point exists only where a dash covers the vertex.
- **Outlined script under any adjustment:** "Not dashes." The adjustment fits its pattern between every two anchors of an outline, and the outline of script has anchors a few points apart, so the dashes run solid. A real fix lays the dashes out in the plugin, adjusting only at real corners. The owner approved that fix (below).

## The fixes approved 2026-10-06

The owner approved the four fixes above and the dash layout. All went into route X `e2c96147`, certified as C24 ([report](ROUTE_X_21_6_RECERTIFICATION_2026-10-03.md)), installed with the panel `5f4f5bab`.

- **Dashes and dots between real corners** (*IsePatternCore.h*):
  - A recording of InDesign's strokers showed how a fitted pattern is laid. On a shape's corners InDesign centers a dash (or a dot) on each corner, half on either side, and fits the dash array between. On an open line it puts a dash at both ends.
  - The plugin cuts the word's outline at its real corners (turns over 30 degrees). It has the style's own stroker paint each run as the side of a square of the same length, and lays what is painted along the run. A dash ending one run and the dash starting the next become one piece through the corner.
  - A pattern laid regardless of corners (None) is told apart on two straight lines, and keeps the stroker along the whole path.
  - On outlined script every adjusted column of the corner adjustment study is dashed or dotted now. Squares, stars, and serif text keep a dash on each corner.
- **Center and Inside patterns where contours meet:**
  - Stripes lie at their signed distance from the filled shape, across it for Center and within it for Inside. InDesign's Inside Thick - Thin keeps its thick stripe at the shape's edge, so both count from the stroke's outer edge, as Outside does.
  - Dashes, dots, and waves follow the united outline (Center) or its inward offset (Inside).
  - Only where contours' strokes meet: Center where contours lie within half the weight of one another, Inside where contours overlap.
- **The stroke's and the fill's own opacity and blend mode:**
  - The stroke is drawn as one isolated transparency group with them, so its overlapping parts composite once.
  - The fill gets them set.
  - Effects on a stroke or a fill (a drop shadow, a feather, a glow) stay InDesign's.
- **Whole points under object opacity or blending:** each point is shortened along its own axis to keep within the item's painted bounds, which bound InDesign's group in live-transparency PDF. A drop shadow alone does not bound it; its points stay whole.
- **Vertical text:** the owner's vertical frame (Japanese, *DFGMincho-UB*) showed the text route drawing strokes beside and across vertical glyphs. Vertical runs are now left to InDesign, and the frame exports within 3 pixels of InDesign's own drawing. Its outlines go through the page-item route and match.

**Measured.** Census 11 against census 9, at the same zoom: every move is one of the fixes or a known layout artifact.

| Row | Case | Before | After |
|---|---|---|---|
| 9 | Center dashed, squares, stray ink | 283 pt² | 4.6 pt² |
| 11 | Center Thin - Thin, text holes | 37.6 pt² | 0 |
| 11 | Center Thin - Thin, squares, stray ink | 147 pt² | 4.6 pt² |
| 54 | Center wavy, squares, stray ink | 139 pt² | 1.6 pt² |
| 60, 75 | Stroke opacity 50%, holes | 145 to 405 pt² | 0 |
| 60, 62, 75 | Stroke opacity 50%, thorn's missing point | 123 to 130 pt² | about 3.5 pt² |
| 61 | Fill opacity 50%, squares' missing points | 140 pt² | 3.9 pt² |

- **Inside dashed and Inside Thick - Thin (rows 49 and 50):** stray ink falls.
- **Thorns reaching past their census cells:** the restored whole points of rows 18 and 60 reach into the rows above, a layout artifact.

**Formats.** The corrected census exported to *PDF/X-4:2008* measures identical to *High Quality Print*. *PDF/X-1a:2001* and EPS stay within a few pt² of it on every metric. Under object opacity, EPS keeps a whole point that the PDFs bound. The transparency groups draw the same on screen (a background capture of overlapping squares with a 50% stroke, Center Thin - Thin, and Inside dashes).

## The adversarial review, 2026-10-07

After the owner's last pass on the showcase PDF, the open spikes were inspected, and the engine's code was reviewed adversarially: first by area, then a second review of the fixes themselves.

**The spikes.** Nothing left to fix. Row 55 (Center inset corners) is the effect's own geometry. Rows 58 and 59 are a bevel at the inverse-rounded cusp, past the limit, as the owner's rule for effect corners keeps it. The hairline beside it is a renderer seam at 2400 dpi.

**Fixed:**

- **A regression: Center patterns with the overlaps switch off.** They still followed one outline per word. Center is now resolved only when the word outline and the overlaps switches are both on.
- **Laying by runs** (*IsePatternCore.h*):
  - A run shorter than its corners' dashes laid them twice. Overlapping spans of one stroke are now one span.
  - Odd dash arrays and negative phases follow PostScript: the array is repeated, and the phase is taken modulo the period.
  - A dash shorter than the tolerance, or a dot a hair past a side's end, was dropped.
  - Fills are grouped per mark.
  - A mark centered on its side is laid only on that side.
  - A mark that only touches the line was misread as one running along it. White Diamond and Straight Hash, whose marks touch the line at their points, were taken for unfitted patterns and drawn by the stroker along the whole path. They are now laid between real corners, as dashes and dots are (showcase cells L12 and the White Diamond squares, below).
  - A pattern with a mark off the line's middle is not laid by runs, where it would have been dropped.
- **The stroke recorder** (*IseStripeProbe.h*):
  - `gsave` and `grestore` keep the path, as in PostScript.
  - `rectfill` keeps the current path.
  - The current point after `closepath` is the subpath's start.
  - A dash array without its values, an even-odd fill, or a stroke under a sheared transform now marks the recording unreadable.
- **The word's caches.** The marks' cache key hashes their points too. The 40,000-point bound now covers every path, and laying by runs is bounded at 20,000 segments.
- **Inside path strokers.** The parts of a shape thinner than the stroke, which the inward path misses, are filled solid, as InDesign's solid Inside stroke fills them. They are defined as the fill less its opening by the stroke's half width, so a shape's sharp corners do not count. The second review caught a first version that filled every corner.
- **Inside stripes** are clipped to the shape.
- **The gap color's band** is drawn undashed, whatever dash the port inherits.
- **Transparency:**
  - The stroke's own transparency group is not isolated when the stroke overprints.
  - Object blending also counts knockout and isolation groups.
  - A whole point within a group's opacity or blending keeps to the group's painted bounds as well.
  - A corner that the bounds leave nothing of keeps its bevel.
- **The research control file** reads every page-item switch. The production build starts from the switches' file.

**Not fixed, and why:**

- **A group above an anchored object** does not bound its whole points. Past the inline's anchor lies the text frame, whose painted bounds need not hold an object hanging out of it, so trimming there could cut visible points.
- **A rare refusal** can paint the gap color twice, at the same place.
- **Reading the transparency attributes on every draw** costs little next to drawing, so it stays.
- **Inside and Center inward bands at reflex corners** trim slightly differently from InDesign's offset. Not visible at review zoom.
- **Needing a host test:**
  - roman and tate-chu-yoko text in a vertical frame;
  - opacity on strokes InDesign draws natively under the plugin's group;
  - the orientation of Center Thick - Thin.

**Certification.**

- **C25, on `906e70e7`, passed in full.** All 110 measured lines are identical to C24's, and all seven PNGs are byte-identical ([*C25.json*](../tests/audit/results/routex/C25.json)). The cross-frame census passes 22 of 22, the extension census passes 8 of 8 built cases, and the 13 saved documents carry no plugin state ([*C25-state.json*](../tests/audit/results/routex/C25-state.json)).
- **The visual checks on `906e70e7`:**
  - The corner adjustment study and the vertical frame export identical to C24 at 150 dpi.
  - The showcase changes in two cells only, the White Diamond squares and L12's Straight Hash, as above.
- **A "recolored" check on the recorder** was then removed: it refused any stroker that set a color after its first mark.
  - The rebuilt `dc36839e` exports the showcase, the study, and the frame exactly as `906e70e7` did.
  - Its certification is open. In C26 one export, the extension census's X-1a reference, was written empty though the job reported success, as the placed EPS was in C21 and C22. The rerun (C27) was stopped by the system when memory ran low.
  - C28 and C29 match C24 on 108 of 110 lines. The two others are the placed-EPS round trip, drawn one device pixel lower from an EPS identical to C25's but for its font-subset prefix. Both sessions also hung at quit with a document open.
  - That document was C27's working copy, which InDesign recovered at every launch after C27 was stopped. It was closed without saving. C30, on the cleared host, was stopped by the system for memory again.
  - **C31 certifies `dc36839e` in full:**
    - All 110 measured lines are identical to C24's, the placed-EPS round trip included, and all seven PNGs are byte-identical ([*C31.json*](../tests/audit/results/routex/C31.json)).
    - The cross-frame census passes 22 of 22, and the extension census 8 of 8 built cases.
    - The 13 saved documents carry no plugin state ([*C31-state.json*](../tests/audit/results/routex/C31-state.json)).
    - InDesign quit cleanly in 6 seconds.
- **Core tests:** 283 miter-core checks.

## The owner's verdicts and the final build, 2026-10-07

The owner judged the two cells the review changed, then the counters and Wavy that followed.

- **White Diamond:** InDesign sets a smaller diamond near each corner, centered on neither side, which no run can lay. Laid by runs, the corners showed gaps, and the owner preferred InDesign's own corners. A stroker that sets such a mark again draws its pattern along the whole path, as in C24.
  - The check looks only at sides at least two weights long. A 0.9 pt run of outlined script, where every mark crowds the corners, had sent the custom dotted style back to InDesign, solid. The corner study caught it, and the research build's trace located it.
- **Straight Hash** stays laid between real corners; the owner found either version fine.
- **Counters:** the rounding of the word's creases, by a disk of half the weight, also filled every counter narrower than twice the weight, which left the counters of script letters without their pattern. Those counters keep their path again, as InDesign's own stroke runs round each counter, unless they come within the disk of the outside. In narrow counters the marks crowd, as in InDesign's drawing; the owner kept them.
- **Wavy:** InDesign lays its wave from the path's start without fitting it, so each closed outline showed a step where the wave met its own start. The plugin now lays the wave itself, from InDesign's own half-wave:
  - a whole number of half-waves between real corners, and an even number round an outline without corners;
  - at a corner, the half-waves on either side bulge away from the inside of the turn;
  - none rises higher than half the tightest radius beneath it;
  - an outline shorter than three whole waves, such as a small counter, has three, scaled down.
  - These rules were tuned offline, with the plugin's own code run on the showcase's outlines, before the host run.
- **The dip on the b stem** stays. The creases are rounded at the smallest radius at which hash ticks do not cross.

**C32 certifies `73fb1805` in full.**

- All 110 measured lines are identical to C24's, and all seven PNGs are byte-identical ([*C32.json*](../tests/audit/results/routex/C32.json)).
- The cross-frame census passes 22 of 22, and the extension census 8 of 8 built cases.
- The 13 saved documents carry no plugin state ([*C32-state.json*](../tests/audit/results/routex/C32-state.json)).
- InDesign quit cleanly in 9 seconds.
- In the review exports, the showcase changes in L12 (hashes in the counters) and L3 (Wavy) only, and the corner study only in the counters of its serif and script rows. The vertical frame is identical.
- **Core tests:** 299 miter-core checks.

## Status

**The InDesign plugin is complete (2026-10-07).** Route X `73fb1805` and the panel `5f4f5bab` are certified and installed, and the owner has signed off the showcase.

- What the plugin leaves to InDesign, and why, is listed above: *The limitations, sorted* and *Not fixed, and why*.
- Three cases still need a check in the host by hand: roman and tate-chu-yoko text in a vertical frame, opacity on strokes InDesign draws under the plugin's group, and the orientation of Center Thick - Thin.
- The extension census's XE2 case is still not built, as in every session.
