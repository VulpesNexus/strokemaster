// build-D-contours.jsx - does contour direction explain the Outside result on outlined counters?
// Requires ise-lib.jsx. Argument: ISE_ARGS.outRoot.
// Three rows of "Bog egg" outlines with Outside alignment applied after Create Outlines:
// contours as converted, inner contours reversed, and every contour reversed. Each row has the
// live text on the left and the converted copy 290 pt to the right for pixel comparison.
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var dir = lib.args.outRoot + '/D-create-outlines';
        var doc = lib.newDoc(612, 290);
        var base = lib.baseStyle(doc, 'ISE DK base', { pointSize: 48, strokeWeight: 4, endJoin: EndJoin.ROUND_END_JOIN });

        function anchors(path) {
            var pts = path.entirePath, out = [];
            for (var i = 0; i < pts.length; i++) {
                var p = pts[i];
                out.push((p.length === 3 && p[0] instanceof Array) ? p[1] : p);
            }
            return out;
        }
        function bbox(pts) {
            var b = [1e9, 1e9, -1e9, -1e9];
            for (var i = 0; i < pts.length; i++) {
                b[0] = Math.min(b[0], pts[i][0]); b[1] = Math.min(b[1], pts[i][1]);
                b[2] = Math.max(b[2], pts[i][0]); b[3] = Math.max(b[3], pts[i][1]);
            }
            return b;
        }
        function signedArea(pts) {
            var s = 0;
            for (var i = 0; i < pts.length; i++) {
                var a = pts[i], b = pts[(i + 1) % pts.length];
                s += a[0] * b[1] - b[0] * a[1];
            }
            return s / 2;
        }
        function inside(a, b) {
            return a[0] >= b[0] && a[1] >= b[1] && a[2] <= b[2] && a[3] <= b[3] && !(a[0] === b[0] && a[1] === b[1] && a[2] === b[2] && a[3] === b[3]);
        }

        function row(key, label, y, mode) {
            var tf = lib.frame(doc, key + ' live text', [y, 24, y + 70, 290], 'Bog egg', base);
            lib.record(tf, key + ': live text; copy: ' + label, { role: 'text', measure: 'pair' });
            var dup = tf.duplicate(undefined, [290, 0]);
            var res = dup.createOutlines(true);
            var reversed = 0, report = [];
            for (var r = 0; r < res.length; r++) {
                var P = res[r];
                var paths = P.paths.everyItem().getElements();
                var boxes = [], areas = [];
                for (var i = 0; i < paths.length; i++) {
                    var pts = anchors(paths[i]);
                    boxes.push(bbox(pts));
                    areas.push(signedArea(pts));
                }
                for (var k = 0; k < paths.length; k++) {
                    var depth = 0;
                    for (var j = 0; j < paths.length; j++) { if (k !== j && inside(boxes[k], boxes[j])) { depth++; } }
                    report.push((depth % 2 ? 'inner' : 'outer') + (areas[k] > 0 ? '+' : '-'));
                    if (mode === 'all' || (mode === 'inner' && depth % 2 === 1)) { paths[k].reverse(); reversed++; }
                }
                P.strokeAlignment = StrokeAlignment.OUTSIDE_ALIGNMENT;
                P.name = key + ' outlines, Outside, ' + label;
                lib.record(P, key + ': ' + label, { role: 'outlines' });
            }
            lib.L(key + ': contours before change (orientation by signed area, page coordinates): ' + report.join(' ') + '; reversed ' + reversed);
        }

        row('K01', 'contours as converted', 20, 'none');
        row('K02', 'inner contours reversed', 110, 'inner');
        row('K03', 'all contours reversed', 200, 'all');
        lib.saveExport(doc, dir, 'D-contours', 300);
    });
})();
