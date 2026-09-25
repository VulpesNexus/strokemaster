"""Curved outlines to topology-core requests: flattening and ring roles, without Adobe.

The reviewed topology core (tools/topology_region.cpp) accepts straight-edged polygons
with explicit shell and hole roles. Glyph outlines are cubic Bezier contours whose roles
come from a fill rule. This module is the adapter between the two:

- flatten_cubic subdivides a cubic Bezier (de Casteljau, at t = 1/2) until both control
  points lie within `tolerance` of the chord. By the convex-hull property the curve then
  lies within `tolerance` of the polyline, and every polyline vertex lies on the curve.
- flatten_contour turns one closed contour of line and cubic segments into a ring.
- classify assigns roles to non-crossing rings: containment depth 0, 2, ... is a shell,
  1, 3, ... a hole of its nearest enclosing ring. That matches both the even-odd and the
  nonzero fill rules for well-formed outlines (no crossing contours, each hole wound
  against its shell); crossing or touching contours are refused, never guessed.
- request builds the core's JSON request.

Bounds are stated, not assumed; tests/geometry/test_curve_region.py measures them.
"""
import math

MAX_DEPTH = 24          # 2**24 pieces per curve at most
MAX_RING_POINTS = 20000  # the core's per-polygon-list budget


class Refused(ValueError):
    """The outline cannot be converted within this module's contract."""


def _flat_enough(p0, p1, p2, p3, tolerance):
    dx, dy = p3[0] - p0[0], p3[1] - p0[1]
    length = math.hypot(dx, dy)
    if length == 0:
        return max(math.dist(p0, p1), math.dist(p0, p2)) <= tolerance
    for c in (p1, p2):
        if abs((c[0] - p0[0]) * dy - (c[1] - p0[1]) * dx) / length > tolerance:
            return False
        # Control points beyond the chord's ends are not covered by the distance to its line.
        t = ((c[0] - p0[0]) * dx + (c[1] - p0[1]) * dy) / (length * length)
        if t < -tolerance / length or t > 1 + tolerance / length:
            return False
    return True


def flatten_cubic(p0, p1, p2, p3, tolerance):
    """Points after p0, ending at p3, all on the curve, within tolerance of it."""
    if not (tolerance > 0 and math.isfinite(tolerance)):
        raise Refused('tolerance must be positive and finite')
    out = []

    def split(a, b, c, d, depth):
        if depth >= MAX_DEPTH or _flat_enough(a, b, c, d, tolerance):
            if depth >= MAX_DEPTH and not _flat_enough(a, b, c, d, tolerance):
                raise Refused('curve does not flatten within the subdivision budget')
            out.append(d)
            return
        ab = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
        bc = ((b[0] + c[0]) / 2, (b[1] + c[1]) / 2)
        cd = ((c[0] + d[0]) / 2, (c[1] + d[1]) / 2)
        abc = ((ab[0] + bc[0]) / 2, (ab[1] + bc[1]) / 2)
        bcd = ((bc[0] + cd[0]) / 2, (bc[1] + cd[1]) / 2)
        mid = ((abc[0] + bcd[0]) / 2, (abc[1] + bcd[1]) / 2)
        split(a, ab, abc, mid, depth + 1)
        split(mid, bcd, cd, d, depth + 1)

    split(tuple(p0), tuple(p1), tuple(p2), tuple(p3), 0)
    return out


def flatten_contour(start, segments, tolerance):
    """start: (x, y); segments: ('L', p) or ('C', c1, c2, p). Returns a closed ring."""
    ring = [tuple(start)]
    for segment in segments:
        if segment[0] == 'L':
            ring.append(tuple(segment[1]))
        elif segment[0] == 'C':
            ring.extend(flatten_cubic(ring[-1], segment[1], segment[2], segment[3], tolerance))
        else:
            raise Refused('unknown segment type ' + repr(segment[0]))
        if len(ring) > MAX_RING_POINTS:
            raise Refused('ring exceeds the core vertex budget')
    if math.dist(ring[0], ring[-1]) > 0:
        ring.append(ring[0])
    # Drop repeated points; the core refuses degenerate edges.
    cleaned = [ring[0]]
    for p in ring[1:]:
        if p != cleaned[-1]:
            cleaned.append(p)
    if len(cleaned) < 4:
        raise Refused('contour has fewer than three distinct points')
    if not all(math.isfinite(v) for p in cleaned for v in p):
        raise Refused('nonfinite coordinate')
    return cleaned


