// The owner's retests of region-prep-29, phase 3 of 3: after idle time (the automatic re-fix
// has run on the square set Outside), the square read back and exported, then Correct Selection
// on it with Dashed Outside strokes on, read back and exported again. Writes <runId>-results.json.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (app.documents.length !== 1) return 'FAIL|expected the phase 1 document';
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    var interaction = app.userInteractionLevel, at = 'phase3';
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    var out = {};
    try {
        out = lib.read(args, 'phase1.json');
        var two = lib.read(args, 'phase2.json');
        for (var k in two) out[k] = two[k];
        var doc = app.activeDocument, sq = null;
        for (var i = 0; i < doc.pathItems.length && !sq; i++)
            if (doc.pathItems[i].strokeDashes.length && doc.pathItems[i].filled) sq = doc.pathItems[i];
        if (!sq) throw Error('no square');
        function read(name) {
            doc.selection = null; sq.selected = true;
            out[name] = {dump: send('style-dump'), status: /"nativeFix":\{[^}]*\}/.exec(send('status'))[0],
                         refix: /"refix":\{[^}]*\}/.exec(send('status'))[0],
                         width: sq.strokeWidth};
            doc.selection = null; app.redraw(); lib.pdf(doc, args, name);
        }
        at = 'square-outside';
        read('square-outside');
        at = 'square-corrected';
        send('settings', 'outside_dashes=1');
        doc.selection = null; sq.selected = true;
        out.squareCorrect = send('correct');
        read('square-corrected');
        send('settings', 'defaults');
        doc.close(SaveOptions.DONOTSAVECHANGES);
        lib.write(args, 'results.json', out);
        return 'DONE|phase3';
    } catch (e) {
        out.error2 = String(e); out.stage2 = at;
        try { lib.write(args, 'results.json', out); } catch (w) {}
        return 'FAIL|' + at + '|' + String(e);
    } finally { app.userInteractionLevel = interaction; }
})();
