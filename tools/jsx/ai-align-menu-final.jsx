// The panel menu's Align Selection Outside, phase 2 of 2: after idle time the timer has run it;
// <runId>-after.pdf, then one undo and <runId>-undone.pdf.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (app.documents.length !== 1) return 'FAIL|expected the phase 1 document';
    var lib = $.evalFile(new File(new File($.fileName).parent.fsName + '/ai-text-parity-lib.jsx'));
    try {
        var doc = app.activeDocument;
        doc.selection = null; app.redraw(); lib.pdf(doc, args, 'after');
        app.undo(); app.redraw(); lib.pdf(doc, args, 'undone');
        doc.close(SaveOptions.DONOTSAVECHANGES);
        return 'DONE|phase2';
    } catch (e) { return 'FAIL|' + String(e); }
})();
