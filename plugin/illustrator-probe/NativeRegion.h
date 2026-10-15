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

} // namespace native_region
