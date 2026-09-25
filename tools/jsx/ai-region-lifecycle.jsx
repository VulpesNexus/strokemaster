// StrokeMaster region effect lifecycle. Runs inside Illustrator via COM DoJavaScript.
// One owned document, one save/close/reopen cycle (more document churn crashes
// Illustrator 30.7.0 with or without plugins). Steps:
//   create  bridge "create region", redraw: expect a published result
//   undo    app.undo(): the created fixture disappears
//   redo    app.redo(): it returns and redraw republishes or reuses the result
//   save    save as AI with PDF compatibility, close, reopen
//   reopen  redraw: expect the stored result or a fresh publish, bounds unchanged
//   pdf     export the reopened document as a flat PDF
//   edit    move the hole's apex: the marked fixture no longer matches its oracle,
//           so the effect must refuse and pass the native appearance through
// Needs $.global.ISE_REGION_ARGS.outDir and runId. Returns a pipe-delimited
// record. Alerts are suppressed for the run and the previous level restored.
(function () {
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try { return run(); } finally { app.userInteractionLevel = interaction; }
    function run() {
    var args = $.global.ISE_REGION_ARGS || {};
    var plugin = 'StrokeMasterRegionProbe';
    var out = [];
    function note(k, v) { out.push(k + '=' + String(v).replace(/[|\r\n]/g, ' ')); }
    function send(selector, input) { return String(app.sendScriptMessage(plugin, selector, input || '')); }
    function field(json, name) {
        var m = new RegExp('"' + name + '":("[^"]*"|[-0-9.e]+|true|false)').exec(json);
        return m ? m[1] : '?';
    }
    function status(tag) {
        var s = send('status');
        var keys = ['goCalls', 'mutations', 'gate', 'passThroughs'];
        for (var i = 0; i < keys.length; ++i) note(tag + '.' + keys[i], field(s, keys[i]));
    }
    function bounds(tag) {
        note(tag + '.items', doc.pageItems.length);
        if (doc.pageItems.length) note(tag + '.bounds', doc.pageItems[0].visibleBounds.join(','));
    }
    function file(suffix) {
        var f = new File(args.outDir + '/' + args.runId + '-' + suffix);
        if (f.exists) throw 'refusing to overwrite ' + f.name;
        return f;
    }
    if (!args.outDir || !args.runId) return 'FAIL|missing outDir or runId';
    var doc = null, at = 'begin';
    try {
        at = 'doc';
        note('build', field(send('status'), 'build'));
        doc = app.documents.add(DocumentColorSpace.RGB, 140, 140);
        doc.artboards[0].artboardRect = [-20, 120, 120, -20];
        app.redraw();

        at = 'create';
        note('create', field(send('create', 'region'), 'gate'));
        app.redraw();
        status('create');
        bounds('create');

        at = 'undo';
        app.undo();
        app.redraw();
        bounds('undo');
        at = 'redo';
        app.redo();
        app.redraw();
        status('redo');
        bounds('redo');

        at = 'save';
        var aiFile = file('region.ai');
        var aiOptions = new IllustratorSaveOptions();
        aiOptions.pdfCompatible = true;
        doc.saveAs(aiFile, aiOptions);
        note('save.bytes', aiFile.length);
        doc.close(SaveOptions.DONOTSAVECHANGES);
        doc = null;

        at = 'reopen';
        doc = app.open(aiFile);
        app.redraw();
        status('reopen');
        bounds('reopen');

        at = 'pdf';
        var pdfOptions = new PDFSaveOptions();
        pdfOptions.acrobatLayers = false;
        var pdfFile = file('reopened.pdf');
        doc.saveAs(pdfFile, pdfOptions);
        note('pdf.bytes', pdfFile.length);

        at = 'edit';
        var wrapper = doc.pageItems[0];
        var hole = wrapper.pageItems[0].pathItems[1];
        var points = hole.pathPoints;
        var apex = points[0].anchor;
        note('edit.apexBefore', apex.join(','));
        points[0].anchor = [apex[0] + 0.5, apex[1]];
        points[0].leftDirection = points[0].anchor;
        points[0].rightDirection = points[0].anchor;
        app.redraw();
        status('edit');
        bounds('edit');

        at = 'close';
        doc.close(SaveOptions.DONOTSAVECHANGES);
        doc = null;
        return 'DONE|' + out.join('|');
    } catch (error) {
        if (doc) { try { doc.close(SaveOptions.DONOTSAVECHANGES); } catch (ignored) {} }
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + '|' + out.join('|');
    }
    }
})();
