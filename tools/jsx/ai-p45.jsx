// The owner's retest notes of 2026-11-01: Close holes left a gap open on their AVMWxy stories with
// Trim glyph tips off. One phase per run in one Illustrator, the timer running between:
//   1  the switches of the owner's panel (Across other text objects, Dashed outside strokes, and
//      Close holes on; Trim glyph tips, the Appearance opt-in, and Also documents off, then
//      Also documents on for this open); args.file opened (a copy of their Tests.ai), corrected
//      as it opens.
//   2  every text frame selected: Correct Selection, as the owner would.
//   3  exported as PDF; the text objects' positions and groups read.
// ISE_REGION_ARGS: outDir, runId, file.
(function () {
    if (!$.getenv('CODEX_HIDDEN_DESKTOP_NAME') || !$.getenv('STROKEMASTER_SETTINGS'))
        throw Error('isolated test host and private settings required');
    var args = $.global.ISE_REGION_ARGS || {};
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    var phase = 1;
    while (lib.path(args, 'phase' + phase + '.json').exists) phase++;
    if (phase > 3) throw Error('all three phases are complete');
    var interaction = app.userInteractionLevel, out = {phase: phase};
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        var doc = app.documents.length ? app.activeDocument : null;
        if (phase === 1) {
            out.build = /"build":"[^"]*"/.exec(send('status'))[0];
            send('settings', 'defaults');
            out.set = [send('settings', 'text_stories=1'), send('settings', 'outside_dashes=1'), send('settings', args.tips ? 'glyph_tips=1' : 'glyph_tips=0'), send('settings', args.flat ? 'flat_tips=1' : 'flat_tips=0'), send('settings', args.objectStrokes ? 'object_strokes=1' : 'object_strokes=0'),
                       send('settings', 'close_holes=1'), send('settings', 'open_documents=1')];
            doc = app.open(new File(args.file));
        } else if (phase === 2) {
            doc.selection = null;
            for (var s = 0; s < doc.pageItems.length; s++) {
                var it = doc.pageItems[s];
                if (it.parent.typename === 'Layer' && !it.locked && !it.hidden &&
                    (!args.triangle || (it.typename === 'PathItem' && it.pathPoints.length === 3))) it.selected = true;
            }
            out.correct = send('correct');
            doc.selection = null;
        } else {
            out.frames = [];
            for (var i = 0; i < doc.textFrames.length; i++) {
                var t = doc.textFrames[i];
                out.frames.push(String(t.contents).substr(0, 8) + ' ' + Math.round(t.left) + ',' + Math.round(t.top) + ' ' +
                                t.parent.typename + (t.parent.typename === 'GroupItem' ? t.parent.textFrames.length : ''));
            }
            out.holes = (/"closeHoles":\{[^}]*\}/.exec(send('status')) || [''])[0];
            out.tipGate = (/"tipGate":"[^"]*"/.exec(send('status')) || [''])[0];
            out.pathJudged = (/"pathJudged":"[^"]*"/.exec(send('status')) || [''])[0];
            // The AVMWxy frames' styles (args.detail): the Close holes effect's parameters among them.
            if (args.detail) {
                out.dumps = [];
                for (var j = 0; j < doc.textFrames.length; j++) {
                    var f = doc.textFrames[j];
                    if (!/^AVMW/.test(f.contents)) continue;
                    var target = f.parent.typename === 'GroupItem' ? f.parent : f;
                    doc.selection = null; target.selected = true;
                    out.dumps.push(Math.round(f.top) + ' ' + send('style-dump', ''));
                    doc.selection = null;
                }
            }
            // The paths' styles (args.detail): the owner's triangle among them (CC3).
            if (args.detail) {
                out.paths = [];
                for (var pi = 0; pi < doc.pathItems.length && pi < 12; pi++) {
                    var pp = doc.pathItems[pi];
                    if (pp.parent.typename === 'CompoundPathItem') continue;
                    doc.selection = null; pp.selected = true;
                    var at = [];
                    for (var q = 0; q < pp.pathPoints.length; q++) {
                        var pt = pp.pathPoints[q];
                        at.push(String(pt.leftDirection) + ' [' + String(pt.anchor) + '] ' + String(pt.rightDirection));
                    }
                    out.paths.push(pp.pathPoints.length + 'pt w' + pp.strokeWidth + ' ' + at.join(' | ') + ' ' +
                                   send('style-dump', '').substr(0, 700));
                    doc.selection = null;
                }
            }
            out.artboard = String(doc.artboards[0].artboardRect);
            lib.pdf(doc, args, 'tests');
        }
        lib.write(args, 'phase' + phase + '.json', out);
        return 'DONE|phase' + phase;
    } catch (e) { return 'FAIL|' + String(e) + ' line ' + e.line; }
    finally { app.userInteractionLevel = interaction; }
})();
