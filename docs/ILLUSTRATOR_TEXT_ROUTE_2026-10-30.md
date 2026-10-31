# Illustrator text route — October 30, 2026

Region-prep-22 rebuilds the story route, settles which native paint the text route writes, and makes the route follow a recolor. It also corrects a false pass in the October 29 record. Region-prep-23, the same day, writes the route's own paint pass as paths so a PDF's text is extracted once, and lays dashed strokes along one outline where contours overlap. Region-prep-24 corrects stories with artwork between their frames and comes with a certification runner and baseline. Host: Illustrator 30.8.2 through `Illustrator.Application.30`, in fresh processes on the isolated hidden desktop. Synthetic documents only.

## The October 29 story fill drew no strokes

Region-prep-21's `story-fill` wrote one fill into the group's style. That style had no explicit *Contents* entry, so Illustrator never drew the frames themselves, only the group's fill on the glyph outlines. The text came out filled and entirely unstroked. The fill-core metric looks only inside the letters, so it read this as zero damage. On the October 29 PDFs themselves, stroke ink outside the letters measures 5,569 to 8,452 pt² natively and 0 pt² under the story fill, in every font. [The October 29 report](ILLUSTRATOR_NATIVE_STORY_2026-10-29.md) and [its record](../tests/audit/results/illustrator-native-story-2026-10-29.json) now carry the withdrawal.

The fix sets the *Contents* position explicitly (`MoveGroupContentsPosition`) and checks it reads back. Every measurement below also checks the halo: outside the letters, the routed export must match the native one.

## One route decision

`native-route` (one frame) and `story-route` (a group holding complete stories) now share one decision, from the characters' paint across every frame they cover:

