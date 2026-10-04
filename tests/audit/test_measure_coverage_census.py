"""Pure tests for tools/measure_coverage_census.py: escaping ink, stray stroke, holes, and beveled
points, on synthetic renders of a square and a thorn. No Adobe, no PDF."""
import sys
import unittest
from pathlib import Path

import numpy as np
from scipy import ndimage

TOOLS = Path(__file__).resolve().parents[2] / 'tools'
sys.path.insert(0, str(TOOLS))
import measure_coverage_census as m  # noqa: E402

Z = 8
CELL = [0, 0, 40, 40]   # y0, x0, y1, x1 in points
LEVELS = [m.BACKDROP, m.FILL, m.STROKE]


def square(x0, y0, x1, y1):
    pts = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    return [('l', pts[i], pts[(i + 1) % 4]) for i in range(4)]


def raster(cs):
    """The geometry render: Black where the contours fill, white elsewhere."""
    from PIL import Image, ImageDraw
    img = Image.new('L', (40 * Z, 40 * Z), 255)
    d = ImageDraw.Draw(img)
    for c in cs:
        d.polygon([(s[1][0] * Z, s[1][1] * Z) for s in c], fill=0)
    return np.array(img).astype(np.int16)


def outside_stroke(geo, h):
    """An Outside stroke drawn exactly: the fill, and the band within h outside it, round joins."""
    fill = geo < 128
    band = ~fill & (ndimage.distance_transform_edt(~fill) <= h * Z)
    img = np.full(geo.shape, m.BACKDROP, np.int16)
    img[band] = m.STROKE
    img[fill] = m.FILL
    return img


def parts(cs):
    return [{'part': 'shape', 'geometryBounds': [5, 5, 35, 35]}]


class Census(unittest.TestCase):
    def setUp(self):
        self.cs = [square(10, 10, 30, 30)]
        self.geo = raster(self.cs)
        self.exact = outside_stroke(self.geo, 2)

    def measure(self, native, corrected=None, align='outside', w=2):
        return m.measure(native, self.exact if corrected is None else corrected, self.geo, self.cs, CELL, Z, align, w, LEVELS, parts(self.cs))

    def test_an_exact_stroke_has_no_defects(self):
        r = self.measure(self.exact)['all']['native']
        self.assertEqual((r['escaping'], r['stray'], r['holes']), (0, 0, 0))

    def test_a_spike_beyond_the_band_is_escaping(self):
        n = self.exact.copy()
        n[20 * Z:20 * Z + 4, 33 * Z:38 * Z] = m.STROKE   # 5 pt long, 0.5 pt wide, 3 pt past the band
        r = self.measure(n)['all']
        self.assertGreater(r['native']['escaping'], 1.0)
        self.assertEqual(r['corrected']['escaping'], 0)

    def test_exterior_miters_are_allowed(self):
        # The square's corners mitered at limit 1.414: the miter's tip lies outside the round band.
        n = self.exact.copy()
        fill = self.geo < 128
        yy, xx = np.mgrid[0:n.shape[0], 0:n.shape[1]] / Z
        n[(np.maximum(np.abs(xx - 20), np.abs(yy - 20)) <= 12) & ~fill] = m.STROKE
        self.assertEqual(self.measure(n)['all']['native']['escaping'], 0)

    def test_stroke_inside_an_outside_fill_is_stray_and_backdrop_is_a_hole(self):
        n = self.exact.copy()
        n[15 * Z:17 * Z, 15 * Z:25 * Z] = m.STROKE
        n[22 * Z:24 * Z, 15 * Z:25 * Z] = m.BACKDROP
        r = self.measure(n)['all']['native']
        self.assertAlmostEqual(r['stray'], 20, delta=1)
        self.assertAlmostEqual(r['holes'], 20, delta=1)

    def test_inside_stroke_outside_the_shape_is_escaping(self):
        fill = self.geo < 128
        n = np.full(self.geo.shape, m.BACKDROP, np.int16)
        n[fill] = m.FILL
        n[fill & (ndimage.distance_transform_edt(fill) <= 2 * Z)] = m.STROKE
        self.assertEqual(self.measure(n, n, 'inside')['all']['native']['escaping'], 0)
        n[(~fill) & (ndimage.distance_transform_edt(~fill) <= 1 * Z)] = m.STROKE   # 1 pt spilled outside
        self.assertGreater(self.measure(n, n, 'inside')['all']['native']['escaping'], 50)

    def test_a_beveled_point_is_missing_and_a_whole_one_is_not(self):
        # A 10-degree thorn: its exterior tip is past limit 4, and its whole point (1 / sin 5 deg
        # half widths, 11.5 pt past the tip) is shorter than its sides.
        tip = [('l', (20, 14), (21.75, 34)), ('l', (21.75, 34), (18.25, 34)), ('l', (18.25, 34), (20, 14))]
        self.cs = [tip]
        self.geo = raster(self.cs)
        beveled = outside_stroke(self.geo, 1)
        r = m.measure(beveled, beveled, self.geo, self.cs, CELL, Z, 'outside', 1, LEVELS, parts(self.cs))
        self.assertEqual(r['points_past_limit'], 1)
        self.assertGreater(r['all']['native']['points_missing'], 1.0)
        whole = beveled.copy()
        yy, xx = np.mgrid[0:whole.shape[0], 0:whole.shape[1]] / Z
        apex = 14 - 1 / np.sin(np.radians(5))
        whole[(yy < 14) & (yy > apex) & (np.abs(xx - 20) < (yy - apex) * np.tan(np.radians(5)) + 0.2)] = m.STROKE
        r = m.measure(whole, whole, self.geo, self.cs, CELL, Z, 'outside', 1, LEVELS, parts(self.cs))
        self.assertLess(r['all']['native']['points_missing'], 0.1)


if __name__ == '__main__':
    unittest.main()
