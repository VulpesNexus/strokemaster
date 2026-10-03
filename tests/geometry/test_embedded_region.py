"""Input/calibration guards for the bounded embedded-font research reader."""
from io import BytesIO
from pathlib import Path
import sys
import unittest
import tempfile
import subprocess
from unittest.mock import patch

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
import pikepdf

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import compare_embedded_region as reader
import measure_miter_calibration as calibration
import measure_miter_refinement as refinement


class EmbeddedRegionInputs(unittest.TestCase):
    def test_explicit_winansi_required(self):
        with self.assertRaisesRegex(ValueError, 'WinAnsi'):
            reader.load_font(pikepdf.Dictionary(Encoding=pikepdf.Name.StandardEncoding))

    def test_unsupported_font_does_not_guess(self):
        with self.assertRaisesRegex(ValueError, 'unsupported'):
            reader.load_font(pikepdf.Dictionary(Encoding=pikepdf.Name.WinAnsiEncoding))

    def test_empty_glyph_is_not_an_open_contour(self):
        p=reader.StrictContourPen({})
        self.assertEqual(p.contours, [])

    def test_open_contour_refused_instead_of_closed(self):
        p=reader.StrictContourPen({})
        p.moveTo((0, 0)); p.lineTo((1, 0))
        with self.assertRaisesRegex(ValueError, 'open glyph'):
            p.endPath()

    def test_closed_curve_is_retained(self):
        p=reader.StrictContourPen({})
        p.moveTo((0, 0)); p.curveTo((0, 1), (1, 1), (1, 0)); p.lineTo((0, 0)); p.closePath()
        self.assertEqual(len(p.contours), 1)
        self.assertEqual(p.contours[0][1][0][0], 'C')

    def test_nonfinite_and_nonpositive_calibration_fail_before_io(self):
        for value in (0, -1, float('nan'), float('inf')):
            for position in range(4):
                args=[12, 1.5, 0.01, 4]; args[position]=value
                with self.subTest(value=value, position=position), self.assertRaisesRegex(ValueError, 'calibration'):
                    reader.compare(Path('absent.pdf'), 'A', *args)

    def test_embedded_truetype_scaling_and_mapping_are_in_memory(self):
        builder=FontBuilder(1000, isTTF=True)
        builder.setupGlyphOrder(['.notdef', 'A'])
        glyphs={}
        for name in ['.notdef', 'A']:
            p=TTGlyphPen(None)
            if name=='A':
                p.moveTo((0, 0)); p.lineTo((100, 0)); p.lineTo((50, 100)); p.closePath()
            glyphs[name]=p.glyph()
        builder.setupGlyf(glyphs)
        builder.setupHorizontalMetrics({n:(500, 0) for n in glyphs})
        builder.setupHorizontalHeader(ascent=800, descent=-200)
        builder.setupCharacterMap({65:'A'})
        builder.setupNameTable({'familyName':'Synthetic', 'styleName':'Regular'})
        builder.setupOS2(); builder.setupPost(); builder.setupMaxp()
        out=BytesIO(); builder.save(out)
        with pikepdf.Pdf.new() as pdf:
            obj=pikepdf.Dictionary(Encoding=pikepdf.Name.WinAnsiEncoding,
                FontDescriptor=pikepdf.Dictionary(FontFile2=pdf.make_stream(out.getvalue())))
            glyphset, name, unit, data, kind=reader.load_font(obj)
            self.assertEqual(unit, 0.001); self.assertEqual(name('A'), 'A'); self.assertIsNone(name('B'))
            self.assertEqual(kind, 'TrueType'); self.assertEqual(data, out.getvalue())
            self.assertIn('A', glyphset)


class CalibrationAndRefinement(unittest.TestCase):
    def make_pdf(self, folder, content):
        path=Path(folder)/'synthetic.pdf'
        with pikepdf.Pdf.new() as pdf:
            page=pdf.add_blank_page(page_size=(100,100))
            page.Contents=pdf.make_stream(content.encode('ascii'))
            pdf.save(path)
        return path

    def test_limit_row_is_selected_and_tm_sets_actual_size(self):
        with tempfile.TemporaryDirectory() as folder:
            path=self.make_pdf(folder,'8 w 0 j BT /F1 1 Tf 1 Tr 1 M 36 0 0 36 0 0 Tm (A) Tj 4 M (A) Tj ET')
            r=calibration.first_run(path,'A',4)
            self.assertEqual(r['observed']['miter'],4)
            self.assertEqual(r['font_size_pt'],36)
            self.assertEqual(r['stroke_half_width_pt'],4)

    def test_nonidentity_stroke_transform_fails(self):
        with tempfile.TemporaryDirectory() as folder:
            path=self.make_pdf(folder,'2 0 0 2 0 0 cm BT /F1 1 Tf 1 Tr 12 0 0 12 0 0 Tm (A) Tj ET')
            with self.assertRaisesRegex(ValueError,'stroke transform'):
                calibration.first_run(path,'A')

    def test_nested_form_is_explicitly_refused(self):
        with tempfile.TemporaryDirectory() as folder:
            path=self.make_pdf(folder,'/Form1 Do')
            with self.assertRaisesRegex(ValueError,'XObject'):
                calibration.first_run(path,'A')

    def test_refinement_compares_both_bounds_and_area(self):
        a=[{'character':'A','symmetric_excess_area_pt2':1,'region_bounds_pt':[0,0,1,1],'symmetric_bounds_pt':[0,0,1,2]}]
        b=[{'character':'A','symmetric_excess_area_pt2':1.005,'region_bounds_pt':[0,0,1,1],'symmetric_bounds_pt':[0,0,1,2.1]}]
        area,bounds=refinement.deltas(a,b)
        self.assertLess(area,0.01);self.assertGreater(bounds,0.005)

    def test_refinement_does_not_pair_different_glyphs(self):
        with self.assertRaisesRegex(ValueError,'identity'):
            refinement.deltas([{'character':'A'}],[{'character':'B'}])

    def test_timeout_is_persisted_and_not_repeated_on_resume(self):
        with tempfile.TemporaryDirectory() as folder:
            out=Path(folder)/'runs'
            calls=[]
            def timed_out(*args):
                calls.append(1)
                raise subprocess.TimeoutExpired('synthetic-core',60)
            report=refinement.measure(out,timed_out)
            self.assertFalse(report['converged_on_corpus'])
            self.assertEqual(report['failure']['type'],'TimeoutExpired')
            self.assertTrue((out/'summary.json').exists())
            refinement.measure(out,timed_out)
            self.assertEqual(len(calls),1)


if __name__=='__main__':
    unittest.main()
