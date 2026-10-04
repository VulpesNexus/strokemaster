"""How far a font's exterior miter points would reach past the miter limit, under three rules.

For each exterior corner of the glyph outlines (the join points away from the fill under the
nonzero rule, as in plugin/source/IseMiterCore.h), the miter ratio 1 / cos(turn / 2) and the
shorter of the two segments meeting there. Reports, for corners past the limit at a given text
size and half width, the point's reach under: the cut at the limit distance (route X 48e27348),
the whole miter capped at 500 half widths, and the whole miter capped by the shorter edge
(ise_miter::PointReach, route X 787656a3).

usage: measure_font_corners.py <font.ttf|otf> [--chars TEXT] [--size 72] [--half 4] [--limit 4]
"""
import argparse, math, string

import numpy as np
from fontTools.pens.basePen import BasePen
from fontTools.ttLib import TTFont


class Collect(BasePen):
    """Contours as cubic segments; quadratic pieces are raised to cubics exactly."""

    def __init__(self, glyph_set):
        super().__init__(glyph_set)
        self.contours, self.cur, self.p = [], None, None

    def _moveTo(self, p):
        self.cur = {'start': p, 'segs': []}
        self.contours.append(self.cur)
        self.p = p

    def _lineTo(self, p):
        self.cur['segs'].append(('l', None, None, p))
        self.p = p

    def _curveToOne(self, a, b, c):
        self.cur['segs'].append(('c', a, b, c))
        self.p = c

    def _qCurveToOne(self, a, b):
        p0 = self.p
        self._curveToOne((p0[0] + 2 / 3 * (a[0] - p0[0]), p0[1] + 2 / 3 * (a[1] - p0[1])),
                         (b[0] + 2 / 3 * (a[0] - b[0]), b[1] + 2 / 3 * (a[1] - b[1])), b)

    def _closePath(self):
        pass

    def _endPath(self):
        pass


def closed(c):
    """Zero-length segments dropped and the closing line made explicit, as ise_miter::Closed."""
    segs, p = [], c['start']
    for s in c['segs']:
        if math.dist(p, s[3]) > 1e-6 or (s[0] == 'c' and (math.dist(p, s[1]) > 1e-6 or math.dist(p, s[2]) > 1e-6)):
            segs.append(s)
        p = s[3]
    if math.dist(p, c['start']) > 1e-6:
        segs.append(('l', None, None, c['start']))
    return segs


def flatten(c, segs, n=32):
    pts, p = [c['start']], c['start']
    for s in segs:
        if s[0] == 'l':
            pts.append(s[3])
        else:
            q = np.array([p, s[1], s[2], s[3]], float)
            for t in np.linspace(0, 1, n)[1:]:
                pts.append(tuple((1 - t) ** 3 * q[0] + 3 * (1 - t) ** 2 * t * q[1] + 3 * (1 - t) * t * t * q[2] + t ** 3 * q[3]))
        p = s[3]
    return np.array(pts, float)


def winding(rings, q):
    w = 0
    for r in rings:
        a, b = r, np.roll(r, -1, axis=0)
        up = (a[:, 1] <= q[1]) & (b[:, 1] > q[1])
        dn = (a[:, 1] > q[1]) & (b[:, 1] <= q[1])
        cr = (b[:, 0] - a[:, 0]) * (q[1] - a[:, 1]) - (b[:, 1] - a[:, 1]) * (q[0] - a[:, 0])
        w += int(np.sum(up & (cr > 0))) - int(np.sum(dn & (cr < 0)))
    return w


def seg_length(p, s):
    """As ise_miter::SegmentLength: a curve's length is the mean of chord and control polygon."""
    if s[0] == 'l':
        return math.dist(p, s[3])
    return 0.5 * (math.dist(p, s[1]) + math.dist(s[1], s[2]) + math.dist(s[2], s[3]) + math.dist(p, s[3]))


def exterior_corners(font, chars):
    """(char, ratio, shorter edge in font units) for every exterior corner."""
    gs, cmap, upm = font.getGlyphSet(), font.getBestCmap(), font['head'].unitsPerEm
    out = []
    for ch in chars:
        name = cmap.get(ord(ch))
        if not name:
            continue
        pen = Collect(gs)
        gs[name].draw(pen)
        cs = [(c, closed(c)) for c in pen.contours if c['segs']]
        rings = [flatten(c, s) for c, s in cs]
        for c, segs in cs:
            n = len(segs)
            for i in range(n):
                s_in, s_out = segs[i], segs[(i + 1) % n]
                p_in = c['start'] if i == 0 else segs[i - 1][3]
                v = s_in[3]
                u = np.subtract(v, s_in[2] if s_in[0] == 'c' and math.dist(v, s_in[2]) > 1e-6 else p_in)
                w = np.subtract(s_out[1] if s_out[0] == 'c' and math.dist(v, s_out[1]) > 1e-6 else s_out[3], v)
                if np.linalg.norm(u) < 1e-9 or np.linalg.norm(w) < 1e-9:
                    continue
                u, w = u / np.linalg.norm(u), w / np.linalg.norm(w)
                cross = u[0] * w[1] - u[1] * w[0]
                turn = math.atan2(abs(cross), u @ w)
                if turn < math.radians(0.5) or abs(cross) < 1e-15:
                    continue   # an invisible join, or an exact reversal (skipped by the core too)
                bis = u - w
                if winding(rings, np.add(v, bis / np.linalg.norm(bis) * upm * 1e-4)) != 0:
                    continue   # interior: the join points into the fill
                out.append((ch, 1 / math.cos(turn / 2), min(seg_length(p_in, s_in), seg_length(v, s_out))))
    return out, upm


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('font')
    ap.add_argument('--chars', default=string.ascii_letters + string.digits)
    ap.add_argument('--size', type=float, default=72, help='text size, pt')
    ap.add_argument('--half', type=float, default=4, help='half the drawn stroke width, pt (the Outside weight)')
    ap.add_argument('--limit', type=float, default=4)
    a = ap.parse_args()
    corners, upm = exterior_corners(TTFont(a.font), a.chars)
    past = [(ch, r, e * a.size / upm) for ch, r, e in corners if r > a.limit]
    print(f"{len(corners)} exterior corners; {len(past)} past limit {a.limit:g} "
          f"(at {a.size:g} pt, half width {a.half:g} pt)")
    if not past:
        return
    r = np.array([x[1] for x in past])
    e = np.array([x[2] for x in past])
    whole = r * a.half
    print('shorter edge, pt, 10/25/50/75/90%:', np.percentile(e, [10, 25, 50, 75, 90]).round(2))
    for name, reach in (('cut at the limit', np.full_like(r, a.limit * a.half)),
                        ('whole, cap 500', np.minimum(whole, 500 * a.half)),
                        ('whole, cap shorter edge', np.minimum(500 * a.half, np.maximum(a.limit * a.half, np.minimum(whole, e))))):
        print(f"  {name:24} reach pt: median {np.median(reach):8.2f}  90% {np.percentile(reach, 90):8.2f}  "
              f"max {reach.max():8.2f}  beyond the limit distance: {int((reach > a.limit * a.half + 1e-9).sum())}")


if __name__ == '__main__':
    main()
