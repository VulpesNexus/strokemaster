# StrokeMaster: what to know

The two plugins correct the same things in opposite ways. InDesign's draws its corrections live and saves nothing; Illustrator's writes them into the document. Everything below follows from that.

## InDesign

- **Rendered live, never saved.** Corrections are drawn on screen and into every export (PDF, PNG, EPS, PostScript, background export included). The *.indd* or *.idml* file itself is unchanged: opened where StrokeMaster is not installed (a colleague, a printer taking the package), it shows InDesign's own strokes, overlaps and spikes included. Hand over PDFs exported with the plugin.
- **Switches are global.** They apply to every open document at once and are kept on the machine, not in the document, so the same file can look different on two machines with different switches.
- **Left to InDesign:** vertical text; effects set on a stroke or fill itself (not on the object); open paths with Outside or Inside strokes, arrowheads, or dashes; separate stories that overlap, unless *Across separate stories* is on; and anything stacked between two frames of a story.
- **Trim glyph tips trims sharp tips only:** tips sharper than 60°, or beside an edge shorter than the stroke's reach. Blunter corners, such as serif feet, keep their whole miters, as InDesign draws them. A center stroke reaches half as far as an outside one of the same weight, so fewer corners count as tips there: Eagle Lake's *v* keeps its pointed foot on a center stroke and loses it on an outside one.
- **Larger PDFs** from outlined decorative fonts: every corrected corner is a small shape in the PDF.
- **One build per InDesign version.** Measured on InDesign 21.5 and 21.6 (2026) on Windows; its ID prefix is a development placeholder, so a build is for your own machine.

## Illustrator

- **Saved in the document.** Corrections are Illustrator's own appearance (strokes and fills in the *Appearance* panel, miter limits, dash alignment, Illustrator's live effects), so the file looks the same opened without StrokeMaster, and so do its exports. One exception: *Close holes inside the outline* is a StrokeMaster effect; without the plugin its last result still shows, but it no longer follows edits.
- **It edits your documents.** With *Correct automatically* (on by default), new and edited art is corrected as you work, in the undo step of your edit, and a switch changed re-corrects the active document at once (*Undo StrokeMaster Switches*); other open documents follow when you return to them. With *Also documents as they open*, opening a document changes it, so Illustrator asks to save it. With *Correct automatically* off, only *Correct Selection* changes anything.
- **Structure changes.** Threaded text is grouped across its frames, and with *Across other text objects* overlapping text objects are grouped too; artwork between the frames keeps its place.
- **Your hand wins.** An object whose miter limit, stroke order, or StrokeMaster stroke you change by hand is left alone from then on; *Correct Selection* takes it back.
- **One miter limit per path or per letter.** *No miter spikes* and *Trim glyph tips* share it, so turning one off does not bring back every miter while the other is on. Illustrator can only bevel a trimmed tip at its point, where InDesign cuts it flush, and live text keeps no whole point past its limit.
- **Close holes** fills only beneath strokes that sit below the letters: a stroke kept above them (*Strokes added through the Appearance panel* off) gets no hole fill.
- **Dashed outside strokes become center strokes.** With *Dashed outside strokes* on, a dashed outside stroke is drawn as a center stroke at twice the weight beneath the fill, its dashes aligned to corners, and Illustrator then grays the Stroke panel's Inside and Outside for it: change its alignment with the StrokeMaster panel menu's *Align Selection* commands.
- **A research build.** Illustrator 2026 (30.x) on Windows; its name and effect identifiers (*StrokeMasterRegionProbe*, `org.strokemaster.proposed.*`) are not final.
