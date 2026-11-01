// The owner's retest Z3 (2026-11-01): Close holes left open a gap between two words that the
// lines above and below close off. One phase per run in one Illustrator, the timer running between:
//   1  the switches of the owner's Z3 panel; a new document: a story threaded across three
//      overlapping frames, and one frame of twelve lines, Myriad Pro 12 pt, Auto leading, a 3 pt
//      character stroke.
//   2  every text frame selected: Correct Selection (scripted art is not corrected automatically).
//   3  read (the stories' groups, Close holes' report); exported as PDF.
// ISE_REGION_ARGS: outDir, runId.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function rgb(r, g, b) { var c = new RGBColor(); c.red = r; c.green = g; c.blue = b; return c; }
    var lorem = 'Lorem ipsum dolor sit amet, consectetuer adipiscing elit, sed diam nonummy nibh euismod tincidunt ut ' +
                'laoreet dolore magna aliquam erat volutpat. Ut wisi enim ad minim veniam, quis nostrud exerci tation ';
    function dress(range) {
        var a = range.characterAttributes;
        a.textFont = app.textFonts.getByName('MyriadPro-Regular');
        a.size = 12;
        a.autoLeading = true;
        a.fillColor = rgb(0, 0, 0);
        a.strokeColor = rgb(220, 50, 10);
        a.strokeWeight = 3;
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
            out.set = [send('settings', 'text_stories=1'), send('settings', 'object_strokes=1'), send('settings', 'outside_dashes=0'),
                       send('settings', 'glyph_tips=0'), send('settings', 'close_holes=1')];
            doc = app.documents.add(DocumentColorSpace.RGB, 400, 400);
            // Three frames, each overlapping the one before by about a line, as in the owner's file.
            var prev = null, first = null;
            for (var k = 0; k < 3; k++) {
                var box = doc.pathItems.rectangle(380 - 36 * k, 20 + 4 * k, 170, 44);
                var f = doc.textFrames.areaText(box);
                if (prev) prev.nextFrame = f;
                else { f.contents = lorem + lorem; first = f; }
                prev = f;
            }
            dress(first.story.textRange);
            var block = doc.textFrames.areaText(doc.pathItems.rectangle(240, 20, 200, 180));
            block.contents = lorem + lorem + lorem;
            dress(block.textRange);
        } else if (phase === 2) {
            doc.selection = null;
            for (var s = 0; s < doc.textFrames.length; s++) doc.textFrames[s].selected = true;
            out.correct = send('correct');
            doc.selection = null;
        } else {
            out.frames = [];
            for (var i = 0; i < doc.textFrames.length; i++) {
                var t = doc.textFrames[i];
                out.frames.push(t.parent.typename + (t.parent.typename === 'GroupItem' ? t.parent.textFrames.length : '') + ' ' +
                                Math.round(t.top) + ':' + t.lines.length);
            }
            out.holes = (/"closeHoles":\{[^}]*\}/.exec(send('status')) || [''])[0];
            lib.pdf(doc, args, 'holes');
        }
        lib.write(args, 'phase' + phase + '.json', out);
        return 'DONE|phase' + phase;
    } catch (e) { return 'FAIL|' + String(e) + ' line ' + e.line; }
    finally { app.userInteractionLevel = interaction; }
})();
