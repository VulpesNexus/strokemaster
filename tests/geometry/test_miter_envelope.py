"""Unit tests for tools/measure_miter_envelope.py. No Adobe host; MuPDF renders a synthetic stroke."""
import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import measure_miter_envelope as me  # noqa: E402


class Paths(unittest.TestCase):
    def test_stroke_paths_place_contours_on_the_page(self):
        ops = [('w', [8]), ('M', [4]), ('q', []), ('cm', [1, 0, 0, 1, 48.732, 677.2284]),
               ('m', [0, 0]), ('l', [10, 0]), ('c', [10, 5, 5, 10, 0, 10]), ('h', []), ('S', []), ('Q', [])]
        ev = me.stroke_paths(ops)[0]
        self.assertEqual((ev['width'], ev['limit'], ev['join']), (8.0, 4.0, 0))
        start, segs = ev['contours'][0]
        self.assertEqual(start, (48.732, 677.2284))
        self.assertEqual(segs[0], ('L', (58.732, 677.2284)))
        self.assertEqual(segs[1][0], 'C')

    def test_a_fill_or_clip_discards_its_path(self):
        ops = [('m', [0, 0]), ('l', [1, 0]), ('l', [1, 1]), ('h', []), ('n', []),
               ('m', [5, 5]), ('l', [6, 5]), ('l', [6, 6]), ('h', []), ('S', [])]
        ev = me.stroke_paths(ops)
        self.assertEqual(len(ev), 1)
        self.assertEqual(ev[0]['contours'][0][0], (5.0, 5.0))

    def test_frame_lookup_uses_indesign_top_down_bounds(self):
        frames = [{'name': 'a', 'bounds': [220, 24, 264, 300]}, {'name': 'b', 'bounds': [270, 312, 314, 588]}]
        contours = [((30.0, 650.0), [('L', (60.0, 650.0)), ('L', (60.0, 670.0))])]
        self.assertEqual(me.frame_for(contours, frames, 900)['name'], 'a')


class Rasters(unittest.TestCase):
    def test_holes_are_cleared(self):
        square = [[(0, 0), (10, 0), (10, 10), (0, 10), (0, 0)], [(4, 4), (6, 4), (6, 6), (4, 6), (4, 4)]]
        mask = me.raster([square], (0, 0, 10, 10), 10, (100, 100))
        self.assertAlmostEqual(mask.sum() / 100.0, 96.0, delta=1.0)
        self.assertFalse(mask[50, 50])

    def test_components_report_area_and_box(self):
        ink = np.zeros((100, 100))
        ink[10:20, 30:40] = 1.0
        c = me.components(ink, (0, 0, 10, 10), 10)
        self.assertEqual(len(c), 1)
        self.assertAlmostEqual(c[0]['area_pt2'], 1.0)
        self.assertEqual([round(v, 6) for v in c[0]['box_pt']], [3.0, 8.0, 4.0, 9.0])

    def test_isolated_stroke_renders_where_the_raster_expects(self):
        # A 2 pt stroke along x = 1..9 at y = 5 near the page's left edge. The render and the
        # envelope raster must share an origin: an unclamped negative crop shifted them by 6 pt.
        import pymupdf
        ev = {'width': 2.0, 'limit': 4.0, 'join': 0, 'contours': [((1.0, 5.0), [('L', (9.0, 5.0))])]}
        doc = me.isolated_stroke(ev, (20, 20))
        scale = 10
        box = (0.0, 0.0, 20.0, 20.0)
        pix = doc[0].get_pixmap(matrix=pymupdf.Matrix(scale, scale), clip=pymupdf.Rect(box[0], 20 - box[3], box[2], 20 - box[1]),
                                colorspace=pymupdf.csGRAY, alpha=False)
        ink = 1.0 - np.frombuffer(pix.samples, dtype=np.uint8).reshape(pix.height, pix.width) / 255.0
        bar = me.raster([[[(1, 4), (9, 4), (9, 6), (1, 6), (1, 4)]]], box, scale, ink.shape)
        self.assertGreater((ink * bar).sum() / max(ink.sum(), 1e-9), 0.97)


if __name__ == '__main__':
    unittest.main()