| Characters | Native paint written | A later recolor of the fills |
| --- | --- | --- |
| One stroke (any fills) | An object-level stroke below *Characters* (or the group's *Contents*); the characters' strokes cleared | Drawn with or without the plugin |
| Mixed strokes, one fill | An object-level fill above *Characters* (or *Contents*) | Hidden by that fill until the re-fix follows it (needs the plugin) |
| Both mixed | Per frame: ATE FillFirst off. A story group refuses | Drawn |

Until today, one fill came before one stroke. The recolor check below shows why the order matters. It also matches the owner's ruling that uniform text strokes stay with Illustrator's own Appearance stroke. The trade-off: under the stroke route, the frame has one miter limit for its object stroke rather than one per character.

## Measured

*tools/jsx/ai-story-route.jsx* (runner *tools/run-ai-story-route.ps1*, measurer *tools/measure_story_route.py*). Each case is a two-frame threaded story whose second frame overlaps the first's text: 38 pt, 7 pt strokes, 600 ppi, the fill core eroded by 3 px. Damage is any core pixel more than 48 off the stroke-free export on any channel; the halo is the same test outside the dilated letters.

Fill-core damage in pt², native → routed. The stroke-free check in parentheses is the halo, outside the letters, changed against the native export:

| Case | Route | Minion Pro | Segoe Script | Kerberos Fang | German Beauty | Great Vibes |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| One RGB fill, mixed strokes | group fill | 1,824.9 → 0 (0) | 2,668.6 → 0 (0) | 5,343.4 → 6.1 (1.2) | 4,298.4 → 0 (0) | 2,096.7 → 0.01 (0) |
| One CMYK fill | group fill | 1,816.8 → 0 (0) | 2,659.4 → 0 (0) | 5,319.0 → 2.7 (1.5) | 4,289.1 → 0 (0) | 2,079.0 → 0 (0) |
| One spot fill | group fill | 1,816.8 → 0 (0) | 2,659.4 → 0 (0) | 5,319.0 → 2.7 (1.5) | 4,289.1 → 0 (0) | 2,079.0 → 0 (0) |
| Mixed fills, one stroke | group stroke | 2,781.0 → 0 (0) | 4,026.3 → 0 (0) | 7,714.7 → 3.9 (1.2) | 6,322.7 → 0 (7.0) | 3,104.8 → 0 (0) |
| One fill, one stroke | group stroke | 2,764.6 → 0 (0) | 4,012.2 → 0 (10.8) | 7,676.9 → 2.3 (0.9) | 6,310.6 → 0 (7.0) | 3,079.0 → 0 (0) |
| Both mixed | refused, restored | 1,836.7 (unchanged) | 2,677.6 | 5,358.3 | 4,305.5 | 2,113.9 |
| Per frame, one fill | frame fills | 1,824.9 → 54.7 | 2,668.6 → 86.5 | 5,343.4 → 438.4 | 4,298.4 → 185.7 | 2,096.7 → 0.01 |

- Kerberos Fang's few pt² are its rough, anti-aliased edges.
- The halo changes under the group stroke (7 to 11 pt²) are miter tips: the group stroke carried the characters' miter limit. Since the review fixes, `native-fix` sets the group stroke's limit from every glyph of the story.
- The per-frame row is the route without the group: a later frame's strokes still reach an earlier frame's fills.

Every routed case kept its frame matrices and two-frame story. The fill-route cases also kept their character formatting. The stroke-route cases clear the characters' strokes by design. Each refusal restored the original tree.

A recolor of the first word in the same script, with no idle time for the timer, measures 0 under the group stroke in every font: the plugin is not needed. Under the group fill, it measures 688 to 1,866 pt² drawn in the old color, which is the stale case.

## The route follows a recolor

*tools/run-ai-route-refix.ps1* runs three phases in one Illustrator, with idle time between them, so the re-fix timer acts as it does for a user:

1. Build and route five cases on one page.
2. Recolor the first word of each case green, or, in case E, all of its text.
3. Export after the timer has run.

Minion Pro, damage in pt² measured against the stroke-free export of the same recolor:

| Case | Route | Native | Routed | After the recolor, prep-22 before the story fix | After the recolor |
| --- | --- | ---: | ---: | ---: | ---: |
| A: story, one fill, mixed strokes | group fill → FillFirst | 1,826.1 | 0 | 690.8 (not followed) | 64.8 |
| B: story, mixed fills, one stroke | group stroke | 2,757.8 | 0 | 0 | 0 |
| C: frame, one fill, one stroke | stroke below | 2,763.3 | 0 | 0 | 0 |
| D: frame, one fill, mixed strokes | fill above → FillFirst | 1,809.6 | 0 | 13.2 | 13.2 |
| E: as D, all of it recolored | fill above, new color | 1,802.1 | 0 | 0 | 0 |

The final build (with the review fixes below) reproduced the last column exactly: run *refix3*, one timer run, five items, three routes rewritten, nothing released.

In A and D, the recolor leaves both fills and strokes mixed. FillFirst is all Illustrator has there, so a later character's stroke can again reach an earlier one's fill. B and C need no plugin to follow the recolor. The timer reported one run, five items, and three routes rewritten. A story group's own time stamp does not move when its frames' text is edited, so the timer now follows a routed story's frames on behalf of their group.

Without the plugin, A, D, and E keep showing the old color until StrokeMaster runs again. That staleness belongs to the fill route, which is why the stroke route now comes first.

## The census

The 71-row census (*tools/jsx/ai-coverage-census.jsx*, run *ai-census-24* on the final build) was compared with the October 16 record (*tests/audit/results/illustrator-census-2026-10-16.json*), part by part. 70 rows are identical, native and corrected. In row 68, Segoe Script live text with one fill and one Center 8 stroke, corrected escaping goes from 0 to 1.3 pt². That row now takes the stroke route, whose object stroke carries one miter limit for the frame instead of one per character. `native-route` reported four frames given a stroke below, where October 16 gave four a fill above.

## Separate stories

`story-route separate` is the opt-in of InDesign's *Across separate stories* switch: a group may hold several complete stories, so one story's strokes also go beneath another's fills. *strokemaster-fix-story.jsx* asks before taking it when the selection spans several stories, since a deliberate halo over another frame would be lost.

Measured on Minion Pro with two overlapping one-frame stories (run *sep1*), each with one red fill and one 7 pt stroke. With the opt-in, the group stroke takes the fill-core damage from 2,764.6 to 0 pt² and leaves the halo unchanged. Without it, the script refuses (`story.separate`) and nothing changes. A recolor afterward measures 0: the group stroke needs no plugin to follow it.

## PDF text: written once (region-prep-23)

Illustrator writes every paint pass of type as another copy of the text. Without a fix, the routes extracted each character twice (48 for 24). Illustrator's own uncorrected export of stroked text already doubles it too (72 for 48 on the route-pass fixture, 48 for 24 on the separate-stories control).

Region-prep-23 adds Illustrator's own *Effect > Path > Outline Object* to the route's paint field alone. It is the live effect `Adobe Outline Type`, with the parameters the menu command writes, read back by `style-dump`. That one pass is then drawn as paths, while *Characters* stay live, editable text. The effect is Illustrator's, so it follows edits with or without the plugin. It is tagged, so the route can find it again, and `route-pass -` removes it for research.

| Check (Minion Pro) | Before (prep-22) | Prep-23 |
| --- | --- | --- |
| Story and frame routes, flow fixture: extracted / source | 48 / 24 | 24 / 24 in every case (RGB, CMYK, spot, group fill, group stroke, separate stories, per frame) |
| Re-fix test: source, routed, after the recolor | 120, 240, 192 | 120, 120, 120 |
| Fill damage, all cases | 0 | 0 (unchanged) |
| Re-fix after a recolor (A to E) | 64.8, 0, 0, 13.2, 0 | 64.8, 0, 0, 13.2, 0 |

These counts are MuPDF's extraction, which merges a glyph drawn twice in one place, as Acrobat does. The owner's hand test 11 read a fill-route frame in PDF-XChange, which does not merge, and saw each stroked letter twice. Illustrator writes a character with its own stroke as two text passes, fill and stroke, uncorrected as well, and the fill route leaves the letters' strokes on the letters. Counting every glyph drawn (region-prep-29's certification, `glyphs`): the stroke-route cases draw each glyph once (27 for 27), and the fill-route cases draw the stroked letters twice (43 for 27), as Illustrator's own export of the same text does. No appearance option gives a stroked letter a single text pass, so the fill route stays so.

