// ai-native-align.jsx - the opt-in native-align (region-prep-17), on every target it handles, each
// alone on the page: N-<case>-orig.pdf before, N-<case>.pdf after, N-<case>-restored.pdf after
// "restore", plus exact references, for tools/measure_native_align.py. Cases:
//   op-out, op-in     an open path (40,40)-(100,160)-(160,40), 8 pt
//   cmp-out, cmp-in   a compound path: a 70 pt square and an open triangle; reference cmpref-out
//                     (the square alone, Illustrator's own Outside)
//   tx-out, tx-in     type "Ax", Minion Pro 96 pt, one 8 pt stroke; reference txref-out (outlined,
//                     Illustrator's own Outside)
//   txm-out           the same with mixed strokes (8 pt on A, 6 pt on x)
//   txs-out           the type in a frame scaled 150% (its strokes not scaled)
//   cmp-edit          a doubled compound path whose weight is then set to 20: restore gives 10
//   sq-out            a closed square (Illustrator's own alignment)
//   th-dash           the 10-degree thorn, Outside 4, dashed 12/4, through "dashes"; reference
//                     thref (the same, solid)
// Needs StrokeMasterRegionProbe region-prep-17 and $.global.ISE_REGION_ARGS outDir and runId.
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
    function clear() { while (doc.pageItems.length) doc.pageItems[0].remove(); }
    function select(it) { doc.selection = null; it.selected = true; app.redraw(); }
    function pdf(name) {
        doc.selection = null; app.redraw();
        var f = new File(args.outDir + '/' + args.runId + '-N-' + name + '.pdf');
        if (f.exists) throw 'refusing to overwrite ' + f.name;
        var o = new PDFSaveOptions(); o.acrobatLayers = false; o.preserveEditability = false;
        doc.saveAs(f, o);
    }
    function stroke(p, w) {
        p.stroked = true; p.strokeColor = gray(60); p.strokeWidth = w || 8; p.strokeJoin = StrokeJoin.MITERENDJOIN; p.strokeMiterLimit = 4;
    }
    function openPath() { clear(); var p = doc.pathItems.add(); p.setEntirePath([[40, 40], [100, 160], [160, 40]]); p.filled = false; stroke(p); return p; }
    function square(x, y) { var r = doc.pathItems.rectangle(y, x, 70, 70); r.filled = true; r.fillColor = gray(100); stroke(r); return r; }
    function compound() {
        clear();
        var cp = doc.compoundPathItems.add();
        doc.pathItems.rectangle(160, 20, 70, 70).move(cp, ElementPlacement.PLACEATEND);
        var o = doc.pathItems.add(); o.setEntirePath([[120, 40], [150, 150], [180, 40]]); o.move(cp, ElementPlacement.PLACEATEND);
        var p = cp.pathItems[0]; p.filled = true; p.fillColor = gray(100); stroke(p);
        return cp;
    }
    function type(mixed) {
        clear();
        var t = doc.textFrames.add(); t.contents = 'Ax';
        var ca = t.textRange.characterAttributes;
        try { ca.textFont = app.textFonts.getByName('MinionPro-Regular'); } catch (e) {}
        ca.size = 96; ca.fillColor = gray(100); ca.strokeColor = gray(60); ca.strokeWeight = 8;
        ca.strokeJoin = StrokeJoin.MITERENDJOIN; ca.strokeMiterLimit = 4;
        if (mixed) t.textRange.characters[1].characterAttributes.strokeWeight = 6;
        t.position = [30, 170];
        return t;
    }
    function thorn(dashed) {
        clear();
        var half = 5 * Math.PI / 180, side = 60, bw = side * Math.sin(half), by = side * Math.cos(half);
        var p = doc.pathItems.add(), x0 = 100, y0 = 185;
        p.setEntirePath([[x0, y0], [x0 + bw, y0 - by], [x0 + 45, y0 - by], [x0 + 45, y0 - by - 60], [x0 - 45, y0 - by - 60], [x0 - 45, y0 - by], [x0 - bw, y0 - by]]);
        p.closed = true; p.filled = true; p.fillColor = gray(100); stroke(p, 4); p.strokeMiterLimit = 20;
        if (dashed) p.strokeDashes = [12, 4];
        select(p); send('stroke-align', 'outside');
        return p;
    }
    function align(item, mode) { select(item); return block(send('native-align', mode), 'align'); }
    function round(name, make, mode) {
        at = name;
        var it = make(); pdf(name + '-orig');
        note(name, align(it, mode)); pdf(name);
        note(name + ' restore', align(it, 'restore')); pdf(name + '-restored');
    }
    try {
        doc = newDocument(DocumentColorSpace.RGB, 240, 200);
        doc.artboards[0].artboardRect = [0, 200, 240, 0];
        round('op-out', openPath, 'outside');
        round('op-in', openPath, 'inside');
        round('cmp-out', compound, 'outside');
        round('cmp-in', compound, 'inside');
        at = 'cmpref'; clear(); var r = square(20, 160); select(r); send('stroke-align', 'outside'); pdf('cmpref-out');
        round('tx-out', function () { return type(false); }, 'outside');
        round('tx-in', function () { return type(false); }, 'inside');
        round('txm-out', function () { return type(true); }, 'outside');
        at = 'txref';
        var g = type(false).createOutline();
        function walk(it) {
            if (it.typename === 'GroupItem') { for (var i = 0; i < it.pageItems.length; i++) walk(it.pageItems[i]); return; }
            var p = it.typename === 'CompoundPathItem' ? it.pathItems[0] : it;
            p.filled = true; p.fillColor = gray(100); stroke(p);
            select(it); send('stroke-align', 'outside');
        }
        walk(g); pdf('txref-out');
        round('sq-out', function () { clear(); return square(60, 140); }, 'outside');
        // A type frame scaled 150% (its strokes not scaled): Outside must reach twice what the
        // character stroke reaches outside the letters as drawn (txs-orig against txs-out).
        round('txs-out', function () { var s = type(false); s.resize(150, 150, true, true, true, true, 100, Transformation.CENTER); return s; }, 'outside');
        // A doubled stroke whose weight the user changes afterward: restore halves what it has then.
        at = 'cmp-edit'; var ce = compound(); note(at, align(ce, 'outside'));
        ce.pathItems[0].strokeWidth = 20; note(at + ' weight set', ce.pathItems[0].strokeWidth);
        note(at + ' restore', align(ce, 'restore')); note(at + ' weight after', ce.pathItems[0].strokeWidth);
        at = 'thref'; thorn(false); pdf('thref');
        round('th-dash', function () { return thorn(true); }, 'dashes');
        doc = null;
        return 'DONE|' + out.join('|');
    } catch (error) {
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + ' line ' + error.line + '|' + out.join('|');
    }
    }
})();
