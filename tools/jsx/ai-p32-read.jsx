// region-prep-32 retests, phase 3 of 4 (ai-p32-start.jsx): what automatic correction made of the
// edits, then the text's Appearance stroke given a limit of 9 by hand (the owner's Q4).
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
        out.square = lib2.read(doc, 'dash-sq');
        out.squareWidth = doc.pageItems.getByName('dash-sq').strokeWidth;
        var fa = doc.pageItems.getByName('thread-a'), fb = doc.pageItems.getByName('thread-b');
        out.threadParents = [fa.parent.typename, fb.parent.typename, fa.parent === fb.parent];
        doc.selection = null; fa.selected = true;
        out.threadGroup = send('style-dump', 'parent');
        doc.selection = null; doc.pageItems.getByName('obj-text').selected = true;
        out.fieldLimit = send('field-limit', '9');
        doc.selection = null;
        lib.write(args, 'phase3.json', out);
        return 'DONE|phase3';
    } catch (e) { return 'FAIL|' + String(e) + ' line ' + e.line; }
    finally { app.userInteractionLevel = interaction; }
})();
