// ai-text-align.jsx - what stroke alignment does to type in Illustrator (2026-10-16). One point-text
// frame, "Ax" in Minion Pro 72 pt, Black fill, 60% gray 8 pt miter stroke, in three set-ups:
//   chars  the stroke on the characters (the usual way)
//   below  the characters unstroked, an object-level stroke below Characters
//   above  the characters unstroked, an object-level stroke above Characters
// each set by align-survey to center, inside, and outside (SetStrokeAlignment on the frame), and
// exported alone as T-<setup>-<align>.pdf, beside T-fill.pdf (unstroked) for the glyph mask, for
// tools/measure_text_align.py. Needs StrokeMasterRegionProbe region-prep-17 and
// $.global.ISE_REGION_ARGS outDir and runId. Alerts suppressed and restored.
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
    function pdf(name) {
        var f = new File(args.outDir + '/' + args.runId + '-' + name + '.pdf');
        if (f.exists) throw 'refusing to overwrite ' + f.name;
        var o = new PDFSaveOptions(); o.acrobatLayers = false; o.preserveEditability = false;
        doc.saveAs(f, o);
    }
    function frame(stroked) {
        while (doc.pageItems.length) doc.pageItems[0].remove();
        var t = doc.textFrames.add(); t.contents = 'Ax';
        var ca = t.textRange.characterAttributes;
        try { ca.textFont = app.textFonts.getByName('MinionPro-Regular'); } catch (e) {}
        ca.size = 72; ca.fillColor = gray(100);
        if (stroked) { ca.strokeColor = gray(60); ca.strokeWeight = 8; ca.strokeJoin = StrokeJoin.MITERENDJOIN; }
        else ca.strokeColor = new NoColor();
        t.position = [30, 160];
        return t;
    }
    try {
        doc = newDocument(DocumentColorSpace.RGB, 240, 200);
        doc.artboards[0].artboardRect = [0, 200, 240, 0];
        at = 'fill'; frame(false); app.redraw(); pdf('T-fill');
        var setups = ['chars', 'below', 'above'], aligns = ['center', 'inside', 'outside'];
        for (var s = 0; s < setups.length; s++) {
            for (var a = 0; a < aligns.length; a++) {
                at = setups[s] + ' ' + aligns[a];
                var t = frame(setups[s] === 'chars');
                doc.selection = null; t.selected = true;
                if (setups[s] === 'below') note(at + ' stroke', send('appearance', 'stroke-below'));
                if (setups[s] === 'above') note(at + ' stroke', send('align-survey', 'stroke-above'));
                doc.selection = null; t.selected = true;
                note(at, send('align-survey', aligns[a]));
                doc.selection = null;
                app.redraw();
                pdf('T-' + setups[s] + '-' + aligns[a]);
            }
        }
        doc = null;
        return 'DONE|' + out.join('|');
    } catch (error) {
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + ' line ' + error.line + '|' + out.join('|');
    }
    }
})();
