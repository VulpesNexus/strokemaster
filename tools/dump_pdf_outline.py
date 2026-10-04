"""Dump the first large filled path in a band of a PDF page as contours, one operator per line:
M x y, L x y, C x1 y1 x2 y2 x y, and Z after each contour (page points, y down, as PyMuPDF
reports them). The reference outline for tools/check_outside_stroke.py.

A rectangle the PDF writes as `re` is dumped with the orientation PyMuPDF reports for it, which
is a guess where the port encoded a winding in the signs of its size: a rectangular hole may
come out filled, and the check then reports its pixels as wrong.

usage: dump_pdf_outline.py <pdf> <y0> <y1> <out.txt> [--min-items 100]
"""
import argparse

import pymupdf


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('pdf')
    ap.add_argument('y0', type=float)
    ap.add_argument('y1', type=float)
    ap.add_argument('out')
    ap.add_argument('--min-items', type=int, default=100, help='skip paths with fewer drawing items')
    a = ap.parse_args()
    page = pymupdf.open(a.pdf)[0]
    lines = []
    for dr in page.get_drawings():
        r = dr['rect']
        if dr['type'] not in ('f', 'fs') or r.y0 < a.y0 or r.y1 > a.y1 or len(dr['items']) < a.min_items:
            continue
        cur = None
        for it in dr['items']:
            if it[0] == 're':
                rect, orientation = it[1], it[2]
                pts = [(rect.x0, rect.y0), (rect.x1, rect.y0), (rect.x1, rect.y1), (rect.x0, rect.y1)]
                if orientation < 0:
                    pts.reverse()
                if cur is not None:
                    lines.append('Z')
                lines.append(f'M {pts[0][0]!r} {pts[0][1]!r}')
                lines += [f'L {x!r} {y!r}' for x, y in pts[1:]]
                lines.append('Z')
                cur = None
                continue
            p0 = it[1]
            if cur is None or abs(p0.x - cur.x) > 1e-6 or abs(p0.y - cur.y) > 1e-6:
                if cur is not None:
                    lines.append('Z')
                lines.append(f'M {p0.x!r} {p0.y!r}')
            if it[0] == 'l':
                cur = it[2]
                lines.append(f'L {cur.x!r} {cur.y!r}')
            elif it[0] == 'c':
                cur = it[4]
                lines.append(f'C {it[2].x!r} {it[2].y!r} {it[3].x!r} {it[3].y!r} {cur.x!r} {cur.y!r}')
        lines.append('Z')
        break
    with open(a.out, 'w', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    print(sum(1 for line in lines if line[0] == 'M'), 'contours,', sum(1 for line in lines if line[0] in 'LC'), 'segments')


if __name__ == '__main__':
    main()
