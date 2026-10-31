// Open in its own COM phase; inspect only after Illustrator has returned to idle.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (app.documents.length) return 'FAIL|preexisting document';
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        var name = args.inspectFile || (args.runId + '-' + args.kinds[0] + '.ai');
        if (!/^[A-Za-z0-9_-]+\.ai$/.test(name)) return 'FAIL|invalid owned file name';
        var f = new File(args.outDir + '/' + name);
        if (!f.exists) return 'FAIL|owned AI file missing';
        app.open(f); app.redraw();
        return 'DONE|opened-for-next-phase';
    } catch (e) { return 'FAIL|open|' + String(e); }
    finally { app.userInteractionLevel = interaction; }
})();
