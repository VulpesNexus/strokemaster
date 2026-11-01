// ai-tips-offset.jsx - what Illustrator's own Effect > Path > Offset Path does at a sharp corner past
// its miter limit (2026-11-01, research for flush-cut glyph tips): bevel at the point, round, or a
// flat cut at the offset distance, as InDesign's Trim glyph tips cuts them. One new document:
//   rows: triangles with apex angles 15, 30, and 45 degrees (filled black, no stroke);
//   columns: (0) a native 7 pt Center Miter stroke, limit 4, gray, on a copy beneath;
//            (1..4) a gray copy beneath with Offset Path 3.5 pt, miter join, limit 1, 2, 4, 10;
//            (5) the same with a round join; (6) with a bevel join.
//   Also live type, Eagle Lake "vAk" 120 pt: the same Offset Path (3.5, miter, limit 1 and 4) on a
//   gray copy beneath, and a native 7 pt stroke, limit 4.
// Each gray item is named <row>-<col>; the style of one Offset Path item is dumped with the
// probe's style-dump. Exported as <runId>-tips.pdf. Needs $.global.ISE_REGION_ARGS (outDir, runId).
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try { return run(); } catch (e) { return 'FAIL|' + String(e).replace(/[|\r\n]/g, ' ') + ' line ' + e.line; }
    finally { app.userInteractionLevel = interaction; }
    function run() {
        var out = [];
        // StrokeMaster off: nothing here is corrected (the run's own switches' file).
        out.push('off=' + String(app.sendScriptMessage('StrokeMasterRegionProbe', 'settings', 'enabled=0')).substr(0, 9));
        var doc = app.documents.add(DocumentColorSpace.RGB, 1000, 900);
        function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
        function offset(item, h, join, limit) {
            item.applyEffect('<LiveEffect name="Adobe Offset Path"><Dict data="R ofst ' + h + ' I jntp ' + join + ' R mlim ' + limit + ' "/></LiveEffect>');
        }
        var angles = [15, 30, 45];
        var cols = [['stroke', 0, 4], ['m1', 2, 1], ['m2', 2, 2], ['m4', 2, 4], ['m10', 2, 10], ['round', 0, 4], ['bevel', 1, 4]];
        for (var r = 0; r < angles.length; r++) {
            var half = angles[r] / 2 * Math.PI / 180, L = 60, w = L * Math.tan(half);
            for (var c = 0; c < cols.length; c++) {
                var x0 = 60 + c * 130, y0 = 860 - r * 130;
                var pts = [[x0, y0], [x0 + w, y0 - L], [x0 - w, y0 - L]];
                var under = doc.pathItems.add();
                under.setEntirePath(pts); under.closed = true;
                under.filled = true; under.fillColor = gray(40); under.stroked = false;
                under.name = r + '-' + c;
                if (cols[c][0] === 'stroke') {
                    under.stroked = true; under.strokeColor = gray(40); under.strokeWidth = 7;
                    under.strokeJoin = StrokeJoin.MITERENDJOIN; under.strokeMiterLimit = 4;
                } else {
                    offset(under, 3.5, cols[c][1], cols[c][2]);
                }
                var top = doc.pathItems.add();
                top.setEntirePath(pts); top.closed = true;
                top.filled = true; top.fillColor = gray(100); top.stroked = false;
                top.name = 'apex ' + x0 + ' ' + y0;
            }
        }
        // Live type.
        var font = null;
        try { font = app.textFonts.getByName('EagleLake-Regular'); } catch (e) { font = null; }
        var typeCols = [['tm1', 2, 1], ['tm4', 2, 4], ['tstroke', 0, 4]];
        for (var t = 0; t < typeCols.length; t++) {
            var make = function () {
                var f = doc.textFrames.add(); f.contents = 'vAk'; f.position = [60 + t * 300, 400];
                var ca = f.textRange.characterAttributes;
                if (font) ca.textFont = font;
                ca.size = 120; ca.strokeColor = new NoColor();
                return f;
            };
            var u = make();
            u.textRange.characterAttributes.fillColor = gray(40);
            u.name = 'type-' + typeCols[t][0];
            if (typeCols[t][0] === 'tstroke') {
                var a = u.textRange.characterAttributes;
                a.strokeColor = gray(40); a.strokeWeight = 7; a.strokeMiterLimit = 4;
            } else {
                offset(u, 3.5, typeCols[t][1], typeCols[t][2]);
            }
            var o = make();
            o.textRange.characterAttributes.fillColor = gray(100);
        }
        // The Appearance stack of one Offset Path item: only Adobe effects.
        doc.selection = null;
        doc.pathItems.getByName('1-1').selected = true;
        out.push('dump=' + String(app.sendScriptMessage('StrokeMasterRegionProbe', 'style-dump', '')).replace(/[|\r\n]/g, ' '));
        doc.selection = null;
        app.redraw();
        var o2 = new PDFSaveOptions(); o2.acrobatLayers = false; o2.preserveEditability = false; o2.viewAfterSaving = false;
        doc.saveAs(new File(args.outDir + '/' + args.runId + '-tips.pdf'), o2);
        out.push('font=' + (font ? font.name : 'none'));
        return 'DONE|' + out.join('|');
    }
})();
