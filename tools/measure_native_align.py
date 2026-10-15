"""Measure the native-align probe (tools/jsx/ai-native-align.jsx), without Adobe.

usage: measure_native_align.py <folder> <runId>

MuPDF at 600 dpi, gray levels quantized to eighths, areas in square points.
- restore: each case's restored render against its original (0 means put back exactly);
- open path: the stroke on each side of the path (the path turns clockwise, so its left side is
  Outside);
- compound path: the square's part against Illustrator's own Outside on the square alone;
- type: against Illustrator's own Outside on the outlined type;
- dashed thorn: the tip (the top 40 pt above the base) drawn, against the solid Outside thorn.
"""
import sys
from pathlib import Path

import numpy
import pymupdf

DPI = 600
A2 = (72 / DPI) ** 2


def render(path):
    with pymupdf.open(path) as doc:
        pix = doc[0].get_pixmap(dpi=DPI, colorspace=pymupdf.csGRAY, alpha=False)
        return numpy.frombuffer(pix.samples, numpy.uint8).reshape(pix.height, pix.width)


def diff(a, b):
    return round(float((a // 32 != b // 32).sum() * A2), 1)


def main():
    folder, run = Path(sys.argv[1]), sys.argv[2]
    f = lambda n: folder / f'{run}-N-{n}.pdf'
    k = DPI / 72
    for case in ('op-out', 'op-in', 'cmp-out', 'cmp-in', 'tx-out', 'tx-in', 'txm-out', 'txs-out', 'sq-out', 'th-dash'):
        if f(case).exists():
            print(f'{case:8} changed {diff(render(f(case)), render(f(case + "-orig"))):8} pt2   restored vs orig '
                  f'{diff(render(f(case + "-restored")), render(f(case + "-orig"))):7} pt2')
    h, w = render(f('op-out')).shape
    yy, xx = numpy.mgrid[0:h, 0:w]
    px, py = xx / k, 200 - yy / k
    def seg(ax, ay, bx, by):
        dx, dy = bx - ax, by - ay
        t = numpy.clip(((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy), 0, 1)
        return numpy.hypot(px - (ax + t * dx), py - (ay + t * dy)), dx * (py - ay) - dy * (px - ax)
    d1, c1 = seg(40, 40, 100, 160)
    d2, c2 = seg(100, 160, 160, 40)
    left = numpy.where(d1 <= d2, c1, c2) > 0
    for case in ('op-out', 'op-in', 'op-out-orig'):
        s = render(f(case)) < 200
        print(f'{case:11} left (Outside) {(s & left).sum() * A2:7.1f}  right (Inside) {(s & ~left).sum() * A2:7.1f}')
    sl = (slice(0, int(110 * k)), slice(0, int(105 * k)))   # the square's part of the page
    print('cmp-out square part vs Illustrator Outside', diff(render(f('cmp-out'))[sl], render(f('cmpref-out'))[sl]), 'pt2')
    print('tx-out vs outlined Outside', diff(render(f('tx-out')), render(f('txref-out'))), 'pt2;',
          'txm-out vs outlined Outside', diff(render(f('txm-out')), render(f('txref-out'))), 'pt2 (x is 6 pt there)')
    if f('txs-out').exists():
        g = render(f('txs-out')) < 64   # the letters: whole and black under an Outside stroke
        def reach(name):
            img = render(f(name))
            s = (img >= 64) & (img <= 200)
            return (s & ~g).sum() * A2
        print('txs-out stroke outside the letters', round(reach('txs-out'), 1), 'pt2 against', round(reach('txs-out-orig'), 1),
              'for the Center stroke as drawn: ratio', round(reach('txs-out') / max(reach('txs-out-orig'), 1e-9), 2), '(2 = Outside of the drawn weight)')
    tip = (slice(0, int((200 - 125) * k)), slice(None))
    for case in ('thref', 'th-dash-orig', 'th-dash'):
        s = render(f(case))[tip] < 200
        print(f'{case:12} tip drawn {s.sum() * A2:6.1f} pt2')


if __name__ == '__main__':
    main()
