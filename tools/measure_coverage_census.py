"""Measure the coverage census (tools/jsx/build-coverage-census.jsx): which strokes that the plugin
leaves to InDesign show the defects it removes.

Each row is measured against the same items in coverage-census-geometry.pdf (the census's
geometry layer alone: filled, unstroked, the outline the stroke follows), in MuPDF renders at
ZOOM px/pt. Every pixel is counted for the item part (outlined text, thorn, squares) whose fill
is nearest. With h the stroke's reach from the outline
(half the weight for Center, the weight for Outside and Inside) and f a fringe of 0.3 pt:

  allowed   stroke within h + f of the outline on the stroke's side(s), within f on the other,
            plus each miter join on the stroke's side: the corner's miter wedge, whole up to
            20 h, outside the fill for exterior corners and inside it for interior ones;
  escaping  stroke-colored pixels outside the fill and outside `allowed`: miter tips leaving
            the shape (the spikes and escaping tips the plugin removes);
  stray     stroke-colored pixels inside the fill and outside `allowed`: stroke drawn into the
            shape, such as contour edges inside an overlapping outline, or a stroke spilling
            past an Inside offset;
  holes     backdrop-colored pixels inside the fill, more than f in: fill that is not drawn
            (InDesign's offset fills break on some outlines);
  points    exterior corners past the miter limit (interior corners for Inside), and the part
            of the points the plugin keeps whole (ise_miter::WholePoint: both edges straight,
            the shorter at least h, the point at most five times it) that is not drawn; other
            corners past the limit are beveled by design and expect no point;
  spill     fill-colored pixels outside the fill, more than f out: fill drawn past the outline
            (InDesign's inward offset for Inside strokes breaks on some outlines and spills);
  off       pixels more than OFF levels from every expected gray: paint at the wrong level,
            such as a stroke drawn opaque that should be 50% (stroke opacity lost).

Areas are in square points. Pixels are classified by the nearest of the row's three expected
gray levels (backdrop, fill, stroke). Also reported: the area where native and corrected
renders differ by more than 64 levels (zero means the research build left the row to
InDesign), and for a transformed item, the same between its native drawing and its untransformed
twin's (one row down).

usage: measure_coverage_census.py <census folder> [--out JSON] [--zoom 8] [--native NAME]
       [--corrected NAME]   (the exports compared: coverage-census-<NAME>.pdf)
"""
import argparse
import json
import math

import numpy as np
import pymupdf
from PIL import Image, ImageDraw
from scipy import ndimage

BACKDROP, FILL, STROKE = 147, 34, 255
FRINGE = 0.3
OFF = 24
LIMIT = 4.0
LONGEST = 20.0


def render(page, cell, zoom):
    y0, x0, y1, x1 = cell
    pix = page.get_pixmap(matrix=pymupdf.Matrix(zoom, zoom), clip=pymupdf.Rect(x0, y0, x1, y1), colorspace=pymupdf.csGRAY)
    return np.frombuffer(pix.samples, np.uint8).reshape(pix.height, pix.width).astype(np.int16)


def contours(page, cell):
    """Filled paths in the cell as contours of ('l', p0, p1) and ('c', p0, c1, c2, p1)."""
    y0, x0, y1, x1 = cell
    out = []
    for dr in page.get_drawings():
        r = dr['rect']
        if dr.get('fill') is None or r.y1 < y0 or r.y0 > y1:
            continue
        cur, last = [], None
        for it in dr['items']:
            if it[0] == 're':
                q = it[1]
                pts = [q.tl, q.tr, q.br, q.bl]
                out.append([('l', pts[i], pts[(i + 1) % 4]) for i in range(4)])
                last = None
                continue
            if it[0] == 'qu':
                q = it[1]
                pts = [q.ul, q.ur, q.lr, q.ll]
                out.append([('l', pts[i], pts[(i + 1) % 4]) for i in range(4)])
                last = None
                continue
            p0 = it[1]
            if last is None or abs(p0.x - last.x) > 1e-3 or abs(p0.y - last.y) > 1e-3:
                if cur:
                    out.append(cur)
                cur = []
            cur.append(it)
            last = it[-1]
        if cur:
            out.append(cur)
    res = []
    for c in out:
        segs = []
        for it in c:
            if it[0] == 'l':
                segs.append(('l', (it[1].x, it[1].y), (it[2].x, it[2].y)))
            elif it[0] == 'c':
                segs.append(('c', (it[1].x, it[1].y), (it[2].x, it[2].y), (it[3].x, it[3].y), (it[4].x, it[4].y)))
        if not segs:
            continue
        s, e = segs[0][1], segs[-1][-1]
        if math.hypot(s[0] - e[0], s[1] - e[1]) > 1e-3:
            segs.append(('l', e, s))   # the fill closes every contour
        res.append(segs)
    return res


