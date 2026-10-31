# Illustrator text parity investigation — October 29, 2026

The region-prep-20 research build repairs destructive glyph inspection in linked text. It preserves threaded stories, local formatting, native save/reopen, and live editing in the five tested fonts. It does not reach InDesign parity: overlapping frames in the same story still damage fills, and PDF extraction repeats text from separate paint passes.

## Build and change

Tested in this stage: `region-prep-20`, frozen as *plugin/illustrator-probe/build/region-prep20-frozen.aip*, SHA-256 `851ff6c11207e070ca08acec46ec0a5dabe7ae9b747ed015e93bf393dbcddbc9`. The [portable evidence record](../tests/audit/results/illustrator-text-parity-2026-10-29.json) binds the binary, final source hashes, host results, measurements, and raw evidence hashes. Builds remain non-reproducible; use this frozen binary to replay these findings.

*RegionProbe.cpp* now measures glyphs on an owned deep copy, clears object-level appearance on that copy, outlines it, and disposes of it. The live frame is never outlined. Both character miter selection and Inside inset inspection use this path. The original frame retains its text, style, and story membership. Ownership mocks cover duplication, style removal, and outline failures.

Correction from the later October 29 investigation: the initial prep-19 result read the text-frame collection immediately after `app.open()`. A fresh process with separate open and read phases finds both linked Segoe Script frames, with 20 and 7 characters. The earlier claim of lost editable text was false; clearing the temporary copy's appearance is retained as isolation, not a proven repair for saved-file corruption. Prep-19 remains archived as the rejected experiment. The prep-18 Minion file still reads as two independent one-frame stories after idle, confirming that separate destructive-inspection defect. See the [story-route follow-up](ILLUSTRATOR_NATIVE_STORY_2026-10-29.md) for the new measurements and current installed build.

## Host verification

The host was Illustrator 30.8.2, accessed through `Illustrator.Application.30` and Windows PowerShell 5.1 COM. Tests used synthetic documents only. A 30-case prep-18 baseline covered five fonts and point, area, path, rotated, scaled, and threaded text. A stronger flow fixture explicitly requires text in both linked frames; its prep-18 Minion control changed shared-story frame counts from 2/2 to 1/1 during `native-fix`.

Prep-20 passes the stronger flow fixture in Minion Pro, Segoe Script, Kerberos Fang, German Beauty, and Great Vibes. Each case retains two nonempty frames in one story before correction, after correction, and after native AI reopen. Character font, size, leading, tracking, scale, baseline shift, fill, stroke, frame matrix, contents, and thread membership remain unchanged by correction; the intentional miter-limit changes are excluded from that comparison. All five remain editable after reopen. Their corrected PDFs are pixel-identical before and after reopen. Great Vibes uses the measured wider first frame so a complete word can enter it; the rejected narrow-frame variants are not counted as flow coverage.

The automatic re-fix test uses three separate COM calls, with four seconds of idle time between them: mark the frames, edit their story, then inspect the timer result. It records one automatic run, no error, both links intact, and the edited text retained. It covers Minion Pro and one edit, rather than undo/redo or broad reflow behavior.

The established alignment probe and measurer reproduce every line of the October 16 prep-18 record. All ten plugin-managed restore comparisons remain at 0 pt². The square using Illustrator’s own alignment is an unmanaged control; its alignment intentionally remains set. Existing differences from outlined Inside/Outside references are unchanged, not newly certified as exact.

Build checks passed: 96 registration/bridge/refusal/outline-ownership assertions, 2,879 SDK-output assertions, 13,229 stroke-behind assertions, and 9,255 text-behind assertions. These mock results remain separate from the native checks above.

## Open rendering gates

At 600 ppi, the fill reference is gray <32, eroded by three pixels. A reference core pixel at gray ≥32 in the tested export counts as damaged. The overlapping flow cases retain the following damage; lower values are better.

| Font | Native damage (pt²) | Corrected damage (pt²) | Reopened damage (pt²) |
| --- | ---: | ---: | ---: |
| MinionPro-Regular | 1728.73 | 64.43 | 64.43 |
| SegoeScript | 2573.70 | 103.05 | 103.05 |
| KerberosFang | 5165.68 | 469.63 | 469.63 |
| GermanBeauty | 4214.04 | 181.32 | 181.32 |
| GreatVibes-Regular | 2002.67 | 336.00 | 336.00 |

The per-frame native appearance route reduces damage but does not protect an earlier frame’s fill from a later frame’s stroke in the same story. This is the next engineering target, corresponding to InDesign Route X. Preserve intervening artwork, z-order, independent-frame behavior, live text, and editability. Do not reclassify the gap as a proven native impossibility.

All five routed PDFs extract 48 nonspace characters for 24 source characters. The duplicated occurrences follow separate native paint passes; prior records already showed increased extraction counts, but treated text retention as sufficient. Exact extraction is now a distinct failed gate. The strict text-parity measurer intentionally exits 1; none of the five flow cases meets all rendering and extraction requirements.

Transformed-text save/reopen also needs further controls. The prep-18 baseline showed small matrix quantization and nonzero pixel differences in some transformed/path cases. One multi-document native control crashed during text-on-path reopen before either correction command was called. That run is not evidence of a correction-code crash, and no general host-stability claim follows from the successful focused cases.

Kerberos Fang’s final fixture spent 1,609 ms in correction and 31,420 ms in its native export phase. Its approximately 71-second total should not be attributed entirely to the correction algorithm. Wider performance and crash/undo coverage remain open.

## Evidence and resume

Raw synthetic files and rejected controls remain under *build/ai-parity-2026-10-29/*. The [portable record](../tests/audit/results/illustrator-text-parity-2026-10-29.json) retains failed gates alongside successes. *tools/run-ai-parity.ps1* runs one fixture per fresh host and validates bridge status, formatting, live editing, and story membership; its *-ValidateOnly* option checks existing receipts without driving Illustrator. *tools/run-ai-threaded-refix.ps1* runs the three timer phases. Both launch through the verified hidden outer route and *hidden-desktop-run.py*; no visible-desktop fallback was used.

Preferences match the saved pre-reload file, and the workspace guard reports no pending snapshot. Illustrator is closed after the isolated tests; that stage ended with prep-20 installed. The follow-up report records the current installed build. No owner document was open at the initial attachment. The seven prior local commits were not pushed, and this investigation remains uncommitted.

The REA tool guidance was consulted. Source and SDK inspection were sufficient for this defect; no REA native import or decompiler result is claimed. Product identity, panel/menu controls, broader exports, real-document validation, and release packaging remain open.
