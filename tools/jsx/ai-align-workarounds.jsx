// ai-align-workarounds.jsx - native stand-ins for Inside and Outside where Illustrator draws no
// stroke alignment (2026-10-16), each measured against an exact reference. Each case alone on the
// page as A-<case>.pdf, for tools/measure_align_workarounds.py:
//   tx-fill                  type "Ax" (Minion Pro 96 pt), unstroked: the glyph mask
//   tx-ref-out, tx-ref-in    the same type outlined, 8 pt Outside or Inside (Illustrator's own)
//   tx-below2w               the type, an object-level 16 pt stroke below Characters (Outside)
//   tx-above-in, tx-above-out  an object-level 8 pt stroke above Characters with a one-sided
//                            profile (sides "<a>" and "<b>" of ISE_REGION_ARGS.sides)
//   op-e01, op-e10           an open path, 8 pt, one-sided profile with a near-zero other side
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
    var sides = args.sides || ['1,0.001', '0.001,1'];
    var plugin = 'StrokeMasterRegionProbe';
    var out = [];
    function note(k, v) { out.push(k + '=' + String(v).replace(/[|\r\n]/g, ' ')); }
    function send(selector, input) { return String(app.sendScriptMessage(plugin, selector, input || '')); }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    var doc = null, at = 'begin';
    function clear() { while (doc.pageItems.length) doc.pageItems[0].remove(); }
    function select(it) { doc.selection = null; it.selected = true; app.redraw(); }
    function pdf(name) {
        doc.selection = null; app.redraw();
        var f = new File(args.outDir + '/' + args.runId + '-A-' + name + '.pdf');
        if (f.exists) throw 'refusing to overwrite ' + f.name;
        var o = new PDFSaveOptions(); o.acrobatLayers = false; o.preserveEditability = false;
        doc.saveAs(f, o);
    }
    function type() {
        clear();
        var t = doc.textFrames.add(); t.contents = 'Ax';
        var ca = t.textRange.characterAttributes;
        try { ca.textFont = app.textFonts.getByName('MinionPro-Regular'); } catch (e) {}
        ca.size = 96; ca.fillColor = gray(100); ca.strokeColor = new NoColor();
        t.position = [30, 170];
        return t;
    }
    function outlined(align) {
        var g = type().createOutline();
        var paths = [];
        function walk(it) {
            if (it.typename === 'CompoundPathItem') { paths.push(it); return; }
            if (it.typename === 'PathItem') { paths.push(it); return; }
            if (it.typename === 'GroupItem') for (var i = 0; i < it.pageItems.length; i++) walk(it.pageItems[i]);
        }
        walk(g);
        for (var i = 0; i < paths.length; i++) {
            var p = paths[i].typename === 'CompoundPathItem' ? paths[i].pathItems[0] : paths[i];
            p.filled = true; p.fillColor = gray(100); p.stroked = true; p.strokeColor = gray(60); p.strokeWidth = 8;
            p.strokeJoin = StrokeJoin.MITERENDJOIN; p.strokeMiterLimit = 4;
            select(paths[i]); send('stroke-align', align);
        }
    }
    function openPath() {
        clear();
        var p = doc.pathItems.add(); p.setEntirePath([[40, 40], [100, 160], [160, 40]]);
        p.filled = false; p.stroked = true; p.strokeColor = gray(60); p.strokeWidth = 8;
        p.strokeJoin = StrokeJoin.MITERENDJOIN; p.strokeMiterLimit = 4;
        return p;
    }
    try {
        doc = newDocument(DocumentColorSpace.RGB, 240, 200);
        doc.artboards[0].artboardRect = [0, 200, 240, 0];
        at = 'tx-fill'; type(); pdf('tx-fill');
        at = 'tx-ref-out'; outlined('outside'); pdf('tx-ref-out');
        at = 'tx-ref-in'; outlined('inside'); pdf('tx-ref-in');
        at = 'tx-below2w'; var t = type(); select(t); note(at, send('align-survey', 'stroke:16')); pdf('tx-below2w');
        at = 'tx-above-a'; t = type(); select(t); note(at, send('align-survey', 'stroke-above')); select(t); note(at + ' side', send('width-side', sides[0])); pdf('tx-above-a');
        at = 'tx-above-b'; t = type(); select(t); note(at, send('align-survey', 'stroke-above')); select(t); note(at + ' side', send('width-side', sides[1])); pdf('tx-above-b');
        at = 'op-center'; openPath(); pdf('op-center');
        at = 'op-a'; var o = openPath(); select(o); note(at, send('width-side', sides[0])); pdf('op-a');
        at = 'op-b'; o = openPath(); select(o); note(at, send('width-side', sides[1])); pdf('op-b');
        doc = null;
        return 'DONE|' + out.join('|');
    } catch (error) {
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + ' line ' + error.line + '|' + out.join('|');
    }
    }
})();
