"""Pure tests for the route W certification measurements (tools/routew_measure.py and the
preference gate in tools/certify_routew.py). Synthetic arrays and a generated PDF; no Adobe."""
import json
import sys
import tempfile
import unittest
from pathlib import Path

import numpy
import pymupdf

TOOLS = Path(__file__).resolve().parents[2] / 'tools'
sys.path.insert(0, str(TOOLS))
import certify_routew  # noqa: E402
import routew_measure as m  # noqa: E402

PPI = 72  # one device pixel per point keeps the geometry readable


def frame(key, top, left, bottom, right):
    return {'key': key, 'bounds': [top, left, bottom, right]}


class CoreDamage(unittest.TestCase):
    def setUp(self):
        self.mask = numpy.zeros((100, 200), numpy.uint8)
        self.mask[20:60, 20:60] = 255     # a fill in frame A
        self.mask[20:60, 120:160] = 255   # a fill in frame B
        self.core = m.core_of(self.mask, PPI)
        self.frames = [frame('A', 0, 0, 100, 100), frame('B', 0, 100, 100, 200)]

    def test_erosion_scales_with_resolution(self):
        self.assertEqual(m.erode_px(600), 6)
        self.assertEqual(m.erode_px(72), 1)
        self.assertEqual(m.erode_px(1200), 12)

    def test_counts_only_large_differences_inside_the_core(self):
        twin = numpy.full((100, 200), 30, numpy.uint8)
        out = twin.copy()
        out[30:40, 30:40] = 255        # stroke over fill A: 100 core pixels
        out[30:40, 130:140] = 60       # a 30-level difference in B: color conversion, not damage
        out[0:5, 0:5] = 255            # outside every core
        r = m.core_damage(out, twin, self.core, self.frames, PPI)
        rows = {f['key']: f for f in r['frames']}
        self.assertEqual(rows['A']['damage_px'], 100)
        self.assertEqual(rows['B']['damage_px'], 0)
        self.assertEqual(rows['B']['core_max_diff'], 30)
        self.assertEqual(r['damage_px_total'], 100)

    def test_sizes_are_trimmed_to_the_common_area(self):
        twin = numpy.full((101, 200), 30, numpy.uint8)
        out = numpy.full((100, 201), 30, numpy.uint8)
        r = m.core_damage(out, twin, self.core, self.frames, PPI)
        self.assertEqual(r['damage_px_total'], 0)
        self.assertEqual(r['trimmed_px'], 1)

    def test_a_frame_without_bounds_is_reported_not_skipped(self):
        r = m.core_damage(self.mask, self.mask, self.core, [{'key': 'X', 'buildError': 'failed'}], PPI)
        self.assertEqual(r['frames'][0], {'key': 'X', 'error': 'failed'})


class EdgeMetrics(unittest.TestCase):
    def square(self, size, extra=None):
        a = numpy.full((120, 120), 128, numpy.uint8)
        a[30:30 + size, 30:30 + size] = 255
        if extra:
            a[extra] = 0
        return a

    def test_a_one_pixel_edge_shift_stays_in_the_band(self):
        ref, out = self.square(40), self.square(41)
        row = m.edge_metrics(out, ref, [frame('F', 20, 20, 100, 100)], PPI, backdrop=128)['frames'][0]
        self.assertGreater(row['px_over_64'], 0)
        self.assertEqual(row['over_64_beyond_band'], 0)
        self.assertEqual(row['over_16_by_edge_distance_px']['3'], 0)

    def test_a_change_far_from_any_edge_is_semantic(self):
        ref = self.square(40)
        out = self.square(40, (slice(45, 55), slice(45, 55)))
        row = m.edge_metrics(out, ref, [frame('F', 20, 20, 100, 100)], PPI, backdrop=128)['frames'][0]
        self.assertGreater(row['over_64_beyond_band'], 0)

    def test_white_edge_offset_measures_a_one_pixel_growth(self):
        ref, out = self.square(40), self.square(42)  # grows by two pixels on two sides
        off = m.white_boundary_offset(out.astype(numpy.uint8), ref, 128)
        # A 40 px square grown to 42 px from one corner: area +164 px over a 160 px perimeter. The
        # band is a 4-connected dilation, so the one diagonal corner pixel outside it is not counted.
        self.assertLessEqual(abs(off['offset_px'] * 160 - 164), 1)
        self.assertEqual(off['edge_length_px'], 160)


class PdfPaintOrder(unittest.TestCase):
    def pdf(self, stroke_first):
        doc = pymupdf.open()
        page = doc.new_page(width=300, height=200)
        def stroke():
            page.draw_rect(pymupdf.Rect(40, 40, 120, 90), color=(1, 0, 0), width=6)
        if stroke_first:
            stroke()
        page.insert_text((50, 70), 'HH', fontsize=24, color=(0, 0, 0))
        if not stroke_first:
            stroke()
        folder = tempfile.mkdtemp()
        path = str(Path(folder) / 'p.pdf')
        doc.save(path)
        return path

    def test_stroke_before_fill_text_is_recognized(self):
        frames = [frame('F', 30, 30, 100, 130)]
        s = m.pdf_structure(self.pdf(True), frames)
        f = s['frames'][0]
        self.assertEqual(f['glyphs'], {'text-fill': 2})
        self.assertEqual(f['strokes_before_first_fill_text'], 1)
        self.assertEqual(f['stroke_after_fill_text'], [])
        self.assertEqual(s['images'], 0)

    def test_stroke_after_fill_text_is_reported(self):
        f = m.pdf_structure(self.pdf(False), [frame('F', 30, 30, 100, 130)])['frames'][0]
        self.assertEqual(f['stroke_after_fill_text'], [6.0])

    def test_page_sized_art_belongs_to_no_frame(self):
        page = pymupdf.open(self.pdf(True))[0]
        self.assertIsNone(m.frame_of((0, 0, 300, 200), [frame('F', 30, 30, 100, 130)], page.rect))
        self.assertEqual(m.frame_of((200, 150, 210, 160), [frame('F', 30, 30, 100, 130)], page.rect), '?')


class PreferenceGate(unittest.TestCase):
    def write(self, folder, name, props, presets=('[High Quality Print]',)):
        path = Path(folder) / name
        path.write_text(json.dumps({'props': props, 'pdfPresets': [{'name': p} for p in presets], 'flattenerPresets': []}), encoding='utf-8')
        return path

    def test_unchanged_and_changed(self):
        with tempfile.TemporaryDirectory() as d:
            a = self.write(d, 'a.json', {'pdfExportPreferences': ['acrobatCompatibility=ACROBAT_6', 'preserveInDesignEditingCapabilities=false']})
            same = self.write(d, 'b.json', {'pdfExportPreferences': ['acrobatCompatibility=ACROBAT_6', 'preserveInDesignEditingCapabilities=false']})
            leaked = self.write(d, 'c.json', {'pdfExportPreferences': ['acrobatCompatibility=ACROBAT_5', 'preserveInDesignEditingCapabilities=true']})
            self.assertEqual(certify_routew.prefs_diff(a, same), [])
            self.assertEqual(len(certify_routew.prefs_diff(a, leaked)), 2)

    def test_a_changed_preset_list_fails_the_gate(self):
        with tempfile.TemporaryDirectory() as d:
            a = self.write(d, 'a.json', {}, presets=('[High Quality Print]',))
            b = self.write(d, 'b.json', {}, presets=('[High Quality Print]', 'cert tmp'))
            self.assertEqual(certify_routew.prefs_diff(a, b), [('pdfExportPresets', 'list', 'list changed')])


if __name__ == '__main__':
    unittest.main()
