// Automatic correction (region-prep-31, the owner's choice of 2026-10-30), phase 1 of 5: a
// document with art made before the plugin's first scan of it, as an opened document's is: a
// stroked point text "old-text", a stroked acute triangle "old-path", and a locked one
// "locked-path". Phase 2 edits, adds, and selects; phase 3 reads and sets a limit by hand;
// phase 4 moves two paths; phase 5 reads again. ISE_REGION_ARGS: outDir, runId; optional
// expectedBuild.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId) return 'FAIL|missing arguments';
    if (app.documents.length) return 'FAIL|requires zero preexisting documents';
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    var interaction = app.userInteractionLevel, out = {};
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        if (args.expectedBuild && send('status').indexOf('"build":"' + args.expectedBuild + '"') < 0) throw Error('unexpected plugin build');
        out.defaults = send('settings', 'defaults');
        var p = new DocumentPreset(); p.colorMode = DocumentColorSpace.RGB; p.width = 600; p.height = 500; p.units = RulerUnits.Points;
        var doc = app.documents.addDocument('Web', p);
        doc.artboards[0].artboardRect = [0, 500, 600, 0];
        var lib2 = $.evalFile(new File(here + '/ai-auto-lib.jsx'));
        lib2.text(doc, 'old-text', 30, 480);
        lib2.triangle(doc, 'old-path', 40);
        var locked = lib2.triangle(doc, 'locked-path', 440);
        locked.locked = true;
        doc.selection = null;
        lib.write(args, 'phase1.json', out);
        return 'DONE|phase1';
    } catch (e) { return 'FAIL|' + String(e); }
    finally { app.userInteractionLevel = interaction; }
})();
