"""Curved outlines through the reviewed topology core, against analytic bounds.

tools/curve_region.py flattens cubic Bezier contours and assigns ring roles; the core
(topology_region.exe, built by tools/build_topology.py) offsets the result. The oracle
here is independent of both: exact Bezier evaluation, circle and lens formulas, and the
bound that follows from flattening. If the polyline P lies within t of the curve C
(Hausdorff) and the core returns the miter offset of P at distance h, every result
vertex v satisfies h - t <= d(v, C) <= h / cos(a / 2) + t, where a is P's largest
turning angle at a vertex that is not clipped. No font data is used.
"""
import math
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import curve_region as cr  # noqa: E402
from test_topology_region import batch, signed_area  # noqa: E402

K = 4 / 3 * (math.sqrt(2) - 1)  # quarter-circle cubic control distance


def bezier(p0, p1, p2, p3, t):
    u = 1 - t
    return (u**3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t**3 * p3[0],
            u**3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t**3 * p3[1])


def circle(cx, cy, r, clockwise=False):
    """Four cubic quarter arcs; returns (start, segments, curves)."""
    pts = [(cx + r, cy), (cx, cy + r), (cx - r, cy), (cx, cy - r)]
    tangents = [(0, 1), (-1, 0), (0, -1), (1, 0)]
    order = [0, 3, 2, 1] if clockwise else [0, 1, 2, 3]
    segments, curves = [], []
    for n, i in enumerate(order):
        j = order[(n + 1) % 4]
        sign = -1 if clockwise else 1
        a, b = pts[i], pts[j]
        ta, tb = tangents[i], tangents[j]
        c1 = (a[0] + sign * K * r * ta[0], a[1] + sign * K * r * ta[1])
        c2 = (b[0] - sign * K * r * tb[0], b[1] - sign * K * r * tb[1])
        segments.append(('C', c1, c2, b))
        curves.append((a, c1, c2, b))
    return pts[order[0]], segments, curves


def lens(half_width, angle_degrees):
    """Two circular arcs (one cubic each) from (-L, 0) to (L, 0) meeting at tips whose
    interior angle is angle_degrees. Returns (start, segments, curves)."""
    phi = math.radians(angle_degrees)
    rho = half_width / math.sin(phi / 2)
    k = 4 / 3 * math.tan(phi / 4) * rho
    t = (math.cos(phi / 2), math.sin(phi / 2))  # tangent at the left tip, upper arc
    a, b = (-half_width, 0.0), (half_width, 0.0)
    upper = (a, (a[0] + k * t[0], a[1] + k * t[1]), (b[0] - k * t[0], b[1] + k * t[1]), b)
    lower = (b, (b[0] - k * t[0], b[1] - k * t[1]), (a[0] + k * t[0], a[1] - k * t[1]), a)
    return a, [('C', upper[1], upper[2], upper[3]), ('C', lower[1], lower[2], lower[3])], [upper, lower]


def samples(curves, n=4000):
    return [bezier(*c, i / n) for c in curves for i in range(n + 1)]


def distance_to_samples(point, pts):
    return min(math.dist(point, p) for p in pts)


def max_turn(ring):
    worst = 0.0
    pts = ring[:-1]
    for i in range(len(pts)):
        a, b, c = pts[i - 1], pts[i], pts[(i + 1) % len(pts)]
        u = (b[0] - a[0], b[1] - a[1])
        v = (c[0] - b[0], c[1] - b[1])
        cos = (u[0] * v[0] + u[1] * v[1]) / (math.hypot(*u) * math.hypot(*v))
        worst = max(worst, math.acos(max(-1.0, min(1.0, cos))))
    return worst


