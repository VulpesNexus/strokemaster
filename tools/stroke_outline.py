"""Build a stroke's outline from a path, with a choice of join rule, and check it against Adobe.

Usage:
    python tools/stroke_outline.py render   <paths.json> <out.png> [glyph-index] [half-width]
    python tools/stroke_outline.py validate <paths.json> <out.png> [half-width]

The input is what tools/jsx/ai-dump-paths.jsx writes: each path's points, with anchors and both
Bezier handles.

Why this exists: PostScript and PDF offer three joins, and the miter is governed by a THRESHOLD.
A join is mitered while 1 / sin(theta / 2) <= limit and becomes a full bevel past it, so a corner
just inside the limit keeps its whole point while one just outside loses it entirely. On real
type that is visible as spikes on the acute corners and blunt stumps on the ones just past the
limit, and the only dial available, the miter limit, moves the cliff rather than removing it.

A clamped miter removes the cliff: keep the miter, but cut its tip off flat once it would reach
further than clip_ratio times the stroke's half-width. It cannot be expressed as a PDF or
PostScript line join, each of which defines exactly three, so it has to be built as geometry,
which is what this does.

The behavior is ordinary elsewhere: GDI+, Direct2D, WPF, XPS, FreeType and Qt all clamp by
default, and have since GDI+ shipped. It is not a web standard any more, though. SVG 2 specified
it as stroke-linejoin: miter-clip in 2018, no browser implemented it, and the Working Group
removed it on 18 March 2026, so do not cite it under that name. See docs/MITER_JOIN_HISTORY.md.

Nothing here is fitted to a particular glyph or size. Every quantity is derived per corner from
the path and the stroke width, so the same code answers for any font, any weight, and any angle.

The default clamp is sqrt(2), the ratio a right angle produces, which states the rule as "no
corner may protrude further than a right-angled corner would". A right angle is therefore the
reference and is left alone: the clamp acts below 90 degrees and not at or above it.

Measured by sweeping every degree from 1 to 179 through join_polygon itself and taking the
farthest point each join actually returns: the native rule at limit 4 changes reach by 23.95 pt
between 28 and 29 degrees on an 8 pt half-width, where it stops mitering and bevels instead,
while this clamp's largest step anywhere in the sweep is 0.0975 pt. Reach divided by half-width
is identical across half-widths of 4, 8, and 12 to five decimal places. Continuous in the angle,
and exactly proportional to the weight.

(An earlier sweep reported that cliff as 30.02 pt. It computed the beveled case analytically as
the distance to the chord's midpoint rather than to the join's farthest point, which understates
where a bevel actually reaches and so overstated the jump. 23.95 pt is the measured figure.)

Two details decide whether it works on real type:

  Corners come from the anchors, not from the flattening. Once a curve is flattened every vertex
  has a small angle change, and applying a join rule to those would facet the curved parts. Only
  anchors whose incoming and outgoing tangents actually diverge are corners.

  Tangents come from the Bezier handles. On a curved segment the direction at an anchor is set
  by its handle, so measuring toward the neighboring anchor would invent corners where the
  outline is smooth and understate the ones that are real.

Verified against Illustrator's own Expand on a 50 pt Times W with a 16 pt centered stroke: the
region this computes and the region Illustrator produced differ by 95 pixels out of 86,650, or
0.11 percent, all of it a one-pixel seam along the boundary.
"""
import json
import math
import sys

from PIL import Image, ImageChops, ImageDraw

SUB = 24           # flattening steps per curved segment
CORNER_DEG = 0.75  # tangent divergence above which an anchor counts as a corner
SS = 3             # supersampling factor for rasterizing

# The rule and its constants live in miter_rule.py, so this file and tools/miter_geometry.py
# cannot come to disagree about what the clamp is.
from miter_rule import (CLIP_DEFAULT, CLIP_EPS, RIGHT_ANGLE_RATIO,  # noqa: E402,F401
                        interior_angle, miter_ratio)


def bez(p0, p1, p2, p3, t):
    m = 1.0 - t
    return (m * m * m * p0[0] + 3 * m * m * t * p1[0] + 3 * m * t * t * p2[0] + t * t * t * p3[0],
            m * m * m * p0[1] + 3 * m * m * t * p1[1] + 3 * m * t * t * p2[1] + t * t * t * p3[1])


def unit(v):
    n = math.hypot(v[0], v[1])
    return (v[0] / n, v[1] / n) if n > 1e-12 else None


def tangents(pts, i):
    """Incoming and outgoing unit tangents at anchor i, taken from its handles."""
    n = len(pts)
    cur, prev, nxt = pts[i], pts[(i - 1) % n], pts[(i + 1) % n]
    a, lh, rh = cur['a'], cur['l'], cur['r']
    din = (a[0] - lh[0], a[1] - lh[1])
    if math.hypot(*din) < 1e-6:
        din = (a[0] - prev['a'][0], a[1] - prev['a'][1])
    dout = (rh[0] - a[0], rh[1] - a[1])
    if math.hypot(*dout) < 1e-6:
        dout = (nxt['a'][0] - a[0], nxt['a'][1] - a[1])
    return unit(din), unit(dout)


