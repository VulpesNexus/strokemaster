// ai-width-side.jsx - whether a one-sided width profile stands in for Inside and Outside where
// Illustrator draws no stroke alignment (2026-10-16). Each case alone on the page, exported as
// W-<case>.pdf for tools/measure_width_side.py:
//   sq-center, sq-inside, sq-outside        an 80 pt square, 8 pt, Illustrator's own alignments
//   sq-w01, sq-w10, sq-w02, sq-w20           Center with a constant profile <left><right>
//   th-outside, th-w01, th-w10               the 10-degree thorn, 8 pt, miter limit 20
//   open-w01, open-w10, open-center          an open path
//   cmp-w01, cmp-w10, cmp-center             a compound path with a closed and an open contour
//   tx-fill, tx-center, tx-w01, tx-w10       type "Ax", unstroked characters, an object-level
//                                            8 pt stroke below Characters
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
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    var doc = null, at = 'begin';
    function clear() { while (doc.pageItems.length) doc.pageItems[0].remove(); }
    function select(it) { doc.selection = null; it.selected = true; app.redraw(); }
    function paint(p, stroked) {
        p.filled = true; p.fillColor = gray(100);
        p.stroked = stroked !== false;
        if (p.stroked) { p.strokeColor = gray(60); p.strokeWidth = 8; p.strokeJoin = StrokeJoin.MITERENDJOIN; p.strokeMiterLimit = 20; }
    }
    function pdf(name) {
        doc.selection = null; app.redraw();
        var f = new File(args.outDir + '/' + args.runId + '-W-' + name + '.pdf');
        if (f.exists) throw 'refusing to overwrite ' + f.name;
        var o = new PDFSaveOptions(); o.acrobatLayers = false; o.preserveEditability = false;
        doc.saveAs(f, o);
    }
    function square() { clear(); var r = doc.pathItems.rectangle(160, 60, 80, 80); paint(r); return r; }
    function thorn() {
        clear();
        var half = 5 * Math.PI / 180, side = 60, bw = side * Math.sin(half), by = side * Math.cos(half);
        var p = doc.pathItems.add(), x0 = 100, y0 = 185;
        p.setEntirePath([[x0, y0], [x0 + bw, y0 - by], [x0 + 45, y0 - by], [x0 + 45, y0 - by - 60], [x0 - 45, y0 - by - 60], [x0 - 45, y0 - by], [x0 - bw, y0 - by]]);
        p.closed = true; paint(p); return p;
    }
    function open() { clear(); var p = doc.pathItems.add(); p.setEntirePath([[40, 40], [100, 160], [160, 40]]); paint(p); p.filled = false; return p; }
    function compound() {
        clear();
        var cp = doc.compoundPathItems.add();
        doc.pathItems.rectangle(160, 20, 70, 70).move(cp, ElementPlacement.PLACEATEND);
        var o = doc.pathItems.add(); o.setEntirePath([[110, 40], [140, 150], [170, 40]]); o.move(cp, ElementPlacement.PLACEATEND);
        paint(cp.pathItems[0]); return cp;
    }
    function type(stroke) {
        clear();
        var t = doc.textFrames.add(); t.contents = 'Ax';
        var ca = t.textRange.characterAttributes;
        try { ca.textFont = app.textFonts.getByName('MinionPro-Regular'); } catch (e) {}
        ca.size = 96; ca.fillColor = gray(100); ca.strokeColor = new NoColor();
        t.position = [30, 170];
        if (stroke) { select(t); note('tx stroke', /"gate":"([^"]*)"/.exec(send('appearance', 'stroke-below'))[1]); }
        return t;
    }
    function align(it, a) { select(it); note(at, send('align-survey', a)); }
    function side(it, lr) { select(it); note(at, send('width-side', lr)); }
    try {
        doc = newDocument(DocumentColorSpace.RGB, 200, 200);
        doc.artboards[0].artboardRect = [0, 200, 200, 0];
        var cases = [
            ['sq-center', square, null], ['sq-inside', square, 'inside'], ['sq-outside', square, 'outside'],
            ['sq-w01', square, '0,1'], ['sq-w10', square, '1,0'], ['sq-w02', square, '0,2'], ['sq-w20', square, '2,0'],
            ['th-outside', thorn, 'outside'], ['th-w01', thorn, '0,1'], ['th-w10', thorn, '1,0'],
            ['open-center', open, null], ['open-w01', open, '0,1'], ['open-w10', open, '1,0'],
            ['cmp-center', compound, null], ['cmp-w01', compound, '0,1'], ['cmp-w10', compound, '1,0']];
        // ISE_REGION_ARGS.sides: further square cases, each <left>,<right> (W-sq-v<i>.pdf).
        var extra = args.sides || [];
        for (var x = 0; x < extra.length; x++) cases.push(['sq-v' + x, square, extra[x]]);
        // ISE_REGION_ARGS.caps: the first extra side on the square and the thorn with each cap.
        var caps = args.caps || [];
        var capOf = { round: StrokeCap.ROUNDENDCAP, projecting: StrokeCap.PROJECTINGENDCAP, butt: StrokeCap.BUTTENDCAP };
        for (var c = 0; c < caps.length; c++) {
            (function (cap) {
                cases.push(['sq-cap-' + cap, function () { var s = square(); s.strokeCap = capOf[cap]; return s; }, extra[0]]);
                cases.push(['th-cap-' + cap, function () { var s = thorn(); s.strokeCap = capOf[cap]; return s; }, extra[0]]);
            })(caps[c]);
        }
        for (var i = 0; i < cases.length; i++) {
            at = cases[i][0];
            var it = cases[i][1]();
            var act = cases[i][2];
            if (act === 'inside' || act === 'outside') align(it, act);
            else if (act) side(it, act);
            pdf(cases[i][0]);
        }
        at = 'tx-fill'; type(false); pdf('tx-fill');
        at = 'tx-center'; type(true); pdf('tx-center');
        at = 'tx-w01'; side(type(true), '0,1'); pdf('tx-w01');
        at = 'tx-w10'; side(type(true), '1,0'); pdf('tx-w10');
        doc = null;
        return 'DONE|' + out.join('|');
    } catch (error) {
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + ' line ' + error.line + '|' + out.join('|');
    }
    }
})();
