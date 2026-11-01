// The owner's retest notes of 2026-11-01: Close holes on outlined text (paths and compound paths),
// in Illustrator as in InDesign. One phase per run in one Illustrator, the timer running between:
//   1  the switches (Close holes on); a new document: "Stroke"/"Master" in Minion Pro Bold, two
//      tight lines, outlined three times: filled with a 3 pt Center Miter stroke (tracked -60), the same with no
//      fill (hollow), and filled with a dashed stroke. args.off: Close holes off instead.
//   2  every top-level item selected: Correct Selection.
//   3  read (each item's style); exported as PDF.
// ISE_REGION_ARGS: outDir, runId, off.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function rgb(r, g, b) { var c = new RGBColor(); c.red = r; c.green = g; c.blue = b; return c; }
    function paint(item, fill, dashed) {
        var paths = [];
        (function walk(a) {
            if (a.typename === 'PathItem') paths.push(a);
            else if (a.typename === 'CompoundPathItem') { for (var i = 0; i < a.pathItems.length; i++) paths.push(a.pathItems[i]); }
            else if (a.pageItems) { for (var j = 0; j < a.pageItems.length; j++) walk(a.pageItems[j]); }
        })(item);
        for (var k = 0; k < paths.length; k++) {
            var p = paths[k];
            p.filled = fill; if (fill) p.fillColor = rgb(35, 50, 100);
            p.stroked = true; p.strokeColor = rgb(240, 150, 30); p.strokeWidth = 3;
            p.strokeJoin = StrokeJoin.MITERENDJOIN; p.strokeMiterLimit = 4;
            p.strokeDashes = dashed ? [8, 4] : [];
        }
    }
    var phase = 1;
    while (lib.path(args, 'phase' + phase + '.json').exists) phase++;
    var interaction = app.userInteractionLevel, out = {phase: phase};
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        var doc = app.documents.length ? app.activeDocument : null;
        if (phase === 1) {
            out.build = /"build":"[^"]*"/.exec(send('status'))[0];
            send('settings', 'defaults');
            out.set = send('settings', args.off ? 'close_holes=0' : 'close_holes=1');
            doc = app.documents.add(DocumentColorSpace.RGB, 420, 840);
            var kinds = [[true, false], [false, false], [true, true], [true, false, true]];
            for (var k = 0; k < kinds.length; k++) {
                var t = doc.textFrames.pointText([20, 790 - 200 * k]);
                t.contents = 'Stroke\rMaster';
                var a = t.textRange.characterAttributes;
                a.textFont = app.textFonts.getByName('MinionPro-Bold'); a.size = 64; a.autoLeading = false; a.leading = 44; a.tracking = -60;
                if (kinds[k][2]) {
                    // The stroke on the text object itself (the Appearance panel's Add New Stroke),
                    // which Create Outlines carries to the group (the owner's BB5).
                    a.fillColor = rgb(35, 50, 100);
                    doc.defaultStroked = true; doc.defaultStrokeColor = rgb(200, 40, 30); doc.defaultStrokeWidth = 3;
                    doc.selection = null; t.selected = true;
                    app.executeMenuCommand('Adobe New Stroke Shortcut');
                    doc.selection = null;
                    out.groupStroke = (out.groupStroke || '') + t.createOutline().typename;
                    continue;
                }
                var g = t.createOutline();
                paint(g, kinds[k][0], kinds[k][1]);
            }
        } else if (phase === 2) {
            doc.selection = null;
            for (var s = 0; s < doc.pageItems.length; s++) if (doc.pageItems[s].parent.typename === 'Layer') doc.pageItems[s].selected = true;
            out.correct = send('correct');
            doc.selection = null;
        } else {
            out.holes = (/"closeHoles":\{[^}]*\}/.exec(send('status')) || [''])[0];
            out.dumps = [];
            for (var q = 0; q < doc.compoundPathItems.length && q < 3; q++) {
                doc.selection = null; doc.compoundPathItems[q].selected = true;
                out.dumps.push(send('style-dump', '').substr(0, 900));
            }
            for (var gi = 0; gi < doc.groupItems.length; gi++) {
                doc.selection = null; doc.groupItems[gi].selected = true;
                out.dumps.push('GROUP ' + send('style-dump', 'parent').substr(0, 900));
            }
            doc.selection = null;
            lib.pdf(doc, args, 'outlined');
        }
        lib.write(args, 'phase' + phase + '.json', out);
        return 'DONE|phase' + phase;
    } catch (e) { return 'FAIL|' + String(e) + ' line ' + e.line; }
    finally { app.userInteractionLevel = interaction; }
})();
