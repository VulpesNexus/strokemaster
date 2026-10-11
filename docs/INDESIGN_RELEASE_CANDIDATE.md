> Superseded by [the latest UI candidate](INDESIGN_LATEST_UI_CANDIDATE.md). This older package used the certified panel and omitted Claude's later UI work; that installation has now been corrected.

# InDesign release candidate

This local package pins the completed InDesign correction engine to the final certified pair: engine `73fb1805` and panel `5f4f5bab`, from source revision `c6673588e7ea1e536c8bcd9aec2f72fb8de518fc`. Certification C32 was recorded on Windows with InDesign 21.6.0.57. Earlier versions, other 21.6 builds, macOS, and InDesign Server are outside this package's tested compatibility.

The candidate is for local evaluation. Its native ID prefix remains the unassigned development value `0x5A200`; external distribution requires an Adobe-assigned prefix and a fresh build and certification. Signing, Marketplace submission, and clean-machine install, update, and uninstall acceptance remain release gates. Preparing the archive does not clear those gates.

## Contents and provenance

The archive contains the two plugins and their resource folders, the existing backup-and-verify installer, the corresponding committed InDesign source and build projects, both license files, this guide, and a SHA-256 manifest. Adobe SDK files, debug symbols, logs, fixtures, personal documents, and the unfinished newer panel are excluded. Building the source requires a separately obtained InDesign 2026 SDK, Visual Studio, and the supported MSVC 14.39 or 14.42 toolset. The source's build script describes the build options.

The engine comes from the certified production build. The packaged copies replace only the absolute PDB path in each PE CodeView record with its basename; GUID, age, record size, executable code, and resource bytes are retained. Their hashes therefore differ from the certified inputs. The manifest records both hashes and the exact changed ranges. Host load and export acceptance of these metadata-sanitized copies remains pending. The panel comes from the verified installer backup preceding the newer panel installation. The source comes from the named commit, rather than the current edited working tree, with one documented overlay: both linker projects use a PDB basename to prevent future personal build paths. This uses [Microsoft’s PDBALTPATH option](https://learn.microsoft.com/en-us/cpp/build/reference/pdbaltpath-use-alternate-pdb-path?view=msvc-170). The packaging utility is included. Rebuilding need not reproduce the binary byte for byte; compare behavior against C32 and record fresh hashes.

## Install, update, and restore

Extract the entire archive into an empty folder. Keep the manifest and resource folders with the binaries. Quit InDesign yourself before installing; the installer refuses a running host and never closes it. Installation into InDesign's Plug-Ins folder requires administrator permission. The elevation prompt is required only when you choose to install.

Use the included *tools/install-indesign-plugin.ps1* from the extracted folder. Select `-Variant Release-RouteX` for the engine, then `-Variant Release-UI -Name StrokeMasterUI` for the panel. Before installation, add `-ValidateOnly` to each invocation to inspect the proposed copies without writing anything. Set `-InDesign` explicitly if the application is installed elsewhere.

Each installation backs up the existing plugin and resources under *build/indesign-install*, compares the installed files with the source by hash, and prints the backup location. Retain both backup folders. To restore, quit InDesign and use `-Restore` with the appropriate backup folder; include `-Name StrokeMasterUI` for the panel. Restoration is a separate operation for each plugin. When no earlier plugin existed, its empty backup is not a usable restore payload: remove only the two newly installed plugin folders, with InDesign closed, to return to the previous state. Do not remove other plugins or the shared Plug-Ins folder.

Start InDesign after installation or restoration. Replacing native binaries takes effect only after a relaunch. This package was prepared without installation or a host restart. The existing installer is included unchanged; its operations were not rerun as part of this packaging step.

## Use and limitations

Open *Window > StrokeMaster*. The certified panel provides a checkbox for each correction and *Restore Defaults*. Defaults enable the certified corrections, with correction across separate stories off. Settings apply to every open document and every export; restore defaults before comparing against certification. The correction changes drawing, not the document's stored text or paths. A recipient opening INDD or IDML without the plugin sees InDesign's native drawing. An exported PDF retains the corrected geometry and live text where the source is live text.

Outside text strokes draw beneath their story's fills, including threaded frames and columns. Miter correction covers text and supported shapes, frames, and outlines. Closed shapes also receive corrected Inside and Outside fills, patterns along the filled outline, and supported opacity and blending. Dashes and dots fit between real corners; Wavy closes with whole waves; White Diamond retains InDesign's corner marks. Whole points and glyph-tip trimming follow the owner's certified rules.

Vertical text, stroke- or fill-specific effects, unsupported open-path strokes, and arrowheads remain native. Content stacked between threaded frames can interrupt cross-frame correction. Whole points are bounded under opacity or blending. Independent stories retain native stacking by default; the experimental switch can remove deliberate halos. Decorative outlines increase PDF size: the heavy-material test recorded about 9% more export time and 58% larger PDFs. These are fixture measurements, not universal performance guarantees.

The complete behavior and evidence are in the repository's *docs/USING_STROKEMASTER.md* and *docs/RELEASE_CANDIDATE_CHECKS_2026-10-06.md*. Newer UI descriptions in the working copy do not apply to this pinned panel.

## Validation and release gates

C32 records 110 measurements equal to C24, seven identical PNGs, a cross-frame census of 22/22, an extension census of 8/8 built cases, and no plugin state in 13 saved documents. The three final host checks covered stroke opacity, Center Thick - Thin orientation, and roman and tate-chu-yoko in vertical text. Packaging verification checks every payload hash and ZIP entry, and rejects input binaries differing from the pinned pair. It also verifies that binary changes are confined to the PDB pathname bytes. It does not rerun host certification.

Before an external release: obtain and apply the assigned prefix; certify the rebuilt engine and panel; decide the supported host range; validate installation, update, restoration, and removal on a disposable clean setup; and complete the chosen channel's signing and publication requirements. No package is submitted or published by this preparation.

## License

GPL-3.0-or-later with the Adobe InDesign SDK linking exception. Read *LICENSE* and *LICENSE-EXCEPTION*. Corresponding project source is included; Adobe SDK material must be obtained separately under Adobe's terms.
