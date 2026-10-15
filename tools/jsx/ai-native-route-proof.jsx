// StrokeMaster native route for text in Illustrator (2026-10-15): the correction
// as ordinary Appearance data, so text edits are followed with or without the
// plugin. The plugin's bridge only writes the attributes ("appearance" commands).
//
// Part A, the mixed-stroke fixture of ai-stroke-behind-proof.jsx (point text
// "HH" / "HH", 48 pt, leading 34 pt, black fills, 8 pt mixed character strokes):
//   fill        every stroke removed (fill reference)
//   native      the fixture as typed
//   fillabove   an object-level black fill above Characters
//   edited      "H" appended to line 2 of the fill-above text
// The fill-above document is saved as <runId>-fillabove.ai for reopening
// without the plugin.
// Part B, glyph tips: one line "AVMWvwNZ" in Times New Roman 60 pt, black fill,
// 4 pt 50% gray character strokes, fill above Characters, exported with the
// character join at miter limit 4 (the default), miter limit 2, and bevel.
// Needs $.global.ISE_REGION_ARGS outDir and runId. Alerts suppressed and restored.
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
    var out = [];
    function note(k, v) { out.push(k + '=' + String(v).replace(/[|\r\n]/g, ' ')); }
    function send(selector, input) { return String(app.sendScriptMessage(plugin, selector, input || '')); }
    function block(json, name) {
        var m = new RegExp('"' + name + '":(\\{[^}]*\\})').exec(json);
        return m ? m[1] : '?';
    }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    var doc = null, at = 'begin';
    function clear() { while (doc.pageItems.length > 0) doc.pageItems[0].remove(); }
    function fixture(stroked) {
        clear();
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
            chars[1].characterAttributes.strokeColor = gray(40);
            chars[1].characterAttributes.strokeWeight = 8;
            for (var i = 3; i < 5; ++i) {
                chars[i].characterAttributes.strokeColor = gray(70);
                chars[i].characterAttributes.strokeWeight = 8;
            }
        }
        return t;
    }
    function tips() {
        clear();
        var t = doc.textFrames.add();
        t.contents = 'AVMWvwNZ';
        t.position = [10, 120];
        var a = t.textRange.characterAttributes;
        try { a.textFont = app.textFonts.getByName('TimesNewRomanPSMT'); note('tips.font', 'TimesNewRomanPSMT'); }
        catch (e) { note('tips.font', 'default'); }
        a.size = 60;
        a.fillColor = gray(100);
        a.strokeColor = gray(50);
        a.strokeWeight = 4;
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

        at = 'fill'; fixture(false); app.redraw(); pdf('fill');
        at = 'native'; fixture(true); app.redraw(); pdf('native');

        at = 'fillabove';
        fixture(true);
        note('fillabove.status', block(send('appearance', 'fill-above'), 'appearance'));
        app.redraw();
        pdf('fillabove');

        at = 'edited';
        var t = doc.textFrames[0];
        t.textRange.characters[4].contents = 'HH';
        app.redraw();
        note('edited.contents', doc.textFrames[0].contents.replace(/\r/g, '/'));
        pdf('edited');
        app.undo();
        app.redraw();
        note('undo.contents', doc.textFrames[0].contents.replace(/\r/g, '/'));

        at = 'save';
        var ai = new File(args.outDir + '/' + args.runId + '-fillabove.ai');
        if (ai.exists) throw 'refusing to overwrite ' + ai.name;
        var saveOptions = new IllustratorSaveOptions();
        saveOptions.pdfCompatible = true;
        doc.saveAs(ai, saveOptions);
        note('save.ai', ai.length);

        var joins = ['4', '2', 'bevel'];
        for (var j = 0; j < joins.length; ++j) {
            at = 'tips' + joins[j];
            tips();
            note(at + '.fill', block(send('appearance', 'fill-above'), 'appearance'));
            note(at + '.join', block(send('appearance', 'miter:' + joins[j]), 'appearance'));
            app.redraw();
            pdf(at);
        }

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
