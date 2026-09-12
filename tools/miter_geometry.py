"""Reference geometry for a clipped miter join, and a picture of what it does.

Usage:
    python tools/miter_geometry.py [out-dir]

Writes miter-joins.svg and prints a reach table. No dependencies outside the standard library,
so this can be checked without InDesign or the measurement stack.

The problem, in one line: a miter join's tip sits at halfwidth / sin(theta / 2) from the vertex,
which grows without bound as the angle closes, while the stroke reaches only halfwidth anywhere
else. On a W the counter apexes are acute enough that their tips punch out past the letter's own
outer edges.

PostScript and PDF offer one control, the miter limit, and it is a THRESHOLD: a join is mitered
while 1 / sin(theta / 2) <= limit and becomes a full bevel past that. So lowering the limit to
tame a spike also flattens every corner just past the threshold, and the change is a jump rather
than a taper. Fixture B measures both halves of that: at limit 4 the spike reaches 6.8 pt on a
stroke whose half-width is 4.0, and at limit 1 it reaches 4.03, the half-width exactly, because
every join has become a bevel.

What is wanted instead is a CLAMP: keep the miter, but cut its tip off flat at a chosen distance.
It is not expressible as a PDF or PostScript line join, each of which defines exactly three, so
it has to be built as geometry.

That behavior is ordinary elsewhere: GDI+, Direct2D, WPF, XPS, FreeType and Qt all clamp by
default. It is not a web standard any more, though. SVG 2 specified it as stroke-linejoin:
miter-clip in 2018, no browser implemented it, and the Working Group removed it on 18 March
2026, so do not cite it under that name. See docs/MITER_JOIN_HISTORY.md.

Distances below are all measured from the vertex along the outward bisector, in the same units as
the stroke width.
"""
import math
import os
import sys

# Fixture B, B3 wedges: apex angle, the miter and bevel distances predicted for a half-width of
# 4.0, and what was actually measured on the page with a Center stroke of 8 pt at limit 4.
# Used here to check the formulas against the host rather than against themselves.
B3 = [
    # angle, theory miter, theory bevel, measured extension
    (10, 45.89, 0.35, 0.4),
    (15, 30.65, 0.52, 0.5),
    (20, 23.04, 0.69, 0.7),
    (25, 18.48, 0.87, 0.9),
    (28, 16.53, 0.97, 1.0),
    (30, 15.45, 1.04, 15.3),
    (35, 13.30, 1.20, 13.2),
    (40, 11.70, 1.37, 11.6),
    (50, 9.46, 1.69, 9.4),
    (60, 8.00, 2.00, 7.9),
]
HALFWIDTH = 4.0
LIMIT = 4.0

# The rule and its constants live in one place, so this file and tools/stroke_outline.py cannot
# come to disagree about what the clamp is.
from miter_rule import (RIGHT_ANGLE_RATIO, bevel_reach, clipped_reach,  # noqa: E402
                        miter_ratio, miter_reach, native_reach, round_reach)