class Flattening(unittest.TestCase):
    def test_vertices_on_curve_and_curve_within_tolerance(self):
        start, segments, curves = circle(0, 0, 10)
        for tol in (1e-2, 1e-3, 1e-4):
            ring = cr.flatten_contour(start, segments, tol)
            # Every polyline vertex lies on the curve: its distance to a 4,000-chord
            # polyline of each quarter arc is below that polyline's own sagitta, s^2 / (8 r).
            dense = samples(curves, 4000)
            for p in ring:
                d = min(_segment_distance(p, a, b) for a, b in zip(dense, dense[1:]))
                self.assertLess(d, (math.pi * 10 / 2 / 4000) ** 2 / 8 * 1.01 + 1e-12)
            # Every curve point lies within tol of the polyline.
            for q in samples(curves, 200):
                d = min(_segment_distance(q, a, b) for a, b in zip(ring, ring[1:]))
                self.assertLessEqual(d, tol * (1 + 1e-9))

    def test_cusp_stays_within_tolerance(self):
        # A control polygon that folds back past the chord's start: the old line-distance
        # test accepted it although the curve strays 1.06 x tolerance from the chord.
        p0, p1, p2, p3 = (0.0, 0.0), (-0.999e-3, 0.999e-3), (-0.999e-3, 0.999e-3), (1e-6, 0.0)
        tol = 1e-3
        ring = [p0] + cr.flatten_cubic(p0, p1, p2, p3, tol)
        for i in range(2001):
            q = bezier(p0, p1, p2, p3, i / 2000)
            self.assertLessEqual(min(_segment_distance(q, a, b) for a, b in zip(ring, ring[1:])), tol * (1 + 1e-9))

    def test_refusals(self):
        with self.assertRaises(cr.Refused):
            cr.flatten_cubic((0, 0), (1, 1), (2, 1), (3, 0), 0)
        with self.assertRaises(cr.Refused):
            cr.flatten_contour((0, 0), [('Q', (1, 1))], 1e-3)
        with self.assertRaises(cr.Refused):
            cr.flatten_contour((0, 0), [('L', (0, 0))], 1e-3)


