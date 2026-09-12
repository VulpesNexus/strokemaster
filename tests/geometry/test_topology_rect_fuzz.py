"""Bounded independent orthogonal-union oracle; no host or external geometry API.

Integer-grid rectangles and binary-exact h make the sweep area an exact rational.
Positive miter buffering of these orthogonal regions is L-infinity dilation,
which distributes over union. No claim extends this oracle to nonorthogonal art.
"""
from collections import Counter
from fractions import Fraction
import hashlib
import json
import math
import random
import unittest

from test_topology_region import EXE, batch, rect, request, wkt


SEED = 20260913
CASE_COUNT = 80
AREA_BUDGET = 2e-7
LENGTH_BUDGET = 2e-6


def expanded_ticks(boxes, h):
    """Coordinates in eighths; input coordinates and h are exact on this grid."""
    distance = int(h * 8)
    assert distance / 8 == h
    return [(8*x0-distance, 8*y0-distance, 8*x1+distance, 8*y1+distance)
            for x0, y0, x1, y1 in boxes]


def exact_sweep_area(boxes):
    """Integrate integer y-interval union lengths over each integer x slab."""
    cuts = sorted({x for x0, _, x1, _ in boxes for x in (x0, x1)})
    area_in_square_ticks = 0
    for left, right in zip(cuts, cuts[1:]):
        intervals = sorted((y0, y1) for x0, y0, x1, y1 in boxes
                           if x0 < right and x1 > left)
        length = 0
        if intervals:
            low, high = intervals[0]
            for start, end in intervals[1:]:
                if start > high:
                    length += high-low
                    low, high = start, end
                else:
                    high = max(high, end)
            length += high-low
        area_in_square_ticks += (right-left)*length
    return Fraction(area_in_square_ticks, 64)


def ray_in_ring(point, ring):
    """Half-open horizontal ray crossing, independent of the core's probes."""
    x, y = point
    crossings = 0
    for (ax, ay), (bx, by) in zip(ring, ring[1:]):
        if (ay <= y < by) or (by <= y < ay):
            hit = ax + (y-ay)*(bx-ax)/(by-ay)
            crossings += hit > x
    return bool(crossings % 2)


def ray_in_region(point, geometry):
    return any(ray_in_ring(point, polygon[0]) and
               not any(ray_in_ring(point, hole) for hole in polygon[1:])
               for polygon in geometry)


def ring_area(ring):
    ox, oy = ring[0]
    return math.fsum((ax-ox)*(by-oy)-(bx-ox)*(ay-oy)
                     for (ax, ay), (bx, by) in zip(ring, ring[1:]))/2


def geometry_request(boxes, h):
    query = request([wkt(rect(*box)) for box in boxes], h=h, limit=4, join="miter")
    # The shared helper defaults to an explicit envelope. This oracle exercises
    # the positive filled buffer itself, with no additional clipping operation.
    query.pop("envelope")
    return query