def unit(v):
    n = math.hypot(*v)
    return (v[0] / n, v[1] / n) if n > 1e-12 else None


def tangents(seg):
    """Unit tangent at the start and at the end of a segment."""
    if seg[0] == 'l':
        t = unit((seg[2][0] - seg[1][0], seg[2][1] - seg[1][1]))
        return t, t
    p0, c1, c2, p1 = seg[1:]
    a = unit((c1[0] - p0[0], c1[1] - p0[1])) or unit((c2[0] - p0[0], c2[1] - p0[1])) or unit((p1[0] - p0[0], p1[1] - p0[1]))
    b = unit((p1[0] - c2[0], p1[1] - c2[1])) or unit((p1[0] - c1[0], p1[1] - c1[1])) or unit((p1[0] - p0[0], p1[1] - p0[1]))
    return a, b


def chord(seg):
    return math.hypot(seg[-1][0] - seg[1][0], seg[-1][1] - seg[1][1])


def corners(cs):
    """(vertex, incoming tangent, outgoing tangent, shorter adjacent chord, both segments
    straight) at each junction."""
    out = []
    for c in cs:
        n = len(c)
        for i in range(n):
            a, b = c[i], c[(i + 1) % n]
            ta, tb = tangents(a)[1], tangents(b)[0]
            if ta is None or tb is None:
                continue
            if ta[0] * tb[0] + ta[1] * tb[1] > math.cos(math.radians(1)):
                continue
            out.append((b[1], ta, tb, min(chord(a), chord(b)), a[0] == 'l' and b[0] == 'l'))
    return out


