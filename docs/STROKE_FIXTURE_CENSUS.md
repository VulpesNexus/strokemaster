# Stroke fixture census

Phase A of the InDesign stroke engine investigation: an inventory of every test case in the supplied fixture document, what each one demonstrates, and where in InDesign's output pipeline the defect is already visible. Evidence labels follow the investigation's ladder: PROVEN, STRONGLY SUPPORTED, PLAUSIBLE, DISPROVEN, and UNKNOWN.

## Environment

| Item | Value |
| --- | --- |
| InDesign | Adobe InDesign 2026, version 21.5.1.73 (scripting version 21.5) |
| Other Adobe hosts available | InDesign 2025 (20.5.5), Illustrator 2026 (30.7.0), Acrobat DC |
| Windows | Windows 11 Pro 10.0.26200 |
| GPUs | NVIDIA GeForce RTX 4070 SUPER (driver 32.0.15.9636) and Intel UHD Graphics 770 |
| GPU Performance | On (`enableGpuPerformance: true`, animated zoom on) |
| Display performance | Default *High Quality*; the live window was at 300 % in GPU Preview with screen mode *Preview* and *Overprint Preview* off |
| Document | *Throwaway.indd*, created and last saved by InDesign 21.5 (XMP `CreatorTool`), one page, US Letter (215.9 × 279.4 mm), facing pages, print intent, one layer |
| Color | Transparency blending space CMYK; color settings *North America General Purpose 2*; CMYK working space *U.S. Web Coated (SWOP) v2* |
| Fonts | Minion Pro Regular (OpenType CFF, version 2.112), the only font used |

## Preservation

When the sprint started, InDesign reported the live document as saved and unmodified (`saved: true`, `modified: false`). Before any inspection, the live document was copied with `Document.saveACopy()` to *tests/indesign-stroke/corpus/pristine/fixture-pristine.indd* (1,249,280 bytes, SHA-256 `3797BA73746CD6BAC8CF55B689F0347E77CB622257651AC4AFA5F4430BA3A783`), which is marked read-only. A byte-identical working copy lives at *tests/indesign-stroke/corpus/working/fixture-working.indd*. Every inspection and export in this census opened the working copy without a window and closed it without saving. Each host job read the live document's name and saved state before and after, and they never changed.

## Inventory

The page holds nine page items on *Layer 1*. None of the text frames has a name, so the *Layers* panel lists them by their contents. The three polygons carry names that spell out the text they were outlined from.

| # | *Layers* panel entry | Class | Bounds (mm: left, top → right, bottom) | Role |
| --- | --- | --- | --- | --- |
| 1 | Rectangle (unnamed, locked) | Rectangle | 0, 0 → 215.9, 279.4 | Backdrop: Black fill at 50 % object opacity, so the white (*Paper*) strokes are visible |
| 2 | "Text on separate lines…" (id 250) | TextFrame | 45.5, 26.7 → 126.4, 42.7 | Fixture A, lower copy |
| 3 | "Text on separate lines…" (id 326) | TextFrame | identical to #2 | Fixture A, upper copy: an exact duplicate stacked on #2 |
| 4 | "Miter joints create awful spikes…" (id 302) | TextFrame | 45.5, 50.1 → 136.8, 66.1 | Fixture B |
| 5 | "Changing any paramater…" (id 274) | TextFrame | 46.4, 69.5 → 131.5, 88.9 | Fixture C |
| 6 | anchored-object story (id 396) | TextFrame | 45.5, 93.4 → 136.8, 109.4 | Fixture D host: three anchor characters in three paragraphs |
| 7 | "Turning any text to outlines (Type > Create Outlines)" | Polygon, inline in #6 | 46.2, 94.1 → 135.6, 98.2 | Fixture D, line 1 |
| 8 | "always loses the fill color, and the resulting stroke left" | Polygon, inline in #6 | 46.2, 99.2 → 135.4, 103.3 | Fixture D, line 2 |
| 9 | "is always the wrong size." | Polygon, inline in #6 | 46.2, 104.3 → 86.8, 108.4 | Fixture D, line 3 |

