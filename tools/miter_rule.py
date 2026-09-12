"""Local outer-join geometry in a Cartesian plane; see MITER_GEOMETRY_CONTRACT.md.

Lengths use the same units as the input vertex. hw is half the full stroke width;
clip_ratio and limit are dimensionless ratios to hw. A clip bounds the outward
bisector projection, not the radial distance of every corner. No host semantics,
whole-path topology, or curve-offset accuracy are established here.
"""
import math

RIGHT_ANGLE_RATIO = math.sqrt(2.0)
CLIP_DEFAULT = RIGHT_ANGLE_RATIO
# Relative branch tolerance; retained tips may exceed the axial cap by this ratio.
CLIP_EPS = 1e-9
UNIT_EPS = 1e-12


def _number(value, name):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(name + ' must be a finite number')
    try:
        valid = math.isfinite(value)
    except OverflowError:
        valid = False
    if not valid:
        raise ValueError(name + ' must be a finite number')
    return value


def _positive(value, name):
    _number(value, name)
    if value <= 0:
        raise ValueError(name + ' must be positive')
    return value


def _ratio(value, name):
    _number(value, name)
    if value < 1:
        raise ValueError(name + ' must be at least 1 (relative to half-width)')
    return value


def _point(value, name):
    if not isinstance(value, (tuple, list)) or len(value) != 2:
        raise ValueError(name + ' must contain two finite numbers')
    return (_number(value[0], name), _number(value[1], name))


def _unit(value, name):
    x, y = _point(value, name)
    length = math.hypot(x, y)
    if not math.isfinite(length) or abs(length - 1) > UNIT_EPS:
        raise ValueError(name + ' must be a unit direction')
    return (x / length, y / length)


def _sine(interior_deg):
    _number(interior_deg, 'interior angle')
    if not 0 < interior_deg <= 180:
        raise ValueError('interior angle must be in (0, 180] degrees')
    s = math.sin(math.radians(interior_deg) / 2)
    if s == 0:
        raise ValueError('interior angle is below representable resolution')
    return s


def _finite(value):
    if not math.isfinite(value):
        raise ValueError('geometry exceeds finite coordinate range')
    return value


def interior_angle(u, w):
    """Interior angle in degrees from incoming/outgoing unit directions."""
    u, w = _unit(u, 'incoming direction'), _unit(w, 'outgoing direction')
    # atan2 avoids subtracting an acos result from 180 near a cusp.
    return math.degrees(2 * math.atan2(math.hypot(u[0]+w[0], u[1]+w[1]),
                                     math.hypot(u[0]-w[0], u[1]-w[1])))


def miter_ratio(interior_deg):
    """Unclipped tip distance divided by HALF the full stroke width."""
    return _finite(1 / _sine(interior_deg))


def miter_reach(hw, interior_deg):
    """Unclipped tip's axial distance, in the same length units as hw."""
    return _finite(_positive(hw, 'half-width') / _sine(interior_deg))


def bevel_reach(hw, interior_deg):
    """Axial distance of the bevel midpoint; endpoints have radial distance hw."""
    return _finite(_positive(hw, 'half-width') * _sine(interior_deg))


def round_reach(hw, _interior_deg=None):
    """Radius of the ideal round join, in length units; polygon arcs approximate it."""
    if _interior_deg is not None:
        _sine(_interior_deg)
    return _positive(hw, 'half-width')


def clipped_reach(hw, interior_deg, clip_ratio=CLIP_DEFAULT):
    """Axial extent including the same threshold tolerance as the polygon code."""
    _positive(hw, 'half-width')
    _ratio(clip_ratio, 'clip ratio')
    s = _sine(interior_deg)
    cap = _finite(hw * clip_ratio)
    return cap if s < (1 / clip_ratio) / (1 + CLIP_EPS) else _finite(hw / s)


def native_reach(hw, interior_deg, limit=4.0):
    """Conventional miter/bevel threshold model's axial extent, in length units."""
    _positive(hw, 'half-width')
    _ratio(limit, 'miter limit')
    s = _sine(interior_deg)
    return _finite(hw / s) if s >= 1 / limit else hw * s


