// region-prep-32 retests, phase 4 of 4 (ai-p32-start.jsx): the text whose Appearance stroke was
// given a limit by hand, after the re-fix saw the edit: released, its limit kept.
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
        out.refix = /"refix":\{[^}]*\}/.exec(send('status'))[0];
        out.objText = lib2.read(doc, 'obj-text');
        lib.write(args, 'phase4.json', out);
        return 'DONE|phase4';
    } catch (e) { return 'FAIL|' + String(e) + ' line ' + e.line; }
    finally { app.userInteractionLevel = interaction; }
})();
