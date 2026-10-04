//========================================================================================
// The region an outline fills, resolved into contours that do not overlap. Host-independent:
// standard C++ and Clipper2 (plugin/third_party/clipper2), so the same code compiles into the
// plugin and into tests/native-repaint/MiterCoreTests.cpp.
//
// A page item's outline may overlap itself: Create Outlines keeps a decorative font's
// overlapping glyph contours (Kerberos Fang), and the nonzero and even-odd regions then differ.
// InDesign fills such an outline under the nonzero rule (`f`, measured 2026-10-04), but a clip
// to the outside of that region cannot be built from the contours themselves: no winding of
// them reads 0 exactly where the fill does. The region's own boundary can: its outer contours
// and holes nest without overlapping, so both fill rules agree on it (ise_miter::OutsideClip).
//
// The boundary is found on the flattened outline (Clipper2's union under the nonzero rule)
// and then rebuilt from the outline's own segments: every vertex Clipper2 outputs carries the
// place on the outline it came from (its Z value), so each run of the boundary along one
// segment becomes that segment's piece, curve and all. Only where contours cross does the
// boundary depend on the flattening, by at most its tolerance.
//========================================================================================
#pragma once

#include "IseMiterCore.h"

#include <vector>

namespace ise_region {

/** The region the contours fill under the nonzero rule, as contours wound counterclockwise
    (positive area) around filled parts and clockwise around holes, none overlapping another.
    `tol` is the flattening tolerance; empty when the outline fills nothing or the union fails. */
std::vector<ise_miter::Contour> NonzeroRegion(const std::vector<ise_miter::Contour>& contours, double tol);

} // namespace ise_region
