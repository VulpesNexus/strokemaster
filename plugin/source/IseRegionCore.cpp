//========================================================================================
// ise_region::NonzeroRegion (IseRegionCore.h). Compiled without the SDK's precompiled header;
// USINGZ must match IseClipper2.cpp, which compiles Clipper2 itself.
//========================================================================================
#define USINGZ
#include "IseRegionCore.h"
#include "clipper2/clipper.engine.h"
#include "clipper2/clipper.h"   // InflatePaths (clipper.offset.cpp)

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <tuple>
#include <utility>

namespace ise_region {
namespace {

using ise_miter::Contour;
using ise_miter::Pt;
using ise_miter::Segment;

/** A place on the outline: a segment and its parameter (a line's fraction, a curve's t). */
struct Place {
    int seg = -1;
    double t = 0;
};

/** The outline's segments, and where each Z value Clipper2 carries lies on them. Z values
    start at 1 (Clipper2 gives 0 to points it knows nothing about): the flattened vertices
    first, then the crossings Clipper2 reports. */
struct Outline {
    std::vector<Pt> from;        // each segment's start point
    std::vector<Segment> segs;
    std::vector<int> previous;   // the segment before each one in its contour
    std::vector<Place> vertex;   // a vertex at a segment's start has t = 0
    std::vector<std::pair<Place, Place>> crossing;
    double tol = 0;              // the flattening tolerance
};

Pt CubicAt(Pt p0, Pt p1, Pt p2, Pt p3, double t)
{
    const double s = 1 - t;
    return p0 * (s * s * s) + p1 * (3 * s * s * t) + p2 * (3 * s * t * t) + p3 * (t * t * t);
}

Pt At(const Outline& o, Place p)
{
    const Segment& s = o.segs[p.seg];
    return s.curve ? CubicAt(o.from[p.seg], s.c1, s.c2, s.p, p.t) : ise_miter::Lerp(o.from[p.seg], s.p, p.t);
}

/** Where a Z value lies: one place, or two (a segment's start is also the end of the one
    before; a crossing lies on two edges). */
int Places(const Outline& o, int64_t z, Place out[2])
{
    if (z <= 0) return 0;
    const size_t i = static_cast<size_t>(z - 1);
    int n = 0;
    if (i < o.vertex.size()) {
        out[n++] = o.vertex[i];
        if (o.vertex[i].t == 0) { Place end; end.seg = o.previous[o.vertex[i].seg]; end.t = 1; out[n++] = end; }
    } else if (i - o.vertex.size() < o.crossing.size()) {
        const std::pair<Place, Place>& c = o.crossing[i - o.vertex.size()];
        if (c.first.seg >= 0) out[n++] = c.first;
        if (c.second.seg >= 0) out[n++] = c.second;
    }
    return n;
}

/** The segment an edge of the flattened outline runs along, and its parameters at both ends.
    Of the pairs of places on one segment, the one whose middle lies nearest the edge's wins:
    a segment's start is also the end of the one before, so a short curve and the closing line
    that meet at the same two points both join an edge's ends, and a contour of one segment
    starts and ends on it. None when no segment's middle lies near the edge. */
bool Along(const Outline& o, const Clipper2Lib::PointD& pa, const Clipper2Lib::PointD& pb, Place* a, Place* b)
{
    Place ca[2], cb[2];
    const int na = Places(o, pa.z, ca), nb = Places(o, pb.z, cb);
    const Pt mid = {(pa.x + pb.x) / 2, (pa.y + pb.y) / 2};
    double best = 0.25 * std::hypot(pb.x - pa.x, pb.y - pa.y) + 2 * o.tol;
    bool found = false;
    for (int i = 0; i < na; ++i)
        for (int j = 0; j < nb; ++j) {
            if (ca[i].seg != cb[j].seg) continue;
            Place m;
            m.seg = ca[i].seg;
            m.t = (ca[i].t + cb[j].t) / 2;
            const double d = ise_miter::Len(At(o, m) - mid);
            if (d <= best) { best = d; *a = ca[i]; *b = cb[j]; found = true; }
        }
    return found;
}

/** Where a crossing at `pt` lies on the edge bot -> top, by its fraction along the edge. */
Place OnEdge(const Outline& o, const Clipper2Lib::PointD& bot, const Clipper2Lib::PointD& top, const Clipper2Lib::PointD& pt)
{
    Place a, b;
    if (!Along(o, bot, top, &a, &b)) return Place();
    const double dx = top.x - bot.x, dy = top.y - bot.y, l2 = dx * dx + dy * dy;
    double f = l2 > 0 ? ((pt.x - bot.x) * dx + (pt.y - bot.y) * dy) / l2 : 0;
    f = std::min(1.0, std::max(0.0, f));
    Place p;
    p.seg = a.seg;
    p.t = a.t + (b.t - a.t) * f;
    return p;
}

void Flatten(Pt p0, Pt p1, Pt p2, Pt p3, double t0, double t1, double tol, int depth,
             std::vector<Pt>* pts, std::vector<double>* ts)
{
    const Pt d = p3 - p0;
    const double n = ise_miter::Len(d);
    const double e1 = n > 0 ? std::fabs(ise_miter::Cross(p1 - p0, d)) / n : ise_miter::Len(p1 - p0);
    const double e2 = n > 0 ? std::fabs(ise_miter::Cross(p2 - p0, d)) / n : ise_miter::Len(p2 - p0);
    if (depth >= 16 || std::max(e1, e2) <= tol) { pts->push_back(p3); ts->push_back(t1); return; }
    const Pt a = (p0 + p1) * 0.5, b = (p1 + p2) * 0.5, c = (p2 + p3) * 0.5;
    const Pt ab = (a + b) * 0.5, bc = (b + c) * 0.5, m = (ab + bc) * 0.5;
    const double tm = (t0 + t1) / 2;
    Flatten(p0, a, ab, m, t0, tm, tol, depth + 1, pts, ts);
    Flatten(m, bc, c, p3, tm, t1, tol, depth + 1, pts, ts);
}

/** A segment's piece between parameters t0 and t1, in that direction. */
Segment Piece(const Outline& o, int seg, double t0, double t1)
{
    const Segment& s = o.segs[seg];
    Segment out;
    if (!s.curve) { out.p = ise_miter::Lerp(o.from[seg], s.p, t1); return out; }
    const double lo = std::min(t0, t1), hi = std::max(t0, t1);
    Pt q1, q2, q3, r0, r1, r2;
    ise_miter::CubicHead(o.from[seg], s.c1, s.c2, s.p, hi, &q1, &q2, &q3);   // [0, hi]
    ise_miter::CubicTail(o.from[seg], q1, q2, q3, hi > 0 ? lo / hi : 0, &r0, &r1, &r2);   // [lo, hi]
    out.curve = true;
    if (t0 <= t1) { out.c1 = r1; out.c2 = r2; out.p = q3; }
    else { out.c1 = r2; out.c2 = r1; out.p = r0; }
    return out;
}

/** The parameter of the point of segment `seg` nearest p: exact on a line; on a curve, the
    best of 65 samples refined by ternary search. `hint` is kept when no point is nearer. */
double Project(const Outline& o, int seg, Pt p, double hint)
{
    const Segment& s = o.segs[seg];
    const Pt a = o.from[seg];
    if (!s.curve) {
        const Pt d = s.p - a;
        const double l2 = ise_miter::Dot(d, d);
        return l2 > 0 ? std::min(1.0, std::max(0.0, ise_miter::Dot(p - a, d) / l2)) : hint;
    }
    auto dist2 = [&](double t) { const Pt q = CubicAt(a, s.c1, s.c2, s.p, t) - p; return ise_miter::Dot(q, q); };
    double best = hint, bestD = dist2(hint);
    for (int i = 0; i <= 64; ++i) {
        const double t = i / 64.0, d = dist2(t);
        if (d < bestD) { bestD = d; best = t; }
    }
    double lo = std::max(0.0, best - 1 / 64.0), hi = std::min(1.0, best + 1 / 64.0);
    for (int it = 0; it < 40; ++it) {
        const double m1 = lo + (hi - lo) / 3, m2 = hi - (hi - lo) / 3;
        if (dist2(m1) < dist2(m2)) hi = m2; else lo = m1;
    }
    const double t = (lo + hi) / 2;
    return dist2(t) < bestD ? t : best;
}

/** Distance from p to the polyline through pts. */
double ToPolyline(const std::vector<Pt>& pts, Pt p)
{
    double best = ise_miter::Len(p - pts[0]);
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        const Pt d = pts[i + 1] - pts[i];
        const double l2 = ise_miter::Dot(d, d);
        const double t = l2 > 0 ? std::min(1.0, std::max(0.0, ise_miter::Dot(p - pts[i], d) / l2)) : 0;
        best = std::min(best, ise_miter::Len(pts[i] + d * t - p));
    }
    return best;
}

/** One edge of Clipper2's output, and the segment it runs along when known. */
struct Edge {
    Pt pa, pb;
    bool along = false;
    Place a, b;
};

bool Continues(const Edge& before, const Edge& e)
{
    return before.along && e.along && before.b.seg == e.a.seg && before.b.t == e.a.t &&
           (before.b.t - before.a.t > 0) == (e.b.t - e.a.t > 0);
}

/** A boundary ring rebuilt from the outline's own segments. Z values name the segment each
    edge runs along; where on it each vertex lies comes from the vertex itself, projected onto
    the segment (a crossing's place from Clipper2's callback was measured wrong by a tenth of a
    segment on German Beauty, 2026-10-04). A run that still strays from Clipper2's polygon by
    more than a few flattening tolerances is drawn as that polygon instead. */
Contour Rebuild(const Outline& o, const Clipper2Lib::PathD& ring)
{
    const size_t m = ring.size();
    std::vector<Edge> edges(m);
    // One projection per vertex, segment, and hint, so the two edges at a vertex agree.
    std::map<std::tuple<size_t, int, double>, double> placed;
    auto place = [&](size_t i, Place hint) {
        const auto key = std::make_tuple(i, hint.seg, hint.t);
        auto it = placed.find(key);
        if (it == placed.end()) it = placed.emplace(key, Project(o, hint.seg, {ring[i].x, ring[i].y}, hint.t)).first;
        Place p;
        p.seg = hint.seg;
        p.t = it->second;
        return p;
    };
    for (size_t i = 0; i < m; ++i) {
        const Clipper2Lib::PointD& p = ring[i];
        const Clipper2Lib::PointD& q = ring[(i + 1) % m];
        Edge& e = edges[i];
        e.pa = {p.x, p.y};
        e.pb = {q.x, q.y};
        e.along = Along(o, p, q, &e.a, &e.b);
        if (e.along) { e.a = place(i, e.a); e.b = place((i + 1) % m, e.b); }
        e.along = e.along && e.a.t != e.b.t;
    }
    size_t first = 0;   // an edge that starts a run, so no run wraps past the ring's end
    for (size_t i = 0; i < m; ++i)
        if (!Continues(edges[(i + m - 1) % m], edges[i])) { first = i; break; }
    // Where two runs meet at a crossing, each run's end lies on its own segment, within the
    // flattening tolerance of the other's: no hairline is drawn between them, since its
    // direction would be noise, and a stroke's corners are taken from these contours. The next
    // piece starts where the last one ended instead, its first control point moved with it.
    const double hair = 4 * o.tol;
    Contour c;
    bool started = false;
    Pt cur;
    auto lineTo = [&](Pt p) {
        if (!started) { c.start = p; started = true; cur = p; }
        else if (ise_miter::Len(p - cur) > hair) { Segment s; s.p = p; c.segments.push_back(s); cur = p; }
    };
    for (size_t k = 0; k < m; ++k) {
        const Edge& e = edges[(first + k) % m];
        if (!e.along) { lineTo(e.pa); lineTo(e.pb); continue; }
        std::vector<Pt> polygon{e.pa, e.pb};   // Clipper2's vertices along the run
        double t1 = e.b.t;
        while (k + 1 < m && Continues(edges[(first + k) % m], edges[(first + k + 1) % m])) {
            ++k;
            t1 = edges[(first + k) % m].b.t;
            polygon.push_back(edges[(first + k) % m].pb);
        }
        Place mid = e.a;
        bool onPolygon = true;
        for (double f : {0.0, 0.25, 0.5, 0.75, 1.0}) {
            mid.t = e.a.t + (t1 - e.a.t) * f;
            onPolygon = onPolygon && ToPolyline(polygon, At(o, mid)) <= 4 * o.tol + 1e-5;
        }
        if (!onPolygon) { for (const Pt& v : polygon) lineTo(v); continue; }
        const Pt from = At(o, e.a);
        lineTo(from);
        Segment s = Piece(o, e.a.seg, e.a.t, t1);
        if (s.curve) s.c1 = s.c1 + (cur - from);
        c.segments.push_back(s);
        cur = s.p;
    }
    // Likewise where the contour closes.
    if (!c.segments.empty() && ise_miter::Len(cur - c.start) <= hair) {
        Segment& last = c.segments.back();
        if (last.curve) last.c2 = last.c2 + (c.start - last.p);
        last.p = c.start;
    }
    return c;
}

/** Contours flattened (ise_miter::Flatten: curves within tol) as Clipper2 paths; a ring is
    closed, as a fill or clip reads it. */
Clipper2Lib::PathsD Paths(const std::vector<Contour>& contours, double tol)
{
    Clipper2Lib::PathsD out;
    for (const ise_miter::Ring& r : ise_miter::Flatten(contours, tol, 1e-9)) {
        Clipper2Lib::PathD path;
        for (const Pt& p : r.points) path.push_back(Clipper2Lib::PointD(p.x, p.y));
        out.push_back(path);
    }
    return out;
}

/** A contour flattened as a line: closed, or for an open contour without its closing line. */
Clipper2Lib::PathD Line(const Contour& c, double tol)
{
    Clipper2Lib::PathD path;
    path.push_back(Clipper2Lib::PointD(c.start.x, c.start.y));
    std::vector<Pt> pts;
    std::vector<double> ts;
    Pt cur = c.start;
    for (const Segment& s : ise_miter::Stroked(c, 1e-9)) {
        pts.clear(); ts.clear();
        if (s.curve) Flatten(cur, s.c1, s.c2, s.p, 0, 1, tol, 0, &pts, &ts);
        else pts.push_back(s.p);
        for (const Pt& p : pts) path.push_back(Clipper2Lib::PointD(p.x, p.y));
        cur = s.p;
    }
    return path;
}

/** The region of `paths` under a fill rule, as polygons that do not overlap. */
Clipper2Lib::PathsD Union(const Clipper2Lib::PathsD& paths, Clipper2Lib::FillRule rule, bool* ok, int precision = 5)
{
    Clipper2Lib::ClipperD clipper(precision);
    clipper.AddSubject(paths);
    Clipper2Lib::PathsD out;
    *ok = clipper.Execute(Clipper2Lib::ClipType::Union, rule, out) && *ok;
    return out;
}

/** Whether two regions (polygons that do not overlap) differ by more than slivers: their
    symmetric difference, shrunk by `slack`, still covers something. Two flattenings of one curve
    differ by slivers up to their tolerances thick; a fixed area threshold would reject them on
    a long outline and accept a real difference on a short one. */
bool Differ(const Clipper2Lib::PathsD& a, const Clipper2Lib::PathsD& b, double slack, bool* ok)
{
    Clipper2Lib::ClipperD clipper(5);
    clipper.AddSubject(a);
    clipper.AddClip(b);
    Clipper2Lib::PathsD out;
    *ok = clipper.Execute(Clipper2Lib::ClipType::Xor, Clipper2Lib::FillRule::NonZero, out) && *ok;
    if (out.empty()) return false;
    const Clipper2Lib::PathsD core = Clipper2Lib::InflatePaths(out, -slack, Clipper2Lib::JoinType::Miter,
                                                               Clipper2Lib::EndType::Polygon, 2.0, 5);
    return std::fabs(Clipper2Lib::Area(core)) > 0;
}

std::vector<Contour> Polygons(const Clipper2Lib::PathsD& paths)
{
    std::vector<Contour> out;
    for (const Clipper2Lib::PathD& path : paths) {
        if (path.size() < 3) continue;
        Contour c;
        c.start = {path[0].x, path[0].y};
        for (size_t i = 1; i < path.size(); ++i) { Segment s; s.p = {path[i].x, path[i].y}; c.segments.push_back(s); }
        out.push_back(c);
    }
    return out;
}

} // namespace