Shared formatting: every story uses *[Basic Paragraph]* with Minion Pro Regular 12 pt, auto leading at 120 % (14.4 pt), fill Black, stroke Paper, the Adobe Paragraph Composer, left alignment, and no character styles, nested styles, or GREP styles. No paragraph has shading or a border. The stroke alignment is Outside and the miter limit is 4, both inherited from *[No Paragraph Style]*; every local override is listed per fixture below. The frames use *[Basic Text Frame]* with zero insets, first baseline *Ascent*, one column, and no auto-sizing. No page item has effects applied, and only the backdrop has non-default transparency.

## Appearance matrix

| Output path | A | B | C | D | Evidence |
| --- | --- | --- | --- | --- | --- |
| Screen, GPU preview, 300 % | Yes | Yes | Yes | Yes | Screenshot from the fixture's author, *corpus/actual/screen/normal-view.jpg* |
| Screen, CPU preview, 300 % | Yes | Yes | Yes | Yes | *corpus/actual/screen/cpu-preview.jpg* |
| Screen, *Overprint Preview*, 300 % | Yes | Yes | Yes | Yes | *corpus/actual/screen/overprint-preview.jpg* |
| Screen, GPU Performance off, 300 % | Yes | Yes | Yes | Yes | *corpus/actual/screen/gpu-perf-off.jpg*. All four screen views render the four defects identically, so the display path is not involved. |
| PDF, *[High Quality Print]* | Yes | Yes | Yes | Yes | Content-stream operators, MuPDF text trace, MuPDF render |
| PDF, *[PDF/X-4:2008]* | Yes | Yes | Yes | Yes | Paint trace identical to *[High Quality Print]*; renders differ by at most 1 level |
| InDesign raster export (PNG, 600 ppi) | Yes | Yes | Yes | Yes | Crops per fixture |
| PNG with *Simulate Overprint* | Yes | Yes | Yes | Yes | Geometry unchanged; colors shift by at most 51 levels |
| PostScript (EPS, ASCII) | Yes | Yes | Yes | Yes | Operator timeline of an export whose backdrop was made opaque in memory, so it was not flattened |
| Distilled or printed PDF | UNKNOWN | UNKNOWN | UNKNOWN | UNKNOWN | The PostScript already shows the same paint order; distilling it is still pending |
| After save and reopen | Yes | Yes | Yes | Yes | Every export above came from the reopened working copy |
| After an IDML round-trip | Yes | Yes | Yes | Yes | Paint trace identical; InDesign PNG and MuPDF render both differ by 0 px |

InDesign's raster export and MuPDF agree on geometry and differ mainly in color conversion (the backdrop renders as gray 127 against 144). MuPDF alone draws faint gray arcs at a few stroke extremities, for example right of "follows." in fixture A. InDesign's raster doesn't show them, so they are a MuPDF artifact and are excluded.

## Fixture A: adjacent-line stroke overlap

**Text.** "Text on separate lines with a stroke weight / that exceeds leading ends up being overlapped / by the stroke of the line that follows." Three paragraphs.

**Local overrides.** Stroke weight 7 pt, join Round, stroke color Paper.

**Intended behavior.** The stroke is a halo behind the text. No part of any glyph's fill should be covered by stroke, whichever line the stroke belongs to.

**Observed behavior.** Descenders of lines 1 and 2 (*p*, *g*) are clipped wherever the next line's halo passes over them. Ascenders are intact, and line 3 is intact. In the MuPDF render of the exported PDF at 600 dpi, 4,562 core fill pixels lie under white stroke.

**Duplicate frame.** Frames #2 and #3 are identical and coincide exactly. Both emit the same operators, and either one alone produces the same appearance. The fixture's author has confirmed the duplicate was a mistake, so it carries no meaning.

**Minimal properties (hypothesis, Phase B).** At least two lines in one story, with a stroke that extends past the gap between the lower edge of one line's glyphs and the upper edge of the next line's glyphs.

**Mechanism evidence.**

- PDF: each line is written as a stroke text object (`Tr 1`, `w 14`, `j 1`, `M 4`) immediately followed by the same line's fill text object (`Tr 0`). The sequence is line 1 stroke, line 1 fill, line 2 stroke, line 2 fill, and so on, in story order. The width is twice the 7 pt weight: outside alignment is emulated by a doubled centered stroke painted under the fill.
- PostScript: the same order. Each line builds its outline with `charpath`, strokes it with `14 lw 1 lj 4 ml @`, and then fills the line with `xshow`.
- Causal test: the text-show operators of fixture A were removed from the PDF and re-emitted as self-contained blocks. Re-emitting them in the original order is a 0 px control. Emitting all of A's stroke objects before all of its fill objects leaves 0 fill pixels hidden while the halo's outline stays the same.

