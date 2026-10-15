// ai-offset-type.jsx - whether Effect > Path > Offset Path changes live type (2026-10-16): "Ax",
// Minion Pro 96 pt, Black, unstroked, alone on the page as O-<case>.pdf: plain, the effect applied
// by script with offset -8 and +8, and the type outlined (a compound path) with -8. Needs
// $.global.ISE_REGION_ARGS outDir and runId. Also insets of 1, 2, and 4 on the type, and a square
// with a thin bar (bar, bar-minus4).
(function () {
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try { return run(); } finally { app.userInteractionLevel = interaction; }
    function run() {
    var args = $.global.ISE_REGION_ARGS || {};
    var out = [];
    var doc = app.documents.addDocument('Web', new DocumentPreset());
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    function type() {
        while (doc.pageItems.length) doc.pageItems[0].remove();
        var t = doc.textFrames.add(); t.contents = 'Ax'; t.position = [100, 500];
        var ca = t.textRange.characterAttributes;
        try { ca.textFont = app.textFonts.getByName('MinionPro-Regular'); } catch (e) {}
        ca.size = 96; ca.fillColor = gray(100); ca.strokeColor = new NoColor();
        return t;
    }
    function pdf(name) {
        doc.selection = null; app.redraw();
        var o = new PDFSaveOptions(); o.acrobatLayers = false; o.preserveEditability = false;
        doc.saveAs(new File(args.outDir + '/' + args.runId + '-O-' + name + '.pdf'), o);
        out.push(name);
    }
    function effect(it, v) { it.applyEffect('<LiveEffect name="Adobe Offset Path"><Dict data="R ofst ' + v + ' I jntp 2 R mlim 4 "/></LiveEffect>'); }
    try {
        type(); pdf('plain');
        effect(type(), -8); pdf('minus');
        effect(type(), 8); pdf('plus');
        var g = type().createOutline(); effect(g, -8); pdf('outlined-minus');
        effect(type(), -1); pdf('minus1');
        effect(type(), -2); pdf('minus2');
        effect(type(), -4); pdf('minus4');
        // A 60 pt square with a 6 pt bar: an inset of 4 should take the bar away and keep a 52 pt square.
        function bar(v) {
            while (doc.pageItems.length) doc.pageItems[0].remove();
            var cp = doc.compoundPathItems.add();
            doc.pathItems.rectangle(500, 100, 60, 60).move(cp, ElementPlacement.PLACEATEND);
            doc.pathItems.rectangle(473, 160, 80, 6).move(cp, ElementPlacement.PLACEATEND);
            cp.pathItems[0].filled = true; cp.pathItems[0].fillColor = gray(100); cp.pathItems[0].stroked = false;
            if (v) effect(cp, v);
        }
        bar(0); pdf('bar');
        bar(-4); pdf('bar-minus4');
        return 'DONE|' + out.join('|');
    } catch (e) {
        return 'FAIL|' + String(e).replace(/[|\r\n]/g, ' ') + ' line ' + e.line + '|' + out.join('|');
    }
    }
})();