bool FillRulesAgree(const std::vector<Contour>& contours, double tol)
{
    if (!(tol > 0)) return false;
    const Clipper2Lib::PathsD paths = Paths(contours, tol);
    if (paths.empty()) return true;
    bool ok = true;
    const Clipper2Lib::PathsD nonzero = Union(paths, Clipper2Lib::FillRule::NonZero, &ok);
    const Clipper2Lib::PathsD evenOdd = Union(paths, Clipper2Lib::FillRule::EvenOdd, &ok);
    const bool differ = Differ(nonzero, evenOdd, 2 * tol, &ok);
    return ok && !differ;
}

bool ClipsOutside(const std::vector<Contour>& clip, const std::vector<Contour>& contours, double tol)
{
    if (!(tol > 0) || clip.empty()) return false;
    const Clipper2Lib::PathsD frame = Paths({clip[0]}, tol), clipPaths = Paths(clip, tol), fill = Paths(contours, tol);
    if (frame.empty()) return false;
    bool ok = true;
    Clipper2Lib::ClipperD difference(5);
    difference.AddSubject(frame);
    difference.AddClip(Union(fill, Clipper2Lib::FillRule::NonZero, &ok));
    Clipper2Lib::PathsD outside;
    ok = difference.Execute(Clipper2Lib::ClipType::Difference, Clipper2Lib::FillRule::NonZero, outside) && ok;
    // A resolved region's contours are rebuilt from the outline's segments, joined at crossings
    // within four flattening tolerances (NonzeroRegion), and flattened again here.
    const bool nonzero = Differ(Union(clipPaths, Clipper2Lib::FillRule::NonZero, &ok), outside, 6 * tol, &ok);
    const bool evenOdd = Differ(Union(clipPaths, Clipper2Lib::FillRule::EvenOdd, &ok), outside, 6 * tol, &ok);
    return ok && !nonzero && !evenOdd;
}

