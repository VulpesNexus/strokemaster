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
            if (Near(x.v, {10, 0})) { corner = true; Check(std::fabs(x.b.x - 10) < 1e-12 && x.b.y > 0, "curve start tangent from c2"); }
        }
        Check(corner, "corner where a line meets a curve");
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

    std::printf("miter core: %d checks passed\n", checks);
    return 0;
}
