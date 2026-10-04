// Pure tests for plugin/source/IseMiterCore.h (which corners keep their miter) and
// IseRegionCore.h (the region an overlapping outline fills). No Adobe host.
#include "../../plugin/source/IseMiterCore.h"
#include "../../plugin/source/IseRegionCore.h"

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

    // Shapes: miter ratio, tip direction, the cut plane past the limit, and fill-rule agreement.
    {
        for (const Stub& x : ExteriorCorners({ccw}, With(1))) {
            Check(std::fabs(x.ratio - std::sqrt(2.0)) < 1e-12, "a right angle's miter ratio is sqrt 2");
            Check(std::fabs(Len(x.outward) - 1) < 1e-12, "tip direction is a unit vector");
            const std::vector<Ring> rings = Flatten({ccw}, 1e-3, 1e-9);
            Check(Winding(rings, x.v + x.outward * 0.01) == 0, "the tip points out of the square");
        }
        // A 10-degree wedge's apex: ratio 1/sin(5 deg), far past limit 4, cut at 4h.
        const double half = 5 * 3.14159265358979323846 / 180;
        const Contour wedge = Poly({{0, 0}, {60 * std::tan(half), -60}, {-60 * std::tan(half), -60}});
        const std::vector<Stub> sw = ExteriorCorners({wedge}, With(2));
        bool apex = false;
        for (const Stub& x : sw) {
            if (!Near(x.v, {0, 0})) continue;
            apex = true;
            Check(std::fabs(x.ratio - 1 / std::sin(half)) < 1e-9, "the apex's ratio is 1/sin(5 deg)");
            Check(x.outward.y > 0.999, "the apex tip points up");
            Check(std::fabs(Dot(x.u, x.u) - 1) < 1e-12 && std::fabs(Dot(x.w, x.w) - 1) < 1e-12, "the tangents are unit vectors");
            const std::vector<Pt> cut = CutMiter(x, 2, 4 * 2);
            Check(cut.size() == 5, "a miter past the limit is cut: five corners");
            Ring rc; rc.points = cut;
            rc.x0 = rc.y0 = -1e9; rc.x1 = rc.y1 = 1e9;
            Check(Winding({rc}, {0, 7.99}) != 0 && Winding({rc}, {0, 8.01}) == 0, "the cut keeps the tip up to 4h and no further");
            Check(Winding({rc}, {0, -0.01}) == 0, "the cut miter lies beyond the vertex, outside the wedge");
            // The cut's edge lies on the stroke's outer edge: the offset of the incoming side.
            const Pt nIn = Dot(Pt{-x.u.y, x.u.x}, x.outward) > 0 ? Pt{-x.u.y, x.u.x} : Pt{x.u.y, -x.u.x};
            Check(std::fabs(Dot(cut[2] - x.v, nIn) - 2) < 1e-9, "the cut's corner lies on the stroke's offset edge");
            const std::vector<Pt> whole = CutMiter(x, 2, 1e6);
            Check(whole.size() == 4 && Near(whole[2], x.v + x.outward * (x.ratio * 2)), "below the limit the full miter is kept, tip at ratio * h");
            // Its edges are 60.23 long and its whole miter reaches 22.94: kept whole.
            Check(std::fabs(x.edge - 60 / std::cos(half)) < 1e-9, "the apex's shorter edge is a side of the wedge");
            Check(std::fabs(PointReach(x, 2, 4, 500) - x.ratio * 2) < 1e-12, "a point no longer than its edges is kept whole");
            Stub shortEdges = x;
            shortEdges.edge = 12;
            Check(std::fabs(PointReach(shortEdges, 2, 4, 500) - 12) < 1e-12, "a point longer than its edges is cut where the shorter edge ends");
            shortEdges.edge = 0.3;
            Check(std::fabs(PointReach(shortEdges, 2, 4, 500) - 8) < 1e-12, "but never closer than the limit distance");
            Stub needle = x;
            needle.ratio = 1e6; needle.edge = 1e9;
            Check(std::fabs(PointReach(needle, 2, 4, 500) - 1000) < 1e-9, "nor farther than the longest limit");
        }
        Check(apex, "the wedge's apex is an exterior corner");
        const std::vector<Ring> one = Flatten({ccw}, 1e-3, 1e-9);
        const std::vector<Ring> holed = Flatten({ccw, hole}, 1e-3, 1e-9);
        const Contour sameWay = Poly({{2, 2}, {8, 2}, {8, 8}, {2, 8}});   // wound like the outer square
        const std::vector<Ring> nested = Flatten({ccw, sameWay}, 1e-3, 1e-9);
        Check(FillRulesAgree(one, 0.01), "a square: both fill rules agree");
        Check(FillRulesAgree(holed, 0.01), "an oppositely wound hole: both fill rules agree");
        Check(!FillRulesAgree(nested, 0.01), "a same-direction inner contour: nonzero fills it, even-odd does not");
    }

    // Reversal, and the outside clip that means the same under either fill rule.
    {
        const double area = SignedArea(Flatten({ccw}, 1e-3, 1e-9)[0]);
        Check(area > 0 && std::fabs(SignedArea(Flatten({Reversed(ccw, 1e-9)}, 1e-3, 1e-9)[0]) + area) < 1e-9, "reversal negates the area");
        Recorder q;
        q.MoveTo({0, 0});
        q.LineTo({10, 0});
        q.CurveTo({14, 4}, {14, 8}, {10, 10});
        q.LineTo({0, 10});
        q.Close();
        const Contour curved = q.Contours()[0];
        const Contour back = Reversed(curved, 1e-9);
        Check(Near(back.start, curved.start) && back.segments.size() == 4, "a reversed contour starts where it did");
        // Reversed: start, up the closing side to (0, 10), across to (10, 10), down the curve.
        Check(back.segments[2].curve && Near(back.segments[2].c1, {14, 8}) && Near(back.segments[2].c2, {14, 4}) && Near(back.segments[2].p, {10, 0}), "a reversed curve swaps its controls");
        const double ac = SignedArea(Flatten({curved}, 1e-4, 1e-9)[0]);
        Check(std::fabs(SignedArea(Flatten({back}, 1e-4, 1e-9)[0]) + ac) < 1e-6, "a reversed curve negates the area");

        auto excluded = [](const std::vector<Contour>& clip, Pt p) { return Winding(Flatten(clip, 1e-3, 1e-9), p) == 0; };
        for (const Contour& outer : {ccw, Reversed(ccw, 1e-9)}) {
            for (const Contour& inner : {hole, Reversed(hole, 1e-9)}) {
                const Contour in = SignedArea(Flatten({outer}, 1e-3, 1e-9)[0]) * SignedArea(Flatten({inner}, 1e-3, 1e-9)[0]) < 0 ? inner : Contour{};
                if (in.segments.empty()) continue;   // same-direction holes fail FillRulesAgree first
                const std::vector<Contour> clip = OutsideClip({outer, in}, 5, 1e-3, 0.01);
                Check(clip.size() == 3, "a holed square: frame, outer, and hole");
                Check(excluded(clip, {1, 1}) && !excluded(clip, {5, 5}) && !excluded(clip, {-2, 5}),
                      "the clip excludes the fill and keeps the hole and the outside, whatever the windings");
                Check(SignedArea(Flatten({clip[0]}, 1e-3, 1e-9)[0]) > 0 && Closed(clip[0], 1e-9).size() == 8,
                      "the frame is a counterclockwise octagon");
            }
        }
        // Two separate squares wound oppositely: both filled under either rule; both excluded.
        const Contour right = Reversed(Poly({{20, 0}, {30, 0}, {30, 10}, {20, 10}}), 1e-9);
        const std::vector<Contour> pair = OutsideClip({ccw, right}, 5, 1e-3, 0.01);
        Check(pair.size() == 3 && excluded(pair, {5, 5}) && excluded(pair, {25, 5}) && !excluded(pair, {15, 5}),
              "opposite windings side by side: both excluded, the gap kept");
        // A same-direction hole: nonzero fills it, so the clip cannot be built from nesting.
        Check(OutsideClip({ccw, Poly({{2, 2}, {8, 2}, {8, 8}, {2, 8}})}, 5, 1e-3, 0.01).empty(), "a same-direction hole yields no clip");
    }

    {
        // A corner's miter join as a filled polygon (MiterJoin): at a square's corner, the unit
        // square the miter adds beyond the vertex, whichever way the square is wound.
        for (const Contour& sq : {Poly({{0, 0}, {10, 0}, {10, 10}, {0, 10}}), Reversed(Poly({{0, 0}, {10, 0}, {10, 10}, {0, 10}}), 1e-9)}) {
            bool corner = false;
            for (const Stub& x : ExteriorCorners({sq}, With(1))) {
                if (!Near(x.v, {10, 0})) continue;
                corner = true;
                const std::vector<Pt> join = Counterclockwise(MiterJoin(x, 1));
                Ring r; r.points = join; r.x0 = r.y0 = -1e9; r.x1 = r.y1 = 1e9;
                Check(join.size() == 4 && std::fabs(SignedArea(r) - 1) < 1e-9, "a square corner's join: the unit square beyond it, wound counterclockwise");
                Check(Winding({r}, {10.5, -0.5}) == 1 && Winding({r}, {10.5, 0.5}) == 0, "the join lies outside the square, at its corner");
                Check(Near(join[0], x.v) || Near(join[1], x.v) || Near(join[2], x.v) || Near(join[3], x.v), "the join includes the vertex");
            }
            Check(corner, "the square's corner at (10, 0)");
        }
        {
            // With the outer half of the stroke along both stub pieces (JoinPolygon): at a square's
            // corner, with stubs one half width long, the join and a unit band on either side.
            Options o = With(1); o.stub = 1;
            for (const Stub& x : ExteriorCorners({Poly({{0, 0}, {10, 0}, {10, 10}, {0, 10}})}, o)) {
                if (!Near(x.v, {10, 0})) continue;
                Ring r; r.points = Counterclockwise(JoinPolygon(x, 1, x.ratio)); r.x0 = r.y0 = -1e9; r.x1 = r.y1 = 1e9;
                Check(std::fabs(SignedArea(r) - 3) < 1e-9, "a square corner's join with its bands: three unit squares");
                Check(Winding({r}, {9.5, -0.5}) == 1 && Winding({r}, {10.5, 0.5}) == 1 && Winding({r}, {10.5, -0.5}) == 1,
                      "the bands lie along both edges, outside the square");
                Check(Winding({r}, {9.5, 0.5}) == 0 && Winding({r}, {8.5, -0.5}) == 0, "nothing inside the square, nothing past the stub");
            }
            // A curve bending toward the join with a radius under h: its band stops before the
            // offset folds back, and the polygon stays simple.
            Recorder q;
            q.MoveTo({0, 0});
            q.LineTo({10, 0});
            q.CurveTo({10, 0.2}, {9.8, 0.4}, {9.6, 0.4});
            q.LineTo({0, 10});
            q.Close();
            for (const Stub& x : Corners(q.Contours(), With(2), true)) {
                const std::vector<Pt> poly = JoinPolygon(x, 2, x.ratio * 2);
                bool finite = true;
                for (const Pt& pt : poly) finite = finite && std::isfinite(pt.x) && std::isfinite(pt.y);
                Check(finite && poly.size() >= 4, "a band beside a tight curve stays finite");
            }
        }
        const std::vector<Pt> clockwise = Counterclockwise({{0, 0}, {0, 1}, {1, 1}, {1, 0}});
        Check(Near(clockwise[0], {1, 0}) && Near(clockwise[3], {0, 0}), "a clockwise polygon is reversed");
    }
    {
        // The region an overlapping outline fills (ise_region::NonzeroRegion).
        using ise_region::NonzeroRegion;
        auto area = [](const Contour& c) { return SignedArea(Flatten({c}, 1e-4, 1e-9)[0]); };
        auto excluded = [](const std::vector<Contour>& clip, Pt p) { return Winding(Flatten(clip, 1e-3, 1e-9), p) == 0; };
        const Contour sqA = Poly({{0, 0}, {10, 0}, {10, 10}, {0, 10}});
        const Contour sqB = Poly({{5, 5}, {15, 5}, {15, 15}, {5, 15}});
        Check(!FillRulesAgree(Flatten({sqA, sqB}, 1e-3, 1e-9), 0.01), "overlapping squares wound alike: the fill rules disagree");
        const std::vector<Contour> ab = NonzeroRegion({sqA, sqB}, 1e-3);
        Check(ab.size() == 1 && std::fabs(area(ab[0]) - 175) < 1e-6, "overlapping squares: one counterclockwise contour around their union");
        Check(Closed(ab[0], 1e-9).size() == 8, "overlapping squares: the union's eight edges");
        Check(FillRulesAgree(Flatten(ab, 1e-3, 1e-9), 0.01), "the region: both fill rules agree");
        Check(OutsideClip({sqA, sqB}, 5, 1e-3, 0.01).empty(), "overlapping squares: no clip from the contours themselves");
        const std::vector<Contour> abClip = OutsideClip(ab, 5, 1e-3, 0.01, true);
        Check(abClip.size() == 2 && excluded(abClip, {2, 2}) && excluded(abClip, {7, 7}) && excluded(abClip, {12, 12}) &&
              !excluded(abClip, {12, 2}) && !excluded(abClip, {-2, 5}), "the region's clip excludes the whole union, overlap included");
        const std::vector<Contour> cwRegion = NonzeroRegion({Reversed(sqA, 1e-9)}, 1e-3);
        Check(cwRegion.size() == 1 && std::fabs(area(cwRegion[0]) - 100) < 1e-6, "a clockwise square: wound counterclockwise in the region");
        const Contour inner = Poly({{2, 2}, {8, 2}, {8, 8}, {2, 8}});
        const std::vector<Contour> sameHole = NonzeroRegion({sqA, inner}, 1e-3);
        Check(sameHole.size() == 1 && std::fabs(area(sameHole[0]) - 100) < 1e-6, "a same-direction inner contour: filled, no hole");
        const std::vector<Contour> holed = NonzeroRegion({sqA, Reversed(inner, 1e-9)}, 1e-3);
        double outerArea = 0, holeArea = 0;
        for (const Contour& c : holed) (area(c) > 0 ? outerArea : holeArea) += area(c);
        Check(holed.size() == 2 && std::fabs(outerArea - 100) < 1e-6 && std::fabs(holeArea + 36) < 1e-6, "an opposite hole: outer counterclockwise, hole clockwise");
        const std::vector<Contour> holedClip = OutsideClip(holed, 5, 1e-3, 0.01, true);
        Check(!holedClip.empty() && excluded(holedClip, {1, 1}) && !excluded(holedClip, {5, 5}), "the holed region's clip keeps the hole");
        Check(NonzeroRegion({Poly({{0, 0}, {5, 0}, {10, 0}})}, 1e-3).empty(), "a collinear contour fills nothing");

        // Overlapping circles: the boundary is rebuilt from the circles' own curves, cut where
        // they cross, not from the flattening.
        auto circle = [](Pt c, double r, bool clockwise) {
            const double k = 0.5522847498 * r, s = clockwise ? -1 : 1;
            Recorder rec;
            rec.MoveTo({c.x + r, c.y});
            rec.CurveTo({c.x + r, c.y + s * k}, {c.x + k, c.y + s * r}, {c.x, c.y + s * r});
            rec.CurveTo({c.x - k, c.y + s * r}, {c.x - r, c.y + s * k}, {c.x - r, c.y});
            rec.CurveTo({c.x - r, c.y - s * k}, {c.x - k, c.y - s * r}, {c.x, c.y - s * r});
            rec.CurveTo({c.x + k, c.y - s * r}, {c.x + r, c.y - s * k}, {c.x + r, c.y});
            rec.Close();
            return rec.Contours()[0];
        };
        for (bool opposite : {false, true}) {
            const std::vector<Contour> disks = NonzeroRegion({circle({0, 0}, 10, false), circle({12, 0}, 10, opposite)}, 1e-3);
            // Opposite windings cancel where the circles overlap: two crescents, no lens.
            Check(disks.size() == (opposite ? 2u : 1u), opposite ? "circles wound oppositely: two crescents" : "circles wound alike: one contour");
            size_t curves = 0, segments = 0;
            bool onCircles = true;
            for (const Contour& c : disks) {
                Check(area(c) > 0, "every contour of the disks' region winds counterclockwise");
                Pt cur = c.start;
                for (const Segment& s : Closed(c, 1e-9)) {
                    ++segments;
                    if (s.curve) {
                        ++curves;
                        const Pt mid = cur * 0.125 + s.c1 * 0.375 + s.c2 * 0.375 + s.p * 0.125;
                        onCircles = onCircles && std::min(std::fabs(Len(mid) - 10), std::fabs(Len(mid - Pt{12, 0}) - 10)) < 0.01;
                    }
                    cur = s.p;
                }
            }
            Check(curves >= 6 && segments <= curves + 4 * disks.size(), "the boundary is the circles' own curves, joined by at most a hair");
            Check(onCircles, "every rebuilt curve lies on its circle, in the boundary's direction");
            const double d = 12, r = 10, pi = 3.14159265358979323846;
            const double lensArea = 2 * r * r * std::acos(d / (2 * r)) - d / 2 * std::sqrt(4 * r * r - d * d);
            double total = 0;
            for (const Contour& c : disks) total += area(c);
            Check(std::fabs(total - (opposite ? 2 * pi * r * r - 2 * lensArea : 2 * pi * r * r - lensArea)) < 0.5, "the region's area");
        }
    }

    std::printf("miter core: %d checks passed\n", checks);
    return 0;
}
