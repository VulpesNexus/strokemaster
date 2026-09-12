"""Analytic adversarial checks for the external Boost region prototype.

The Python oracle uses rectangle/triangle formulas and ray crossing, not Boost
or the prior local join kernel. Build the executable before running this file.
"""
import json
import math
import os
from pathlib import Path
import random
import subprocess
import unittest

EXE = Path(os.environ.get("TOPOLOGY_EXE", Path(__file__).parent /
                         "topology_build/topology_region.exe"))


def rect(x0, y0, x1, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1), (x0, y0)]


def wkt(shell, *holes):
    return "POLYGON(" + ",".join("(" + ",".join(
        f"{x:.17g} {y:.17g}" for x, y in ring) + ")"
        for ring in (shell, *holes)) + ")"


def request(source, envelope=None, h=1, **extra):
    return dict(source=source, envelope=envelope or [wkt(rect(-100, -100, 100, 100))],
                h=h, **extra)


def batch(requests):
    if not EXE.is_file():
        raise RuntimeError("Build tools/topology_region.cpp first; missing " + str(EXE))
    process = subprocess.run([str(EXE)], input="".join(json.dumps(r) + "\n" for r in requests),
                             text=True, capture_output=True, timeout=60, check=True)
    results = [json.loads(line) for line in process.stdout.splitlines()]
    if len(results) != len(requests) or process.stderr:
        raise AssertionError((process.stdout, process.stderr))
    return results


def signed_area(ring):
    # Translate before summing to keep the independent oracle stable under shifts.
    ox, oy = ring[0]
    return math.fsum((a[0]-ox)*(b[1]-oy) - (b[0]-ox)*(a[1]-oy)
                     for a, b in zip(ring, ring[1:])) / 2


def inside_ring(point, ring):
    x, y = point
    crossing = False
    for (ax, ay), (bx, by) in zip(ring, ring[1:]):
        if (ay > y) != (by > y) and x < ax + (y-ay)*(bx-ax)/(by-ay):
            crossing = not crossing
    return crossing


def inside(point, geometry):
    return any(inside_ring(point, p[0]) and not any(inside_ring(point, r) for r in p[1:])
               for p in geometry)


def rectangle_union_area(boxes):
    """Independent scalar area integral; creates no polygon or clipping engine."""
    xs = sorted({x for b in boxes for x in (b[0], b[2])})
    area = 0
    for left, right in zip(xs, xs[1:]):
        intervals = sorted((b[1], b[3]) for b in boxes if b[0] < right and b[2] > left)
        total = 0
        end = -math.inf
        for low, high in intervals:
            total += max(0, high-max(low, end))
            end = max(end, high)
        area += (right-left)*total
    return area


