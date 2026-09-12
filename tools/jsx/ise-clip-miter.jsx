// ise-clip-miter.jsx - build a stroke's outline with a clamped miter join.
// Pure ASCII: ExtendScript sources are read in the system codepage.
// Defines $.global.ISE_CLIP; the caller deletes it afterward.
//
// This is the ExtendScript port of tools/stroke_outline.py, and it takes the same point format
// so the two can be compared against each other rather than trusted separately: an array of
// { a: [x, y], l: [x, y], r: [x, y] }, being the anchor and its two Bezier handles.
//
// Why a clamp. PostScript, PDF and SVG 1.1 all govern the miter with a THRESHOLD: a join is
// mitered while 1 / sin(theta / 2) is at most the limit, and becomes a full bevel past it. So a
// corner just inside the limit keeps its whole point and one just outside loses it entirely, and
// the only dial available moves the cliff rather than removing it. Measured at an 8 pt
// half-width, the native rule changes reach by 23.95 pt between 28 and 29 degrees.
//
// The clamp keeps the miter and cuts its tip off flat once it would reach further than
// clipRatio times the half-width. clipRatio is measured in half-widths, the convention
// Microsoft's stack uses, where this clamp has been the plain miter join since GDI+; FreeType
// and Qt clamp by default too. Do not call it SVG 2's miter-clip: that name was specified in
// 2018, never implemented by a browser, and removed on 18 March 2026
// (docs/MITER_JOIN_HISTORY.md). The default is the square root of two, the ratio a
// right angle produces, so the rule reads "no corner may protrude further than a right-angled
// corner would": ordinary corners are untouched and only acute ones are acted on.
//
// Two details decide whether this works on real type rather than on test wedges:
//
//   Corners come from the anchors, not from the flattening. Once a curve is flattened every
//   vertex has a small angle change, and applying a join rule to those would facet the curves.
//
//   Tangents come from the Bezier handles. On a curved segment the direction at an anchor is set
//   by its handle, so measuring toward the neighboring anchor would invent corners where the
//   outline is smooth and understate the ones that are real.
$.global.ISE_CLIP = (function () {
    var api = {};

    api.SUB = 24;            // flattening steps per curved segment
    api.CORNER_DEG = 0.75;   // tangent divergence above which an anchor counts as a corner
    api.RIGHT_ANGLE_RATIO = Math.sqrt(2.0);
    api.CLIP_DEFAULT = api.RIGHT_ANGLE_RATIO;
    // Written exactly, a 90 degree corner's ratio and a sqrt(2) clamp are the same number, so
    // whether it clips would depend on the last bit of a square root. Without this tolerance the
    // boundary lands at 91 degrees and every right angle is clipped by a fraction of nothing.
    api.CLIP_EPS = 1e-9;

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

    /** The angle between two segments meeting at a corner; 180 is a straight continuation. */
    api.interiorAngle = function (u, w) {
        var dot = u[0] * w[0] + u[1] * w[1];
        if (dot > 1.0) { dot = 1.0; }
        if (dot < -1.0) { dot = -1.0; }
        return 180.0 - (Math.acos(dot) * 180.0 / Math.PI);
    };

    /** Miter length over stroke width: the quantity a miter limit is compared against. */
    api.miterRatio = function (interiorDeg) {
        var s = Math.sin(interiorDeg * Math.PI / 360.0);
        return s < 1e-9 ? Number.MAX_VALUE : 1.0 / s;
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

    /** The filled wedge that closes the outer side of one corner. */
    api.joinPolygon = function (vx, vy, u, w, hw, mode, clipRatio, limit) {
        var cross = u[0] * w[1] - u[1] * w[0];
        if (Math.abs(cross) < 1e-12) { return null; }
        var n1, n2;
        if (cross < 0) { n1 = [-u[1], u[0]]; n2 = [-w[1], w[0]]; }
        else { n1 = [u[1], -u[0]]; n2 = [w[1], -w[0]]; }
        var p1 = [vx + n1[0] * hw, vy + n1[1] * hw];
        var p2 = [vx + n2[0] * hw, vy + n2[1] * hw];

        if (mode === 'bevel') { return [[vx, vy], p1, p2]; }
        if (mode === 'round') {
            var a1 = Math.atan2(n1[1], n1[0]), a2 = Math.atan2(n2[1], n2[0]), k;
            while (a2 - a1 > Math.PI) { a2 -= 2 * Math.PI; }
            while (a1 - a2 > Math.PI) { a2 += 2 * Math.PI; }
            var arc = [[vx, vy]];
            for (k = 0; k <= 12; k++) {
                var ang = a1 + (a2 - a1) * k / 12.0;
                arc.push([vx + Math.cos(ang) * hw, vy + Math.sin(ang) * hw]);
            }
            return arc;
        }
        var bx = n1[0] + n2[0], by = n1[1] + n2[1];
        var bl = Math.sqrt(bx * bx + by * by);
        if (bl < 1e-9) { return [[vx, vy], p1, p2]; }
        var b = [bx / bl, by / bl];
        var ratio = api.miterRatio(api.interiorAngle(u, w));
        var tipAt = hw * ratio;
        var tip = [vx + b[0] * tipAt, vy + b[1] * tipAt];

        if (mode === 'miter') {
            return ratio <= limit ? [[vx, vy], p1, tip, p2] : [[vx, vy], p1, p2];
        }
        var cap = hw * clipRatio;
        if (tipAt <= cap * (1.0 + api.CLIP_EPS)) { return [[vx, vy], p1, tip, p2]; }
        // Cut the miter off flat at the cap. Interpolating along both miter edges keeps the
        // corner pointing the same way and tapers continuously as the angle closes, which is the
        // whole difference from a bevel.
        var t = tipAt > hw ? (cap - hw) / (tipAt - hw) : 0.0;
        var a = [p1[0] + (tip[0] - p1[0]) * t, p1[1] + (tip[1] - p1[1]) * t];
        var c = [p2[0] + (tip[0] - p2[0]) * t, p2[1] + (tip[1] - p2[1]) * t];
        return [[vx, vy], p1, a, c, p2];
    };

    /** Every polygon whose union is the stroked region: one per segment, one per vertex. */
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
