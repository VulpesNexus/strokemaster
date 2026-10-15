// ai-offset-tag.jsx - research (2026-10-16): the style of a compound path with an open contour
// after native-align inside (two fills), across a PDF save, and after restore, dumped with
// style-dump: the fix's tag on the Offset Path effect survives all three. With args.outsideFirst,
// an Outside round on another compound first, as ai-native-align does. The same compound as
// ai-native-align. Needs $.global.ISE_REGION_ARGS outDir and runId.
(function () {
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try { return run(); } finally { app.userInteractionLevel = interaction; }
    function run() {
    var out = [];
    function note(k, v) { out.push(k + '=' + String(v).replace(/[|\r\n]/g, ' ')); }
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    function sel(it) { app.activeDocument.selection = null; it.selected = true; app.redraw(); }
    function align(mode) { var m = /"align":(\{[^}]*\})/.exec(send('native-align', mode)); return m ? m[1] : '?'; }
    var doc = app.documents.addDocument('Web', new DocumentPreset());
    try {
        function compound() {
            while (doc.pageItems.length) doc.pageItems[0].remove();
            var c = doc.compoundPathItems.add();
            doc.pathItems.rectangle(160, 20, 70, 70).move(c, ElementPlacement.PLACEATEND);
            var o = doc.pathItems.add(); o.setEntirePath([[120, 40], [150, 150], [180, 40]]); o.move(c, ElementPlacement.PLACEATEND);
            var p = c.pathItems[0]; p.filled = true; p.fillColor = gray(100); p.stroked = true; p.strokeColor = gray(60);
            p.strokeWidth = 8; p.strokeJoin = StrokeJoin.MITERENDJOIN; p.strokeMiterLimit = 4;
            return c;
        }
        // As ai-native-align does: an Outside round on another compound first.
        if (($.global.ISE_REGION_ARGS || {}).outsideFirst) {
            var c0 = compound(); sel(c0); note('out', align('outside')); sel(c0); note('out dump', send('style-dump'));
            note('out restore', align('restore')); sel(c0); note('out restored', send('style-dump'));
        }
        var cp = compound();
        sel(cp); note('before', send('style-dump'));
        note('inside', align('inside'));
        sel(cp); note('after', send('style-dump'));
        var a0 = $.global.ISE_REGION_ARGS || {};
        if (a0.outDir) {
            doc.selection = null; app.redraw();
            var so = new PDFSaveOptions(); so.acrobatLayers = false; so.preserveEditability = false;
            doc.saveAs(new File(a0.outDir + '/' + a0.runId + '-tag-inside.pdf'), so);
            sel(cp); note('after save', send('style-dump'));
        }
        note('restore', align('restore'));
        sel(cp); note('restored', send('style-dump'));
        return 'DONE|' + out.join('|');
    } catch (e) {
        return 'FAIL|' + String(e).replace(/[|\r\n]/g, ' ') + ' line ' + e.line + '|' + out.join('|');
    }
    }
})();
