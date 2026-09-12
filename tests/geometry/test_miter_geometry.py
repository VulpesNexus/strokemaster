"""Join-only analytic regressions; no host, flattening, or Boolean-union oracle."""
import math
import os
from pathlib import Path
import random
import shutil
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import miter_geometry as geometry
import miter_rule as rule
import stroke_outline as outline

NODE = os.environ.get('ISE_NODE') or shutil.which('node')
if not NODE:
    candidate = Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'nodejs/node.exe'
    NODE = str(candidate) if candidate.is_file() else None


def js_calls(cases):
    import json
    if not NODE:
        raise unittest.SkipTest('Node unavailable; JSX UNEXECUTED')
    result = subprocess.run(
        [NODE, str(Path(__file__).with_name('node_join_runner.js')),
         str(ROOT / 'tools/jsx/ise-clip-miter.jsx')],
        input=json.dumps(cases, allow_nan=False), text=True, capture_output=True, check=True)
    return json.loads(result.stdout)


def wedge(angle, h=1.0, cap_ratio=math.sqrt(2), mode='clip', limit=4.0):
    """Independent line intersections: -c*x+s*y=h and c*x+s*y=h.

    Intersect each line with y=L*h; never interpolate toward a miter tip.
    This oracle deliberately does not call production angle or join helpers.
    """
    a = math.radians(angle) / 2
    s, c = math.sin(a), math.cos(a)
    u, w = (s, c), (s, -c)
    p, q = (-h*c, h*s), (h*c, h*s)
    if mode == 'bevel' or (mode == 'miter' and 1/s > limit):
        boundary = [p, q]
    elif mode == 'miter' or s >= 1/cap_ratio/(1+1e-9):
        boundary = [p, (0, h/s), q]
    else:
        y = cap_ratio*h
        x = (h-s*y)/c
        boundary = [p, (-x, y), (x, y), q]
    return u, w, boundary