std::vector<Contour> StrokeClip(const std::vector<Contour>& stroked, double h, double margin,
                                const std::vector<std::vector<Pt>>& extra, const std::vector<Contour>& fill,
                                FillMode mode, double tol)
{
    std::vector<Contour> out;
    if (!(tol > 0) || !(h > 0) || !(margin >= 0)) return out;
    const double delta = h + margin, arc = std::max(tol, margin / 4);
    // Three decimal places: the band lies a margin past the stroke's reach, flattened within
    // tol; Clipper2's offset of thousands of small contours took 8 s at five, 1.4 s at two.
    const int precision = 3;
    bool ok = true;
    // The band is built from the fill's region, not from each contour: a stroke along a contour
    // inside the fill paints outside it only within its reach of the region's boundary, and
    // offsetting thousands of overlapping contours as lines took minutes on outlined Kerberos
    // Fang. Grown by the reach, the region holds the fill and every point within the reach
    // outside it; shrunk, it leaves the band inside.
    const Clipper2Lib::PathsD fillRegion = fill.empty() ? Clipper2Lib::PathsD()
                                                        : Union(Paths(fill, tol), Clipper2Lib::FillRule::NonZero, &ok, precision);
    // Mitered (limit 2), which holds every point within delta, as round joins would, and adds
    // two points per corner where round joins add an arc.
    Clipper2Lib::PathsD reach = fillRegion.empty() ? Clipper2Lib::PathsD()
        : Clipper2Lib::InflatePaths(fillRegion, mode == FillMode::Within ? -delta : delta, Clipper2Lib::JoinType::Miter,
                                    Clipper2Lib::EndType::Polygon, 2.0, precision, arc);
    // An open contour's stroke reaches past the fill around its own line, with round ends.
    Clipper2Lib::PathsD openLines;
    for (const Contour& c : stroked) {
        if (c.closed) continue;
        const Clipper2Lib::PathD line = Line(c, tol);
        if (line.size() >= 2) openLines.push_back(line);
    }
    Clipper2Lib::PathsD lineBand;
    if (!openLines.empty())
        lineBand = Clipper2Lib::InflatePaths(openLines, delta, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Round, 2.0, precision, arc);
    if (fillRegion.empty() && lineBand.empty()) return out;
    Clipper2Lib::PathsD joins;
    for (const std::vector<Pt>& poly : extra) {
        if (poly.size() < 3) continue;
        Clipper2Lib::PathD path;
        for (const Pt& p : poly) path.push_back(Clipper2Lib::PointD(p.x, p.y));
        joins.push_back(path);
    }
    joins = Union(joins, Clipper2Lib::FillRule::NonZero, &ok, precision);
    auto run = [&ok, precision](Clipper2Lib::ClipType op, const Clipper2Lib::PathsD& a, const Clipper2Lib::PathsD& b) {
        Clipper2Lib::ClipperD clipper(precision);
        clipper.AddSubject(a);
        if (!b.empty()) clipper.AddClip(b);
        Clipper2Lib::PathsD r;
        ok = clipper.Execute(op, Clipper2Lib::FillRule::NonZero, r) && ok;
        return r;
    };
    Clipper2Lib::PathsD region;
    if (mode == FillMode::Within) {
        // The fill less its core, with the joins that lie in it.
        region = run(Clipper2Lib::ClipType::Union, run(Clipper2Lib::ClipType::Difference, fillRegion, reach),
                     run(Clipper2Lib::ClipType::Intersection, joins, fillRegion));
    } else {
        region = run(Clipper2Lib::ClipType::Union, run(Clipper2Lib::ClipType::Union, reach, joins), lineBand);
        if (mode == FillMode::Remove && !fillRegion.empty()) region = run(Clipper2Lib::ClipType::Difference, region, fillRegion);
    }
    if (!ok) return out;
    return Polygons(region);
}

