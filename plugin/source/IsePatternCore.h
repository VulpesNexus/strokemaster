//========================================================================================
// A stroke pattern (dashes, dots, any marks a path stroker paints) laid along a path by runs
// between its real corners. Host-independent: standard C++ on ise_miter's paths, so the same
// code compiles into the plugin and into tests/native-repaint/MiterCoreTests.cpp.
//
// InDesign's corner adjustment fits a dash pattern between every two anchors of a path, so
// along the outline of script, whose anchors lie a few points apart, the dashes run together
// solid (the owner's check of the corner adjustment study, 2026-10-06). Here the path is cut at
// its real corners only (turning more than 30 degrees; a contour without one is a single run
// around), the stroker paints a square whose sides are as long as each run into a recording
// port, and what it painted along the first side is laid along the run: a mark on the line (a dash, a round-capped dot) as the
// run's own piece, any other mark (a filled dot, a hash) turned to the run's direction at its
// middle. A dash that ends one run at a corner and the dash that starts the next become one
// piece through the corner, stroked with the join, as a pattern fitted to a shape's corners
// draws it. So the stroker's own pattern and its own corner adjustment are kept, applied at
// real corners only.
//========================================================================================
#pragma once

#include "IseMiterCore.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace ise_pattern {

using ise_miter::Contour;
using ise_miter::Pt;
using ise_miter::Segment;

/** A path from one point along segments, parametrized by length (sampled: 32 steps a curve). */
class Arc {
public:
    Arc() = default;
    Arc(Pt start, std::vector<Segment> segments) : fStart(start), fSegments(std::move(segments))
    {
        Pt from = fStart;
        double total = 0;
        for (const Segment& s : fSegments) {
            std::vector<std::pair<double, double>> table{{0.0, total}};   // (t, length so far)
            const int n = s.curve ? 32 : 1;
            Pt prev = from;
            for (int k = 1; k <= n; ++k) {
                const double t = static_cast<double>(k) / n;
                const Pt q = Eval(from, s, t);
                total += ise_miter::Len(q - prev);
                table.push_back({t, total});
                prev = q;
            }
            fTables.push_back(std::move(table));
            fFroms.push_back(from);
            from = s.p;
        }
        fLength = total;
    }

    double Length() const { return fLength; }
    Pt Start() const { return fStart; }
    const std::vector<Segment>& Segments() const { return fSegments; }

    /** The point at length `s` along the arc (clamped), and the unit direction there. */
    void At(double s, Pt* at, Pt* dir) const
    {
        size_t i = 0;
        double t = 0;
        Locate(s, &i, &t);
        if (fSegments.empty()) { *at = fStart; *dir = {1, 0}; return; }
        const Segment& seg = fSegments[i];
        const Pt from = fFroms[i];
        *at = Eval(from, seg, t);
        Pt d = Derivative(from, seg, t);
        if (!(ise_miter::Len(d) > 1e-12)) d = seg.p - from;   // a cusp: the chord's way
        const double n = ise_miter::Len(d);
        *dir = n > 0 ? d * (1 / n) : Pt{1, 0};
    }

