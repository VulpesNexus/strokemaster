// StrokeMaster native route (bridge command "native-route", region-prep-14): which route each
// kind of text frame gets, and whether the result follows edits. Five frames, each alone in an
// owned document state (the others removed), 48 pt, leading 34 pt, 8 pt strokes:
//   strokes  "HH" / "HH": black fills, mixed character strokes (unstroked, 40% and 70% gray)
//   fills    "HH" / "HH": mixed fills (Black and 85% Black), one 40% gray 8 pt stroke
//   both     mixed fills and mixed strokes
//   plain    no stroke at all
//   styled   the strokes frame, already given an object-level fill above Characters
// For each: <case>-fill.pdf (strokes removed), <case>-native.pdf, then native-route,
// <case>-routed.pdf, the route's status, an edit ("H" appended to line 2) and
// <case>-edited.pdf. The strokes and fills documents are saved as <runId>-<case>.ai for a
// reopen and edit without the plugin. Needs $.global.ISE_REGION_ARGS outDir and runId.
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
    function frame(kind, stroked) {
        clear();
        var t = doc.textFrames.add();
        t.contents = 'HH\rHH';
        t.position = [20, 180];
        var all = t.textRange.characterAttributes;
        all.size = 48; all.leading = 34; all.autoLeading = false;
        all.fillColor = gray(100);
        all.strokeColor = new NoColor();
        var chars = t.textRange.characters;
        if (kind === 'fills' || kind === 'both') {
            chars[1].characterAttributes.fillColor = gray(85);
            chars[3].characterAttributes.fillColor = gray(85);
        }
        if (!stroked || kind === 'plain') { return t; }
        if (kind === 'fills') {
            var fresh = t.textRange.characterAttributes;   // runs split by the fills above
            fresh.strokeColor = gray(40); fresh.strokeWeight = 8;
        } else {
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
        var kinds = ['strokes', 'fills', 'both', 'plain', 'styled'];
        for (var k = 0; k < kinds.length; k++) {
            var kind = kinds[k];
            doc = newDocument(DocumentColorSpace.RGB, 200, 200);
            doc.artboards[0].artboardRect = [0, 200, 200, 0];
            at = kind + '.fill'; frame(kind === 'styled' ? 'strokes' : kind, false); app.redraw(); pdf(kind + '-fill');
            at = kind + '.native'; var t = frame(kind === 'styled' ? 'strokes' : kind, true); app.redraw(); pdf(kind + '-native');
            if (kind === 'styled') { note('styled.pre', block(send('appearance', 'fill-above'), 'appearance')); }
            at = kind + '.route';
            note(kind + '.route', block(send('native-route'), 'appearance'));
            app.redraw();
            pdf(kind + '-routed');
            at = kind + '.edit';
            doc.textFrames[0].textRange.characters[4].contents = 'HH';
            app.redraw();
            note(kind + '.edited', doc.textFrames[0].contents.replace(/\r/g, '/'));
            pdf(kind + '-edited');
            app.undo();
            app.redraw();
            if (kind === 'strokes' || kind === 'fills') {
                var ai = new File(args.outDir + '/' + args.runId + '-' + kind + '.ai');
                if (ai.exists) throw 'refusing to overwrite ' + ai.name;
                var so = new IllustratorSaveOptions();
                so.pdfCompatible = true;
                doc.saveAs(ai, so);
            }
            doc.close(SaveOptions.DONOTSAVECHANGES);
            doc = null;
        }
        return 'DONE|' + out.join('|');
    } catch (error) {
        if (doc) { try { doc.close(SaveOptions.DONOTSAVECHANGES); } catch (ignored) {} }
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + '|' + out.join('|');
    }
    }
})();
