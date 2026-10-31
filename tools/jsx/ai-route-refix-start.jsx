// Route re-fix test, phase 1 of 3 (region-prep-22): does the route follow a recolor?
// One owned RGB document, five text cases stacked down the page, 38 pt, 7 pt strokes:
//   A  two-frame story, red fill, mixed strokes          -> story group, fill above Contents
//   B  two-frame story, mixed fills, one gray stroke      -> story group, stroke below Contents
//   C  point text, red fill, one gray stroke              -> frame, stroke below Characters
//   D  point text, red fill, mixed strokes                -> frame, fill above Characters
//   E  as D; phase 2 recolors all of it, not one word     -> the fill takes the new color
// Exports <runId>-fill.pdf (strokes off), <runId>-fill-recolor.pdf (strokes off, phase 2's
// recolor applied), <runId>-native.pdf, routes everything, exports <runId>-route.pdf, and
// keeps the document open for ai-route-refix-edit.jsx. ISE_REGION_ARGS: outDir, runId, font.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId || !args.font) return 'FAIL|missing arguments';
    if (app.documents.length) return 'FAIL|requires zero preexisting documents';
    var interaction = app.userInteractionLevel, at = 'start', doc = null;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, '')); }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    function rgb(r, g, b) { var c = new RGBColor(); c.red = r; c.green = g; c.blue = b; return c; }
    var text = 'AVMWxy AVMWxy AVMWxy AVMWxy';
    function story(top) {
        var wide = args.font === 'GreatVibes-Regular';
        var t = doc.textFrames.areaText(doc.pathItems.rectangle(top, 35, wide ? 400 : 185, wide ? 62 : 48));
        t.nextFrame = doc.textFrames.areaText(doc.pathItems.rectangle(top - 25, 45, 400, 160));
        t.contents = text;
        return t;
    }
    function point(top) {
        var t = doc.textFrames.add(); t.position = [35, top];
        t.contents = 'AVMWxy AVMWxy\rAVMWxy AVMWxy';
        return t;
    }
    function style(t) {
        var a = t.story.textRange.characterAttributes;
        a.textFont = app.textFonts.getByName(args.font); a.size = 38; a.leading = 28; a.autoLeading = false;
        a.strokeColor = new NoColor();
    }
    function fills(c) {
        var r = c.t.story.textRange;
        r.characterAttributes.fillColor = rgb(200, 30, 30);
        if (c.id === 'B') for (var j = 7; j < 13; j++) r.characters[j].characterAttributes.fillColor = rgb(30, 60, 200);
    }
    function strokes(c) {
        var r = c.t.story.textRange, chars = r.characters;
        if (c.id === 'B' || c.id === 'C') { r.characterAttributes.strokeColor = gray(40); r.characterAttributes.strokeWeight = 7; return; }
        for (var j = 0; j < chars.length; j++) {
            if (/[ \r\n]/.test(chars[j].contents) || j % 3 === 0) continue;
            chars[j].characterAttributes.strokeColor = gray(j % 2 ? 40 : 70);
            chars[j].characterAttributes.strokeWeight = 7;
        }
    }
    var lib2 = $.evalFile(new File(here + '/ai-route-refix-lib.jsx'));
    try {
        if (send('status').indexOf('"build":"' + (args.expectedBuild || 'region-prep-22') + '"') < 0) throw Error('unexpected plugin build');
        var p = new DocumentPreset(); p.colorMode = DocumentColorSpace.RGB; p.width = 480; p.height = 1100; p.units = RulerUnits.Points;
        doc = app.documents.addDocument('Web', p);
        doc.artboards[0].artboardRect = [0, 1100, 480, 0];
        var cases = [{id: 'A', t: story(1060)}, {id: 'B', t: story(840)}, {id: 'C', t: point(600)},
                     {id: 'D', t: point(420)}, {id: 'E', t: point(240)}];
        for (var i = 0; i < cases.length; i++) { style(cases[i].t); fills(cases[i]); }
        at = 'fill'; lib.pdf(doc, args, 'fill');
        lib2.recolor(doc); lib.pdf(doc, args, 'fill-recolor');
        for (i = 0; i < cases.length; i++) fills(cases[i]);
        for (i = 0; i < cases.length; i++) strokes(cases[i]);
        at = 'native'; lib.pdf(doc, args, 'native');
        at = 'route';
        var routes = {};
        // The stories through the guarded script, the point text through native-route and native-fix.
        for (i = 0; i < 2; i++) {
            doc.selection = null;
            var frames = cases[i].t.story.textFrames;
            for (var f = 0; f < frames.length; f++) frames[f].selected = true;
            routes[cases[i].id] = String($.evalFile(new File(here + '/strokemaster-fix-story.jsx')));
        }
        doc.selection = null;
        for (i = 2; i < cases.length; i++) cases[i].t.selected = true;
        routes.points = /"appearance":\{[^}]*\}/.exec(send('native-route'))[0];
        routes.fix = /"nativeFix":\{[^}]*\}/.exec(send('native-fix'))[0];
        doc.selection = null; app.redraw();
        lib.pdf(doc, args, 'route');
        lib.write(args, 'phase1.json', {host: app.version, font: args.font, routes: routes, groups: doc.groupItems.length,
                                        status: /"refix":\{[^}]*\}/.exec(send('status'))[0]});
        return 'DONE|phase1';
    } catch (e) {
        return 'FAIL|stage=' + at + '|error=' + String(e);
    } finally { app.userInteractionLevel = interaction; }
})();
