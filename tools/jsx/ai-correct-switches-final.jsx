// Correct Selection against the switches, phase 2 of 2: after idle time, where the posted Tab
// took the panel's focus (ui-status: focus, tabs, and the tooltip tools), then the document is
// closed and <runId>-results.json written.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (app.documents.length !== 1) return 'FAIL|expected the phase 1 document';
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        var out = lib.read(args, 'phase1.json');
        out.uiStatusAfterTab = send('ui-status');
        app.activeDocument.close(SaveOptions.DONOTSAVECHANGES);
        lib.write(args, 'results.json', out);
        return 'DONE|phase2';
    } catch (e) { return 'FAIL|' + String(e); }
    finally { app.userInteractionLevel = interaction; }
})();
