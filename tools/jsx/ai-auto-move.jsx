// Automatic correction, phase 4 of 5: after the re-fix has seen "new-path"'s limit set by hand
// (released), "new-path" and "old-path" are moved: an edit of each. "old-path" is corrected
// then; "new-path", taken back by the user, is not.
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
        out.released = lib2.read(doc, 'new-path');
        doc.pageItems.getByName('new-path').translate(0, -20);
        doc.pageItems.getByName('old-path').translate(0, -20);
        doc.selection = null;
        lib.write(args, 'phase4.json', out);
        return 'DONE|phase4';
    } catch (e) { return 'FAIL|' + String(e); }
    finally { app.userInteractionLevel = interaction; }
})();
