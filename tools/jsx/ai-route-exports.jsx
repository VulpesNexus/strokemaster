// Route exports (region-prep-23): the text routes with their outlined passes in every export.
// One owned RGB document: S, a two-frame story with one red fill and one gray 7 pt stroke (the
// story script's group stroke); F, point text with one red fill and mixed strokes (native-route's
// fill above Characters). Each format is exported stroke-free (<runId>-fill.<ext>), natively
// (<runId>-native.<ext>), and routed (<runId>-route.<ext>): PDF, PDF/X-4 and PDF/X-1a
// (<stem>-pdfx4.pdf, <stem>-pdfx1a.pdf), PNG and JPEG (Illustrator's own rasters, 300 ppi), SVG
// (text kept as text), and EPS. ISE_REGION_ARGS: outDir, runId, font.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId || !args.font) return 'FAIL|missing arguments';
    if (app.documents.length) return 'FAIL|requires zero preexisting documents';
    var interaction = app.userInteractionLevel, out = {}, doc = null, at = 'start';
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    function rgb(r, g, b) { var c = new RGBColor(); c.red = r; c.green = g; c.blue = b; return c; }
    function fresh(stem) { var f = lib.path(args, stem); if (f.exists) throw Error('refusing existing ' + f.name); return f; }
    function exportAll(stem) {
        app.redraw();
        lib.pdf(doc, args, stem);
        var png = new ExportOptionsPNG24(); png.horizontalScale = png.verticalScale = 300 / 72 * 100;
        png.artBoardClipping = true; png.antiAliasing = true; png.transparency = false;
        doc.exportFile(fresh(stem + '.png'), ExportType.PNG24, png);
        // Defaults: text stays text (SVG font type left as Illustrator sets it).
        doc.exportFile(fresh(stem + '.svg'), ExportType.SVG, new ExportOptionsSVG());
        // PDF/X presets (X-1a flattens), and JPEG at 300 ppi.
        var presets = [['pdfx4', '[PDF/X-4:2008]'], ['pdfx1a', '[PDF/X-1a:2001]']];
        for (var k = 0; k < presets.length; k++) {
            var xo = new PDFSaveOptions(); xo.pDFPreset = presets[k][1]; xo.viewAfterSaving = false;
            doc.saveAs(fresh(stem + '-' + presets[k][0] + '.pdf'), xo);
        }
        var jpg = new ExportOptionsJPEG(); jpg.horizontalScale = jpg.verticalScale = 300 / 72 * 100;
        jpg.artBoardClipping = true; jpg.antiAliasing = true; jpg.qualitySetting = 100;
        doc.exportFile(fresh(stem + '.jpg'), ExportType.JPEG, jpg);
        var eps = new EPSSaveOptions(); eps.embedAllFonts = true; eps.cmykPostScript = false;
        var copy = fresh(stem + '.eps');
        doc.saveAs(copy, eps);
    }
    try {
        if (send('status').indexOf('"build":"' + (args.expectedBuild || 'region-prep-23') + '"') < 0) throw Error('unexpected plugin build');
        var font = app.textFonts.getByName(args.font);
        var p = new DocumentPreset(); p.colorMode = DocumentColorSpace.RGB; p.width = 480; p.height = 600; p.units = RulerUnits.Points;
        doc = app.documents.addDocument('Web', p);
        doc.artboards[0].artboardRect = [0, 600, 480, 0];
        var t = doc.textFrames.areaText(doc.pathItems.rectangle(560, 35, 185, 48));
        t.nextFrame = doc.textFrames.areaText(doc.pathItems.rectangle(535, 45, 400, 160));
        t.contents = 'AVMWxy AVMWxy AVMWxy AVMWxy';
        var f = doc.textFrames.add(); f.position = [35, 250]; f.contents = 'AVMWxy AVMWxy\rAVMWxy AVMWxy';
        var frames = [t, f];
        for (var i = 0; i < frames.length; i++) {
            var a = frames[i].story.textRange.characterAttributes;
            a.textFont = font; a.size = 38; a.leading = 28; a.autoLeading = false;
            a.fillColor = rgb(200, 30, 30); a.strokeColor = new NoColor();
        }
        at = 'fill'; exportAll('fill');
        var s = t.story.textRange.characterAttributes; s.strokeColor = gray(40); s.strokeWeight = 7;
        var chars = f.textRange.characters;
        for (var j = 0; j < chars.length; j++) {
            if (/[ \r\n]/.test(chars[j].contents) || j % 3 === 0) continue;
            chars[j].characterAttributes.strokeColor = gray(j % 2 ? 40 : 70);
            chars[j].characterAttributes.strokeWeight = 7;
        }
        at = 'native'; exportAll('native');
        at = 'route';
        doc.selection = null;
        for (var q = 0; q < t.story.textFrames.length; q++) t.story.textFrames[q].selected = true;
        out.story = /"gate":"[^"]*"/.exec(String($.evalFile(new File(here + '/strokemaster-fix-story.jsx'))))[0];
        doc.selection = null; f.selected = true;
        out.frame = /"appearance":\{"gate":"[^"]*"/.exec(send('native-route'))[0];
        doc.selection = null;
        exportAll('route');
        doc.close(SaveOptions.DONOTSAVECHANGES); doc = null;
        lib.write(args, 'results.json', out);
        return 'DONE';
    } catch (e) {
        if (doc) try { doc.close(SaveOptions.DONOTSAVECHANGES); } catch (ignored) {}
        out.error = String(e); out.stage = at;
        try { lib.write(args, 'results.json', out); } catch (w) {}
        return 'FAIL|' + at + '|' + String(e);
    } finally { app.userInteractionLevel = interaction; }
})();