std::vector<Contour> NonzeroRegion(const std::vector<Contour>& contours, double tol)
{
    std::vector<Contour> out;
    if (!(tol > 0)) return out;
    const double eps = 1e-9;
    Outline o;
    o.tol = tol;
    Clipper2Lib::PathsD paths;
    for (const Contour& c : contours) {
        const std::vector<Segment> segs = ise_miter::Closed(c, eps);
        if (segs.empty()) continue;
        const int base = static_cast<int>(o.segs.size());
        Clipper2Lib::PathD path;
        Pt cur = c.start;
        for (size_t k = 0; k < segs.size(); ++k) {
            const int seg = base + static_cast<int>(k);
            o.from.push_back(cur);
            o.segs.push_back(segs[k]);
            o.previous.push_back(k == 0 ? base + static_cast<int>(segs.size()) - 1 : seg - 1);
            std::vector<Pt> pts{cur};
            std::vector<double> ts{0};
            if (segs[k].curve) Flatten(cur, segs[k].c1, segs[k].c2, segs[k].p, 0, 1, tol, 0, &pts, &ts);
            // The segment's end is the next one's start (t = 0), or the contour's.
            for (size_t j = 0; j + (segs[k].curve ? 1 : 0) < pts.size(); ++j) {
                Place p;
                p.seg = seg;
                p.t = ts[j];
                o.vertex.push_back(p);
                path.push_back(Clipper2Lib::PointD(pts[j].x, pts[j].y, static_cast<int64_t>(o.vertex.size())));
            }
            cur = segs[k].p;
        }
        paths.push_back(path);
    }
    if (paths.empty()) return out;
    Clipper2Lib::ClipperD clipper(5);
    clipper.SetZCallback([&o](const Clipper2Lib::PointD& e1bot, const Clipper2Lib::PointD& e1top,
                              const Clipper2Lib::PointD& e2bot, const Clipper2Lib::PointD& e2top, Clipper2Lib::PointD& pt) {
        o.crossing.push_back({OnEdge(o, e1bot, e1top, pt), OnEdge(o, e2bot, e2top, pt)});
        pt.z = static_cast<int64_t>(o.vertex.size() + o.crossing.size());
    });
    clipper.AddSubject(paths);
    Clipper2Lib::PathsD solution;
    if (!clipper.Execute(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::NonZero, solution)) return out;
    for (const Clipper2Lib::PathD& ring : solution) {
        if (ring.size() < 3) continue;
        Contour c = Rebuild(o, ring);
        if (!c.segments.empty()) out.push_back(c);
    }
    return out;
}

namespace {

Pt Unit(Pt d)
{
    const double l = ise_miter::Len(d);
    return l > 1e-300 ? d * (1 / l) : Pt{0, 0};
}

/** Schneider's fitting of cubic curves to digitized points ("An Algorithm for Automatically
    Fitting Digitized Curves", Graphics Gems, 1990): one curve by least squares on a chord-length
    parameterization, refined by Newton steps, split at the worst point while it errs by more
    than the tolerance. */
class Fitter {
public:
    Fitter(const std::vector<Pt>& p, double tol, Contour* out) : fP(p), fTol2(tol * tol), fOut(out) {}

    /** Fits fP[first..last], leaving the first along t1 and arriving at the last from t2 (unit
        tangents, t2 pointing back from the last point). */
    void Fit(size_t first, size_t last, Pt t1, Pt t2, int depth)
    {
        if (last - first == 1 || depth > 24) {
            for (size_t i = first + 1; i <= last; ++i) Line(fP[i]);
            return;
        }
        std::vector<double> u = ChordLength(first, last);
        Pt bez[4];
        Generate(first, last, u, t1, t2, bez);
        size_t split = 0;
        double err = MaxError(first, last, bez, u, &split);
        if (err < fTol2) { Curve(bez); return; }
        if (err < 16 * fTol2) {
            for (int k = 0; k < 4; ++k) {
                Reparameterize(first, last, bez, &u);
                Generate(first, last, u, t1, t2, bez);
                err = MaxError(first, last, bez, u, &split);
                if (err < fTol2) { Curve(bez); return; }
            }
        }
        if (split <= first) split = first + 1;
        if (split >= last) split = last - 1;
        const Pt center = Unit(fP[split - 1] - fP[split + 1]);
        Fit(first, split, t1, center, depth + 1);
        Fit(split, last, center * -1, t2, depth + 1);
    }

private:
    void Line(Pt q) { Segment s; s.p = q; fOut->segments.push_back(s); }
    void Curve(const Pt bez[4]) { Segment s; s.curve = true; s.c1 = bez[1]; s.c2 = bez[2]; s.p = bez[3]; fOut->segments.push_back(s); }

    std::vector<double> ChordLength(size_t first, size_t last) const
    {
        std::vector<double> u{0};
        for (size_t i = first + 1; i <= last; ++i) u.push_back(u.back() + ise_miter::Len(fP[i] - fP[i - 1]));
        const double total = u.back();
        for (double& v : u) v = total > 0 ? v / total : 0;
        return u;
    }

    static Pt Bezier(const Pt b[4], double t) { return CubicAt(b[0], b[1], b[2], b[3], t); }

    void Generate(size_t first, size_t last, const std::vector<double>& u, Pt t1, Pt t2, Pt bez[4]) const
    {
        const Pt p0 = fP[first], p3 = fP[last];
        double c00 = 0, c01 = 0, c11 = 0, x0 = 0, x1 = 0;
        for (size_t i = 0; i < u.size(); ++i) {
            const double t = u[i], s = 1 - t;
            const double b0 = s * s * s, b1 = 3 * t * s * s, b2 = 3 * t * t * s, b3 = t * t * t;
            const Pt a1 = t1 * b1, a2 = t2 * b2;
            c00 += ise_miter::Dot(a1, a1);
            c01 += ise_miter::Dot(a1, a2);
            c11 += ise_miter::Dot(a2, a2);
            const Pt tmp = fP[first + i] - (p0 * (b0 + b1) + p3 * (b2 + b3));
            x0 += ise_miter::Dot(a1, tmp);
            x1 += ise_miter::Dot(a2, tmp);
        }
        const double det = c00 * c11 - c01 * c01;
        double alpha1 = 0, alpha2 = 0;
        if (std::fabs(det) > 1e-12) {
            alpha1 = (x0 * c11 - x1 * c01) / det;
            alpha2 = (c00 * x1 - c01 * x0) / det;
        }
        const double chord = ise_miter::Len(p3 - p0), eps = 1e-6 * chord;
        if (alpha1 < eps || alpha2 < eps) alpha1 = alpha2 = chord / 3;   // Wu and Barsky's heuristic
        bez[0] = p0; bez[1] = p0 + t1 * alpha1; bez[2] = p3 + t2 * alpha2; bez[3] = p3;
    }