def measure(native, corrected, geo, cs, cell, zoom, align, w, levels, parts):
    y0, x0 = cell[0], cell[1]
    fill = geo < 128
    h = w / 2 if align == 'center' else w
    f = FRINGE * zoom
    din = ndimage.distance_transform_edt(fill)
    dout = ndimage.distance_transform_edt(~fill)
    lv = np.array(levels)

    def classes(img):
        return np.argmin(np.abs(img[..., None] - lv[None, None, :]), axis=2)   # 0 backdrop, 1 fill, 2 stroke

    reach_out = h * zoom + f if align in ('center', 'outside') else f
    reach_in = h * zoom + f if align in ('center', 'inside') else f
    allowed = (~fill & (dout <= reach_out)) | (fill & (din <= reach_in))
    ext = Image.new('L', (fill.shape[1], fill.shape[0]), 0)
    intr = Image.new('L', (fill.shape[1], fill.shape[0]), 0)
    pts_img = Image.new('L', (fill.shape[1], fill.shape[0]), 0)
    de, di, dp = ImageDraw.Draw(ext), ImageDraw.Draw(intr), ImageDraw.Draw(pts_img)
    past = 0

    def px(p):
        return ((p[0] - x0) * zoom, (p[1] - y0) * zoom)

    for v, ta, tb, edge, straight in corners(cs):
        b = unit((ta[0] - tb[0], ta[1] - tb[1]))
        if b is None:
            continue
        n1 = (-ta[1], ta[0]) if -ta[1] * b[0] + ta[0] * b[1] > 0 else (ta[1], -ta[0])
        n2 = (-tb[1], tb[0]) if -tb[1] * b[0] + tb[0] * b[1] > 0 else (tb[1], -tb[0])
        c = max(1e-9, math.sqrt(max(0.0, (1 + n1[0] * n2[0] + n1[1] * n2[1]) / 2)))
        ratio = 1 / c
        probe = px((v[0] + 0.5 * b[0], v[1] + 0.5 * b[1]))
        iy, ix = int(round(probe[1])), int(round(probe[0]))
        if not (0 <= iy < fill.shape[0] and 0 <= ix < fill.shape[1]):
            continue
        exterior = not fill[iy, ix]
        whole = align != 'inside' and straight and edge >= h and ratio * h <= 5 * edge   # ise_miter::WholePoint
        tip = min(ratio, 500 if whole else LONGEST) * h
        poly = [px(v), px((v[0] + h * n1[0], v[1] + h * n1[1])), px((v[0] + tip * b[0], v[1] + tip * b[1])), px((v[0] + h * n2[0], v[1] + h * n2[1]))]
        (de if exterior else di).polygon(poly, fill=255)
        stroked_side = exterior if align != 'inside' else not exterior
        if stroked_side and ratio > LIMIT:
            past += 1
            if not whole:
                continue   # beveled by design, as InDesign bevels it
            reach = min(500 * h, ratio * h)
            # The point beyond the bevel: the miter wedge from the bevel line out to `reach`.
            bev = c * h   # the bevel line's distance from the vertex along the bisector
            k = reach / (ratio * h)
            q1 = (v[0] + h * n1[0], v[1] + h * n1[1])
            q2 = (v[0] + h * n2[0], v[1] + h * n2[1])
            tipp = (v[0] + ratio * h * b[0], v[1] + ratio * h * b[1])
            if k >= 1:
                dp.polygon([px(q1), px(tipp), px(q2)], fill=255)
            else:
                cut1 = (q1[0] + (tipp[0] - q1[0]) * (reach - bev) / (ratio * h - bev), q1[1] + (tipp[1] - q1[1]) * (reach - bev) / (ratio * h - bev))
                cut2 = (q2[0] + (tipp[0] - q2[0]) * (reach - bev) / (ratio * h - bev), q2[1] + (tipp[1] - q2[1]) * (reach - bev) / (ratio * h - bev))
                dp.polygon([px(q1), px(cut1), px(cut2), px(q2)], fill=255)
    ext = np.array(ext) > 0
    intr = np.array(intr) > 0
    pts = np.array(pts_img) > 0
    if align != 'inside':
        allowed |= ext & ~fill
    if align != 'outside':
        allowed |= intr & fill
    # The beveled point: the wedge's part farther than f beyond the band, on the stroke's side.
    side = ~fill if align != 'inside' else fill
    pts &= side & ((dout if align != 'inside' else din) > h * zoom + 2 * f)
    a = 1.0 / zoom ** 2
    owner = label_parts(fill, cell, zoom, parts)
    names = sorted(set(p['part'] for p in parts))
    diff = np.abs(native - corrected) > 64
    res = {'points_past_limit': past}
    for part in names + ['all']:
        sel = np.ones_like(fill) if part == 'all' else owner == names.index(part)
        r = {}
        for name, img in (('native', native), ('corrected', corrected)):
            k = classes(img)
            stroke = k == 2
            r[name] = {
                'escaping': round(float((sel & stroke & ~fill & ~allowed).sum() * a), 2),
                'stray': round(float((sel & stroke & fill & ~allowed).sum() * a), 2),
                'holes': round(float((sel & (k == 0) & fill & (din > f)).sum() * a), 2),
                'points_missing': round(float((sel & pts & ~stroke).sum() * a), 2),
                'spill': round(float((sel & (k == 1) & ~fill & (dout > f)).sum() * a), 2),
                'off': round(float((sel & (np.abs(img[..., None] - lv[None, None, :]).min(axis=2) > OFF)).sum() * a), 2),
            }
        r['points_area'] = round(float((sel & pts).sum() * a), 2)
        r['native_vs_corrected'] = round(float((sel & diff).sum() * a), 2)
        res[part] = r
    return res


