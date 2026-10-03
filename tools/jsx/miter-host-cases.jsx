// Host-control specimens; coordinates are Cartesian points, not glyph claims.
// Pure ES3 so the same specimen definitions run in ExtendScript and Node's VM.
$.global.ISE_MITER_CASES = (function () {
    var joins = ['miter', 'bevel', 'round'], caps = ['butt', 'round', 'projecting'];
    var rows = [], i, j, k;
    function add(kind, points, closed, join, cap, epsilon) {
        rows.push({id: 'MC' + ('00' + rows.length).slice(-2), kind: kind,
            points: points, closed: closed, join: join, cap: cap,
            epsilon: epsilon === undefined ? null : epsilon, width: 3, limit: 4});
    }
    for (i = 0; i < joins.length; i++) {
        for (j = 0; j < caps.length; j++) {
            add('segment', [[0,0],[30,0]], false, joins[i], caps[j]);
            add('exact-uturn', [[0,0],[30,0],[0,0]], false, joins[i], caps[j]);
            add('closed-two-anchor', [[0,0],[30,0]], true, joins[i], caps[j]);
        }
        add('right-angle', [[0,-20],[0,0],[20,0]], false, joins[i], 'butt');
        var eps = [-0.01, -0.001, 0.001, 0.01];
        for (k = 0; k < eps.length; k++) {
            add('near-uturn', [[0,0],[30,0],[0,eps[k]]], false, joins[i], 'butt', eps[k]);
        }
    }
    return rows;
})();
