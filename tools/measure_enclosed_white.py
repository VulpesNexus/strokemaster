"""White areas enclosed by ink in exported PDFs: what Close holes should have filled.

Renders each PDF's first page with MuPDF and counts the white pixels not connected to the page's
own white around the art (holes the eye sees as closed). With two PDFs, the first being the
reference, also reports the pixels that differ and how many of them lie outside the reference's
enclosed white (a change anywhere else is a change to the drawing itself). Areas in pt^2.

    python tools/measure_enclosed_white.py before.pdf [after.pdf] [--dpi 600]
"""
import argparse
import json
import sys

import fitz
import numpy as np
from scipy import ndimage


def render(path, dpi):
    with fitz.open(path) as doc:
        pix = doc[0].get_pixmap(dpi=dpi, alpha=False)
        return np.frombuffer(pix.samples, dtype=np.uint8).reshape(pix.height, pix.width, pix.n)[:, :, :3].astype(np.int16)


def enclosed(img):
    white = (img > 235).all(axis=2)
    labels, count = ndimage.label(white)
    border = set(np.unique(np.concatenate([labels[0], labels[-1], labels[:, 0], labels[:, -1]]))) - {0}
    inside = white & ~np.isin(labels, list(border))
    sizes = ndimage.sum(inside, labels, index=range(1, count + 1)) if count else []
    return inside, int(sum(1 for s in sizes if s > 0))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('pdfs', nargs='+')
    ap.add_argument('--dpi', type=int, default=600)
    a = ap.parse_args()
    pt2 = (72.0 / a.dpi) ** 2
    out = {}
    imgs = []
    for p in a.pdfs[:2]:
        img = render(p, a.dpi)
        inside, n = enclosed(img)
        imgs.append((img, inside))
        out[p] = {'enclosedRegions': n, 'enclosedPt2': round(float(inside.sum()) * pt2, 3)}
    if len(imgs) == 2 and imgs[0][0].shape == imgs[1][0].shape:
        (ref, refInside), (img, _) = imgs
        changed = (np.abs(ref - img) > 40).any(axis=2)
        # The reference's enclosed white, grown by two pixels for the antialiased edges around it.
        near = ndimage.binary_dilation(refInside, iterations=2)
        out['changedPt2'] = round(float(changed.sum()) * pt2, 3)
        out['changedElsewherePt2'] = round(float((changed & ~near).sum()) * pt2, 3)
    json.dump(out, sys.stdout, indent=1)
    print()


if __name__ == '__main__':
    main()
