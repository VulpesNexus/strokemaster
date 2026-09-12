// ise-clip-miter.jsx - experimental stroke pieces with tested local joins.
// Pure ASCII. Defines $.global.ISE_CLIP; the caller deletes it afterward.
// Lengths use input-coordinate units. hw is half the full stroke width;
// clipRatio and limit are dimensionless ratios to hw. Clipping limits the
// outward bisector projection, not the radial distance of all flat corners.
// Fixed 24-step flattening, absolute handle thresholds, and automatic contour
// closure remain experimental. Handles classify corners; flattened chords
// determine the joins. Open caps, holes, and Boolean topology are not resolved.
// See docs/MITER_GEOMETRY_CONTRACT.md for the join-only validation envelope.
$.global.ISE_CLIP = (function () {
    var api = {};

    api.SUB = 24;            // flattening steps per curved segment
    api.CORNER_DEG = 0.75;   // tangent divergence above which an anchor counts as a corner
    api.RIGHT_ANGLE_RATIO = Math.sqrt(2.0);
    api.CLIP_DEFAULT = api.RIGHT_ANGLE_RATIO;
    // Relative branch tolerance; retained tips may slightly exceed the plane.
    api.CLIP_EPS = 1e-9;

    function number(value, name) {
        if (typeof value !== 'number' || !isFinite(value)) {
            throw new Error(name + ' must be a finite number');
        }
        return value;
    }
    function positive(value, name) {
        number(value, name);
        if (value <= 0) { throw new Error(name + ' must be positive'); }
        return value;
    }
    function ratioValue(value, name) {
        number(value, name);
        if (value < 1) { throw new Error(name + ' must be at least 1 (relative to half-width)'); }
        return value;
    }
    function finite(value) {
        if (!isFinite(value)) { throw new Error('geometry exceeds finite coordinate range'); }
        return value;
    }
    // Scaled norm works in ES3 without Math.hypot, avoiding squared overflow/underflow.
    function norm(x, y) {
        var scale = Math.max(Math.abs(x), Math.abs(y));
        if (scale === 0) { return 0; }
        return scale * Math.sqrt((x / scale) * (x / scale) + (y / scale) * (y / scale));
    }
    function direction(v, name) {
        if (!v || v.length !== 2) { throw new Error(name + ' must contain two numbers'); }
        var x = number(v[0], name), y = number(v[1], name), length = norm(x, y);
        if (!isFinite(length) || Math.abs(length - 1) > 1e-12) {
            throw new Error(name + ' must be a unit direction');
        }
        return [x / length, y / length];
    }

    function unit(x, y) {
        var n = Math.sqrt(x * x + y * y);
        return n > 1e-12 ? [x / n, y / n] : null;
    }

    function bez(p0, p1, p2, p3, t) {
        var m = 1.0 - t;
        return [m * m * m * p0[0] + 3 * m * m * t * p1[0] + 3 * m * t * t * p2[0] + t * t * t * p3[0],
                m * m * m * p0[1] + 3 * m * m * t * p1[1] + 3 * m * t * t * p2[1] + t * t * t * p3[1]];
    }

    /** Incoming and outgoing unit tangents at anchor i, taken from its handles. */
    api.tangentsAt = function (pts, i) {
        var n = pts.length, cur = pts[i], prev = pts[(i - 1 + n) % n], nxt = pts[(i + 1) % n];
        var a = cur.a, lh = cur.l, rh = cur.r;
        var dx = a[0] - lh[0], dy = a[1] - lh[1];
        if (Math.sqrt(dx * dx + dy * dy) < 1e-6) { dx = a[0] - prev.a[0]; dy = a[1] - prev.a[1]; }
        var ex = rh[0] - a[0], ey = rh[1] - a[1];
        if (Math.sqrt(ex * ex + ey * ey) < 1e-6) { ex = nxt.a[0] - a[0]; ey = nxt.a[1] - a[1]; }
        return [unit(dx, dy), unit(ex, ey)];
    };

    /** Interior angle in degrees from incoming/outgoing unit directions. */
    api.interiorAngle = function (u, w) {
        u = direction(u, 'incoming direction'); w = direction(w, 'outgoing direction');
        return 2 * Math.atan2(norm(u[0] + w[0], u[1] + w[1]),
                             norm(u[0] - w[0], u[1] - w[1])) * 180 / Math.PI;
    };

    /** Unclipped tip distance divided by HALF the full stroke width. */
    api.miterRatio = function (interiorDeg) {
        number(interiorDeg, 'interior angle');
        if (interiorDeg <= 0 || interiorDeg > 180) {
            throw new Error('interior angle must be in (0, 180] degrees');
        }
        var s = Math.sin((interiorDeg * (Math.PI / 180)) / 2);
        if (s === 0) { throw new Error('interior angle is below representable resolution'); }
        return finite(1 / s);
    };

    /** The outline as vertices, each flagged as a real corner or a smooth point. */
    api.flatten = function (pts) {
        var out = [], n = pts.length, i, s;
        for (i = 0; i < n; i++) {
            var cur = pts[i], nxt = pts[(i + 1) % n];
            var t = api.tangentsAt(pts, i), corner = false;
            if (t[0] && t[1]) { corner = (180.0 - api.interiorAngle(t[0], t[1])) > api.CORNER_DEG; }
            out.push({ x: cur.a[0], y: cur.a[1], corner: corner });
            var p0 = cur.a, p1 = cur.r, p3 = nxt.a, p2 = nxt.l;
            var straight = Math.sqrt((p1[0] - p0[0]) * (p1[0] - p0[0]) + (p1[1] - p0[1]) * (p1[1] - p0[1])) < 1e-6 &&
                           Math.sqrt((p2[0] - p3[0]) * (p2[0] - p3[0]) + (p2[1] - p3[1]) * (p2[1] - p3[1])) < 1e-6;
            if (!straight) {
                for (s = 1; s < api.SUB; s++) {
                    var q = bez(p0, p1, p2, p3, s / api.SUB);
                    out.push({ x: q[0], y: q[1], corner: false });
                }
            }
        }
        return out;
    };

    /** Filled local outer join; null for straight travel, Error for invalid inputs. */
    api.joinPolygon = function (vx, vy, u, w, hw, mode, clipRatio, limit) {
        if (clipRatio === undefined) { clipRatio = api.CLIP_DEFAULT; }
        if (limit === undefined) { limit = 4; }
        number(vx, 'vertex'); number(vy, 'vertex');
        u = direction(u, 'incoming direction'); w = direction(w, 'outgoing direction');
        positive(hw, 'half-width'); ratioValue(clipRatio, 'clip ratio'); ratioValue(limit, 'miter limit');
        if (mode !== 'clip' && mode !== 'miter' && mode !== 'bevel' && mode !== 'round') {
            throw new Error('unknown join mode');
        }
        var cap = mode === 'clip' ? finite(hw * clipRatio) : null;
        var cross = u[0] * w[1] - u[1] * w[0], dot = u[0] * w[0] + u[1] * w[1];
        if (cross === 0) {
            if (dot < 0) { throw new Error('cusp or unresolved turn: outward side is undefined'); }
            return null;
        }
        var side = cross < 0 ? 1 : -1;
        var n1 = [-side * u[1], side * u[0]], n2 = [-side * w[1], side * w[0]];
        function point(x, y) { return [finite(vx + x), finite(vy + y)]; }
        var p1 = point(hw * n1[0], hw * n1[1]), p2 = point(hw * n2[0], hw * n2[1]);
        if (mode === 'bevel') { return [[vx, vy], p1, p2]; }
        if (mode === 'round') {
            var a1 = Math.atan2(n1[1], n1[0]), a2 = Math.atan2(n2[1], n2[0]), k;
            while (a2 - a1 > Math.PI) { a2 -= 2 * Math.PI; }
            while (a1 - a2 > Math.PI) { a2 += 2 * Math.PI; }
            var arc = [[vx, vy]];
            for (k = 0; k <= 12; k++) {
                var ang = a1 + (a2 - a1) * k / 12;
                arc.push(point(Math.cos(ang) * hw, Math.sin(ang) * hw));
            }
            arc[1] = p1; arc[arc.length - 1] = p2;
            return arc;
        }
        var sx = u[0] + w[0], sy = u[1] + w[1], dx = u[0] - w[0], dy = u[1] - w[1];
        var s = norm(sx, sy) / 2, c = norm(dx, dy) / 2, bx, by;
        if (s === 0 || c === 0) { throw new Error('turn is below representable resolution'); }
        if (dot < 0) { bx = dx / (2 * c); by = dy / (2 * c); }
        else { bx = -side * sy / (2 * s); by = side * sx / (2 * s); }
        if (mode === 'miter' && s < 1 / limit) { return [[vx, vy], p1, p2]; }
        if (mode === 'miter' || s >= (1 / clipRatio) / (1 + api.CLIP_EPS)) {
            var reach = finite(hw / s);
            return [[vx, vy], p1, point(bx * reach, by * reach), p2];
        }
        // Offset line: s*axial + c*lateral = hw. Intersect it with axial = cap.
        // This avoids materializing the nearly-cusp miter tip.
        var lateral = finite(hw * ((1 - clipRatio * s) / c));
        var ex = -side * by, ey = side * bx;
        var a = point(cap * bx + lateral * ex, cap * by + lateral * ey);
        var b = point(cap * bx - lateral * ex, cap * by - lateral * ey);
        return [[vx, vy], p1, a, b, p2];
    };

    /** Experimental closed-contour pieces; no whole-stroke topology guarantee. */
    api.strokeShapes = function (pts, hw, mode, clipRatio, limit) {
        if (mode === undefined) { mode = 'clip'; }
        if (clipRatio === undefined) { clipRatio = api.CLIP_DEFAULT; }
        if (limit === undefined) { limit = 4.0; }
        var flat = api.flatten(pts), n = flat.length, shapes = [], i;
        for (i = 0; i < n; i++) {
            var a = flat[i], b = flat[(i + 1) % n];
            var d = unit(b.x - a.x, b.y - a.y);
            if (!d) { continue; }
            var nx = -d[1] * hw, ny = d[0] * hw;
            shapes.push([[a.x + nx, a.y + ny], [b.x + nx, b.y + ny], [b.x - nx, b.y - ny], [a.x - nx, a.y - ny]]);
        }
        for (i = 0; i < n; i++) {
            var cur = flat[i], prev = flat[(i - 1 + n) % n], nxt = flat[(i + 1) % n];
            var u = unit(cur.x - prev.x, cur.y - prev.y);
            var w = unit(nxt.x - cur.x, nxt.y - cur.y);
            if (!u || !w) { continue; }
            // Smooth points get a round join: the rule under test applies to real corners only.
            var poly = api.joinPolygon(cur.x, cur.y, u, w, hw, cur.corner ? mode : 'round', clipRatio, limit);
            if (poly) { shapes.push(poly); }
        }
        return shapes;
    };

    /** A path as a dense polygon, curves included, for filling. */
    api.contour = function (pts) {
        var flat = api.flatten(pts), out = [], i;
        for (i = 0; i < flat.length; i++) { out.push([flat[i].x, flat[i].y]); }
        return out;
    };

    return api;
})();
