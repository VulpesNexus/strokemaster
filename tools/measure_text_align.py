"""Measure the Illustrator text alignment probe (tools/jsx/ai-text-align.jsx), without Adobe.

usage: measure_text_align.py <folder> <runId>

MuPDF at 600 dpi. T-fill.pdf gives the glyph mask (black). In each T-<setup>-<align>.pdf, a pixel
is stroke when it is neither black nor white (the stroke is gray). Reported per render:
  out_pt  the stroke's mean reach outside the glyphs (stroke area outside / glyph perimeter), pt
  in_pt   the visible stroke's mean reach inside the glyphs, pt
  fill    the share of the glyph area still drawn black
Center 8 shows out ~4 and in ~4; Outside 8 out ~8, in 0; Inside 8 out 0, in ~8 (or 0 when the
fill paints over it).
"""
import sys
from pathlib import Path

import numpy
import pymupdf
from scipy import ndimage

DPI = 600


def render(path):
    with pymupdf.open(path) as doc:
        pix = doc[0].get_pixmap(dpi=DPI, colorspace=pymupdf.csGRAY, alpha=False)
        return numpy.frombuffer(pix.samples, numpy.uint8).reshape(pix.height, pix.width)


def main():
    folder, run = Path(sys.argv[1]), sys.argv[2]
    glyph = render(folder / f'{run}-T-fill.pdf') < 64
    edge = glyph & ~ndimage.binary_erosion(glyph)
    perimeter_pt = edge.sum() * 72 / DPI
    px_pt2 = (72 / DPI) ** 2
    print(f'glyph area {glyph.sum() * px_pt2:.0f} pt2, perimeter {perimeter_pt:.0f} pt')
    for setup in ('chars', 'below', 'above'):
        for align in ('center', 'inside', 'outside'):
            path = folder / f'{run}-T-{setup}-{align}.pdf'
            if not path.exists():
                continue
            img = render(path)
            stroke = (img >= 64) & (img <= 200)
            out_pt = (stroke & ~glyph).sum() * px_pt2 / perimeter_pt
            in_pt = (stroke & glyph).sum() * px_pt2 / perimeter_pt
            fill = ((img < 64) & glyph).sum() / glyph.sum()
            print(f'{setup:6} {align:8} out {out_pt:5.2f} pt  in {in_pt:5.2f} pt  fill {fill:5.1%}')


if __name__ == '__main__':
    main()
