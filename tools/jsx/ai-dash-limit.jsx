// ai-dash-limit.jsx - whether Illustrator's Align Dashes to Corners keeps the miter limit
// (2026-10-16). The 10-degree thorn (ratio 11.5 at its tip), Center 8, miter limit 4, so the tip
// must be beveled: solid (L4-solid.pdf), dashed 12/4 (L4-dashed.pdf), dashed and aligned with
// dash-align (L4-aligned.pdf); and the same at the other limits of ISE_REGION_ARGS.limits
// (L<limit>-*.pdf); ISE_REGION_ARGS.start starts the path at another anchor. Needs StrokeMasterRegionProbe and
// $.global.ISE_REGION_ARGS outDir and runId.
(function () {
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try { return run(); } finally { app.userInteractionLevel = interaction; }
    function run() {
    function newDocument(cs, w, h) {
        try { return app.documents.add(cs, w, h); } catch (e) {
            var pr = new DocumentPreset(); pr.colorMode = cs; pr.width = w; pr.height = h; pr.units = RulerUnits.Points;
            return app.documents.addDocument(cs === DocumentColorSpace.CMYK ? 'Print' : 'Web', pr);
        }
    }
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId) return 'FAIL|missing outDir or runId';
    var out = [];
    function note(k, v) { out.push(k + '=' + String(v).replace(/[|\r\n]/g, ' ')); }
    function send(selector, input) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', selector, input || '')); }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    var doc = null, at = 'begin';
    function pdf(name) {
        doc.selection = null; app.redraw();
        var f = new File(args.outDir + '/' + args.runId + '-' + name + '.pdf');
        if (f.exists) throw 'refusing to overwrite ' + f.name;
        var o = new PDFSaveOptions(); o.acrobatLayers = false; o.preserveEditability = false;
        doc.saveAs(f, o);
    }
    function thorn(limit, dashed) {
        while (doc.pageItems.length) doc.pageItems[0].remove();
        var half = 5 * Math.PI / 180, side = 60, bw = side * Math.sin(half), by = side * Math.cos(half);
        var p = doc.pathItems.add(), x0 = 100, y0 = 130;
        var pts = [[x0, y0], [x0 + bw, y0 - by], [x0 + 45, y0 - by], [x0 + 45, y0 - by - 50], [x0 - 45, y0 - by - 50], [x0 - 45, y0 - by], [x0 - bw, y0 - by]];
        // ISE_REGION_ARGS.start: the anchor the path starts at (0, the tip, by default).
        var k0 = args.start || 0;
        p.setEntirePath(pts.slice(k0).concat(pts.slice(0, k0)));
        p.closed = true; p.filled = true; p.fillColor = gray(100);
        p.stroked = true; p.strokeColor = gray(60); p.strokeWidth = 8; p.strokeJoin = StrokeJoin.MITERENDJOIN; p.strokeMiterLimit = limit;
        if (dashed) p.strokeDashes = [12, 4];
        return p;
    }
    try {
        doc = newDocument(DocumentColorSpace.RGB, 200, 240);
        doc.artboards[0].artboardRect = [0, 240, 200, 0];
        var limits = args.limits || [4, 20];
        for (var i = 0; i < limits.length; i++) {
            var pre = 'L' + limits[i] + '-';
            at = pre + 'solid'; var q = thorn(limits[i], false); note(at + ' limit', q.strokeMiterLimit); pdf(pre + 'solid');
            // ISE_REGION_ARGS.viaFix: the solid thorn again, its limit chosen and set by native-fix
            // (SetPathStyle) instead of by script.
            if (args.viaFix) {
                at = pre + 'fixed'; q = thorn(limits[i], false);
                doc.selection = null; q.selected = true;
                note(at, /"pathLimits":(\d+)/.exec(send('native-fix'))[1] + ' limit ' + q.strokeMiterLimit);
                pdf(pre + 'fixed');
            }
            at = pre + 'dashed'; thorn(limits[i], true); pdf(pre + 'dashed');
            at = pre + 'aligned'; var p = thorn(limits[i], true);
            doc.selection = null; p.selected = true;
            note(at, /"fieldsAfter":(-?\d+)/.exec(send('dash-align', 'on'))[1]);
            pdf(pre + 'aligned');
        }
        doc = null;
        return 'DONE|' + out.join('|');
    } catch (error) {
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + ' line ' + error.line + '|' + out.join('|');
    }
    }
})();