**Layer.** PROVEN for PDF output: the defect is paint order, not composition geometry, because glyph positions are untouched and reordering alone removes it. STRONGLY SUPPORTED: the order is produced upstream of the output backends, in InDesign's text drawing, because PDF export, PostScript export, and InDesign's own raster export share it exactly.

## Fixture B: miter-join spikes

**Text.** "Miter joints create awful spikes that get increasingly / worse as the weight of the stroke increases." Two paragraphs.

**Local overrides.** Stroke weight 4 pt, stroke color Paper. The join (Miter), miter limit (4), and alignment (Outside) are inherited.

**Intended behavior.** Sharp corners should stay crisp without long spikes.

**Observed behavior.** Pointed spikes project from acute glyph corners. In the 600 dpi render the tallest spike reaches 17 px (about 2 pt) beyond the extent of the same stroke with a round join, and 15,701 white pixels lie outside the round-join halo.

**Mechanism evidence.**

- PDF: each line's stroke object sets `w 8` (twice the weight) and `M 4`; the join is the default miter (`j 0`). The text model carries `miterLimit: 4`, and IDML serializes it as `MiterLimit="4"` on *[No paragraph style]*.
- Miter-limit sweep, editing only the `M` operator on fixture B's stroke objects: `M 3`, `M 4`, `M 10`, and `M 100` render pixel-identical to the original. At `M 2` the tallest spike is beveled (the white extent drops from row 60 to row 64) and 13,805 pixels remain outside the round halo. At `M 1.414`, 5,800 remain, and `M 1` is identical to a bevel join.
- Reading of the sweep: every join in this text has a miter ratio (1 / sin(θ / 2)) below 3, so the default limit of 4 never bevels any of them. The visible spikes come from corners with ratios between about 1.4 and 3, that is, interior angles between about 39° and 90°.

**Layer.** PROVEN: the spikes are standard PDF miter joins governed by the emitted miter limit, and lowering `M` alone removes them progressively. The miter limit is a document attribute exposed to scripting and IDML, so whether setting it on text changes the emitted `M` is a Level 1 candidate for Phase B. Because outside alignment doubles the emitted width, the spike length scales with twice the visible weight: a join can reach up to limit × weight beyond the glyph corner.

## Fixture C: character-run stroke fragmentation

**Text.** "Changing any paramater of an individual glyph / always makes its stroke decorrelate itself from / the stroke applied to the text surrounding it." Three paragraphs; "paramater" is spelled that way in the fixture.

**Local overrides.** On the whole story: stroke weight 3 pt, join Round, stroke color Paper. Thirteen single-character overrides:

| Character | Override |
| --- | --- |
| *g* in "Changing" | Horizontal scale 95 % |
| *a* in "any" | Skew −6° |
| *m* in "paramater" | Fill Magenta |
| *a* in "an" | Baseline shift 1 pt |
| *n* in "individual" (first) | Superscript |
| *u* in "individual" | Vertical scale 105 % |
| *l* in "always" | Underline |
| *m* in "makes" | All Caps |
| *s* in "its" | Small Caps |
| *e* in "decorrelate" (first) | Lowercase, which changes nothing visually |
| *l* in "decorrelate" | Subscript |
| *e* in "itself" | Strikethrough |
| *m* in "from" | Stroke color Magenta |

**Intended behavior.** Formatting a character should affect only that character. Its halo should merge with the neighboring halo exactly as unformatted text does.

**Observed behavior.** At every boundary, the character before the override and the overridden character itself lose their right-hand portion under the next run's white halo ("Char ging", "a ny"). The no-op Lowercase *e* shows the same damage. In the MuPDF render, 6,239 core fill pixels lie under white stroke.

**Mechanism evidence.**

