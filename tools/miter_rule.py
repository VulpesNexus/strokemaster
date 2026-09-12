"""The join rule itself: one definition of the clamp, shared by everything that applies it.

Two tools need this arithmetic. tools/miter_geometry.py checks the formulas against measured
wedges and draws the rules side by side; tools/stroke_outline.py builds real stroke outlines from
a path. Each used to carry its own copy of the constants, which is how two files quietly come to
disagree: one of them once spelled the clamp as 1.4142, fractionally smaller than the square root
it stands for, and so clipped the very right angle it exists to leave alone.

The rule, in one sentence: keep the miter, but never let a corner protrude further than a right
angle would.

Background. PostScript, PDF, and SVG 1.1 all offer three joins and govern the miter with a
LIMIT, which is a threshold: the join is mitered while 1 / sin(theta / 2) <= limit and becomes a
full bevel past it. A corner just inside the limit keeps its whole point and one just outside
loses it entirely, so the only available dial moves the cliff rather than removing it. Measured
on an 8 pt half-width, the native rule changes reach by 23.95 pt between 28 and 29 degrees.

A clamp removes the cliff: the miter is kept and its tip cut off flat once it would reach further
than clip_ratio times the half-width. Largest step anywhere in a 1 to 179 degree sweep: 0.0975 pt
at the same half-width.

clip_ratio is measured in half-widths, which is the same convention SVG 2 uses for
stroke-linejoin: miter-clip, where the clip distance is stroke-miterlimit / 2 times the stroke
width, and the stroke width is twice the half-width. The default here is therefore equivalent to
an SVG 2 miter-clip at a stroke-miterlimit of sqrt(2).
"""
import math

# A right angle's miter reaches sqrt(2) times the half-width. Clamping there states the rule as
# "no corner may protrude further than a right-angled corner would", which is independent of the
# font, the size, and the stroke weight.
RIGHT_ANGLE_RATIO = math.sqrt(2.0)
CLIP_DEFAULT = RIGHT_ANGLE_RATIO

# The clamp comparison carries a relative tolerance. Written exactly, a 90 degree corner's ratio
# and a sqrt(2) clamp are the same number, so whether it clips depends on the last bit of a
# square root. Without this the boundary landed at 91 degrees and every right angle was clipped
# by about 5e-8 of the stroke width.
CLIP_EPS = 1e-9


def interior_angle(u, w):
    """The angle between two segments meeting at a corner, from their unit tangents.

    180 degrees is a straight continuation; a small angle is a sharp point.
    """
    dot = max(-1.0, min(1.0, u[0] * w[0] + u[1] * w[1]))
    return 180.0 - math.degrees(math.acos(dot))


def miter_ratio(interior_deg):
    """Miter length over stroke width: the quantity a miter limit is compared against."""
    s = math.sin(math.radians(interior_deg) / 2.0)
    return float('inf') if s < 1e-9 else 1.0 / s


def miter_reach(hw, interior_deg):
    """How far an unclamped miter tip sits from the vertex."""
    return hw * miter_ratio(interior_deg)


def bevel_reach(hw, interior_deg):
    """How far the middle of a bevel's flat sits from the vertex."""
    return hw * math.sin(math.radians(interior_deg) / 2.0)


def round_reach(hw, _interior_deg=None):
    """A round join is an arc of the stroke's own radius, so it reaches exactly the half-width."""
    return hw


def clipped_reach(hw, interior_deg, clip_ratio=CLIP_DEFAULT):
    """The clamp: the miter tip, but never further than clip_ratio times the half-width."""
    return min(miter_reach(hw, interior_deg), hw * clip_ratio)


def native_reach(hw, interior_deg, limit=4.0):
    """What PostScript and PDF do: miter under the limit, full bevel past it."""
    if miter_ratio(interior_deg) <= limit:
        return miter_reach(hw, interior_deg)
    return bevel_reach(hw, interior_deg)


def is_clamped(interior_deg, clip_ratio=CLIP_DEFAULT):
    """True when the clamp actually acts on this corner, tolerance included."""
    return miter_ratio(interior_deg) > clip_ratio * (1.0 + CLIP_EPS)
