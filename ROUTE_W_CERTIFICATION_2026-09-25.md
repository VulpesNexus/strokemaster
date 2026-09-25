# Route W certification, 2026-09-25

Route W is StrokeMaster's InDesign plugin route that draws each Outside-stroked text run's stroke in an adornment pass of its own before the frame's text, so a line's stroke no longer covers another line's fill. This document certifies what it does, where, and where it stops. The machine-readable matrix is [route_w_certification.json](tests/audit/results/route_w_certification.json), built by `tools/certify_routew.py summarize` from the session records in [tests/audit/results/routew-cert](tests/audit/results/routew-cert).

Statuses: **CERTIFIED** (repeated in two route W sessions, with the native control damaged in the same output), **CERTIFIED_WITH_VARIANCE** (functionally correct, measurably different from native at edges), **CERTIFIED_FOR_APPEARANCE** (correct pixels in an output that keeps no live text, with or without route W), **PASS** (0 damage where the control or the second session was not run), **MANUAL_PASS**, **UNSUPPORTED**, **FAILED**, **BLOCKED**, **UNTESTED**, **INCONCLUSIVE**.

## 1. Executive verdict

**ROUTE_W_CORE: CERTIFIED.** Within one text frame, route W paints every Outside-stroked run's stroke before any of that frame's fills. On the 20-case victim census, every certified output has 0 damaged fill-core pixels in both route W sessions, and the native control loses 48,176–48,778 fill-core pixels in the same outputs. The correction holds in live PDF (1.4 through 1.7, and PDF/X-4), flattened PDF (PDF 1.3, including PDF/X-1a and X-3), EPS, printed PostScript, and PNG at every resolution and setting tested. It also survives save and reopen, IDML, packaging, editing, undo and redo alike, and a scripted screen-drawing stress. The document keeps no route W state.

**ROUTE_W_EDGE_FIDELITY: CERTIFIED_WITH_VARIANCE.** Route W's stroke has exactly the font's outline geometry, but it is rasterized as a path, not as a text stroke. Edges differ from InDesign's own stroke by up to 106 of 255 levels, only within two device pixels of an edge, at every resolution.

**ROUTE_W_PDF_LEGACY_FLATTENED: CERTIFIED** where the flattener keeps text (PDF/X-1a, PDF 1.3 flattening), and **CERTIFIED_FOR_APPEARANCE** where the flattener rasterizes everything (raster-vector balance 0), which it does with or without route W.

