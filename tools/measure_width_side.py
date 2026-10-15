"""Measure the one-sided width profile probe (tools/jsx/ai-width-side.jsx), without Adobe.

usage: measure_width_side.py <folder> <runId>

MuPDF at 600 dpi. For each render: the stroke (gray, neither black nor white) area outside and
inside the fill reference's black area, as a mean reach in points (area / the reference outline's
length). The reference is the case's Center render's black-or-gray union for shapes measured
against Illustrator's own Inside and Outside, and tx-fill for type. Also the pixels that differ
from Illustrator's own Outside or Inside render, where one exists.
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


def stroke(img):
    return (img >= 64) & (img <= 200)


def main():
    folder, run = Path(sys.argv[1]), sys.argv[2]
    f = lambda n: folder / f'{run}-W-{n}.pdf'
    a2 = (72 / DPI) ** 2
    # Fill masks: the square from Illustrator's Inside render (fill plus inner ring), the thorn from
    # its Outside render's black, the type from tx-fill, and the open and compound shapes from their
    # Center render's black.
    masks = {}
    for group, ref in (('sq', 'sq-inside'), ('th', 'th-outside'), ('open', 'open-center'), ('cmp', 'cmp-center'), ('tx', 'tx-fill')):
        img = render(f(ref))
        masks[group] = (img < 200) if group == 'sq' else (img < 64)
        if group == 'th':   # Outside leaves the fill whole: black is the fill
            masks[group] = img < 64
    for name in ('sq-center', 'sq-inside', 'sq-outside', 'sq-w01', 'sq-w10', 'sq-w02', 'sq-w20', 'th-outside', 'th-w01', 'th-w10',
                 'open-center', 'open-w01', 'open-w10', 'cmp-center', 'cmp-w01', 'cmp-w10', 'tx-center', 'tx-w01', 'tx-w10'):
        if not f(name).exists():
            continue
        img = render(f(name))
        m = masks[name.split('-')[0]]
        per = (m & ~ndimage.binary_erosion(m)).sum() * 72 / DPI
        s = stroke(img)
        line = f'{name:12} out {(s & ~m).sum() * a2 / per:5.2f} pt  in {(s & m).sum() * a2 / per:5.2f} pt'
        for ref in ('outside', 'inside'):
            r = f(name.split('-')[0] + '-' + ref)
            if r.exists() and name.startswith(('sq', 'th')):
                d = (render(r) // 32 != img // 32).sum() * a2
                line += f'  vs {ref} {d:7.1f} pt2'
        print(line)


if __name__ == '__main__':
    main()
