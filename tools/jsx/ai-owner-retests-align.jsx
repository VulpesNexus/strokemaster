// The owner's retests of region-prep-29, phase 2 of 3: the corrected dashed square set Outside
// as the Stroke panel sets it (stroke-align), after the automatic re-fix has seen it corrected,
// so its next scan takes the change for an edit, as a hand's would be.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (app.documents.length !== 1) return 'FAIL|expected the phase 1 document';
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        var out = {}, doc = app.activeDocument, sq = null;
        for (var i = 0; i < doc.pathItems.length && !sq; i++)
            if (doc.pathItems[i].strokeDashes.length && doc.pathItems[i].filled) sq = doc.pathItems[i];
        if (!sq) throw Error('no square');
        out.refixBeforeAlign = /"refix":\{[^}]*\}/.exec(send('status'))[0];
        doc.selection = null; sq.selected = true;
        out.squareOutside = /"appearance":\{[^}]*\}/.exec(send('stroke-align', 'outside'))[0];
        doc.selection = null;
        lib.write(args, 'phase2.json', out);
        return 'DONE|phase2';
    } catch (e) { return 'FAIL|' + String(e); }
    finally { app.userInteractionLevel = interaction; }
})();
