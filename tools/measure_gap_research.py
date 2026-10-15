"""Measure the Illustrator gap research probe (tools/jsx/ai-gap-research.jsx), without Adobe.

usage: measure_gap_research.py <folder> <runId> [--out JSON]

T (text): fill-core damage as in measure_text_gaps.py: the fill reference (T-fill) gives the glyph
fill mask, eroded by 3 px at 600 dpi; a core pixel lighter than level 32 is a stroke painted over
a fill. The characters MuPDF extracts show whether the text stayed text.
I (Inside on overlapping squares): the union of the squares (I-fill, fill level about 204) eroded
by the stroke width plus 3 px is the interior an Inside stroke must not reach; a pixel darker than
level 128 there is stroke on an inner contour edge (stray). The band inside the union's boundary
must be stroked: a pixel there lighter than 128 is missing stroke.
D and L (dashes): dark pixel counts, so a vanished stroke shows as 0, and the rows of the line
renders compared, which tells whether a collinear anchor moved the dashes.
"""
import argparse
import json
from pathlib import Path

import numpy
import pymupdf
from scipy import ndimage

DPI = 600
ERODE = 3
DARK = 32


def render(path):
    with pymupdf.open(path) as doc:
        page = doc[0]
        pix = page.get_pixmap(dpi=DPI, colorspace=pymupdf.csGRAY, alpha=False)
        img = numpy.frombuffer(pix.samples, numpy.uint8).reshape(pix.height, pix.width).copy()
        return img, len(page.get_text().strip().replace('\n', ''))


def text_damage(fill, img):
    mask = fill < DARK
    core = ndimage.binary_erosion(mask, iterations=ERODE)
    return {'core_px': int(core.sum()), 'core_damaged_px': int((core & (img >= DARK)).sum())}


def inside(fill, img, width_pt):
    union = fill < 250
    px = int(round(width_pt * DPI / 72))
    interior = ndimage.binary_erosion(union, iterations=px + ERODE)
    band = union & ~ndimage.binary_erosion(union, iterations=max(px - ERODE, 1)) & ndimage.binary_erosion(union, iterations=ERODE)
    return {'interior_px': int(interior.sum()), 'stray_px': int((interior & (img < 128)).sum()),
            'stray_pt2': round(float((interior & (img < 128)).sum()) * (72 / DPI) ** 2, 1),
            'band_px': int(band.sum()), 'band_missing_px': int((band & (img >= 128)).sum()),
            'outside_px': int((~union & (img < 128)).sum())}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('folder', type=Path)
    ap.add_argument('run')
    ap.add_argument('--out', type=Path)
    a = ap.parse_args()
    f = lambda n: a.folder / f'{a.run}-{n}.pdf'
    res = {'dpi': DPI}
    if f('T-fill').exists():
        fill, _ = render(f('T-fill'))
        for case in ('native', 'ff', 'reopen', 'edited'):
            if f('T-' + case).exists():
                img, text = render(f('T-' + case))
                res['T-' + case] = dict(text_damage(fill, img), text=text)
    if f('I-fill').exists():
        fill, _ = render(f('I-fill'))
        for case in sorted(p.stem.split('-', 1)[1] for p in a.folder.glob(f'{a.run}-I-*.pdf')):
            if case != 'I-fill':
                img, _ = render(f(case))
                res[case] = inside(fill, img, 8)
    for case in ('D-native', 'D-align', 'L-plain', 'L-plain-align', 'L-mid-align'):
        if f(case).exists():
            img, _ = render(f(case))
            res[case] = {'dark_px': int((img < 128).sum())}
    if f('L-plain-align').exists() and f('L-mid-align').exists():
        p, _ = render(f('L-plain-align'))
        m, _ = render(f('L-mid-align'))
        res['L-mid-vs-plain-differing_px'] = int(((p < 128) != (m < 128)).sum())
    print(json.dumps(res, indent=1))
    if a.out:
        a.out.write_text(json.dumps(res, indent=1) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
