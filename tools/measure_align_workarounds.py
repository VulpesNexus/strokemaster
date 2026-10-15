"""Measure the alignment workaround probe (tools/jsx/ai-align-workarounds.jsx), without Adobe.

usage: measure_align_workarounds.py <folder> <runId>

MuPDF at 600 dpi, gray levels quantized to eighths. For the type: each stand-in against the exact
reference (Illustrator's own Outside or Inside on the outlined type): the area that differs, in
square points, and the stroke's mean reach outside and inside the glyph mask (tx-fill). For the open
path: the stroke's area on each side of the path, and the area beyond the stroke weight from the
path (a spike or star at a join).
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
    f = lambda n: folder / f'{run}-A-{n}.pdf'
    a2 = (72 / DPI) ** 2
    glyph = render(f('tx-fill')) < 64
    per = (glyph & ~ndimage.binary_erosion(glyph)).sum() * 72 / DPI
    refs = {'out': render(f('tx-ref-out')), 'in': render(f('tx-ref-in'))}
    for name in ('tx-ref-out', 'tx-ref-in', 'tx-below2w', 'tx-above-a', 'tx-above-b'):
        img = render(f(name))
        s = (img >= 64) & (img <= 200)
        line = f'{name:11} out {(s & ~glyph).sum() * a2 / per:5.2f} pt  in {(s & glyph).sum() * a2 / per:5.2f} pt'
        for k, r in refs.items():
            line += f'  vs ref-{k} {(r // 32 != img // 32).sum() * a2:7.1f} pt2'
        print(line)
    # The open path: (40,40)-(100,160)-(160,40) in page points, y up; the page is 200 high.
    h, w = render(f('op-center')).shape
    yy, xx = numpy.mgrid[0:h, 0:w]
    px, py = xx * 72 / DPI, 200 - yy * 72 / DPI
    def side_and_dist(ax, ay, bx, by):
        dx, dy = bx - ax, by - ay
        L = numpy.hypot(dx, dy)
        t = numpy.clip(((px - ax) * dx + (py - ay) * dy) / L ** 2, 0, 1)
        d = numpy.hypot(px - (ax + t * dx), py - (ay + t * dy))
        cross = dx * (py - ay) - dy * (px - ax)   # > 0: left of travel
        return d, cross
    d1, c1 = side_and_dist(40, 40, 100, 160)
    d2, c2 = side_and_dist(100, 160, 160, 40)
    near1 = d1 <= d2
    dist = numpy.where(near1, d1, d2)
    left = numpy.where(near1, c1, c2) > 0
    for name in ('op-center', 'op-a', 'op-b'):
        s = render(f(name)) < 200
        print(f'{name:11} left {(s & left).sum() * a2:7.1f} pt2  right {(s & ~left).sum() * a2:7.1f} pt2  '
              f'beyond 8.5 pt {(s & (dist > 8.5)).sum() * a2:6.1f} pt2')


if __name__ == '__main__':
    main()