Every export format keeps the correction (*tools/jsx/ai-route-exports.jsx*, run *exp2*: a story on the stroke route and a frame on the fill route):

| Format | Native damage (pt²) | Routed damage (pt²) | Text |
| --- | ---: | ---: | --- |
| PDF | 3,957.9 | 0 | 48 of 48 |
| PNG, Illustrator's own raster | 4,210.9 | 6.2 | — |
| EPS (Ghostscript; the art-cropped reference aligned by its offset) | 3,987.0 | 0.3 | 48 of 48 |
| SVG | — | corrected in a browser (headless Edge) | 48 of 48, three text elements as natively |

- **SVG:** the outlined pass carries the font's exact glyphs, while a viewer lays out the live text in whatever font it has. Without the font, as in the browser check, the two differ slightly in shape. MuPDF's SVG renderer, which uses the embedded SVG font, drew the fill-route frame as if uncorrected. SVG text without the font is approximate in any case; Illustrator's own *Convert to Outlines* option for SVG text renders exactly.
- **Miters:** the outlined stroke pass changes the PDF's stroke halo slightly: 14.9 pt² on Minion, 33.6 on Segoe Script, and joins on German Beauty. The cause is the renderer, not the correction. Illustrator's own raster of the same text is identical with and without the pass (6 pixels differ at 600 ppi, *miter4*). The earlier PDF's text-stroke pass was stroked by the viewer on the font's outlines, which bevel some joins that Illustrator itself draws as miters. Census row 65 (German Beauty text) therefore reads 10.4 pt² of escaping instead of 1.6. That is what Illustrator always drew, now in the PDF too.
- **Open, to check by hand:** in a fill-route frame, characters added at the end of a stroked run by script (`characters.add`, or a last character's `contents +=`) draw their stroke over the route's fill and extract twice. A control without the outline pass does the same. After such an insert, every character read fails with "illegal text range", so the scripted edit itself leaves the text in an odd state. Typing can't be driven by script; it needs a try by hand.

## Dashed strokes along one outline (region-prep-23)

`native-fix` now also unites the contours of dashed Center and Outside strokes on overlapping filled contours, with the same tagged Pathfinder Add pre-effect it uses for Inside. InDesign's plugin lays a pattern along one outline of the filled shape. A solid Center stroke keeps its inner contour edges, per the owner's ruling. Census 25 against census 24, both on the same day's builds: 67 of 71 rows unchanged. Row 9 (Center 8 dashed on the overlapping squares): stray stroke 375.4 → 4.6 pt², holes 25.9 → 16.1. Row 48 (Outside 4 dashed on outlined Segoe Script): escaping 3.1 → 0. Rows 65 and 68 change through the outline pass above. `native-fix` added Pathfinder Add to 20 items, 13 before.

## Story frames with artwork between them (region-prep-24)

*strokemaster-fix-story.jsx* no longer refuses a story whose frames have other artwork between them in the stacking order:

- An item that draws nothing (an unpainted path, an empty text frame, a hidden item, a group of those) is moved below the story's frames, which changes nothing drawn, and the story is grouped whole.
- Visible artwork keeps its place. It splits the story into runs of adjacent frames, and each run of two or more is grouped and routed on its own (`story-route part`: a group with only some of one story's frames). A frame alone between artwork gets `native-route`. Across the artwork the stacking stays, as InDesign's plugin keeps its order where such content covers a letter.
- Separate stories (the opt-in) still need no visible artwork between their frames. Any refusal restores the original order, grouping, and selection.

Measured on Minion Pro (*tools/jsx/ai-story-runs.jsx*), each frame overlapping the text of the one above, with the rectangle left of the text so it covers no letter. Stacking is listed top first: T a frame, R the rectangle.

| Case | Native (pt²) | Routed (pt²) | Text | Stacking before | Stacking after |
| --- | ---: | ---: | --- | --- | --- |
| Two frames, an unpainted rectangle between | 5,499.7 | 0 | 48 of 48 | TRT | TTR |
| Three frames, a filled rectangle between frames 2 and 3 | 8,260.2 | 141.8 | 72 of 72 | TRTT | TRTT |
| Four frames, the rectangle between frames 2 and 3 | 11,013.9 | 141.8 | 96 of 96 | TTRTT | TTRTT |

The 141.8 pt² left is the one overlap across the rectangle, where the stacking is kept by design. Before the frame alone got its own route, the three-frame case left 2,924.8.

## Certification runner

*tools/certify_illustrator.py* re-certifies the Illustrator side in one command, as InDesign's C-runs do: `plan` lists 27 jobs, `run` launches each in a fresh Illustrator on the hidden desktop (resumable, with a time budget per call), `measure` writes *certification.json*, and `compare` judges it against a baseline and exits 1 on any failed line. The jobs: the story-route cases (nine on Minion Pro, three each on four more fonts), the three story-run cases, the recolor re-fix, the export formats, and the 71-row census (244 measured parts). Tolerances: damage may rise by at most max(1 pt², 5 %); the halo by max(2 pt², 10 %); extracted text and gates must match exactly.

The first run (*build/ai-cert-1*, region-prep-24, about 15 minutes, no host failure) passed every job. The baseline, [illustrator-certification-baseline-prep24.json](../tests/audit/results/illustrator-certification-baseline-prep24.json), comes from *build/ai-cert-2*. That run is *ai-cert-1* with two changes:

- **Great Vibes, rerun.** Its swash capitals made one word wider than the usual first frame, so Illustrator skipped that frame, and every taller frame held the whole text. The flow never crossed frames; the October 29 fixture had the same fault. The fixture now gives Great Vibes a 260 pt first frame and twice the text, so both frames hold text (41 and 14 characters). The story is corrected across them: 4,402.4 → 0.09 pt² with the group fill, 6,159.6 → 0 with the group stroke, and the text extracts 48 of 48.
- **The exports job, extended.** It now also covers PDF/X-4 (0 pt², text 48 of 48), PDF/X-1a, which flattens transparency (0 pt², 48 of 48), and JPEG, Illustrator's own raster (6.2 pt², as PNG). The gate was checked against a doctored copy: a 12 pt² damage regression, duplicated text, a changed gate, a missing job, and a census row 50 pt² worse each failed, and a halo change inside its tolerance passed.

The run measured the Kerberos Fang stroke route's halo at 940 pt². Illustrator's own raster of that text with and without the outlined pass differs by 5 pt²; MuPDF's renders of the two PDFs differ by 557 pt². The halo numbers measure the viewer's stroking of the font in the uncorrected PDF, as with German Beauty above, not a change in what Illustrator draws.

## Open paths: no gap

The census's open-path rows (27 to 30) keep the thorn's whole point missing, about 162 pt² each, corrected as natively. That is the fixture, not the plugin: the thorn's path starts at its tip, so opening it puts both ends there, and an end is capped, not joined. No rule in either application keeps a point at a cap. The open squares' and outlined text's joins are fixed as on closed paths.

## The owner's showcase notes (region-prep-25)

The owner annotated the showcase. Each note, looked up at 600 ppi in the exports it was drawn from:

- **Kerberos Fang, "those circle things":** the font's splatter dots are tiny contours. A 7 pt stroke around a contour narrower than the stroke draws, by the PDF's definition, a solid disc, and Illustrator's own raster shows exactly that. MuPDF and Ghostscript both draw hollow rings with pie-shaped gaps instead. Illustrator's own uncorrected PDF shows the same rings, so the correction neither causes nor can cure it; Illustrator's *Outline Stroke* would bake the stroke into fills, but it changed Illustrator's own drawing (339 pt² on Kerberos Fang), so it was not adopted.
- **German Beauty, "any reason those get beveled; same in InDesign?":** yes. InDesign's plugin strokes live text with the run's own miter limit, and keeps whole points only on shapes and outlined text. The V tips are sharper than the limit (4), so both applications bevel them. The note found a real departure, though: `native-fix`'s per-character limits used the shape rule, so a whole point could have been kept on live text. Region-prep-25 applies InDesign's live-text rule to per-character limits and to object and story strokes (`MustKeep`'s `wholePoints`, two new contour checks). The census does not change on any row.
- **Great Vibes, "two spikes":** miter tips on the *x*. The showcase had run the route alone; `native-fix`, which the user's script runs too, trims them as glyph tips. The certification's story jobs now run both, and the showcase shows the result (*showcase-prep25.pdf*).
- **The re-fix page, "letters overlapped by strokes again":** after the recolor, cases A and D have both their fills and their strokes mixed, and per-character FillFirst is the only native construct left. Overlap returns across characters (A 64.8 pt², D 13.2). This is the known limit: no Appearance construct paints one character's stroke beneath another's fill when both differ per character. The text-kept live effect handles it while the plugin runs, but goes stale without it, against the owner's ruling.

## Rotated text and text on a path

The census measured about 2 pt² of escaping stroke on rotated live text and on type on a path, against 0.34 on the same text unrotated. On the rotated row:

- The real spikes, the crotches of *v* and *w* (109 and 119 px at 8 px/pt), are gone after the fix, as on the unrotated row.
- What is left is 74 specks of 1 to 5 px (0.02 to 0.08 pt² each) along straight stroke edges, a median 0.39 pt beyond the allowed band.
- The reference outline and the drawn fill register exactly (the best shift is zero on every row).
- Rotation adds the same amount without StrokeMaster: 1.77 pt² natively and 1.64 corrected over the unrotated row.

The residual is therefore how a rotated text stroke is drawn or measured, not a correction the plugin misses. Miter ratios do not change under rotation, and the limits chosen are the same. Recorded as such; not a gap.

## Showcase

*tools/compose_illustrator_showcase.py* sets a certification run's native and corrected exports side by side, labeled with the measured numbers, one page per job: the story routes, separate stories, the per-frame comparison, three decorative fonts, the story split by artwork, and the re-fix after a recolor. Built from *build/ai-cert-2* into *build/look/illustrator-showcase-2026-10-30/showcase.pdf* (nine pages). Review material, kept in the ignored build folder.

## Independent review

An independent read on the API credit (read-only) checked the route code. It confirmed the *Contents* position math, the cleanup on error paths, the re-fix stamp bookkeeping, and the undo appending. It found three real defects and some smaller gaps, fixed before the final host runs:

- Per-character miter limits, which `native-fix` writes, kept a fixed frame off the stroke route: the miter limit is no longer part of "one stroke".
- A frame left on FillFirst because its style is the user's own could have been rebuilt by a later re-fix. It now stays on FillFirst.
- A frame of a story group could get a route of its own on top of the group's. A story's frames now lose their own records, and the re-fix never routes them alone.
- Smaller: the *Contents* position is put back after the route's field is removed; text cut away entirely keeps its route for what is pasted back; a story group's stroke gets its miter limit from every glyph of the story (`FixStoryStroke`, also run by `native-fix` on a selected story group).

Left as they are, all minor: a weight change, such as scaling with *Scale Strokes & Effects*, makes the stroke route's record stop matching its field, so the route is released rather than followed. A duplicated route field is matched first-found. Dictionary write errors are not reported.

## Limits of this run

- Great Vibes put no text in the sweep's first frame in these region-prep-22 runs, so that font's rows cover one frame. Fixed in the region-prep-24 certification (see *Certification runner*).
- Memory ran short at one point, and Claude Code stopped the background shell of a host batch after its runs had finished. Later host runs ran one at a time in the foreground.
- A combined run of five cases in one Illustrator process stopped during its fourth document without a receipt. The case passed alone, and the rest passed in a fresh process. This is the known instability after several scripted documents in one process, so sweeps now keep to four cases per process.

## Remaining

Against the parity matrix drafted the same day (kept privately with the handoff), what is still open on the Illustrator side, roughly by value:

- **Interface:** the panel exists since region-prep-26/27 ([report](ILLUSTRATOR_PANEL_2026-10-30.md)); the owner's hand tests are answered in region-prep-29 and their retests in region-prep-30; region-prep-31 corrects automatically, as InDesign does. The license exception is in place; what is left is the prep-31 retests, the final plugin and effect names, and an installer section.
- **PostScript** output (needs a printer driver), and real documents.
- **By hand, since scripts cannot do it:** typing at the end of a stroked run in a fill-route frame (the scripted insertion above), and redo after the re-fix.
- **Smaller:** Outside dashed whole points stay the opt-in (`native-align dashes`), as the owner decided; the interface will offer it as a switch.

## State

Region-prep-31, SHA-256 `2ce07480df0ebb78c32f395c74de91d5638d75865d9c29d67fbc5f60d699e5af`, is installed and frozen as *plugin/illustrator-probe/build/region-prep31-frozen.aip*: Illustrator corrects new and edited art automatically, as InDesign does, and *Trim glyph tips* is off by default (the owner's answers, [panel report](ILLUSTRATOR_PANEL_2026-10-30.md)), certified (*build/ai-cert-8*, 31 jobs with the new `auto`) against [its baseline](../tests/audit/results/illustrator-certification-baseline-prep31.json), with no regression against prep-30's; with tips untrimmed, the halo of the spiky fonts' stories falls (Kerberos Fang 492.0 to 1.1 pt²). Region-prep-30, SHA-256 `7b8edbb6702c30c20a5648dec6ca2f43d1ba0aff8740abbabc66aa9b2e0b5aba`, is frozen as *plugin/illustrator-probe/build/region-prep30-frozen.aip*: the answers to the owner's retests, among them a route stroke now judged against the frame's own limit, so turning *Trim glyph tips* off raises it again (Kerberos Fang 1.938, 3.995, 1.938), certified (*build/ai-cert-7*, 30 jobs) against [its baseline](../tests/audit/results/illustrator-certification-baseline-prep30.json). Region-prep-29, SHA-256 `e4384dbf22d7141ca1051a3ba605692af6356554ff6c55c52997ace42586cf4a`, is frozen as *plugin/illustrator-probe/build/region-prep29-frozen.aip*: the answers to the owner's hand tests ([panel report](ILLUSTRATOR_PANEL_2026-10-30.md)), certified (*build/ai-cert-6*, 30 jobs with the new `switches`, and every glyph drawn counted) against [its baseline](../tests/audit/results/illustrator-certification-baseline-prep29.json), with no regression against prep-28's. Its first census run drew one fixture path (row 28) with Illustrator's default stroke in both the uncorrected and corrected exports; a rerun drew it as specified and is the one measured (the first is kept in *build/ai-cert-6/census-fixture-glitch/*). Region-prep-28, SHA-256 `ee19c6f68b0b04085ff70bdb142d61c5229265593f1befc0d0b12620edc1571d`, is frozen as *plugin/illustrator-probe/build/region-prep28-frozen.aip*: prep-27 with the license notice (each plugin carries its own SDK's linking exception, *LICENSE-EXCEPTION*), certified (*build/ai-cert-5*, 29 jobs, each with a settings file of its own) against [its baseline](../tests/audit/results/illustrator-certification-baseline-prep28.json). Region-prep-27, SHA-256 `6ba4185c6a0e56e23a29b56f39499cfe0abc51c0a9465243f8a3c930c99bb1be`, is frozen as *plugin/illustrator-probe/build/region-prep27-frozen.aip*, with the panel ([its report](ILLUSTRATOR_PANEL_2026-10-30.md), [hand tests](ILLUSTRATOR_HAND_TESTS_2026-10-30.md)), certified (*build/ai-cert-4*, 29 jobs) against [its baseline](../tests/audit/results/illustrator-certification-baseline-prep27.json) and with no regression against prep-25's. Region-prep-25 (`e6d3fc28…`) is frozen as *plugin/illustrator-probe/build/region-prep25-frozen.aip*, certified (*build/ai-cert-3*, 27 jobs, the story jobs now with `native-fix`) against [its baseline](../tests/audit/results/illustrator-certification-baseline-prep25.json), and with no regression against prep-24's. Region-prep-24 (`83d3830b…`), certified against [its baseline](../tests/audit/results/illustrator-certification-baseline-prep24.json), stays frozen. Region-prep-23 (`7c299b52…`) and region-prep-22 (`197da1da…`) stay frozen beside it. The prep-24 raw evidence is in *build/ai-prep24-2026-10-30/* and *build/ai-cert-1/*. The prep-23 raw evidence is in *build/ai-prep23-2026-10-30/*, *build/ai-route-pass-2026-10-30/*, and *build/ai-census-25/*.

The region-prep-22 results above: The four mock suites pass (108, 2,879, 13,229, and 9,255 assertions). The portable record is [illustrator-text-route-2026-10-30.json](../tests/audit/results/illustrator-text-route-2026-10-30.json); raw evidence is in the ignored *build/ai-story-route-2026-10-30/* and *build/ai-census-24/*. Illustrator is closed, its preferences are as before the runs, and the workspace guard has no pending snapshot.