- PDF: the story's 27 style ranges, split by 3 lines, become 28 paint units (13 on line 1, 14 on line 2, 1 on line 3). Each unit is a stroke text object followed by its own fill text object. The Lowercase *e* is its own unit.
- PostScript: the same units, each built with `charpath`, stroked with `@`, and then filled with `show` or `xshow`.
- Causal test: painting all strokes of each line before that line's fills leaves 124 fill pixels hidden; painting all of the frame's strokes before all of its fills leaves 120. The residue is consistent with the underline, which is painted as a path before the frame's text and ends up under the strokes. That still has to be confirmed. The re-emission control differs from the original by 106 px.
- The halo itself is continuous in every variant: no stroke is missing or offset at a run boundary. What looks like decorrelation is a later unit's stroke painted over an earlier unit's fill.

**Layer.** PROVEN for PDF output: the fragmentation is paint-order interleaving at style-range boundaries, the same mechanism as fixture A at a finer unit. Which attributes create a boundary, and whether an attribute that affects neither composition nor painting still does, is Phase B's attribute sweep.

**Related observation.** The underline is drawn as a path before the frame's text, so it disappears under the halos. The strikethrough is drawn immediately after its own run, so it survives.

## Fixture D: Create Outlines

**Text.** Frame #6 contains only three anchor characters. The outlined lines, read from the polygons' names, say: "Turning any text to outlines (Type > Create Outlines) always loses the fill color, and the resulting stroke left is always the wrong size."

**Model state.** Each line is one compound polygon (783, 815, and 376 path segments). Fill Black with overprint fill on; stroke Paper, 4 pt, join Round, miter limit 4, stroke alignment Center (no `StrokeAlignment` attribute in IDML, which means the default). The anchor characters still carry a 4 pt Paper Outside stroke and 14.4 pt leading.

**Intended behavior.** Outlined text should look exactly like the live text it came from: the same halo size, with the fill visible.

**Observed behavior.** The outlines render as solid white shapes. The black fill is invisible, and the halo reaches only half as far as the live text's.

**Mechanism evidence.**

- PDF and PostScript: each polygon is filled (`f`) and then stroked with a centered 4 pt stroke (`4 w`, `1 j`, `S`), which paints over the fill.
- Live text with a 4 pt Outside stroke is emitted as an 8 pt centered stroke under the fill, so its halo reaches 4 pt beyond the glyph edge. The outline's centered 4 pt stroke reaches only 2 pt beyond the edge and covers 2 pt inside it. Minion Pro's stems are about 0.95 pt wide at 12 pt (`StdVW 79` per 1,000 units), so the inner half of the stroke covers every stem completely.
- The fill color is still Black in the model. "Loses the fill color" is an appearance effect, not a change to the document model.

**Layer.** PROVEN on this fixture: after conversion, stroke alignment is Center (not Outside) and the stroke is painted over the fill (not under it). Whether *Type > Create Outlines* itself produces that state, rather than a later edit, is for Phase B to reproduce.

## Cross-cutting finding

InDesign emulates Outside text-stroke alignment by painting a doubled centered stroke under the fill, one paint unit at a time. A paint unit is a style range within a line. That single implementation choice accounts for three of the four fixtures:

- A and C: the "stroke under fill" guarantee holds only inside a unit, so each later unit's stroke covers earlier units' fills. STRONGLY SUPPORTED as one mechanism at the time of this census; Phase B's attribute and scope tests have since PROVEN it (*STROKE_MECHANISMS.md*, M3 and M4).
- B: the doubled width was suspected of making text spikes longer than page-item spikes with the same settings. Phase B DISPROVED this: for mitered joins both reach weight / sin(θ / 2) (*STROKE_MECHANISMS.md*, M5). B is a separate mechanism: standard miter joining at the text's miter-limit attribute.
- D: a path has no equivalent of "stroke under fill", so the conversion falls back to a centered stroke over the fill. Phase B reproduced the conversion in a clean document and PROVED it: Center alignment, half the painted width, and overprint fill (*STROKE_MECHANISMS.md*, M6).

## Open items

1. Distilling the PostScript to check the printed path end to end.

On-screen behavior is settled: the author's screenshots show all four defects in the GPU preview, the CPU preview, *Overprint Preview*, and with GPU Performance off. Phase B and the phases after it are covered by *STROKE_MECHANISMS.md* and *INDESIGN_STROKE_INVESTIGATION.md*.
