// StrokeMaster Q20 discriminator. Runs inside Illustrator via COM DoJavaScript.
// Question: does the region effect run, and on which attachment route does it
// reach its published result? Arms run one at a time in one owned document,
// each at the fixture's own coordinates, and each arm's art is removed before
// the next (reusing one document avoids the host's document-churn crash):
//   A  bridge "create region" (the bridge's own attachment route)
//   B  bridge "create control", then host applyEffect on the wrapper group
//   C  bridge "create control", then host applyEffect on the inner compound
//   N  bridge "create control" only (no effect): the negative control
// Arms A and N also export a PDF when $.global.ISE_REGION_ARGS.outDir is set.
// Returns a pipe-delimited record. Never touches documents it did not create.
// Alerts are suppressed for the run and the previous level is restored.
(function () {
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try { return run(); } finally { app.userInteractionLevel = interaction; }
    function run() {
    var args = $.global.ISE_REGION_ARGS || {};
    var plugin = 'StrokeMasterRegionProbe';
    var effect = 'org.strokemaster.proposed.closed-region.v1';
    var xml = '<LiveEffect name="' + effect + '"><Dict data="I StrokeMasterRegionFixtureVersion 1 "/></LiveEffect>';
    var out = [];
    function note(k, v) { out.push(k + '=' + String(v).replace(/[|\r\n]/g, ' ')); }
    function send(selector, input) { return String(app.sendScriptMessage(plugin, selector, input || '')); }
    function field(json, name) {
        var m = new RegExp('"' + name + '":("[^"]*"|[-0-9.e]+|true|false)').exec(json);
        return m ? m[1] : '?';
    }
    var keys = ['goCalls', 'mutations', 'gate', 'lastGoError', 'passThroughs', 'wrapperPaintFields',
                'wrapperContainsPaint', 'wrapperBase', 'groupContentsPosition', 'processingCensus',
                'maxVertexDeviation', 'readbackAnchorDelta', 'readbackHandleDelta'];
    function status(tag) {
        var s = send('status');
        for (var i = 0; i < keys.length; ++i) note(tag + '.' + keys[i], field(s, keys[i]));
        return s;
    }
    var doc = null, at = 'begin';
    function clear() { while (doc.pageItems.length > 0) doc.pageItems[0].remove(); app.redraw(); }
    function pdf(name) {
        if (!args.outDir) return;
        var f = new File(args.outDir + '/' + (args.runId || 'region') + '-' + name + '.pdf');
        if (f.exists) throw 'refusing to overwrite ' + f.name;
        var options = new PDFSaveOptions();
        // Layers exported as optional content are refused by the measurer.
        options.acrobatLayers = false;
        doc.saveAs(f, options);
        note(name + '.pdf', f.exists ? f.length : 'missing');
    }
    function arm(name, kind, target) {
        at = 'arm' + name;
        note(name + '.create', field(send('create', kind), 'gate'));
        var root = doc.activeLayer.pageItems[0];
        var item = target === 'compound' ? root.pageItems[0] : root;
        note(name + '.typename', item.typename);
        if (target) {
            try { item.applyEffect(xml); note(name + '.apply', 'ok'); }
            catch (e) { note(name + '.apply', 'throw ' + e); }
        }
        app.redraw();
        status(name);
        note(name + '.bounds', item.visibleBounds.join(','));
    }
    try {
        at = 'status0';
        note('build', field(status('before'), 'build'));
        at = 'doc';
        doc = app.documents.add(DocumentColorSpace.RGB, 140, 140);
        doc.artboards[0].artboardRect = [-20, 120, 120, -20];
        note('applyEffectType', typeof doc.activeLayer.pathItems.add().applyEffect);
        clear();

        arm('A', 'region', null);
        pdf('A');
        clear();
        arm('B', 'control', 'group');
        clear();
        arm('C', 'control', 'compound');
        clear();
        arm('N', 'control', null);
        pdf('N');
        clear();

        at = 'diagnostics';
        note('diagnostics', send('diagnostics'));
        at = 'close';
        doc.close(SaveOptions.DONOTSAVECHANGES);
        doc = null;
        return 'DONE|' + out.join('|');
    } catch (error) {
        try { note('diagnostics', send('diagnostics')); } catch (ignored) {}
        if (doc) { try { doc.close(SaveOptions.DONOTSAVECHANGES); } catch (ignored2) {} }
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + '|' + out.join('|');
    }
    }
})();
