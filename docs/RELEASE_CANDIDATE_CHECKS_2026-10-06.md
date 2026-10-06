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

**One defect found.** With object-level opacity, the live-transparency exports (*High Quality Print*, *X-4*) clip a whole point flat at the bounds of InDesign's transparency group. The point reaches past the stroke bounds InDesign computes for the item. The flattened *X-1a* keeps the point whole. Not fixed yet (see the classification below).

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

The census gained the forms the plugin leaves to InDesign: Inside and Outside open paths with arrowheads, radial gradients on a fill, a stroke, and text, and type on a sheared path. The owner's comments on the interactive census point at the rest. Vertical text has no row: a script cannot set a story's orientation.

**Safe to leave to InDesign:**

- **Inside strokes on open paths:** InDesign's drawing has no side to stay inside, and the corrected and native rows are identical.
- **Arrowheads:** drawn by InDesign, unchanged by the plugin.
- **Center inset corners:** the effect's own geometry.
- **Straight Hash and Japanese Dots on script:** the ticks are perpendicular to the path, so they fan on tight curves, as InDesign draws them.
- **Radial gradients:** they already go through the plugin and lose their stray ink (row 83: 359 to 11 pt² on the squares).

**Should be fixed by the plugin:**

- **Stroke- and fill-level opacity and blending** (rows 60, 61, and 75): InDesign doubles the overlaps and strokes the inner edges. The plugin could draw these strokes inside a transparency group of its own.
- **Whole points under object opacity** in live-transparency PDF, as above.
- **Inside patterned strokes on overlapping shapes** (rows 49 and 50): the squares' stripes and dashes cross in the overlap. Distance bands inward, as for Outside, would fix it.
- **Center patterned strokes on overlaps** (row 11): the Thin - Thin stripes overlap.

**Left to InDesign by design:** a drop shadow set on the stroke (row 64). The effect is InDesign's own and applies around InDesign's stroke.

## Corner adjustment, the owner's review

The owner's notes on the corner adjustment study (with StrokeMaster on) fall into two groups.

- **The None column:** dots and dashes not aligned to corners, lost spikes at the star's tips, overlaps at concave corners. This is what None means, and InDesign's own drawing shows the same. The star's tips (36°) are within the miter limit, so a point exists only where a dash covers the vertex.
- **Outlined script under any adjustment:** "Not dashes." The adjustment fits its pattern between every two anchors of an outline, and the outline of script has anchors a few points apart, so the dashes run solid. A real fix lays the dashes out in the plugin, adjusting only at real corners. That is open for the owner's decision.