    double MaxError(size_t first, size_t last, const Pt bez[4], const std::vector<double>& u, size_t* split) const
    {
        double worst = 0;
        *split = (first + last) / 2;
        for (size_t i = first + 1; i < last; ++i) {
            const Pt d = Bezier(bez, u[i - first]) - fP[i];
            const double e = ise_miter::Dot(d, d);
            if (e >= worst) { worst = e; *split = i; }
        }
        // Between the points as well: a curve may bulge where the points lie far apart.
        for (size_t i = first; i < last; ++i) {
            const double t = (u[i - first] + u[i + 1 - first]) / 2;
            const Pt mid = (fP[i] + fP[i + 1]) * 0.5, seg = fP[i + 1] - fP[i];
            const double len = ise_miter::Len(seg);
            if (len <= 1e-12) continue;
            const Pt off = Bezier(bez, t) - mid;
            const double across = ise_miter::Cross(seg, off) / len;   // distance from the chord
            // The points lie within the tolerance of the curve they were taken from, and a chord
            // within it of the curve: twice the tolerance between them.
            const double e = across * across / 4;
            if (e > worst) { worst = e; *split = i + 1 < last ? i + 1 : i; }
        }
        return worst;
    }

    void Reparameterize(size_t first, size_t last, const Pt bez[4], std::vector<double>* u) const
    {
        for (size_t i = first; i <= last; ++i) {
            double& t = (*u)[i - first];
            const Pt q = Bezier(bez, t);
            const double s = 1 - t;
            const Pt d1 = (bez[1] - bez[0]) * (3 * s * s) + (bez[2] - bez[1]) * (6 * s * t) + (bez[3] - bez[2]) * (3 * t * t);
            const Pt d2 = (bez[2] - bez[1] * 2 + bez[0]) * (6 * s) + (bez[3] - bez[2] * 2 + bez[1]) * (6 * t);
            const Pt diff = q - fP[i];
            const double num = ise_miter::Dot(diff, d1), den = ise_miter::Dot(d1, d1) + ise_miter::Dot(diff, d2);
            if (std::fabs(den) > 1e-12) t = std::min(1.0, std::max(0.0, t - num / den));
        }
    }

    const std::vector<Pt>& fP;
    double fTol2;
    Contour* fOut;
};

Clipper2Lib::PathD Path(const std::vector<Pt>& poly)
{
    Clipper2Lib::PathD path;
    for (const Pt& p : poly) path.push_back(Clipper2Lib::PointD(p.x, p.y));
    return path;
}

std::vector<Pt> Points(const Clipper2Lib::PathD& path)
{
    std::vector<Pt> pts;
    for (const Clipper2Lib::PointD& p : path) pts.push_back({p.x, p.y});
    return pts;
}

Clipper2Lib::PathsD Run(Clipper2Lib::ClipType op, const Clipper2Lib::PathsD& a, const Clipper2Lib::PathsD& b, int precision,
                        bool* ok)
{
    Clipper2Lib::ClipperD clipper(precision);
    clipper.AddSubject(a);
    if (!b.empty()) clipper.AddClip(b);
    Clipper2Lib::PathsD r;
    *ok = clipper.Execute(op, Clipper2Lib::FillRule::NonZero, r) && *ok;
    return r;
}

/** Four decimal places: a ten-thousandth of a point, well under the fitting tolerance. */
const int kBandPrecision = 4;

std::vector<Contour> Fitted(const Clipper2Lib::PathsD& paths, double tol)
{
    std::vector<Contour> out;
    for (const Clipper2Lib::PathD& raw : paths) {
        // Simplified within the tolerance first: an offset's arcs, shrunk back to a corner,
        // leave clusters of tiny edges there, each turning too little to read as a corner,
        // which split the fit into curves half a point long.
        const Clipper2Lib::PathD path = Clipper2Lib::SimplifyPath(raw, std::min(tol, 0.005), true);
        if (path.size() < 3) continue;
        Contour c = FitCurves(Points(path), tol);
        if (!c.segments.empty()) out.push_back(c);
    }
    return out;
}

} // namespace

Contour FitCurves(const std::vector<Pt>& ring, double tol)
{
    Contour out;
    // Repeated points dropped, and long edges divided, so the fit is checked along them.
    std::vector<Pt> p;
    std::vector<size_t> corners;
    const double step = std::max(0.5, 100 * tol);
    for (size_t i = 0; i < ring.size(); ++i) {
        const Pt a = ring[i], b = ring[(i + 1) % ring.size()];
        if (!p.empty() && ise_miter::Len(a - p.back()) <= 1e-9) continue;
        p.push_back(a);
        const double len = ise_miter::Len(b - a);
        const int pieces = static_cast<int>(std::ceil(len / step));
        for (int k = 1; k < pieces; ++k) p.push_back(ise_miter::Lerp(a, b, static_cast<double>(k) / pieces));
    }
    while (p.size() > 1 && ise_miter::Len(p.back() - p.front()) <= 1e-9) p.pop_back();
    if (p.size() < 3) return out;
    // Corners: turning by more than 30 degrees within `reach` either side. An offset's arc
    // shrunk back to a corner leaves a cluster of tiny edges there, each turning too little to
    // read as a corner on its own; the cluster's sharpest point is kept as the corner, and the
    // rest of it dropped, so the fit does not split into short curves around it.
    const double reach = std::max(4 * tol, 0.05);
    const double cosCorner = std::cos(30 * 3.14159265358979323846 / 180);
    {
        const size_t m = p.size();
        std::vector<double> turn(m, 1);   // the cosine of the turn at each point
        for (size_t i = 0; i < m; ++i) {
            size_t j = (i + m - 1) % m, k = (i + 1) % m;
            for (size_t s = 0; s + 2 < m && ise_miter::Len(p[i] - p[j]) < reach; ++s) j = (j + m - 1) % m;
            for (size_t s = 0; s + 2 < m && ise_miter::Len(p[k] - p[i]) < reach; ++s) k = (k + 1) % m;
            turn[i] = ise_miter::Dot(Unit(p[i] - p[j]), Unit(p[k] - p[i]));
        }
        std::vector<char> corner(m, 0), drop(m, 0);
        for (size_t i = 0; i < m; ++i) {
            if (!(turn[i] < cosCorner)) continue;
            // The sharpest point within reach either side.
            bool best = true;
            for (size_t s = 1; s < m && best; ++s) {
                const size_t a = (i + s) % m, b = (i + m - s) % m;
                const bool nearA = ise_miter::Len(p[a] - p[i]) < reach, nearB = ise_miter::Len(p[b] - p[i]) < reach;
                if (!nearA && !nearB) break;
                if ((nearA && (turn[a] < turn[i] || (turn[a] == turn[i] && a < i))) ||
                    (nearB && (turn[b] < turn[i] || (turn[b] == turn[i] && b < i)))) best = false;
            }
            if (!best) continue;
            corner[i] = 1;
            for (size_t s = 1; s < m; ++s) {
                const size_t a = (i + s) % m;
                if (ise_miter::Len(p[a] - p[i]) >= reach) break;
                drop[a] = 1;
            }
            for (size_t s = 1; s < m; ++s) {
                const size_t b = (i + m - s) % m;
                if (ise_miter::Len(p[b] - p[i]) >= reach) break;
                drop[b] = 1;
            }
        }
        std::vector<Pt> kept;
        std::vector<char> keptCorner;
        for (size_t i = 0; i < m; ++i) {
            if (drop[i] && !corner[i]) continue;
            kept.push_back(p[i]);
            keptCorner.push_back(corner[i]);
        }
        p.swap(kept);
        std::vector<size_t> found;
        for (size_t i = 0; i < keptCorner.size(); ++i)
            if (keptCorner[i]) found.push_back(i);
        corners.swap(found);
    }
    const size_t n = p.size();
    if (n < 3) return out;
    std::vector<Pt> seq;
    if (corners.empty()) {
        // A smooth loop: from the first point around to it again, with one tangent there.
        seq.assign(p.begin(), p.end());
        seq.push_back(p[0]);
        out.start = p[0];
        const Pt t = Unit(p[1] - p[n - 1]);
        Fitter(seq, tol, &out).Fit(0, seq.size() - 1, t, t * -1, 0);
        out.closed = true;
        return out;
    }
    out.start = p[corners[0]];
    for (size_t k = 0; k < corners.size(); ++k) {
        const size_t from = corners[k], to = corners[(k + 1) % corners.size()];
        seq.clear();
        for (size_t i = from;; i = (i + 1) % n) {
            seq.push_back(p[i]);
            if (seq.size() > 1 && i == to) break;
        }
        const Pt t1 = Unit(seq[1] - seq[0]), t2 = Unit(seq[seq.size() - 2] - seq.back());
        Fitter(seq, tol, &out).Fit(0, seq.size() - 1, t1, t2, 0);
    }
    out.closed = true;
    return out;
}

