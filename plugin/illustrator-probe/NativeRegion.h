// Whether a path's contours overlap inside its own fill (2026-10-16), for the native route: an
// Inside stroke on such a path is drawn along every contour edge, including the edges inside the
// filled region, which the owner's rule leaves unstroked. Illustrator's own Effect > Pathfinder >
// Add unites the contours first (native-fix). No host types.
#pragma once
#include "../source/IseRegionCore.h"
#include <cmath>
#include <vector>

namespace native_region {

// The total length of the contours, flattened within `tol`.
inline double Perimeter(const std::vector<ise_miter::Contour>& contours, double tol)
{
    double total = 0;
    for (const auto& r : ise_miter::Flatten(contours, tol, 1e-9))
        for (size_t i = 0; i < r.points.size(); ++i) {
            const auto a = r.points[i], b = r.points[(i + 1) % r.points.size()];
            total += std::hypot(b.x - a.x, b.y - a.y);
        }
    return total;
}

// Whether the contours have edges inside their own nonzero fill: the filled region's boundary is
// shorter than the contours together by more than `slack`. A counter (a hole wound the other
// way) is boundary, so a letter's outline has none; two overlapping squares do. Open contours are
// closed as the fill closes them.
inline bool InternalEdges(const std::vector<ise_miter::Contour>& contours, double tol, double slack)
{
    if (contours.size() < 2) return false;
    const auto region = ise_region::NonzeroRegion(contours, tol);
    if (region.empty()) return false;
    return Perimeter(contours, tol) - Perimeter(region, tol) > slack;
}

// Whether shrinking the contours' nonzero fill by `w` leaves nothing: every part is thinner than
// 2w. Illustrator's Offset Path returns the shape unshrunk then (measured 2026-10-16: Minion
// "Ax" at 96 pt, -1, -2, -4 shrink it to 997, 460, 9 pt2; -8 gives back 1,602 of 1,614), so an
// Inside drawn as two fills shows the band color alone in that case. False when the contours
// fill nothing. (WordPath reports an inset that leaves nothing as a failure, so the region is
// checked first.)
inline bool InsetEmpty(const std::vector<ise_miter::Contour>& contours, double w, double tol = 0.01)
{
    if (contours.empty() || !(w > 0) || ise_region::NonzeroRegion(contours, tol).empty()) return false;
    bool ok = false;
    const auto inset = ise_region::WordPath(contours, -w, ise_region::BandJoin::Miter, 4, false, tol, &ok);
    return !ok || inset.empty();
}

} // namespace native_region
