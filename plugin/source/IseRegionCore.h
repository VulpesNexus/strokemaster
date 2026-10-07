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

/** The regions of the bands of a stroke, by signed distance from the boundary of the nonzero
    fill of `fill` (outside positive, inside negative): for each [from, to] of `bands`, every
    point whose distance lies between them, with the join (BandJoin) at the offsets' corners
    (outward, the miters of `corners` added; inward, mitered within `limit`), so each band
    follows one outline around touching and overlapping contours, into the notches where they
    meet, and nested bands keep one another's points. The distance 0 is taken `inset` into the
    fill, so that no seam shows between an Outside stroke's innermost band and the fill drawn
    beneath; Center and Inside bands, which cross or lie within the fill, pass 0. As contours of cubic
    curves fitted to the offsets within `tol` (FitCurves), wound for the nonzero rule; empty, and
    `ok` false, when Clipper2 fails. */
std::vector<std::vector<ise_miter::Contour>> DistanceBands(const std::vector<ise_miter::Contour>& fill,
                                                           const std::vector<std::pair<double, double>>& bands, BandJoin join,
                                                           const std::vector<BandCorner>& corners, double inset, double tol,
                                                           bool* ok, double limit = 4);

/** The path a pattern stroker follows around one word: the nonzero fill of `fill` grown by
    `half` with the join (a miter squared past `limit`); for a Center stroke (half 0) its boundary
    itself, for an Inside stroke (half < 0) shrunk, without creases. With `creases` (contours whose strokes
    meet, ContoursInteract), then closed by a disk of radius `half`, which rounds the creases
    where two letters' offsets meet and the corners of a counter's offset; the pattern broke
    into spikes and restarted at every crease of InDesign's own offset (the owner's check,
    2026-10-06). A counter's offset smaller than that disk, which the closing fills, is kept
    as grown (the owner's check, 2026-10-07); a counter narrower than the stroke has none. Otherwise a shape's own concave
    corners stay sharp, as InDesign strokes them. As contours of cubic curves fitted within
    `tol` (FitCurves), corners kept sharp; empty, and `ok` false, when Clipper2 fails. */
std::vector<ise_miter::Contour> WordPath(const std::vector<ise_miter::Contour>& fill, double half, BandJoin join, double limit,
                                         bool creases, double tol, bool* ok);

/** What a stroke of half width `half` along `path` (WordPath) leaves unpainted between it and
    the fill: every point of the region `path` encloses (even-odd) that lies neither in the fill
    nor within `half` of the path, the notches where letters meet included. Grown by `grow` all
    round, so it reaches under the stroke beside it and, for the caller's clip to the outside of
    the fill, into the fill. Contours fitted within `tol`; empty where there is nothing, and `ok`
    false when Clipper2 fails. */
std::vector<ise_miter::Contour> Uncovered(const std::vector<ise_miter::Contour>& fill, const std::vector<ise_miter::Contour>& path,
                                          double half, double grow, double tol, bool* ok);

/** What an Inside stroke of half width `half` along the fill shrunk by `half` (WordPath) leaves
    unpainted of the nonzero fill of `fill`: the parts thinner than the stroke (2 half), which
    the shrunk path drops; the fill less its opening by a disk of radius half, regrown with
    miter joins so that sharp corners are not counted. Grown by `grow`, within the fill.
    Contours fitted within `tol`; empty where there is nothing, and `ok` false when Clipper2
    fails. */
std::vector<ise_miter::Contour> UncoveredWithin(const std::vector<ise_miter::Contour>& fill, double half, double grow, double tol,
                                                bool* ok);

/** Whether a stroke `reach` either side of each contour would overlap another contour's: the
    contours' bands, each within reach of its own line, overlap by more than slivers. Where they
    do not, the word's path keeps a shape's own concave corners sharp (WordPath). True for more
    than 400 contours, without measuring. */
bool ContoursInteract(const std::vector<ise_miter::Contour>& contours, double reach, double tol);

/** A closed polyline (at least three points) as cubic curves fitted within `tol` (Schneider's
    method), split at its corners (turning more than 30 degrees), which are kept sharp. */
ise_miter::Contour FitCurves(const std::vector<ise_miter::Pt>& ring, double tol);

} // namespace ise_region
