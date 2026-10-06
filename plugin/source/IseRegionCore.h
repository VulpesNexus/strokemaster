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

#include <utility>
#include <vector>

namespace ise_region {

/** The region the contours fill under the nonzero rule, as contours wound counterclockwise
    (positive area) around filled parts and clockwise around holes, none overlapping another.
    `tol` is the flattening tolerance; empty when the outline fills nothing or the union fails. */
std::vector<ise_miter::Contour> NonzeroRegion(const std::vector<ise_miter::Contour>& contours, double tol);

/** Whether the contours fill the same region under the nonzero and even-odd rules: their
    flattened regions (tolerance `tol`) differ by no more than slivers 4 tol thick. Exact where
    ise_miter::FillRulesAgree probes: probe points missed two overlapping parallelograms, whose
    every edge midpoint lay on the other's edge (coverage census, 2026-10-04). */
bool FillRulesAgree(const std::vector<ise_miter::Contour>& contours, double tol);

/** Whether `clip` (ise_miter::OutsideClip: a frame, then contours) reads, under the nonzero and
    the even-odd rule alike, as the frame less the contours' nonzero fill, up to slivers 12 tol
    thick (flattening, and a resolved region's joins at crossings): the check OutsideClip's
    probes approximate. */
bool ClipsOutside(const std::vector<ise_miter::Contour>& clip, const std::vector<ise_miter::Contour>& contours, double tol);

/** What StrokeClip does with the fill: adds it (a Center stroke lies over its fill), takes it
    away (an Outside stroke lies outside it), or keeps only what lies within it (Inside). */
enum class FillMode { Add, Remove, Within };

/** Where a stroke of half width h along `stroked` may paint, as polygons that read the same under
    either fill rule, for clipping a stroke InDesign draws itself (dashes, stripes): every point
    within h + margin of the boundary of the nonzero fill of `fill` on the side `mode` says (the
    fill itself added for Add, taken away for Remove; for Within, the band inside the fill only),
    the polygons `extra` (joins that reach past that band; for Within, their part in the fill),
    and around an open contour of `stroked`, every point within h + margin of its line. Empty
    when there is neither a fill nor an open contour, or Clipper2 fails. */
std::vector<ise_miter::Contour> StrokeClip(const std::vector<ise_miter::Contour>& stroked, double h, double margin,
                                           const std::vector<std::vector<ise_miter::Pt>>& extra,
                                           const std::vector<ise_miter::Contour>& fill, FillMode mode, double tol);

/** The join the bands (DistanceBands) give their offsets' corners: rounded, beveled, or for a
    miter squared off at the offset's own distance, with the miters of BandCorner added. */
enum class BandJoin { Round, Bevel, Miter };

/** A corner of the fill whose miter the bands keep: at distance d its miter (ise_miter::CutMiter
    with half width d) is cut min(ratio d, cap d + extra) from the vertex. A whole miter has cap
    = ratio (or the longest limit's) and no extra; a miter trimmed to its shorter edge past the
    stroke (an effect's corner) has cap 1 and the edge as extra. */
struct BandCorner {
    ise_miter::Stub stub;
    double cap = 0;
    double extra = 0;
};

/** The regions of the bands of an Outside stroke, by distance from the fill: for each [from,
    to] of `bands`, every point outside the nonzero fill of `fill` whose distance from it lies
    between them, with the join (BandJoin) at the offsets' corners, so each band follows one
    outline around touching and overlapping contours, into the notches where they meet, and
    nested bands keep one another's points. A band from 0 reaches `inset` into the fill, so that
    no seam shows between it and the fill drawn beneath. As contours of cubic curves fitted to
    the offsets within `tol` (FitCurves), wound for the nonzero rule; empty, and `ok` false, when
    Clipper2 fails. */
std::vector<std::vector<ise_miter::Contour>> DistanceBands(const std::vector<ise_miter::Contour>& fill,
                                                           const std::vector<std::pair<double, double>>& bands, BandJoin join,
                                                           const std::vector<BandCorner>& corners, double inset, double tol,
                                                           bool* ok);

/** The notches a stroke along the fill's offset by `half` leaves: every point outside the fill,
    within `half` of it, that lies farther than `half` from that offset (the fill's closing by a
    disk of radius half, less the fill), where two contours meet at a sharp angle or a counter is
    narrower than the stroke. Grown by `grow` all round, so they reach under the stroke beside
    them and, for the caller's clip to the outside of the fill, into the fill. Contours fitted
    within `tol`; empty where there are none, and `ok` false when Clipper2 fails. */
std::vector<ise_miter::Contour> Notches(const std::vector<ise_miter::Contour>& fill, double half, double grow, double tol, bool* ok);

/** A closed polyline (at least three points) as cubic curves fitted within `tol` (Schneider's
    method), split at its corners (turning more than 30 degrees), which are kept sharp. */
ise_miter::Contour FitCurves(const std::vector<ise_miter::Pt>& ring, double tol);

} // namespace ise_region
