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

} // namespace ise_region