def is_clamped(interior_deg, clip_ratio=CLIP_DEFAULT):
    """Whether the clipping branch acts, with dimensionless relative tolerance."""
    _ratio(clip_ratio, 'clip ratio')
    return _sine(interior_deg) < (1 / clip_ratio) / (1 + CLIP_EPS)


def join_boundary(vertex, d_in, d_out, hw, mode, clip_ratio=CLIP_DEFAULT,
                  limit=4.0, round_steps=12):
    """Outer boundary for one join; [] for straight travel, ValueError for cusps.

    Inputs are finite, hw > 0, both ratios >= 1, and directions unit to 1e-12.
    Near-cusp clipping intersects offset lines with the axial plane without
    constructing the unbounded tip. Round joins retain fixed subdivisions.
    """
    vertex = _point(vertex, 'vertex')
    u, w = _unit(d_in, 'incoming direction'), _unit(d_out, 'outgoing direction')
    _positive(hw, 'half-width')
    _ratio(clip_ratio, 'clip ratio')
    _ratio(limit, 'miter limit')
    if mode not in ('clip', 'miter', 'bevel', 'round'):
        raise ValueError('unknown join mode')
    if isinstance(round_steps, bool) or not isinstance(round_steps, int) or round_steps < 1:
        raise ValueError('round steps must be a positive integer')
    cap = _finite(hw * clip_ratio) if mode == 'clip' else None
    cross = u[0]*w[1] - u[1]*w[0]
    dot = u[0]*w[0] + u[1]*w[1]
    if cross == 0:
        if dot < 0:
            raise ValueError('cusp or unresolved turn: outward side is undefined')
        return []
    side = 1 if cross < 0 else -1
    n1, n2 = (-side*u[1], side*u[0]), (-side*w[1], side*w[0])

    def point(x, y):
        return (_finite(vertex[0] + x), _finite(vertex[1] + y))

    p1, p2 = point(hw*n1[0], hw*n1[1]), point(hw*n2[0], hw*n2[1])
    if mode == 'bevel':
        return [p1, p2]
    if mode == 'round':
        a1, a2 = math.atan2(n1[1], n1[0]), math.atan2(n2[1], n2[0])
        while a2-a1 > math.pi:
            a2 -= 2*math.pi
        while a1-a2 > math.pi:
            a2 += 2*math.pi
        arc = [point(hw*math.cos(a1+(a2-a1)*k/round_steps),
                     hw*math.sin(a1+(a2-a1)*k/round_steps)) for k in range(round_steps+1)]
        arc[0], arc[-1] = p1, p2
        return arc

    sx, sy = u[0]+w[0], u[1]+w[1]
    dx, dy = u[0]-w[0], u[1]-w[1]
    s, c = math.hypot(sx, sy)/2, math.hypot(dx, dy)/2
    if s == 0 or c == 0:
        raise ValueError('turn is below representable resolution')
    # Use the well-conditioned pair: differences near a cusp, sums near straight.
    if dot < 0:
        bx, by = dx/(2*c), dy/(2*c)
    else:
        bx, by = -side*sy/(2*s), side*sx/(2*s)
    if mode == 'miter' and s < 1/limit:
        return [p1, p2]
    if mode == 'miter' or s >= (1/clip_ratio)/(1+CLIP_EPS):
        reach = _finite(hw/s)
        return [p1, point(bx*reach, by*reach), p2]

    # In the (bisector, lateral) frame the offset line is s*axial+c*lateral=hw.
    # Setting axial=cap gives lateral=hw*(1-L*s)/c; no distant tip is needed.
    lateral = _finite(hw*((1-clip_ratio*s)/c))
    ex, ey = -side*by, side*bx
    a = point(cap*bx+lateral*ex, cap*by+lateral*ey)
    b = point(cap*bx-lateral*ex, cap*by-lateral*ey)
    return [p1, a, b, p2]