    /** The arc's part from length s0 to s1 (s0 <= s1, clamped), as an open contour of the
        arc's own segments, cut. Of zero length, a line from the point a hair along the arc. */
    Contour Piece(double s0, double s1) const
    {
        Contour c;
        c.closed = false;
        s0 = std::max(0.0, std::min(s0, fLength));
        s1 = std::max(s0, std::min(s1, fLength));
        Pt at, dir;
        At(s0, &at, &dir);
        c.start = at;
        if (fSegments.empty() || s1 - s0 <= 1e-9) {
            Segment s;
            s.p = at + dir * 1e-4;
            c.segments.push_back(s);
            return c;
        }
        size_t i0 = 0, i1 = 0;
        double t0 = 0, t1 = 0;
        Locate(s0, &i0, &t0);
        Locate(s1, &i1, &t1);
        for (size_t i = i0; i <= i1; ++i) {
            const double a = i == i0 ? t0 : 0, b = i == i1 ? t1 : 1;
            if (b - a <= 1e-12 && !(i0 == i1)) continue;
            c.segments.push_back(Sub(fFroms[i], fSegments[i], a, b));
        }
        if (c.segments.empty()) { Segment s; s.p = at + dir * 1e-4; c.segments.push_back(s); }
        return c;
    }

private:
    static Pt Eval(Pt from, const Segment& s, double t)
    {
        if (!s.curve) return ise_miter::Lerp(from, s.p, t);
        const double r = 1 - t;
        return from * (r * r * r) + s.c1 * (3 * r * r * t) + s.c2 * (3 * r * t * t) + s.p * (t * t * t);
    }
    static Pt Derivative(Pt from, const Segment& s, double t)
    {
        if (!s.curve) return s.p - from;
        const double r = 1 - t;
        return (s.c1 - from) * (3 * r * r) + (s.c2 - s.c1) * (6 * r * t) + (s.p - s.c2) * (3 * t * t);
    }
    /** The segment [a, b] of a segment starting at `from`, as a segment. */
    static Segment Sub(Pt from, const Segment& s, double a, double b)
    {
        Segment out;
        if (!s.curve) { out.p = ise_miter::Lerp(from, s.p, b); return out; }
        Pt q0 = from, q1 = s.c1, q2 = s.c2, q3 = s.p;
        if (a > 0) {
            Pt n0, n1, n2;
            ise_miter::CubicTail(q0, q1, q2, q3, a, &n0, &n1, &n2);
            q0 = n0; q1 = n1; q2 = n2;
        }
        const double tb = a < 1 ? (b - a) / (1 - a) : 1;
        if (tb < 1) {
            Pt n1, n2, n3;
            ise_miter::CubicHead(q0, q1, q2, q3, tb, &n1, &n2, &n3);
            q1 = n1; q2 = n2; q3 = n3;
        }
        out.curve = true;
        out.c1 = q1; out.c2 = q2; out.p = q3;
        return out;
    }
    void Locate(double s, size_t* index, double* t) const
    {
        *index = 0; *t = 0;
        if (fSegments.empty()) return;
        s = std::max(0.0, std::min(s, fLength));
        for (size_t i = 0; i < fTables.size(); ++i) {
            const auto& tab = fTables[i];
            if (s > tab.back().second && i + 1 < fTables.size()) continue;
            for (size_t k = 1; k < tab.size(); ++k) {
                if (s > tab[k].second && k + 1 < tab.size()) continue;
                const double l0 = tab[k - 1].second, l1 = tab[k].second;
                const double f = l1 > l0 ? std::max(0.0, std::min(1.0, (s - l0) / (l1 - l0))) : 0;
                *index = i;
                *t = tab[k - 1].first + f * (tab[k].first - tab[k - 1].first);
                return;
            }
        }
        *index = fSegments.size() - 1; *t = 1;
    }

    Pt fStart;
    std::vector<Segment> fSegments;
    std::vector<std::vector<std::pair<double, double>>> fTables;
    std::vector<Pt> fFroms;
    double fLength = 0;
};

/** A run of a closed contour between two real corners, or (`loop`) all the way round a contour
    without one, from its start back to it. */
struct Run {
    Arc arc;
    bool loop = false;
};

/** The runs of a closed contour between its real corners: where its direction turns by more
    than `cornerDegrees` from one segment to the next. */
