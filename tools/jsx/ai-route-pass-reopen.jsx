// Route pass test, later phases: <runId>-pass.ai (saved after the edit by ai-route-pass.jsx)
// opened and exported as <runId>-reopen.pdf; then one more edit, idle time for Illustrator to
// re-evaluate, and <runId>-reopen-edited.pdf. ISE_REGION_ARGS: outDir, runId. Phase "open"
// opens and exports; phase "edit" (the next script) edits; phase "final" exports and closes.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        if (!app.documents.length) {
            app.open(lib.path(args, 'pass.ai'));
            return 'DONE|opened';
        }
        var doc = app.activeDocument;
        var reopen = lib.path(args, 'reopen.pdf');
        if (!reopen.exists) {
            app.redraw(); lib.pdf(doc, args, 'reopen');
            var f = null;
            for (var i = 0; i < doc.textFrames.length; i++) if (doc.textFrames[i].kind === TextType.POINTTEXT) f = doc.textFrames[i];
            f.textRange.characters[0].contents = 'W' + f.textRange.characters[0].contents;
            return 'DONE|reopened-and-edited|' + f.contents.replace(/\r/g, '/');
        }
        app.redraw(); lib.pdf(doc, args, 'reopen-edited');
        doc.close(SaveOptions.DONOTSAVECHANGES);
        return 'DONE|final';
    } catch (e) { return 'FAIL|' + String(e); }
    finally { app.userInteractionLevel = interaction; }
})();
