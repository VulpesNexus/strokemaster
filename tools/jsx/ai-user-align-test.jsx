// Test of tools/jsx/strokemaster-align-selection.jsx and strokemaster-outside-dashes.jsx as shipped:
// an open path, type with one stroke, and a dashed Outside thorn; the align script with Outside on
// the first two, the dashes script on the thorn, then the align script with Restore on all three.
// Each script's message is returned (run with ISE_REGION_ARGS set, the scripts return it instead
// of showing it). Needs $.global.ISE_REGION_ARGS outDir and runId.
(function () {
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try { return run(); } finally { app.userInteractionLevel = interaction; }
    function run() {
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId) return 'FAIL|missing outDir or runId';
    var out = [];
    function note(k, v) { out.push(k + '=' + String(v).replace(/[|\r\n]/g, ' ')); }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    var folder = new File($.fileName).parent;
    var alignScript = new File(folder + '/strokemaster-align-selection.jsx'), dashScript = new File(folder + '/strokemaster-outside-dashes.jsx');
    var doc = app.documents.addDocument('Web', new DocumentPreset());
    try {
        doc.artboards[0].artboardRect = [0, 400, 400, 0];
        var o = doc.pathItems.add(); o.setEntirePath([[40, 40], [100, 160], [160, 40]]);
        o.filled = false; o.stroked = true; o.strokeColor = gray(60); o.strokeWidth = 8;
        var t = doc.textFrames.add(); t.contents = 'Ax'; t.position = [200, 380];
        var ca = t.textRange.characterAttributes; ca.size = 72; ca.fillColor = gray(100); ca.strokeColor = gray(60); ca.strokeWeight = 6;
        var half = 5 * Math.PI / 180, bw = 60 * Math.sin(half), by = 60 * Math.cos(half);
        var th = doc.pathItems.add();
        th.setEntirePath([[300, 260], [300 + bw, 260 - by], [345, 260 - by], [345, 260 - by - 50], [255, 260 - by - 50], [255, 260 - by], [300 - bw, 260 - by]]);
        th.closed = true; th.filled = true; th.fillColor = gray(100); th.stroked = true; th.strokeColor = gray(60); th.strokeWidth = 4;
        th.strokeDashes = [12, 4];
        doc.selection = null; th.selected = true;
        app.sendScriptMessage('StrokeMasterRegionProbe', 'stroke-align', 'outside');
        doc.selection = null; o.selected = true; t.selected = true;
        $.global.ISE_REGION_ARGS.align = 'outside';
        note('align outside', $.evalFile(alignScript));
        doc.selection = null; th.selected = true;
        note('dashes', $.evalFile(dashScript));
        doc.selection = null; o.selected = true; t.selected = true; th.selected = true;
        $.global.ISE_REGION_ARGS.align = 'restore';
        note('restore', $.evalFile(alignScript));
        note('thorn after restore', th.strokeWidth + ' pt');
        doc.selection = null;
        note('empty', $.evalFile(alignScript));
        return 'DONE|' + out.join('|');
    } catch (error) {
        return 'FAIL|' + String(error).replace(/[|\r\n]/g, ' ') + ' line ' + error.line + '|' + out.join('|');
    }
    }
})();
