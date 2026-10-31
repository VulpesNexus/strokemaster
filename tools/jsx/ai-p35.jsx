// The owner's prep-34 retests S3, S4, and S6 (2026-10-31), one phase per run in one Illustrator
// (the phase is the number of <runId>-phase<n>.json files already written):
//   1  the copy args.file opened. S4: its threaded frames (a story of two or more) corrected with
//      Correct Selection. S6: new text "Lorem ipsum" in Kerberos Fang, red stroke 6 pt, its
//      characters' limit 1.825 as inherited from corrected text, corrected with Trim glyph tips
//      off, then on.
//   2  S3: that text's characters given a limit of 9, as the Stroke panel sets it
//   3  read it
// ISE_REGION_ARGS: outDir, runId, file.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function rgb(r, g, b) { var c = new RGBColor(); c.red = r; c.green = g; c.blue = b; return c; }
    function limitOf(d) { var m = /STROKE w [0-9.]+ join 0 limit ([0-9.]+)[^;]*EFFECT 'Adobe Outline Type'/.exec(d); return m ? Number(m[1]) : null; }
    var phase = 1;
    while (lib.path(args, 'phase' + phase + '.json').exists) phase++;
    var interaction = app.userInteractionLevel, out = {phase: phase};
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        if (phase === 1) {
            out.build = /"build":"[^"]*"/.exec(send('status'))[0];
            send('settings', 'defaults');
            var doc = app.open(new File(args.file));
            var story = null;
            for (var i = 0; i < doc.textFrames.length && !story; i++) if (doc.textFrames[i].story.textFrames.length > 1) story = doc.textFrames[i].story;
            if (story) {
                doc.selection = null;
                for (var k = 0; k < story.textFrames.length; k++) story.textFrames[k].selected = true;
                out.correct = send('correct');
                var f0 = story.textFrames[0];
                out.storyParent = f0.parent.typename;
                doc.selection = null; f0.selected = true;
                out.groupDump = send('style-dump', 'parent');
                out.frameDump = send('style-dump');
                doc.selection = null;
            } else out.story = 'none';
            var t = doc.textFrames.add(); t.contents = 'Lorem ipsum'; t.position = [40, -40]; t.name = 's6';
            var a = t.textRange.characterAttributes;
            a.textFont = app.textFonts.getByName('KerberosFang'); a.size = 60; a.fillColor = rgb(0, 0, 0); a.strokeColor = rgb(190, 40, 20); a.strokeWeight = 6;
            doc.selection = null; t.selected = true;
            send('appearance', 'miter:1.825');
            send('settings', 'glyph_tips=0');
            doc.selection = null; t.selected = true; send('correct');
            doc.selection = null; t.selected = true; out.off = limitOf(send('style-dump'));
            send('settings', 'glyph_tips=1');
            doc.selection = null; t.selected = true; send('correct');
            doc.selection = null; t.selected = true; out.on = limitOf(send('style-dump'));
            doc.selection = null;
        } else {
            var d2 = app.activeDocument, s6 = d2.pageItems.getByName('s6');
            if (phase === 2) {
                d2.selection = null; s6.selected = true;
                out.set = send('appearance', 'miter:9');
                d2.selection = null;
            } else {
                d2.selection = null; s6.selected = true;
                out.dump = send('style-dump');
                out.limit = limitOf(out.dump);
                out.charLimits = send('char-limits');
                // S4 again, a phase later: each frame of the threaded story.
                out.frames = [];
                for (var q = 0; q < d2.textFrames.length; q++) if (d2.textFrames[q].story.textFrames.length > 1) { d2.selection = null; d2.textFrames[q].selected = true; out.frames.push(send('style-dump')); }
                d2.selection = null;
            }
        }
        out.refix = /"refix":\{[^}]*\}/.exec(send('status'))[0];
        lib.write(args, 'phase' + phase + '.json', out);
        return 'DONE|phase' + phase;
    } catch (e) { return 'FAIL|' + String(e) + ' line ' + e.line; }
    finally { app.userInteractionLevel = interaction; }
})();
