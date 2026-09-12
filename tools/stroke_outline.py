"""Experimental stroke-outline raster tools with tested local join geometry.

Input paths contain anchors and Bezier handles. Fixed 24-step flattening and
absolute handle/corner thresholds remain experimental. Handles classify corners;
actual joins use adjacent flattened chords. Contours are always closed, and
holes, open caps, self-intersections, and Boolean topology are not resolved.
The existing validate command compares conventional miters with a saved Adobe
expansion; it does not validate clipping or certify whole-stroke production use.
Lengths, including hw (half the full stroke width), use input-coordinate units.
"""
import json
import math
import sys

from PIL import Image, ImageChops, ImageDraw

SUB = 24           # flattening steps per curved segment
CORNER_DEG = 0.75  # tangent divergence above which an anchor counts as a corner
SS = 3             # supersampling factor for rasterizing

# Local joins share the Python kernel; JSX is cross-checked separately.
from miter_rule import (CLIP_DEFAULT, CLIP_EPS, RIGHT_ANGLE_RATIO,  # noqa: E402,F401
                        interior_angle, miter_ratio, join_boundary)


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
    """Filled local outer join, or None for straight travel; invalid inputs raise ValueError.

    u/w are incoming/outgoing unit directions. clip_ratio and limit use half-width
    units; clipping bounds the bisector projection, not every radial distance.
    """
    boundary = join_boundary(v, u, w, hw, mode, clip_ratio, limit)
    return [tuple(v)] + boundary if boundary else None


def stroke_shapes(pts, hw, mode, clip_ratio=CLIP_DEFAULT, limit=4.0):
    """Experimental closed-contour pieces; no whole-stroke topology guarantee."""
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
    rules = [('miter L4', 'miter', CLIP_DEFAULT, 4.0), ('miter L2', 'miter', CLIP_DEFAULT, 2.0),
             ('clip 1.0', 'clip', 1.0, 4.0), ('clip sqrt2', 'clip', CLIP_DEFAULT, 4.0),
             ('clip 2.0', 'clip', 2.0, 4.0), ('round', 'round', CLIP_DEFAULT, 4.0)]
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
        for poly in stroke_shapes(glyph['pts'], hw, 'miter', CLIP_DEFAULT, 4.0):
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
