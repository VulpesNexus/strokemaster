// Route pass test (region-prep-23): can the route's own paint pass be written as paths, so a
// PDF's text is extracted once? One owned RGB document:
//   probe  point text with Illustrator's own Effect > Path > Outline Object applied through its
//          menu command, read back by style-dump (the effect's name and parameters)
//   S      a two-frame story, one red fill and one gray 7 pt stroke: the story script's group stroke
//   F      point text, one red fill, mixed strokes: native-route's fill above Characters
// Exports <runId>-fill.pdf (strokes off), <runId>-route.pdf, then route-pass <effect> on S and F,
// <runId>-pass.pdf, and <runId>-results.json (effects list, dumps, bridge answers).
// ISE_REGION_ARGS: outDir, runId, font; optional effect (else the name read back from the probe);
// noPass: the control, every step but route-pass.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId || !args.font) return 'FAIL|missing arguments';
    if (app.documents.length) return 'FAIL|requires zero preexisting documents';
    var interaction = app.userInteractionLevel, at = 'start', doc = null, out = {};
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    function rgb(r, g, b) { var c = new RGBColor(); c.red = r; c.green = g; c.blue = b; return c; }
    function style(t, font) {
        var a = t.story.textRange.characterAttributes;
        a.textFont = font; a.size = 38; a.leading = 28; a.autoLeading = false;
        a.strokeColor = new NoColor(); a.fillColor = rgb(200, 30, 30);
    }
    try {
        if (send('status').indexOf('"build":"region-prep-23"') < 0) throw Error('unexpected plugin build');
        out.effects = send('effects-list');
        var font = app.textFonts.getByName(args.font);
        var p = new DocumentPreset(); p.colorMode = DocumentColorSpace.RGB; p.width = 480; p.height = 700; p.units = RulerUnits.Points;
        doc = app.documents.addDocument('Web', p);
        doc.artboards[0].artboardRect = [0, 700, 480, 0];
        at = 'probe';
        var probe = doc.textFrames.add(); probe.contents = 'AV'; probe.position = [20, 690];
        style(probe, font);
        probe.textRange.characterAttributes.strokeColor = gray(40); probe.textRange.characterAttributes.strokeWeight = 4;
        doc.selection = null; probe.selected = true;
        app.executeMenuCommand('Live Outline Object');
        app.redraw();
        doc.selection = null; probe.selected = true;
        out.probeDump = send('style-dump');
        var m = /'([^']*[Oo]utline[^']*)'/.exec(out.probeDump);
        var effect = args.effect || (m && m[1] === 'Adobe Outline Type' ? 'outline-object' : '');
        out.effect = effect;
        probe.remove();
        at = 'build';
        var t = doc.textFrames.areaText(doc.pathItems.rectangle(640, 35, 185, 48));
        t.nextFrame = doc.textFrames.areaText(doc.pathItems.rectangle(615, 45, 400, 160));
        t.contents = 'AVMWxy AVMWxy AVMWxy AVMWxy';
        style(t, font);
        var f = doc.textFrames.add(); f.position = [35, 300]; f.contents = 'AVMWxy AVMWxy\rAVMWxy AVMWxy';
        style(f, font);
        app.redraw(); lib.pdf(doc, args, 'fill');
        var all = t.story.textRange.characterAttributes; all.strokeColor = gray(40); all.strokeWeight = 7;
        var chars = f.textRange.characters;
        for (var j = 0; j < chars.length; j++) {
            if (/[ \r\n]/.test(chars[j].contents) || j % 3 === 0) continue;
            chars[j].characterAttributes.strokeColor = gray(j % 2 ? 40 : 70);
            chars[j].characterAttributes.strokeWeight = 7;
        }
        app.redraw(); lib.pdf(doc, args, 'native');
        at = 'route';
        doc.selection = null;
        for (var q = 0; q < t.story.textFrames.length; q++) t.story.textFrames[q].selected = true;
        out.story = /"appearance":\{[^}]*\}/.exec(String($.evalFile(new File(here + '/strokemaster-fix-story.jsx'))))[0];
        doc.selection = null; f.selected = true;
        out.frame = /"appearance":\{[^}]*\}/.exec(send('native-route'))[0];
        doc.selection = null; app.redraw();
        lib.pdf(doc, args, 'route');
        at = 'pass';
        if (!effect) throw Error('no outline effect found');
        doc.selection = null;
        doc.groupItems[0].selected = true; f.selected = true;
        if (args.noPass) out.pass = 'skipped (control)';
        else try { out.pass = /"appearance":\{[^}]*\}/.exec(send('route-pass', effect))[0]; }
        catch (passError) { out.pass = 'error: ' + String(passError) + ' ' + /"appearance":\{[^}]*\}/.exec(send('status'))[0]; }
        doc.selection = null; f.selected = true;
        out.frameDump = send('style-dump');
        doc.selection = null; app.redraw();
        lib.pdf(doc, args, 'pass');
        out.snapshot = lib.snapshot(doc);
        // Still live text, still editable, and the pass follows an edit.
        at = 'edit';
        // editMode: append (the last character's contents += 'AV'), whole (the frame's contents
        // += 'AV'), or insert (characters.add at the end).
        if (args.editMode === 'whole') f.contents = f.contents + 'AV';
        else if (args.editMode === 'insert') f.textRange.characters.add('AV');
        else f.textRange.characters[f.textRange.characters.length - 1].contents += 'AV';
        app.redraw();
        lib.pdf(doc, args, 'pass-edited');
        out.edited = f.contents;
        doc.selection = null; f.selected = true;
        out.editedDump = send('style-dump');
        out.editedFillFirst = /"appearance":\{[^}]*\}/.exec(send('appearance', 'fill-first:?'))[0];
        var ec = f.textRange.characters, tail = [];
        for (var z = Math.max(0, ec.length - 6); z < ec.length; z++) {
            try {
                var za = ec[z].characterAttributes;
                tail.push([ec[z].contents, za.fillColor.typename, za.strokeColor.typename,
                           za.strokeColor.typename === 'GrayColor' ? za.strokeColor.gray : -1, za.strokeWeight]);
            } catch (tailError) { tail.push(['?', String(tailError)]); }
        }
        out.editedTail = tail;
        doc.selection = null;
        var ai = lib.path(args, 'pass.ai'), so = new IllustratorSaveOptions(); so.pdfCompatible = true;
        doc.saveAs(ai, so);
        doc.close(SaveOptions.DONOTSAVECHANGES); doc = null;
        lib.write(args, 'results.json', out);
        return 'DONE|effect=' + effect;
    } catch (e) {
        if (doc) try { doc.close(SaveOptions.DONOTSAVECHANGES); } catch (ignored) {}
        out.error = String(e); out.stage = at;
        try { lib.write(args, 'results.json', out); } catch (w) {}
        return 'FAIL|stage=' + at + '|error=' + String(e);
    } finally { app.userInteractionLevel = interaction; }
})();
