// ai-tips-union.jsx - a native stand-in for flush-cut glyph tips (2026-11-01, research): beneath
// the fill, the union of two Offset Path shapes in the stroke's color, one with miter joins at a
// limit (whole miters within it) and one with round joins (every corner reaching the stroke's reach
// h). Past the limit a tip then reaches h, rounded, where a stroke bevels it at its point.
//   rows: triangles with apex angles 15, 30, 45, 60, and 90 degrees (filled black);
//   columns: (0) a native 7 pt Center Miter stroke, limit 4; (1) miter limit 4 + round;
//            (2) miter limit 2 + round; (3) round only.
//   type rows: Eagle Lake and Minion Pro Bold "vAkW" 110 pt (args.fonts, args.word), the same four.
// Exported as <runId>-union.pdf. Needs $.global.ISE_REGION_ARGS (outDir, runId).
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try { return run(); } catch (e) { return 'FAIL|' + String(e).replace(/[|\r\n]/g, ' ') + ' line ' + e.line; }
    finally { app.userInteractionLevel = interaction; }
    function run() {
        var out = [];
        out.push('off=' + String(app.sendScriptMessage('StrokeMasterRegionProbe', 'settings', 'enabled=0')).substr(0, 9));
        var doc = app.documents.add(DocumentColorSpace.RGB, 1200, 1400);
        function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
        function offset(item, join, limit) {
            item.applyEffect('<LiveEffect name="Adobe Offset Path"><Dict data="R ofst 3.5 I jntp ' + join + ' R mlim ' + limit + ' "/></LiveEffect>');
        }
        // Each variant: the gray copies beneath to make, as [join, limit] pairs; null for the stroke.
        var variants = [null, [[2, 4], [0, 4]], [[2, 2], [0, 4]], [[0, 4]]];
        var angles = [15, 30, 45, 60, 90];
        for (var r = 0; r < angles.length; r++) {
            var half = angles[r] / 2 * Math.PI / 180, L = 50, w = L * Math.tan(half);
            for (var c = 0; c < variants.length; c++) {
                var x0 = 80 + c * 140, y0 = 1360 - r * 110;
                var pts = [[x0, y0], [x0 + w, y0 - L], [x0 - w, y0 - L]];
                var copies = variants[c] || [null];
                for (var k = 0; k < copies.length; k++) {
                    var u = doc.pathItems.add(); u.setEntirePath(pts); u.closed = true;
                    u.filled = true; u.fillColor = gray(40); u.stroked = false;
                    if (copies[k]) offset(u, copies[k][0], copies[k][1]);
                    else { u.stroked = true; u.strokeColor = gray(40); u.strokeWidth = 7; u.strokeJoin = StrokeJoin.MITERENDJOIN; u.strokeMiterLimit = 4; }
                }
                var top = doc.pathItems.add(); top.setEntirePath(pts); top.closed = true;
                top.filled = true; top.fillColor = gray(100); top.stroked = false;
            }
        }
        var fonts = args.fonts || ['EagleLake-Regular', 'MinionPro-Bold'];
        for (var f = 0; f < fonts.length; f++) {
            var font = null;
            try { font = app.textFonts.getByName(fonts[f]); } catch (e) { font = null; }
            out.push(fonts[f] + '=' + (font ? 'ok' : 'missing'));
            for (var v = 0; v < variants.length; v++) {
                var make = function (k) {
                    var t = doc.textFrames.add(); t.contents = args.word || 'vAkW'; t.position = [40 + (v % 2) * 560, 760 - f * 330 - Math.floor(v / 2) * 150];
                    var ca = t.textRange.characterAttributes;
                    if (font) ca.textFont = font;
                    ca.size = 110; ca.strokeColor = new NoColor(); ca.fillColor = gray(k);
                    return t;
                };
                var list = variants[v] || [null];
                for (var q = 0; q < list.length; q++) {
                    var g = make(40);
                    if (list[q]) offset(g, list[q][0], list[q][1]);
                    else { var a = g.textRange.characterAttributes; a.strokeColor = gray(40); a.strokeWeight = 7; a.strokeMiterLimit = 4; }
                }
                make(100);
            }
        }
        doc.selection = null;
        app.redraw();
        var o = new PDFSaveOptions(); o.acrobatLayers = false; o.preserveEditability = false; o.viewAfterSaving = false;
        doc.saveAs(new File(args.outDir + '/' + args.runId + '-union.pdf'), o);
        return 'DONE|' + out.join('|');
    }
})();
