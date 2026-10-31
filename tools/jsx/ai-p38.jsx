// The owner's prep-36 retest U1 (2026-10-31): with their switches (every one on but Strokes added
// through the Appearance panel), their Tests.ai opened, then that switch turned on: the threaded
// frames were grouped only once one was moved. One phase per run in one Illustrator (the phase is
// the number of <runId>-phase<n>.json files already written), the timer running between:
//   1  the owner's switches; args.file opened (corrected as it opens).
//   2  read: the threaded frames. Strokes added through the Appearance panel on.
//   3  read them again, with Correct Selection's last answer.
// ISE_REGION_ARGS: outDir, runId, file.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function refix() { var m = /"refix":\{[^}]*\}/.exec(send('status')); return m ? m[0] : ''; }
    function dumpOf(doc, item, parent) { doc.selection = null; item.selected = true; var d = send('style-dump', parent ? 'parent' : ''); doc.selection = null; return d; }
    function read(doc, o) {
        o.frames = [];
        for (var i = 0; i < doc.textFrames.length; i++) {
            var f = doc.textFrames[i];
            o.frames.push({story: f.story.textFrames.length, parent: f.parent.typename, dump: dumpOf(doc, f),
                           group: f.parent.typename === 'GroupItem' ? dumpOf(doc, f, true) : ''});
        }
        o.top = [];
        for (var k = 0; k < doc.layers[0].pageItems.length; k++) o.top.push(doc.layers[0].pageItems[k].typename);
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
            out.set = [send('settings', 'text_stories=1'), send('settings', 'outside_dashes=1'), send('settings', 'close_holes=1'),
                       send('settings', 'open_documents=1')];
            doc = app.open(new File(args.file));
        } else if (phase === 2) {
            read(doc, out);
            out.set = send('settings', 'object_strokes=1');
        } else {
            read(doc, out);
        }
        out.correct = /"correct":\{[^}]*\}/.test(send('status')) ? /"correct":\{[^}]*\}/.exec(send('status'))[0] : '';
        out.refix = refix();
        lib.write(args, 'phase' + phase + '.json', out);
        return 'DONE|phase' + phase;
    } catch (e) { return 'FAIL|' + String(e) + ' line ' + e.line; }
    finally { app.userInteractionLevel = interaction; }
})();
