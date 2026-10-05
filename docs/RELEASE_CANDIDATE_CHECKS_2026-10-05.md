# Release-candidate checks, InDesign plugin (2026-10-05)

The owner asked for the release-candidate checks after route X `e367f134` was certified. This records what was checked, what it found, and what changed because of it. The plugin certified at the end of these checks is `8b924675` ([report](ROUTE_X_21_6_RECERTIFICATION_2026-10-03.md#whole-points-on-straight-edges-outside-fill-route-x-candidate-8b924675)).

## Output paths

The coverage census, suite B, and the font, thorn, and shape test were exported with `e367f134` to every output the plugin must hold up in: PDF *[High Quality Print]*, *[PDF/X-4:2008]*, *[PDF/X-1a:2001]* (flattened), PNG at 300 ppi, EPS, and PostScript (*[High Resolution]* flattener). The jobs ran through *tools/jsx/cert-run.jsx* in the owner's running InDesign, on copies opened without windows; *cert-lib.jsx* gained a *hideLayer* preparation, since the census saves its document with the measure's geometry layer visible. EPS and PostScript were converted to PDF with Ghostscript for the census measure. Record: [*output_paths_21.6.json*](../tests/audit/results/output_paths_21.6.json).

- **PDF/X-4 and PDF/X-1a:** suite B and the font test are pixel-identical to *[High Quality Print]*. On the census, the flattened and X-4 exports differ only at the anti-aliasing level, mostly on drop-shadow rows (off-level pixels up to 47 pt² more, and as often fewer).
- **PostScript:** the same as the PDF on every corner and point, within anti-aliasing.
- **PNG:** suite B equals the PDF within a one-pixel shift. The font test differs where MuPDF strokes a contour much smaller than the stroke as a ring with a hole (Kerberos Fang's specks), which InDesign's own rasterizer draws as a disk: a renderer difference, known since the overlapping-outlines certification. InDesign wrote a 0-byte PNG for the census page, 400 × 11,570 pt.
- **EPS: one limitation.** InDesign's EPS export turns the census page's strokes into filled outlines (the Ghostscript conversion holds 2 strokes where the PDF holds 699), drawing them with its own joins. Where the plugin bevels or trims a corner and relies on its stroke's bevel join, the EPS shows a round join instead: escaping ink on corner-effect text rises from 0.9 to 19.0 pt² (inset corners), 0.6 to 12.6 (inverse rounded), and 0.5 to 4.7 (fancy). Suite B and the font test, which have no transparency, keep their strokes in EPS and match the PDF. Not fixed: EPS is a legacy format here, and the fix would draw every beveled join as geometry.

## Fonts

The census builder gained a sweep mode (*ISE_ARGS.sweep*: family, style, and an optional sample text), which builds its core rows for each font: outlined text, the thorn, and the squares at Outside 4 and 8, Center 8, and Inside 4, and live text at Outside 4 and 8 and Center 8. Fourteen fonts: Times New Roman Regular and Bold, Arial Black, Myriad Pro Bold, Acumin Variable Concept Wide Black (a variable font), Impact, Segoe Script Bold, Great Vibes, Old English Text MT, CC Wild Words Bold Italic, Anime Ace 2.0 BB Bold, and Kozuka Gothic Pr6N B, Source Han Sans VF Heavy, and Yu Mincho Demibold with Japanese text. Record: [*font_sweep_21.6.json*](../tests/audit/results/font_sweep_21.6.json).

No row shows stroke inside the letters, holes in the fills, or escaping ink beyond fractions of a pt² on live text. Every flagged value traced to the layout (wide fonts running into the thorn and squares; the thorn's point reaching the row above; the last row at the page edge), or to choices by design: live text and Inside strokes bevel past the limit as InDesign does, and Center strokes keep InDesign's drawing of overlapping contours.

## On screen

The owner looked at the census and the font sweep in InDesign with the plugin loaded. Two findings, both fixed in `8b924675`:

- **Spikes on script text.** Segoe Script and Great Vibes outlined text showed long spikes: corners past the miter limit where a curve meets a straight piece 0.02 to 0.12 pt long, or two curves meet, kept as points at the limit distance (16 pt at Outside 4). At the same time the owner was sorry to see the thorn's point cut flat, at its 20 pt side. The new rule, chosen by the owner: a corner past the limit keeps its whole point when both edges are straight, the shorter is at least the stroke's reach, and the point is at most five times that edge; every other corner is beveled as InDesign bevels it (*ise_miter::WholePoint*). A survey of the fonts (glyph outlines read with *fontTools*) set the bounds: script fonts have no straight-edged corner past the limit; Kerberos Fang's straight corners have points 13 to 111 times their edges, so none qualifies at 72 pt and 2 or 3 of about 3,000 at 300 pt; the thorn's point is 2.3 times its sides at Outside 4 and 4.6 times at Outside 8.
- **A black gap in Great Vibes' A.** InDesign fills an Outside-stroked item's outline offset outward, and on the A's loop the offset broke and flooded the letter's opening; the plugin's refill could not take that fill back. The fill is now drawn at the fill event for Outside as for Inside, and InDesign's offset is never drawn: spill falls to the anti-aliasing baseline on the row (167 to 55 pt²), on the thick-thin row (275 to 55), and on the corner-effect rows.

## Explained, not changed

- **Text in a frame both sheared and rotated** escapes about equally natively and corrected (94 and 88 pt²): the escaping ink is the next row's text reaching into the row's cell, in every export including the geometry. A layout artifact of the census, not a defect.
- **Inside strokes in PDF renders** reach one device pixel past a frame's edge where the PDF port writes the stroke's clip 0.002 pt wide (route X `e367f134`).

## Not done

- **A real document of the owner's:** skipped for now at the owner's request.
- **Packaging, the README and usage guide, and the compatibility statement** (only InDesign 21.6.0.57 is tested).
