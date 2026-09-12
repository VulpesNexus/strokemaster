"""Small in-memory controls for the optional embedded-font research census."""
import importlib.util
import unittest

AVAILABLE = all(importlib.util.find_spec(name) is not None for name in ('fontTools', 'pikepdf'))


class Glyph:
    def __init__(self, draw):
        self.draw = draw

    def drawPoints(self, pen):
        from fontTools.pens.pointPen import SegmentToPointPen
        self.draw(SegmentToPointPen(pen))


class Font:
    def __init__(self, glyphs):
        self.glyphs = glyphs

    def getBestCmap(self):
        return {32: 'space', 65: 'A', 66: 'B'}

    def getGlyphSet(self):
        return self.glyphs


def triangle(pen):
    pen.moveTo((0, 0))
    pen.lineTo((1, 0))
    pen.lineTo((0, 1))
    pen.closePath()


@unittest.skipUnless(AVAILABLE, 'optional provenance dependencies unavailable; use repo venv')
class FontProvenanceTests(unittest.TestCase):
    def test_composite_components_and_repeated_characters(self):
        from font_provenance import glyph_counts
        def composite(pen):
            pen.addComponent('A', (1, 0, 0, 1, 0, 0))
            pen.addComponent('A', (-1, 0, 0, 1, 4, 0))
        font = Font({'A': Glyph(triangle), 'B': Glyph(composite), 'space': Glyph(lambda pen: None)})
        rows = glyph_counts(font, 'AB B')
        self.assertEqual([row['character'] for row in rows], list('AB B'))
        self.assertEqual([row['contours'] for row in rows], [1, 2, 0, 2])
        self.assertEqual(sum(row['contours'] for row in rows), 5)
        self.assertEqual(rows[1]['operations']['closePath'], 2)

    def test_open_glyph_is_rejected(self):
        from font_provenance import glyph_counts
        def open_path(pen):
            pen.moveTo((0, 0))
            pen.lineTo((1, 0))
            pen.endPath()
        with self.assertRaisesRegex(ValueError, 'open or unbalanced'):
            glyph_counts(Font({'A': Glyph(open_path)}), 'A')

    def test_unsupported_mapping_is_rejected(self):
        from font_provenance import glyph_counts
        font = Font({'A': Glyph(triangle)})
        for text in ('C', '\u00e9', '\n'):
            with self.assertRaises(ValueError):
                glyph_counts(font, text)

    def test_TJ_spacing_is_not_a_glyph_and_rejects_unsupported_text(self):
        import pikepdf
        from font_provenance import decode_show
        operands = [pikepdf.Array([pikepdf.String('Lo'), -12, pikepdf.String('rem'), 8])]
        self.assertEqual(decode_show(operands, 'TJ'), 'Lorem')
        self.assertEqual(decode_show([pikepdf.String('Lorem ipsum')], 'Tj'), 'Lorem ipsum')
        with self.assertRaises(ValueError):
            decode_show([pikepdf.String(bytes([233]))], 'Tj')
        with self.assertRaises(ValueError):
            decode_show([pikepdf.String('A')], "'")


    def test_point_histogram_distinguishes_raw_points_from_segments(self):
        from font_provenance import contour_point_statistics
        class PointGlyph:
            def drawPoints(self, pen):
                for points in ([((0, 0), 'line')],
                               [((0, 0), 'line'), ((1, 0), 'line')],
                               [((0, 0), 'line'), ((0, 0), 'line')],
                               [((0, 0), 'qcurve'), ((1, 1), None)]):
                    pen.beginPath()
                    for point, kind in points:
                        pen.addPoint(point, segmentType=kind)
                    pen.endPath()
        result = contour_point_statistics({'A': PointGlyph()}, 'A')
        self.assertEqual(result['explicit_points_per_contour'], {1: 1, 2: 3})
        self.assertEqual(result['single_point_contours'], 1)
        self.assertEqual(result['two_point_contours'], 3)
        self.assertEqual(result['two_point_details'],
                         {'both_on_curve': 2, 'coincident': 1, 'distinct_coordinates': 2, 'has_off_curve': 1})

    def test_missing_component_fails_instead_of_silently_disappearing(self):
        from font_provenance import contour_point_statistics
        class Missing:
            def drawPoints(self, pen):
                pen.addComponent('absent', (1, 0, 0, 1, 0, 0))
        with self.assertRaises(KeyError):
            contour_point_statistics({'A': Missing()}, 'A')


if __name__ == '__main__':
    unittest.main()