def label_parts(fill, cell, zoom, parts):
    """Index (into the sorted part names) of the part whose fill is nearest each pixel."""
    names = sorted(set(p['part'] for p in parts))
    lab, n = ndimage.label(fill)
    comp = np.full(n + 1, 0)
    if n:
        cy = ndimage.center_of_mass(fill, lab, range(1, n + 1))
        for j, (yy, xx) in enumerate(cy):
            py, pxx = cell[0] + yy / zoom, cell[1] + xx / zoom
            best, bd = 0, 1e18
            for p in parts:
                b = p['geometryBounds']
                dy = max(b[0] - py, 0, py - b[2])
                dx = max(b[1] - pxx, 0, pxx - b[3])
                if dy * dy + dx * dx < bd:
                    bd, best = dy * dy + dx * dx, names.index(p['part'])
            comp[j + 1] = best
    if not fill.any():
        return np.zeros(fill.shape, int)
    _, (iy, ix) = ndimage.distance_transform_edt(~fill, return_indices=True)
    return comp[lab[iy, ix]]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('folder')
    ap.add_argument('--out')
    ap.add_argument('--zoom', type=float, default=8)
    ap.add_argument('--native', default='native')
    ap.add_argument('--corrected', default='corrected')
    a = ap.parse_args()
    d = a.folder.rstrip('/\\') + '/'
    m = json.load(open(d + 'coverage-census.manifest.json', encoding='utf-8'))
    nat = pymupdf.open(d + 'coverage-census-' + a.native + '.pdf')
    cor = pymupdf.open(d + 'coverage-census-' + a.corrected + '.pdf')
    geo = pymupdf.open(d + 'coverage-census-geometry.pdf')
    rows = []
    by_label = {r['label']: r for r in m['items']}
    for r in m['items']:
        lab = r['label']
        align = 'center' if 'center' in lab else ('inside' if 'inside' in lab else 'outside')
        levels = [BACKDROP, FILL, STROKE]
        if 'stroke opacity 50' in lab:
            levels = [BACKDROP, FILL, (STROKE + BACKDROP) // 2]
        elif 'fill opacity 50' in lab:
            levels = [BACKDROP, (FILL + BACKDROP) // 2, STROKE]
        elif 'opacity 50' in lab:
            levels = [BACKDROP, (FILL + BACKDROP) // 2, (STROKE + BACKDROP) // 2]
        if 'gradient fill' in lab:
            levels[1] = 65
        if 'gradient stroke' in lab:
            levels[2] = 228
        cell = r['cell']
        n = render(nat[0], cell, a.zoom)
        c = render(cor[0], cell, a.zoom)
        g = render(geo[0], cell, a.zoom)
        rec = {'row': r['row'], 'kind': r['kind'], 'case': lab, 'font': r.get('font'), 'align': align, 'weight': r['weight']}
        rec.update(measure(n, c, g, contours(geo[0], cell), cell, a.zoom, align, r['weight'], levels, r['items']))
        twin = by_label.get(lab + ' - twin, untransformed')
        if twin:
            t = render(nat[0], twin['cell'], a.zoom)
            rec['native_vs_twin'] = round(float((np.abs(n - t) > 64).sum() / a.zoom ** 2), 2)
        rows.append(rec)
        for part in [k for k in rec if isinstance(rec[k], dict) and 'native' in rec[k]]:
            q = rec[part]
            print(f"{r['row']:2d} {lab[:52]:52s} {part:7s} esc {q['native']['escaping']:7.2f}/{q['corrected']['escaping']:7.2f}"
                  f"  stray {q['native']['stray']:7.2f}/{q['corrected']['stray']:7.2f}  holes {q['native']['holes']:7.2f}/{q['corrected']['holes']:7.2f}"
                  f"  ptmiss {q['native']['points_missing']:6.2f}/{q['corrected']['points_missing']:6.2f}"
                  f"  spill {q['native']['spill']:6.2f}/{q['corrected']['spill']:6.2f}  off {q['native']['off']:6.2f}/{q['corrected']['off']:6.2f}"
                  f"  diff {q['native_vs_corrected']:7.2f}" + (f"  twin {rec['native_vs_twin']:.2f}" if twin and part == 'all' else ''))
    if a.out:
        out = {'app': m.get('app'), 'zoom': a.zoom, 'fringe_pt': FRINGE, 'limit': LIMIT, 'rows': rows}
        with open(a.out, 'w', encoding='utf-8', newline='\n') as fh:
            json.dump(out, fh, indent=1)
            fh.write('\n')


if __name__ == '__main__':
    main()
