# StrokeMaster

Fix stroke overlaps, miter spikes, and enclosed gaps in Adobe Illustrator and InDesign. Choose which corrections to apply, keep text editable where supported, and compare the result with each correction switched on or off.

## Download

**[StrokeMaster 1.2.0](https://github.com/VulpesNexus/strokemaster/releases/tag/v1.2.0)** provides two separate downloads:

| Plugin | Download | Tested host |
| --- | --- | --- |
| Illustrator | [Windows x64 ZIP](https://github.com/VulpesNexus/strokemaster/releases/download/v1.2.0/StrokeMaster-1.2.0-Illustrator-Windows-x64.zip) | Illustrator 2026, 30.8.2 |
| InDesign, including its panel | [Windows x64 ZIP](https://github.com/VulpesNexus/strokemaster/releases/download/v1.2.0/StrokeMaster-1.2.0-InDesign-Windows-x64.zip) | InDesign 2026, 21.6 |

macOS and other host versions are not currently supported by these downloads. Each archive includes installation instructions and licenses; the release includes checksums.

## What it does

- Places supported text strokes beneath letter fills so overlapping lines and frames remain readable.
- Removes unwanted miter spikes while preserving supported exterior points.
- Trims glyph tips and closes enclosed gaps beneath supported strokes.
- Offers switches for individual corrections, with [illustrated examples](docs/FEATURES.md).

The two hosts have different capabilities. Read [what to know](docs/WHAT_TO_KNOW.md) for the supported cases and limitations.

## Install

Quit the host before installing or updating, back up an older plugin, and keep only one installed copy.

**Illustrator:** copy *StrokeMasterRegionProbe.aip* into *%ProgramFiles%\Adobe\Adobe Illustrator 2026\Plug-ins* with administrator permission. Alternatively, use *%LOCALAPPDATA%\Adobe Illustrator Plug-ins\30* and select that shared folder under *Edit > Preferences > Plug-ins & Scratch Disks > Additional Plug-ins Folder*. That single preference is shared by all additional plugins; do not point it at a StrokeMaster-only folder. Restart Illustrator. Duplicate copies can prevent the additional folder from loading.

**InDesign:** copy both folders from the archive's *Plug-Ins* folder into *%ProgramFiles%\Adobe\Adobe InDesign 2026\Plug-Ins* with administrator permission. Keep each parenthesized resources folder alongside its *.pln* file. Restart InDesign, then open *Window > StrokeMaster*.

These are native plugins, not Creative Cloud Marketplace installers. For updates and removal, follow *INSTALL.txt* in the relevant archive.

## Use

Start with a copy of your document and enable the corrections you need. Hover over the controls for explanations. InDesign's panel supports Tab and Shift+Tab after clicking a control, Space to activate it, and Escape to return keyboard focus.

**Illustrator changes document appearances.** Automatic correction can modify artwork as you edit it; use *Correct Selection* for explicit control. Saved live effects retain their last result without the plugin but stop following edits. Some mixed-stroke text groups are intentionally left separate, and their shared cavities remain open.

**InDesign changes viewing and exports, not the document's contents.** Corrections require the plugin on the viewing machine. Send a PDF exported with StrokeMaster when a recipient does not have it installed. Panel settings apply to all open documents.

For workflows and optional InDesign scripts, see the [usage guide](docs/USING_STROKEMASTER.md). The [release notes](docs/RELEASE_1.2.0.md) cover this version's fixes and remaining limitations, including the development InDesign prefix.

## Documentation and feedback

- [What to know before use](docs/WHAT_TO_KNOW.md)
- [What each switch fixes](docs/FEATURES.md)
- [Usage guide](docs/USING_STROKEMASTER.md)
- [Technical background, build routes, and verification](docs/TECHNICAL_BACKGROUND.md)
- [Research archive and relocated documents](docs/RELOCATIONS.md)
- [Report an issue](https://github.com/VulpesNexus/strokemaster/issues) — include the host version, relevant switches, and a minimal example you can share.

## License

[GPL-3.0-or-later](LICENSE), with an [Adobe SDK linking exception](LICENSE-EXCEPTION). Copyright © 2026 [Vixen420](https://github.com/VulpesNexus).
