# Open engineering questions

Takeover started 2026-09-12. Source availability, compilation, mocked calls, and actual-host execution are separate evidence levels. The [takeover audit](PROJECT_TAKEOVER_AUDIT.md) records the execution history.

| ID | Status and remaining question | Evidence / next experiment | Blocker? |
| --- | --- | --- | --- |
| Q01 | CLOSED for Illustrator 30.7.0, build 114R: singular `createOutline()` works | Four-route disposable-host probe, 17 checks; corrected extraction and injected-failure cleanup | No for measured host |
| Q02 | CLOSED for the declared local straight-join kernel; old clipping formula was wrong | Independent decimal half-plane oracle, ordinary-miter controls, and 12,984 coordinate comparisons; [contract](docs/MITER_GEOMETRY_CONTRACT.md) | Whole-path topology still blocks |
| Q03 | DISPROVEN: local clipping alone meets the original envelope requirement | Synthetic counterexample protrudes 0.321320 pt; [region experiment](docs/REGION_OFFSET_EXPERIMENT.md) specifies the next construction | Yes, global envelope derivation |
| Q04 | Where is the actual Illustrator Appearance stroke stored? | PDF emits 3 pt although character DOM says 1 pt; inspect per-contribution SDK style and multiple strokes | Yes |
| Q05 | Partially closed: basic Unite works; arbitrary topology remains open | Ten host checks include expanded overlap area 15,000 pt² and compound area 7,500 pt²; add Boolean holes, bowties, degeneracies, and transforms | Yes |
| Q06 | Can `kStrokeFilter` replace exactly one stroke contribution on live type? | Native straight-line diagnostic builds and passes mocks; loading awaits preservation of unsaved user art and restart, then text needs a separate probe | Yes |
| Q07 | Can InDesign suppress/reorder native strokes without altering composition? | Public drawing contracts and isolated callback proof; existing adornment only adds painting | Yes |
| Q08 | CONFIRMED defect: unstroked victim runs are omitted. Transparency scope is bounded | Unstroked victim has 3,759 erroneous fill-core pixels at 600 dpi; controls hold. Independent masks give unchanged fill cores in 23 historical transparency samples. Test overlapping colors, decorations, clipping, and single-pass references next | Yes |
| Q09 | Partially closed: preservation and sync regressions pass in actual InDesign | Eleven safety checks and 16 sync/IDML checks pass in 21.5.1.73. PDF has 14 unreadable/unsupported preference properties, EPS one; 13 outline transaction checks also pass; broader injected cleanup failures need host proof | Yes for remaining mutation paths |
| Q10 | What is the actual version compatibility envelope? | Build resources and required suites, then isolated per-version load/edit/export tests | Release |
| Q11 | What survives missing-plugin open, copy/paste, and native document round-trips? | New script pair tokens survive measured IDML import/resync; native missing-plugin and broader clipboard/AI/INDD tests remain open | Release |
| Q12 | Identity routes are documented; assigned prefix and account-specific packaging remain open | [Distribution audit](docs/PLUGIN_DISTRIBUTION.md) separates native prefix, effect identity, package identity, signing, and Marketplace review; no application submitted | Distribution |
| Q13 | CLOSED as a bounded primary-source history audit | [History](docs/MITER_JOIN_HISTORY.md) links verified sources and qualifies unverified anecdotes; documentation is not renderer execution | No |
| Q14 | Is whole-path clipping stable on curves, holes, transforms, and extreme scales? | Adaptive approximation/error policy, topology oracle, deterministic fuzzing; local straight-join tests are insufficient | Yes |
| Q15 | Does implementation respect edit/undo/document lifetimes? | Repeated type/edit/undo/reopen/delete/cancel cycles; leak and callback tracing | Release |
| Q16 | Are font, language, writing-direction, and complex Appearance limits explicit? | Serif/sans/script/variable/extreme fonts, shaping, and vertical fixtures, alongside synthetic geometry | Release |
| Q17 | Why does outlining change the extreme font's contour census? | Embedded version 1.000: 3,552 contours; extraction: 3,336. Two-point contour counts do not establish a removal mapping. [Provenance](tests/geometry/extreme_font_provenance.json) | Full-font pass |
| Q18 | Why do MuPDF and Poppler disagree on the same native extreme PDF? | At 864 dpi, thresholded crop XOR is 23,454 pixels. Isolate glyphs, stroker parameters, curve flattening, and degenerate outlines | Renderer agreement |
