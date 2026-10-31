// The panel menu's Align Selection Outside (region-prep-27), phase 1 of 2: an open zigzag (Illustrator
// draws only Center on it) selected and the command queued (ui-press align-outside, the menu item's
// own handler); <runId>-before.pdf. The document stays open for ai-align-menu-final.jsx.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId) return 'FAIL|missing arguments';
    if (app.documents.length) return 'FAIL|requires zero preexisting documents';
    var lib = $.evalFile(new File(new File($.fileName).parent.fsName + '/ai-text-parity-lib.jsx'));
    try {
        var p = new DocumentPreset(); p.colorMode = DocumentColorSpace.RGB; p.width = 400; p.height = 200; p.units = RulerUnits.Points;
        var doc = app.documents.addDocument('Web', p);
        doc.artboards[0].artboardRect = [0, 200, 400, 0];
        var z = doc.pathItems.add(); z.setEntirePath([[60, 40], [60, 160], [340, 160], [340, 40]]);   // an open U: its closing encloses one side
        z.filled = false; z.stroked = true; z.strokeWidth = 12; var g = new GrayColor(); g.gray = 60; z.strokeColor = g;
        lib.pdf(doc, args, 'before');
        doc.selection = null; z.selected = true;
        return 'DONE|' + String(app.sendScriptMessage('StrokeMasterRegionProbe', 'ui-press', 'align-outside')).slice(0, 12);
    } catch (e) { return 'FAIL|' + String(e); }
})();