def _segment_distance(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    length2 = dx * dx + dy * dy
    t = 0 if length2 == 0 else max(0, min(1, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / length2))
    return math.dist(p, (a[0] + t * dx, a[1] + t * dy))


class Classification(unittest.TestCase):
    def test_nested_shell_hole_island(self):
        rings = [cr.flatten_contour(*circle(0, 0, r, clockwise=(r == 6))[:2], 1e-3) for r in (10, 6, 3)]
        polygons = cr.classify(rings)
        self.assertEqual(len(polygons), 2)
        self.assertEqual(len(polygons[0][1]), 1)
        self.assertEqual(len(polygons[1][1]), 0)

    def test_nested_contour_wound_like_its_parent_refused(self):
        # Under nonzero, a same-wound nested contour is filled, not a hole.
        outer = cr.flatten_contour(*circle(0, 0, 10)[:2], 1e-3)
        inner = cr.flatten_contour(*circle(0, 0, 5)[:2], 1e-3)
        with self.assertRaises(cr.Refused):
            cr.classify([outer, inner])
        # Wound against it, it is a hole.
        hole = cr.flatten_contour(*circle(0, 0, 5, clockwise=True)[:2], 1e-3)
        self.assertEqual(len(cr.classify([outer, hole])[0][1]), 1)

    def test_crossing_contours_refused(self):
        rings = [cr.flatten_contour(*circle(0, 0, 10)[:2], 1e-3), cr.flatten_contour(*circle(5, 0, 10)[:2], 1e-3)]
        with self.assertRaises(cr.Refused):
            cr.classify(rings)

    def test_touching_contours_refused(self):
        square = [(0, 0), (10, 0), (10, 10), (0, 10), (0, 0)]
        inner = [(0, 2), (4, 2), (4, 6), (0, 6), (0, 2)]  # shares the x = 0 wall
        with self.assertRaises(cr.Refused):
            cr.classify([square, inner])


class CurvedOffsets(unittest.TestCase):
    TOL = 1e-3

    def run_core(self, polygons, h, limit=4):
        result = batch([cr.request(polygons, h, limit)])[0]
        self.assertTrue(result['ok'], result)
        return result

    def check_band(self, result_rings, curves, source_rings, h):
        pts = samples(curves)
        turn = max(max_turn(r) for r in source_rings)
        high = h / math.cos(turn / 2) + self.TOL + 1e-6
        low = h - self.TOL - 1e-6
        for ring in result_rings:
            for v in ring[:-1]:
                d = distance_to_samples(v, pts)
                self.assertGreaterEqual(d, low, (v, d))
                self.assertLessEqual(d, high, (v, d))

    def test_circle(self):
        start, segments, curves = circle(0, 0, 10)
        ring = cr.flatten_contour(start, segments, self.TOL)
        result = self.run_core(cr.classify([ring]), 1.0)
        self.assertEqual((result['components'], result['holes']), (1, 0))
        shell = result['geometry'][0][0]
        self.check_band([shell], curves, [ring], 1.0)
        # Area lies between circles of the band's radii (the cubic circle overshoots by < 2.8e-4 r).
        self.assertLess(abs(abs(signed_area(shell)) - math.pi * 11**2), 2 * math.pi * 11 * (self.TOL + 3e-3))

    def test_annulus_keeps_its_hole(self):
        outer = cr.flatten_contour(*circle(0, 0, 10)[:2], self.TOL)
        hole_start, hole_segments, hole_curves = circle(0, 0, 5, clockwise=True)
        hole = cr.flatten_contour(hole_start, hole_segments, self.TOL)
        polygons = cr.classify([outer, hole])
        result = self.run_core(polygons, 1.0)
        self.assertEqual((result['components'], result['holes']), (1, 1))
        rings = result['geometry'][0]
        inner = min(rings, key=lambda r: abs(signed_area(r)))
        self.check_band([inner], hole_curves, [hole], 1.0)
        self.assertLess(abs(abs(signed_area(inner)) - math.pi * 4**2), 2 * math.pi * 4 * (self.TOL + 3e-3))

    def lens_tip(self, angle, limit=4, h=1.0):
        start, segments, curves = lens(20.0, angle)
        ring = cr.flatten_contour(start, segments, 1e-4)
        result = self.run_core(cr.classify([ring]), h, limit)
        shell = result['geometry'][0][0]
        return ring, max(p[0] for p in shell)

    def test_legitimate_miter_is_kept(self):
        # Interior angle 30 degrees: miter ratio 1/sin(15 deg) = 3.864 < limit 4, so the tip
        # extends to L + h/sin(theta/2), with theta the flattened ring's own tip angle.
        ring, reach = self.lens_tip(30)
        a, b, c = ring[-2], ring[0], ring[1]
        theta = _angle_at(b, a, c)
        self.assertLess(abs(math.degrees(theta) - 30), 0.2)
        self.assertLess(abs(reach - (20 + 1 / math.sin(theta / 2))), 1e-6)

    def test_excessive_miter_is_clamped_not_rounded_or_beveled(self):
        # Interior angle 20 degrees: ratio 5.76 > 4. The core clips the miter flat at
        # limit * h beyond the tip (a clamped miter). A native bevel would reach only
        # L + h sin(theta / 2), about L + 0.17, and a round join L + h.
        ring, reach = self.lens_tip(20)
        self.assertLess(abs(reach - (20 + 4 * 1.0)), 1e-6)

    def test_limit_boundary_moves_the_clamp(self):
        _, reach = self.lens_tip(20, limit=6)
        ring, _ = self.lens_tip(20, limit=6)
        theta = _angle_at(ring[0], ring[-2], ring[1])
        self.assertLess(abs(reach - (20 + 1 / math.sin(theta / 2))), 1e-6)


def _angle_at(b, a, c):
    u = (a[0] - b[0], a[1] - b[1])
    v = (c[0] - b[0], c[1] - b[1])
    return math.acos((u[0] * v[0] + u[1] * v[1]) / (math.hypot(*u) * math.hypot(*v)))


if __name__ == '__main__':
    unittest.main()
