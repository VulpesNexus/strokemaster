// Diagnostic (region-prep-27): native-align outside on an open zigzag, called directly.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    var lib = $.evalFile(new File(new File($.fileName).parent.fsName + '/ai-text-parity-lib.jsx'));
    try {
        var p = new DocumentPreset(); p.colorMode = DocumentColorSpace.RGB; p.width = 400; p.height = 200; p.units = RulerUnits.Points;
        var doc = app.documents.addDocument('Web', p);
        doc.artboards[0].artboardRect = [0, 200, 400, 0];
        var z = doc.pathItems.add(); z.setEntirePath([[40, 50], [120, 150], [200, 50], [280, 150]]);
        z.filled = false; z.stroked = true; z.strokeWidth = 12; var g = new GrayColor(); g.gray = 60; z.strokeColor = g;
        lib.pdf(doc, args, 'before');
        doc.selection = null; z.selected = true;
        var r = String(app.sendScriptMessage('StrokeMasterRegionProbe', 'native-align', 'outside'));
        doc.selection = null; app.redraw(); lib.pdf(doc, args, 'after');
        doc.close(SaveOptions.DONOTSAVECHANGES);
        return 'DONE|' + (/"align":\{[^}]*\}/.exec(r) || [r.slice(0, 200)])[0];
    } catch (e) { return 'FAIL|' + String(e); }
})();
