# Route W certification corpus

The fixtures the route W certification measures (*ROUTE_W_CERTIFICATION_2026-09-25.md* at the repository root). They are frozen: every certification job copies a fixture before opening it (*tools/jsx/cert-lib.jsx*), so nothing here is ever opened, saved, or exported in place.

| File | What it is | SHA-256 |
| --- | --- | --- |
| *victims.indd* | The 20-case victim census, as the route W census run saved it (*tools/jsx/probe-victim-census.jsx*) | `f3270a31…` |
| *victims.manifest.json* | Its frame keys, names, and bounds | `6357c939…` |
| *victims.reference.png* | The census's split reference at 600 ppi: centered doubled strokes behind fill-only text | `15f3d988…` |
| *victims.mask.png* | The census's independent fill mask at 600 ppi | `31885db1…` |
| *victims.host.json* | The census run's own record (host version, frames, preference restore log) | `4c647cc8…` |
| *cert-T.indd* | Twelve victims under transparency: text, object, and group opacity, blend modes, drop shadow, feathers, clipping, transparent art above and below | `28fe6428…` |
| *cert-C.indd* | Eight victims in color: process, spot, RGB, tints, a gradient stroke, an overprinted stroke | `b1d134dd…` |
| *cert-E.indd* | Twelve scope and editing cases: threaded frames, independent overlapping frames, mixed runs, overset, transforms, a table cell, an anchored frame, text on a path | `5498dcb6…` |
| *cert-X.indd* | The cross-frame census (route X, 22 cases): independent frames in both stacking orders, touching and on two layers, and a headline halo; threaded pairs in both stacking orders, overlapping, split across layers, grouped, bordered, with a rectangle stacked between, with a filled downstream frame, and two columns of one frame; a translucent and a hidden upstream frame, table rows; negative controls (rectangles above and below stroked text, Center strokes, a nonprinting upstream frame) | `29237a41…` |

*cert-X.indd* and its manifest were built by *tools/jsx/cert-build-xframe.jsx*; *tools/certify_routex.py* holds each case's expected outcome under route X, and *CROSS_FRAME_RESEARCH_2026-09-26.md* describes them.

The census files are byte-identical to the ones *tests/audit/results/indesign-routew-victim-census.json* records. The *cert-* fixtures and their manifests were built by *tools/jsx/cert-build-fixtures.jsx*; rebuilding them gives new documents with new hashes, so the committed ones are the record. Every fixture uses Minion Pro, which InDesign 2026 installs; the font program itself is not part of the repository.