class TopologyRegionTests(unittest.TestCase):
    max_normalized_analytic_error = 0
    requests_checked = 0

    def successful(self, req):
        result = batch([req])[0]
        self.assertTrue(result.get("ok"), result)
        type(self).requests_checked += 1
        self.assertLessEqual(result["outside_area_normalized"], 2e-7)
        self.assertLessEqual(result["lost_fill_area_normalized"], 2e-7)
        area = 0
        for p in result["geometry"]:
            self.assertLess(signed_area(p[0]), 0)  # Boost output: clockwise shells.
            for hole in p[1:]:
                self.assertGreater(signed_area(hole), 0)
            area -= sum(signed_area(r) for r in p)
            for ring in p:
                self.assertEqual(ring[0], ring[-1])
                self.assertTrue(all(math.isfinite(v) for p in ring for v in p))
        self.assertAlmostEqual(area / result["scale"]**2,
                               result["area"] / result["scale"]**2, delta=1e-10)
        return result

    def area_close(self, result, expected):
        error = abs(result["area"]-expected)/result["scale"]**2
        type(self).max_normalized_analytic_error = max(type(self).max_normalized_analytic_error, error)
        self.assertLessEqual(error, 2e-7, (result["area"], expected, error))

    def test_rectangle_preserves_ordinary_miters(self):
        expected = wkt(rect(-1, -1, 5, 4))
        r = self.successful(request([wkt(rect(0, 0, 4, 3))], [expected],
                                    expected=[expected], probes=[[-1, -1], [5, 4]]))
        self.area_close(r, 30)
        self.assertEqual(r["probes"], [True, True])
        self.assertEqual(r["expected_error_normalized"], 0)

    def test_triangular_hole_erodes_into_hole(self):
        s, c = math.sin(math.pi/12), math.cos(math.pi/12)
        a, h = 7.5, .5
        hole = [(0, 0), (a*s/c, -a), (-a*s/c, -a), (0, 0)]
        k = ((a-h)*s-h)/c
        remaining = [(0, -h/s), (k, -a+h), (-k, -a+h), (0, -h/s)]
        envelope = wkt(rect(-8, -9, 8, .3+h))
        expected = wkt(rect(-5-h, -8-h, 5+h, .3+h), remaining)
        r = self.successful(request([wkt(rect(-5, -8, 5, .3), hole)], [envelope], h=h,
                                    expected=[expected], probes=[[0, .7], [0, -3], [0, -1]]))
        self.assertEqual(r["holes"], 1)
        self.assertEqual(r["probes"], [True, False, True])
        self.assertLess(r["expected_error_normalized"], 2e-7)
        for p in r["geometry"]:
            for x, y in p[0]:
                self.assertLessEqual(y, .8+1e-10)

    def test_original_synthetic_hole_has_no_upward_spike(self):
        a, h, d = 7.5, 1.5, .3
        x = a*math.tan(math.pi/12)
        env = wkt(rect(-9, -13, 9, d+h))
        r = self.successful(request([wkt(rect(-7.5, -12, 7.5, d),
                                          [(0, 0), (x, -a), (-x, -a), (0, 0)])], [env], h=h))
        inradius = a*math.sin(math.pi/12)/(1+math.sin(math.pi/12))
        self.assertEqual(r["holes"], int(h < inradius))
        self.assertGreater(math.sqrt(2)*h, d+h)  # Original local policy negative control.
        self.assertLessEqual(max(y for p in r["geometry"] for x, y in p[0]), d+h+1e-10)

    def test_filled_buffer_suppresses_symmetric_inner_spike_without_external_envelope(self):
        a,h,d = 7.5,1.5,.3
        x = a*math.tan(math.pi/12)
        source = wkt(rect(-7.5,-12,7.5,d),[(0,0),(x,-a),(-x,-a),(0,0)])
        r = self.successful({"source":[source],"h":h,"limit":4,"compare_boundary":True})
        self.assertEqual(r["envelope_source"],"filled_region_buffer")
        self.assertEqual(r["clipped_area_normalized"],0)
        top = max(y for p in r["geometry"] for x,y in p[0])
        symmetric_top = max(y for p in r["symmetric_geometry"] for x,y in p[0])
        self.assertAlmostEqual(top,d+h,delta=1e-8)
        self.assertAlmostEqual(symmetric_top,h/math.sin(math.pi/12),delta=1e-6)
        self.assertGreater(r["symmetric_excess_area_normalized"],0)
        self.assertTrue(inside((0,3),r["symmetric_geometry"]))
        self.assertFalse(inside((0,3),r["geometry"]))
        self.assertTrue(any(abs(x+9)<1e-8 and abs(y-1.8)<1e-8
                            for p in r["geometry"] for x,y in p[0]))

    def test_closed_notch_filled_buffer_without_external_envelope(self):
        d,h = .3,1.5
        mouth = d*math.tan(math.pi/12)
        shell = [(-5,-8),(5,-8),(5,d),(mouth,d),(0,0),(-mouth,d),(-5,d),(-5,-8)]
        r = self.successful({"source":[wkt(shell)],"h":h,"compare_boundary":True})
        self.area_close(r,13*11.3)
        self.assertAlmostEqual(max(y for p in r["geometry"] for x,y in p[0]),1.8,delta=1e-8)
        self.assertAlmostEqual(max(y for p in r["symmetric_geometry"] for x,y in p[0]),1.8,delta=1e-8)
        self.assertLessEqual(r["symmetric_excess_area_normalized"],2e-7)

    def test_symmetric_comparison_preserves_ordinary_rectangle_control(self):
        r = self.successful({"source":[wkt(rect(0,0,4,3))],"h":1,"compare_boundary":True})
        self.area_close(r,30)
        self.assertLessEqual(r["symmetric_excess_area_normalized"],2e-7)
        self.assertAlmostEqual(-sum(signed_area(ring) for p in r["symmetric_geometry"] for ring in p),
                               30,delta=1e-8)

    def test_miter_buffer_not_monotone_for_arbitrary_surrounding_region(self):
        triangle = wkt([(0,0),(2,0),(2,2),(0,0)])
        r = self.successful({"source":[triangle],"h":1,"limit":4})
        # The triangle's genuine exterior miter exceeds the enclosing square's
        # [-1,3]^2 buffer. This is not an inward stroke contribution.
        self.assertAlmostEqual(min(x for p in r["geometry"] for x,y in p[0]),
                               -(1+math.sqrt(2)),delta=1e-8)

    def test_open_notch_disappears_without_extrapolated_spike(self):
        d, h = .3, 1.5
        mouth = d*math.tan(math.pi/12)
        shell = [(-5,-8),(5,-8),(5,d),(mouth,d),(0,0),(-mouth,d),(-5,d),(-5,-8)]
        expected = wkt(rect(-6.5, -9.5, 6.5, 1.8))
        r = self.successful(request([wkt(shell)], [expected], h=h, expected=[expected]))
        self.assertLess(r["expected_error_normalized"], 2e-7)
        self.assertEqual(r["holes"], 0)
        self.area_close(r, 13*11.3)

    def test_hole_disappears_at_analytic_inradius(self):
        hole = [(0,0),(6,0),(0,8),(0,0)]
        for h in (1.9, 2, 2.1, 20):
            with self.subTest(h=h):
                r = self.successful(request([wkt(rect(-3,-3,10,11), hole)], h=h))
                self.assertEqual(r["holes"], int(h < 2))
                self.area_close(r, (13+2*h)*(14+2*h) - 24*max(0, 1-h/2)**2)

    def test_two_counters_disappear_independently(self):
        f = wkt(rect(0,0,20,12), rect(2,2,4,4), rect(8,2,16,10))
        for h, holes in ((.5,2),(1,1),(3,1),(4,0)):
            r = self.successful(request([f], h=h))
            self.assertEqual(r["holes"], holes)
            self.area_close(r, (20+2*h)*(12+2*h)-max(0,2-2*h)**2-max(0,8-2*h)**2)

    def test_component_merger_at_gap_two_h(self):
        for gap, count in ((2.01,2),(2,1),(1.99,1)):
            r = self.successful(request([wkt(rect(0,0,2,2)), wkt(rect(2+gap,0,4+gap,2))]))
            self.assertEqual(r["components"], count)
            self.area_close(r, 32-max(0,2-gap)*4)

    def test_union_precedes_offset_to_remove_decomposition_seams(self):
        lower = [(0,0),(2,0),(2,2),(0,0)]
        upper = [(0,0),(2,2),(0,2),(0,0)]
        # A loose envelope ensures only union-before-offset prevents the seam spike.
        r = self.successful(request([wkt(lower), wkt(upper)], expected=[wkt(rect(-1,-1,3,3))]))
        self.area_close(r, 16)
        self.assertEqual(r["expected_error_normalized"], 0)
        self.assertTrue(all(-1-1e-10 <= x <= 3+1e-10 and -1-1e-10 <= y <= 3+1e-10
                            for p in r["geometry"] for x,y in p[0]))

    def test_overlap_and_duplicate_sources_union_once(self):
        a, b = wkt(rect(0,0,3,2)), wkt(rect(2,0,5,2))
        r = self.successful(request([a,b,a], expected=[wkt(rect(-1,-1,6,3))]))
        self.assertAlmostEqual(r["source_area"], 10, delta=1e-10)
        self.area_close(r, 28)
        self.assertEqual(r["components"], 1)

    def test_nested_island_in_explicit_hole(self):
        r = self.successful(request([wkt(rect(0,0,20,20), rect(3,3,17,17)),
                                    wkt(rect(7,7,13,13))], h=.5))
        self.assertEqual((r["components"],r["holes"]), (2,1))
        self.area_close(r, 21**2-13**2+7**2)

    def test_point_touching_components_merge_under_positive_offset(self):
        r = self.successful(request([wkt(rect(0,0,2,2)),wkt(rect(2,2,4,4))]))
        self.assertEqual(r["components"], 1)
        self.area_close(r, 28)

    def test_explicit_envelope_actually_clips(self):
        source = wkt([(0,0),(8,-1),(8,1),(0,0)])
        envelope = wkt(rect(-1,-2,9,2))
        r = self.successful(request([source],[envelope], limit=20))
        self.assertGreater(r["clipped_area_normalized"], 0)
        self.assertTrue(all(x >= -1-2e-6*r["scale"] for p in r["geometry"] for x,y in p[0]))

    def test_protected_miter_patch_is_retained_or_conflict_rejected(self):
        source = wkt(rect(0,0,4,3))
        patch = wkt(rect(-1,-1,0,0))
        req = request([source],[wkt(rect(-1,-1,5,4))],protected=[patch])
        self.successful(req)
        req["envelope"] = [wkt(rect(-.5,-1,5,4))]
        result = batch([req])[0]
        self.assertIn("protected region versus envelope",result.get("error",""))
        self.assertNotIn("geometry",result)

    def test_concave_envelope_and_envelope_hole(self):
        shell = [(0,0),(4,0),(4,1),(1,1),(1,4),(0,4),(0,0)]
        r = self.successful(request([wkt(rect(.2,.2,.8,.8))],[wkt(shell)],h=1))
        self.area_close(r,2.6)  # [0,1.8]^2 intersect the analytic L.
        for x in (.1,.7,1.2,1.7):
            for y in (.1,.7,1.2,1.7):
                self.assertEqual(inside((x,y),r["geometry"]),x<1 or y<1)
        env = wkt(rect(-3,-3,3,3),rect(.5,.5,2,2))
        r = self.successful(request([wkt(rect(-1,-1,0,0))],[env],h=2))
        self.assertEqual(r["holes"],0)  # Counter touches candidate boundary, becoming a notch.
        self.assertFalse(inside((1,1),r["geometry"]))
        self.area_close(r,25-2.25)

    def test_empty_negative_space_at_hole_threshold_epsilon(self):
        source = wkt(rect(-3,-3,10,11),[(0,0),(6,0),(0,8),(0,0)])
        for h in (2-1e-4,2,2+1e-4):
            r = self.successful(request([source],h=h))
            self.assertEqual(r["holes"],int(h<2))

    def test_deterministic_acute_triangle_fuzz(self):
        rng = random.Random(81745)
        for seed in range(24):
            h, a, alpha = rng.uniform(.1,2),rng.uniform(8,20),rng.uniform(.03,1.2)
            s,c = math.sin(alpha),math.cos(alpha)
            points = [(0,0),(a*s,-a*c),(-a*s,-a*c),(0,0)]
            r = self.successful(request([wkt(points)],h=h,limit=100))
            bottom,top = -a*c-h,h/s
            width = (h-s*bottom)/c
            self.area_close(r,width*(top-bottom))
            for p in r["geometry"]:
                for x,y in p[0]:
                    self.assertGreaterEqual(y,bottom-1e-8)
                    self.assertLessEqual(c*abs(x)+s*y,h+1e-8)

    def test_ordinary_acute_miter_intersection_is_preserved(self):
        for degrees in (30,60,90,120):
            s, c = math.sin(math.radians(degrees/2)), math.cos(math.radians(degrees/2))
            tip = 1/s
            triangle = [(0,0),(10*s,-10*c),(-10*s,-10*c),(0,0)]
            r = self.successful(request([wkt(triangle)], limit=4))
            actual_tip = max(y for p in r["geometry"] for x,y in p[0])
            self.assertAlmostEqual(actual_tip, tip, delta=1e-8)

    def test_boost_limit_is_not_native_bevel_or_flatclip(self):
        s, c = math.sin(math.pi/36), math.cos(math.pi/36)
        r = self.successful(request([wkt([(0,0),(10*s,-10*c),(-10*s,-10*c),(0,0)])],limit=4))
        self.assertAlmostEqual(max(y for p in r["geometry"] for x,y in p[0]), 4, delta=1e-8)
        self.assertNotAlmostEqual(4, s)  # Native bevel plane for this analytic wedge.

    def test_round_offset_convergence_has_independent_area_and_chord_bounds(self):
        previous = math.inf
        for n in (16,32,64,128,256):
            r = self.successful(request([wkt(rect(0,0,4,3))],join="round",circle_points=n))
            expected = 12+14+(n/2)*math.sin(2*math.pi/n)
            self.area_close(r, expected)
            error = 12+14+math.pi-r["area"]
            self.assertGreater(error, 0)
            self.assertLess(error, previous/3 if math.isfinite(previous) else math.inf)
            previous = error
            # Every midpoint on the top-right circular arc has the analytic sagitta bound.
            points = r["geometry"][0][0]
            for a,b in zip(points,points[1:]):
                if a[0] >= 4 and a[1] >= 3 and b[0] >= 4 and b[1] >= 3:
                    radius = math.hypot((a[0]+b[0])/2-4,(a[1]+b[1])/2-3)
                    self.assertLessEqual(1-radius, 1-math.cos(math.pi/n)+1e-8)

    def test_orientation_reflection_rotation_and_similarity(self):
        ring, hole = rect(0,0,8,6), rect(2,2,6,4)
        for scale in (1e-9,1,1e9):
            for reflected in (-1,1):
                for reverse in (False,True):
                    angle = .371
                    def transform(r):
                        out = [(scale*(100+reflected*x*math.cos(angle)-y*math.sin(angle)),
                                scale*(-50+reflected*x*math.sin(angle)+y*math.cos(angle))) for x,y in r]
                        return out[::-1] if reverse else out
                    env = wkt(transform(rect(-3,-3,11,9)))
                    r = self.successful(request([wkt(transform(ring),transform(hole))],[env],h=.25*scale))
                    self.area_close(r, ((8.5*6.5)-(3.5*1.5))*scale**2)
                    self.assertEqual(r["holes"], 1)

    def test_near_parallel_and_thin_stem(self):
        for delta in (1e-3,1e-5):
            source = wkt([(0,0),(10,delta),(10,delta+.0001),(0,.0001),(0,0)])
            r = self.successful(request([source],h=.1))
            self.assertEqual(r["components"], 1)
            self.assertGreater(r["area"], 2)
        r = self.successful(request([wkt(rect(0,0,10,.0001))],h=.1))
        self.area_close(r,10.2*.2001)

    def test_deterministic_fuzz_orthogonal_regions(self):
        randomizer = random.Random(4596837)
        for seed in range(60):
            with self.subTest(seed=seed):
                boxes = []
                for _ in range(4):
                    x,y = randomizer.uniform(-4,4),randomizer.uniform(-4,4)
                    boxes.append((x,y,x+randomizer.uniform(.3,3),y+randomizer.uniform(.3,3)))
                h = randomizer.uniform(.1,1)
                req = request([wkt(rect(*b)) for b in boxes],h=h)
                r = self.successful(req)
                self.assertEqual(batch([req])[0],r)
                expanded = [(x0-h,y0-h,x1+h,y1+h) for x0,y0,x1,y1 in boxes]
                self.area_close(r,rectangle_union_area(expanded))
                for _ in range(30):
                    p = (randomizer.uniform(-6,8),randomizer.uniform(-6,8))
                    expected = any(x0 < p[0] < x1 and y0 < p[1] < y1 for x0,y0,x1,y1 in expanded)
                    self.assertEqual(inside(p,r["geometry"]),expected,(seed,p))

    def test_invalid_geometry_is_rejected_not_skipped(self):
        bad = ["POLYGON EMPTY", "POLYGON((0 0,1 0,0 0))",
               "POLYGON((0 0,2 2,0 2,2 0,0 0))",  # Bow tie.
               "POLYGON((0 0,1 0,1 0,1 1,0 1,0 0))",  # Repeated anchor.
               "POLYGON((0 0,2 0,1 0,2 2,0 2,0 0))",  # Reversal cusp.
               "POLYGON((0 0,2 0,1 0.000000001,2 2,0 2,0 0))",  # Near cusp.
               "POLYGON((0 0,2 0,2 2,0 2))",  # Open.
               wkt(rect(0,0,2,2),rect(4,4,5,5)),
               wkt(rect(0,0,10,10),rect(2,2,8,8),rect(3,3,7,7))]
        for value,result in zip(bad,batch([request([p]) for p in bad])):
            self.assertIn("error",result,value)
            self.assertNotIn("geometry",result)

    def test_invalid_parameters_and_incompatible_envelope(self):
        base = request([wkt(rect(0,0,2,2))])
        bad = [dict(base,**change) for change in (
            {"h":0},{"h":-1},{"h":"nan"},{"h":"inf"},{"limit":.5},{"limit":101},
            {"circle_points":15},{"join":"clip"},{"fill_rule":"nonzero"},{"h":1e-10},
            {"envelope":[wkt(rect(.1,0,2,2))]},
            {"source":["POLYGON((nan 0,1 0,1 1,nan 0))"]})]
        for result in batch(bad):
            self.assertIn("error",result)
            self.assertNotIn("geometry",result)

    def test_error_does_not_poison_next_request(self):
        good = request([wkt(rect(0,0,2,2))])
        bad,following = batch([dict(good,h=-1),good])
        self.assertIn("error",bad)
        self.assertTrue(following["ok"])

    @classmethod
    def tearDownClass(cls):
        print(f"Topology successful requests: {cls.requests_checked}; "
              f"max normalized analytic area error: {cls.max_normalized_analytic_error:.17g}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