def corner_polygon(vertex, d_in, d_out, hw, mode, clip_ratio=2.0, limit=4.0):
    """The outer boundary of one join, as a list of points.

    vertex is the corner, d_in the unit direction arriving at it, d_out the unit direction
    leaving it. The outward side is taken to be the left of travel, which is the convex side of
    the corners this is built for.
    """
    def left(v):
        return (-v[1], v[0])

    def add(p, v, s):
        return (p[0] + v[0] * s, p[1] + v[1] * s)

    n_in, n_out = left(d_in), left(d_out)
    p_in = add(vertex, n_in, hw)
    p_out = add(vertex, n_out, hw)

    bx, by = n_in[0] + n_out[0], n_in[1] + n_out[1]
    blen = math.hypot(bx, by)
    if blen < 1e-12:
        return [p_in, p_out]
    bis = (bx / blen, by / blen)

    cos_half = max(-1.0, min(1.0, (n_in[0] * n_out[0] + n_in[1] * n_out[1])))
    half = math.acos(cos_half) / 2.0
    theta_deg = 180.0 - math.degrees(half * 2.0)
    ratio = miter_ratio(theta_deg)

    if mode == 'bevel':
        return [p_in, p_out]
    if mode == 'round':
        steps, arc = 16, []
        a_in = math.atan2(n_in[1], n_in[0])
        a_out = math.atan2(n_out[1], n_out[0])
        while a_out < a_in:
            a_out += 2 * math.pi
        for i in range(steps + 1):
            a = a_in + (a_out - a_in) * i / steps
            arc.append(add(vertex, (math.cos(a), math.sin(a)), hw))
        return arc
    if mode == 'miter':
        if ratio > limit:
            return [p_in, p_out]
        return [p_in, add(vertex, bis, hw * ratio), p_out]

    # Clamped miter. Under the clamp it is an ordinary miter; past it the tip is cut off flat,
    # which keeps the corner pointing while capping how far it reaches.
    tip_at = hw * ratio
    cap = hw * clip_ratio
    tip = add(vertex, bis, tip_at)
    if tip_at <= cap:
        return [p_in, tip, p_out]
    t = (cap - hw) / (tip_at - hw) if tip_at > hw else 0.0
    a = (p_in[0] + (tip[0] - p_in[0]) * t, p_in[1] + (tip[1] - p_in[1]) * t)
    b = (p_out[0] + (tip[0] - p_out[0]) * t, p_out[1] + (tip[1] - p_out[1]) * t)
    return [p_in, a, b, p_out]


def wedge(theta_deg, hw, mode, **kw):
    """One symmetric wedge pointing up, drawn as its outer stroke boundary."""
    half = math.radians(theta_deg) / 2.0
    apex = (0.0, 0.0)
    d_in = (math.sin(half), math.cos(half))
    d_out = (math.sin(half), -math.cos(half))
    return corner_polygon(apex, d_in, d_out, hw, mode, **kw)


def check():
    print("Checking the formulas against fixture B's measured wedges (half-width 4.0, limit 4):")
    print("%6s %10s %10s %10s %10s %9s" % ("angle", "ratio", "predicted", "measured", "delta", "join"))
    worst = 0.0
    for angle, th_miter, th_bevel, measured in B3:
        predicted = native_reach(HALFWIDTH, angle, LIMIT)
        expected = th_miter if miter_ratio(angle) <= LIMIT else th_bevel
        delta = abs(predicted - measured)
        worst = max(worst, delta)
        print("%6d %10.3f %10.2f %10.2f %10.2f %9s" % (
            angle, miter_ratio(angle), predicted, measured, delta,
            "miter" if miter_ratio(angle) <= LIMIT else "bevel"))
        assert abs(predicted - expected) < 0.02, (angle, predicted, expected)
    print("largest disagreement with the host: %.2f pt\n" % worst)
    return worst


def table(hw=4.0):
    print("Reach from the vertex, half-width %.1f, against what each rule gives:" % hw)
    print("%6s %9s %9s %9s %9s %9s %9s" % (
        "angle", "ratio", "miter", "native L4", "clip sqrt2", "clip 2", "round/bevel cap"))
    for angle in (10, 15, 20, 25, 30, 45, 60, 90, 120):
        print("%6d %9.3f %9.2f %9.2f %9.2f %9.2f %9.2f" % (
            angle, miter_ratio(angle), miter_reach(hw, angle), native_reach(hw, angle, 4.0),
            clipped_reach(hw, angle, RIGHT_ANGLE_RATIO), clipped_reach(hw, angle, 2.0),
            round_reach(hw)))
    print()


