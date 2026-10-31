// Correct Selection against the switches (region-prep-29, the owner's hand tests 3, 4, 7, and 14,
// 2026-10-30), phase 1 of 2. A switch that is off takes its correction off the selection, as
// InDesign draws nothing for it; two one-frame stories take part in Across separate stories.
// One owned RGB document (Minion Pro 38 pt, one red fill, one gray 7 pt stroke):
//   story     a two-frame threaded story, the second frame over the first's text:
//             on (defaults), behind-off (Strokes behind off), behind-on (back on), frames
//             (Across a story's threaded frames off), again (back on)
//   separate  two one-frame stories, one over the other: on (Across separate stories on),
//             separate-off (off again), frames-behind-off (Strokes behind off: each frame's own
//             route taken off), frames-behind-on
//   miter     point text: on, miter-off (No miter spikes off; char-limits read back)
//   dashes    a filled square with a dashed Outside stroke: on (Dashed Outside strokes on),
//             dashes-off; its stroke weight read back (the opt-in doubles it)
// Exports <runId>-<case>-fill.pdf (strokes off), -native.pdf, and one PDF per stage, then
// presses Tab in the panel (ui-press tab) for phase 2. ISE_REGION_ARGS: outDir, runId; optional
// expectedBuild (the plugin's build, checked first).
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
        out.uiStatus0 = send('ui-status');
        out.defaults = send('settings', 'defaults');
        var font = app.textFonts.getByName('MinionPro-Regular');
        var p = new DocumentPreset(); p.colorMode = DocumentColorSpace.RGB; p.width = 520; p.height = 760; p.units = RulerUnits.Points;
        var doc = app.documents.addDocument('Web', p);
        doc.artboards[0].artboardRect = [0, 760, 520, 0];
        function styleText(range) {
            var a = range.characterAttributes;
            a.textFont = font; a.size = 38; a.leading = 28; a.autoLeading = false;
            a.fillColor = rgb(200, 30, 30); a.strokeColor = new NoColor();
        }
        function stroke(range) { var a = range.characterAttributes; a.strokeColor = gray(40); a.strokeWeight = 7; }
        at = 'build';
        var t = doc.textFrames.areaText(doc.pathItems.rectangle(740, 30, 185, 48));
        t.nextFrame = doc.textFrames.areaText(doc.pathItems.rectangle(715, 40, 400, 120));
        t.contents = 'AVMWxy AVMWxy AVMWxy AVMWxy';
        styleText(t.story.textRange);
        var s4a = doc.textFrames.add(); s4a.position = [30, 540]; s4a.contents = 'AVMWxy AVMWxy';
        var s4b = doc.textFrames.add(); s4b.position = [40, 516]; s4b.contents = 'AVMWxy AVMWxy';
        styleText(s4a.textRange); styleText(s4b.textRange);
        var m = doc.textFrames.add(); m.position = [30, 400]; m.contents = 'AVMWxy';
        styleText(m.textRange);
        var sq = doc.pathItems.rectangle(260, 300, 160, 160);
        sq.filled = true; sq.fillColor = rgb(30, 60, 200); sq.stroked = false;
        app.redraw(); lib.pdf(doc, args, 'fill');
        stroke(t.story.textRange); stroke(s4a.textRange); stroke(s4b.textRange); stroke(m.textRange);
        sq.stroked = true; sq.strokeWidth = 8; sq.strokeColor = gray(40); sq.strokeDashes = [12, 6];
        app.redraw(); lib.pdf(doc, args, 'native');
        function select(items) { doc.selection = null; for (var i = 0; i < items.length; i++) items[i].selected = true; }
        function storyFrames() { var a = []; for (var i = 0; i < t.story.textFrames.length; i++) a.push(t.story.textFrames[i]); return a; }
        function stage(name, settings, items) {
            at = name;
            var o = {};
            if (settings) o.settings = send('settings', settings);
            select(items);
            o.correct = send('correct');
            o.groups = doc.groupItems.length;
            o.storyStroke = t.story.textRange.characters[0].characterAttributes.strokeColor.typename;
            doc.selection = null; app.redraw();
            lib.pdf(doc, args, name);
            out.stages[name] = o;
            return o;
        }
        // The story.
        stage('story-on', '', storyFrames());
        stage('story-behind-off', 'text_behind=0', storyFrames());
        stage('story-behind-on', 'text_behind=1', storyFrames());
        stage('story-frames', 'text_story=0', storyFrames());
        stage('story-again', 'text_story=1', storyFrames());
        // Two one-frame stories.
        stage('separate-on', 'text_stories=1', [s4a, s4b]);
        stage('separate-off', 'text_stories=0', [s4a, s4b]);
        // Each frame's own route taken off, then put back.
        stage('frames-behind-off', 'text_behind=0', [s4a, s4b]);
        stage('frames-behind-on', 'text_behind=1', [s4a, s4b]);
        // Miter limits.
        send('settings', 'defaults');
        select([m]); out.limitsBefore = send('char-limits');
        stage('miter-on', '', [m]);
        select([m]); out.limitsOn = send('char-limits'); out.dumpOn = send('style-dump');
        stage('miter-off', 'text_miter=0', [m]);
        select([m]); out.limitsOff = send('char-limits'); out.dumpOff = send('style-dump');
        // Dashed Outside.
        send('settings', 'defaults');
        select([sq]); out.alignSquare = send('native-align', 'outside');
        out.squareBefore = sq.strokeWidth;
        stage('dashes-on', 'outside_dashes=1', [sq]);
        out.squareOn = sq.strokeWidth;
        stage('dashes-off', 'outside_dashes=0', [sq]);
        out.squareOff = sq.strokeWidth;
        send('settings', 'defaults');
        out.snapshot = lib.snapshot(doc);
        // The panel: Tab posted to its first row (KeyHook moves the focus; phase 2 reads it).
        at = 'tab';
        out.tab = send('ui-press', 'tab');
        lib.write(args, 'phase1.json', out);
        return 'DONE|phase1';
    } catch (e) {
        out.error = String(e); out.stage = at;
        try { lib.write(args, 'phase1.json', out); } catch (w) {}
        return 'FAIL|stage=' + at + '|error=' + String(e);
    } finally { app.userInteractionLevel = interaction; }
})();
