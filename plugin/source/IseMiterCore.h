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
// exterior corner, a two-segment stub (a -> v -> b along the corner's own tangents) stroked
// with the run's miter join and limit. The renderer's own stroker builds each kept miter and
// applies the limit exactly as it does for the native stroke.
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
};

/** Collects an outline as IOutlineInfo emits it; coordinates are already placed. */
class Recorder {
public:
    void MoveTo(Pt p) { Finish(); fOpen = true; fCurrent = Contour(); fCurrent.start = p; }
    void LineTo(Pt p) { if (!fOpen) MoveTo(p); Segment s; s.p = p; fCurrent.segments.push_back(s); }
    void CurveTo(Pt c1, Pt c2, Pt p)
    {
        if (!fOpen) MoveTo(c1);
        Segment s; s.curve = true; s.c1 = c1; s.c2 = c2; s.p = p;
        fCurrent.segments.push_back(s);
    }
    void Close() { Finish(); }
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
};

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
        r.x0 = r.x1 = r.points[0].x; r.y0 = r.y1 = r.points[0].y;
        for (const Pt& p : r.points) {
            r.x0 = std::min(r.x0, p.x); r.x1 = std::max(r.x1, p.x);
            r.y0 = std::min(r.y0, p.y); r.y1 = std::max(r.y1, p.y);
        }
        rings.push_back(r);
    }
    return rings;
}

/** Nonzero winding number of q against the rings, skipping rings whose box excludes it. */
inline int Winding(const std::vector<Ring>& rings, Pt q)
{
    int w = 0;
    for (const Ring& r : rings) {
        if (q.x < r.x0 || q.x > r.x1 || q.y < r.y0 || q.y > r.y1) continue;
        const size_t n = r.points.size();
        for (size_t i = 0; i < n; ++i) {
            const Pt a = r.points[i], b = r.points[(i + 1) % n];
            if (a.y <= q.y) {
                if (b.y > q.y && Cross(b - a, q - a) > 0) ++w;
            } else if (b.y <= q.y && Cross(b - a, q - a) < 0) {
                --w;
            }
        }
    }
    return w;
}

/** A stub to restroke with the miter join: a -> v -> b, each piece a short part of the
    outline's own segment (a line, or a curve with controls), so the stub's band lies exactly
    on the stroke's and its square ends sit on the stroke's own cross-section. A straight
    stub along the tangent instead stuck out of the stroke where the outline curves tightly
    next to a corner (serif brackets; visible on Center strokes, measured 2026-10-03). An
    interior stub's join points into the fill; it is kept only for Center strokes, clipped to
    the fill (see Corners). */
struct Stub {
    Pt a, v, b;
    bool interior = false;
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

/** The curve parameter that cuts a piece of about `length` off a cubic's end. */
inline double PieceParameter(Pt p0, Pt p1, Pt p2, Pt p3, double length)
{
    const double approx = 0.5 * (Len(p1 - p0) + Len(p2 - p1) + Len(p3 - p2) + Len(p3 - p0));
    if (!(approx > 0)) return 0.5;
    return std::min(0.5, std::max(1e-4, length / approx));
}

struct Options {
    double halfWidth = 1;       // h: half of the drawn stroke width (the Outside weight)
    double minTurn = 0.5 * 3.14159265358979323846 / 180;   // radians; smaller turns join invisibly
    double probe = 0;           // distance of the exterior test point; 0 means 1% of h
    double stub = 0;            // stub length along each tangent; 0 means 5% of h
};

/** The corners of a run's outline, as stubs to stroke with the miter join: the exterior
    ones, and with includeInterior the interior ones too, flagged. An exact or numerically
    unresolved reversal is skipped: its miter is infinitely long, so the native stroke bevels
    it, and the bevel stroke already draws that.

    Outside strokes use the exterior stubs only. A Center stroke lies over its own fill, so
    an interior miter is visible there and belongs; only its part outside the fill escapes.
    Center strokes therefore also restroke the interior stubs, clipped to the fill. */
inline std::vector<Stub> Corners(const std::vector<Contour>& contours, const Options& o, bool includeInterior)
{
    std::vector<Stub> out;
    if (!(o.halfWidth > 0) || !std::isfinite(o.halfWidth)) return out;
    const double h = o.halfWidth;
    const double eps = 1e-9 * std::max(1.0, h);
    const double probe = o.probe > 0 ? o.probe : 0.01 * h;
    const double stub = o.stub > 0 ? o.stub : 0.05 * h;
    const std::vector<Ring> rings = Flatten(contours, probe * 0.1, eps);
    for (const Contour& c : contours) {
        const std::vector<Segment> segs = Closed(c, eps);
        const size_t n = segs.size();
        if (n < 2) continue;
        // Vertex i is the end of segment i and the start of segment i + 1 (wrapping).
        for (size_t i = 0; i < n; ++i) {
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
            const bool interior = Winding(rings, q) != 0;   // the join points into the fill
            if (interior && !includeInterior) continue;
            const double din = std::min(stub, 0.5 * Len(v - inFrom));
            const Pt nextEnd = next.p;
            const double dout = std::min(stub, 0.5 * Len(nextEnd - v));
            if (din <= eps || dout <= eps) continue;
            Stub st;
            st.v = v;
            st.interior = interior;
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

/** The exterior corners only: what an Outside stroke restrokes. */
inline std::vector<Stub> ExteriorCorners(const std::vector<Contour>& contours, const Options& o)
{
    return Corners(contours, o, false);
}

} // namespace ise_miter
