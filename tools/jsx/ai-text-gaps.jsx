// StrokeMaster text gaps in Illustrator (2026-10-15): exports other than PDF, threaded text,
// and appearance rows with real paint. Runs through tools/run-ai-fresh.ps1 with
// StrokeMasterRegionProbe loaded. Needs $.global.ISE_REGION_ARGS outDir and runId.
//
// Part A, exports. The mixed-stroke fixture (point text "HH" / "HH", 48 pt, leading 34 pt,
// black fills, mixed 8 pt strokes) in four documents: fill (strokes removed), native,
// native route (native-route: an object fill above Characters), and effect (the text-kept
// stroke-behind effect). Each is saved as PDF, EPS, SVG (fonts as SVG text), and PNG at 600 ppi.
// Part B, threaded text: two linked area frames, the second overlapping the first by 20 pt,
// flowing "HH HH HH HH" with mixed strokes; PDFs native, native route, and effect.
// Part C, existing appearance rows: the fixture with an object-level 2 pt black stroke
// beneath Characters (added by the bridge's stroke-below, 0.4 gray 8 pt) and the effect
// applied on top; PDF.
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
    function block(json, name) { var m = new RegExp('"' + name + '":(\\{[^}]*\\})').exec(json); return m ? m[1] : '?'; }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    var effect = '<LiveEffect name="org.strokemaster.proposed.stroke-behind-text.v1"><Dict data="I StrokeMasterRegionFixtureVersion 1 "/></LiveEffect>';
    var doc = null, at = 'begin';
    function file(name) {
        var f = new File(args.outDir + '/' + args.runId + '-' + name);
        if (f.exists) throw 'refusing to overwrite ' + f.name;
        return f;
    }
    function strokeChars(t, from) {
        var chars = t.textRange.characters;
        for (var i = from; i < chars.length; ++i) {
            if (chars[i].contents === ' ' || chars[i].contents === '\r') { continue; }
            var gk = (i % 3 === 0) ? null : (i % 3 === 1 ? 40 : 70);
            if (gk === null) { continue; }
            chars[i].characterAttributes.strokeColor = gray(gk);
            chars[i].characterAttributes.strokeWeight = 8;
        }
    }
    function fixture(stroked) {
        var t = doc.textFrames.add();
        t.contents = 'HH\rHH';
        t.position = [20, 180];
        var all = t.textRange.characterAttributes;
        all.size = 48; all.leading = 34; all.autoLeading = false;
        all.fillColor = gray(100); all.strokeColor = new NoColor();
        if (stroked) {
            var chars = t.textRange.characters;
            chars[1].characterAttributes.strokeColor = gray(40);
            chars[1].characterAttributes.strokeWeight = 8;
            for (var i = 3; i < 5; ++i) { chars[i].characterAttributes.strokeColor = gray(70); chars[i].characterAttributes.strokeWeight = 8; }
        }
        return t;
    }
    function newDoc() { doc = newDocument(DocumentColorSpace.RGB, 200, 200); doc.artboards[0].artboardRect = [0, 200, 200, 0]; }
    function select(items) { doc.selection = null; var n = doc.selection.length; for (var q = 0; q < items.length; q++) { items[q].selected = true; } return n; }
    function exports(name) {
        app.redraw();
        var pdfo = new PDFSaveOptions(); pdfo.acrobatLayers = false; pdfo.preserveEditability = false;
        doc.saveAs(file(name + '.pdf'), pdfo);
        // Cropped to the artboard, so every EPS shares one origin (cropped to its artwork, a
        // stroked file is offset from its stroke-free reference).
        var eps = new EPSSaveOptions(); eps.embedAllFonts = true; eps.saveMultipleArtboards = true; eps.artboardRange = '1';
        doc.saveAs(file(name + '.eps'), eps);
        var svg = new ExportOptionsSVG(); svg.fontType = SVGFontType.SVGFONT; svg.embedRasterImages = true;
        doc.exportFile(file(name + '.svg'), ExportType.SVG, svg);
        var png = new ExportOptionsPNG24(); png.antiAliasing = true; png.transparency = false; png.artBoardClipping = true;
        png.horizontalScale = 600 / 72 * 100; png.verticalScale = 600 / 72 * 100;
        doc.exportFile(file(name + '.png'), ExportType.PNG24, png);
        note(name + '.exported', 'pdf eps svg png');
    }
    try {
        note('build', /"build":"([^"]*)"/.exec(send('status'))[1]);
        var cases = ['fill', 'native', 'route', 'effect'];
        for (var c = 0; c < cases.length; c++) {
            at = 'A.' + cases[c];
            newDoc();
            var t = fixture(cases[c] !== 'fill');
            if (cases[c] === 'route') { select([t]); note('A.route', block(send('native-route'), 'appearance')); }
            if (cases[c] === 'effect') { t.applyEffect(effect); app.redraw(); note('A.effect', block(send('status'), 'textBehind')); }
            exports('A-' + cases[c]);
            doc.close(SaveOptions.DONOTSAVECHANGES); doc = null;
        }
        var threaded = ['fill', 'native', 'route', 'effect'];
        for (var d = 0; d < threaded.length; d++) {
            at = 'B.' + threaded[d];
            newDoc();
            var b1 = doc.pathItems.rectangle(190, 10, 110, 70);
            var b2 = doc.pathItems.rectangle(140, 30, 110, 70);
            var f1 = doc.textFrames.areaText(b1);
            var f2 = doc.textFrames.areaText(b2);
            f1.nextFrame = f2;
            f1.contents = 'HH HH HH HH';
            var a = f1.textRange.characterAttributes;
            a.size = 48; a.leading = 50; a.autoLeading = false; a.fillColor = gray(100); a.strokeColor = new NoColor();
            if (threaded[d] !== 'fill') { strokeChars(f1, 0); }
            if (threaded[d] === 'route') { select([f1, f2]); note('B.route', block(send('native-route'), 'appearance')); }
            if (threaded[d] === 'effect') { f1.applyEffect(effect); f2.applyEffect(effect); app.redraw(); note('B.effect', block(send('status'), 'textBehind')); }
            note('B.frames', f1.contents.length + '/' + f2.contents.length);
            app.redraw();
            var pdfo2 = new PDFSaveOptions(); pdfo2.acrobatLayers = false; pdfo2.preserveEditability = false;
            doc.saveAs(file('B-' + threaded[d] + '.pdf'), pdfo2);
            doc.close(SaveOptions.DONOTSAVECHANGES); doc = null;
        }
        at = 'C';
        newDoc();
        var tc = fixture(true);
        select([tc]);
        note('C.styled', block(send('appearance', 'stroke-below'), 'appearance'));
        doc.selection = null;
        tc.applyEffect(effect);
        app.redraw();
        note('C.effect', block(send('status'), 'textBehind'));
        var pdfo3 = new PDFSaveOptions(); pdfo3.acrobatLayers = false; pdfo3.preserveEditability = false;
        doc.saveAs(file('C-effect.pdf'), pdfo3);
        doc.close(SaveOptions.DONOTSAVECHANGES); doc = null;
        return 'DONE|' + out.join('|');
    } catch (error) {
        if (doc) { try { doc.close(SaveOptions.DONOTSAVECHANGES); } catch (ignored) {} }
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + '|' + out.join('|');
    }
    }
})();