def flatten(pts, tag_corners=True):
    """The outline as vertices, each flagged as a corner or a smooth point."""
    out = []
    n = len(pts)
    for i in range(n):
        cur, nxt = pts[i], pts[(i + 1) % n]
        corner = False
        if tag_corners:
            u, w = tangents(pts, i)
            corner = bool(u and w and (180.0 - interior_angle(u, w)) > CORNER_DEG)
        out.append((cur['a'][0], cur['a'][1], corner))
        p0, p1 = cur['a'], cur['r']
        p3, p2 = nxt['a'], nxt['l']
        straight = math.hypot(p1[0] - p0[0], p1[1] - p0[1]) < 1e-6 and \
            math.hypot(p2[0] - p3[0], p2[1] - p3[1]) < 1e-6
        if not straight:
            for s in range(1, SUB):
                x, y = bez(p0, p1, p2, p3, s / float(SUB))
                out.append((x, y, False))
    return out


def join_polygon(v, u, w, hw, mode, clip_ratio=CLIP_DEFAULT, limit=4.0):
    """The filled wedge that closes the outer side of one corner.

    mode is 'miter' (threshold, what PDF does), 'bevel', 'round', or 'clip' (the clamp).
    """
    cross = u[0] * w[1] - u[1] * w[0]
    if abs(cross) < 1e-12:
        return None
    # The join is built on the outside of the turn, which is the side the corner opens away from.
    if cross < 0:
        n1, n2 = (-u[1], u[0]), (-w[1], w[0])
    else:
        n1, n2 = (u[1], -u[0]), (w[1], -w[0])
    p1 = (v[0] + n1[0] * hw, v[1] + n1[1] * hw)
    p2 = (v[0] + n2[0] * hw, v[1] + n2[1] * hw)

    if mode == 'bevel':
        return [v, p1, p2]
    if mode == 'round':
        a1, a2 = math.atan2(n1[1], n1[0]), math.atan2(n2[1], n2[0])
        while a2 - a1 > math.pi:
            a2 -= 2 * math.pi
        while a1 - a2 > math.pi:
            a2 += 2 * math.pi
        arc = [v]
        for k in range(13):
            a = a1 + (a2 - a1) * k / 12.0
            arc.append((v[0] + math.cos(a) * hw, v[1] + math.sin(a) * hw))
        return arc

    bx, by = n1[0] + n2[0], n1[1] + n2[1]
    bl = math.hypot(bx, by)
    if bl < 1e-9:
        return [v, p1, p2]
    b = (bx / bl, by / bl)
    ratio = miter_ratio(interior_angle(u, w))
    tip_at = hw * ratio
    tip = (v[0] + b[0] * tip_at, v[1] + b[1] * tip_at)

    if mode == 'miter':
        return [v, p1, tip, p2] if ratio <= limit else [v, p1, p2]

    cap = hw * clip_ratio
    if tip_at <= cap * (1.0 + CLIP_EPS):
        return [v, p1, tip, p2]
    # Cut the miter off flat at the cap. Interpolating along both miter edges keeps the corner
    # pointing in the same direction and tapers continuously as the angle closes, which is the
    # whole difference from a bevel.
    t = (cap - hw) / (tip_at - hw) if tip_at > hw else 0.0
    a = (p1[0] + (tip[0] - p1[0]) * t, p1[1] + (tip[1] - p1[1]) * t)
    c = (p2[0] + (tip[0] - p2[0]) * t, p2[1] + (tip[1] - p2[1]) * t)
    return [v, p1, a, c, p2]


def stroke_shapes(pts, hw, mode, clip_ratio=CLIP_DEFAULT, limit=4.0):
    """Every polygon whose union is the stroked region: one per segment, one per vertex."""
    flat = flatten(pts)
    n = len(flat)
    shapes = []
    for i in range(n):
        x0, y0, _ = flat[i]
        x1, y1, _ = flat[(i + 1) % n]
        d = unit((x1 - x0, y1 - y0))
        if d is None:
            continue
        nx, ny = -d[1] * hw, d[0] * hw
        shapes.append([(x0 + nx, y0 + ny), (x1 + nx, y1 + ny), (x1 - nx, y1 - ny), (x0 - nx, y0 - ny)])
    for i in range(n):
        x, y, corner = flat[i]
        prev, nxt = flat[(i - 1) % n], flat[(i + 1) % n]
        u = unit((x - prev[0], y - prev[1]))
        w = unit((nxt[0] - x, nxt[1] - y))
        if u is None or w is None:
            continue
        # Smooth points get a round join: the rule under test applies to real corners only.
        poly = join_polygon((x, y), u, w, hw, mode if corner else 'round', clip_ratio, limit)
        if poly:
            shapes.append(poly)
    return shapes


def contour(pts):
    """A path as a dense polygon, curves included, for filling."""
    return [(x, y) for x, y, _ in flatten(pts, tag_corners=False)]


