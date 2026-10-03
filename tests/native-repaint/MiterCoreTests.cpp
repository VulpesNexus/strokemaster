// Pure tests for plugin/source/IseMiterCore.h: which corners keep their miter. No Adobe host.
#include "../../plugin/source/IseMiterCore.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace ise_miter;

static int checks = 0;
static void Check(bool ok, const char* what)
{
    ++checks;
    if (!ok) { std::printf("FAIL: %s\n", what); std::exit(1); }
}

static Contour Poly(const std::vector<Pt>& pts)
{
    Recorder r;
    r.MoveTo(pts[0]);
    for (size_t i = 1; i < pts.size(); ++i) r.LineTo(pts[i]);
    r.Close();
    return r.Contours()[0];
}

static Options With(double h) { Options o; o.halfWidth = h; return o; }

static bool Near(Pt a, Pt b) { return Len(a - b) < 1e-9; }

static bool HasCornerAt(const std::vector<Stub>& s, Pt v)
{
    for (const Stub& x : s) if (Near(x.v, v)) return true;
    return false;
}

int main()
{
    // A square keeps all four miters, wound either way.
    const Contour ccw = Poly({{0, 0}, {10, 0}, {10, 10}, {0, 10}});
    const Contour cw = Poly({{0, 0}, {0, 10}, {10, 10}, {10, 0}});
    Check(ExteriorCorners({ccw}, With(1)).size() == 4, "counterclockwise square: four exterior corners");
    Check(ExteriorCorners({cw}, With(1)).size() == 4, "clockwise square: four exterior corners");

    // An L-shape: its reflex corner's join points into the fill.
    const Contour ell = Poly({{0, 0}, {10, 0}, {10, 4}, {4, 4}, {4, 10}, {0, 10}});
    const std::vector<Stub> l = ExteriorCorners({ell}, With(1));
    Check(l.size() == 5, "L-shape: five exterior corners");
    Check(!HasCornerAt(l, {4, 4}), "L-shape: the reflex corner keeps no miter");

    // A square with a square hole: the hole's corners miter into the material (the case of
    // the original thin-wall spike); only the outer corners keep a miter.
    const Contour hole = Poly({{2, 2}, {2, 8}, {8, 8}, {8, 2}});
    const std::vector<Stub> sh = ExteriorCorners({ccw, hole}, With(0.5));
    Check(sh.size() == 4, "square with a hole: only the four outer corners");
    Check(!HasCornerAt(sh, {2, 2}) && !HasCornerAt(sh, {8, 8}), "square with a hole: no hole corner");

    // A thin chevron, like the notch of a v: the notch's join points down through the thin
    // bottom, where its miter tip would leave the fill; it must be dropped.
    const Contour v = Poly({{0, 10}, {2, 10}, {5, 1.5}, {8, 10}, {10, 10}, {5, 0}});
    const std::vector<Stub> sv = ExteriorCorners({v}, With(1));
    Check(!HasCornerAt(sv, {5, 1.5}), "chevron: the notch keeps no miter");
    Check(HasCornerAt(sv, {5, 0}), "chevron: the outer point keeps its miter");
    // The dropped miter's tip really is outside the fill, below the bottom point.
    {
        const Pt u = Pt{3, -8.5} * (1 / Len(Pt{3, -8.5})), w = Pt{3, 8.5} * (1 / Len(Pt{3, 8.5}));
        const double half = std::acos(Dot(u * -1, w)) / 2;   // half the interior angle
        const Pt tip = Pt{5, 1.5} + (u - w) * (1 / Len(u - w)) * (1 / std::sin(half));
        const std::vector<Ring> rings = Flatten({v}, 1e-3, 1e-9);
        Check(tip.y < 0 && Winding(rings, tip) == 0, "chevron: the notch's miter tip pokes out below the glyph");
    }

    // Overlapping glyphs: a corner inside the other glyph's fill keeps no miter.
    const Contour a = Poly({{0, 0}, {10, 0}, {10, 10}, {0, 10}});
    const Contour b = Poly({{8, 2}, {18, 2}, {18, 12}, {8, 12}});
    const std::vector<Stub> sab = ExteriorCorners({a, b}, With(0.5));
    Check(!HasCornerAt(sab, {10, 10}) && !HasCornerAt(sab, {8, 2}), "overlap: corners inside the other fill keep no miter");
    Check(sab.size() == 6, "overlap: the six corners on the union's boundary keep theirs");

    // The closed two-anchor contour of the host controls: both vertices are exact
    // reversals; the native stroke bevels them, and so does the bevel stroke alone.
    Check(ExteriorCorners({Poly({{0, 0}, {30, 0}})}, With(1.5)).empty(), "closed two-anchor contour: no stub");

    // A collinear point joins nothing.
    Check(ExteriorCorners({Poly({{0, 0}, {5, 0}, {10, 0}, {10, 10}, {0, 10}})}, With(1)).size() == 4, "collinear point: no stub");

    // Stubs follow the corner's own tangents, at most half of each adjacent segment.
    {
        Options o = With(1); o.stub = 0.2;
        const std::vector<Stub> s = ExteriorCorners({ccw}, o);
        bool found = false;
        for (const Stub& x : s) {
            if (Near(x.v, {10, 0})) { found = true; Check(Near(x.a, {9.8, 0}) && Near(x.b, {10, 0.2}), "stub along both tangents"); }
        }
        Check(found, "stub at (10, 0)");
        o.stub = 50;
        for (const Stub& x : ExteriorCorners({ccw}, o)) Check(Len(x.a - x.v) <= 5 + 1e-12 && Len(x.b - x.v) <= 5 + 1e-12, "stub at most half a segment");
    }

    // Curves: a tangent comes from the nearest distinct control point, and a smooth
    // junction (equal tangents) joins nothing.
    {
        Recorder r;
        r.MoveTo({0, 0});
        r.LineTo({10, 0});
        r.CurveTo({10, 0}, {10, 10}, {0, 10});   // c1 coincides with the start: tangent from c2
        r.Close();
        const std::vector<Stub> s = ExteriorCorners(r.Contours(), With(0.5));
        bool corner = false;
        for (const Stub& x : s) {
            if (Near(x.v, {10, 0})) {
                corner = true;
                Check(x.outCurve && !x.inCurve, "the stub's outgoing piece is part of the curve, the incoming one a line");
                // The piece leaves v along the curve's tangent (from c2, since c1 == v)...
                const Pt t = x.out2 - x.v;
                Check(std::fabs(t.x) < 1e-12 && t.y > 0, "curve start tangent from c2");
                // ...and ends on the curve itself: B(s) for the head parameter s.
                const Pt p0 = {10, 0}, p1 = {10, 0}, p2 = {10, 10}, p3 = {0, 10};
                bool onCurve = false;
                for (int k = 1; k <= 5000 && !onCurve; ++k) {
                    const double u = k / 10000.0, m = 1 - u;
                    const Pt q = p0 * (m * m * m) + p1 * (3 * m * m * u) + p2 * (3 * m * u * u) + p3 * (u * u * u);
                    onCurve = Len(q - x.b) < 1e-3;
                }
                Check(onCurve, "the outgoing piece ends on the curve");
            }
        }
        Check(corner, "corner where a line meets a curve");

        // A split piece is the original curve: its midpoint matches the curve at the mapped parameter.
        {
            const Pt p0 = {0, 0}, p1 = {3, 6}, p2 = {7, 6}, p3 = {10, 0};
            Pt q1, q2, q3;
            CubicHead(p0, p1, p2, p3, 0.3, &q1, &q2, &q3);
            auto B = [](Pt a, Pt b, Pt c, Pt d, double u) { const double m = 1 - u; return a * (m * m * m) + b * (3 * m * m * u) + c * (3 * m * u * u) + d * (u * u * u); };
            Check(Len(B(p0, q1, q2, q3, 0.5) - B(p0, p1, p2, p3, 0.15)) < 1e-12, "head piece reproduces the curve");
            Pt r0, r1, r2;
            CubicTail(p0, p1, p2, p3, 0.8, &r0, &r1, &r2);
            Check(Len(B(r0, r1, r2, p3, 0.5) - B(p0, p1, p2, p3, 0.9)) < 1e-12, "tail piece reproduces the curve");
        }
        Recorder q;
        q.MoveTo({0, 0});
        q.CurveTo({5, 0}, {10, 5}, {10, 10});
        q.CurveTo({10, 15}, {5, 20}, {0, 20});
        q.Close();
        Check(!HasCornerAt(ExteriorCorners(q.Contours(), With(0.5)), {10, 10}), "smooth junction: no stub");
    }

    // A degenerate segment does not hide a corner, and an unusable width yields nothing.
    Check(ExteriorCorners({Poly({{0, 0}, {10, 0}, {10, 0}, {10, 10}, {0, 10}})}, With(1)).size() == 4, "zero-length segment skipped");
    Check(ExteriorCorners({ccw}, With(0)).empty() && ExteriorCorners({ccw}, With(NAN)).empty(), "invalid width: no stub");

    // Center strokes: interior corners are reported too, flagged, and exterior ones unflagged.
    {
        const std::vector<Stub> all = Corners({ell}, With(1), true);
        Check(all.size() == 6, "L-shape with interior corners: all six");
        int interior = 0;
        for (const Stub& x : all) if (x.interior) { ++interior; Check(Near(x.v, {4, 4}), "the interior stub is the reflex corner"); }
        Check(interior == 1, "L-shape: exactly one interior corner");
        const std::vector<Stub> sv2 = Corners({v}, With(1), true);
        bool notch = false;
        for (const Stub& x : sv2) if (Near(x.v, {5, 1.5})) { notch = true; Check(x.interior, "chevron notch flagged interior"); }
        Check(notch, "chevron notch reported for Center strokes");
        Check(Corners({ccw, hole}, With(0.5), true).size() == 8, "square with a hole: four outer and four interior");
        Check(ExteriorCorners({ell}, With(1)).size() == Corners({ell}, With(1), false).size(), "ExteriorCorners is Corners without interior");
        for (const Stub& x : ExteriorCorners({ccw, hole}, With(0.5))) Check(!x.interior, "exterior stubs are unflagged");
    }

    std::printf("miter core: %d checks passed\n", checks);
    return 0;
}
