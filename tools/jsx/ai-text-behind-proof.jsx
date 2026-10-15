// StrokeMaster stroke-behind with live text kept (region-prep-13 effect
// org.strokemaster.proposed.stroke-behind-text.v1, TextBehind.h). Runs inside
// Illustrator via COM DoJavaScript, in one owned document.
//
// Fixture as ai-stroke-behind-proof.jsx: point text "HH" / "HH", 48 pt, leading
// 34 pt, every fill black, 8 pt strokes with mixed per-character paint: line 1
// glyph 1 unstroked (the victim), line 1 glyph 2 40% gray, line 2 70% gray.
//
// Exports flat PDFs:
//   fill     the same text with every stroke removed (fill reference)
//   native   the fixture with no effect
//   outlined the fixture under the pass-through census effect (outline export)
//   behind   the fixture under the text-kept stroke-behind effect
//   edited   after appending "H" to line 2 of the effected, still-live text
//   undo     after undoing that edit
// and saves the document as <runId>-doc.ai for a fresh-process reopen.
// The PDFs are measured externally (damage, ink, text characters, fonts). Needs
// $.global.ISE_REGION_ARGS outDir and runId. Alerts suppressed and restored.
(function () {
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try { return run(); } finally { app.userInteractionLevel = interaction; }
    function run() {
    // documents.add failed with error 129 once Illustrator's recent new-document sizes had
    // filled with scripted ones (2026-10-15); addDocument with a profile still works.
    function newDocument(cs, w, h) {
        try { return app.documents.add(cs, w, h); } catch (e) {
            var pr = new DocumentPreset(); pr.colorMode = cs; pr.width = w; pr.height = h; pr.units = RulerUnits.Points;
            return app.documents.addDocument(cs === DocumentColorSpace.CMYK ? 'Print' : 'Web', pr);
        }
    }
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId) return 'FAIL|missing outDir or runId';
    var plugin = 'StrokeMasterRegionProbe';
    var xml = '<LiveEffect name="org.strokemaster.proposed.stroke-behind-text.v1"><Dict data="I StrokeMasterRegionFixtureVersion 1 "/></LiveEffect>';
    var censusXml = '<LiveEffect name="org.strokemaster.proposed.input-census.v1"><Dict data="I StrokeMasterRegionFixtureVersion 1 "/></LiveEffect>';
    var out = [];
    function note(k, v) { out.push(k + '=' + String(v).replace(/[|\r\n]/g, ' ')); }
    function send(selector, input) { return String(app.sendScriptMessage(plugin, selector, input || '')); }
    function block(json, name) {
        var m = new RegExp('"' + name + '":(\\{[^}]*\\})').exec(json);
        return m ? m[1] : '?';
    }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    var doc = null, at = 'begin';
    function fixture(stroked) {
        while (doc.pageItems.length > 0) doc.pageItems[0].remove();
        var t = doc.textFrames.add();
        t.contents = 'HH\rHH';
        t.position = [20, 180];
        var all = t.textRange.characterAttributes;
        all.size = 48;
        all.leading = 34;
        all.autoLeading = false;
        all.fillColor = gray(100);
        all.strokeColor = new NoColor();
        if (stroked) {
            var chars = t.textRange.characters;
            // characters: H H CR H H
            chars[1].characterAttributes.strokeColor = gray(40);
            chars[1].characterAttributes.strokeWeight = 8;
            for (var i = 3; i < 5; ++i) {
                chars[i].characterAttributes.strokeColor = gray(70);
                chars[i].characterAttributes.strokeWeight = 8;
            }
        }
        return t;
    }
    function pdf(name) {
        var f = new File(args.outDir + '/' + args.runId + '-' + name + '.pdf');
        if (f.exists) throw 'refusing to overwrite ' + f.name;
        var options = new PDFSaveOptions();
        options.acrobatLayers = false;
        doc.saveAs(f, options);
        note(name + '.pdf', f.length);
    }
    try {
        note('build', /"build":"([^"]*)"/.exec(send('status'))[1]);
        doc = newDocument(DocumentColorSpace.RGB, 200, 200);
        doc.artboards[0].artboardRect = [0, 200, 200, 0];

        at = 'fill';
        fixture(false);
        app.redraw();
        pdf('fill');

        at = 'native';
        fixture(true);
        app.redraw();
        pdf('native');

        at = 'outlined';
        fixture(true).applyEffect(censusXml);
        app.redraw();
        pdf('outlined');

        at = 'behind';
        var t = fixture(true);
        t.applyEffect(xml);
        app.redraw();
        note('behind.status', block(send('status'), 'textBehind'));
        note('behind.liveText', doc.textFrames.length === 1 && doc.textFrames[0].contents === 'HH\rHH');
        pdf('behind');

        at = 'edit';
        t = doc.textFrames[0];
        t.textRange.characters[4].contents = 'HH';
        app.redraw();
        note('edited.contents', doc.textFrames[0].contents.replace(/\r/g, '/'));
        note('edited.status', block(send('status'), 'textBehind'));
        pdf('edited');

        at = 'undo';
        app.undo();
        app.redraw();
        note('undo.contents', doc.textFrames[0].contents.replace(/\r/g, '/'));
        note('undo.status', block(send('status'), 'textBehind'));
        pdf('undo');

        at = 'save';
        var ai = new File(args.outDir + '/' + args.runId + '-doc.ai');
        if (ai.exists) throw 'refusing to overwrite ' + ai.name;
        var saveOptions = new IllustratorSaveOptions();
        saveOptions.pdfCompatible = true;
        doc.saveAs(ai, saveOptions);
        note('save.ai', ai.length);

        at = 'close';
        doc.close(SaveOptions.DONOTSAVECHANGES);
        doc = null;
        return 'DONE|' + out.join('|');
    } catch (error) {
        if (doc) { try { doc.close(SaveOptions.DONOTSAVECHANGES); } catch (ignored) {} }
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + '|' + out.join('|');
    }
    }
})();
