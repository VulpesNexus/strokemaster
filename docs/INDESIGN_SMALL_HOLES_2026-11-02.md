# Small enclosed holes in Close holes

The native lettering exports exposed small white cavities that remained when Close holes was enabled. This continuation fixes the reproduced core defect and verifies the reported cavities in genuine InDesign PDF output. The candidate was installed temporarily for testing; the original production plugin was restored afterward.

## Cause and change

*IseRegionCore.cpp* previously collected holes only after enlarging the stroke reach by the greater of three tolerances or five percent. That enlargement intentionally closes narrow slits between nearly touching strokes, but it can also erase a small enclosed cavity before hole collection. A four-bar fixture with a 1 pt square cavity at a 12 pt reach reproduces the loss.

The revised *Silhouette* collects holes at both the original reach and the enlarged reach, using the existing round, bevel, and bounded miter construction. Original-reach holes overlap the stroke by one tolerance; enlarged-reach holes retain their previous overlap distance. The union of those hole regions is fitted once. The existing exterior silhouette and hollow-letter subtraction remain in place.

## Evidence

The new regression failed against the previous implementation, then passed with the fix. The nine added checks cover all three joins, tight cavity boundaries, reversed winding, and overlapping detections resolving to one region with winding one. Existing near-touching, open-gap, miter-limit, hollow-letter, and exterior-corner controls also pass.

The full native run passed 33 refusal checks, 65,585 stroke-behind checks, 65 cross-frame checks, 344 miter checks, 51 Illustrator contour checks, and 60 settings checks. Peak job memory was 0.46 GB under a 1 GB cap, with the standard 4 GB reserve. The later closeout fixes the toolchain initialization warning by giving the compiler child a minimal Windows PATH before MSVC setup.

An independent source review found no correctness defect. The focused whitespace check passed. Reproduction and passing logs are retained under *build/pinhole-fix-2026-11-02/*.

All three InDesign variants compiled successfully: Route W (`888881382da2`), Route X (`e1cb9e1bc13b`), and Route X Research (`2c743b4a326b`). Full hashes and successful exits are recorded in *build/pinhole-fix-2026-11-02/build-results.json*.

## InDesign verification

Status: **DONE** for the reported pinhole regression. The owner's request to complete the remaining tests authorized a temporary candidate installation. The saved synthetic *names10/name-examples.indd* was opened without saving or changing its 60 pt spacing and exported in native, default, and Close holes states. The tested engine was `2c743b4a326b56a80d2e7354b188839ce34e894a69daa25b1636d01a2286bd10`.

At both 360 and 720 dpi, native and default output match the previous production exports pixel for pixel in the measured top-cell region. Close holes removes the reported cavities while preserving the dark glyph-fill pixel count exactly. The only residual thresholded white component is a 0.01 pt² single-pixel speck elsewhere at 720 dpi; independently rendering that location at 1,440 and 2,880 dpi yields no enclosed white area. Raw results retain this distinction rather than claiming that every low-resolution white pixel disappeared.

Evidence is in *build/host-final-2026-11-02/candidate-1/lettering-measurement.json* and its *lettering/* PDFs. Production was restored and hash-verified as `6b9e422fa1ea359931ea9c2fac4c2ed01eab27896c569e8e9b49ce78a7ff287d`; no workspace snapshot remained. This pinhole result does not promote the combined candidate; the separate artwork-removal defect and its subsequent fixes are covered in the [closeout review](REVIEW_2026-11-02.md). Installing a future accepted candidate requires an InDesign relaunch.
