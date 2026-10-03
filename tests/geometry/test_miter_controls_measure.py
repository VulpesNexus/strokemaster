"""Unit tests for tools/measure_miter_controls.py. No Adobe host, PDF, or renderer is used."""
import math
import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import measure_miter_controls as mm  # noqa: E402


def case(kind, join, cap, points, closed=False, epsilon=None):
    return {'id': 'T', 'kind': kind, 'join': join, 'cap': cap, 'points': points, 'closed': closed,
            'epsilon': epsilon, 'width': 3, 'limit': 4}


def capsule_area(step=0.01):
    """Grid area of every point within 1.5 pt of the segment (0,0)-(30,0): an independent oracle
    for round caps, round reversal joins, and their combinations."""
    xs = np.arange(-2, 32, step) + step / 2
    ys = np.arange(-2, 2, step) + step / 2
    x, y = np.meshgrid(xs, ys)
    dx = np.clip(x, 0, 30) - x
    return float(((dx * dx + y * y) <= 1.5 ** 2).sum() * step * step)


class Expectations(unittest.TestCase):
    def test_butt_segment_is_a_rectangle(self):
        e = mm.expected_ink(case('segment', 'miter', 'butt', [[0, 0], [30, 0]]))
        self.assertAlmostEqual(e['area'], 90.0)
        self.assertEqual(e['bounds'], [0.0, 30.0, -1.5, 1.5])

    def test_round_cases_match_a_capsule_oracle(self):
        oracle = capsule_area()
        for c in (case('segment', 'miter', 'round', [[0, 0], [30, 0]]),
                  case('exact-uturn', 'round', 'round', [[0, 0], [30, 0], [0, 0]]),
                  case('closed-two-anchor', 'round', 'butt', [[0, 0], [30, 0]], closed=True)):
            self.assertAlmostEqual(mm.expected_ink(c)['area'], oracle, delta=0.02, msg=c['kind'])

    def test_reversal_without_round_join_adds_no_miter(self):
        for join in ('miter', 'bevel'):
            e = mm.expected_ink(case('exact-uturn', join, 'butt', [[0, 0], [30, 0], [0, 0]]))
            self.assertAlmostEqual(e['area'], 90.0)
            self.assertEqual(e['bounds'][1], 30.0)

    def test_right_angle_joins(self):
        areas = {j: mm.expected_ink(case('right-angle', j, 'butt', [[0, -20], [0, 0], [20, 0]]))['area']
                 for j in ('miter', 'bevel', 'round')}
        self.assertAlmostEqual(areas['miter'], 120.0)
        self.assertAlmostEqual(areas['bevel'], 118.875)
        self.assertAlmostEqual(areas['round'], 117.75 + math.pi * 2.25 / 4)

    def test_outline_lengths(self):
        r = 1.5
        self.assertAlmostEqual(mm.expected_ink(case('segment', 'miter', 'butt', [[0, 0], [30, 0]]))['perimeter'], 66.0)
        self.assertAlmostEqual(mm.expected_ink(case('segment', 'miter', 'projecting', [[0, 0], [30, 0]]))['perimeter'], 72.0)
        self.assertAlmostEqual(mm.expected_ink(case('closed-two-anchor', 'round', 'butt', [[0, 0], [30, 0]], True))['perimeter'],
                               60 + 2 * math.pi * r)
        corner = {j: mm.expected_ink(case('right-angle', j, 'butt', [[0, -20], [0, 0], [20, 0]]))['perimeter']
                  for j in ('miter', 'bevel', 'round')}
        self.assertAlmostEqual(corner['miter'], 86.0)  # an L-shape's outline equals its 21.5 pt square's
        self.assertAlmostEqual(corner['bevel'], 83 + r * math.sqrt(2))
        self.assertAlmostEqual(corner['round'], 83 + math.pi * r / 2)

    def test_near_uturn_sliver_and_bounds_follow_the_sign(self):
        up = mm.expected_ink(case('near-uturn', 'miter', 'butt', [[0, 0], [30, 0], [0, 0.01]], epsilon=0.01))
        down = mm.expected_ink(case('near-uturn', 'miter', 'butt', [[0, 0], [30, 0], [0, -0.01]], epsilon=-0.01))
        self.assertAlmostEqual(up['area'], 90.15)
        self.assertAlmostEqual(up['bounds'][3], 1.51)
        self.assertAlmostEqual(down['bounds'][2], -1.51)