std::vector<std::vector<Contour>> DistanceBands(const std::vector<Contour>& fill, const std::vector<std::pair<double, double>>& bands,
                                                BandJoin join, const std::vector<BandCorner>& corners, double inset, double tol, bool* ok,
                                                double limit)
{
    std::vector<std::vector<Contour>> out(bands.size());
    *ok = tol > 0;
    if (!*ok) return out;
    const int precision = kBandPrecision;
    const Clipper2Lib::PathsD region = Union(Paths(fill, tol), Clipper2Lib::FillRule::NonZero, ok, precision);
    if (!*ok || region.empty()) { *ok = false; return out; }
    std::map<double, Clipper2Lib::PathsD> offsets;
    // The fill grown by d (shrunk by the inset for d = 0), with the join at its corners; for d < 0,
    // shrunk by -d, its inner corners mitered within the limit.
    auto offset = [&](double d) -> const Clipper2Lib::PathsD& {
        auto it = offsets.find(d);
        if (it != offsets.end()) return it->second;
        Clipper2Lib::PathsD r;
        if (d == 0) {
            r = inset > 0 ? Clipper2Lib::InflatePaths(region, -inset, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon,
                                                      2.0, precision, tol)
                          : region;
        } else if (d < 0) {
            const Clipper2Lib::JoinType jt = join == BandJoin::Round   ? Clipper2Lib::JoinType::Round
                                             : join == BandJoin::Bevel ? Clipper2Lib::JoinType::Bevel
                                                                       : Clipper2Lib::JoinType::Miter;
            r = Clipper2Lib::InflatePaths(region, d, jt, Clipper2Lib::EndType::Polygon, std::max(limit, 1.0), precision, tol);
        } else {
            const Clipper2Lib::JoinType jt = join == BandJoin::Round   ? Clipper2Lib::JoinType::Round
                                             : join == BandJoin::Bevel ? Clipper2Lib::JoinType::Bevel
                                                                       : Clipper2Lib::JoinType::Square;
            r = Clipper2Lib::InflatePaths(region, d, jt, Clipper2Lib::EndType::Polygon, 2.0, precision, tol);
            if (join == BandJoin::Miter && !corners.empty()) {
                // Each miter wound as the offset's outer contours are (positive area), so the
                // nonzero union adds it instead of cancelling.
                Clipper2Lib::PathsD miters;
                for (const BandCorner& c : corners) {
                    const double reach = std::min(c.stub.ratio * d, c.cap * d + c.extra);
                    miters.push_back(Path(ise_miter::Counterclockwise(ise_miter::CutMiter(c.stub, d, reach))));
                }
                r = Run(Clipper2Lib::ClipType::Union, r, miters, precision, ok);
            }
        }
        return offsets.emplace(d, r).first->second;
    };
    for (size_t i = 0; i < bands.size() && *ok; ++i) {
        const double from = bands[i].first, to = bands[i].second;
        if (!(to > from)) continue;
        out[i] = Fitted(Run(Clipper2Lib::ClipType::Difference, offset(to), offset(from), precision, ok), tol);
    }
    if (!*ok) out.assign(bands.size(), {});
    return out;
}

std::vector<Contour> UncoveredWithin(const std::vector<Contour>& fill, double half, double grow, double tol, bool* ok)
{
    *ok = tol > 0 && half > 0;
    if (!*ok) return {};
    const int precision = kBandPrecision;
    const Clipper2Lib::PathsD region = Union(Paths(fill, tol), Clipper2Lib::FillRule::NonZero, ok, precision);
    if (!*ok || region.empty()) { *ok = false; return {}; }
    // The fill less its opening by a disk of radius half: shrunk by half (the path's own
    // shrinking), then grown back with miter joins, which restores every part at least as thick
    // as the stroke, its sharp corners included (a round band along the path left every corner).
    const Clipper2Lib::PathsD core = Clipper2Lib::InflatePaths(region, -half, Clipper2Lib::JoinType::Round,
                                                               Clipper2Lib::EndType::Polygon, 2.0, precision, tol);
    const Clipper2Lib::PathsD opening = core.empty() ? core
        : Clipper2Lib::InflatePaths(core, half, Clipper2Lib::JoinType::Miter, Clipper2Lib::EndType::Polygon, 100.0, precision, tol);
    const Clipper2Lib::PathsD left = Run(Clipper2Lib::ClipType::Difference, region, opening, precision, ok);
    if (!*ok || left.empty()) return {};
    // Opened by a few tolerances, which drops the slivers two flattenings leave, then grown
    // under the band beside it and kept within the fill.
    const double open = 3 * tol;
    const Clipper2Lib::PathsD opened = Clipper2Lib::InflatePaths(left, -open, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon,
                                                                 2.0, precision, tol);
    if (opened.empty()) return {};
    const Clipper2Lib::PathsD grown = Clipper2Lib::InflatePaths(opened, open + grow, Clipper2Lib::JoinType::Round,
                                                                Clipper2Lib::EndType::Polygon, 2.0, precision, tol);
    return Fitted(Run(Clipper2Lib::ClipType::Intersection, grown, region, precision, ok), tol);
}

bool ContoursInteract(const std::vector<Contour>& contours, double reach, double tol)
{
    if (contours.size() < 2) return false;
    if (contours.size() > 400) return true;
    const int precision = 3;
    double sum = 0;
    Clipper2Lib::PathsD all;
    for (const Contour& c : contours) {
        const Clipper2Lib::PathsD line = Paths({c}, tol);
        if (line.empty()) continue;
        const Clipper2Lib::PathsD band = Clipper2Lib::InflatePaths(line, reach, Clipper2Lib::JoinType::Round,
                                                                   Clipper2Lib::EndType::Joined, 2.0, precision, tol);
        sum += std::fabs(Clipper2Lib::Area(band));
        all.insert(all.end(), band.begin(), band.end());
    }
    bool ok = true;
    const double united = std::fabs(Clipper2Lib::Area(Union(all, Clipper2Lib::FillRule::NonZero, &ok, precision)));
    return !ok || sum - united > 0.01 * reach * reach;
}

