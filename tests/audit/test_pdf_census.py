"""PDF parser tests use synthetic streams, independent of StrokeMaster's old probe."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

import pikepdf

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('census', ROOT / 'tools/audit_pdf_evidence.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class GraphicsStateTests(unittest.TestCase):
    def measure(self, content, setup=None):
        with pikepdf.new() as pdf, tempfile.TemporaryDirectory() as tmp:
            page = pdf.add_blank_page()
            page.Resources = pikepdf.Dictionary()
            page.Contents = pdf.make_stream(content)
            if setup:
                setup(pdf, page)
            filename = Path(tmp) / 'synthetic.pdf'
            pdf.save(filename)
            return module.census(filename)

    def test_restore_and_default_miter(self):
        result = self.measure(b'1 Tr 3 w BT (a) Tj ET q 8 w 2 M BT (b) Tj ET Q BT (c) Tj ET')
        self.assertEqual(result['text_shows'], 3)
        self.assertEqual(result['stroke_text_states'], [
            dict(w=3.0, j=0, M=10, count=2), dict(w=8.0, j=0, M=2.0, count=1)])

    def test_form_inherits_graphics_state_and_extgstate(self):
        def setup(pdf, page):
            form = pdf.make_stream(b'BT (a) Tj ET /Style gs BT (b) Tj ET')
            form.Type = pikepdf.Name.XObject
            form.Subtype = pikepdf.Name.Form
            form.BBox = pikepdf.Array([0, 0, 100, 100])
            form.Resources = pikepdf.Dictionary(ExtGState=pikepdf.Dictionary(
                Style=pikepdf.Dictionary(LW=4, ML=2, LJ=2)))
            page.Resources.XObject = pikepdf.Dictionary(F=form)
        result = self.measure(b'1 Tr 3 w 7 M /F Do BT (c) Tj ET', setup)
        self.assertEqual(result['stroke_text_states'], [
            dict(w=3.0, j=0, M=7.0, count=2), dict(w=4.0, j=2.0, M=2.0, count=1)])

    def test_unbalanced_state_is_not_silently_accepted(self):
        for content in (b'Q', b'q'):
            with self.subTest(content=content), self.assertRaises(ValueError):
                self.measure(content)


if __name__ == '__main__':
    unittest.main()
