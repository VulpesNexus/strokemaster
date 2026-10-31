// Route pass and miters (region-prep-23): census row 65's text (German Beauty "VWAMNvwkyzK",
// 36 pt, black fill, white 8 pt Center Miter stroke on a 50% backdrop) under the stroke route
// and native-fix, exported with the route's default Outline Object pass (<runId>-object.pdf),
// with no pass (<runId>-none.pdf), and with Outline Stroke on the route's field
// (<runId>-outline-stroke.pdf), and with Outline Object then Outline Stroke (<runId>-outline-both.pdf);
// each also as Illustrator's own raster (.png); <runId>-fill.pdf is the unstroked reference.
// ISE_REGION_ARGS: outDir, runId, font (default GermanBeauty).
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId) return 'FAIL|missing arguments';
    if (app.documents.length) return 'FAIL|requires zero preexisting documents';
    var interaction = app.userInteractionLevel, out = {}, doc = null, at = 'start';
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    var lib = $.evalFile(new File(new File($.fileName).parent.fsName + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    // Illustrator's own raster of the artboard, 600 ppi.
    function png(stem) {
        var f = lib.path(args, stem + '.png');
        if (f.exists) throw Error('refusing existing PNG');
        var o = new ExportOptionsPNG24(); o.horizontalScale = o.verticalScale = 600 / 72 * 100;
        o.artBoardClipping = true; o.antiAliasing = true; o.transparency = false;
        doc.exportFile(f, ExportType.PNG24, o);
    }
    function block(raw, name) { var m = new RegExp('"' + name + '":\{[^}]*\}').exec(raw); return m ? m[0] : raw; }
    try {
        var p = new DocumentPreset(); p.colorMode = DocumentColorSpace.RGB; p.width = 400; p.height = 160; p.units = RulerUnits.Points;
        doc = app.documents.addDocument('Web', p);
        doc.artboards[0].artboardRect = [0, 160, 400, 0];
        var back = doc.pathItems.rectangle(160, 0, 400, 160); back.fillColor = gray(50); back.stroked = false;
        var t = doc.textFrames.add(); t.contents = 'VWAMNvwkyzK'; t.position = [20, 110];
        var a = t.textRange.characterAttributes;
        a.textFont = app.textFonts.getByName(args.font || 'GermanBeauty'); a.size = 36;
        a.fillColor = gray(100); a.strokeColor = new NoColor();
        app.redraw(); lib.pdf(doc, args, 'fill');
        a.strokeColor = gray(0); a.strokeWeight = 8; a.strokeJoin = StrokeJoin.MITERENDJOIN;
        doc.selection = null; t.selected = true;
        at = 'route';
        out.route = block(send('native-route'), 'appearance');
        out.fix = block(send('native-fix'), 'nativeFix');
        doc.selection = null; t.selected = true;
        out.dumpObject = send('style-dump');
        out.charLimits = send('char-limits');
        doc.selection = null; app.redraw(); lib.pdf(doc, args, 'object'); png('object');
        doc.selection = null; t.selected = true;
        out.none = block(send('route-pass', '-'), 'appearance');
        doc.selection = null; app.redraw(); lib.pdf(doc, args, 'none'); png('none');
        doc.selection = null; t.selected = true;
        out.outlineStroke = block(send('route-pass', 'outline-stroke'), 'appearance');
        out.dump = send('style-dump');
        doc.selection = null; app.redraw(); lib.pdf(doc, args, 'outline-stroke'); png('outline-stroke');
        doc.selection = null; t.selected = true;
        out.outlineBoth = block(send('route-pass', 'outline-both'), 'appearance');
        out.dumpBoth = send('style-dump');
        doc.selection = null; app.redraw(); lib.pdf(doc, args, 'outline-both'); png('outline-both');
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