std::vector<Contour> WordPath(const std::vector<Contour>& fill, double half, BandJoin join, double limit, bool creases, double tol,
                              bool* ok)
{
    *ok = tol > 0;
    if (!*ok) return {};
    const int precision = kBandPrecision;
    const Clipper2Lib::PathsD region = Union(Paths(fill, tol), Clipper2Lib::FillRule::NonZero, ok, precision);
    if (!*ok || region.empty()) { *ok = false; return {}; }
    if (half == 0) return Fitted(region, tol);   // a Center stroke: the united outline itself
    if (half < 0) creases = false;              // an Inside stroke: the region shrunk, its corners kept
    const Clipper2Lib::JoinType jt = join == BandJoin::Round   ? Clipper2Lib::JoinType::Round
                                     : join == BandJoin::Bevel ? Clipper2Lib::JoinType::Bevel
                                                               : Clipper2Lib::JoinType::Miter;
    const Clipper2Lib::PathsD grown = Clipper2Lib::InflatePaths(region, half, jt, Clipper2Lib::EndType::Polygon,
                                                                std::max(limit, 1.0), precision, tol);
    if (!creases) {
        if (grown.empty()) { *ok = false; return {}; }
        return Fitted(grown, tol);
    }
    // Closed by a disk of radius half: grown by it, then shrunk back with round joins, which
    // round the creases. Grown with miter joins, the convex corners come back exact; grown
    // round, they came back chamfered by the arcs' flattening, which split the fit there.
    const Clipper2Lib::PathsD wide = Clipper2Lib::InflatePaths(grown, half, Clipper2Lib::JoinType::Miter,
                                                               Clipper2Lib::EndType::Polygon, 100.0, precision, tol);
    Clipper2Lib::PathsD closed = Clipper2Lib::InflatePaths(wide, -half, Clipper2Lib::JoinType::Round,
                                                           Clipper2Lib::EndType::Polygon, 2.0, precision, tol);
    if (closed.empty()) { *ok = false; return {}; }
    // A counter's offset narrower than the disk is filled by the closing: it is cut back out as
    // grown, so the pattern runs round every counter that has room for its path, as InDesign's
    // runs round each counter (the owner's check, 2026-10-07: the counters of script letters
    // were left empty). Holes are the paths Clipper2 winds negative. Not a pocket that comes
    // within the disk of the outside (a script terminal curled against its stroke): cut out,
    // it pinched the path into a loop.
    Clipper2Lib::PathsD filled;
    for (const Clipper2Lib::PathD& p : grown) {
        const double area = Clipper2Lib::Area(p);
        if (!(area < 0)) continue;
        const double left = std::fabs(Clipper2Lib::Area(Run(Clipper2Lib::ClipType::Difference, {p}, closed, precision, ok)));
        if (left > 0.01 * -area) continue;
        const Clipper2Lib::PathsD reach = Clipper2Lib::InflatePaths({p}, half, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon,
                                                                    2.0, precision, tol);
        const Clipper2Lib::PathsD outside = Run(Clipper2Lib::ClipType::Difference, Run(Clipper2Lib::ClipType::Difference, reach, grown, precision, ok),
                                               {p}, precision, ok);
        if (std::fabs(Clipper2Lib::Area(outside)) <= 0.01 * -area)
            filled.push_back(p);
    }
    if (!filled.empty()) closed = Run(Clipper2Lib::ClipType::Difference, closed, filled, precision, ok);
    if (!*ok || closed.empty()) { *ok = false; return {}; }
    return Fitted(closed, tol);
}

std::vector<Contour> Uncovered(const std::vector<Contour>& fill, const std::vector<Contour>& path, double half, double grow,
                               double tol, bool* ok)
{
    *ok = tol > 0 && half > 0;
    if (!*ok) return {};
    const int precision = kBandPrecision;
    const Clipper2Lib::PathsD lines = Paths(path, tol);
    const Clipper2Lib::PathsD enclosed = Union(lines, Clipper2Lib::FillRule::EvenOdd, ok, precision);
    const Clipper2Lib::PathsD region = Union(Paths(fill, tol), Clipper2Lib::FillRule::NonZero, ok, precision);
    const Clipper2Lib::PathsD band = Clipper2Lib::InflatePaths(lines, half, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Joined,
                                                               2.0, precision, tol);
    const Clipper2Lib::PathsD left = Run(Clipper2Lib::ClipType::Difference, Run(Clipper2Lib::ClipType::Difference, enclosed, region,
                                                                                precision, ok),
                                         band, precision, ok);
    if (!*ok || left.empty()) return {};
    // Opened by a few tolerances, which drops the slivers between the band and the fill that
    // two flattenings leave, then grown.
    const double open = 3 * tol;
    const Clipper2Lib::PathsD core = Clipper2Lib::InflatePaths(left, -open, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon,
                                                               2.0, precision, tol);
    if (core.empty()) return {};
    return Fitted(Clipper2Lib::InflatePaths(core, open + grow, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon, 2.0,
                                            precision, tol),
                  tol);
}


