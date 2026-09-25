"""tools/record_routew_census.py frame attribution and paint order; no PDF or Adobe host."""
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import record_routew_census as r  # noqa: E402

FRAMES = [{'key': 'A', 'bounds': [20, 20, 185, 190]}, {'key': 'B', 'bounds': [20, 215, 185, 385]}]
PAGE = SimpleNamespace(width=800, height=1000)


class FrameOrder(unittest.TestCase):
    def test_strokes_then_fills(self):
        self.assertEqual(r.frame_order([('stroke', 24), ('stroke', 24), ('fill', None), ('fill', None)]), ([24, 24], []))

    def test_stroke_after_a_fill_is_reported(self):
        self.assertEqual(r.frame_order([('stroke', 24), ('fill', None), ('stroke', 5), ('fill', None)]), ([24], [5]))

    def test_no_fill(self):
        self.assertEqual(r.frame_order([('stroke', 1)]), ([1], []))


class FrameOf(unittest.TestCase):
    def test_center_inside_a_frame(self):
        self.assertEqual(r.frame_of((30, 30, 60, 60), FRAMES, PAGE), 'A')
        self.assertEqual(r.frame_of((300, 100, 320, 120), FRAMES, PAGE), 'B')

    def test_page_background_belongs_to_no_frame(self):
        self.assertIsNone(r.frame_of((0, 0, 800, 1000), FRAMES, PAGE))

    def test_unmatched_event_is_flagged(self):
        self.assertEqual(r.frame_of((600, 900, 620, 920), FRAMES, PAGE), '?')


if __name__ == '__main__':
    unittest.main()
