// Route re-fix test, phase 3 of 3: after idle time, the re-fix counters and
// <runId>-recolor.pdf, then the document saved as <runId>.ai and closed.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (app.documents.length !== 1) return 'FAIL|expected the phase 1 document';
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        var doc = app.activeDocument;
        var status = String(app.sendScriptMessage('StrokeMasterRegionProbe', 'status', ''));
        lib.pdf(doc, args, 'recolor');
        var ai = lib.path(args, 'refix.ai'), so = new IllustratorSaveOptions(); so.pdfCompatible = true;
        if (ai.exists) throw Error('refusing existing ai');
        doc.saveAs(ai, so);
        lib.write(args, 'phase3.json', {refix: /"refix":\{[^}]*\}/.exec(status)[0], groups: doc.groupItems.length,
                                        snapshot: lib.snapshot(doc)});
        doc.close(SaveOptions.DONOTSAVECHANGES);
        return 'DONE|phase3';
    } catch (e) { return 'FAIL|' + String(e); }
    finally { app.userInteractionLevel = interaction; }
})();
