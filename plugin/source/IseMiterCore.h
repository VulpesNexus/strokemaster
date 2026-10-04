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
    probe points just off both sides of each edge's midpoint (up to 64 edges per ring). A clip
    to the outside of a shape can only be built with the even-odd rule, so it is used only when
    this holds. */
inline bool FillRulesAgree(const std::vector<Ring>& rings, double probe)
{
    for (const Ring& r : rings) {
        const size_t n = r.points.size();
        const size_t step = std::max<size_t>(1, n / 64);
        for (size_t i = 0; i < n; i += step) {
            const Pt a = r.points[i], b = r.points[(i + 1) % n];
            const Pt d = b - a;
            const double l = Len(d);
            if (l <= 1e-12) continue;
            const Pt m = (a + b) * 0.5, nrm = {-d.y / l, d.x / l};
            for (int side = -1; side <= 1; side += 2) {
                const Pt q = m + nrm * (probe * side);
                const int w = Winding(rings, q);
                int crossings = 0;
                for (const Ring& s : rings) {
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
    empty and the shape's stroke stays InDesign's. */
inline std::vector<Contour> OutsideClip(const std::vector<Contour>& contours, double margin, double tol, double probe)
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
        for (size_t j = 0; j < rings.size(); ++j)
            if (j != i && Winding({rings[j]}, rings[i].points[0]) != 0) ++depth;
        const bool positive = SignedArea(rings[i]) > 0, wantPositive = depth % 2 == 1;
        out.push_back(positive == wantPositive ? *source[i] : Reversed(*source[i], eps));
    }
    const std::vector<Ring> clip = Flatten(out, tol, eps);
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
                if ((Winding(clip, q) != 0) != (Winding(rings, q) == 0)) return {};
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

/** How far past its vertex an exterior miter beyond the miter limit keeps its point, for half
    width h: the whole miter (ratio * h) when that is no longer than the shorter of the corner's
    two edges, otherwise as far as that edge reaches; never less than the limit distance, nor
    more than `longest` half widths. A sharp corner between long edges is the shape's own
    point, and its stroke keeps it whole; one between short edges (the notches and thorns of
    decorative fonts, a near-reversal) would throw a point many times longer than the detail
    it comes from, and is cut. Measured on Kerberos Fang (2026-10-04): every corner past limit
    4 has edges under 16 pt at 72 pt, so its cut stays at the limit distance. */
inline double PointReach(const Stub& s, double h, double limit, double longest)
{
    return std::min(longest * h, std::max(limit * h, std::min(s.ratio * h, s.edge)));
}

/** The exterior corners only: what an Outside stroke restrokes. */
inline std::vector<Stub> ExteriorCorners(const std::vector<Contour>& contours, const Options& o)
{
    return Corners(contours, o, false);
}

} // namespace ise_miter
