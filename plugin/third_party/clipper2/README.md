# Clipper2

Angus Johnson's polygon clipping library, release `Clipper2_1.5.4` (commit `ef88ee97c0e759792e43a2b2d8072def6c9244e8`), from <https://github.com/AngusJohnson/Clipper2>. Boost Software License 1.0, in *LICENSE*.

Only the C++ engine and offsetter are kept: *include/clipper2/* (without *clipper.export.h*), *src/clipper.engine.cpp*, and *src/clipper.offset.cpp* (added 2026-10-04 from the same commit), with line endings converted to LF and no other change. The plugin compiles them with Z values (`USINGZ`) through *plugin/source/IseClipper2.cpp* and uses them in *plugin/source/IseRegionCore.cpp* to resolve an overlapping outline into the region it fills, compare fill rules, and bound the strokes InDesign draws itself.
