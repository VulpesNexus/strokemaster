"""Whether one miter limit per glyph reproduces InDesign's glyph-tip rule natively.

The InDesign plugin trims a glyph's tips (ise_miter::TrimmedTip: a corner of ratio 2 or more with
a curve on either side or an edge shorter than the half width h, or of ratio 1.5 or more with a
curve beside an edge shorter than h) and keeps every other miter whole within the limit.
Illustrator has no per-corner rule, but each character has its own miter limit, a native
attribute that follows edits. A limit L bevels exactly the corners whose ratio exceeds L, so a
glyph is reproduced when every trimmed corner's ratio is above every kept corner's ratio (within
the document's limit). For each glyph: the kept maximum, the trimmed minimum, and a limit between
them, or "conflict" when a kept corner is sharper than a trimmed one.

usage: classify_glyph_tips.py <font> [--chars TEXT] [--size 36] [--half 4] [--limit 4] [--json OUT]
"""
import argparse
import json
import math

import numpy as np
from fontTools.ttLib import TTFont

from measure_font_corners import Collect, closed, flatten, seg_length, winding


def corners(font, ch):
    """(ratio, in-curve, out-curve, shorter edge in font units) for each exterior corner of ch."""
    gs, cmap = font.getGlyphSet(), font.getBestCmap()
    upm = font['head'].unitsPerEm
    name = cmap.get(ord(ch))
    if not name:
        return None
    pen = Collect(gs)
    gs[name].draw(pen)
    cs = [(c, closed(c)) for c in pen.contours if c['segs']]
    rings = [flatten(c, s) for c, s in cs]
    out = []
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
                continue
            bis = u - w
            if winding(rings, np.add(v, bis / np.linalg.norm(bis) * upm * 1e-4)) != 0:
                continue
            out.append((1 / math.cos(turn / 2), s_in[0] == 'c', s_out[0] == 'c',
                        min(seg_length(p_in, s_in), seg_length(v, s_out))))
    return out


def trimmed(ratio, in_curve, out_curve, edge, h):
    """ise_miter::TrimmedTip."""
    curve = in_curve or out_curve
    return (ratio >= 2 and (curve or edge < h)) or (ratio >= 1.5 and curve and edge < h)


def classify(path, chars, size, half, limit):
    font = TTFont(path, fontNumber=0)
    upm = font['head'].unitsPerEm
    rows = []
    for ch in chars:
        cs = corners(font, ch)
        if cs is None:
            continue
        kept, trim = [], []
        for ratio, ic, oc, edge in cs:
            if ratio > limit:
                continue   # past the limit: beveled natively and by InDesign's flush cut alike
            (trim if trimmed(ratio, ic, oc, edge * size / upm, half) else kept).append(ratio)
        kmax = max(kept) if kept else 1.0
        tmin = min(trim) if trim else None
        if tmin is None:
            verdict, chosen = 'no tips', limit
        elif kmax < tmin:
            verdict, chosen = 'separable', round((kmax + tmin) / 2, 3)
        else:
            verdict, chosen = 'conflict', None
        rows.append({'char': ch, 'kept_max': round(kmax, 3), 'trim_min': None if tmin is None else round(tmin, 3),
                     'trimmed': len(trim), 'kept': len(kept), 'verdict': verdict, 'limit': chosen})
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('font')
    ap.add_argument('--chars', default='VWAMNvwkyzKRibonsAgel')
    ap.add_argument('--size', type=float, default=36)
    ap.add_argument('--half', type=float, default=4)
    ap.add_argument('--limit', type=float, default=4)
    ap.add_argument('--json')
    a = ap.parse_args()
    rows = classify(a.font, a.chars, a.size, a.half, a.limit)
    for r in rows:
        print(f"{r['char']}  kept<= {r['kept_max']:5.2f}  trim>= {str(r['trim_min']):6}  {r['verdict']:9}  limit {r['limit']}")
    counts = {v: sum(1 for r in rows if r['verdict'] == v) for v in ('no tips', 'separable', 'conflict')}
    print(counts)
    if a.json:
        with open(a.json, 'w', encoding='utf-8', newline='\n') as fh:
            json.dump({'font': a.font.replace('\\', '/').split('/')[-1], 'size': a.size, 'half': a.half, 'limit': a.limit,
                       'rows': rows, 'counts': counts}, fh, indent=1)


if __name__ == '__main__':
    main()
