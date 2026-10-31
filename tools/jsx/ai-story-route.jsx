// Story route color and paint-model sweep (region-prep-22). Own documents only.
// ISE_REGION_ARGS: outDir, runId, font, cases (array of rgb, cmyk, spot, mixfill, uniform,
// mixboth, perframe, separate, separate-off; fix: also native-fix after the route, <case>-fixed.pdf; the last two are two independent one-frame stories
// in the same places, with and without the separate-stories opt-in), expectedBuild. Each case: a two-frame linked flow whose second frame overlaps
// the first's text; <case>-fill.pdf (strokes off), <case>-native.pdf, the guarded story
// script (strokemaster-fix-story.jsx, quiet), <case>-route.pdf, a recolor of the first
// word's fill (<case>-recolor.pdf; <case>-fill-recolor.pdf is its stroke-free reference), and the receipt <runId>-results.json.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId || !args.font) return 'FAIL|missing arguments';
    if (app.documents.length) return 'FAIL|requires zero preexisting documents';
    var interaction = app.userInteractionLevel, doc = null, rows = [], at = 'start';
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, '')); }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    function rgb(r, g, b) { var c = new RGBColor(); c.red = r; c.green = g; c.blue = b; return c; }
    function cmyk(c0, m, y, k) { var c = new CMYKColor(); c.cyan = c0; c.magenta = m; c.yellow = y; c.black = k; return c; }
    var spotColor = null;
    function spot() {
        if (spotColor) return spotColor;
        var s = doc.spots.add(); s.name = 'StrokeMaster test spot'; s.colorType = ColorModel.SPOT;
        s.color = cmyk(0, 90, 85, 10);
        var c = new SpotColor(); c.spot = s; c.tint = 100; spotColor = c; return c;
    }
    function make(kind) {
        var p = new DocumentPreset();
        p.colorMode = kind === 'cmyk' || kind === 'spot' ? DocumentColorSpace.CMYK : DocumentColorSpace.RGB;
        p.width = 480; p.height = 360; p.units = RulerUnits.Points;
        doc = app.documents.addDocument(kind === 'cmyk' || kind === 'spot' ? 'Print' : 'Web', p);
        doc.artboards[0].artboardRect = [0, 360, 480, 0];
        spotColor = null;
        var wide = args.font === 'GreatVibes-Regular';
        // Great Vibes's first line needs more than 48 pt of frame height, and its narrow words
        // let a taller frame hold the whole text, leaving the second empty (2026-10-30): a taller
        // frame and twice the text, so the story flows into both.
        var t = doc.textFrames.areaText(doc.pathItems.rectangle(310, 35, wide ? 260 : 185, wide ? 70 : 48));
        var second = doc.textFrames.areaText(doc.pathItems.rectangle(285, 45, 400, 180));
        var single = kind === 'separate' || kind === 'separate-off';
        if (single) { t.contents = 'AVMWxy'; second.contents = 'AVMWxy AVMWxy AVMWxy'; }
        else { t.nextFrame = second; t.contents = wide ? 'AVMWxy AVMWxy AVMWxy AVMWxy AVMWxy AVMWxy AVMWxy AVMWxy' : 'AVMWxy AVMWxy AVMWxy AVMWxy'; }
        var frames = single ? [t, second] : [t];
        for (var q = 0; q < frames.length; q++) {
            var a = frames[q].story.textRange.characterAttributes;
            a.textFont = app.textFonts.getByName(args.font); a.size = 38; a.leading = 28; a.autoLeading = false;
            a.strokeColor = new NoColor();
        }
        return t;
    }
    function fills(t, kind) {
        var chars = t.story.textRange.characters, base;
        if (kind === 'rgb' || kind === 'perframe' || kind === 'uniform' || kind === 'separate' || kind === 'separate-off')
            base = rgb(200, 30, 30);
        else if (kind === 'cmyk') base = cmyk(0, 90, 85, 10);
        else if (kind === 'spot') base = spot();
        else base = gray(100);
        for (var q = 0; q < doc.textFrames.length; q++) doc.textFrames[q].story.textRange.characterAttributes.fillColor = base;
        if (kind === 'mixfill' || kind === 'mixboth')
            for (var j = 7; j < 13; j++) chars[j].characterAttributes.fillColor = rgb(30, 60, 200);
    }
    function strokes(t, kind) {
        var chars = t.story.textRange.characters;
        if (kind === 'mixfill' || kind === 'uniform' || kind === 'separate' || kind === 'separate-off') {
            for (var q = 0; q < doc.textFrames.length; q++) {
                var all = doc.textFrames[q].story.textRange.characterAttributes;
                all.strokeColor = gray(40); all.strokeWeight = 7;
            }
            return;
        }
        var cm = kind === 'cmyk' || kind === 'spot';
        for (var j = 0; j < chars.length; j++) {
            if (/[ \r\n]/.test(chars[j].contents) || j % 3 === 0) continue;
            chars[j].characterAttributes.strokeColor = cm ? cmyk(0, 0, 0, j % 2 ? 40 : 70) : gray(j % 2 ? 40 : 70);
            chars[j].characterAttributes.strokeWeight = 7;
        }
    }
    // The first word's fill recolored green, as a user would.
    function recolor() {
        var cm = doc.documentColorSpace === DocumentColorSpace.CMYK;
        var chars = doc.textFrames[0].story.textRange.characters;
        for (var r = 0; r < 6; r++) chars[r].characterAttributes.fillColor = cm ? cmyk(80, 0, 90, 0) : rgb(20, 160, 40);
    }
    function selectFrames() {
        doc.selection = null;
        for (var q = 0; q < doc.textFrames.length; q++) doc.textFrames[q].selected = true;
    }
    function frameStats() {
        var out = [];
        for (var i = 0; i < doc.textFrames.length; i++)
            out.push([doc.textFrames[i].story.textFrames.length, doc.textFrames[i].textRange.characters.length]);
        return out;
    }
    try {
        var status = send('status');
        if (status.indexOf('"build":"' + (args.expectedBuild || 'region-prep-22') + '"') < 0) throw Error('unexpected plugin build');
        var cases = args.cases || ['rgb', 'cmyk', 'spot', 'mixfill', 'uniform', 'mixboth', 'perframe'];
        for (var k = 0; k < cases.length; k++) {
            var kind = cases[k], row = {kind: kind};
            rows.push(row);
            at = kind + '.create';
            var t = make(kind);
            fills(t, kind); app.redraw();
            lib.pdf(doc, args, kind + '-fill');
            recolor(); app.redraw();
            lib.pdf(doc, args, kind + '-fill-recolor');
            fills(t, kind); app.redraw();
            strokes(t, kind); app.redraw();
            lib.pdf(doc, args, kind + '-native');
            row.before = lib.snapshot(doc);
            at = kind + '.route';
            selectFrames();
            if (kind === 'perframe') row.route = send('native-route');
            else if (args.viaCorrect) row.route = send('correct');
            else {
                $.global.ISE_REGION_ARGS.separate = kind === 'separate';
                try { row.route = String($.evalFile(new File(here + '/strokemaster-fix-story.jsx'))); }
                finally { delete $.global.ISE_REGION_ARGS.separate; }
            }
            doc.selection = null; app.redraw();
            row.after = lib.snapshot(doc);
            row.frames = frameStats();
            row.groups = doc.groupItems.length;
            lib.pdf(doc, args, kind + '-route');
            // native-fix on everything routed (with args.fix): character limits, and the story
            // group's stroke limit from every glyph of the story.
            if (args.fix) {
                at = kind + '.fix';
                doc.selection = null;
                for (var g = 0; g < doc.groupItems.length; g++) doc.groupItems[g].selected = true;
                for (var q2 = 0; q2 < doc.textFrames.length; q2++) doc.textFrames[q2].selected = true;
                row.fix = /"nativeFix":\{[^}]*\}/.exec(send('native-fix'))[0];
                doc.selection = null; app.redraw();
                lib.pdf(doc, args, kind + '-fixed');
            }
            // The user recolors the first word's fill: is the change drawn?
            at = kind + '.recolor';
            recolor(); app.redraw();
            lib.pdf(doc, args, kind + '-recolor');
            at = kind + '.save';
            var ai = lib.path(args, kind + '.ai');
            if (ai.exists) throw Error('refusing existing ai');
            var so = new IllustratorSaveOptions(); so.pdfCompatible = true;
            doc.saveAs(ai, so);
            doc.close(SaveOptions.DONOTSAVECHANGES); doc = null;
        }
        lib.write(args, 'results.json', {schema: 1, host: app.version, font: args.font,
            build: args.expectedBuild || 'region-prep-22', rows: rows});
        return 'DONE|font=' + args.font + '|cases=' + rows.length;
    } catch (e) {
        if (doc) try { doc.close(SaveOptions.DONOTSAVECHANGES); } catch (ignored) {}
        rows.push({error: String(e), stage: at});
        try { lib.write(args, 'results.json', {schema: 1, host: app.version, font: args.font, rows: rows}); } catch (w) {}
        return 'FAIL|stage=' + at + '|error=' + String(e);
    } finally { app.userInteractionLevel = interaction; }
})();
