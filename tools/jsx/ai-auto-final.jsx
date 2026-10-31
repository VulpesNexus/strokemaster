// Automatic correction, phase 5 of 5: each item read back after the moves, then the results of
// all five phases in <runId>-results.json.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (app.documents.length !== 1) return 'FAIL|expected the phase 1 document';
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    var lib2 = $.evalFile(new File(here + '/ai-auto-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    var interaction = app.userInteractionLevel, out = {};
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        var doc = app.activeDocument;
        out.phase1 = lib.read(args, 'phase1.json');
        out.phase2 = lib.read(args, 'phase2.json');
        out.phase3 = lib.read(args, 'phase3.json');
        out.phase4 = lib.read(args, 'phase4.json');
        out.refix = /"refix":\{[^}]*\}/.exec(send('status'))[0];
        out.afterIdle = {};
        var names = ['old-text', 'old-path', 'locked-path', 'new-text', 'new-path', 'bare-path'];
        for (var i = 0; i < names.length; i++) out.afterIdle[names[i]] = lib2.read(doc, names[i]);
        doc.close(SaveOptions.DONOTSAVECHANGES);
        lib.write(args, 'results.json', out);
        return 'DONE|phase5';
    } catch (e) { return 'FAIL|' + String(e); }
    finally { app.userInteractionLevel = interaction; }
})();
