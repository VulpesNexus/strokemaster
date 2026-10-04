# Clipper2

Angus Johnson's polygon clipping library, release `Clipper2_1.5.4` (commit `ef88ee97c0e759792e43a2b2d8072def6c9244e8`), from <https://github.com/AngusJohnson/Clipper2>. Boost Software License 1.0, in *LICENSE*.

Only the C++ engine is kept: *include/clipper2/* (without *clipper.export.h*) and *src/clipper.engine.cpp*, with line endings converted to LF and no other change. The plugin compiles it with Z values (`USINGZ`) through *plugin/source/IseClipper2.cpp* and uses it in *plugin/source/IseRegionCore.cpp* to resolve an overlapping outline into the region it fills.