inline std::vector<Run> CornerRuns(const Contour& c, double cornerDegrees = 30)
{
    std::vector<Run> out;
    const std::vector<Segment> segs = ise_miter::Closed(c, 1e-9);
    if (segs.empty()) return out;
    const size_t n = segs.size();
    std::vector<Pt> from(n);
    from[0] = c.start;
    for (size_t i = 1; i < n; ++i) from[i] = segs[i - 1].p;
    const double cosLimit = std::cos(cornerDegrees * 3.14159265358979323846 / 180);
    // corner[i]: the vertex where segment i starts.
    std::vector<char> corner(n, 0);
    for (size_t i = 0; i < n; ++i) {
        const size_t prev = (i + n - 1) % n;
        const Pt a = ise_miter::EndTangent(from[prev], segs[prev], 1e-9), b = ise_miter::StartTangent(from[i], segs[i], 1e-9);
        const double la = ise_miter::Len(a), lb = ise_miter::Len(b);
        if (!(la > 0) || !(lb > 0)) continue;
        corner[i] = ise_miter::Dot(a, b) < cosLimit * la * lb ? 1 : 0;
    }
    size_t first = n;
    for (size_t i = 0; i < n; ++i) if (corner[i]) { first = i; break; }
    if (first == n) {
        Run r;
        r.arc = Arc(c.start, segs);
        r.loop = true;
        out.push_back(r);
        return out;
    }
    size_t i = first;
    do {
        std::vector<Segment> run;
        const Pt start = from[i];
        size_t k = i;
        do {
            run.push_back(segs[k]);
            k = (k + 1) % n;
        } while (!corner[k]);
        Run r;
        r.arc = Arc(start, run);
        out.push_back(r);
        i = k;
    } while (i != first);
    return out;
}

/** What a stroker painted along a straight line from (0, 0) to (length, 0): one paint (a fill
    or a stroke) with its path, in the line's space. */
struct Mark {
    bool stroke = true;
    double width = 0;              // a stroke's width
    int cap = 0, join = 0;         // a stroke's cap and join, as the port takes them
    std::vector<double> dash;      // a dash array set for the stroke, if any
    double phase = 0;
    std::vector<Contour> path;
};

/** Marks laid along runs: strokes (a width, cap, and join, and their pieces) and fills. */
struct Laid {
    struct Stroke {
        double width = 0;
        int cap = 0, join = 0;
        std::vector<Contour> pieces;
    };
    std::vector<Stroke> strokes;
    std::vector<Contour> fills;
    bool Empty() const { return strokes.empty() && fills.empty(); }
};

