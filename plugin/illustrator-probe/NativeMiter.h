// The owner's miter rulings as one native miter limit per path or character (2026-10-15).
//
// InDesign's plugin draws each corner by rule (plugin/source/IseMiterCore.h): within the
// limit, a glyph tip is trimmed (TrimmedTip) and every other corner keeps its miter; past
// the limit, a shape's own point between straight edges is kept whole (WholePoint) and any
// other corner is beveled; and a Center stroke's interior miters are clipped to the fill.
// Illustrator draws one rule per path, or per character of text: miter up to its limit L,
// bevel past it. That is a native attribute, so it follows edits with no plugin. One L
// reproduces the rulings exactly when every corner that must stay mitered has a smaller
// ratio than every corner that must be cut; otherwise the limit that leaves the fewest
// corners wrong is taken (ties: the limit nearest the current one). A trimmed tip is beveled
// natively where InDesign cuts it flush one half width past its vertex; both remove the
// spike.
//
// No host types: the corners come from ise_miter::Corners.
#pragma once
#include "../source/IseMiterCore.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace native_miter {

struct Choice {
    double limit = 4;     // the miter limit to set
    int kept = 0;         // corners that must keep their miter
    int cut = 0;          // corners that must be beveled
    int keptBeveled = 0;  // of the kept, beveled by `limit` (a ruling not met)
    int cutMitered = 0;   // of the cut, mitered by `limit` (a ruling not met)
};

// Illustrator's range for a miter limit.
constexpr double kMinLimit = 1, kMaxLimit = 500;

// Whether an exterior corner must keep its miter under limit `limit` and half width h. Live
// text (`wholePoints` false) keeps no point past the run's own limit: InDesign's plugin strokes
// a text run with the run's own limit and keeps whole points only on shapes and outlined text
// (owner's question on the German Beauty bevels, 2026-10-30).
// `glyphTips` false (the panel's Trim glyph tips off): every corner within the limit keeps its miter.
inline bool MustKeep(const ise_miter::Stub& s, double h, double limit, bool wholePoints = true, bool glyphTips = true)
{
    if (s.ratio <= limit) return !glyphTips || !ise_miter::TrimmedTip(s, h);
    return wholePoints && ise_miter::WholePoint(s, h) && s.ratio <= kMaxLimit;
}

// The limit for corners that must keep their miter (`keep`) and corners that must be cut
// (`cut`), by ratio, given the current limit.
inline Choice ChooseFromRatios(const std::vector<double>& keep, const std::vector<double>& cut, double limit)
{
    Choice best;
    best.limit = std::min(std::max(limit, kMinLimit), kMaxLimit);
    best.kept = static_cast<int>(keep.size());
    best.cut = static_cast<int>(cut.size());
    if (keep.empty() && cut.empty()) return best;
    // Candidates: the current limit, just above every kept ratio (a limit equal to a ratio
    // still miters it, as the renderer compares), and just below every cut ratio.
    std::vector<double> candidates{best.limit};
    for (double r : keep) candidates.push_back(std::min(r * (1 + 1e-6), kMaxLimit));
    for (double r : cut) candidates.push_back(std::max(r * (1 - 1e-6), kMinLimit));
    int bestWrong = -1;
    for (double L : candidates) {
        if (L < kMinLimit || L > kMaxLimit) continue;
        int kb = 0, cm = 0;
        for (double r : keep) kb += r > L ? 1 : 0;
        for (double r : cut) cm += r <= L ? 1 : 0;
        const int wrong = kb + cm;
        const bool better = bestWrong < 0 || wrong < bestWrong ||
            (wrong == bestWrong && std::fabs(L - limit) < std::fabs(best.limit - limit));
        if (better) { bestWrong = wrong; best.limit = L; best.keptBeveled = kb; best.cutMitered = cm; }
    }
    // Every ruling met: between the largest kept and the smallest cut ratio, keeping the
    // current limit when it already lies there; with nothing to cut, just above the largest
    // kept ratio, so a sharper corner added by a later edit is beveled rather than drawn as
    // a spike up to 500.
    if (bestWrong == 0) {
        double lo = kMinLimit, hi = kMaxLimit;
        for (double r : keep) lo = std::max(lo, r);
        for (double r : cut) hi = std::min(hi, r);
        if (lo < hi) {
            const double target = cut.empty() ? std::max(limit, lo * (1 + 1e-3)) : (lo + hi) / 2;
            // The current limit is kept only with a margin over the largest kept ratio, as the
            // candidates have, in case the host compares in single precision.
            best.limit = (limit >= lo * (1 + 1e-6) && limit < hi) ? limit : std::min(std::max(target, kMinLimit), kMaxLimit);
        }
    }
    return best;
}

