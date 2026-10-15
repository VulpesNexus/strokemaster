// Test of tools/jsx/strokemaster-fix-selection.jsx as shipped: a scene with mixed-stroke text,
// text with both fills and strokes mixed, an Outside 4 thorn and overlapping squares, a dashed
// Center 8 thorn, and Inside 4 overlapping squares; everything selected; the script evaluated; its message returned; a
// flat PDF before and after. Then the script again with nothing selected. Needs
// $.global.ISE_REGION_ARGS outDir and runId.
(function () {
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try { return run(); } finally { app.userInteractionLevel = interaction; }
    function run() {
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId) return 'FAIL|missing outDir or runId';
    var out = [];
    function note(k, v) { out.push(k + '=' + String(v).replace(/[|\r\n]/g, ' ')); }
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    function pdf(doc, name) {
        var f = new File(args.outDir + '/' + args.runId + '-' + name + '.pdf');
        var o = new PDFSaveOptions(); o.acrobatLayers = false; o.preserveEditability = false;
        doc.saveAs(f, o);
    }
    var doc = app.documents.addDocument('Web', new DocumentPreset());
    var script = new File(new File($.fileName).parent + '/strokemaster-fix-selection.jsx');
    try {
        doc.artboards[0].artboardRect = [0, 400, 400, 0];
        var t = doc.textFrames.add(); t.contents = 'HH\rHH'; t.position = [20, 380];
        var a = t.textRange.characterAttributes; a.size = 48; a.leading = 34; a.autoLeading = false;
        a.fillColor = gray(100); a.strokeColor = new NoColor();
        var ch = t.textRange.characters;
        ch[1].characterAttributes.strokeColor = gray(40); ch[1].characterAttributes.strokeWeight = 8;
        for (var i = 3; i < 5; i++) { ch[i].characterAttributes.strokeColor = gray(70); ch[i].characterAttributes.strokeWeight = 8; }
        var b = doc.textFrames.add(); b.contents = 'HH'; b.position = [200, 380];
        var ba = b.textRange.characterAttributes; ba.size = 48; ba.fillColor = gray(100); ba.strokeColor = new NoColor();
        b.textRange.characters[0].characterAttributes.fillColor = gray(60);
        b.textRange.characters[1].characterAttributes.strokeColor = gray(40); b.textRange.characters[1].characterAttributes.strokeWeight = 6;
        function thorn(x, y) {
            var p = doc.pathItems.add(), k = Math.sin(5 * Math.PI / 180) * 20, c = Math.cos(5 * Math.PI / 180) * 20;
            p.setEntirePath([[x, y], [x + k, y - c], [x + 15, y - c], [x + 15, y - c - 30], [x - 15, y - c - 30], [x - 15, y - c], [x - k, y - c]]);
            p.closed = true; p.filled = true; p.fillColor = gray(100); p.stroked = true; p.strokeColor = gray(40);
            return p;
        }
        var th = thorn(60, 200); th.strokeWidth = 4;
        var sq = doc.compoundPathItems.add();
        var s1 = sq.pathItems.add(); s1.setEntirePath([[120, 200], [160, 200], [160, 160], [120, 160]]); s1.closed = true;
        var s2 = sq.pathItems.add(); s2.setEntirePath([[140, 180], [180, 180], [180, 140], [140, 140]]); s2.closed = true;
        s1.filled = true; s1.fillColor = gray(100); s1.stroked = true; s1.strokeColor = gray(40); s1.strokeWidth = 4;
        doc.selection = null; doc.selection.length; th.selected = true; sq.selected = true;
        note('align', /"ok":(true|false)/.exec(send('stroke-align', 'outside'))[1]);
        var dt = thorn(260, 200); dt.strokeWidth = 8; dt.strokeDashes = [12, 4];
        // Inside 4 on overlapping squares (Pathfinder Add expected).
        var iq = doc.compoundPathItems.add();
        var i1 = iq.pathItems.add(); i1.setEntirePath([[220, 120], [260, 120], [260, 80], [220, 80]]); i1.closed = true;
        var i2 = iq.pathItems.add(); i2.setEntirePath([[240, 100], [280, 100], [280, 60], [240, 60]]); i2.closed = true;
        i1.filled = true; i1.fillColor = gray(100); i1.stroked = true; i1.strokeColor = gray(40); i1.strokeWidth = 4;
        doc.selection = null; doc.selection.length; iq.selected = true;
        note('alignInside', /"ok":(true|false)/.exec(send('stroke-align', 'inside'))[1]);
        app.redraw();
        pdf(doc, 'before');
        doc.selection = null; doc.selection.length;
        for (var s = 0; s < doc.pageItems.length; s++) { doc.pageItems[s].selected = true; }
        note('selected', doc.selection.length);
        note('message', $.evalFile(script));
        app.redraw();
        pdf(doc, 'after');
        note('textStillLive', doc.textFrames.length === 2 && doc.textFrames[1].contents.replace(/\r/g, '/') === 'HH/HH');
        doc.selection = null; doc.selection.length;
        note('empty', $.evalFile(script));
    } catch (e) { note('error', e + ' line ' + e.line); }
    doc.close(SaveOptions.DONOTSAVECHANGES);
    return 'DONE|' + out.join('|');
    }
})();
