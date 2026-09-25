// StrokeMaster AI-13 export control: does any live effect on live text make
// Illustrator export the text as outlines, or only a plugin effect? Exports
// the same point text "HH" (character fill black, 6 pt gray character stroke)
// as a flat PDF three ways, in one owned document:
//   none    no effect
//   adobe   Adobe Offset Path at 0 pt (a host effect that changes no geometry)
//   census  StrokeMaster's pass-through input census effect
// Needs $.global.ISE_REGION_ARGS outDir and runId. Returns a pipe-delimited
// record; the PDFs are inspected externally. Alerts suppressed and restored.
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
    var effects = {
        adobe: '<LiveEffect name="Adobe Offset Path"><Dict data="R ofst 0 I jntp 2 R mlim 4 "/></LiveEffect>',
        census: '<LiveEffect name="org.strokemaster.proposed.input-census.v1"><Dict data="I StrokeMasterRegionFixtureVersion 1 "/></LiveEffect>'
    };
    var doc = null, at = 'begin';
    try {
        doc = app.documents.add(DocumentColorSpace.RGB, 400, 200);
        var names = ['none', 'adobe', 'census'];
        for (var i = 0; i < names.length; ++i) {
            at = names[i];
            while (doc.pageItems.length > 0) doc.pageItems[0].remove();
            var t = doc.textFrames.add();
            t.contents = 'HH';
            t.position = [0, 150];
            var a = t.textRange.characterAttributes;
            a.size = 48;
            a.fillColor = gray(100);
            a.strokeColor = gray(40);
            a.strokeWeight = 6;
            if (effects[names[i]]) {
                try { t.applyEffect(effects[names[i]]); note(names[i] + '.apply', 'ok'); }
                catch (e) { note(names[i] + '.apply', 'throw ' + e); }
            }
            app.redraw();
            note(names[i] + '.liveText', doc.textFrames.length === 1 && doc.textFrames[0].contents === 'HH');
            var f = new File(args.outDir + '/' + args.runId + '-' + names[i] + '.pdf');
            if (f.exists) throw 'refusing to overwrite ' + f.name;
            var options = new PDFSaveOptions();
            options.acrobatLayers = false;
            doc.saveAs(f, options);
            note(names[i] + '.pdf', f.length);
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
