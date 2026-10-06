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
            Check(WholePoint(x, 2) && std::fabs(WholePointReach(x, 2, 500) - x.ratio * 2) < 1e-12,
                  "a point between straight edges no shorter than it is kept whole");
            Stub shape = x;
            shape.edge = 20;   // the census thorn: 10 degrees, 20 pt sides
            Check(WholePoint(shape, 4) && WholePoint(shape, 8), "the thorn's point, 2.3 and 4.6 times its sides, is whole at h 4 and 8");
            Check(!WholePoint(shape, 10), "at h 10 (5.7 times its sides) it is beveled");
            Stub tiny = x;
            tiny.edge = 0.12;   // a script glyph's tip: a 0.12 pt segment beside a curve
            Check(!WholePoint(tiny, 4), "an edge shorter than h: beveled");
            Stub curved = x;
            curved.outCurve = true;
            Check(!WholePoint(curved, 2), "a curve on either side: beveled");
            curved.outCurve = false; curved.inCurve = true;
            Check(!WholePoint(curved, 2), "a curve arriving: beveled");
            Stub needle = x;
            needle.ratio = 1e6; needle.edge = 1e9;
            Check(std::fabs(WholePointReach(needle, 2, 500) - 1000) < 1e-9, "a whole point reaches no farther than the longest limit");
            // TrimmedTip: within the limit, a glyph tip's miter is trimmed as an effect's corner's.
            Check(!TrimmedTip(x, 2), "straight edges at least h long: the whole miter");
            Check(TrimmedTip(tiny, 4) && TrimmedTip(curved, 2), "a short edge or a curve: trimmed");
            Stub foot = tiny;
            foot.ratio = std::sqrt(2.0);   // a serif foot's square corner beside a 1 pt edge
            Check(!TrimmedTip(foot, 4), "a square corner beside a short edge keeps its miter");
            Stub ear = curved;
            ear.ratio = 1.96; ear.edge = 1.26;   // Great Vibes' g: an ear beside a 1.26 pt curve
            Check(TrimmedTip(ear, 5), "a curved corner of 61 degrees beside a short edge: a glyph tip");
            ear.edge = 6;
            Check(!TrimmedTip(ear, 5), "the same corner between long curves (a leaf's point): its whole miter");
            Stub flat = curved;
            flat.ratio = 1.45; flat.edge = 1;
            Check(!TrimmedTip(flat, 5), "a curved corner blunter than 84 degrees: its whole miter");
            // TipReach: flush beside a short edge, otherwise trimmed to the shorter edge past the stroke.
            Check(std::fabs(TipReach(tiny, 4) - 4) < 1e-12, "a tip beside a short edge: cut flush at h");
            Stub longTip = curved;
            longTip.ratio = 4; longTip.edge = 3;
            Check(std::fabs(TipReach(longTip, 2) - 5) < 1e-12, "a curved tip between long edges: h past the stroke's edge by the edge");
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

    {
        // Two overlapping parallelograms (the coverage census's sheared squares, untransformed):
        // every edge midpoint of one lies on an edge of the other.
        const double k = std::tan(20 * 3.14159265358979323846 / 180);
        auto para = [k](double x, double y, double a) {
            return Poly({{x + k * y, y}, {x + a + k * y, y}, {x + a + k * (y + a), y + a}, {x + k * (y + a), y + a}});
        };
        const Contour pa = para(0, 0, 40), pb = para(20, 20, 40);
        Check(!FillRulesAgree(Flatten({pa, pb}, 1e-3, 1e-9), 0.04), "parallelograms: the probes see the overlap off the midpoints");
        Check(!ise_region::FillRulesAgree({pa, pb}, 1e-3), "parallelograms: the exact test sees the overlap");
        const Contour sq = Poly({{0, 0}, {10, 0}, {10, 10}, {0, 10}});
        const Contour opposite = Reversed(Poly({{2, 2}, {8, 2}, {8, 8}, {2, 8}}), 1e-9);
        const Contour same = Poly({{2, 2}, {8, 2}, {8, 8}, {2, 8}});
        Check(ise_region::FillRulesAgree({sq}, 1e-3), "exact: a square, both rules agree");
        Check(ise_region::FillRulesAgree({sq, opposite}, 1e-3), "exact: an opposite hole, both rules agree");
        Check(!ise_region::FillRulesAgree({sq, same}, 1e-3), "exact: a same-direction inner contour, the rules disagree");
        Check(ise_region::FillRulesAgree({sq, Poly({{10, 0}, {20, 0}, {20, 10}, {10, 10}})}, 1e-3), "exact: squares sharing an edge, both rules agree");
        Check(!ise_region::FillRulesAgree({sq, Poly({{9.9, 0}, {20, 0}, {20, 10}, {9.9, 10}})}, 1e-3), "exact: a 0.1-wide overlap, the rules disagree");

        // The outside clip, checked exactly.
        const std::vector<Contour> holedClip = OutsideClip({sq, opposite}, 5, 1e-3, 0.01);
        Check(ise_region::ClipsOutside(holedClip, {sq, opposite}, 1e-3), "exact: a holed square's clip is its outside");
        const std::vector<Contour> unchecked = OutsideClip({pa, pb}, 5, 1e-3, 0.01, false, false);
        Check(unchecked.size() == 3 && !ise_region::ClipsOutside(unchecked, {pa, pb}, 1e-3), "exact: a clip from overlapping contours is refused");
        const std::vector<Contour> region = ise_region::NonzeroRegion({pa, pb}, 1e-3);
        const std::vector<Contour> regionClip = OutsideClip(region, 5, 1e-3, 0.01, true, false);
        Check(region.size() == 1 && ise_region::ClipsOutside(regionClip, {pa, pb}, 1e-3), "exact: the parallelograms' region clip is their outside");
        std::vector<Contour> wrong = holedClip;
        wrong[1] = Reversed(wrong[1], 1e-9);   // the outline wound with the frame: its fill reads 2, not 0
        Check(!ise_region::ClipsOutside(wrong, {sq, opposite}, 1e-3), "exact: an outline wound with the frame is refused");
    }
    {
        // The region's corners the outline does not have (CornersNotOnOutline).
        const std::vector<Contour> squares = {Poly({{0, 0}, {10, 0}, {10, 10}, {0, 10}}), Poly({{5, 5}, {15, 5}, {15, 15}, {5, 15}})};
        const std::vector<Contour> union_ = ise_region::NonzeroRegion(squares, 1e-3);
        const std::vector<Stub> us = Corners(union_, With(1), true);
        const std::vector<char> ux = CornersNotOnOutline(us, squares, With(1), 4);
        Check(us.size() == 8 && ux.size() == 8, "two overlapping squares: eight region corners");
        for (size_t i = 0; i < us.size(); ++i) {
            const bool crossing = Near(us[i].v, {10, 5}) || Near(us[i].v, {5, 10});
            Check(ux[i] == (crossing ? 1 : 0), crossing ? "where the squares cross: not the outline's corner"
                                                       : "a square's own corner: the outline's");
            if (crossing) Check(us[i].interior, "where the squares cross, the region's corner is interior");
        }
        // A bowtie: the region pinches where the contour crosses itself, sharper than the limit.
        const std::vector<Contour> bowtie = {Poly({{0, -1}, {0, 1}, {10, -1}, {10, 1}})};
        const std::vector<Contour> pinched = ise_region::NonzeroRegion(bowtie, 1e-3);
        const std::vector<Stub> bs = Corners(pinched, With(1), true);
        const std::vector<char> bx = CornersNotOnOutline(bs, bowtie, With(1), 4);
        size_t pinches = 0;
        for (size_t i = 0; i < bs.size(); ++i) {
            if (Len(bs[i].v - Pt{5, 0}) < 1e-6) {
                ++pinches;
                // Either side of the pinch: its probe lands in the other lobe, so it reads interior.
                Check(bx[i] == 1 && bs[i].ratio > 4, "the bowtie's pinch: a corner past the limit the outline lacks");
            } else {
                Check(bx[i] == 0, "the bowtie's own corners: the outline's");
            }
        }
        Check(pinches >= 1, "the bowtie's region has a corner at its pinch");
        Check(CornersNotOnOutline({}, bowtie, With(1), 4).empty(), "no region corners: no flags");
    }
    {
        // Corners a corner effect adds (CornersNotAtAnchors): an inset corner cut into the
        // right-angle corner at (10, 10) of a square, as the effect's outline draws it.
        const std::vector<Pt> anchors = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
        const Contour inset = Poly({{0, 0}, {10, 0}, {10, 8}, {8, 8}, {8, 10}, {0, 10}});
        const std::vector<Stub> cs = Corners({inset}, With(1), true);
        const std::vector<char> made = CornersNotAtAnchors(cs, anchors);
        Check(cs.size() == 6 && made.size() == 6, "an inset corner: six corners");
        for (size_t i = 0; i < cs.size(); ++i) {
            const bool own = Near(cs[i].v, {0, 0}) || Near(cs[i].v, {10, 0}) || Near(cs[i].v, {0, 10});
            Check(made[i] == (own ? 0 : 1), own ? "a corner at an anchor: the path's own" : "a corner the effect cut: not at an anchor");
        }
        Check(CornersNotAtAnchors(cs, {}).size() == 6, "no anchors: every corner flagged");
        Check(CornersNotAtAnchors({}, anchors).empty(), "no corners: no flags");
        // The effect's miters within the limit (EffectMiterReach): the notch's right angles keep
        // their whole miter when their edges are as long as the stroke is wide.
        for (const Stub& s : cs) {
            if (s.interior) continue;
            Check(std::fabs(EffectMiterReach(s, 1) - s.ratio) < 1e-9, "a corner between edges longer than h: the whole miter");
        }
        {
            // A zigzag corner: 60 degrees between edges of 0.1 at h = 4 (ratio 2): reach 4.1, not 8.
            const Contour zig = Poly({{0, 0}, {0.1, 0}, {0.05, 0.0866}});
            for (const Stub& s : Corners({zig}, With(4), true)) {
                if (s.interior) continue;
                Check(std::fabs(EffectMiterReach(s, 4) - std::min(s.ratio * 4, 4 + s.edge)) < 1e-9 && EffectMiterReach(s, 4) < 4.2,
                      "a zigzag corner of 0.1 pt edges: the miter no farther past the stroke than its edge");
            }
        }
        // Small holes of a region (SmallHoles): a 1 pt hole is small at size 4, a 6 pt hole and
        // a 1 pt island are not, whichever way the region is wound.
        for (int flip = 0; flip < 2; ++flip) {
            std::vector<Contour> region = {Poly({{0, 0}, {40, 0}, {40, 40}, {0, 40}}), Poly({{5, 5}, {5, 6}, {6, 6}, {6, 5}}),
                                           Poly({{20, 20}, {20, 26}, {26, 26}, {26, 20}}), Poly({{50, 0}, {51, 0}, {51, 1}, {50, 1}})};
            if (flip) for (Contour& c : region) c = Reversed(c, 1e-9);
            const std::vector<char> tiny = SmallHoles(region, 4, 1e-3);
            Check(tiny.size() == 4 && !tiny[0] && tiny[1] && !tiny[2] && !tiny[3],
                  "small holes: the 1 pt hole only, not the 6 pt hole, the island, or the outline");
        }
        Check(SmallHoles({}, 4, 1e-3).empty(), "no contours: no flags");
        // Within the tolerance only: an anchor 0.002 off is not the corner's, 0.0005 off is.
        const std::vector<char> off = CornersNotAtAnchors(cs, {{0.002, 0}, {10, 0}, {0, 10}});
        const std::vector<char> inTol = CornersNotAtAnchors(cs, {{0.0005, 0}, {10, 0}, {0, 10}});
        for (size_t i = 0; i < cs.size(); ++i) {
            if (!Near(cs[i].v, {0, 0})) continue;
            Check(off[i] == 1, "an anchor beyond the tolerance does not claim the corner");
            Check(inTol[i] == 0, "an anchor within the tolerance claims the corner");
        }
    }
    {
        // Open contours: no closing line, no corner at either end.
        Recorder open(true);
        open.MoveTo({0, 0}); open.LineTo({10, 0}); open.LineTo({10, 10}); open.LineTo({0, 10});
        Recorder shut(true);
        shut.MoveTo({0, 0}); shut.LineTo({10, 0}); shut.LineTo({10, 10}); shut.LineTo({0, 10}); shut.Close();
        Recorder glyph;   // without trackOpen every contour is closed, as before
        glyph.MoveTo({0, 0}); glyph.LineTo({10, 0}); glyph.LineTo({10, 10});
        const Contour o = open.Contours()[0], c = shut.Contours()[0];
        Check(!o.closed && c.closed && glyph.Contours()[0].closed, "trackOpen marks a contour without closepath open");
        Check(Stroked(o, 1e-9).size() == 3 && Stroked(c, 1e-9).size() == 4 && Closed(o, 1e-9).size() == 4, "an open contour is stroked without its closing line");
        const std::vector<Stub> oc = Corners({o}, With(1), true), cc = Corners({c}, With(1), true);
        Check(oc.size() == 2 && HasCornerAt(oc, {10, 0}) && HasCornerAt(oc, {10, 10}), "an open contour: corners only between its segments");
        Check(cc.size() == 4, "the same contour closed: four corners");
        Check(Winding(Flatten({o}, 1e-3, 1e-9), {5, 5}) != 0, "an open contour still fills as if closed");
    }
    {
        // OffsetBevel: the stroke of an offset beveled at a square's corner.
        const Contour sq = Poly({{0, 0}, {10, 0}, {10, 10}, {0, 10}});
        for (const Stub& x : ExteriorCorners({sq}, With(4))) {
            if (!Near(x.v, {10, 0})) continue;
            const double w = 4, r2 = std::sqrt(2.0);
            const std::vector<Pt> bevel = OffsetBevel(x, w, 4, false);
            // v, v + n1 w, c1 + o w / 2, c2 + o w / 2, v + n2 w; o = (1, -1) / sqrt 2.
            Check(bevel.size() == 5 && Near(bevel[0], {10, 0}) && Near(bevel[1], {10, -4}) && Near(bevel[4], {14, 0}),
                  "offset bevel: the vertex and the stroke edge's corners at w");
            Check(Near(bevel[2], {10 + w / 2 / r2, -w / 2 - w / 2 / r2}) && Near(bevel[3], {10 + w / 2 + w / 2 / r2, -w / 2 / r2}),
                  "offset bevel: the bevel's own stroke edge, w / 2 beyond c1 and c2");
            // The bevel's edge lies 3.41 from the vertex, past the outline's bevel at 2.83.
            Check(std::fabs(Dot(bevel[2] - x.v, x.outward) - (w / 2 / r2 + w / 2)) < 1e-9, "offset bevel: reaches w / 2 (1 + cos 45 deg)");
            const std::vector<Pt> mitered = OffsetBevel(x, w, 4, true);
            // Each of the bevel's corners turns 45 degrees: mitered, their tips 0.207 w past e0, e1.
            Check(mitered.size() == 5 && Near(mitered[2], {10 + w * (1 / r2 - 0.5), -4}) && Near(mitered[3], {14, -w * (1 / r2 - 0.5)}),
                  "offset bevel, mitered: tips where the edges meet the bevel's edge");
            const std::vector<Pt> limited = OffsetBevel(x, w, 1.05, true);
            Check(Near(limited[2], bevel[2]) && Near(limited[3], bevel[3]), "offset bevel past its own limit: beveled");
            const std::vector<Pt> r = Counterclockwise(bevel);
            std::vector<Ring> rr = Flatten({Poly(r)}, 1e-3, 1e-9);
            Check(Winding(rr, {10.5, -3.5}) != 0 && Winding(rr, {13.5, -0.5}) != 0 && Winding(rr, {10.5, -3.9}) == 0 && Winding(rr, {10.5, 0.5}) == 0, "offset bevel: outside the square, at its corner");
        }
    }
    {
        // StrokeClip: where a stroke InDesign draws itself may paint.
        const Contour sq = Poly({{0, 0}, {10, 0}, {10, 10}, {0, 10}});
        auto inside = [](const std::vector<Contour>& clip, Pt p) {
            const std::vector<Ring> r = Flatten(clip, 1e-4, 1e-9);
            int evenOdd = 0;
            for (const Ring& ring : r) evenOdd += RingWinding(ring, p) != 0 ? 1 : 0;
            Check((Winding(r, p) != 0) == (evenOdd % 2 == 1), "a stroke clip reads the same under both rules");
            return Winding(r, p) != 0;
        };
        const std::vector<Contour> outsideBand = ise_region::StrokeClip({sq}, 1, 0.1, {}, {sq}, ise_region::FillMode::Remove, 1e-3);
        Check(!outsideBand.empty() && inside(outsideBand, {10.5, 5}) && inside(outsideBand, {11.05, 5}) && !inside(outsideBand, {11.2, 5}),
              "outside band: out to h + margin");
        Check(!inside(outsideBand, {9.5, 5}) && !inside(outsideBand, {5, 5}), "outside band: the fill taken away");
        // Mitered at limit 2: the corner reaches (h + margin) sqrt 2 along its diagonal.
        Check(inside(outsideBand, {11.0, -1.0}) && !inside(outsideBand, {11.3, -1.3}), "outside band: mitered at the corner, within (h + margin) sqrt 2");
        const std::vector<Contour> centerBand = ise_region::StrokeClip({sq}, 1, 0.1, {}, {sq}, ise_region::FillMode::Add, 1e-3);
        Check(inside(centerBand, {5, 5}) && inside(centerBand, {10.5, 5}) && !inside(centerBand, {11.2, 5}), "center band: the fill added");
        const std::vector<Contour> insideBand = ise_region::StrokeClip({sq}, 1, 0.1, {}, {sq}, ise_region::FillMode::Within, 1e-3);
        Check(inside(insideBand, {9.5, 5}) && inside(insideBand, {8.95, 5}) && !inside(insideBand, {8.8, 5}) && !inside(insideBand, {10.5, 5}),
              "inside band: within the fill, out to h + margin from its edge");
        const std::vector<Pt> join = {{10, 0}, {10, -1.1}, {11.5, -1.5}, {11.1, 0}};
        const std::vector<Contour> joined = ise_region::StrokeClip({sq}, 1, 0.1, {join}, {sq}, ise_region::FillMode::Remove, 1e-3);
        Check(inside(joined, {11.3, -1.3}) && !inside(outsideBand, {11.3, -1.3}), "a join past the band is added");
        Recorder line(true);
        line.MoveTo({0, 0}); line.LineTo({10, 0});
        const std::vector<Contour> lineBand = ise_region::StrokeClip(line.Contours(), 1, 0.1, {}, {}, ise_region::FillMode::Add, 1e-3);
        Check(inside(lineBand, {5, 1}) && inside(lineBand, {-1, 0}) && !inside(lineBand, {-1.2, 0}) && !inside(lineBand, {5, 1.2}),
              "an open line's band: round ends, h + margin either side");
        Check(ise_region::StrokeClip({sq}, 1, 0.1, {}, {}, ise_region::FillMode::Remove, 1e-3).empty(), "no fill and no open line: no clip");
        // An inner contour of an overlapping outline: its stroke lies inside the fill, so the
        // band follows the region's boundary only.
        const Contour inner = Poly({{4, 4}, {6, 4}, {6, 6}, {4, 6}});
        const std::vector<Contour> overlapBand = ise_region::StrokeClip({sq, inner}, 1, 0.1, {}, {sq, inner}, ise_region::FillMode::Remove, 1e-3);
        Check(!inside(overlapBand, {5, 5}) && !inside(overlapBand, {6.5, 5}) && inside(overlapBand, {10.5, 5}), "overlapping contours: the band around the region only");
        const std::vector<Pt> deep = {{9, 4}, {9.9, 4}, {9.9, 6}, {9, 6}};
        const std::vector<Contour> insideJoined = ise_region::StrokeClip({sq}, 1, 0.1, {deep, join}, {sq}, ise_region::FillMode::Within, 1e-3);
        Check(inside(insideJoined, {9.5, 5}) && !inside(insideJoined, {10.5, -0.5}), "inside band: a join's part in the fill only");
    }
    {
        // DistanceBands: an Outside patterned stroke's bands, by distance from the fill.
        using ise_region::BandCorner;
        using ise_region::BandJoin;
        const Contour sq = Poly({{0, 0}, {10, 0}, {10, 10}, {0, 10}});
        auto in = [](const std::vector<Contour>& region, Pt p) { return Winding(Flatten(region, 1e-4, 1e-9), p) != 0; };
        std::vector<BandCorner> miters;
        for (const Stub& s : ExteriorCorners({sq}, With(1))) {
            BandCorner c;
            c.stub = s;
            c.cap = s.ratio;
            miters.push_back(c);
        }
        bool ok = false;
        const auto whole = ise_region::DistanceBands({sq}, {{0, 2}}, BandJoin::Miter, miters, 0.03, 1e-3, &ok);
        Check(ok && whole.size() == 1 && !whole[0].empty(), "a band around a square");
        Check(in(whole[0], {11, 5}) && in(whole[0], {11.98, 5}) && !in(whole[0], {12.05, 5}) && !in(whole[0], {5, 5}),
              "the band: from the fill out to its distance");
        Check(in(whole[0], {9.985, 5}) && !in(whole[0], {9.95, 5}), "a band from 0 reaches the inset into the fill");
        Check(in(whole[0], {11.95, 11.95}) && !in(whole[0], {12.05, 12.05}), "a corner's miter: the whole point");
        const auto squared = ise_region::DistanceBands({sq}, {{0, 2}}, BandJoin::Miter, {}, 0.03, 1e-3, &ok);
        Check(ok && in(squared[0], {11.3, 11.3}) && !in(squared[0], {11.6, 11.6}), "without its miter a corner is squared at the distance");
        const auto rounded = ise_region::DistanceBands({sq}, {{0, 2}}, BandJoin::Round, {}, 0.03, 1e-3, &ok);
        Check(ok && in(rounded[0], {11.35, 11.35}) && !in(rounded[0], {11.45, 11.45}), "round joins: an arc of the distance");
        const auto stripes = ise_region::DistanceBands({sq}, {{0, 0.5}, {1.5, 2}}, BandJoin::Miter, miters, 0.03, 1e-3, &ok);
        Check(ok && stripes.size() == 2 && in(stripes[0], {10.25, 5}) && !in(stripes[0], {11, 5}) && !in(stripes[1], {11, 5}) &&
                  in(stripes[1], {11.75, 5}) && !in(stripes[1], {10.25, 5}),
              "stripes: each its own distances, the gap between them empty");
        Check(in(stripes[1], {11.6, 11.6}) && !in(stripes[1], {11.0, 11.0}) && in(stripes[0], {10.3, 10.3}),
              "stripes follow the corner's miter, each at its own distance");
        // Two squares overlapping at a corner, and an L: one outline, into the notch between them.
        const Contour sq2 = Poly({{8, 8}, {18, 8}, {18, 18}, {8, 18}});
        const auto pair = ise_region::DistanceBands({sq, sq2}, {{0, 1}}, BandJoin::Miter, {}, 0.03, 1e-3, &ok);
        Check(ok && in(pair[0], {10.5, 7.5}) && !in(pair[0], {9, 9}) && !in(pair[0], {12, 12}), "overlapping squares: one band, not through the fill");
        const Contour bend = Poly({{0, 0}, {10, 0}, {10, 4}, {4, 4}, {4, 10}, {0, 10}});
        const auto lBand = ise_region::DistanceBands({bend}, {{0, 2}}, BandJoin::Miter, {}, 0.03, 1e-3, &ok);
        Check(ok && in(lBand[0], {4.2, 4.2}) && in(lBand[0], {5.9, 5.9}) && !in(lBand[0], {6.1, 6.1}), "an L: the band fills its inner corner");
        // WordPath: the path a pattern stroker follows, the creases between letters rounded.
        const std::vector<Contour> around = ise_region::WordPath({sq}, 1, BandJoin::Miter, 4, true, 1e-3, &ok);
        double sideOff = 0;
        for (const Ring& r : Flatten(around, 1e-4, 1e-9))
            for (const Pt& p : r.points)
                if (p.x > 0.5 && p.x < 9.5) sideOff = std::max(sideOff, std::fabs(std::fabs(p.y - 5) - 6));
        Check(ok && around.size() == 1 && sideOff < 3e-3, "a square's path: its sides offset by half");
        Check(in(around, {10.95, 10.95}) && !in(around, {11.05, 11.05}), "a square's path: mitered corners");
        Check(ise_region::WordPath({sq}, 1, BandJoin::Miter, 4, false, 0.02, &ok)[0].segments.size() == 4,
              "a square's path: four anchors, its corners (corner adjustment fits a pattern between anchors)");
        // ContoursInteract: whether the contours' strokes meet.
        const Contour far = Poly({{20, 0}, {30, 0}, {30, 10}, {20, 10}});
        Check(!ise_region::ContoursInteract({sq, far}, 1, 1e-3), "squares 10 apart, reach 1: their strokes do not meet");
        Check(ise_region::ContoursInteract({sq, far}, 6, 1e-3), "squares 10 apart, reach 6: they meet");
        Check(ise_region::ContoursInteract({sq, sq2}, 1, 1e-3), "overlapping squares: they meet");
        Check(!ise_region::ContoursInteract({sq}, 1, 1e-3), "one contour: nothing to meet");
        const std::vector<Contour> sharp = ise_region::WordPath({bend}, 1, BandJoin::Miter, 4, false, 1e-3, &ok);
        Check(ok && sharp.size() == 1 && sharp[0].segments.size() == 6, "without creases an L's path keeps its six corners");
        const std::vector<Contour> creased = ise_region::WordPath({sq, sq2}, 1, BandJoin::Miter, 4, true, 1e-3, &ok);
        Check(ok && in(creased, {11.1, 6.9}) && !in(creased, {11.5, 6.5}), "overlapping squares: the crease between their offsets rounded");
        const Contour counter = Reversed(Poly({{3, 3}, {6, 3}, {6, 6}, {3, 6}}), 1e-9);
        Check(ise_region::WordPath({sq, counter}, 1, BandJoin::Miter, 4, true, 1e-3, &ok).size() == 1 && ok,
              "a counter whose offset is smaller than the stroke: dropped");
        // Uncovered: what the stroke along that path leaves at an L's inner corner.
        const std::vector<Contour> bendPath = ise_region::WordPath({bend}, 1, BandJoin::Miter, 4, true, 1e-3, &ok);
        const std::vector<Contour> notch = ise_region::Uncovered({bend}, bendPath, 1, 0.1, 1e-3, &ok);
        Check(ok && !notch.empty() && in(notch, {4.2, 4.2}) && !in(notch, {4.75, 4.75}), "an L's inner corner: a notch the stroke misses");
        Check(ise_region::Uncovered({sq}, around, 1, 0.1, 1e-3, &ok).empty() && ok, "a square leaves nothing uncovered");
        // FitCurves: a circle's polyline as a few curves within the tolerance; a square's corners kept.
        std::vector<Pt> circle;
        for (int k = 0; k < 400; ++k) {
            const double angle = 2 * 3.14159265358979323846 * k / 400;
            circle.push_back({10 * std::cos(angle), 10 * std::sin(angle)});
        }
        const Contour fitted = ise_region::FitCurves(circle, 1e-3);
        double worst = 0;
        for (const Ring& r : Flatten({fitted}, 1e-5, 1e-9))
            for (const Pt& p : r.points) worst = std::max(worst, std::fabs(Len(p) - 10));
        Check(fitted.segments.size() < 40 && worst < 4e-3, "a circle's 400 points: under 40 curves within the tolerance");
        const Contour box = ise_region::FitCurves({{0, 0}, {10, 0}, {10, 10}, {0, 10}}, 1e-3);
        bool corners = Near(box.start, {0, 0}) || Near(box.start, {10, 0}) || Near(box.start, {10, 10}) || Near(box.start, {0, 10});
        double off = 0;
        for (const Ring& r : Flatten({box}, 1e-5, 1e-9))
            for (const Pt& p : r.points) off = std::max(off, std::min(std::min(std::fabs(p.x), std::fabs(p.x - 10)), std::min(std::fabs(p.y), std::fabs(p.y - 10))));
        Check(corners && off < 1e-6, "a square: its corners kept, its sides straight");
    }

    std::printf("miter core: %d checks passed\n", checks);
    return 0;
}