def svg(out_dir, hw=4.0):
    angles = [10, 15, 20, 30, 45, 60, 90]
    modes = [('miter L4', {}), ('bevel', {}), ('round', {}),
             ('clip sqrt2', {'clip_ratio': RIGHT_ANGLE_RATIO}), ('clip 2.0', {'clip_ratio': 2.0})]
    colw, rowh, scale, pad = 150, 150, 3.0, 40
    w = pad * 2 + colw * len(angles)
    h = pad * 2 + rowh * len(modes)
    parts = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
             'viewBox="0 0 %d %d"><rect width="%d" height="%d" fill="#f4f4f4"/>' % (w, h, w, h, w, h)]
    for r, (label, kw) in enumerate(modes):
        mode = label.split()[0]
        cy = pad + rowh * r + rowh * 0.72
        parts.append('<text x="6" y="%d" font-family="sans-serif" font-size="11" fill="#333">%s</text>'
                     % (pad + rowh * r + 14, label))
        for c, angle in enumerate(angles):
            cx = pad + colw * c + colw * 0.5
            pts = wedge(angle, hw, mode, **kw)
            d = ' '.join('%.3f,%.3f' % (cx + x * scale, cy - y * scale) for x, y in pts)
            # The corner is a peak: the path arrives from below left, turns at the apex, and
            # leaves below right, so the outward side is up. Drawing both arms to the same side
            # would show a different corner from the one the join is computed for.
            half = math.radians(angle) / 2.0
            leg = 18.0
            a1 = (cx - math.sin(half) * leg * scale, cy + math.cos(half) * leg * scale)
            a2 = (cx + math.sin(half) * leg * scale, cy + math.cos(half) * leg * scale)
            parts.append('<polyline points="%.3f,%.3f %.3f,%.3f %.3f,%.3f" fill="none" '
                         'stroke="#bbb" stroke-width="1"/>' % (a1[0], a1[1], cx, cy, a2[0], a2[1]))
            parts.append('<polyline points="%s" fill="none" stroke="#c0392b" stroke-width="2"/>' % d)
            parts.append('<circle cx="%.3f" cy="%.3f" r="%.3f" fill="none" stroke="#888" '
                         'stroke-dasharray="3 3" stroke-width="1"/>' % (cx, cy, hw * scale))
            if r == 0:
                parts.append('<text x="%.1f" y="%d" text-anchor="middle" font-family="sans-serif" '
                             'font-size="11" fill="#333">%d deg</text>' % (cx, pad - 12, angle))
    parts.append('</svg>')
    path = os.path.join(out_dir, 'miter-joins.svg')
    with open(path, 'w', encoding='utf-8') as f:
        f.write(''.join(parts))
    print('wrote %s' % path)
    print('The dashed circle is the stroke\'s own reach. Anything outside it is a spike.')


def png(out_dir, hw=4.0):
    """The same comparison as a raster, so it can be looked at without an SVG viewer."""
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print('Pillow is not available; skipping the PNG')
        return
    angles = [10, 15, 20, 30, 45, 60, 90]
    modes = [('miter L4', {}), ('bevel', {}), ('round', {}),
             ('clip 1.0', {'clip_ratio': 1.0}), ('clip sqrt2', {'clip_ratio': RIGHT_ANGLE_RATIO}),
             ('clip 2.0', {'clip_ratio': 2.0})]
    colw, rowh, scale, padx, pady = 170, 150, 3.0, 96, 34
    w = padx + colw * len(angles) + 20
    h = pady + rowh * len(modes) + 20
    img = Image.new('RGB', (w, h), (244, 244, 244))
    dr = ImageDraw.Draw(img)
    for r, (label, kw) in enumerate(modes):
        mode = label.split()[0]
        cy = pady + rowh * r + rowh * 0.66
        dr.text((8, pady + rowh * r + 60), label, fill=(40, 40, 40))
        for c, angle in enumerate(angles):
            cx = padx + colw * c + colw * 0.5
            if r == 0:
                dr.text((cx - 14, 10), '%d deg' % angle, fill=(40, 40, 40))
            # Both arms go downward from the apex: the corner is a peak, and its outward side is
            # up, which is the side the join is built on.
            half = math.radians(angle) / 2.0
            leg = 16.0
            dr.line([(cx - math.sin(half) * leg * scale, cy + math.cos(half) * leg * scale),
                     (cx, cy),
                     (cx + math.sin(half) * leg * scale, cy + math.cos(half) * leg * scale)],
                    fill=(170, 170, 170), width=1)
            rr = hw * scale
            dr.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], outline=(150, 150, 150))
            pts = wedge(angle, hw, mode, **kw)
            dr.line([(cx + x * scale, cy - y * scale) for x, y in pts], fill=(192, 57, 43), width=3)
    path = os.path.join(out_dir, 'miter-joins.png')
    img.save(path)
    print('wrote %s' % path)


if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else '.'
    check()
    table()
    svg(out)
    png(out)
