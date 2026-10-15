// StrokeMaster text-kept stroke-behind research modes (region-prep-13,
// TextBehind.h). Same fixture as ai-text-behind-proof.jsx. Applies the effect
// with StrokeMasterTextMode 1 (census only, pass-through), 2 (stroke-only
// outlines beneath, text strokes kept), 3 (text strokes cleared, no outlines),
// and 0 (full), records the effect's status, and exports a flat PDF per mode.
// Needs $.global.ISE_REGION_ARGS outDir and runId. Alerts suppressed and restored.
(function () {
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try { return run(); } finally { app.userInteractionLevel = interaction; }
    function run() {
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId) return 'FAIL|missing outDir or runId';
    var plugin = 'StrokeMasterRegionProbe';
    var xml = '<LiveEffect name="org.strokemaster.proposed.stroke-behind-text.v1"><Dict data="I StrokeMasterRegionFixtureVersion 1 "/></LiveEffect>';
    var censusXml = '<LiveEffect name="org.strokemaster.proposed.input-census.v1"><Dict data="I StrokeMasterRegionFixtureVersion 1 "/></LiveEffect>';
    var out = [];
    function note(k, v) { out.push(k + '=' + String(v).replace(/[|\r\n]/g, ' ')); }
    function send(selector, input) { return String(app.sendScriptMessage(plugin, selector, input || '')); }
    function block(json, name) {
        var m = new RegExp('"' + name + '":(\\{[^}]*\\})').exec(json);
        return m ? m[1] : '?';
    }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    var doc = null, at = 'begin';
    function fixture(stroked) {
        while (doc.pageItems.length > 0) doc.pageItems[0].remove();
        var t = doc.textFrames.add();
        t.contents = 'HH\rHH';
        t.position = [20, 180];
        var all = t.textRange.characterAttributes;
        all.size = 48;
        all.leading = 34;
        all.autoLeading = false;
        all.fillColor = gray(100);
        all.strokeColor = new NoColor();
        if (stroked) {
            var chars = t.textRange.characters;
            // characters: H H CR H H
            chars[1].characterAttributes.strokeColor = gray(40);
            chars[1].characterAttributes.strokeWeight = 8;
            for (var i = 3; i < 5; ++i) {
                chars[i].characterAttributes.strokeColor = gray(70);
                chars[i].characterAttributes.strokeWeight = 8;
            }
        }
        return t;
    }
    function pdf(name) {
        var f = new File(args.outDir + '/' + args.runId + '-' + name + '.pdf');
        if (f.exists) throw 'refusing to overwrite ' + f.name;
        var options = new PDFSaveOptions();
        options.acrobatLayers = false;
        doc.saveAs(f, options);
        note(name + '.pdf', f.length);
    }
    try {
        note('build', /"build":"([^"]*)"/.exec(send('status'))[1]);
        doc = app.documents.add(DocumentColorSpace.RGB, 200, 200);
        doc.artboards[0].artboardRect = [0, 200, 200, 0];
        at = 'native';
        fixture(true);
        app.redraw();
        pdf('native');
        var modes = [1, 2, 3, 0];
        for (var m = 0; m < modes.length; ++m) {
            at = 'mode' + modes[m];
            var t = fixture(true);
            t.applyEffect('<LiveEffect name="org.strokemaster.proposed.stroke-behind-text.v1"><Dict data="I StrokeMasterRegionFixtureVersion 1 I StrokeMasterTextMode ' + modes[m] + ' "/></LiveEffect>');
            app.redraw();
            note(at + '.status', block(send('status'), 'textBehind'));
            note(at + '.liveText', doc.textFrames.length === 1 && doc.textFrames[0].contents === 'HH\rHH');
            pdf(at);
        }
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