namespace detail {

/** The points a contour is drawn through (its start, controls, and ends). */
inline void Points(const Contour& c, std::vector<Pt>* out)
{
    out->push_back(c.start);
    for (const Segment& s : c.segments) {
        if (s.curve) { out->push_back(s.c1); out->push_back(s.c2); }
        out->push_back(s.p);
    }
}

/** A contour as a polyline, when every segment is straight: a curve whose controls lie on its
    chord counts (InDesign's strokers write straight pieces as such curves). Its points, the
    closing line included. False for a real curve. */
inline bool Polyline(const Contour& c, double eps, std::vector<Pt>* out)
{
    out->clear();
    out->push_back(c.start);
    Pt from = c.start;
    for (const Segment& s : c.segments) {
        if (s.curve) {
            const Pt d = s.p - from;
            const double n = ise_miter::Len(d);
            auto off = [&](Pt q) { return n > 1e-12 ? std::fabs(ise_miter::Cross(q - from, d)) / n : ise_miter::Len(q - from); };
            if (off(s.c1) > eps || off(s.c2) > eps) return false;
        }
        out->push_back(s.p);
        from = s.p;
    }
    if (c.closed && ise_miter::Len(out->back() - c.start) > 1e-12) out->push_back(c.start);
    return true;
}

/** The intervals along a length `total` that a dash array paints (PostScript's: from the
    start, offset by phase); all of it without one. */
inline std::vector<std::pair<double, double>> OnIntervals(double total, const std::vector<double>& dash, double phase)
{
    std::vector<std::pair<double, double>> on;
    double period = 0;
    for (double d : dash) period += std::max(0.0, d);
    if (dash.empty() || !(period > 0)) {
        on.push_back({0, total});
        return on;
    }
    double s = -std::fmod(std::max(0.0, phase), period);
    size_t k = 0;
    for (int guard = 0; s <= total && guard < 1000000; ++guard) {
        const double d = std::max(0.0, dash[k % dash.size()]);
        if (k % 2 == 0) {
            // A dash cut to nothing at either end is none; a dash of no length (a dot) is one.
            const double from = std::max(0.0, s), to = std::min(total, s + d);
            if (to > from || (d == 0 && s >= 0)) on.push_back({from, to});
        }
        s += d;
        ++k;
        if (d == 0 && dash.size() == 1) break;
    }
    return on;
}

/** What a stroked polyline paints on the side y = 0, 0 <= x <= length (within eps): each
    interval its dash array paints along it, cut to its pieces on that side, as [from, to] in x
    (a dot as [x, x]). */
inline std::vector<std::pair<double, double>> SideSpans(const std::vector<Pt>& pts, const std::vector<double>& dash, double phase,
                                                        double length, double eps)
{
    std::vector<std::pair<double, double>> out;
    std::vector<double> at{0};
    for (size_t i = 1; i < pts.size(); ++i) at.push_back(at.back() + ise_miter::Len(pts[i] - pts[i - 1]));
    const double total = at.back();
    auto onSide = [&](Pt p) { return std::fabs(p.y) <= eps && p.x >= -eps && p.x <= length + eps; };
    for (const auto& iv : OnIntervals(total, dash, phase)) {
        std::vector<std::pair<double, double>> parts;
        if (pts.size() == 1 || total <= 0) {   // a dot
            if (onSide(pts[0])) parts.push_back({pts[0].x, pts[0].x});
        }
        for (size_t i = 1; i < pts.size() && total > 0; ++i) {
            const Pt p0 = pts[i - 1], p1 = pts[i];
            if (!onSide(p0) || !onSide(p1)) continue;
            const double lo = std::max(iv.first, at[i - 1]), hi = std::min(iv.second, at[i]);
            if (hi < lo - 1e-12) continue;
            const double span = at[i] - at[i - 1];
            auto x = [&](double s) { return span > 0 ? p0.x + (p1.x - p0.x) * (s - at[i - 1]) / span : p0.x; };
            const double a = x(lo), b = x(hi);
            parts.push_back({std::min(a, b), std::max(a, b)});
        }
        // One on-interval along collinear pieces of the side is one span.
        std::sort(parts.begin(), parts.end());
        for (const auto& p : parts) {
            if (!out.empty() && &p != &parts.front() && p.first <= out.back().second + eps) out.back().second = std::max(out.back().second, p.second);
            else out.push_back(p);
        }
    }
    for (auto& sp : out) { sp.first = std::max(0.0, std::min(length, sp.first)); sp.second = std::max(sp.first, std::min(length, sp.second)); }
    return out;
}

/** A subpath's bounds in x and its middle. */
inline void Extent(const Contour& c, double* lo, double* hi, Pt* middle)
{
    std::vector<Pt> pts;
    Points(c, &pts);
    double x0 = pts.front().x, x1 = x0, y0 = pts.front().y, y1 = y0;
    for (const Pt& p : pts) { x0 = std::min(x0, p.x); x1 = std::max(x1, p.x); y0 = std::min(y0, p.y); y1 = std::max(y1, p.y); }
    *lo = x0; *hi = x1;
    *middle = {(x0 + x1) / 2, (y0 + y1) / 2};
}

/** A contour moved rigidly so that the line's point (cx, 0) lands on `at`, its x axis along
    `dir`. */
inline Contour Turned(const Contour& c, double cx, Pt at, Pt dir)
{
    const Pt normal = {-dir.y, dir.x};
    auto map = [&](Pt p) { return at + dir * (p.x - cx) + normal * p.y; };
    Contour out;
    out.closed = c.closed;
    out.start = map(c.start);
    for (const Segment& s : c.segments) {
        Segment m;
        m.curve = s.curve;
        if (s.curve) { m.c1 = map(s.c1); m.c2 = map(s.c2); }
        m.p = map(s.p);
        out.segments.push_back(m);
    }
    return out;
}

/** A stroke's piece along a run, as a span of it. */
struct Span {
    double from, to;
    size_t group;   // index into Laid::strokes
};

inline size_t GroupFor(Laid* laid, const Mark& m)
{
    for (size_t g = 0; g < laid->strokes.size(); ++g) {
        const Laid::Stroke& s = laid->strokes[g];
        if (s.cap == m.cap && s.join == m.join && std::fabs(s.width - m.width) <= 1e-9) return g;
    }
    Laid::Stroke s;
    s.width = m.width; s.cap = m.cap; s.join = m.join;
    laid->strokes.push_back(s);
    return laid->strokes.size() - 1;
}

/** Joins two open contours end to start (the first ends where the second starts). */
inline Contour Joined(const Contour& a, const Contour& b)
{
    Contour out = a;
    for (const Segment& s : b.segments) out.segments.push_back(s);
    return out;
}

} // namespace detail

