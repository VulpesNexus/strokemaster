# Fixture corpus

The supplied fixture document and the evidence gathered from it in Phase A. The census is in *docs/STROKE_FIXTURE_CENSUS.md*.

## Files

| Path | What it is |
| --- | --- |
| *pristine/fixture-pristine.indd* | The fixture exactly as InDesign 21.5.1.73 saved it with `Document.saveACopy()` from the live document. Read-only. SHA-256 `3797BA73746CD6BAC8CF55B689F0347E77CB622257651AC4AFA5F4430BA3A783`, 1,249,280 bytes. |
| *working/fixture-working.indd* | Byte-identical working copy, opened without a window and closed without saving by every Phase A job. Not tracked. |
| *actual/fixture-exports/* | Exports of the working copy: *[High Quality Print]* and *[PDF/X-4:2008]* PDFs, InDesign PNG at 600 ppi plain and with *Simulate Overprint*, and IDML. Also the PDF operator dump (*hqprint.ops.txt*) and MuPDF paint traces (*\*.trace.txt*). |
| *actual/fixture-exports/crops/* | Per-fixture crops from InDesign's PNG and from MuPDF's render of the PDF |
| *actual/idml-roundtrip/* | The IDML reopened in InDesign and exported again; paint trace and renders identical to the working copy |
| *actual/pdf-surgery/* | Montages and numbers from the Phase A PDF edits: strokes first for A and C, miter-limit sweep for B |
| *actual/eps/* | PostScript exports: as-is (flattened, over the transparent backdrop) and with the backdrop made opaque in memory |
| *actual/screen/* | Screenshots of the live document from the fixture's author at 300 %: GPU preview, CPU preview, *Overprint Preview*, and GPU Performance off. Cropped to the canvas, since the window chrome shows the account avatar. |

## What was removed

- The `%%For:` line of both EPS files named the person the export was made for. Its value was replaced with `(removed)`; nothing else in the files changed.
- The raw document-model dump behind the census isn't included. It recorded machine-specific font and preset paths and custom preference names. The census keeps everything from it that bears on stroke rendering.

Run *tools/scan_identifiable.py* on any new evidence before adding it here.
