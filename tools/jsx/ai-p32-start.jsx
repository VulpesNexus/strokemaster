// The owner's prep-31 retests, as host tests for region-prep-32 (2026-10-31), phase 1 of 4: a
// dashed square set Outside and corrected with Dashed outside strokes on (the opt-in draws it as
// Center at twice the weight under its fill). Phase 2 (ai-p32-edit.jsx) adds text with a stroke of
// its own above Characters, threads two overlapping frames, and sets the square Inside as the
// Stroke panel does; phase 3 (ai-p32-read.jsx) reads and sets the text's Appearance stroke limit
// by hand; phase 4 (ai-p32-final.jsx) reads again. ISE_REGION_ARGS: outDir, runId; optional expectedBuild.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId) return 'FAIL|missing arguments';
    if (app.documents.length) return 'FAIL|requires zero preexisting documents';
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    var lib2 = $.evalFile(new File(here + '/ai-auto-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    var interaction = app.userInteractionLevel, out = {};
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        out.status = /"build":"[^"]*"/.exec(send('status'))[0];
        if (args.expectedBuild && out.status !== '"build":"' + args.expectedBuild + '"') throw Error('unexpected plugin build');
        send('settings', 'defaults');
        out.settings = send('settings', 'outside_dashes=1');
        // The Appearance stroke's lift (S4) is behind its opt-in since region-prep-36.
        out.settings += '|' + send('settings', 'object_strokes=1');
        var p = new DocumentPreset(); p.colorMode = DocumentColorSpace.RGB; p.width = 700; p.height = 500; p.units = RulerUnits.Points;
        var doc = app.documents.addDocument('Web', p);
        doc.artboards[0].artboardRect = [0, 500, 700, 0];
        var sq = doc.pathItems.rectangle(460, 440, 200, 200); sq.name = 'dash-sq';
        sq.filled = true; sq.fillColor = lib2.gray(10); sq.stroked = true; sq.strokeColor = lib2.gray(70);
        sq.strokeWidth = 6; sq.strokeDashes = [12, 6]; sq.strokeJoin = StrokeJoin.MITERENDJOIN;
        doc.selection = null; sq.selected = true;
        out.outside = send('stroke-align', 'outside');
        doc.selection = null; sq.selected = true;
        out.correct = send('correct');
        out.square = lib2.read(doc, 'dash-sq');
        out.squareWidth = sq.strokeWidth;
        lib.write(args, 'phase1.json', out);
        return 'DONE|phase1';
    } catch (e) { return 'FAIL|' + String(e) + ' line ' + e.line; }
    finally { app.userInteractionLevel = interaction; }
})();
