"""Pixel check of an Outside stroke against its outline: inside the outline's nonzero fill, more
than 0.3 pt in, the item's fill (or the backdrop, for an unfilled item); outside it, from 0.3 pt
to 0.3 pt short of the stroke weight, the stroke's paint (white). Counts the pixels that are not.

The outline is a dump from tools/dump_pdf_outline.py, shifted by dx, dy page points (the same
outline drawn elsewhere on the page). The page is rendered by MuPDF, or read from a whole-page
grayscale PNG rendered at the zoom given (Ghostscript, PDFium), so renderers can be compared:
MuPDF strokes a contour much smaller than the stroke's width as a ring with a hole, which
Ghostscript does not (2026-10-04).

usage: check_outside_stroke.py <pdf | page.png> <outline dump> <dx> <dy> <weight> <fill|backdrop>
                               [zoom, default 8] [prefix: save the wrong pixels as <prefix>inside.npy, band.npy]
"""
import sys

import numpy as np
import pymupdf
from scipy import ndimage

FILL, BACKDROP = 0x22, 0x93   # the font test's Black fill and 50% Black backdrop, in gray levels


def read(path, dx, dy):
    rings, cur, p = [], None, None
    for line in open(path):
        t = line.split()
        if not t:
            continue
        if t[0] == 'M':
            p = (float(t[1]) + dx, float(t[2]) + dy)
            cur = [p]
            rings.append(cur)
        elif t[0] == 'L':
            p = (float(t[1]) + dx, float(t[2]) + dy)
            cur.append(p)
        elif t[0] == 'C':
            v = [float(x) for x in t[1:]]
            c1, c2, e = (v[0] + dx, v[1] + dy), (v[2] + dx, v[3] + dy), (v[4] + dx, v[5] + dy)
            for k in range(1, 17):
                s = k / 16
                cur.append(tuple((1 - s) ** 3 * p[i] + 3 * (1 - s) ** 2 * s * c1[i] + 3 * (1 - s) * s * s * c2[i] + s ** 3 * e[i] for i in range(2)))
            p = e
    return [np.array(r) for r in rings if len(r) >= 3]


def nonzero_mask(rings, x0, y0, w, h, z):
    """The rings' nonzero fill at pixel centers, scanline by scanline."""
    a = np.concatenate(rings)
    b = np.concatenate([np.roll(r, -1, axis=0) for r in rings])
    mask = np.zeros((h, w), bool)
    xs = x0 + (np.arange(w) + 0.5) / z
    for row in range(h):
        y = y0 + (row + 0.5) / z
        up = (a[:, 1] <= y) & (b[:, 1] > y)
        dn = (a[:, 1] > y) & (b[:, 1] <= y)
        sel = up | dn
        if not sel.any():
            continue
        ax, ay, bx, by = a[sel, 0], a[sel, 1], b[sel, 0], b[sel, 1]
        x = ax + (y - ay) * (bx - ax) / (by - ay)
        order = np.argsort(x)
        x, winding = x[order], np.cumsum(np.where(up[sel], 1, -1)[order])
        idx = np.searchsorted(x, xs, side='right') - 1
        mask[row] = np.where(idx >= 0, winding[np.clip(idx, 0, None)], 0) != 0
    return mask


def main():
    src, dump = sys.argv[1], sys.argv[2]
    dx, dy, weight, inside = float(sys.argv[3]), float(sys.argv[4]), float(sys.argv[5]), sys.argv[6]
    z = float(sys.argv[7]) if len(sys.argv) > 7 else 8
    prefix = sys.argv[8] if len(sys.argv) > 8 else None
    rings = read(dump, dx, dy)
    pts = np.concatenate(rings)
    x0, y0 = pts[:, 0].min() - weight - 2, pts[:, 1].min() - weight - 2
    x1, y1 = pts[:, 0].max() + weight + 2, pts[:, 1].max() + weight + 2
    if src.lower().endswith('.png'):
        from PIL import Image
        Image.MAX_IMAGE_PIXELS = None
        page = np.asarray(Image.open(src).convert('L')).astype(int)
        c0, r0 = int(round(x0 * z)), int(round(y0 * z))
        x0, y0 = c0 / z, r0 / z
        img = page[r0:int(round(y1 * z)), c0:int(round(x1 * z))]
    else:
        pix = pymupdf.open(src)[0].get_pixmap(matrix=pymupdf.Matrix(z, z), clip=pymupdf.Rect(x0, y0, x1, y1),
                                              colorspace=pymupdf.csGRAY, alpha=False)
        img = np.frombuffer(pix.samples, np.uint8).reshape(pix.height, pix.width).astype(int)
    fill = nonzero_mask(rings, x0, y0, img.shape[1], img.shape[0], z)
    dist_out = ndimage.distance_transform_edt(~fill) / z
    dist_in = ndimage.distance_transform_edt(fill) / z
    deep = dist_in > 0.3
    band = (dist_out > 0.3) & (dist_out < weight - 0.3)
    bad_in = deep & (np.abs(img - {'fill': FILL, 'backdrop': BACKDROP}[inside]) > 40)
    bad_band = band & (img < 255 - 40)
    print(f'inside px {int(deep.sum())}, wrong {int(bad_in.sum())}; band px {int(band.sum())}, not stroke {int(bad_band.sum())}')
    for name, m in (('inside', bad_in), ('band', bad_band)):
        ys, xs = np.nonzero(m)
        if prefix:
            np.save(prefix + name + '.npy', np.stack([x0 + xs / z, y0 + ys / z], 1))
        for i in range(min(5, len(ys))):
            print(f'  {name} wrong at page {x0 + xs[i] / z:.2f} {y0 + ys[i] / z:.2f} value {img[ys[i], xs[i]]}')


if __name__ == '__main__':
    main()
