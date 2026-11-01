# StrokeMaster 1.2.0

Two separate Windows x64 downloads are available in [one release](https://github.com/VulpesNexus/strokemaster/releases/tag/v1.2.0):

- **Illustrator 2026:** *StrokeMaster-1.2.0-Illustrator-Windows-x64.zip*, tested on Illustrator 30.8.2.
- **InDesign 2026:** *StrokeMaster-1.2.0-InDesign-Windows-x64.zip*, including the engine and StrokeMaster panel, tested on InDesign 21.6.

Quit the relevant host before installing and follow *INSTALL.txt* inside its archive. Keep only one installed copy. InDesign requires both plugin folders and their resources; Illustrator requires the single *.aip* file. The archives include licenses, corresponding-source links, and manifests. *SHA256SUMS.txt* verifies the downloads.

This release corrects the Illustrator triangle and compatible two-story hole cases, improves InDesign repainting after frame edits, closes shared cavities between compatible frames with different stroke widths, and enables keyboard focus in the InDesign panel. The final InDesign export tests preserve lettering, close the reported gap, and pass Tab/Space, disabled-control skipping, and Escape checks. The owner confirmed GPU viewing and the remaining keyboard checks. The [closeout review](REVIEW_2026-11-02.md) and [follow-up](2026-11-02_FOLLOWUP.md) identify the tested builds and coverage.

The packaged executable binaries retain their verified bytes. The InDesign panel resource's diagnostic source filename is rebuilt as a basename to avoid exposing a local build path; no control layout or behavior changes. Packaging verifies all archive contents and checksums. Adobe SDK files, debug symbols, private fixtures, and test logs are excluded.

Read [what to know](WHAT_TO_KNOW.md) before use. Illustrator edits document appearances; incompatible mixed-stroke text grouping remains refused. InDesign draws corrections without changing the document itself. Cross-version support, macOS, paragraph decorations, and inline artwork are not newly certified by this release. These are unsigned native packages, not Marketplace installers. The InDesign prefix remains the unassigned development value `0x5A200`; this release does not establish Adobe allocation or eliminate conflicts with another plugin using that prefix.