// The limit for a set of exterior corners at half width h (ise_miter::ExteriorCorners).
inline Choice ChooseLimit(const std::vector<ise_miter::Stub>& corners, double h, double limit)
{
    if (!(h > 0) || !std::isfinite(h) || corners.empty()) {
        Choice c;
        c.limit = std::min(std::max(limit, kMinLimit), kMaxLimit);
        return c;
    }
    std::vector<double> keep, cut;
    for (const auto& s : corners) {
        if (!std::isfinite(s.ratio)) continue;
        (MustKeep(s, h, limit) ? keep : cut).push_back(s.ratio);
    }
    return ChooseFromRatios(keep, cut, limit);
}

// How far an interior join's miter runs inside the fill before it comes out the far side:
// from v along `into` (unit, pointing into the fill), the first distance at which the point
// is outside the filled region (nonzero), sampled every `step` up to `limit`; `limit` when it
// never leaves.
inline double DepthInside(const ise_miter::RingGrid& grid, ise_miter::Pt v, ise_miter::Pt into, double step, double limit)
{
    for (double d = step; d <= limit; d += step)
        if (grid.Winding(v + into * d) == 0) return d;
    return limit;
}

// The limit for a whole outline at half width h. Besides the exterior corners, a Center
// stroke's interior join (a concave corner, such as the crotch of a v) draws its miter into
// the fill, where the fill or the stroke over it hides it; but in a thin part the miter runs
// through and out the far side as a spike (Minion's v and w at Center 8 on 36 pt,
// 2026-10-15). InDesign's plugin clips interior miters to the fill; natively, such a corner
// is cut (beveled) when its miter would reach past the far side's own stroke band (depth
// plus h), at any ratio, since a whole point elsewhere can raise the limit above it.
//   interior     the stroke draws interior joins: Center and Inside (an Outside stroke
//                draws none on its side)
//   hiddenInside the fill paints above the stroke (the text routes): an interior miter that
//                stays inside the fill is hidden and constrains nothing
//   clipped      the host clips the stroke to the fill (Inside): nothing escapes, and an
//                interior miter is kept within the limit and beveled past it, as InDesign
// Otherwise an interior miter that stays inside is visible: kept within the limit, and cut
// past it (InDesign bevels it there).
//   wholePoints  false for live text (see MustKeep); glyphTips false keeps tips within the limit
inline Choice ChooseLimitForOutline(const std::vector<ise_miter::Contour>& contours, double h, double limit, bool interior,
                                    bool hiddenInside = false, bool clipped = false, bool wholePoints = true,
                                    bool glyphTips = true)
{
    if (!(h > 0) || !std::isfinite(h)) return ChooseLimit({}, h, limit);
    ise_miter::Options o;
    o.halfWidth = h;
    const std::vector<ise_miter::Stub> corners = ise_miter::Corners(contours, o, interior);
    std::vector<double> keep, cut;
    // RingGrid keeps a reference to its rings, so they live here (a temporary dangled).
    std::vector<ise_miter::Ring> rings;
    std::unique_ptr<ise_miter::RingGrid> grid;
    const double eps = 1e-9 * std::max(1.0, h);
    for (const auto& s : corners) {
        if (!std::isfinite(s.ratio)) continue;
        if (!s.interior) { (MustKeep(s, h, limit, wholePoints, glyphTips) ? keep : cut).push_back(s.ratio); continue; }
        if (s.ratio > kMaxLimit) continue;   // beveled at any limit
        bool escapes = false;
        if (!clipped) {
            if (!grid) { rings = ise_miter::Flatten(contours, 0.001 * h, eps); grid.reset(new ise_miter::RingGrid(rings)); }
            const double reach = s.ratio * h;
            escapes = reach > DepthInside(*grid, s.v, s.outward, h / 16, reach + h) + h;
        }
        if (escapes) cut.push_back(s.ratio);
        else if (!hiddenInside) (s.ratio <= limit ? keep : cut).push_back(s.ratio);
    }
    return ChooseFromRatios(keep, cut, limit);
}

} // namespace native_miter
