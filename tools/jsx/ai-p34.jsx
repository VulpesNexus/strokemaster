// The owner's prep-34 retests S5, S7, and S9 as they ran them (2026-10-31), one phase per run in
// one Illustrator (the phase is the number of p34-phase<n>.json files already written):
//   1  a blank document; the switches open_documents, outside_dashes, follow_edits on
//   2  the copy args.file opened while that document is active (S5)
//   3  read what opening corrected; close_holes on; the dashed square (the one path with dashes)
//      and the first stroked text frame moved by 5 pt (S7, S9)
//   4  read both; the square set Inside, as the Stroke panel does (S7)
//   5  read the square
// ISE_REGION_ARGS: outDir, runId, file.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    var phase = 1;
    while (lib.path(args, 'phase' + phase + '.json').exists) phase++;
    var interaction = app.userInteractionLevel, out = {phase: phase};
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    function opened() {
        for (var i = 0; i < app.documents.length; i++) if (app.documents[i].fullName && String(app.documents[i].fullName.fsName).indexOf(new File(args.file).name) >= 0) return app.documents[i];
        return null;
    }
    function square(doc) {
        for (var i = 0; i < doc.pathItems.length; i++) if (doc.pathItems[i].strokeDashes.length > 0) return doc.pathItems[i];
        return null;
    }
    function stroked(doc) {
        for (var i = 0; i < doc.textFrames.length; i++) {
            var t = doc.textFrames[i];
            if (t.parent.typename === 'Layer' && t.textRange.characterAttributes.strokeColor.typename !== 'NoColor') return t;
        }
        return null;
    }
    function dump(doc, item) { doc.selection = null; item.selected = true; var d = send('style-dump'); doc.selection = null; return d; }
    try {
        out.refix = /"refix":\{[^}]*\}/.exec(send('status'))[0];
        if (phase === 1) {
            out.build = /"build":"[^"]*"/.exec(send('status'))[0];
            send('settings', 'defaults');
            // One switch per call: the bridge takes 16 characters of input.
            send('settings', 'open_documents=1');
            out.settings = send('settings', 'outside_dashes=1');
            var p = new DocumentPreset(); p.width = 400; p.height = 300; p.units = RulerUnits.Points;
            app.documents.addDocument('Print', p);
        } else if (phase === 2) {
            app.open(new File(args.file));
        } else {
            var doc = opened();
            if (!doc) throw Error('the opened copy is gone');
            app.activeDocument = doc;
            var sq = square(doc), t = stroked(doc);
            out.square = sq ? dump(doc, sq) : 'none';
            out.squareWidth = sq ? sq.strokeWidth : -1;
            out.text = t ? dump(doc, t) : 'none';
            out.textContents = t ? t.contents.substr(0, 30) : '';
            if (phase === 3) {
                out.holes = send('settings', 'close_holes=1');
                if (sq) { sq.translate(5, 0); }
                if (t) { t.translate(5, 0); }
                doc.selection = null;
            } else if (phase === 4 && sq) {
                doc.selection = null; sq.selected = true;
                out.inside = /"align":\{[^}]*\}/.exec(send('stroke-align', 'inside'))[0];
                doc.selection = null;
            }
        }
        lib.write(args, 'phase' + phase + '.json', out);
        return 'DONE|phase' + phase;
    } catch (e) { return 'FAIL|' + String(e) + ' line ' + e.line; }
    finally { app.userInteractionLevel = interaction; }
})();
