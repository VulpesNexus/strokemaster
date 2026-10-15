// StrokeMaster text-in-PDF control 2: which host effects keep live text as PDF
// text? Claim 8 found Offset Path (path input) outlines it. This exports the same
// point text "HH" (character fill black, 6 pt gray character stroke) as a flat PDF
// under: none; offset (Adobe Offset Path 0 pt, the claim-8 control); shadow (Adobe
// Drop Shadow); transform (Adobe Transform, identity); glow (Adobe Outer Glow).
// The PDFs are inspected externally for text characters and fonts. Needs
// $.global.ISE_REGION_ARGS outDir and runId. Alerts suppressed and restored.
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
        offset: '<LiveEffect name="Adobe Offset Path"><Dict data="R ofst 0 I jntp 2 R mlim 4 "/></LiveEffect>',
        shadow: '<LiveEffect name="Adobe Drop Shadow"><Dict data="R horz 7 R vert 7 R blur 5 R opac 0.75 I blnd 1 I csrc 1 R dark 0 "/></LiveEffect>',
        transform: '<LiveEffect name="Adobe Transform"><Dict data="R scaleH_Factor 1 R scaleV_Factor 1 R moveH_Pts 0 R moveV_Pts 0 R rotate_Radians 0 I numCopies 0 "/></LiveEffect>',
        glow: '<LiveEffect name="Adobe Outer Glow"><Dict data="R blur 5 R opac 0.75 I mode 2 "/></LiveEffect>'
    };
    var doc = null, at = 'begin';
    try {
        doc = app.documents.add(DocumentColorSpace.RGB, 400, 200);
        var names = ['none', 'offset', 'shadow', 'transform', 'glow'];
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
