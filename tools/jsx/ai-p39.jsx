// Region-prep-37 (2026-10-31): Across other text objects turned off, then on, regroups the stories
// it had grouped (Reapply's overlapping stories), and No miter spikes in shapes and outlines. One
// phase per run in one Illustrator (the phase is the number of <runId>-phase<n>.json files already
// written), the timer running between:
//   1  Across other text objects on; args.file opened (the owner's hand tests), first seen.
//   2  read: the story groups. Across other text objects off.
//   3  read. On again.
//   4  read. Outlined text (KerberosFang, MWAK), 10 pt Center miter strokes at limit 10 (corrected automatically).
//   5  read its limit. No miter spikes in shapes and outlines off.
//   6  read its limit; the panel's status.
// ISE_REGION_ARGS: outDir, runId, file.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function refix() { var m = /"refix":\{[^}]*\}/.exec(send('status')); return m ? m[0] : ''; }
    function rgb(r, g, b) { var c = new RGBColor(); c.red = r; c.green = g; c.blue = b; return c; }
    function groups(doc) {
        var out = [];
        for (var i = 0; i < doc.textFrames.length; i++) {
            var f = doc.textFrames[i];
            out.push(f.parent.typename === 'GroupItem' ? 'G' + f.parent.textFrames.length : 'L');
        }
        return out.join(',');
    }
    var phase = 1;
    while (lib.path(args, 'phase' + phase + '.json').exists) phase++;
    var interaction = app.userInteractionLevel, out = {phase: phase};
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        var doc = app.documents.length ? app.activeDocument : null;
        if (phase === 1) {
            out.build = /"build":"[^"]*"/.exec(send('status'))[0];
            send('settings', 'defaults');
            out.set = send('settings', 'text_stories=1');
            doc = app.open(new File(args.file));
        } else if (phase === 2) {
            out.groups = groups(doc);
            out.set = send('settings', 'text_stories=0');
        } else if (phase === 3) {
            out.groups = groups(doc);
            out.set = send('settings', 'text_stories=1');
        } else if (phase === 4) {
            out.groups = groups(doc);
            // Outlined text, whose inner corners poke miter spikes through thin parts.
            var t = doc.textFrames.add(); t.contents = 'MWAK'; t.position = [40, -40];
            t.textRange.characterAttributes.textFont = app.textFonts.getByName('KerberosFang');
            t.textRange.characterAttributes.size = 120;
            var g = t.createOutline();
            var star = g.compoundPathItems.length ? g.compoundPathItems[0].pathItems[0] : g.pathItems[0];
            for (var q = 0; q < g.pageItems.length; q++) {
                var it = g.pageItems[q], ps = it.typename === 'CompoundPathItem' ? it.pathItems : [it];
                for (var r = 0; r < ps.length; r++) {
                    ps[r].stroked = true; ps[r].strokeColor = rgb(20, 20, 20); ps[r].strokeWidth = 10;
                    ps[r].strokeJoin = StrokeJoin.MITERENDJOIN; ps[r].strokeMiterLimit = 10;
                }
            }
            star.name = 'star';
            g.name = 'mwak';
            doc.selection = null;
        } else if (phase === 5) {
            out.limit = (function () { var g = doc.groupItems.getByName('mwak'), a = []; for (var i = 0; i < g.pathItems.length; i++) a.push(Math.round(g.pathItems[i].strokeMiterLimit * 100) / 100); for (var j = 0; j < g.compoundPathItems.length; j++) a.push(Math.round(g.compoundPathItems[j].pathItems[0].strokeMiterLimit * 100) / 100); return a.join(' '); })();
            out.set = send('settings', 'shape_miter=0');
        } else {
            out.limit = (function () { var g = doc.groupItems.getByName('mwak'), a = []; for (var i = 0; i < g.pathItems.length; i++) a.push(Math.round(g.pathItems[i].strokeMiterLimit * 100) / 100); for (var j = 0; j < g.compoundPathItems.length; j++) a.push(Math.round(g.compoundPathItems[j].pathItems[0].strokeMiterLimit * 100) / 100); return a.join(' '); })();
            out.ui = send('ui-status');
        }
        out.refix = refix();
        lib.write(args, 'phase' + phase + '.json', out);
        return 'DONE|phase' + phase;
    } catch (e) { return 'FAIL|' + String(e) + ' line ' + e.line; }
    finally { app.userInteractionLevel = interaction; }
})();
