// The owner's prep-37 retests V1 and V4 (2026-10-31), one phase per run in one Illustrator (the
// phase is the number of <runId>-phase<n>.json files already written), the timer running between:
//   1  the owner's switches (their status log: every opt-in on); args.file opened (their hand
//      tests): corrected as it opens.
//   2  read: section 4's two stories and the dashed square. Across other text objects off.
//   3  read (V1: the stories taken apart). On again.
//   4  read (grouped again). The square selected, then the panel menu's Align Selection Inside
//      (V4: Illustrator grays the Stroke panel's Inside for the doubled, corner-aligned dashes).
//   5  read the square; Align Selection Outside (W2: its whole-point dashes back).
//   6  read the square.
// ISE_REGION_ARGS: outDir, runId, file.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function refix() { var m = /"refix":\{[^}]*\}/.exec(send('status')); return m ? m[0] : ''; }
    function dumpOf(doc, item) { doc.selection = null; item.selected = true; var d = send('style-dump', ''); doc.selection = null; return d; }
    function squares(doc) {
        var out = [];
        for (var k = 0; k < doc.pathItems.length; k++) { var p = doc.pathItems[k]; if (p.stroked && p.strokeDashes.length) out.push(p); }
        return out;
    }
    function read(doc, o) {
        var sq = squares(doc);
        o.squares = [];
        for (var i = 0; i < sq.length; i++) o.squares.push({w: sq[i].strokeWidth, dump: dumpOf(doc, sq[i])});
        o.stories = [];
        for (var k = 0; k < doc.textFrames.length; k++) {
            var f = doc.textFrames[k];
            if (!/^AVMW/.test(f.contents)) continue;
            o.stories.push(f.parent.typename === 'GroupItem' ? 'G' + f.parent.textFrames.length : 'L');
            o.records = (o.records || []).concat([Math.round(f.left) + ',' + Math.round(f.top) + ' ' + (/RECORD[^;]*/.exec(dumpOf(doc, f)) || [''])[0]]);
        }
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
            out.set = [send('settings', 'text_stories=1'), send('settings', 'object_strokes=1'), send('settings', 'outside_dashes=1'),
                       send('settings', 'close_holes=1'), send('settings', 'open_documents=1')];
            doc = app.open(new File(args.file));
        } else if (phase === 2) {
            read(doc, out);
            out.set = send('settings', 'text_stories=0');
        } else if (phase === 3) {
            read(doc, out);
            out.set = send('settings', 'text_stories=1');
        } else if (phase === 4) {
            read(doc, out);
            var sq = squares(doc);
            doc.selection = null;
            if (sq.length) { sq[0].selected = true; out.align = send('ui-press', 'align-inside'); }
            // Section 4's two stories (the topmost pair) by hand, for Correct Selection's own report.
            var pair = [];
            for (var k = 0; k < doc.textFrames.length; k++) if (/^AVMW/.test(doc.textFrames[k].contents) && doc.textFrames[k].top > 440 && doc.textFrames[k].top < 500) pair.push(doc.textFrames[k]);
            if (args.probe && pair.length === 2 && pair[0].parent.typename !== 'GroupItem') {
                doc.selection = null;
                pair[0].selected = true; pair[1].selected = true;
                out.correct = send('correct');
                out.between = [];
                var sibs = pair[0].parent.pageItems;
                for (var z = 0; z < sibs.length; z++) out.between.push(sibs[z].typename + (sibs[z] === pair[0] || sibs[z] === pair[1] ? '*' : '') + (sibs[z].hidden ? 'h' : ''));
                doc.selection = null;
            }
        } else if (phase === 5) {
            read(doc, out);
            var sq5 = squares(doc);
            doc.selection = null;
            if (sq5.length) { sq5[0].selected = true; out.align = send('ui-press', 'align-outside'); }
        } else {
            read(doc, out);
        }
        out.refix = refix();
        lib.write(args, 'phase' + phase + '.json', out);
        return 'DONE|phase' + phase;
    } catch (e) { return 'FAIL|' + String(e) + ' line ' + e.line; }
    finally { app.userInteractionLevel = interaction; }
})();