/** Whether marks can be laid along runs: every stroked subpath is straight (a dash, a dot, a
    hash), and every other mark no longer than `longest` along the line (a filled dot); a mark
    that curves along the line (a wave) cannot be laid by pieces. */
inline bool Layable(const std::vector<Mark>& marks, double eps, double longest)
{
    std::vector<Pt> pts;
    for (const Mark& m : marks)
        for (const Contour& c : m.path) {
            if (m.stroke && detail::Polyline(c, eps, &pts)) continue;
            double lo = 0, hi = 0;
            Pt middle;
            detail::Extent(c, &lo, &hi, &middle);
            if (hi - lo > longest) return false;
        }
    return true;
}

/** What marks paint along a straight line from (0, 0) to (length, 0), as a signature for
    comparing recordings: each span on it, and each other subpath's x middle, sorted; only
    those ending before `before`. */
inline std::vector<std::pair<double, double>> Signature(const std::vector<Mark>& marks, double length, double eps, double before)
{
    std::vector<std::pair<double, double>> out;
    std::vector<Pt> pts;
    for (const Mark& m : marks)
        for (const Contour& c : m.path) {
            if (m.stroke && detail::Polyline(c, eps, &pts)) {
                for (const auto& sp : detail::SideSpans(pts, m.dash, m.phase, length, eps))
                    if (sp.second < before) out.push_back(sp);
                continue;
            }
            double lo = 0, hi = 0;
            Pt middle;
            detail::Extent(c, &lo, &hi, &middle);
            if (hi < before) out.push_back({middle.x, middle.x});
        }
    std::sort(out.begin(), out.end());
    return out;
}

/** Whether a stroker fits its pattern to a path's length (a corner adjustment): what it paints
    along a line of length `a`, away from its end, differs from what it paints along a longer
    line `b`. A pattern laid from the start regardless (None) paints the same there. */
inline bool Fitted(const std::vector<Mark>& shortLine, double a, const std::vector<Mark>& longLine, double b, double eps)
{
    const double before = 0.75 * a;
    const auto s = Signature(shortLine, a, eps, before), l = Signature(longLine, b, eps, before);
    if (s.size() != l.size()) return true;
    for (size_t i = 0; i < s.size(); ++i)
        if (std::fabs(s[i].first - l[i].first) > eps || std::fabs(s[i].second - l[i].second) > eps) return true;
    return false;
}

/** The square a run of `length` is measured on (LayContour): its first side is the run, from
    (0, 0) to (length, 0), between two corners, as a stroker that fits its pattern to corners
    lays it between a shape's corners (InDesign 21.6: a dash centered on each corner, half on
    either side, and the pattern fitted between). */
inline Contour RunSquare(double length)
{
    Contour c;
    c.start = {0, 0};
    for (const Pt& q : {Pt{length, 0}, Pt{length, length}, Pt{0, length}}) {
        Segment s;
        s.p = q;
        c.segments.push_back(s);
    }
    c.closed = true;
    return c;
}

/** Lays a contour's pattern by its runs (CornerRuns): `record(length)` returns what the stroker
    paints around RunSquare(length), of which the first side is taken: a stroke's parts on it
    as pieces of the run, any other mark centered on it turned to the run's direction. A piece
    that ends a run at its end joins the piece the next run starts with at its start (the same
    width, cap, and join), so a dash through a corner is one piece. `eps`: how near the side
    counts as on it. */
