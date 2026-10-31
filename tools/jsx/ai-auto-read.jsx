// Automatic correction, phase 3 of 5: what the plugin corrected after phase 2's edit (each item
// read back) and exported; then "new-path"'s miter limit set by hand (3), as the user takes it
// back. (Undo is a hand test: a script's app.undo() reverted nothing here, 2026-10-30.)
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (app.documents.length !== 1) return 'FAIL|expected the phase 1 document';
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    var lib2 = $.evalFile(new File(here + '/ai-auto-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    var interaction = app.userInteractionLevel, out = {items: {}};
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        var doc = app.activeDocument;
        out.status = send('status');
        out.refix = /"refix":\{[^}]*\}/.exec(out.status)[0];
        var names = ['old-text', 'old-path', 'locked-path', 'new-text', 'new-path', 'bare-path'];
        for (var i = 0; i < names.length; i++) out.items[names[i]] = lib2.read(doc, names[i]);
        lib.pdf(doc, args, 'corrected');
        doc.pageItems.getByName('new-path').strokeMiterLimit = 3;
        lib.write(args, 'phase3.json', out);
        return 'DONE|phase3';
    } catch (e) { return 'FAIL|' + String(e); }
    finally { app.userInteractionLevel = interaction; }
})();