class TopologyRectFuzzTests(unittest.TestCase):
    def check_accepted(self, boxes, h, result):
        self.assertIs(result.get("ok"), True, result)
        self.assertNotIn("error", result)
        self.assertTrue(math.isfinite(result["area"]))
        geometry = result["geometry"]
        self.assertTrue(geometry)
        ticks = expanded_ticks(boxes, h)
        expected = exact_sweep_area(ticks)
        scale = max(max(b[2] for b in boxes)-min(b[0] for b in boxes),
                    max(b[3] for b in boxes)-min(b[1] for b in boxes), h)
        self.assertEqual(result["scale"], scale)
        actual_area = 0
        for polygon in geometry:
            self.assertTrue(polygon)
            for ring in polygon:
                self.assertGreaterEqual(len(ring), 4)
                self.assertEqual(ring[0], ring[-1])
                self.assertTrue(all(math.isfinite(v) for p in ring for v in p))
            actual_area += abs(ring_area(polygon[0]))
            actual_area -= math.fsum(abs(ring_area(hole)) for hole in polygon[1:])
        errors = [abs(actual_area-float(expected))/scale**2,
                  abs(result["area"]-float(expected))/scale**2]
        self.assertLessEqual(max(errors), AREA_BUDGET,
                             (boxes, h, actual_area, float(expected), result["area"]))

        # Every cell of the rectangle-edge arrangement has constant oracle
        # membership. Its midpoint is strictly off all expected boundary lines.
        xs = sorted({x for x0, _, x1, _ in ticks for x in (x0, x1)})
        ys = sorted({y for _, y0, _, y1 in ticks for y in (y0, y1)})
        xs = [xs[0]-8] + xs + [xs[-1]+8]
        ys = [ys[0]-8] + ys + [ys[-1]+8]
        probes = 0
        for left, right in zip(xs, xs[1:]):
            for bottom, top in zip(ys, ys[1:]):
                self.assertGreater(min(right-left, top-bottom)/16,
                                   LENGTH_BUDGET*scale)
                # Doubled tick coordinates keep the rectangle membership exact.
                px2, py2 = left+right, bottom+top
                expected_inside = any(2*x0 < px2 < 2*x1 and 2*y0 < py2 < 2*y1
                                      for x0, y0, x1, y1 in ticks)
                point = (px2/16, py2/16)
                self.assertEqual(ray_in_region(point, geometry), expected_inside,
                                 (boxes, h, point, expected_inside))
                probes += 1
        return max(errors), probes

    def test_00_concave_offset_oracle_sanity(self):
        boxes = [(0, 0, 4, 1), (0, 0, 1, 4)]
        self.assertEqual(exact_sweep_area(expanded_ticks(boxes, .125)), Fraction(145, 16))
        self.assertEqual(exact_sweep_area(expanded_ticks(boxes, .5)), Fraction(16))
        for h, result in zip((.125, .5), batch([geometry_request(boxes, h)
                                               for h in (.125, .5)])):
            with self.subTest(h=h):
                self.check_accepted(boxes, h, result)
                self.assertFalse(ray_in_region((1+h+.125, 1+h+.125), result["geometry"]))
                self.assertTrue(ray_in_region((1+h-.0625, 1+h-.0625), result["geometry"]))

    def test_seeded_rectangle_unions(self):
        rng = random.Random(SEED)
        cases = []
        for index in range(CASE_COUNT):
            boxes = []
            for _ in range(rng.randint(2, 6)):
                x, y = rng.randint(-10, 10), rng.randint(-10, 10)
                boxes.append((x, y, x+rng.randint(1, 6), y+rng.randint(1, 6)))
            cases.append((boxes, (.125, .5)[index % 2]))
        executable_before = hashlib.sha256(EXE.read_bytes()).hexdigest()
        results = batch([geometry_request(boxes, h) for boxes, h in cases])
        self.assertEqual(hashlib.sha256(EXE.read_bytes()).hexdigest(), executable_before,
                         "Executable changed during fuzz; do not attribute these results")
        accepted = validated = probes = 0
        max_error = 0
        refusals = []
        for index, ((boxes, h), result) in enumerate(zip(cases, results)):
            with self.subTest(seed=SEED, index=index, h=h, boxes=boxes):
                if result.get("ok") is not True:
                    self.assertIsInstance(result.get("error"), str, result)
                    self.assertTrue(result["error"])
                    self.assertNotIn("geometry", result)
                    refusals.append({"index": index, "error": result["error"]})
                    continue
                accepted += 1
                error, count = self.check_accepted(boxes, h, result)
                max_error = max(max_error, error)
                probes += count
                validated += 1
        print("Rectangle union fuzz: " + json.dumps({
            "seed": SEED, "cases": CASE_COUNT, "h": [.125, .5],
            "executable_sha256": executable_before,
            "accepted": accepted, "validated_geometry_successes": validated,
            "refused": len(refusals), "refusals": refusals,
            "refusal_reasons": dict(Counter(r["error"] for r in refusals)),
            "independent_membership_probes": probes,
            "max_normalized_area_error": max_error,
        }, sort_keys=True))
        self.assertGreater(accepted, 0, "All inputs refused; no geometry success was established")


if __name__ == "__main__":
    unittest.main(verbosity=2)
