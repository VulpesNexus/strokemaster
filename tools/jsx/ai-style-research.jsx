// ai-style-research.jsx - what Illustrator writes for Effect > Pathfinder > Add and for Align Dashes to
// Corners (2026-10-16), read through StrokeMasterRegionProbe's style-dump (region-prep-16). One
// document; returns each dump, and exports the dash variants as PDFs (V-<variant>.pdf: the 10-degree
// thorn, Center 4, dashed 12/4, miter limit 20) for tools/measure_gap_research.py-style counts.
// Needs $.global.ISE_REGION_ARGS outDir and runId. Alerts suppressed and restored.
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
    var plugin = 'StrokeMasterRegionProbe';
    var out = [];
    function note(k, v) { out.push(k + '=' + String(v).replace(/[|\r\n]/g, ' ')); }
    function send(selector, input) { return String(app.sendScriptMessage(plugin, selector, input || '')); }
    function block(json, name) { var m = new RegExp('"' + name + '":(\\{[^}]*\\})').exec(json); return m ? m[1] : '?'; }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    var doc = null, at = 'begin';
    function clear() { while (doc.pageItems.length > 0) doc.pageItems[0].remove(); }
    function select(items) { doc.selection = null; for (var i = 0; i < items.length; i++) items[i].selected = true; app.redraw(); }
    function pdf(name) {
        var f = new File(args.outDir + '/' + args.runId + '-' + name + '.pdf');
        if (f.exists) throw 'refusing to overwrite ' + f.name;
        var options = new PDFSaveOptions(); options.acrobatLayers = false;
        doc.saveAs(f, options);
    }
    function squares(align) {
        clear();
        var cp = doc.compoundPathItems.add();
        var a = doc.pathItems.rectangle(150, 40, 80, 80); a.move(cp, ElementPlacement.PLACEATEND);
        var b = doc.pathItems.rectangle(110, 80, 80, 80); b.move(cp, ElementPlacement.PLACEATEND);
        for (var i = 0; i < cp.pathItems.length; i++) {
            var p = cp.pathItems[i];
            p.filled = true; p.fillColor = gray(20);
            p.stroked = true; p.strokeColor = gray(100); p.strokeWidth = 8; p.strokeJoin = StrokeJoin.MITERENDJOIN;
        }
        select([cp]);
        if (align) send('stroke-align', align);
        return cp;
    }
    function thorn() {
        clear();
        var half = 5 * Math.PI / 180, side = 60, bw = side * Math.sin(half), by = side * Math.cos(half);
        var p = doc.pathItems.add(), x0 = 100, y0 = 190;
        p.setEntirePath([[x0, y0], [x0 + bw, y0 - by], [x0 + 45, y0 - by], [x0 + 45, y0 - by - 60], [x0 - 45, y0 - by - 60], [x0 - 45, y0 - by], [x0 - bw, y0 - by]]);
        p.closed = true; p.filled = true; p.fillColor = gray(20);
        p.stroked = true; p.strokeColor = gray(100); p.strokeWidth = 4; p.strokeJoin = StrokeJoin.MITERENDJOIN;
        p.strokeMiterLimit = 20; p.strokeDashes = [12, 4];
        select([p]);
        return p;
    }
    try {
        note('build', /"build":"([^"]*)"/.exec(send('status'))[1]);
        doc = newDocument(DocumentColorSpace.RGB, 200, 200);
        doc.artboards[0].artboardRect = [0, 200, 200, 0];
        at = 'plain'; squares(null); note('squares.center', send('style-dump'));
        at = 'inside'; squares('inside'); note('squares.inside', send('style-dump'));
        app.executeMenuCommand('Live Pathfinder Add'); app.redraw();
        select([doc.pageItems[0]]);
        note('squares.inside.pf', send('style-dump'));
        at = 'outside'; squares('outside'); note('squares.outside', send('style-dump'));
        at = 'thorn'; thorn(); note('thorn', send('style-dump')); pdf('V-native');
        var variants = ['on', 'off'];
        for (var v = 0; v < variants.length; v++) {
            at = 'dash ' + variants[v];
            thorn();
            note('dash.' + variants[v], block(send('dash-align', variants[v]), 'appearance'));
            app.redraw();
            note('dash.' + variants[v] + '.style', send('style-dump'));
            pdf('V-' + variants[v].replace(':', ''));
        }
        // Does a smooth (collinear) anchor anchor the dashes too? A line with and without one.
        function line(mid) {
            clear();
            var p = doc.pathItems.add();
            p.setEntirePath(mid ? [[10, 100], [93, 100], [190, 100]] : [[10, 100], [190, 100]]);
            p.filled = false; p.stroked = true; p.strokeColor = gray(100); p.strokeWidth = 6;
            p.strokeDashes = [20, 10];
            select([p]);
            return p;
        }
        at = 'L.plain'; line(false); note('L.plain', block(send('dash-align', 'on'), 'appearance')); app.redraw(); pdf('L-plain-align');
        at = 'L.mid'; line(true); note('L.mid', block(send('dash-align', 'on'), 'appearance')); app.redraw(); pdf('L-mid-align');
        line(true); app.redraw(); pdf('L-mid');
        doc = null;
        return 'DONE|' + out.join('|');
    } catch (error) {
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + ' line ' + error.line + '|' + out.join('|');
    }
    }
})();
