"""tools/measure_stroke_order.py metrics on synthetic images; no PDF or Adobe host."""
import sys
import unittest
from pathlib import Path

import numpy

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import measure_stroke_order as m  # noqa: E402


def canvas():
    return numpy.full((60, 60), 255, dtype=numpy.uint8)


class StrokeOrderMetrics(unittest.TestCase):
    def setUp(self):
        self.fill = canvas()
        self.fill[20:40, 20:40] = 0  # one glyph fill
        # Native: a gray stroke band painted over the fill's right half and beyond.
        self.native = self.fill.copy()
        self.native[20:40, 30:50] = 150
        # Corrected: the same band, beneath the fill.
        self.corrected = canvas()
        self.corrected[20:40, 30:50] = 150
        self.corrected[20:40, 20:40] = 0

    def test_stroke_over_fill_is_counted_in_the_core_only(self):
        result = m.measure(self.fill, self.native, self.native, 3, 32, 250, 8)
        self.assertEqual(result['core_px'], 14 * 14)
        self.assertEqual(result['core_damaged_px'], 14 * 7)  # core columns 30..36 of 23..36
        self.assertEqual(result['ink_xor_px'], 0)

    def test_corrected_order_has_no_damage_and_the_same_coverage(self):
        result = m.measure(self.fill, self.native, self.corrected, 3, 32, 250, 8)
        self.assertEqual(result['core_damaged_px'], 0)
        self.assertEqual(result['ink_xor_px'], 0)
        self.assertEqual(result['outside_changed_px'], 0)

    def test_a_lost_stroke_shows_as_ink_and_outside_change(self):
        lost = self.corrected.copy()
        lost[20:40, 40:50] = 255
        result = m.measure(self.fill, self.native, lost, 3, 32, 250, 8)
        self.assertEqual(result['core_damaged_px'], 0)
        self.assertEqual(result['ink_xor_px'], 200)
        self.assertGreater(result['outside_changed_px'], 0)

    def test_paint_sequence_order(self):
        self.assertTrue(m.strokes_before_fills(['S', 'S', 'f', 'f']))
        self.assertFalse(m.strokes_before_fills(['f', 'S', 'f', 'S']))
        self.assertFalse(m.strokes_before_fills(['BT', 'Tj', 'Tr1', 'Tj', 'ET']))
        self.assertFalse(m.strokes_before_fills(['f', 'f']))


if __name__ == '__main__':
    unittest.main()