template <typename Record>
void LayContour(const Contour& c, Record record, double eps, double cornerDegrees, Laid* laid)
{
    const std::vector<Run> runs = CornerRuns(c, cornerDegrees);
    if (runs.empty()) return;
    // Per run, the spans its strokes lay, in order along it.
    std::vector<std::vector<detail::Span>> spans(runs.size());
    std::vector<Pt> pts;
    for (size_t r = 0; r < runs.size(); ++r) {
        const Arc& arc = runs[r].arc;
        const double length = arc.Length();
        if (!(length > 0)) continue;
        const std::vector<Mark> marks = record(length);
        for (const Mark& m : marks)
            for (const Contour& sub : m.path) {
                if (m.stroke && detail::Polyline(sub, eps, &pts)) {
                    const std::vector<std::pair<double, double>> on = detail::SideSpans(pts, m.dash, m.phase, length, eps);
                    if (!on.empty()) {
                        const size_t g = detail::GroupFor(laid, m);
                        for (const auto& sp : on) spans[r].push_back({sp.first, sp.second, g});
                    }
                    continue;
                }
                // Any other mark centered on the side (its start corner's included, its end
                // corner's left to the next run).
                double lo = 0, hi = 0;
                Pt middle;
                detail::Extent(sub, &lo, &hi, &middle);
                if (std::fabs(middle.y) > eps || middle.x < -eps || middle.x >= length - eps) continue;
                Pt at, dir;
                arc.At(std::max(0.0, std::min(length, middle.x)), &at, &dir);
                const Contour turned = detail::Turned(sub, middle.x, at, dir);
                if (m.stroke) laid->strokes[detail::GroupFor(laid, m)].pieces.push_back(turned);
                else laid->fills.push_back(turned);
            }
        std::sort(spans[r].begin(), spans[r].end(), [](const detail::Span& x, const detail::Span& y) { return x.from < y.from; });
    }
    // The pieces, a run's last span joined to the next run's first through the corner (round a
    // loop, to its own first). A chain starts at a span that continues no other, so that a dash
    // through a corner is one piece whichever run it starts in; a chain all the way round (a
    // solid contour) is closed.
    const size_t n = runs.size();
    auto reachesEnd = [&](size_t r, size_t k) {
        return k + 1 == spans[r].size() && spans[r][k].to >= runs[r].arc.Length() - eps;
    };
    auto continues = [&](size_t r) {   // spans[r][0] carries on the previous run's last span
        if (spans[r].empty() || spans[r][0].from > eps) return false;
        const size_t prev = (r + n - 1) % n;
        return !spans[prev].empty() && reachesEnd(prev, spans[prev].size() - 1) && spans[prev].back().group == spans[r][0].group;
    };
    std::vector<std::vector<char>> used(n);
    for (size_t r = 0; r < n; ++r) used[r].assign(spans[r].size(), 0);
    auto chain = [&](size_t r, size_t k) {
        const size_t startRun = r, startSpan = k;
        used[r][k] = 1;
        Contour piece = runs[r].arc.Piece(spans[r][k].from, spans[r][k].to);
        while (reachesEnd(r, k)) {
            const size_t next = (r + 1) % n;
            if (!continues(next)) break;
            if (used[next][0]) {
                piece.closed = next == startRun && startSpan == 0;   // all the way round
                break;
            }
            used[next][0] = 1;
            piece = detail::Joined(piece, runs[next].arc.Piece(spans[next][0].from, spans[next][0].to));
            r = next;
            k = 0;
        }
        return piece;
    };
    for (int pass = 0; pass < 2; ++pass)
        for (size_t r = 0; r < n; ++r)
            for (size_t k = 0; k < spans[r].size(); ++k) {
                if (used[r][k]) continue;
                if (pass == 0 && k == 0 && continues(r)) continue;   // picked up by the chain before it
                const size_t group = spans[r][k].group;
                laid->strokes[group].pieces.push_back(chain(r, k));
            }
}

} // namespace ise_pattern
