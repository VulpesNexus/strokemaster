// Automatic correction, phase 2 of 5, after the plugin's first scan: new stroked text
// "new-text", a new stroked triangle "new-path", a new unstroked triangle "bare-path", an edit of
// "old-text" (its contents), and "old-path" only selected, not edited. The whole phase is one
// undo step, as a hand's edit would be.
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
        out.refixBefore = /"refix":\{[^}]*\}/.exec(send('status'))[0];
        lib2.text(doc, 'new-text', 30, 380);
        lib2.triangle(doc, 'new-path', 190);
        var bare = lib2.triangle(doc, 'bare-path', 320); bare.stroked = false;
        doc.pageItems.getByName('old-text').contents = 'AVMWxy AVMWxyz';
        doc.selection = null;
        doc.pageItems.getByName('old-path').selected = true;
        lib.write(args, 'phase2.json', out);
        return 'DONE|phase2';
    } catch (e) { return 'FAIL|' + String(e); }
    finally { app.userInteractionLevel = interaction; }
})();