std::vector<Contour> Silhouette(const std::vector<Contour>& fill, double grow, double tol, bool* ok, BandJoin join, double limit,
                                bool holesOnly, bool hollow)
{
    *ok = tol > 0 && grow >= 0;
    if (!*ok) return {};
    const int precision = kBandPrecision;
    const Clipper2Lib::PathsD region = Union(Paths(fill, tol), Clipper2Lib::FillRule::NonZero, ok, precision);
    if (!*ok || region.empty()) return {};
    auto tree = [&](const Clipper2Lib::PathsD& paths, Clipper2Lib::PolyTreeD& out) {
        Clipper2Lib::ClipperD clipper(precision);
        clipper.AddSubject(paths);
        *ok = clipper.Execute(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::NonZero, out) && *ok;
    };
    const Clipper2Lib::PathsD grown = grow > 0 ? Clipper2Lib::InflatePaths(region, grow, Clipper2Lib::JoinType::Bevel,
                                                                           Clipper2Lib::EndType::Polygon, 2.0, precision, tol)
                                               : region;
    Clipper2Lib::PolyTreeD shape;
    tree(grown, shape);
    if (!*ok) return {};
    // The outer polygons only: their holes, and whatever lies within those, are dropped.
    Clipper2Lib::PathsD outer;
    for (const auto& child : shape) outer.push_back(child->Polygon());
    // The holes the stroke's own join encloses, filled too: a gap that mitered corners close off
    // is a hole, where the bevel left it open (the owner's retest Z3, 2026-11-01: the space
    // between two words, closed by the lines above and below, stayed open). A Miter join is the
    // bevel with each corner's miter added within the limit; past it the corner stays beveled, as
    // the stroke draws it (Clipper2's own miter squares it off a whole reach out). The holes are
    // found a twentieth of the reach past `grow` (which callers keep short of the stroke's edge):
    // strokes that nearly touch enclose a hole as the eye sees it (the owner's retest notes,
    // 2026-11-01: gaps between Minion's 7 pt serif strokes, open by slits of 0.1 to 0.3 pt, stayed
    // white: 74 pt^2 in one story). The slits are filled with the holes (grown back below).
    if (grow > 0) {
        const double wide = grow + std::max(3 * tol, 0.05 * grow);
        // Keep holes at the real reach too: widening can erase small cavities.
        auto holesAt = [&](double distance, double overlap) {
            Clipper2Lib::PathsD joined;
            if (join != BandJoin::Miter) {
                joined = Clipper2Lib::InflatePaths(region, distance,
                                                   join == BandJoin::Round ? Clipper2Lib::JoinType::Round : Clipper2Lib::JoinType::Bevel,
                                                   Clipper2Lib::EndType::Polygon, 2.0, precision, tol);
            } else {
                joined = Clipper2Lib::InflatePaths(region, distance, Clipper2Lib::JoinType::Bevel, Clipper2Lib::EndType::Polygon, 2.0,
                                                   precision, tol);
                // The side the region lies on, from its largest polygon's winding (an outer one).
                double largest = 0;
                for (const auto& p : region) {
                    const double a = Clipper2Lib::Area(p);
                    if (std::fabs(a) > std::fabs(largest)) largest = a;
                }
                const double side = largest < 0 ? -1 : 1;
                for (const auto& p : region) {
                    const size_t n = p.size();
                    if (n < 3) continue;
                    for (size_t i = 0; i < n; ++i) {
                        const Clipper2Lib::PointD& a = p[(i + n - 1) % n];
                        const Clipper2Lib::PointD& b = p[i];
                        const Clipper2Lib::PointD& c = p[(i + 1) % n];
                        const double l1 = std::hypot(b.x - a.x, b.y - a.y), l2 = std::hypot(c.x - b.x, c.y - b.y);
                        if (!(l1 > 0) || !(l2 > 0)) continue;
                        const double d1x = (b.x - a.x) / l1, d1y = (b.y - a.y) / l1, d2x = (c.x - b.x) / l2, d2y = (c.y - b.y) / l2;
                        if (side * (d1x * d2y - d1y * d2x) <= 0) continue;   // not a corner the stroke goes around
                        // Outward normals, the miter's tip where the offset edges meet, and its length
                        // in reaches (the miter limit's measure).
                        const double n1x = side * d1y, n1y = -side * d1x, n2x = side * d2y, n2y = -side * d2x;
                        const double cosine = n1x * n2x + n1y * n2y;
                        if (!(cosine > -1 + 1e-9)) continue;
                        const double ratio = std::sqrt(2 / (1 + cosine));
                        // Within the limit, and longer than the bevel by more than the tolerance.
                        if (ratio > limit || distance * (ratio - 1 / ratio) <= tol) continue;
                        const double k = distance / (1 + cosine);
                        joined.push_back({b, {b.x + distance * n1x, b.y + distance * n1y}, {b.x + k * (n1x + n2x), b.y + k * (n1y + n2y)},
                                          {b.x + distance * n2x, b.y + distance * n2y}});
                        if (Clipper2Lib::Area(joined.back()) * side < 0) std::reverse(joined.back().begin(), joined.back().end());
                    }
                }
            }
            Clipper2Lib::PolyTreeD enclosed;
            tree(joined, enclosed);
            if (!*ok) return Clipper2Lib::PathsD{};
            Clipper2Lib::PathsD found;
            for (const auto& child : enclosed) {
                for (const auto& hole : *child) {
                    Clipper2Lib::PathD p = hole->Polygon();
                    if (Clipper2Lib::Area(p) < 0) std::reverse(p.begin(), p.end());
                    found.push_back(std::move(p));
                }
            }
            if (!found.empty()) {
                // Grown back under the stroke, so no hairline shows between the stroke and the fill
                // (holes alone too: a hairline showed around a hole filled between two stories).
                found = Clipper2Lib::InflatePaths(found, overlap, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon,
                                                  2.0, precision, tol);
            }
            return found;
        };
        Clipper2Lib::PathsD found = holesAt(grow, tol);
        if (!*ok) return {};
        const Clipper2Lib::PathsD nearTouching = holesAt(wide, wide - grow + tol);
        if (!*ok) return {};
        found.insert(found.end(), nearTouching.begin(), nearTouching.end());
        found = Union(found, Clipper2Lib::FillRule::NonZero, ok, precision);
        if (!*ok) return {};
        if (!found.empty()) {
            if (holesOnly) return Fitted(found, tol);
            outer.insert(outer.end(), found.begin(), found.end());
            for (Clipper2Lib::PathD& p : outer) if (Clipper2Lib::Area(p) < 0) std::reverse(p.begin(), p.end());
            Clipper2Lib::PolyTreeD merged;
            tree(outer, merged);
            if (!*ok) return {};
            outer.clear();
            for (const auto& child : merged) outer.push_back(child->Polygon());
        }
    }
    if (holesOnly) return {};   // no hole: nothing to fill (never the band)
    if (hollow) {
        outer = Run(Clipper2Lib::ClipType::Difference, outer, region, precision, ok);
        if (!*ok) return {};
    }
    return Fitted(outer, tol);
}

std::vector<Contour> TippedOutline(const std::vector<Contour>& fill, double h, double limit, double tol, bool* ok)
{
    *ok = tol > 0 && h > 0;
    if (!*ok) return {};
    const int precision = kBandPrecision;
    const Clipper2Lib::PathsD region = Union(Paths(fill, tol), Clipper2Lib::FillRule::NonZero, ok, precision);
    if (!*ok || region.empty()) return {};
    Clipper2Lib::PathsD all = Clipper2Lib::InflatePaths(region, h, Clipper2Lib::JoinType::Bevel, Clipper2Lib::EndType::Polygon, 2.0,
                                                       precision, tol);
    // The corners on the filled region's own outline, rebuilt from the outline's segments, so
    // that a tip beside a curve is told apart as the route tells it (overlapping contours resolved).
    const std::vector<Contour> outline = NonzeroRegion(fill, tol);
    if (outline.empty()) { *ok = false; return {}; }
    ise_miter::Options options;
    options.halfWidth = h;
    for (const ise_miter::Stub& s : ise_miter::Corners(outline, options, false)) {
        if (s.interior) continue;
        double reach = s.ratio * h;
        const bool tip = ise_miter::TrimmedTip(s, h);
        if (s.ratio > limit) {
            if (!tip) continue;   // beveled, as the stroke draws it
            reach = h;
        } else if (ise_miter::NearTip(s, h)) {
            reach = std::min(reach, ise_miter::TipReach(s, h));
        }
        Clipper2Lib::PathD poly;
        for (const Pt& q : ise_miter::JoinPolygon(s, h, reach)) poly.push_back(Clipper2Lib::PointD(q.x, q.y));
        if (Clipper2Lib::Area(poly) < 0) std::reverse(poly.begin(), poly.end());
        all.push_back(std::move(poly));
    }
    Clipper2Lib::PathsD united = Union(all, Clipper2Lib::FillRule::NonZero, ok, precision);
    if (!*ok) return {};
    united = Run(Clipper2Lib::ClipType::Difference, united, region, precision, ok);
    if (!*ok) return {};
    return Fitted(united, tol);
}
} // namespace ise_region