**ROUTE_W_SCREEN: PASS** (scripted redraw stress, one session) and **MANUAL_PASS** (the owner's look at 400%).

**ROUTE_W_CROSS_FRAME: UNSUPPORTED.** Strokes of one frame over another frame's fills are not reordered, and that includes the frames of one threaded story.

**Addendum, 2026-09-26.** The cross-frame sprint ([CROSS_FRAME_RESEARCH_2026-09-26.md](CROSS_FRAME_RESEARCH_2026-09-26.md)) corrected two boundaries of this certification:

- **Parcel, not frame.** Route W's scope is the parcel: two columns of one frame are separate, and their overlap stays native (2,461 pixels).
- **Background export.** Every PDF measured here was exported in the foreground by script. File > Export > Adobe PDF exports in the background by default, and the certified build, a UI plugin, is not loaded on that thread, so that export is drawn natively.

Route X (`1e7c417e…`), a model plugin, extends the correction across the frames and columns of one story and also runs in background export. Everything certified here holds for it unchanged.

**STROKEMASTER_MITER: OUT_OF_SCOPE, unchanged.** **STROKEMASTER_CENTER_ALIGNMENT: OUT_OF_SCOPE, unchanged.**

## 2. Binary and build identity

| Item | Value |
| --- | --- |
| Route W plugin | *InDesignStrokeEngine.pln*, 27,136 bytes, SHA-256 `f31dd2c9c7a4f7bd94ea9c9c1c48b19ba33fec76f2c3332730d1d82d7c4f4215`, built by *plugin/build-plugin.ps1 -RouteW* into *build/Release-RouteW* |
| Resources | *(InDesignStrokeEngine Resources)*: 500.idrc `fb2c576e…`, 1.idrc `af5570f5…`, 550.idrc `8317edf2…`, IDPL 1.idrc `a9be1211…`, 450.idrc `fed8f94c…` |
| Control (refusal build) | 17,920 bytes, `15b802ebfb38f567eb3aa586e995c0a1afff47efd078f3e2a0443596b54df4c4`, in *build/indesign-install/20260925T105031Z-backup* |
| Installed location | *C:\Program Files\Adobe\Adobe InDesign 2026\Plug-Ins\InDesignStrokeEngine* (the host's own folder) |
| Loaded module | read from the running process in sessions B (`15b802eb…`) and A2 (`f31dd2c9…`) |
| Host | InDesign 21.5.1.73; Illustrator 30.7.0 for the round trip; Acrobat Distiller 24.5 and Ghostscript 9.23 for conversions; MuPDF 1.28.2 as the PDF renderer |
| Commits | route W implementation `77b08e9`, host evidence `c50e05f`, `6c27a40`, `439457d`, `98b87e7`, `688d3b0` (unchanged by this certification); certification `16b4f1a`, `191702d`, `f3870aa`, `35e0714`, and this document's commit |
| Tests at start | 90 geometry, 32 audit, 310 Node, all passing |

Sessions: **A1**, route W in the InDesign process the owner had open since 17:50. **B**, the refusal build, installed by the owner from an elevated shell, in a fresh process this certification launched. **A2**, route W reinstalled (hash-verified), in a fresh process. The owner ran both elevated installs; each printed "Installed and verified".

## 3. Corpus identity

Frozen in [tests/certification/corpus](tests/certification/corpus/README.md); every job copies a fixture before opening it.

| Fixture | Cases | SHA-256 |
| --- | --- | --- |
| *victims.indd* | the 20-case census (VC00–VC19), with its split reference and fill mask | `f3270a31…` (identical to the census record) |
| *cert-T.indd* | T00–T11: text, object, and group opacity; transparent art above and below; Multiply and Screen; drop shadow; basic and gradient feather; clipping | `28fe6428…` |
| *cert-C.indd* | C00–C07: Paper over black, process, spot, spot on spot, RGB, tints, gradient stroke, overprinted stroke | `b1d134dd…` |
| *cert-E.indd* | E00–E11: threaded pair, its one-frame control, independent overlapping frames (both orders), mixed runs and fonts, overset, rotation, scale, shear, table cell, anchored frame, text on a path | `5498dcb6…` |
| Saved suites | A, B, C, N, and S under *tests/indesign-stroke*, as committed | per-file hashes in the records |

## 4. Certification matrix

Fill-core damage: pixels inside the eroded fill mask more than 64 levels from the fill-only twin (the same document with every text stroke set to [None]), rendered by the same renderer at the same resolution. "W" is route W (A1 and A2), "N" the native control (B). The full matrix has 114 rows.

| Route | Variant | 0 core damage (W) | N damage | Live text | Vector | Fonts | Paint order | Verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| INDD live session | victims, all exports | yes (A1, A2) | 48,176–48,778 | yes | yes | embedded subset | strokes first | CERTIFIED |
| INDD save and reopen | same process and fresh process | yes | native drawing returns | yes | yes | yes | strokes first | CERTIFIED |
| IDML round trip | INDD saved as IDML, opened, saved as INDD, reopened | yes | native drawing returns | yes | yes | yes | strokes first | CERTIFIED |
| Package | packaged copy with fonts | yes | not run | yes | yes | yes | strokes first | PASS |
| Screen, normal redraw | owner at 400% | by eye | — | — | — | — | — | MANUAL_PASS |
| Screen, stressed redraw, then save and reopen | 60 calls, 4 saves mid-redraw | yes | — | yes | yes | yes | strokes first | PASS |
| PDF 1.4 live | [High Quality Print] | yes | 48,778 | 84 glyphs (native 142) | yes | yes | strokes first | CERTIFIED |
| PDF/X-4 (1.6 live) | [PDF/X-4:2008] | yes | 48,777 | 84 (native 142) | yes | yes | strokes first | CERTIFIED |
| PDF/X-1a (1.3 flattened, CMYK) | [PDF/X-1a:2001] | yes | 48,682 | 92 (native 138) | yes | yes | strokes first | CERTIFIED |
| PDF 1.3 flattened, full raster | custom flattener, balance 0 | yes | 48,239 | none, either way | rasterized, either way | none | — | CERTIFIED_FOR_APPEARANCE |
| Other PDF families | Press, Smallest, X-3, Acrobat 4 and 8, gray, RGB, editable, two user presets | yes | not run | yes | yes | yes | strokes first | PASS |
| PNG | 72–600 ppi, with and without anti-aliasing, transparent, gray | yes | 11,036 (300), 48,176 (600) | — | — | — | — | CERTIFIED at 300 and 600, PASS elsewhere |
| EPS | CMYK level 2 | yes | 48,689 | — | yes | — | — | CERTIFIED |
| Print to PostScript | [High Resolution] flattener | yes | 48,689 | — | yes | — | — | CERTIFIED |
| PostScript and EPS converted to PDF | Distiller (COM), Ghostscript pdfwrite | yes | not run | 81–90 glyphs | yes, except the low-resolution flattener's one image | — | — | PASS |
| PDF placed into InDesign | HQP, X-4, X-1a, EPS placed and re-exported | yes | not run | placed art | yes | yes | — | PASS |
| PDF opened in Illustrator | HQP, X-1a, X-4 | yes | not run | editable, 45–48 frames | yes | Minion Pro | stroke as separate paths | PASS |
| Illustrator re-save, editing capabilities off / on / reopened | all three | yes | not run | yes | yes | yes | — | PASS |
| Editable PDF reopened in InDesign | *preserveInDesignEditingCapabilities* | yes (72 ppi) | not run | yes (document) | yes | yes | — | PASS |
| Acrobat preflight, optimize, conversion | — | — | — | — | — | — | — | UNTESTED |

## 5. 20-case victim results

Fill-core damage in pixels at 600 ppi; the ring is route W against the split reference (A2, the corrected reference).

| Frame | Case | Core px | Native PNG / PDF / EPS | Route W A1 PNG / PDF / EPS | Route W A2 PNG / PDF / EPS | Ring PNG: px, max, beyond band | Ring PDF: px, max |
| --- | --- | --- | --- | --- | --- | --- | --- |
| VC00 | unstroked first line, stroked second | 11,600 | 3,762 / 3,814 / 3,808 | 0 / 0 / 0 | 0 / 0 / 0 | 1,346, 91, 0 | 1,858, 46 |
| VC01 | stroked first line, unstroked second | 11,600 | 0 / 0 / 0 | 0 / 0 / 0 | 0 / 0 / 0 | 1,941, 106, 0 | 2,090, 65 |
| VC02 | both stroked | 11,600 | 3,762 / 3,814 / 3,808 | 0 / 0 / 0 | 0 / 0 / 0 | 1,785, 106, 0 | 2,376, 64 |
| VC03 | both unstroked | 11,600 | 0 / 0 / 0 | 0 / 0 / 0 | 0 / 0 / 0 | 0, 0, 0 | 0, 0 |
| VC04 | alternating three lines | 16,956 | 3,762 / 3,814 / 3,808 | 0 / 0 / 0 | 0 / 0 / 0 | 1,346, 91, 0 | 1,770, 46 |
| VC05 | different stroke weights | 11,600 | 3,762 / 3,814 / 3,808 | 0 / 0 / 0 | 0 / 0 / 0 | 1,597, 106, 0 | 2,117, 80 |
| VC06 | different stroke and fill colors | 11,600 | 0 / 0 / 0 | 0 / 0 / 0 | 0 / 0 / 0 | 2,173, 77, 0 | 2,464, 37 |
| VC07 | None with positive stored weight | 11,600 | 3,762 / 3,814 / 3,808 | 0 / 0 / 0 | 0 / 0 / 0 | 1,345, 91, 0 | 1,858, 46 |
| VC08 | zero weight with Paper stroke color | 11,600 | 3,762 / 3,814 / 3,808 | 0 / 0 / 0 | 0 / 0 / 0 | 1,345, 91, 0 | 1,858, 46 |
| VC09 | 0 percent text opacity | 11,600 | 0 / 0 / 0 | 0 / 0 / 0 | 0 / 0 / 0 | 0, 0, 0 | 0, 0 |
| VC10 | character mixed stroke states | 11,600 | 2,875 / 2,905 / 2,901 | 0 / 0 / 0 | 0 / 0 / 0 | 2,003, 106, 0 | 2,335, 73 |
| VC11 | thin benign round strokes | 12,488 | 0 / 0 / 0 | 0 / 0 / 0 | 0 / 0 / 0 | 5,392, 106, 0 | 7,636, 61 |
| VC12 | thin benign miter strokes | 12,488 | 0 / 0 / 0 | 0 / 0 / 0 | 0 / 0 / 0 | 5,382, 106, 0 | 7,654, 61 |
| VC13 | thick nonoverlapping lines | 12,488 | 0 / 0 / 0 | 0 / 0 / 0 | 0 / 0 / 0 | 4,066, 106, 0 | 4,356, 65 |
| VC14 | underline decoration | 11,600 | 3,762 / 3,814 / 3,808 | 0 / 0 / 0 | 0 / 0 / 0 | 23,693, 164, 17,008 | 24,546, 132 |
| VC15 | strikethrough decoration | 11,600 | 3,762 / 3,814 / 3,808 | 0 / 0 / 0 | 0 / 0 / 0 | 1,785, 106, 0 | 2,376, 64 |
| VC16 | stroke tint zero | 11,600 | 3,762 / 3,814 / 3,808 | 0 / 0 / 0 | 0 / 0 / 0 | 1,783, 106, 0 | 2,376, 64 |
| VC17 | None plus zero weight | 11,600 | 3,762 / 3,814 / 3,808 | 0 / 0 / 0 | 0 / 0 / 0 | 1,940, 91, 0 | 2,631, 80 |
| VC18 | rotated frame | 11,379 | 3,733 / 3,729 / 3,707 | 0 / 0 / 0 | 0 / 0 / 0 | 2,805, 99, 0 | 3,408, 80 |
| VC19 | baseline shift boundary | 11,795 | 3,948 / 4,004 / 4,001 | 0 / 0 / 0 | 0 / 0 / 0 | 1,787, 106, 0 | 2,373, 64 |

Thirteen frames are damaged natively; none with route W. VC14's large "beyond band" count is an oracle limit known since the census: the split reference paints the underline over the stroke, while InDesign draws it beneath the text stroke with or without the plugin. Every one of the 45 outputs A1 and A2 share renders pixel-identically between the two sessions, across a reinstall and a new process.

## 6. Structural PDF findings

From *tools/routew_measure.py* `pdf_structure` (pikepdf content-stream census and MuPDF paint order), victim census:

- **Text stays text, once.** Route W's PDFs carry 84 fill-mode glyphs for the census's 84 characters (plus one 2-glyph stroke-mode run in VC08, whose first line has a 0 pt Paper stroke: route W has nothing to draw there and leaves InDesign's own zero-width stroke, as native does). Native InDesign writes 142 in live PDF and 138 in PDF/X-1a, because it emits each stroked line twice, as a stroke-mode copy and a fill-mode copy. Flattened route W PDFs carry 92: the flattener splits some runs, and emits one clip-mode run (text render mode 7).
- **Fonts:** Minion Pro, embedded as a subset, in every PDF family except the fully rasterized one, which has no text with or without route W.
- **Paint order:** in every frame, route W's strokes are path strokes painted before the frame's first fill-mode glyph. The only strokes after a fill are VC15's two 5 pt strikethrough bars, which InDesign always draws on top.
- **Geometry of the strokes:** each stroke path's points equal the embedded font program's outline points at the fill glyph's origin and size, to within 0.00002 pt (112 of 112 points per frame). Widths are exactly double the run's weight (24, 6, and 0.2 pt), with the run's own join.
- **Rasterization:** 0 images in every PDF family except the custom balance-0 flattener (14 images, no text, with and without route W).
- **Transparency:** live families keep one transparency group and one soft-mask graphics state (VC09); flattened ones have none.
- **Center alignment stays native:** in the saved suites, the only frames that still carry InDesign's stroke-mode text with route W are the Center-aligned ones (A04, B2, C09's Center character, N20, S05c, S05t); natively every stroked frame does.

## 7. Save and reopen, and IDML

- **INDD, same process (A1):** saved as a copy, reopened, and exported. The result is pixel-identical to the direct export (0 differing pixels) with 0 damage.
- **INDD, fresh process (A2):** the documents saved in A1 (plain, converted from IDML, and both edited states) reopened in a new process after a reinstall. All are pixel-identical to their A1 exports, with 0 damage in scope.
- **Native control (B):** the same saved documents render natively, with the full damage (48,176 pixels for the plain save). The documents carry nothing of route W. No saved INDD and no IDML entry contains the plugin's ID prefix (`0x5A200`, either byte order) or its name.
- **IDML:** the census saved as IDML, opened, saved as INDD, and reopened is pixel-identical to the direct export, with 0 damage. Route W keeps no persistent data, so there is nothing for IDML to lose. It reads InDesign's own stroke attributes at every draw.
- **Package:** the packaged copy, with its fonts, exports identically (PASS; one session).

## 8. Interactive-redraw persistence

- **MANUAL_PASS (owner, earlier today):** VC00 at 400% on screen showed the first line whole, and repeated zooming and scrolling interrupted redraws. That check was by eye, with no capture ([screen record](tests/audit/results/indesign-routew-screen.json)). The document's exports and a saved copy afterward matched the pre-screen census export pixel for pixel.
- **PASS (scripted, A2):** *tools/cert-redraw.ps1* opened a copy of the census in a window of the InDesign session it launched. It sent 60 separate scripting calls (zoom 12.5–3200%, fit, zoom in, display quality, preview mode, minimize and restore, and a VC05 stroke-weight toggle), each followed by 0–150 ms, so the next call arrives while drawing may be in progress. One call returned no value and is logged. Every 15 steps it saved a copy with no pause.
  - All four copies, one saved while VC05 stood at 8 pt, reopened without a window and exported. Each is pixel-identical to the export of its intended state, with 0 damage.
  - The exports from the window itself are pixel-identical to the direct export.
- **Not verifiable from outside:** whether any single draw was actually cut short between route W's mute and restore passes. A stroke left muted would show in every output measured, and none does. Screen pixels themselves were not captured: an automated capture was blocked by antivirus earlier, and it was not bypassed.

## 9. PDF compatibility and preset results

The installed host offers Acrobat 4–8 compatibility, the PDF/X standards 1a:2001, 3:2002, 1a:2003, 3:2003, and 4:2010 (shown as [PDF/X-4:2008]), and the flatteners [Low Resolution], [Medium Resolution], and [High Resolution]. Route W's victim damage is 0 in both sessions for all 15 PDF families below.

| Family | PDF | Transparency | Color | Glyphs (W / N) | Verdict |
| --- | --- | --- | --- | --- | --- |
| [High Quality Print] | 1.4 | live | unchanged | 84 / 142 | CERTIFIED |
| [PDF/X-4:2008] | 1.6, X | live | unchanged | 84 / 142 | CERTIFIED |
| [PDF/X-1a:2001] | 1.3, X | flattened, [High Resolution] | CMYK (SWOP) | 92 / 138 | CERTIFIED |
| HQP at Acrobat 4, balance-0 custom flattener | 1.3 | fully rasterized | unchanged | 0 / 0 | CERTIFIED_FOR_APPEARANCE |
| [PDF/X-3:2002] | 1.3, X | flattened | unchanged | 92 | PASS |
| [Press Quality] | 1.4 | live | CMYK | 84 | PASS |
| [Smallest File Size] | 1.5 | live | RGB, downsampling | 84 | PASS |
| HQP at Acrobat 4 | 1.3 | flattened (preference flattener) | unchanged | 92 | PASS |
| HQP at Acrobat 4, text-to-outlines custom flattener | 1.3 | flattened | unchanged | 92 (the flattener outlines only text in transparent areas) | PASS |
| HQP at Acrobat 8 | 1.7 | live | unchanged | 84 | PASS |
| HQP, gray | 1.4 | live | gray | 84 | PASS |
| HQP, RGB | 1.4 | live | RGB | 84 | PASS |
| HQP, InDesign editing capabilities | 1.4 | live | unchanged | 84 | PASS |
| User preset U1 | 1.3, X-1a | flattened | CMYK (FOGRA39) | 92 | PASS |
| User preset U2 | 1.5 | live | RGB | 84 | PASS |

User presets are named only in an ignored local file. The color fixture (C00–C07: spot, spot on spot, RGB, tints, gradient stroke, overprint) is 0 in every family measured, against 18,709–18,838 native. Output color conversion does not change route W's behavior.

## 10. Flattening and transparency results

Fill-core damage in scope per T frame, route W / native, over PNG 600, PDF 1.4, PDF/X-4, PDF/X-1a, Acrobat 4, balance 0, EPS, and PostScript: **T00–T07 and T09 are 0 with route W in every output; natively 3,712–3,814 each.** That covers text, object, and group opacity, transparent art above and below, Multiply, drop shadow, and a frame clipped by a rectangle.

Three frames need their own oracle, recorded in the matrix under *excluded_frames*:

- **T08, basic feather:** the fill-only twin is feathered as a different shape (thin stems fade), so it is not an oracle. With the cell-shift oracle (T08 against the unfeathered T00 in the same file, 1,625 px apart at 600 ppi), route W's T08 core equals T00's: 0 pixels over 64 in PDF 1.4 and balance-0 flattening. PDF/X-1a shows 44 pixels in one horizontal line at 110 against the fill's 31, a flattener seam, not the stroke's 255. PASS.
- **T10, Screen over blue:** in CMYK blending, black screened over blue is white, so both orders give a white fill and the live outputs show 0 either way. Under flattening the stroke-free twin composites differently (blue), but route W equals native pixel for pixel in the core (0 over 64). Not discriminating; route W changes nothing there.
- **T11, gradient feather:** the text sits where the object is faded out; native also measures 0. INCONCLUSIVE.

Flattened route W output keeps live text (116 glyphs against native 68, with the flattener's 19 raster pieces for the effects). The balance-0 flattener rasterizes the whole page with or without route W, so that route is certified for appearance only, and the loss of editability belongs to the workflow.

## 11. EPS and PostScript results

- **EPS** (CMYK level 2 and RGB level 3) and **print to PostScript** (Adobe PDF PPD, custom paper at page size, the [High Resolution] and [Low Resolution] flatteners, and composite gray) have 0 damage for the victims in both sessions. Natively EPS and [High Resolution] PostScript lose 48,689 pixels. The certified ones are EPS level 2 and [High Resolution] print; the rest PASS.
- **Downstream to PDF:** route W's [High Resolution] and [Low Resolution] PostScript went through Acrobat Distiller 24.5 (COM *FileToPDF*, High Quality Print job options) and Ghostscript 9.23 pdfwrite, and its EPS through Ghostscript. All five PDFs have 0 damage and keep text (81–90 glyphs). The [Low Resolution] print flattener leaves one raster image in both conversions.
- Distiller's command line hung on the PDF/X-1a job options, apparently at a prompt. It was stopped, and its one registry change (a new window placement) was removed. The COM route was used instead and left the registry unchanged.

## 12. Round-trip and conversion results

- **PDF placed back into InDesign:** HQP, X-4, and X-1a PDFs, and the EPS, placed into a new 800 × 1000 pt document and exported again: 0 damage against their fill twins taken the same way. They differ from the direct export only in a one-pixel line at the page edge, a few edge pixels in VC11, VC12, and VC15, and VC18's rotated content (1,112 pixels for X-1a). The content becomes placed art: InDesign no longer edits it as text.
- **Editable PDF reopened in InDesign:** a PDF exported with *preserveInDesignEditingCapabilities* opens in InDesign 21.5 as a document with 20 frames and 20 stories. VC00's first line is unstroked, as in the source. Its export is pixel-identical to route W's direct export (72 ppi, the only resolution captured) with 0 damage.
- **Distiller and Ghostscript:** see section 11.

## 13. Illustrator interoperability

Illustrator 30.7.0, launched under the workspace guard and quit afterward, with no workspace file changed. It opened route W's HQP, X-1a, and X-4 PDFs and their fill twins, then saved each as PDF with *preserveEditability* on and off. It reopened the editable save and saved again.

- **Appearance:** all nine route W outputs have 0 damage against their twins taken through the same steps. They differ from the InDesign export only in VC18, the rotated frame: 817 pixels over 64, and 1,634 after the reopen.
- **What Illustrator makes of it:** 45 text frames (48 from X-1a), all Minion Pro, and editable. Route W's strokes arrive as 67 separate stroked paths (63 from X-1a), plus 29 compound paths and 20 clipping groups. In Illustrator the stroke is no longer tied to the text, so an edit to the text there does not move its stroke. That is a semantic change of the conversion, not a visual one.
- **Editing capabilities:** the editable save is about three times the size of the plain one (89,400 against 28,214 bytes for HQP). Reopening it rebuilds the identical inventory, and the re-save keeps 0 damage.

## 14. Edge-fidelity characterization

The ring is **rasterization, not geometry** (DERIVED):

1. **Geometry is identical.** Route W's stroke points equal the font outline to 0.00002 pt, at exactly double weight with the run's own join and cap ([ring.json](tests/audit/results/routew-cert/ring.json)).
2. **The difference stays in the edge pixel.** Against the corrected split reference (A2), over 16 levels, by distance from the nearest reference edge:

| Rendering | Differing px | Over 64 | Max | 0 px / 1 px / 2 px / ≥ 3 px from an edge | Over 64 beyond 2 px |
| --- | --- | --- | --- | --- | --- |
| InDesign PNG 72 ppi | 1,606 | 0 | 51 | 319 / 179 / 2 / 0 | 0 |
| InDesign PNG 150 ppi | 4,582 | 259 | 79 | 1,023 / 963 / 0 / 18 | 0 |
| InDesign PNG 300 ppi | 8,913 | 461 | 88 | 3,630 / 791 / 0 / 0 | 0 |
| InDesign PNG 600 ppi | 20,395 | 2,505 | 106 | 7,137 / 5,303 / 52 / 1 | 0 |
| InDesign PNG 300 ppi, no anti-aliasing | 1,750 | 1,702 | 106 | 1,562 / 165 / 0 / 0 | 0 |
| InDesign PNG 600 ppi, no anti-aliasing | 6,084 | 5,656 | 106 | 3,022 / 2,710 / 0 / 0 | 0 |
| MuPDF, HQP, 150 dpi | 5,988 | 38 | 105 | 1,391 / 796 / 8 / 114 | 0 |
| MuPDF, HQP, 600 dpi | 26,603 | 8 | 80 | 9,284 / 4,841 / 24 / 4 | 0 |
| MuPDF, HQP, 2400 dpi | 103,036 | 884 | 84 | 55,983 / 5,878 / 14 / 415 | 0 |

(Frames VC02, VC05, VC06, VC11, VC12, VC13.) The count roughly doubles with each doubling of resolution, as edge length does, while the width stays at the edge pixel. A geometric offset would widen in device pixels as resolution rises, and it doesn't. Both InDesign's rasterizer and MuPDF show the same pattern, so text-stroke rendering and path-stroke rendering treat the same outline differently at the boundary pixel. Where inside each rasterizer (flattening tolerance, stroke adjustment, glyph-path handling) is a HYPOTHESIS. No small, provably safe correction presented itself, and route W is left unchanged.

Correcting an earlier record: the census noted the ring on the benign frames VC11–VC13 (2,493–5,398 pixels) and attributed it to "InDesign's text stroker flattens or rasterizes the same outlines slightly differently" as a hypothesis. The geometry half of that is now excluded.

## 15. Performance

Median export times in milliseconds, 15 timed repetitions after two warm-ups, interleaved ([bench.json](tests/audit/results/routew-cert/bench.json)):

| Session | Census PDF | PNG 300 | PNG 600 | 20-page PDF | 20-page PNG 150 |
| --- | --- | --- | --- | --- | --- |
| A1, route W, the owner's long-running process | 1,166 | 818 | 2,227 | 4,987 | 7,309 |
| B, native, fresh process | 1,156 | 1,200 | 3,365 | 6,410 | 14,798 |
| A2, route W, fresh process | 1,147 | 1,385 | 3,661 | 6,237 | 12,888 |

**No distinguishable overhead in this benchmark.** PDF export medians agree within 2% across all three sessions. PNG times differ by up to 70% between the two route W sessions alone, far more than route W against native. B also ran while this certification rendered measurements on the same machine, and other sessions ran background work throughout. A tighter bound needs a quiet, alternating A/B run, which needs two more elevated installs. Redraw time was not measured.

## 16. Preference-state audit

- **The gate:** *tools/jsx/cert-enumerate.jsx* reads every readable property of the PDF, PNG, and EPS export preferences, PDF placement, EPS import, transparency, display, and GPU preferences, plus the preset lists. `tools/certify_routew.py prefs-diff` compares two snapshots and fails on any change; unit tests cover it.
  - Snapshots were taken before any certification job, after A1, at the start and end of B (B's start confirms the preferences persisted through a restart), and at the end of A2.
- **Every script job ran under *lib.run*,** which snapshots and restores the export preferences plus any the job names (PDF placement for place jobs), on success and on a thrown error. Node tests now cover the error path, the extra preference objects, and named preset references, which were previously reported unverified.
- **One leak found and fixed:** A2's end snapshot showed 20 changed PDF preferences. The cause is *tools/cert-redraw.ps1*'s export from the open window, written outside *lib.run*.
  - Exporting with the [High Quality Print] preset copies the preset into *app.pdfExportPreferences*. It also turns on *preserveInDesignEditingCapabilities*, a property the preset object does not expose (confirmed with a probe inside *lib.run*).
  - All readable values were restored before InDesign quit. The monochrome threshold needed its hidden sampling resolution lowered first, as *lib.run* does. The gate then reported no change.
  - The script now exports through *tools/jsx/ise-export-open.jsx*, under *lib.run*, and a verification run left the preferences unchanged.
- **Residual:** 19 PDF export properties can't be read while their feature is off (security passwords, and sampling resolutions while sampling is off). They were never readable before or after, so their pre-certification values are unknown; the restore of the monochrome threshold set one of them to 450.
- **Workspace:** every InDesign and Illustrator session this certification launched ran under *.workspace/tools/adobe-workspace-guard.ps1*. The guard put back InDesign's *Start_CurrentWorkspace.xml* after B, and *Start_CurrentWorkspace.xml* and *Essentials_CurrentWorkspace.xml* after A2. Illustrator's session changed nothing.

## 17. Stability audit

No InDesign or Illustrator application error, crash, or hang event in the Windows Application log on 2026-09-25, and no InDesign crash report. Three InDesign processes ran every certification job to completion, including the edit batteries, the benchmarks, and the redraw stress; one Illustrator process ran the round trip. Repetition accumulates no state: five edit, save, close, and reopen cycles alternate exactly between two drawings, and 23 undo and 23 redo steps (13 and 13 on the scope fixture) return exactly to the original and the edited drawing.

## 18. Explicit unsupported cases

- **Cross-frame overlap: UNSUPPORTED.** One frame's stroke over another frame's fill stays as native, in every output, with and without route W. Measured cases:
  - Independent overlapping frames in both stacking orders: E02 and E11, 3,762 pixels each.
  - A threaded story whose last line in one frame meets the first line of the next: E00, 4,214 pixels, against 0 for the same four lines in one frame (E01).
  - The saved suite's A06–A08.
  
  Route W's scope is the text frame's drawing, not the story.
- **Miter spikes: OUT_OF_SCOPE, unchanged.** Suite B, route W against native: no frame differs beyond the edge band, the ink bounding box moves by at most 1 pixel, and the Center-aligned miter frames are pixel-identical inside their own bounds.
- **Center-aligned strokes: OUT_OF_SCOPE, unchanged.** Route W leaves them to InDesign by construction: they are the only frames that still carry stroke-mode text. Inside their own bounds A04, B2, N20, S05c, and S05t are pixel-identical to native. C09's Center character is native, and its Outside characters are route W's.
- **Underline:** InDesign draws it beneath the text stroke with or without route W (VC14); not reordered.

## 19. Remaining risks

- **Corpus breadth:** one font (Minion Pro, plus Myriad Pro in E03), one page size, and synthetic documents. Tables, anchored frames, text on a path, and threads are covered by one case each; no real multi-page production document has been run.
- **Controls are family-representative, not exhaustive.** For the victims the native control ran for four of the fifteen PDF families (HQP, PDF/X-4, PDF/X-1a, and the balance-0 flattener), two of the eight PNG settings (300 and 600 ppi), EPS, and [High Resolution] PostScript; the rest are PASS, not CERTIFIED.
- **Single-session evidence:** the redraw stress, packaging, place-back, Illustrator, and conversions ran once each.
- **Renderers:** PDFs were judged by MuPDF, PostScript and EPS by Ghostscript. Acrobat's own renderer and third-party RIPs were not used.
- **Acrobat PDF processing** (preflight fixups, optimization, version conversion) was not run. PDF Optimizer has no scripting interface, and Acrobat's window state is not covered by the workspace guard.
- **Benchmark noise** hides anything smaller than about 70% on PNG export (section 15).

## 20. Exact next engineering target

**Frame-crossing order within threaded stories and overlapping frames (E00, E02, E11, A06–A08).** Route W's frame-local passes cannot reach a stroke drawn by a later frame over an earlier frame's fill. The next experiment is a page-level or spread-level adornment probe: register a draw-order observer on the spread and measure, with *cert-E.indd* and the harness here, whether all stroked runs of the overlapping frames can be drawn in a pass before any of their fills without breaking z-order against unrelated artwork. Its pass criterion is E00, E02, and E11 at 0 fill-core damage with every row of this matrix unchanged.

## Proposed support statement

> StrokeMaster's route W for InDesign 2026 (21.5.1) draws Outside-aligned text strokes behind the fills of the same text frame. It is certified for InDesign's live drawing, save, reopen, and IDML round trips; PDF export in live (PDF 1.4, PDF/X-4) and flattened (PDF/X-1a) form; EPS; printing to PostScript; and PNG. Text stays live, fonts stay embedded, and each stroked line is written once. Edges differ from InDesign's own stroke by up to one device pixel. Route W does not reorder strokes across separate or threaded frames, and it leaves Center-aligned strokes and miter joins as InDesign draws them.
