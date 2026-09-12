"""Diagnostic join drawings and inherited conventional-miter measurements.

The shared Python join kernel is not an independent oracle. The analytic tests
in tests/geometry are the clipping evidence. B3 values below are historical input,
not a fresh host measurement or clipped-join validation. Distances are lengths
in input-coordinate units; hw is half the full stroke width. Axial and radial
extent differ for flat clips. No whole-stroke production claim is made here.
"""
import math
import os
import sys

# Fixture B, B3 wedges: apex angle, the miter and bevel distances predicted for a half-width of
# 4.0, and what was actually measured on the page with a Center stroke of 8 pt at limit 4.
# Historical comparison data only; no host is run by this module.
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

# This visualization uses the shared kernel, not an independent reference.
from miter_rule import (RIGHT_ANGLE_RATIO, bevel_reach, clipped_reach,  # noqa: E402
                        miter_ratio, miter_reach, native_reach, round_reach, join_boundary, _sine)


def corner_polygon(vertex, d_in, d_out, hw, mode, clip_ratio=2.0, limit=4.0):
    """Local outer boundary for either turn orientation; [] for straight travel.

    Directions are unit vectors. Lengths use input units; both ratios use hw.
    The diagnostic round arc keeps 16 subdivisions and follows the short arc.
    """
    return join_boundary(vertex, d_in, d_out, hw, mode, clip_ratio, limit, round_steps=16)


def wedge(theta_deg, hw, mode, **kw):
    """One symmetric wedge pointing up, drawn as its outer stroke boundary."""
    _sine(theta_deg)  # Validate the public angle input.
    half = math.radians(theta_deg) / 2.0
    apex = (0.0, 0.0)
    d_in = (math.sin(half), math.cos(half))
    d_out = (math.sin(half), -math.cos(half))
    return corner_polygon(apex, d_in, d_out, hw, mode, **kw)


def check():
    print("Historical conventional-miter comparison with fixture B's recorded wedges (half-width 4.0, limit 4):")
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
    print("largest disagreement with historical measurements: %.2f pt\n" % worst)
    return worst


def table(hw=4.0):
    print("Reach from the vertex, half-width %.1f, against what each rule gives:" % hw)
    print("%6s %9s %9s %9s %9s %9s %9s" % (
        "angle", "ratio", "miter", "native L4", "clip sqrt2", "clip 2", "round radius"))
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
    print('The dashed circle has radius hw; it is not the axial clipping plane.')


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
