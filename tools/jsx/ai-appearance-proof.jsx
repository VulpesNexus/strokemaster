// StrokeMaster Q22 proof: can corrected Illustrator text export as PDF text?
// Compares, for point text "HH" / "HH" at 48 pt on 34 pt leading with black fills
// and a uniform 40% gray 8 pt stroke:
//   fill        no stroke at all (fill reference)
//   native      the stroke as a character attribute (Illustrator's default)
//   appearance  no character stroke; one object-level Appearance stroke added
//               beneath the frame's characters by the probe plugin's bridge
//               ("appearance" / "stroke-below"), a plain style with no effect
// Exports each as a flat PDF and saves the appearance version as AI for the
// cross-process and missing-plugin checks. Needs $.global.ISE_REGION_ARGS outDir
// and runId. Returns a pipe-delimited record. Alerts suppressed and restored.
(function () {
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try { return run(); } finally { app.userInteractionLevel = interaction; }
    function run() {
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId) return 'FAIL|missing outDir or runId';
    var plugin = 'StrokeMasterRegionProbe';
    var out = [];
    function note(k, v) { out.push(k + '=' + String(v).replace(/[|\r\n]/g, ' ')); }
    function send(selector, input) { return String(app.sendScriptMessage(plugin, selector, input || '')); }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    function file(name) {
        var f = new File(args.outDir + '/' + args.runId + '-' + name);
        if (f.exists) throw 'refusing to overwrite ' + f.name;
        return f;
    }
    function pdf(name) {
        var f = file(name + '.pdf');
        var options = new PDFSaveOptions();
        options.acrobatLayers = false;
        doc.saveAs(f, options);
        note(name + '.pdf', f.length);
    }
    var doc = null, at = 'begin';
    function fixture(stroked) {
        while (doc.pageItems.length > 0) doc.pageItems[0].remove();
        var t = doc.textFrames.add();
        t.contents = 'HH\rHH';
        t.position = [20, 180];
        var a = t.textRange.characterAttributes;
        a.size = 48; a.leading = 34; a.autoLeading = false;
        a.fillColor = gray(100);
        if (stroked) { a.strokeColor = gray(40); a.strokeWeight = 8; } else { a.strokeColor = new NoColor(); }
        app.redraw();
        return t;
    }
    try {
        note('build', /"build":"([^"]*)"/.exec(send('status'))[1]);
        doc = app.documents.add(DocumentColorSpace.RGB, 200, 200);
        doc.artboards[0].artboardRect = [0, 200, 200, 0];
        at = 'fill'; fixture(false); pdf('fill');
        at = 'native'; fixture(true); pdf('native');
        at = 'appearance';
        fixture(false);
        var reply = send('appearance', 'stroke-below');
        note('appearance.ok', /"ok":(true|false)/.exec(reply)[1]);
        note('appearance.status', /"appearance":(\{[^}]*\})/.exec(send('status'))[1]);
        app.redraw();
        note('appearance.liveText', doc.textFrames.length === 1 && doc.textFrames[0].contents === 'HH\rHH');
        pdf('appearance');
        at = 'save';
        var ai = file('appearance.ai');
        var aiOptions = new IllustratorSaveOptions();
        aiOptions.pdfCompatible = true;
        doc.saveAs(ai, aiOptions);
        note('appearance.ai', ai.length);
        doc.close(SaveOptions.DONOTSAVECHANGES);
        doc = null;
        return 'DONE|' + out.join('|');
    } catch (error) {
        if (doc) { try { doc.close(SaveOptions.DONOTSAVECHANGES); } catch (ignored) {} }
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + '|' + out.join('|');
    }
    }
})();
