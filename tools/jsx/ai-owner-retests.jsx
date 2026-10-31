// The owner's retests of region-prep-29 (2026-10-30), phase 1 of 3. Three findings:
//   tips     R17: Trim glyph tips turned off, then Correct Selection on a frame already corrected
//            with it on (Kerberos Fang 72 pt, 14 pt stroke): its route limit read back after each
//            of on, off, and on again
//   stories  R14: corners wedged with Across separate stories on and not off (Minion Pro 38 pt,
//            7 pt stroke, two one-frame stories): the story group's limit (on) and each frame's
//            (off), and a pair corrected with the switch off only, never grouped
//   dashes   a dashed square corrected Inside, then set Outside as the Stroke panel sets it
//            (stroke-align): phase 2 reads it after the automatic re-fix had its idle time
// Exports <runId>-<stage>.pdf. ISE_REGION_ARGS: outDir, runId; optional expectedBuild.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId) return 'FAIL|missing arguments';
    if (app.documents.length) return 'FAIL|requires zero preexisting documents';
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    function rgb(r, g, b) { var c = new RGBColor(); c.red = r; c.green = g; c.blue = b; return c; }
    var interaction = app.userInteractionLevel, at = 'start', out = {stages: {}};
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        if (args.expectedBuild && send('status').indexOf('"build":"' + args.expectedBuild + '"') < 0) throw Error('unexpected plugin build');
        send('settings', 'defaults');
        var fang = app.textFonts.getByName('KerberosFang'), minion = app.textFonts.getByName('MinionPro-Regular');
        var p = new DocumentPreset(); p.colorMode = DocumentColorSpace.RGB; p.width = 900; p.height = 760; p.units = RulerUnits.Points;
        var doc = app.documents.addDocument('Web', p);
        doc.artboards[0].artboardRect = [0, 760, 900, 0];
        function point(text, x, y, font, size, weight) {
            var t = doc.textFrames.add(); t.contents = text; t.position = [x, y];
            var a = t.textRange.characterAttributes;
            a.textFont = font; a.size = size; a.fillColor = rgb(200, 30, 30); a.strokeColor = gray(40); a.strokeWeight = weight;
            return t;
        }
        function select(items) { doc.selection = null; for (var i = 0; i < items.length; i++) items[i].selected = true; }
        function limitOf(dump) { var m = /STROKE w [0-9.]+ join 0 limit ([0-9.]+)[^;]*EFFECT 'Adobe Outline Type'/.exec(dump); return m ? Number(m[1]) : null; }
        at = 'build';
        var fangFrame = point('Lorem ipsum dolor', 30, 740, fang, 72, 14);
        var a1 = point('AVMWxy AVMWxy', 30, 560, minion, 38, 7), b1 = point('AVMWxy AVMWxy', 40, 536, minion, 38, 7);
        var a2 = point('AVMWxy AVMWxy', 400, 560, minion, 38, 7), b2 = point('AVMWxy AVMWxy', 410, 536, minion, 38, 7);
        var sq = doc.pathItems.rectangle(400, 60, 160, 160);
        sq.filled = true; sq.fillColor = rgb(50, 30, 200); sq.stroked = true; sq.strokeWidth = 16; sq.strokeColor = gray(55);
        sq.strokeDashes = [24, 12];
        app.redraw(); lib.pdf(doc, args, 'native');
        function stage(name, settings, items, dumpWhat) {
            at = name;
            var o = {};
            if (settings) o.settings = send('settings', settings);
            select(items);
            o.correct = send('correct');
            o.nativeFix = /"nativeFix":\{[^}]*\}/.exec(send('status'))[0];
            o.dumps = [];
            for (var i = 0; i < dumpWhat.length; i++) {
                select([dumpWhat[i][0]]);
                var d = send('style-dump', dumpWhat[i][1] || '');
                o.dumps.push({dump: d, routeLimit: limitOf(d), charLimits: send('char-limits')});
            }
            out.stages[name] = o;
            return o;
        }
        // R17: tips on, off, on again on one frame.
        stage('tips-on', '', [fangFrame], [[fangFrame]]);
        stage('tips-off', 'glyph_tips=0', [fangFrame], [[fangFrame]]);
        stage('tips-again', 'glyph_tips=1', [fangFrame], [[fangFrame]]);
        send('settings', 'defaults');
        // R14: the pair on the left grouped (on), then taken apart (off); the pair on the right
        // corrected with the switch off only.
        stage('stories-on', 'text_stories=1', [a1, b1], [[a1, 'parent']]);
        doc.selection = null; app.redraw(); lib.pdf(doc, args, 'stories-on');
        stage('stories-off', 'text_stories=0', [a1, b1], [[a1], [b1]]);
        stage('stories-never', '', [a2, b2], [[a2], [b2]]);
        doc.selection = null; app.redraw(); lib.pdf(doc, args, 'stories-off');
        send('settings', 'defaults');
        // The dashed square: Inside, corrected with Dashed Outside strokes on (the owner's switches),
        // then Outside as the Stroke panel sets it.
        at = 'square';
        select([sq]); out.squareInside = send('stroke-align', 'inside');
        stage('square-inside', 'outside_dashes=1', [sq], [[sq]]);
        doc.selection = null; app.redraw(); lib.pdf(doc, args, 'square-inside');
        doc.selection = null;
        lib.write(args, 'phase1.json', out);
        return 'DONE|phase1';
    } catch (e) {
        out.error = String(e); out.stage = at;
        try { lib.write(args, 'phase1.json', out); } catch (w) {}
        return 'FAIL|stage=' + at + '|error=' + String(e);
    } finally { app.userInteractionLevel = interaction; }
})();