def signed_area(ring):
    ox, oy = ring[0]
    return math.fsum((a[0] - ox) * (b[1] - oy) - (b[0] - ox) * (a[1] - oy) for a, b in zip(ring, ring[1:])) / 2


def _segments_meet(a, b, c, d):
    def orient(p, q, r):
        v = (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])
        return 0 if v == 0 else (1 if v > 0 else -1)

    def on(p, q, r):
        return min(p[0], q[0]) <= r[0] <= max(p[0], q[0]) and min(p[1], q[1]) <= r[1] <= max(p[1], q[1])

    o1, o2, o3, o4 = orient(a, b, c), orient(a, b, d), orient(c, d, a), orient(c, d, b)
    if o1 != o2 and o3 != o4:
        return True
    return (o1 == 0 and on(a, b, c)) or (o2 == 0 and on(a, b, d)) or (o3 == 0 and on(c, d, a)) or (o4 == 0 and on(c, d, b))


def _box(ring):
    xs, ys = [p[0] for p in ring], [p[1] for p in ring]
    return min(xs), min(ys), max(xs), max(ys)


def rings_cross(r, s):
    rb, sb = _box(r), _box(s)
    if rb[2] < sb[0] or sb[2] < rb[0] or rb[3] < sb[1] or sb[3] < rb[1]:
        return False
    for a, b in zip(r, r[1:]):
        lo_x, hi_x = min(a[0], b[0]), max(a[0], b[0])
        lo_y, hi_y = min(a[1], b[1]), max(a[1], b[1])
        if hi_x < sb[0] or sb[2] < lo_x or hi_y < sb[1] or sb[3] < lo_y:
            continue
        for c, d in zip(s, s[1:]):
            if _segments_meet(a, b, c, d):
                return True
    return False


def point_in_ring(point, ring):
    x, y = point
    inside = False
    for (ax, ay), (bx, by) in zip(ring, ring[1:]):
        if (ay > y) != (by > y) and x < ax + (y - ay) * (bx - ax) / (by - ay):
            inside = not inside
    return inside


def classify(rings):
    """[(shell, [holes])] for non-crossing rings; refuses crossing or touching rings."""
    for i in range(len(rings)):
        for j in range(i + 1, len(rings)):
            if rings_cross(rings[i], rings[j]):
                raise Refused('contours cross or touch; fill-rule resolution is not supported')
    parents = []
    for i, ring in enumerate(rings):
        containers = [j for j, other in enumerate(rings) if j != i and point_in_ring(ring[0], other)]
        parents.append(containers)
    depth = [len(c) for c in parents]
    polygons = {}
    for i in range(len(rings)):
        if depth[i] % 2 == 0:
            polygons[i] = (rings[i], [])
    for i in range(len(rings)):
        if depth[i] % 2 == 1:
            # The nearest enclosing ring is the container with the greatest depth.
            parent = max(parents[i], key=lambda j: depth[j])
            if depth[parent] != depth[i] - 1 or parent not in polygons:
                raise Refused('hole without an enclosing shell')
            polygons[parent][1].append(rings[i])
    return [polygons[i] for i in sorted(polygons)]


def wkt(polygon):
    shell, holes = polygon
    return 'POLYGON(' + ','.join('(' + ','.join(f'{x:.17g} {y:.17g}' for x, y in ring) + ')'
                                 for ring in (shell, *holes)) + ')'


def request(polygons, h, limit=4, margin=None):
    xs = [p[0] for shell, _ in polygons for p in shell]
    ys = [p[1] for shell, _ in polygons for p in shell]
    m = margin if margin is not None else 2 * h * max(limit, 1) + 1
    x0, y0, x1, y1 = min(xs) - m, min(ys) - m, max(xs) + m, max(ys) + m
    envelope = f'POLYGON(({x0} {y0},{x1} {y0},{x1} {y1},{x0} {y1},{x0} {y0}))'
    return {'source': [wkt(p) for p in polygons], 'envelope': [envelope], 'h': h, 'limit': limit}
