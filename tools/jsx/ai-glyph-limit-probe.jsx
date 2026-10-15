// StrokeMaster glyph miter probe (2026-10-15): one glyph, large, stroked 2 * 4 / 36 of its
// size (as Center 8 on 36 pt text), exported once per character miter limit, for a pixel diff
// of where the limit changes the drawing. Needs $.global.ISE_REGION_ARGS outDir, runId, and
// optionally glyphs (default "vw"), font (PostScript name, default MinionPro-Regular), limits
// (default [4, 2]).
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
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    var glyphs = args.glyphs || 'vw', limits = args.limits || [4, 2];
    var size = 300, doc = newDocument(DocumentColorSpace.RGB, 400 * glyphs.length, 400);
    try {
        doc.artboards[0].artboardRect = [0, 400, 400 * glyphs.length, 0];
        var t = doc.textFrames.add();
        t.contents = glyphs.split('').join('  ');
        t.position = [20, 330];
        var a = t.textRange.characterAttributes;
        a.textFont = app.textFonts.getByName(args.font || 'MinionPro-Regular');
        a.size = size; a.tracking = 200;
        var k = new GrayColor(); k.gray = 100; a.fillColor = k;
        var s = new GrayColor(); s.gray = 40; a.strokeColor = s; a.strokeWeight = size * 8 / 36;
        for (var i = 0; i < limits.length; i++) {
            doc.selection = null; doc.selection.length; t.selected = true;
            note('limit ' + limits[i], /"gate":"([a-z-]*)"/.exec(send('appearance', 'miter:' + limits[i]).replace(/.*"appearance":/, ''))[1]);
            doc.selection = null; doc.selection.length;
            app.redraw();
            var f = new File(args.outDir + '/' + args.runId + '-limit' + limits[i] + '.pdf');
            var o = new PDFSaveOptions(); o.acrobatLayers = false; o.preserveEditability = false;
            doc.saveAs(f, o);
        }
    } catch (e) { note('error', e + ' line ' + e.line); }
    doc.close(SaveOptions.DONOTSAVECHANGES);
    return 'DONE|' + out.join('|');
    }
})();
