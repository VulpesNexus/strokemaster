# StrokeMaster InDesign candidate with the latest panel

This candidate includes the C32 correction engine and Claude's final, uncommitted UI build `e84b62ea`. It supersedes the earlier candidate that bundled panel `5f4f5bab` and therefore omitted the UI work.

The panel contains the owner's crown in light- and dark-theme SVG and PNG resources, descriptions on hover for every switch and Restore Defaults, the master label *StrokeMaster*, *About StrokeMaster* in the panel menu, and an *Engine not installed* warning. The icon appears when the panel is collapsed to icons. Open *Window > StrokeMaster*, hover an option, and open the panel menu to inspect About.

The packaged engine hash is `424d1652b6bfde6f0a8d85e0d88b2228d8e360e6d9c2b861e7f9f04d834377ea`; the panel hash is `09d7891eb5a7bd4f9a0b59f9afc10f05e94bf1e969a3bda0ee0f3b6e8579ace4`. Only personal PDB paths were replaced in the built inputs' debug metadata. Their executable code and resources are retained; host acceptance of these copies remains pending. The engine's C32 evidence does not certify the newer UI.

The local installation was corrected on 2026-10-07: the newer panel and all 21 files in its installation were read back and verified, and the engine was unchanged. A rollback backup was retained. Start InDesign to load the corrected panel. Nothing was committed or published.

For installation on another local evaluation setup, extract the whole archive. With InDesign closed, use the included *tools/install-indesign-plugin.ps1*: `-Variant Release-RouteX` for the engine, and `-Variant Release-UI -Name StrokeMasterUI` for the panel. `-ValidateOnly` checks without writing. Installation requires administrator permission; the script creates backups, verifies copies, and refuses a running InDesign. Keep its backup locations; `-Restore` with the appropriate backup restores each plugin. Installing native binaries takes effect after InDesign is started again.

The source snapshot includes the current uncommitted UI work, icons, and About implementation, plus the corresponding native build files. Adobe SDK material and debug symbols are excluded. The project files use a PDB basename for future builds. Both GPL-3.0-or-later license files and the Adobe InDesign SDK linking exception are included. These files do not assign a native prefix or authorize a Marketplace release.

Compatibility is limited to InDesign 21.6.0.57 on Windows x64 for the certified engine. Before external distribution, assign the Adobe prefix, validate the final UI and sanitized binaries in the host, exercise installation and rollback on a clean setup, and settle signing and publication. The latest panel has not been launched by this installation task. Engine settings were preserved.
