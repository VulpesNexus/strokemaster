"""Unit tests for tools/measure_miter_sliver.py. No PDF or renderer is used."""
import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import measure_miter_sliver as ms  # noqa: E402


def wedge_strips(eps, scale=100):
    """Gray strips for the top edge: control ink below y = 1.5, plus an exact wedge eps(30-x)/30
    above it in `near`, with anti-aliased coverage in the partial pixel of each column."""
    box = ms.strip_box(1)
    w, h = round((box[1] - box[0]) * scale), round((box[3] - box[2]) * scale)
    x = box[0] + (np.arange(w) + 0.5) / scale
    y_top = box[3] - np.arange(h) / scale                 # top edge of each row
    def coverage(height):
        lo, hi = y_top - 1 / scale, y_top                  # row extent
        top = 1.5 + np.where((x >= 0) & (x <= 30), height, 0.0)
        return np.clip((np.minimum(hi[:, None], top[None, :]) - lo[:, None]) * scale, 0, 1) * \
            ((x >= 0) & (x <= 30))[None, :]
    control = coverage(np.zeros_like(x))
    near = coverage(np.clip(eps * (30 - x) / 30, 0, None))
    gray = lambda c: np.round(255 * (1 - c)).astype(np.uint8)
    return gray(near), gray(control), box, scale


class Sliver(unittest.TestCase):
    def test_expected_area_is_on_the_side_of_eps(self):
        self.assertAlmostEqual(ms.expected_sliver(0.01, 1), 0.15)
        self.assertEqual(ms.expected_sliver(0.01, -1), 0.0)
        self.assertAlmostEqual(ms.expected_sliver(-0.001, -1), 0.015)

    def test_strips_straddle_each_edge(self):
        self.assertEqual(ms.strip_box(1)[2:], [1.0, 2.0])
        self.assertEqual(ms.strip_box(-1)[2:], [-2.0, -1.0])

    def test_exact_wedge_is_recovered(self):
        near, control, box, scale = wedge_strips(0.01)
        r = ms.analyze(near, control, scale, box)
        self.assertAlmostEqual(r['area'], 0.15, delta=0.002)
        self.assertAlmostEqual(r['fitHeightAtX0'], 0.01, delta=0.0005)
        self.assertAlmostEqual(r['fitHeightAtX30'], 0.0, delta=0.0005)
        self.assertAlmostEqual(r['controlInkThickness'], 0.5, delta=0.001)
        self.assertTrue(ms.judge(r, 0.01, 1)['pass'])

    def test_missing_wedge_fails(self):
        near, control, box, scale = wedge_strips(0.01)
        r = ms.analyze(control, control, scale, box)
        self.assertFalse(ms.judge(r, 0.01, 1)['pass'])

    def test_doubled_wedge_fails(self):
        near, control, box, scale = wedge_strips(0.02)
        self.assertFalse(ms.judge(ms.analyze(near, control, scale, box), 0.01, 1)['pass'])

    def test_blank_strips_are_misframed_not_sliverless(self):
        # Ghostscript's first run: a wrong PageOffset rendered blank strips, which read as
        # "no sliver" and passed on the side where none is expected.
        blank = np.full((100, 3210), 255, np.uint8)
        r = ms.analyze(blank, blank, 100, ms.strip_box(-1))
        out = ms.judge(r, 0.01, -1)  # eps > 0: no sliver expected on the bottom edge
        self.assertFalse(out['framed'])
        self.assertFalse(out['pass'])

    def test_shape_mismatch_is_refused(self):
        with self.assertRaises(ValueError):
            ms.analyze(np.zeros((2, 3), np.uint8), np.zeros((3, 3), np.uint8), 100, ms.strip_box(1))


if __name__ == '__main__':
    unittest.main()
