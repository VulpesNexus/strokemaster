// The owner's prep-36 retests U5 and the side note on Across other text objects (2026-10-31), one
// phase per run in one Illustrator (the phase is the number of <runId>-phase<n>.json files already
// written), the timer running between:
//   1  the owner's switches (Dashed outside strokes, Across other text objects, Also documents as
//      they open on); args.file opened (their hand tests): corrected as it opens.
//   2  read: the dashed square and section 4's two stories. The square set Inside as the Stroke
//      panel sets it (panel-align, at the next timer message).
//   3  read the square. Section 4's stories selected and corrected (Correct Selection).
//   4  read both again. The square set Outside as the Stroke panel sets it.
//   5  read.
// ISE_REGION_ARGS: outDir, runId, file.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function refix() { var m = /"refix":\{[^}]*\}/.exec(send('status')); return m ? m[0] : ''; }
    function dumpOf(doc, item, parent) { doc.selection = null; item.selected = true; var d = send('style-dump', parent ? 'parent' : ''); doc.selection = null; return d; }
    function squares(doc) {
        var out = [];
        for (var k = 0; k < doc.pathItems.length; k++) { var p = doc.pathItems[k]; if (p.stroked && p.strokeDashes.length) out.push(p); }
        return out;
    }
    function stories(doc) {
        var out = [];
        for (var k = 0; k < doc.textFrames.length; k++) if (/^AVMW/.test(doc.textFrames[k].contents)) out.push(doc.textFrames[k]);
        return out;
    }
    function read(doc, o) {
        o.squares = [];
        var sq = squares(doc);
        for (var i = 0; i < sq.length; i++) o.squares.push({w: sq[i].strokeWidth, dump: dumpOf(doc, sq[i])});
        o.stories = [];
        var st = stories(doc);
        for (var j = 0; j < st.length; j++)
            o.stories.push({text: st[j].contents.substr(0, 12), parent: st[j].parent.typename, top: Math.round(st[j].top),
                            left: Math.round(st[j].left), dump: dumpOf(doc, st[j]), group: st[j].parent.typename === 'GroupItem' ? dumpOf(doc, st[j], true) : ''});
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
            out.set = [send('settings', 'outside_dashes=1'), send('settings', 'text_stories=1'), send('settings', 'open_documents=1')];
            doc = app.open(new File(args.file));
        } else if (phase === 2) {
            read(doc, out);
            var sq = squares(doc);
            doc.selection = null;
            if (sq.length) { sq[0].selected = true; out.align = send('panel-align', 'inside'); }
        } else if (phase === 3) {
            read(doc, out);
            var st = stories(doc);
            doc.selection = null;
            for (var j = 0; j < st.length; j++) st[j].selected = true;
            out.correct = send('correct');
            doc.selection = null;
        } else if (phase === 4) {
            read(doc, out);
            var sq4 = squares(doc);
            doc.selection = null;
            if (sq4.length) { sq4[0].selected = true; out.align = send('panel-align', 'outside'); }
        } else {
            read(doc, out);
        }
        out.refix = refix();
        lib.write(args, 'phase' + phase + '.json', out);
        return 'DONE|phase' + phase;
    } catch (e) { return 'FAIL|' + String(e) + ' line ' + e.line; }
    finally { app.userInteractionLevel = interaction; }
})();