class GeometryTests(unittest.TestCase):
    max_normalized_error = 0.0
    coordinate_comparisons = 0

    @classmethod
    def tearDownClass(cls):
        print(f'Analytic comparisons: {cls.coordinate_comparisons}; '
              f'max |actual/h - expected/h| = {cls.max_normalized_error:.17g}')

    def assertPoints(self, actual, expected, scale=1.0, tolerance=3e-12):
        self.assertIsNotNone(actual)
        self.assertEqual(len(actual), len(expected))
        for p, q in zip(actual, expected):
            for x, y in zip(p, q):
                self.assertTrue(math.isfinite(x))
                error = abs(x/scale-y/scale)
                GeometryTests.max_normalized_error = max(GeometryTests.max_normalized_error, error)
                GeometryTests.coordinate_comparisons += 1
                self.assertLessEqual(error, tolerance,
                                     msg=f'{p} != {q} (scale={scale})')

    def test_plane_regression_outline(self):
        u, w, expected = wedge(30, 8)
        self.assertPoints(outline.join_polygon((0, 0), u, w, 8, 'clip')[1:], expected, 8)

    def test_plane_regression_diagnostic(self):
        u, w, expected = wedge(30, 8)
        self.assertPoints(geometry.corner_polygon((0, 0), u, w, 8, 'clip', math.sqrt(2)), expected, 8)

    def test_plane_regression_jsx(self):
        u, w, expected = wedge(30, 8)
        result = js_calls([{'args': [0, 0, u, w, 8, 'clip', math.sqrt(2), 4]}])[0]
        self.assertNotIn('error', result)
        self.assertPoints(result['value'][1:], expected, 8)


    def verifyCases(self, cases):
        requests = []
        for v, u, w, h, mode, ratio, limit, expected in cases:
            with self.subTest(v=v, h=h, mode=mode, ratio=ratio):
                self.assertPoints(outline.join_polygon(v, u, w, h, mode, ratio, limit)[1:], expected, h)
                self.assertPoints(geometry.corner_polygon(v, u, w, h, mode, ratio, limit), expected, h)
            requests.append({'args': [*v, u, w, h, mode, ratio, limit]})
        for result, case in zip(js_calls(requests), cases):
            self.assertNotIn('error', result, case)
            self.assertPoints(result['value'][1:], case[-1], case[3])

    def test_conventional_joins_and_threshold_epsilon(self):
        cases = []
        for angle in (10, 30, 60, 90, 120, 179.99):
            for mode in ('bevel', 'miter', 'clip'):
                u, w, expected = wedge(angle, mode=mode)
                cases.append(((0, 0), u, w, 1, mode, math.sqrt(2), 4, expected))
        for ratio in (1.0, math.sqrt(2), 2.0, 4.0):
            for relative in (-2e-9, 0, 0.5e-9, 0.99e-9, 1.01e-9, 2e-9):
                reach = ratio*(1+relative)
                if reach <= 1:
                    continue
                angle = math.degrees(2*math.asin(1/reach))
                u, w, expected = wedge(angle, cap_ratio=ratio)
                cases.append(((0, 0), u, w, 1, 'clip', ratio, 4, expected))
                self.assertEqual(rule.is_clamped(angle, ratio), len(expected) == 4)
                self.assertAlmostEqual(rule.clipped_reach(1, angle, ratio), max(p[1] for p in expected), places=14)
        self.verifyCases(cases)

    def test_orientation_reflection_similarity_and_scales(self):
        cases = []
        for angle in (0.001, 30, 89, 90, 120, 179.99):
            for h in (1e-200, 1e-12, 1, 1e12, 1e200):
                for rotation in (0, 0.73, 2.1):
                    for reflection in (-1, 1):
                        co, si = math.cos(rotation), math.sin(rotation)
                        def rotate(p):
                            x, y = p[0]*reflection, p[1]
                            return (co*x-si*y, si*x+co*y)
                        v = (2*h, -3*h)
                        u, w, expected = wedge(angle, h)
                        expected = [(v[0]+rotate(p)[0], v[1]+rotate(p)[1]) for p in expected]
                        cases.append((v, rotate(u), rotate(w), h, 'clip', math.sqrt(2), 4, expected))
        self.verifyCases(cases)

    def test_deterministic_fuzz_and_offset_line_residuals(self):
        rng = random.Random(20260912)
        cases = []
        for _ in range(400):
            angle, h, ratio = rng.uniform(0.01, 179.9), 10**rng.uniform(-150, 150), rng.uniform(1, 8)
            u, w, expected = wedge(angle, h, ratio)
            cases.append(((0, 0), u, w, h, 'clip', ratio, 4, expected))
            actual = outline.join_polygon((0, 0), u, w, h, 'clip', ratio)
            if len(actual) == 5:
                a, b = actual[2:4]
                s, c = u
                self.assertAlmostEqual(-c*(a[0]/h)+s*(a[1]/h), 1, places=12)
                self.assertAlmostEqual(c*(b[0]/h)+s*(b[1]/h), 1, places=12)
                self.assertAlmostEqual(a[1]/h, ratio, places=12)
                self.assertAlmostEqual(b[1]/h, ratio, places=12)
        self.verifyCases(cases)

    def test_nearly_cusp_without_materializing_overflowing_tip(self):
        cases = []
        for angle, h in ((1e-10, 1), (1e-200, 1e200), (1e-200, 1e-200)):
            for mode in ('clip', 'miter', 'bevel'):
                u, w, expected = wedge(angle, h, mode=mode)
                cases.append(((0, 0), u, w, h, mode, math.sqrt(2), 4, expected))
        self.verifyCases(cases)

    def test_flat_clip_is_axial_not_radial_cap(self):
        u, w, expected = wedge(30)
        actual = outline.join_polygon((0, 0), u, w, 1, 'clip')
        self.assertAlmostEqual(max(p[1] for p in actual), math.sqrt(2))
        self.assertGreater(max(math.hypot(*p) for p in actual), math.sqrt(2))
        self.verifyCases([((0, 0), u, w, 1, 'clip', math.sqrt(2), 4, expected)])

    def test_round_join_short_arc_and_existing_subdivision_counts(self):
        requests, expected_js = [], []
        for angle in (30, 90, 150):
            for reflect in (-1, 1):
                u, w, _ = wedge(angle)
                u, w = (reflect*u[0], u[1]), (reflect*w[0], w[1])
                half = math.radians(angle)/2
                def arc(steps):
                    return [(reflect*math.cos(math.pi-half-(math.pi-2*half)*k/steps),
                             math.sin(math.pi-half-(math.pi-2*half)*k/steps)) for k in range(steps+1)]
                self.assertPoints(outline.join_polygon((0, 0), u, w, 1, 'round')[1:], arc(12))
                self.assertPoints(geometry.corner_polygon((0, 0), u, w, 1, 'round'), arc(16))
                requests.append({'args': [0, 0, u, w, 1, 'round', math.sqrt(2), 4]})
                expected_js.append(arc(12))
        for result, expected in zip(js_calls(requests), expected_js):
            self.assertNotIn('error', result)
            self.assertPoints(result['value'][1:], expected)

    def test_straight_continuation_has_no_join(self):
        for u in ((1, 0), (0, -1)):
            self.assertIsNone(outline.join_polygon((0, 0), u, u, 1, 'clip'))
            self.assertEqual(geometry.corner_polygon((0, 0), u, u, 1, 'clip'), [])
        results = js_calls([{'args': [0, 0, [1, 0], [1, 0], 1, 'clip', 2, 4]}])
        self.assertEqual(results, [{'value': None}])

    def test_invalid_join_inputs_fail_explicitly(self):
        base = [(0, 0), (1, 0), (0, 1), 1, 'clip', 2, 4]
        bad = []
        for index, values in ((0, [(float('nan'), 0), (float('inf'), 0), (0,), None]),
                              (1, [(0, 0), (2, 0), (float('nan'), 0), None]),
                              (2, [(-1, 0)]),
                              (3, [0, -1, float('inf'), float('nan'), '1', True]),
                              (4, ['typo', None]),
                              (5, [0, 0.5, float('inf'), float('nan')]),
                              (6, [0, 0.5, float('inf'), float('nan')])):
            for value in values:
                args = base.copy()
                args[index] = value
                bad.append(args)
        # Overflow in the cap and in translated output must also fail.
        bad.extend([[(0, 0), (1, 0), (0, 1), 1e308, 'clip', 2, 4],
                    [(1.7e308, 1.7e308), (0, 1), (1, 0), 1e308, 'bevel', 2, 4]])
        requests = []
        for args in bad:
            for function in (outline.join_polygon, geometry.corner_polygon):
                with self.subTest(function=function.__name__, args=args):
                    with self.assertRaises(ValueError):
                        function(*args)
            # JSON null exercises the JS invalid-number path without nonstandard JSON NaN.
            def json_safe(v):
                if isinstance(v, (tuple, list)):
                    return [json_safe(x) for x in v]
                return None if isinstance(v, float) and not math.isfinite(v) else v
            v, *rest = json_safe(args)
            requests.append({'args': [*(v if isinstance(v, list) and len(v) == 2 else [None, None]), *rest]})
        for result in js_calls(requests):
            self.assertIn('error', result)

    def test_scalar_units_and_invalid_inputs(self):
        # Python integers outside binary64 range must use the same explicit error.
        with self.assertRaises(ValueError):
            rule.miter_reach(10**1000, 30)
        with self.assertRaises(ValueError):
            outline.join_polygon((10**1000, 0), (1, 0), (0, 1), 1, 'clip')
        self.assertAlmostEqual(rule.miter_ratio(60), 2)
        self.assertAlmostEqual(rule.miter_reach(8, 60), 16)
        self.assertAlmostEqual(rule.bevel_reach(8, 60), 4)
        self.assertEqual(rule.round_reach(8), 8)
        self.assertEqual(rule.clipped_reach(1e200, 1e-200, 2), 2e200)
        for angle in (0, -1, 181, float('nan'), float('inf')):
            for function in (rule.miter_ratio, rule.is_clamped):
                with self.assertRaises(ValueError):
                    function(angle)
        for h in (0, -1, float('nan'), float('inf')):
            for function in (rule.miter_reach, rule.bevel_reach, rule.round_reach,
                             rule.clipped_reach, rule.native_reach):
                with self.assertRaises(ValueError):
                    function(h, 30)
        for function in (rule.miter_reach, rule.clipped_reach):
            with self.assertRaises(ValueError):
                function(1.7e308, 30)
        for ratio in (0.5, float('nan'), float('inf')):
            with self.assertRaises(ValueError):
                rule.is_clamped(30, ratio)
        self.assertAlmostEqual(rule.interior_angle((1, 0), (1, 0)), 180)
        with self.assertRaises(ValueError):
            rule.interior_angle((0, 0), (1, 0))

    def test_experimental_flattening_uses_chords_for_join_geometry(self):
        # This records a limitation, not an exact Bezier offset guarantee.
        pts = [{'a': [0, 0], 'l': [-1, 0], 'r': [0, 1]},
               {'a': [3, 3], 'l': [2, 3], 'r': [3, 3]},
               {'a': [-3, -2], 'l': [-3, -2], 'r': [-3, -2]}]
        flat = outline.flatten(pts)
        tangent_u, tangent_w = outline.tangents(pts, 0)
        chord_w = outline.unit((flat[1][0]-flat[0][0], flat[1][1]-flat[0][1]))
        self.assertGreater(abs(chord_w[0]-tangent_w[0]), 0.01)
        self.assertTrue(flat[0][2])
        self.assertEqual(outline.SUB, 24)
        self.assertIsNone(outline.unit((0, 0)))


    def test_decimal_radical_oracle_at_30_degrees(self):
        # 80-digit radicals give sin/cos(15 degrees), independent of binary trig.
        from decimal import Decimal, localcontext
        with localcontext() as context:
            context.prec = 80
            two, three, h = Decimal(2), Decimal(3), Decimal(8)
            s, c = (two-three.sqrt()).sqrt()/two, (two+three.sqrt()).sqrt()/two
            cap = h*two.sqrt()
            x = (h-s*cap)/c
            expected = [(-float(h*c), float(h*s)), (-float(x), float(cap)),
                        (float(x), float(cap)), (float(h*c), float(h*s))]
            u, w = (float(s), float(c)), (float(s), -float(c))
        self.verifyCases([((0, 0), u, w, 8, 'clip', math.sqrt(2), 4, expected)])

    def test_path_reversal_and_nearly_straight_join(self):
        cases = []
        for angle in (0.0001, 30, 90, 150, 179.99999999999):
            u, w, expected = wedge(angle)
            cases.append(((0, 0), (-w[0], -w[1]), (-u[0], -u[1]), 1,
                          'clip', math.sqrt(2), 4, expected[::-1]))
        self.verifyCases(cases)

    def test_js_scalar_validation_and_nonfinite_numbers(self):
        requests = [{'method': 'miterRatio', 'args': [x]} for x in (0, -1, 181, None, '30', True)]
        for value in ('NaN', 'Infinity', '-Infinity'):
            requests.append({'method': 'miterRatio', 'args': [0], 'replace': [[0]], 'number': value})
            requests.append({'args': [0, 0, [1, 0], [0, 1], 1, 'clip', 2, 4],
                             'replace': [[2, 0]], 'number': value})
        for result in js_calls(requests):
            self.assertIn('error', result)
        result = js_calls([{'method': 'miterRatio', 'args': [60]},
                           {'method': 'interiorAngle', 'args': [[1, 0], [0, 1]]}])
        self.assertAlmostEqual(result[0]['value'], 2)
        self.assertAlmostEqual(result[1]['value'], 90)


    def test_local_clip_cannot_guarantee_surrounding_envelope(self):
        from render_local_envelope import cases
        contained, overflow = cases()
        self.assertEqual(contained['clip_boundary'], overflow['clip_boundary'])
        self.assertFalse(contained['exceeds_outer_envelope'])
        self.assertTrue(overflow['exceeds_outer_envelope'])
        self.assertAlmostEqual(contained['signed_axial_excess'], -0.2786796564403571)
        self.assertAlmostEqual(overflow['signed_axial_excess'], 0.3213203435596428)
        for row in (contained, overflow):
            left, bottom, right, top = row['outer_rectangle']
            for x, y in row['inner_triangle']:
                self.assertTrue(left < x < right and bottom < y < top)
            cap = 1.5*math.sqrt(2)
            envelope = row['gap']+1.5
            self.assertAlmostEqual(max(p[1] for p in row['clip_boundary']), cap)
            self.assertEqual(cap > envelope, row['exceeds_outer_envelope'])


if __name__ == '__main__':
    unittest.main()
