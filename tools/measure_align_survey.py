"""Measure the Illustrator alignment survey (tools/jsx/ai-align-survey.jsx), without Adobe.

usage: measure_align_survey.py <folder> <runId>

Each target sits in its own cell (five per row, 125 x 115 pt, from the page's top left at 20, 20).
For each of S-center.pdf, S-inside.pdf, and S-outside.pdf (MuPDF, 300 dpi), the cell's painted
area (anything not white) and its black (fill) area, in square points. Center 8 on a shape: Outside
paints a wider ring than Center, and shows more of the fill; Inside paints within the fill. Equal
areas in all three mean the alignment was not drawn.
"""
import sys
from pathlib import Path

import numpy
import pymupdf

DPI = 300


def main():
    folder, run = Path(sys.argv[1]), sys.argv[2]
    pages = {}
    for align in ('center', 'inside', 'outside'):
        with pymupdf.open(folder / f'{run}-S-{align}.pdf') as doc:
            pix = doc[0].get_pixmap(dpi=DPI, colorspace=pymupdf.csGRAY, alpha=False)
            pages[align] = numpy.frombuffer(pix.samples, numpy.uint8).reshape(pix.height, pix.width)
    k = DPI / 72
    a2 = (72 / DPI) ** 2
    for i in range(16):
        x0, y0 = 10 + (i % 5) * 125, 15 + (i // 5) * 115
        sl = (slice(int(y0 * k), int((y0 + 115) * k)), slice(int(x0 * k), int((x0 + 125) * k)))
        cells = []
        for align in ('center', 'inside', 'outside'):
            c = pages[align][sl]
            cells.append(((c < 250).sum() * a2, (c < 40).sum() * a2))
        same = all(abs(p - cells[0][0]) < 2 and abs(b - cells[0][1]) < 2 for p, b in cells)
        print(f'{i:2}  ' + '  '.join(f'{a[:3]} paint {p:7.0f} black {b:7.0f}' for a, (p, b) in zip(('center', 'inside', 'outside'), cells))
              + ('   SAME (not drawn)' if same else ''))


if __name__ == '__main__':
    main()
