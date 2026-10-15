// ai-dash-corner.jsx - which corners Illustrator's Align Dashes to Corners anchors (2026-10-16). The
// InDesign plugin fits dashes between real corners only (turns over 30 degrees). Three open paths,
// Center 6, dashed 20/10, each with a corner anchor 83 pt along, turning 0, 10, and 40 degrees,
// (or ISE_REGION_ARGS.turns), given the option by native-fix and exported alone (D-<turn>.pdf). A dash centered
// on the corner means the corner anchors the dashes. Needs $.global.ISE_REGION_ARGS outDir and runId.
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
    var doc = null, at = 'begin';
    try {
        doc = newDocument(DocumentColorSpace.RGB, 200, 200);
        doc.artboards[0].artboardRect = [0, 200, 200, 0];
        var turns = args.turns || [0, 10, 40];
        for (var i = 0; i < turns.length; i++) {
            at = 'turn ' + turns[i];
            while (doc.pageItems.length) doc.pageItems[0].remove();
            var a = turns[i] * Math.PI / 180, L = 97;
            var p = doc.pathItems.add();
            p.setEntirePath([[10, 100], [93, 100], [93 + L * Math.cos(a), 100 - L * Math.sin(a)]]);
            p.filled = false; p.stroked = true; p.strokeWidth = 6; p.strokeDashes = [20, 10];
            var c = new GrayColor(); c.gray = 100; p.strokeColor = c;
            p.strokeJoin = StrokeJoin.MITERENDJOIN;
            doc.selection = null; p.selected = true;
            note('fix ' + turns[i], block(send('native-fix'), 'nativeFix'));
            doc.selection = null;
            app.redraw();
            var f = new File(args.outDir + '/' + args.runId + '-D-' + turns[i] + '.pdf');
            if (f.exists) throw 'refusing to overwrite ' + f.name;
            var o = new PDFSaveOptions(); o.acrobatLayers = false;
            doc.saveAs(f, o);
        }
        doc = null;
        return 'DONE|' + out.join('|');
    } catch (error) {
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + ' line ' + error.line + '|' + out.join('|');
    }
    }
})();
