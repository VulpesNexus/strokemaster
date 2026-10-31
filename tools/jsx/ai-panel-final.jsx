// Panel test, phase 2 of 2: after idle time, the timer has run Correct Selection; exports
// <runId>-route.pdf, records the document's state, undoes once (the correction is one undo
// step) and records again, then removes the settings file if phase 1 found none.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (app.documents.length !== 1) return 'FAIL|expected the phase 1 document';
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        var doc = app.activeDocument, out = lib.read(args, 'phase1.json');
        out.groups = doc.groupItems.length;
        out.framesInGroup = doc.groupItems.length ? doc.groupItems[0].textFrames.length : 0;
        out.status = send('ui-status');
        lib.pdf(doc, args, 'route');
        app.undo();
        app.redraw();
        out.groupsAfterUndo = doc.groupItems.length;
        doc.close(SaveOptions.DONOTSAVECHANGES);
        var settingsFile = new File(args.settingsPath || (Folder.userData.fsName + '/StrokeMaster/settings-illustrator.txt'));
        if (!out.settingsExisted && settingsFile.exists) out.settingsRemoved = settingsFile.remove();
        var outFile = lib.path(args, 'results.json');
        lib.write(args, 'results.json', out);
        return 'DONE|phase2';
    } catch (e) { return 'FAIL|' + String(e); }
    finally { app.userInteractionLevel = interaction; }
})();
