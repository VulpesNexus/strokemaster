//========================================================================================
// Miter correction for route W's Outside strokes. Host-independent: standard C++ only, so
// the same code compiles into the plugin and into tests/native-repaint/MiterCoreTests.cpp.
//
// InDesign draws an Outside text stroke as a centered stroke of twice the weight beneath the
// fill. At a corner where the outline turns away from the fill, the miter join points into
// the glyph; where the glyph is thin there, its tip comes out the other side: a spike. A
// stroke with bevel joins never leaves the band within half its width of the outline, so it
// has no spikes; the joins that belong are the miters at corners where the join points away
// from the fill. Measured on InDesign 21.6 (docs/ROUTE_X_21_6_RECERTIFICATION_2026-10-03.md):
// every spike is such an interior miter, and ink outside the one-sided outward offset of the
// fill is otherwise only anti-aliasing fringe.
//
// So a mitered run is drawn as: the whole outline stroked with bevel joins, then, for every
// exterior corner within the miter limit, its miter join filled (MiterJoin): the polygon the
// renderer's stroker adds at a miter join, built from the corner's own tangents. The plugin
// first restroked a two-segment stub along the corner (a -> v -> b) and left the join to the
// renderer; the PDF port's rounding bent stubs beside tiny segments until renderers disagreed
// on the join (2026-10-04).
//
// A corner is exterior when a point just off it, on the join's side (the outward bisector
// u - w of the incoming direction u and the outgoing direction w), lies outside the run's
// fill under the nonzero rule. That test needs no contour orientation, and it treats holes
// and overlapping glyphs the way the fill itself does.
//========================================================================================
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace ise_miter {

struct Pt { double x = 0, y = 0; };

inline Pt operator+(Pt a, Pt b) { return {a.x + b.x, a.y + b.y}; }
inline Pt operator-(Pt a, Pt b) { return {a.x - b.x, a.y - b.y}; }
inline Pt operator*(Pt a, double k) { return {a.x * k, a.y * k}; }
inline double Dot(Pt a, Pt b) { return a.x * b.x + a.y * b.y; }
inline double Cross(Pt a, Pt b) { return a.x * b.y - a.y * b.x; }
inline double Len(Pt a) { return std::hypot(a.x, a.y); }

struct Segment {
    bool curve = false;
    Pt c1, c2, p;   // control points (curves only) and end point
};

struct Contour {
    Pt start;
    std::vector<Segment> segments;
    // An open path's contour (a page item's open path): stroked without its closing line, its
    // ends capped, no corner at either end. Its fill is still the contour closed.
    bool closed = true;
};

/** Collects an outline as IOutlineInfo emits it; coordinates are already placed. With
    trackOpen, a contour that ends without closepath is marked open; otherwise every contour is
    closed, as glyph outlines are. */
class Recorder {
public:
    explicit Recorder(bool trackOpen = false) : fTrackOpen(trackOpen) {}
    void MoveTo(Pt p) { Finish(); fOpen = true; fCurrent = Contour(); fCurrent.start = p; fCurrent.closed = !fTrackOpen; }
    void LineTo(Pt p) { if (!fOpen) MoveTo(p); Segment s; s.p = p; fCurrent.segments.push_back(s); }
    void CurveTo(Pt c1, Pt c2, Pt p)
    {
        if (!fOpen) MoveTo(c1);
        Segment s; s.curve = true; s.c1 = c1; s.c2 = c2; s.p = p;
        fCurrent.segments.push_back(s);
    }
    void Close() { fCurrent.closed = true; Finish(); }
    const std::vector<Contour>& Contours() { Finish(); return fContours; }
private:
    void Finish()
    {
        if (fOpen && !fCurrent.segments.empty()) fContours.push_back(fCurrent);
        fOpen = false;
    }
    std::vector<Contour> fContours;
    Contour fCurrent;
    bool fOpen = false;
    bool fTrackOpen = false;
};

/** A contour's segments with zero-length ones dropped and the implicit closing line made
    explicit: glyph contours are closed, and closepath draws a line back to the start. */
inline std::vector<Segment> Closed(const Contour& c, double eps)
{
    std::vector<Segment> out;
    Pt cur = c.start;
    for (const Segment& s : c.segments) {
        const bool degenerate = Len(s.p - cur) <= eps && (!s.curve || (Len(s.c1 - cur) <= eps && Len(s.c2 - cur) <= eps));
        if (!degenerate) out.push_back(s);
        cur = s.p;
    }
    if (Len(cur - c.start) > eps) { Segment s; s.p = c.start; out.push_back(s); }
    return out;
}

/** The segments a stroke follows: Closed for a closed contour; for an open one, its segments
    with zero-length ones dropped and no closing line. */
inline std::vector<Segment> Stroked(const Contour& c, double eps)
{
    if (c.closed) return Closed(c, eps);
    std::vector<Segment> out;
    Pt cur = c.start;
    for (const Segment& s : c.segments) {
        const bool degenerate = Len(s.p - cur) <= eps && (!s.curve || (Len(s.c1 - cur) <= eps && Len(s.c2 - cur) <= eps));
        if (!degenerate) out.push_back(s);
        cur = s.p;
    }
    return out;
}

/** Direction leaving `from` along a segment, and arriving at its end. */
inline Pt StartTangent(Pt from, const Segment& s, double eps)
{
    if (s.curve) {
        if (Len(s.c1 - from) > eps) return s.c1 - from;
        if (Len(s.c2 - from) > eps) return s.c2 - from;
    }
    return s.p - from;
}
inline Pt EndTangent(Pt from, const Segment& s, double eps)
{
    if (s.curve) {
        if (Len(s.p - s.c2) > eps) return s.p - s.c2;
        if (Len(s.p - s.c1) > eps) return s.p - s.c1;
    }
    return s.p - from;
}

inline void FlattenCubic(Pt p0, Pt p1, Pt p2, Pt p3, double tol, int depth, std::vector<Pt>* out)
{
    const Pt d = p3 - p0;
    const double n = Len(d);
    const double e1 = n > 0 ? std::fabs(Cross(p1 - p0, d)) / n : Len(p1 - p0);
    const double e2 = n > 0 ? std::fabs(Cross(p2 - p0, d)) / n : Len(p2 - p0);
    if (depth >= 16 || std::max(e1, e2) <= tol) { out->push_back(p3); return; }
    const Pt a = (p0 + p1) * 0.5, b = (p1 + p2) * 0.5, c = (p2 + p3) * 0.5;
    const Pt ab = (a + b) * 0.5, bc = (b + c) * 0.5, m = (ab + bc) * 0.5;
    FlattenCubic(p0, a, ab, m, tol, depth + 1, out);
    FlattenCubic(m, bc, c, p3, tol, depth + 1, out);
}

struct Ring {
    std::vector<Pt> points;   // closed implicitly
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    // The edges (edge i runs from point i to the next) by horizontal band of the box, so that
    // a winding query scans only the edges level with it; empty for a short ring, whose edges
    // are all scanned. A large glyph contour flattens to thousands of points.
    std::vector<std::vector<unsigned>> bands;
    size_t Band(double y) const
    {
        const size_t k = bands.size();
        if (!(y1 > y0)) return 0;
        return std::min(k - 1, static_cast<size_t>(std::max(0.0, (y - y0) / (y1 - y0) * static_cast<double>(k))));
    }
};

/** Fills in a ring's box and edge bands from its points. */
inline void Index(Ring* r)
{
    r->x0 = r->x1 = r->points[0].x; r->y0 = r->y1 = r->points[0].y;
    for (const Pt& p : r->points) {
        r->x0 = std::min(r->x0, p.x); r->x1 = std::max(r->x1, p.x);
        r->y0 = std::min(r->y0, p.y); r->y1 = std::max(r->y1, p.y);
    }
    const size_t n = r->points.size();
    r->bands.clear();
    if (n < 32 || !(r->y1 > r->y0)) return;
    r->bands.resize(std::min<size_t>(256, n / 8));
    for (size_t i = 0; i < n; ++i) {
        const Pt a = r->points[i], b = r->points[(i + 1) % n];
        for (size_t k = r->Band(std::min(a.y, b.y)), last = r->Band(std::max(a.y, b.y)); k <= last; ++k)
            r->bands[k].push_back(static_cast<unsigned>(i));
    }
}

inline std::vector<Ring> Flatten(const std::vector<Contour>& contours, double tol, double eps)
{
    std::vector<Ring> rings;
    for (const Contour& c : contours) {
        Ring r;
        r.points.push_back(c.start);
        Pt cur = c.start;
        for (const Segment& s : Closed(c, eps)) {
            if (s.curve) FlattenCubic(cur, s.c1, s.c2, s.p, tol, 0, &r.points);
            else r.points.push_back(s.p);
            cur = s.p;
        }
        if (r.points.size() < 3) continue;
        Index(&r);
        rings.push_back(r);
    }
    return rings;
}

/** Nonzero winding number of q against one ring; 0 when its box excludes q. */
inline int RingWinding(const Ring& r, Pt q)
{
    if (q.x < r.x0 || q.x > r.x1 || q.y < r.y0 || q.y > r.y1) return 0;
    int w = 0;
    const size_t n = r.points.size();
    auto edge = [&](size_t i) {
        const Pt a = r.points[i], b = r.points[(i + 1) % n];
        if (a.y <= q.y) {
            if (b.y > q.y && Cross(b - a, q - a) > 0) ++w;
        } else if (b.y <= q.y && Cross(b - a, q - a) < 0) {
            --w;
        }
    };
    // Every edge that crosses q's level lies in q's band.
    if (!r.bands.empty()) for (unsigned i : r.bands[r.Band(q.y)]) edge(i);
    else for (size_t i = 0; i < n; ++i) edge(i);
    return w;
}

/** Nonzero winding number of q against the rings, skipping rings whose box excludes it. */
inline int Winding(const std::vector<Ring>& rings, Pt q)
{
    int w = 0;
    for (const Ring& r : rings) w += RingWinding(r, q);
    return w;
}

/** The rings by cell of a grid laid over their boxes, so that a query about a point visits only
    the rings whose box can contain it: an outlined line of a decorative font has thousands of
    contours (5,725 for one line of Kerberos Fang at 48 pt). Answers are exactly Winding's. The
    rings must outlive the grid. */
class RingGrid {
public:
    explicit RingGrid(const std::vector<Ring>& rings) : fRings(rings)
    {
        if (rings.empty()) return;
        fX0 = rings[0].x0; fY0 = rings[0].y0; fX1 = rings[0].x1; fY1 = rings[0].y1;
        for (const Ring& r : rings) {
            fX0 = std::min(fX0, r.x0); fY0 = std::min(fY0, r.y0);
            fX1 = std::max(fX1, r.x1); fY1 = std::max(fY1, r.y1);
        }
        fN = std::min<size_t>(64, static_cast<size_t>(std::sqrt(static_cast<double>(rings.size()))) + 1);
        fCells.resize(fN * fN);
        for (size_t i = 0; i < rings.size(); ++i) {
            const size_t cx0 = Cell(rings[i].x0, fX0, fX1), cx1 = Cell(rings[i].x1, fX0, fX1);
            const size_t cy0 = Cell(rings[i].y0, fY0, fY1), cy1 = Cell(rings[i].y1, fY0, fY1);
            for (size_t cy = cy0; cy <= cy1; ++cy)
                for (size_t cx = cx0; cx <= cx1; ++cx) fCells[cy * fN + cx].push_back(i);
        }
    }
    /** The indices of the rings whose box may contain q. */
    const std::vector<size_t>& Near(Pt q) const
    {
        static const std::vector<size_t> none;
        if (fCells.empty() || !(q.x >= fX0 && q.x <= fX1 && q.y >= fY0 && q.y <= fY1)) return none;
        return fCells[Cell(q.y, fY0, fY1) * fN + Cell(q.x, fX0, fX1)];
    }
    int Winding(Pt q) const
    {
        int w = 0;
        for (size_t i : Near(q)) w += RingWinding(fRings[i], q);
        return w;
    }
private:
    size_t Cell(double v, double lo, double hi) const
    {
        if (!(hi > lo)) return 0;
        return std::min(fN - 1, static_cast<size_t>(std::max(0.0, (v - lo) / (hi - lo) * static_cast<double>(fN))));
    }
    const std::vector<Ring>& fRings;
    double fX0 = 0, fY0 = 0, fX1 = 0, fY1 = 0;
    size_t fN = 1;
    std::vector<std::vector<size_t>> fCells;
};

/** A corner whose miter join is drawn (MiterJoin, CutMiter): its vertex, tangents, and miter
    ratio. It also carries a stub a -> v -> b, each piece a short part of the outline's own
    segment (a line, or a curve with controls), so that a restroke along it lies exactly on the
    stroke; joins are filled now, and the stub is kept for tests and research. An interior
    corner's join points into the fill; it is kept only for Center strokes, clipped to the fill
    (see Corners). */
struct Stub {
    Pt a, v, b;
    bool interior = false;
    // The miter's length over the stroke width (1 / cos(turn / 2)), compared with a miter
    // limit exactly as the renderer compares it, and the unit direction its tip points in.
    double ratio = 1;
    Pt outward;
    Pt u, w;         // unit tangents arriving at and leaving v: the miter's edges are their offsets
    double edge = 0; // length of the shorter of the two segments meeting at v
    bool inCurve = false, outCurve = false;
    Pt in1, in2;     // controls of a -> v when inCurve
    Pt out1, out2;   // controls of v -> b when outCurve
};

inline Pt Lerp(Pt a, Pt b, double t) { return a + (b - a) * t; }

/** A cubic's part [t, 1] and part [0, t] (de Casteljau). */
inline void CubicTail(Pt p0, Pt p1, Pt p2, Pt p3, double t, Pt* q0, Pt* q1, Pt* q2)
{
    const Pt p12 = Lerp(p1, p2, t), p23 = Lerp(p2, p3, t);
    const Pt p123 = Lerp(p12, p23, t);
    const Pt p012 = Lerp(Lerp(p0, p1, t), p12, t);
    *q0 = Lerp(p012, p123, t); *q1 = p123; *q2 = p23;
}
inline void CubicHead(Pt p0, Pt p1, Pt p2, Pt p3, double t, Pt* q1, Pt* q2, Pt* q3)
{
    const Pt p01 = Lerp(p0, p1, t), p12 = Lerp(p1, p2, t);
    const Pt p012 = Lerp(p01, p12, t);
    const Pt p123 = Lerp(p12, Lerp(p2, p3, t), t);
    *q1 = p01; *q2 = p012; *q3 = Lerp(p012, p123, t);
}

/** A cubic's length, about: the mean of its chord and its control polygon. */
inline double ApproxLength(Pt p0, Pt p1, Pt p2, Pt p3)
{
    return 0.5 * (Len(p1 - p0) + Len(p2 - p1) + Len(p3 - p2) + Len(p3 - p0));
}

/** A segment's length (about, for a curve), starting at `from`. */
inline double SegmentLength(Pt from, const Segment& s)
{
    return s.curve ? ApproxLength(from, s.c1, s.c2, s.p) : Len(s.p - from);
}

/** The curve parameter that cuts a piece of about `length` off a cubic's end. */
inline double PieceParameter(Pt p0, Pt p1, Pt p2, Pt p3, double length)
{
    const double approx = ApproxLength(p0, p1, p2, p3);
    if (!(approx > 0)) return 0.5;
    return std::min(0.5, std::max(1e-4, length / approx));
}

struct Options {
    double halfWidth = 1;       // h: half of the drawn stroke width (the Outside weight)
    double minTurn = 0.5 * 3.14159265358979323846 / 180;   // radians; smaller turns join invisibly
    double probe = 0;           // distance of the exterior test point; 0 means 1% of h
    double stub = 0;            // stub length along each tangent; 0 means h (JoinPolygon's band)
};

/** The corners of a run's outline whose miter joins are drawn: the exterior ones, and with
    includeInterior the interior ones too, flagged. An exact or numerically
    unresolved reversal is skipped: its miter is infinitely long, so the native stroke bevels
    it, and the bevel stroke already draws that.

    Outside strokes use the exterior stubs only. A Center stroke lies over its own fill, so
    an interior miter is visible there and belongs; only its part outside the fill escapes.
    Center strokes therefore also fill the interior joins, clipped to the fill. */
inline std::vector<Stub> Corners(const std::vector<Contour>& contours, const Options& o, bool includeInterior)
{
    std::vector<Stub> out;
    if (!(o.halfWidth > 0) || !std::isfinite(o.halfWidth)) return out;
    const double h = o.halfWidth;
    const double eps = 1e-9 * std::max(1.0, h);
    const double probe = o.probe > 0 ? o.probe : 0.01 * h;
    const double stub = o.stub > 0 ? o.stub : h;
    const std::vector<Ring> rings = Flatten(contours, probe * 0.1, eps);
    const RingGrid grid(rings);
    for (const Contour& c : contours) {
        const std::vector<Segment> segs = Stroked(c, eps);
        const size_t n = segs.size();
        if (n < 2) continue;
        // Vertex i is the end of segment i and the start of segment i + 1 (wrapping on a closed
        // contour; an open one's ends are capped, not joined).
        for (size_t i = 0; i < (c.closed ? n : n - 1); ++i) {
            const Segment& in = segs[i];
            const Segment& next = segs[(i + 1) % n];
            const Pt inFrom = i == 0 ? c.start : segs[i - 1].p;
            // The closing segment ends at the start, where the first segment begins.
            const Pt v = in.p;
            Pt u = EndTangent(inFrom, in, eps), w = StartTangent(v, next, eps);
            const double lu = Len(u), lw = Len(w);
            if (lu <= eps || lw <= eps) continue;
            u = u * (1 / lu); w = w * (1 / lw);
            const double turn = std::atan2(std::fabs(Cross(u, w)), Dot(u, w));
            if (turn < o.minTurn) continue;
            const Pt outward = u - w;
            const double lo = Len(outward);
            if (lo <= 1e-12 || std::fabs(Cross(u, w)) <= 1e-15) continue;   // reversal
            const Pt q = v + outward * (probe / lo);
            const bool interior = grid.Winding(q) != 0;   // the join points into the fill
            if (interior && !includeInterior) continue;
            const double din = std::min(stub, 0.5 * Len(v - inFrom));
            const Pt nextEnd = next.p;
            const double dout = std::min(stub, 0.5 * Len(nextEnd - v));
            if (din <= eps || dout <= eps) continue;
            Stub st;
            st.v = v;
            st.interior = interior;
            st.ratio = 1 / std::cos(turn / 2);
            st.outward = outward * (1 / lo);
            st.u = u; st.w = w;
            st.edge = std::min(SegmentLength(inFrom, in), SegmentLength(v, next));
            if (in.curve) {
                st.inCurve = true;
                CubicTail(inFrom, in.c1, in.c2, v, 1 - PieceParameter(inFrom, in.c1, in.c2, v, din), &st.a, &st.in1, &st.in2);
            } else {
                st.a = v - u * din;
            }
            if (next.curve) {
                st.outCurve = true;
                CubicHead(v, next.c1, next.c2, nextEnd, PieceParameter(v, next.c1, next.c2, nextEnd, dout), &st.out1, &st.out2, &st.b);
            } else {
                st.b = v + w * dout;
            }
            out.push_back(st);
        }
    }
    return out;
}

/** Whether the nonzero and even-odd rules give the same region near every edge of the rings:
    probe points just off both sides of each edge at three places along it (up to 64 edges per
    ring). A quick test: points can miss an overlap, and the plugin decides with the exact
    ise_region::FillRulesAgree. Probing only the midpoints missed two overlapping
    parallelograms, whose every midpoint lay on the other's edge (coverage census, 2026-10-04);
    the other two places are irrational fractions of the edge, so a symmetric figure does not
    put them on an edge too. */
inline bool FillRulesAgree(const std::vector<Ring>& rings, double probe)
{
    const RingGrid grid(rings);
    for (const Ring& r : rings) {
        const size_t n = r.points.size();
        const size_t step = std::max<size_t>(1, n / 64);
        for (size_t i = 0; i < n; i += step) {
            const Pt a = r.points[i], b = r.points[(i + 1) % n];
            const Pt d = b - a;
            const double l = Len(d);
            if (l <= 1e-12) continue;
            const Pt nrm = {-d.y / l, d.x / l};
            for (double f : {0.5, 0.31830988618379067, 0.70710678118654752})
            for (int side = -1; side <= 1; side += 2) {
                const Pt m = a + d * f;
                const Pt q = m + nrm * (probe * side);
                const int w = grid.Winding(q);
                int crossings = 0;
                for (size_t index : grid.Near(q)) {
                    const Ring& s = rings[index];
                    if (q.x < s.x0 || q.x > s.x1 || q.y < s.y0 || q.y > s.y1) continue;
                    const size_t k = s.points.size();
                    for (size_t j = 0; j < k; ++j) {
                        const Pt e0 = s.points[j], e1 = s.points[(j + 1) % k];
                        if ((e0.y > q.y) != (e1.y > q.y) && q.x < e0.x + (q.y - e0.y) * (e1.x - e0.x) / (e1.y - e0.y)) ++crossings;
                    }
                }
                if ((w != 0) != (crossings % 2 == 1)) return false;
            }
        }
    }
    return true;
}

inline double SignedArea(const Ring& r)
{
    double a = 0;
    for (size_t i = 0, n = r.points.size(); i < n; ++i) a += Cross(r.points[i], r.points[(i + 1) % n]);
    return a / 2;
}

/** The contour traversed the other way, curves included; it starts where it did. */
inline Contour Reversed(const Contour& c, double eps)
{
    const std::vector<Segment> segs = Closed(c, eps);   // the last segment returns to the start
    Contour r;
    r.start = c.start;
    for (size_t k = segs.size(); k-- > 0;) {
        Segment t;
        t.curve = segs[k].curve;
        t.c1 = segs[k].c2; t.c2 = segs[k].c1;
        t.p = k == 0 ? c.start : segs[k - 1].p;
        r.segments.push_back(t);
    }
    return r;
}

/** A clip path to the outside of a shape whose meaning does not depend on the fill rule the
    port applies: a frame `margin` beyond the shape, then each contour wound against the frame
    at even nesting depth and with it at odd depth. InDesign's PDF port wrote an even-odd clip
    as a nonzero one and rewrote a rectangular frame as `re`, with its own winding (measured
    2026-10-03); the frame is therefore an octagon, and the contours are wound explicitly.
    Probe points just off every edge confirm that the clip's nonzero reading is exactly the
    outside of the fill (nonzero rule, which FillRulesAgree makes the even-odd one too); where
    it is not (contours overlapping more deeply than the nesting rule handles), the result is
    empty and the shape's stroke stays InDesign's. With `oriented`, the contours are a resolved
    region (ise_region::NonzeroRegion): wound counterclockwise around filled parts and clockwise
    around holes, so each is simply wound against the frame, whatever touches what. Without
    `verify` the probes are skipped: the caller checks the clip exactly (ise_region::ClipsOutside),
    and a probe that lands on an edge where two contours touch would refuse a clip that is right. */
inline std::vector<Contour> OutsideClip(const std::vector<Contour>& contours, double margin, double tol, double probe,
                                        bool oriented = false, bool verify = true)
{
    const double eps = 1e-9;
    std::vector<Ring> rings;
    std::vector<const Contour*> source;
    for (const Contour& c : contours) {
        const std::vector<Ring> r = Flatten({c}, tol, eps);
        if (r.empty()) continue;
        rings.push_back(r[0]);
        source.push_back(&c);
    }
    std::vector<Contour> out;
    if (rings.empty() || !(margin > 0)) return out;
    double x0 = rings[0].x0, y0 = rings[0].y0, x1 = rings[0].x1, y1 = rings[0].y1;
    for (const Ring& r : rings) {
        x0 = std::min(x0, r.x0); y0 = std::min(y0, r.y0); x1 = std::max(x1, r.x1); y1 = std::max(y1, r.y1);
    }
    x0 -= margin; y0 -= margin; x1 += margin; y1 += margin;
    const double k = margin / 2;
    Contour frame;   // counterclockwise: positive area
    frame.start = {x0 + k, y0};
    for (const Pt& p : {Pt{x1 - k, y0}, Pt{x1, y0 + k}, Pt{x1, y1 - k}, Pt{x1 - k, y1}, Pt{x0 + k, y1},
                        Pt{x0, y1 - k}, Pt{x0, y0 + k}}) {
        Segment s; s.p = p; frame.segments.push_back(s);
    }
    out.push_back(frame);
    for (size_t i = 0; i < rings.size(); ++i) {
        int depth = 0;
        for (size_t j = 0; !oriented && j < rings.size(); ++j)
            if (j != i && RingWinding(rings[j], rings[i].points[0]) != 0) ++depth;
        const bool positive = SignedArea(rings[i]) > 0, wantPositive = oriented ? !positive : depth % 2 == 1;
        out.push_back(positive == wantPositive ? *source[i] : Reversed(*source[i], eps));
    }
    if (!verify) return out;
    const std::vector<Ring> clip = Flatten(out, tol, eps);
    const RingGrid clipGrid(clip), fillGrid(rings);
    for (const Ring& r : rings) {
        const size_t n = r.points.size();
        const size_t step = std::max<size_t>(1, n / 64);
        for (size_t i = 0; i < n; i += step) {
            const Pt a = r.points[i], b = r.points[(i + 1) % n], d = b - a;
            const double l = Len(d);
            if (l <= 1e-12) continue;
            const Pt m = (a + b) * 0.5, nrm = {-d.y / l, d.x / l};
            for (int side = -1; side <= 1; side += 2) {
                const Pt q = m + nrm * (probe * side);
                if ((clipGrid.Winding(q) != 0) != (fillGrid.Winding(q) == 0)) return {};
            }
        }
    }
    return out;
}

/** The miter join of an exterior corner of half width h, cut flat at `distance` from its vertex
    along the tip's direction, as a polygon to fill: the vertex, the two stroke edges' corners
    (offsets of the tangents), and where the miter's edges cross the cut. A miter past the
    limit is cut there instead of beveled at the vertex; at the limit itself the cut falls
    exactly at the tip, so the shape changes continuously as a corner sharpens. The join is
    defined by the tangents at the vertex alone, so the polygon is exact on curves too. It is
    filled rather than drawn as a clipped stroke: InDesign's PDF port merges or drops clip
    paths it judges redundant, and dropped the cut on a Center wedge (measured 2026-10-03). */
inline std::vector<Pt> CutMiter(const Stub& s, double h, double distance)
{
    Pt nIn = {-s.u.y, s.u.x}, nOut = {-s.w.y, s.w.x};
    if (Dot(nIn, s.outward) < 0) nIn = nIn * -1;
    if (Dot(nOut, s.outward) < 0) nOut = nOut * -1;
    const Pt e0 = s.v + nIn * h, e1 = s.v + nOut * h, tip = s.v + s.outward * (s.ratio * h);
    const double a0 = Dot(e0 - s.v, s.outward), reach = s.ratio * h;
    if (!(distance < reach) || !(reach - a0 > 1e-12)) return {s.v, e0, tip, e1};
    const double t = std::max(0.0, (distance - a0) / (reach - a0));
    return {s.v, e0, Lerp(e0, tip, t), Lerp(e1, tip, t), e1};
}

/** A corner's whole miter join for half width h, as a polygon: the vertex, the two stroke
    edges' corners, and the tip. */
inline std::vector<Pt> MiterJoin(const Stub& s, double h)
{
    return CutMiter(s, h, s.ratio * h);
}

/** The point of a stub piece at t, and its unit direction of travel from `from` to `to`. */
inline void PieceAt(Pt p0, Pt p1, Pt p2, Pt p3, bool curve, double t, Pt* at, Pt* dir)
{
    if (!curve) {
        *at = Lerp(p0, p3, t);
        const Pt d = p3 - p0;
        *dir = d * (1 / std::max(Len(d), 1e-300));
        return;
    }
    const double r = 1 - t;
    *at = p0 * (r * r * r) + p1 * (3 * r * r * t) + p2 * (3 * r * t * t) + p3 * (t * t * t);
    Pt d = (p1 - p0) * (3 * r * r) + (p2 - p1) * (6 * r * t) + (p3 - p2) * (3 * t * t);
    if (Len(d) <= 1e-12) d = p3 - p0;
    *dir = d * (1 / std::max(Len(d), 1e-300));
}

/** The miter join to fill at a corner, cut at `distance` along its tip (CutMiter), together
    with the outer half of the stroke along the corner's two stub pieces: from each piece's far
    end out to its offset by h on the join's side, along the offsets to the join, and back
    along the outline through the vertex. One polygon, to fill in the stroke's paint over a
    stroke drawn with bevel joins.

    The join alone, from the corner's exact tangents, met renderers' strokes unevenly: PDFium
    approximates a curve that bends hard just past a corner, and stops short of the exact offset
    there, leaving a notch beside the join; and a restroke of the corner, the plugin's first
    way, bent under the PDF port's rounding to a thousandth of a point when its piece lay along
    a tiny segment, so that Ghostscript beveled the corner (German Beauty and Kerberos Fang,
    2026-10-04). The band fills the exact stroke for up to h beside the corner, whatever the
    renderer draws there, and its points lie half a width apart, where rounding is negligible.
    A band stops where its offset would fold back (the outline curving toward the join with a
    radius under h). */
inline std::vector<Pt> JoinPolygon(const Stub& s, double h, double distance)
{
    const std::vector<Pt> join = CutMiter(s, h, distance);   // v, e0, ..., e1
    Pt nIn = {-s.u.y, s.u.x}, nOut = {-s.w.y, s.w.x};
    if (Dot(nIn, s.outward) < 0) nIn = nIn * -1;
    if (Dot(nOut, s.outward) < 0) nOut = nOut * -1;
    // The normal's turn from the direction of travel, the same all along a piece.
    const bool inLeft = Cross(s.u, nIn) > 0, outLeft = Cross(s.w, nOut) > 0;
    auto offset = [h](Pt at, Pt dir, bool left) { return at + (left ? Pt{-dir.y, dir.x} : Pt{dir.y, -dir.x}) * h; };
    // Samples from the vertex outward: outline points and their offsets.
    auto band = [&](bool incoming, std::vector<Pt>* line, std::vector<Pt>* off) {
        const bool curve = incoming ? s.inCurve : s.outCurve;
        const int n = curve ? 8 : 1;
        Pt prevAt = s.v, prevOff = incoming ? join[1] : join.back();
        for (int k = 1; k <= n; ++k) {
            const double f = static_cast<double>(k) / n;
            Pt at, dir;
            if (incoming) PieceAt(s.a, s.in1, s.in2, s.v, curve, 1 - f, &at, &dir);
            else PieceAt(s.v, s.out1, s.out2, s.b, curve, f, &at, &dir);
            const Pt o = offset(at, dir, incoming ? inLeft : outLeft);
            if (Dot(o - prevOff, at - prevAt) <= 0) break;   // the offset folds back
            line->push_back(at);
            off->push_back(o);
            prevAt = at;
            prevOff = o;
        }
    };
    std::vector<Pt> inLine, inOff, outLine, outOff;
    band(true, &inLine, &inOff);
    band(false, &outLine, &outOff);
    std::vector<Pt> poly;
    poly.push_back(s.v);
    for (const Pt& p : inLine) poly.push_back(p);                            // out along the incoming piece
    if (!inLine.empty()) for (size_t k = inOff.size(); k-- > 0;) poly.push_back(inOff[k]);   // back along its offset
    for (size_t k = 1; k < join.size(); ++k) poly.push_back(join[k]);        // e0, tip, e1
    for (const Pt& p : outOff) poly.push_back(p);                            // out along the outgoing offset
    for (size_t k = outLine.size(); k-- > 0;) poly.push_back(outLine[k]);    // back along the piece to v
    return poly;
}

/** The polygon wound counterclockwise (positive area), so overlapping joins filled together
    under the nonzero rule add up instead of cancelling. */
inline std::vector<Pt> Counterclockwise(std::vector<Pt> pts)
{
    double a = 0;
    for (size_t i = 0, n = pts.size(); i < n; ++i) a += Cross(pts[i], pts[(i + 1) % n]);
    if (a < 0) std::reverse(pts.begin(), pts.end());
    return pts;
}

/** Which of a resolved region's corners (`stubs`, from Corners on the region's boundary) the
    outline itself does not have: one flag per stub, set where no corner of `outline` lies within
    `within` of the vertex as sharp as the stub, up to `limit`. Where contours cross, or a
    contour's loop pinches off, the boundary turns at a point the outline passes straight
    through, or turns less; a point drawn there is the resolution's, not the shape's. */
inline std::vector<char> CornersNotOnOutline(const std::vector<Stub>& stubs, const std::vector<Contour>& outline,
                                             const Options& o, double limit, double within = 1e-3)
{
    std::vector<Stub> own = Corners(outline, o, true);
    std::sort(own.begin(), own.end(), [](const Stub& a, const Stub& b) { return a.v.x < b.v.x; });
    std::vector<char> out(stubs.size(), 1);
    for (size_t i = 0; i < stubs.size(); ++i) {
        const Stub& s = stubs[i];
        const double need = std::min(s.ratio, limit) - 1e-6;
        auto it = std::lower_bound(own.begin(), own.end(), s.v.x - within, [](const Stub& a, double x) { return a.v.x < x; });
        for (; it != own.end() && it->v.x <= s.v.x + within; ++it) {
            if (std::fabs(it->v.y - s.v.y) <= within && it->ratio >= need) { out[i] = 0; break; }
        }
    }
    return out;
}

/** Which corners (`stubs`) lie at none of `anchors`, the anchor points of the path the user
    drew: one flag per stub, set where no anchor lies within `within` of the vertex. A corner
    effect replaces each corner of that path with its own small shape, whose corners lie off
    the anchors; on an acute corner of a glyph that shape turns sharper than the miter limit,
    and a point drawn there is the effect's, not the shape's (the M's apexes of outlined Minion
    with fancy, inset, or inverse rounded corners threw 16 pt points, 2026-10-05). */
inline std::vector<char> CornersNotAtAnchors(const std::vector<Stub>& stubs, std::vector<Pt> anchors, double within = 1e-3)
{
    std::sort(anchors.begin(), anchors.end(), [](const Pt& a, const Pt& b) { return a.x < b.x; });
    std::vector<char> out(stubs.size(), 1);
    for (size_t i = 0; i < stubs.size(); ++i) {
        const Pt v = stubs[i].v;
        auto it = std::lower_bound(anchors.begin(), anchors.end(), v.x - within, [](const Pt& a, double x) { return a.x < x; });
        for (; it != anchors.end() && it->x <= v.x + within; ++it) {
            if (std::fabs(it->y - v.y) <= within) { out[i] = 0; break; }
        }
    }
    return out;
}

/** Which contours of a resolved region (one flag each) are holes no larger than `size` across
    in either direction: wound against the region's largest contour. Corner effects on
    overlapping contours leave such holes where a corner's loop falls inside another contour's
    fill (1.3 pt across on the census's overlapping squares, 2026-10-05); an Inside stroke
    around one paints a block the stroke's width wider on every side than the hole. */
inline std::vector<char> SmallHoles(const std::vector<Contour>& region, double size, double tol)
{
    const size_t n = region.size();
    std::vector<double> area(n, 0), wide(n, 0), tall(n, 0);
    size_t largest = 0;
    for (size_t i = 0; i < n; ++i) {
        const std::vector<Ring> rings = Flatten({region[i]}, tol, 1e-9);
        if (rings.empty()) continue;
        const std::vector<Pt>& p = rings[0].points;
        double x0 = p[0].x, x1 = p[0].x, y0 = p[0].y, y1 = p[0].y;
        for (size_t k = 0; k < p.size(); ++k) {
            area[i] += Cross(p[k], p[(k + 1) % p.size()]) / 2;
            x0 = std::min(x0, p[k].x); x1 = std::max(x1, p[k].x);
            y0 = std::min(y0, p[k].y); y1 = std::max(y1, p[k].y);
        }
        wide[i] = x1 - x0;
        tall[i] = y1 - y0;
        if (std::fabs(area[i]) > std::fabs(area[largest])) largest = i;
    }
    std::vector<char> out(n, 0);
    if (n == 0) return out;
    const double outer = area[largest] >= 0 ? 1 : -1;
    for (size_t i = 0; i < n; ++i)
        out[i] = i != largest && area[i] * outer < 0 && wide[i] <= size && tall[i] <= size;
    return out;
}

/** How far past its vertex the miter of a corner a corner effect adds reaches, within the miter
    limit, for half width h: the whole miter, but no farther past the stroke's edge (h) than the
    corner's shorter edge. The fancy and inset effects cut a glyph's corner into a zigzag of
    segments 0.06 to 0.5 pt long, whose miters within the limit threw points 7 to 10 pt from
    the vertex (InDesign draws them too, 2026-10-05); an effect's own corners between edges
    as long as the stroke is wide (an inset notch on a square) keep their whole miter. */
inline double EffectMiterReach(const Stub& s, double h)
{
    return std::min(s.ratio * h, h + s.edge);
}

/** Whether an exterior corner past the miter limit keeps its whole point, for half width h (the
    owner's rule, 2026-10-05): both edges straight, the shorter at least h long, and the whole
    miter (ratio * h) no longer than five times that edge. Such a corner is a shape's own point
    (a triangle's, the census thorn's at Outside 4 and 8, 2.3 and 4.6 times its 20 pt sides),
    and its stroke reaches all the way (WholePointReach). Any other corner past the limit is
    beveled as InDesign bevels it: a curve on either side or a short edge (the tapered tips of
    script glyphs, which drew 16 pt spikes at the limit distance in Segoe Script and Great
    Vibes), or a needle whose point would be many times longer than its edges (Kerberos Fang's
    straight corners past the limit: 13 to 111 times at 72 to 300 pt; none qualifies at 72 pt,
    2 or 3 of about 3,000 at 300 pt). */
inline bool WholePoint(const Stub& s, double h)
{
    return !s.inCurve && !s.outCurve && s.edge >= h && s.ratio * h <= 5 * s.edge;
}

/** How far past its vertex a whole point reaches: the whole miter, up to `longest` half widths
    (InDesign's highest miter limit is 500). */
inline double WholePointReach(const Stub& s, double h, double longest)
{
    return std::min(s.ratio, longest) * h;
}

/** Whether a corner is a glyph tip, whose miter is trimmed (TipReach), for half width h: a
    corner sharper than 60 degrees (ratio 2 or more) with a curve on either side or an edge
    shorter than h, such as the tapered tip of a script glyph, a curve meeting a 0.02 pt segment,
    whose whole miter stuck out as a spike within the limit (Segoe Script's W at ratio 3.67, the
    owner's screen check, 2026-10-05); or one sharper than 84 degrees (ratio 1.5) with a curve
    on either side and an edge shorter than h (the ear of Great Vibes' g, ratio 1.96 beside a
    1.3 pt curve, threw a 5 pt spike at Outside 5 pt, the owner's check, 2026-10-06). Blunter
    corners keep their whole miter, as InDesign draws it: the square corners of serif feet
    (ratio 1.41, straight edges about 1 pt) were chamfered when they were trimmed too. So does a
    corner between straight edges at least h long. */
inline bool TrimmedTip(const Stub& s, double h)
{
    const bool curve = s.inCurve || s.outCurve;
    return (s.ratio >= 2 && (curve || s.edge < h)) || (s.ratio >= 1.5 && curve && s.edge < h);
}

/** How far past its vertex a glyph tip's miter reaches (TrimmedTip), for half width h: beside an
    edge shorter than h, h itself, so the stroke ends flush around the tip (cut at its own reach
    along the tip; the miter trimmed to its shorter edge past the stroke still left a small point,
    the owner's check, 2026-10-06); otherwise the whole miter, but no farther past the stroke's
    edge than the shorter edge (EffectMiterReach). The same past the limit, where a glyph tip is
    cut flush instead of beveled as InDesign's offset bevels it (its two corners mitered, a point
    of their own on Great Vibes' n). */
inline double TipReach(const Stub& s, double h)
{
    return s.edge < h ? std::min(s.ratio * h, h) : EffectMiterReach(s, h);
}

/** Where InDesign strokes an offset of the outline (an Outside stroke strokes it offset outward
    by half the weight w, an Inside stroke offset inward), the offset's corner on the stroke's
    side (an exterior corner for Outside, an interior one for Inside) follows the join: extended
    to a point for a miter within the limit, an arc for a round join, and otherwise beveled
    straight from c1 = v + n1 w / 2 to c2 = v + n2 w / 2 (measured on InDesign 21.6's PDF,
    2026-10-04). The offset is then stroked with the same join, at half width w / 2. A miter
    within the limit reaches exactly the outline's own miter at width 2w (JoinPolygon), and a
    round join exactly the round stroke of the outline; a beveled offset does neither. This is
    the beveled offset's stroke beyond the outline's own bevel: the polygon v, v + n1 w, the
    stroke's corners around c1 and c2, v + n2 w. Those corners are beveled, or with `mitered` (a
    Miter join past its limit) mitered where their own ratio (they turn half as far) is within
    `limit`. */
inline std::vector<Pt> OffsetBevel(const Stub& s, double w, double limit, bool mitered)
{
    Pt nIn = {-s.u.y, s.u.x}, nOut = {-s.w.y, s.w.x};
    if (Dot(nIn, s.outward) < 0) nIn = nIn * -1;
    if (Dot(nOut, s.outward) < 0) nOut = nOut * -1;
    const Pt c1 = s.v + nIn * (w / 2), c2 = s.v + nOut * (w / 2);
    const Pt b1 = c1 + s.outward * (w / 2), b2 = c2 + s.outward * (w / 2);   // the bevel's stroke edge
    const Pt e0 = s.v + nIn * w, e1 = s.v + nOut * w;
    std::vector<Pt> poly{s.v, e0};
    // The bevel turns half as far as the corner: cos(turn / 4) = sqrt((1 + cos(turn / 2)) / 2).
    const double half = std::sqrt(std::max(0.0, (1 + Dot(nIn, s.outward)) / 2));
    const bool tips = mitered && half > 0 && 1 / half <= limit;
    const Pt d = b2 - b1;
    auto meet = [&](Pt p, Pt dir, Pt& out) {   // where the line p + t dir meets the bevel's edge
        const double den = Cross(dir, d);
        if (std::fabs(den) <= 1e-12 * std::max(1.0, Len(d))) return false;
        out = p + dir * (Cross(b1 - p, d) / den);
        return true;
    };
    Pt t1, t2;
    if (tips && meet(e0, s.u, t1) && meet(e1, s.w * -1, t2)) { poly.push_back(t1); poly.push_back(t2); }
    else { poly.push_back(b1); poly.push_back(b2); }
    poly.push_back(e1);
    return poly;
}

/** A whole point past the limit beyond the bevel a patterned stroke (dashes, dots, stripes)
    draws there, as a polygon to fill solid in the stroke's paint: the miter cut at `distance`
    from the vertex (CutMiter), less what the stroke itself paints at the corner. For a Center
    stroke of half width h, the stroke's bevel from e0 to e1; for an Outside stroke of weight h
    (`offset`), the beveled offset's stroke (OffsetBevel, mitered within `limit`). The point
    does not carry the pattern (the owner's choice, 2026-10-05). */
inline std::vector<Pt> PointBeyondBevel(const Stub& s, double h, double distance, bool offset, double limit)
{
    const std::vector<Pt> cut = CutMiter(s, h, distance);   // v, e0, tip or its cut, e1
    std::vector<Pt> poly(cut.begin() + 1, cut.end());
    if (offset) {
        const std::vector<Pt> bevel = OffsetBevel(s, h, limit, true);   // v, e0, two edge points, e1
        for (size_t k = bevel.size() - 2; k >= 2; --k) poly.push_back(bevel[k]);
    }
    return poly;
}

/** The exterior corners only: the joins an Outside stroke draws. */
inline std::vector<Stub> ExteriorCorners(const std::vector<Contour>& contours, const Options& o)
{
    return Corners(contours, o, false);
}

} // namespace ise_miter
