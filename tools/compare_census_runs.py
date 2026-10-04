"""Compare coverage-census exports row by row: two exports of one run, or the same rows of two runs.

Rows are matched by label, so a run with rows added (and rows moved down) still lines up with an
earlier one. Each row's cell is rendered at ZOOM px/pt in MuPDF (grayscale) from both PDFs and
compared: the area where they differ by more than 16 and by more than 64 levels, in square
points, and the largest difference. Rows absent from either run are listed.

usage: compare_census_runs.py A_FOLDER A_EXPORT B_FOLDER B_EXPORT [--zoom 8] [--out JSON]
       [--only SUBSTRING ...]
  e.g. compare_census_runs.py build/look/coverage-census corrected build/look/coverage-census-2 corrected
       compare_census_runs.py build/look/coverage-census-2 native build/look/coverage-census-2 probe
"""
import argparse
import json

import numpy as np
import pymupdf


def render(page, cell, zoom):
    y0, x0, y1, x1 = cell
    pix = page.get_pixmap(matrix=pymupdf.Matrix(zoom, zoom), clip=pymupdf.Rect(x0, y0, x1, y1), colorspace=pymupdf.csGRAY)
    return np.frombuffer(pix.samples, np.uint8).reshape(pix.height, pix.width).astype(np.int16)


def load(folder, export):
    d = folder.rstrip('/\\') + '/'
    m = json.load(open(d + 'coverage-census.manifest.json', encoding='utf-8'))
    return {r['label']: r for r in m['items']}, pymupdf.open(d + 'coverage-census-' + export + '.pdf')[0]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('a_folder')
    ap.add_argument('a_export')
    ap.add_argument('b_folder')
    ap.add_argument('b_export')
    ap.add_argument('--zoom', type=float, default=8)
    ap.add_argument('--out')
    ap.add_argument('--only', nargs='*')
    a = ap.parse_args()
    rows_a, page_a = load(a.a_folder, a.a_export)
    rows_b, page_b = load(a.b_folder, a.b_export)
    out = []
    for label, rb in rows_b.items():
        if a.only and not any(s in label for s in a.only):
            continue
        ra = rows_a.get(label)
        if ra is None:
            continue
        ia, ib = render(page_a, ra['cell'], a.zoom), render(page_b, rb['cell'], a.zoom)
        h, w = min(ia.shape[0], ib.shape[0]), min(ia.shape[1], ib.shape[1])
        d = np.abs(ia[:h, :w] - ib[:h, :w])
        px = 1.0 / a.zoom ** 2
        rec = {'case': label, 'over16': round(float((d > 16).sum() * px), 2), 'over64': round(float((d > 64).sum() * px), 2),
               'max': int(d.max())}
        out.append(rec)
        print(f"{label[:64]:64s} >16 {rec['over16']:8.2f}  >64 {rec['over64']:8.2f}  max {rec['max']:3d}")
    missing_a = [k for k in rows_b if k not in rows_a and not (a.only and not any(s in k for s in a.only))]
    missing_b = [k for k in rows_a if k not in rows_b and not (a.only and not any(s in k for s in a.only))]
    if missing_a:
        print('only in B:', '; '.join(missing_a))
    if missing_b:
        print('only in A:', '; '.join(missing_b))
    if a.out:
        with open(a.out, 'w', encoding='utf-8', newline='\n') as fh:
            json.dump({'a': [a.a_folder, a.a_export], 'b': [a.b_folder, a.b_export], 'zoom': a.zoom, 'rows': out,
                       'only_in_a': missing_b, 'only_in_b': missing_a}, fh, indent=1)
            fh.write('\n')


if __name__ == '__main__':
    main()