class Operators(unittest.TestCase):
    # Page 3 of the 2026-10-03 run: closed two-anchor contour, InDesign's exact operators.
    OPS = [('w', [3.0]), ('M', [4.0]), ('gs', ['/GS0']), ('q', []), ('cm', [1, 0, 0, 1, 35, 50]),
           ('m', [0, 0]), ('l', [30, 0]), ('l', [0, 0]), ('h', []), ('S', []), ('Q', [])]

    def test_closed_contour_with_written_return_matches(self):
        events = mm.stroke_events(self.OPS, {'/GS0': {'LJ': 0, 'LC': 0}})
        c = case('closed-two-anchor', 'miter', 'butt', [[0, 0], [30, 0]], closed=True)
        out = mm.compare_operators(c, events)
        self.assertTrue(out['match'], out)
        self.assertTrue(out['closingSegmentWritten'])

    def test_extgstate_join_is_compared(self):
        events = mm.stroke_events(self.OPS, {'/GS0': {'LJ': 1, 'LC': 0}})
        c = case('closed-two-anchor', 'miter', 'butt', [[0, 0], [30, 0]], closed=True)
        self.assertFalse(mm.compare_operators(c, events)['match'])

    def test_state_is_restored_by_Q(self):
        ops = [('q', []), ('j', [1]), ('Q', []), ('m', [35, 50]), ('l', [65, 50]), ('S', [])]
        self.assertEqual(mm.stroke_events(ops, {})[0]['join'], 0)

    def test_open_path_must_not_be_closed(self):
        ops = [('j', [0]), ('J', [0]), ('w', [3]), ('M', [4]), ('m', [35, 50]), ('l', [65, 50]), ('s', [])]
        c = case('segment', 'miter', 'butt', [[0, 0], [30, 0]])
        self.assertFalse(mm.compare_operators(c, mm.stroke_events(ops, {}))['match'])


class Coverage(unittest.TestCase):
    def test_pixel_aligned_rectangle(self):
        dpi = 72  # one pixel per point
        img = np.full((100, 100), 255, dtype=np.uint8)
        img[100 - 52:100 - 49, 35:65] = 0  # specimen x 0..30, y -1..2
        m = mm.measure_coverage(img, dpi)
        self.assertAlmostEqual(m['area'], 90.0)
        self.assertEqual(m['bounds'], [0.0, 30.0, -1.0, 2.0])

    def test_half_coverage_counts_area_but_not_bounds_below_half(self):
        img = np.full((100, 100), 255, dtype=np.uint8)
        img[50, 35:45] = 0
        img[51, 35:45] = 160  # 37% coverage, below the bounds threshold
        m = mm.measure_coverage(img, 72)
        self.assertAlmostEqual(m['area'], 10 + 10 * (95 / 255))
        self.assertEqual(m['bounds'][2], -1.0)

    def test_judge_uses_fixed_tolerances(self):
        e = {'area': 90.0, 'bounds': [0, 30, -1.5, 1.5]}
        self.assertTrue(mm.judge({'area': 90.4, 'bounds': [0.1, 30, -1.5, 1.5]}, e)['pass'])
        self.assertFalse(mm.judge({'area': 90.6, 'bounds': [0, 30, -1.5, 1.5]}, e)['pass'])
        self.assertFalse(mm.judge({'area': 90.0, 'bounds': [0, 30.2, -1.5, 1.5]}, e)['pass'])


if __name__ == '__main__':
    unittest.main()
