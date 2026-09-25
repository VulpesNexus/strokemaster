"""Split fill/stroke control paint in tools/measure_region_pdf.py, on synthetic PDFs.

Illustrator 30.7.0 exports a filled and stroked compound as f* followed by S
(region-prep-7 host PDFs). These PDFs reproduce that operator shape exactly,
with the analytic fixture's source geometry, so no Adobe host is needed.
"""
import sys
import tempfile
import unittest
from pathlib import Path

import pikepdf

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import measure_region_pdf as m  # noqa: E402

SOURCE = b'20 20 10 10 re 20.2 25 m 22.2 24 l 22.2 26 l h'


def write_pdf(folder, name, content):
    pdf = pikepdf.new()
    pdf.add_blank_page(page_size=(140, 140))
    page = pdf.pages[0]
    page.obj['/Contents'] = pdf.make_stream(content)
    path = Path(folder) / name
    pdf.save(path)
    return path


class SplitPaintControl(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)

    def measure(self, content, role='control'):
        path = write_pdf(self.folder.name, role + '.pdf', content)
        return m.measure_pdf(path, role, (20, 20), 1, gray_value=.25, tolerance=6e-4)

    def test_fill_then_stroke_of_one_path_is_measured_as_the_stroked_control(self):
        result = self.measure(b'0.25 g ' + SOURCE + b' f* 0.25 G 0.6 w 4 M 0 j ' + SOURCE + b' S')
        self.assertEqual(result['status'], 'PASS', result)
        self.assertTrue(result['split_fill_stroke_paint'])
        self.assertAlmostEqual(result['painted_min_x'], m.analytic_fixture()['control_min_x'], places=9)

    def test_single_combined_paint_still_measures(self):
        result = self.measure(b'0.25 g 0.25 G 0.6 w 4 M 0 j ' + SOURCE + b' B*')
        self.assertEqual(result['status'], 'PASS', result)
        self.assertFalse(result['split_fill_stroke_paint'])

    def test_split_paint_is_refused_for_the_effect(self):
        with self.assertRaisesRegex(m.Unsupported, 'control only'):
            self.measure(b'0.25 g ' + SOURCE + b' f* 0.25 G 0.6 w 4 M 0 j ' + SOURCE + b' S', role='effect')

    def test_stroke_before_fill_is_refused(self):
        with self.assertRaises(m.Unsupported):
            self.measure(b'0.25 G 0.6 w 4 M 0 j ' + SOURCE + b' S 0.25 g ' + SOURCE + b' f*')

    def test_split_paint_with_different_stroke_geometry_fails_the_geometry_checks(self):
        moved = b'20 20 10 10 re 20.2 25 m 22.2 24.5 l 22.2 26 l h'
        try:
            result = self.measure(b'0.25 g ' + SOURCE + b' f* 0.25 G 0.6 w 4 M 0 j ' + moved + b' S')
        except m.Unsupported:
            return
        self.assertNotEqual(result['status'], 'PASS', result)


if __name__ == '__main__':
    unittest.main()