def _transform(gb, pad, scale, ssf):
    def tx(p):
        return ((p[0] - gb[0] + pad) * scale * ssf, (gb[1] + pad - p[1]) * scale * ssf)
    return tx


def _mask(size, ssf, draw_fn):
    big = Image.new('L', (size[0] * ssf, size[1] * ssf), 0)
    draw_fn(ImageDraw.Draw(big))
    return big.resize(size, Image.LANCZOS)


def render(json_path, out_path, index=0, hw=8.0):
    doc = json.load(open(json_path, encoding='utf-8'))
    path = [p for p in doc['paths'] if p['index'] == index][0]
    pts, gb = path['pts'], path['gb']
    rules = [('miter L4', 'miter', 0, 4.0), ('miter L2', 'miter', 0, 2.0),
             ('clip 1.0', 'clip', 1.0, 4.0), ('clip sqrt2', 'clip', CLIP_DEFAULT, 4.0),
             ('clip 2.0', 'clip', 2.0, 4.0), ('round', 'round', 0, 4.0)]
    scale, pad = 5.0, hw + 22
    pw = int(((gb[2] - gb[0]) + pad * 2) * scale)
    ph = int(((gb[1] - gb[3]) + pad * 2) * scale)
    img = Image.new('RGB', (pw * len(rules), ph + 26), (255, 255, 255))
    top = ImageDraw.Draw(img)
    tx = _transform(gb, pad, scale, SS)
    for r, (label, mode, clip, limit) in enumerate(rules):
        def paint(d, mode=mode, clip=clip, limit=limit):
            for poly in stroke_shapes(pts, hw, mode, clip, limit):
                d.polygon([tx(q) for q in poly], fill=255)
            d.polygon([tx(q) for q in contour(pts)], fill=255)
        panel = Image.new('RGB', (pw, ph), (255, 255, 255))
        panel.paste((0, 0, 0), (0, 0), _mask((pw, ph), SS, paint))
        img.paste(panel, (pw * r, 26))
        top.text((pw * r + 8, 8), label, fill=(0, 0, 0))
    img.save(out_path)
    print('wrote %s  (%d rules, half-width %.2f pt)' % (out_path, len(rules), hw))


def validate(json_path, out_path, hw=8.0):
    """Compare a plain miter at limit 4 against Adobe's own expansion of the same stroke."""
    doc = json.load(open(json_path, encoding='utf-8'))
    glyph = [p for p in doc['paths'] if p['index'] == 0][0]
    expanded = [p for p in doc['paths'] if p['index'] == 1][0]
    gb = expanded['gb']
    scale, pad = 6.0, 6.0
    pw = int(((gb[2] - gb[0]) + pad * 2) * scale)
    ph = int(((gb[1] - gb[3]) + pad * 2) * scale)
    tx = _transform(gb, pad, scale, SS)

    def mine(d):
        for poly in stroke_shapes(glyph['pts'], hw, 'miter', 0, 4.0):
            d.polygon([tx(q) for q in poly], fill=255)
        d.polygon([tx(q) for q in contour(glyph['pts'])], fill=255)

    def theirs(d):
        d.polygon([tx(q) for q in contour(expanded['pts'])], fill=255)

    flat = lambda v: 255 if v > 127 else 0
    a = _mask((pw, ph), SS, mine).point(flat)
    b = _mask((pw, ph), SS, theirs).point(flat)
    diff = ImageChops.difference(a, b)
    area_a = sum(a.histogram()[128:])
    area_b = sum(b.histogram()[128:])
    differing = sum(diff.histogram()[128:])
    print('computed region     : %d px' % area_a)
    print("Adobe's expansion   : %d px" % area_b)
    print('differing           : %d px  (%.3f%% of their area)' % (
        differing, 100.0 * differing / max(1, area_b)))
    print('  only computed     : %d px' % sum(ImageChops.subtract(a, b).histogram()[128:]))
    print('  only Adobe        : %d px' % sum(ImageChops.subtract(b, a).histogram()[128:]))
    out = Image.new('RGB', (pw * 3, ph), (255, 255, 255))
    out.paste(Image.merge('RGB', (a, a, a)).point(lambda v: 255 - v), (0, 0))
    out.paste(Image.merge('RGB', (b, b, b)).point(lambda v: 255 - v), (pw, 0))
    grey = b.point(lambda v: 255 - v // 3)
    over = Image.merge('RGB', (grey, grey, grey))
    over.paste(Image.new('RGB', (pw, ph), (220, 30, 30)), (0, 0), diff)
    out.paste(over, (pw * 2, 0))
    out.save(out_path)
    print('wrote %s  (computed | Adobe | disagreement in red)' % out_path)


if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'render'
    if cmd == 'validate':
        validate(sys.argv[2], sys.argv[3], float(sys.argv[4]) if len(sys.argv) > 4 else 8.0)
    else:
        render(sys.argv[2], sys.argv[3],
               int(sys.argv[4]) if len(sys.argv) > 4 else 0,
               float(sys.argv[5]) if len(sys.argv) > 5 else 8.0)
