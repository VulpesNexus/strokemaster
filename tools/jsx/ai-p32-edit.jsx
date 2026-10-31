// region-prep-32 retests, phase 2 of 4 (ai-p32-start.jsx): the edits, each as a hand makes it,
// in one undo step. "obj-text": red point text with no stroke on its characters and a 7 pt
// Appearance stroke above Characters (the owner's Q1). "thread-a" and "thread-b": two overlapping
// threaded frames, red fill and a gray 7 pt stroke (Q6). The square set Inside (P-dash).
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (app.documents.length !== 1) return 'FAIL|expected the phase 1 document';
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    var lib2 = $.evalFile(new File(here + '/ai-auto-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function rgb(r, g, b) { var c = new RGBColor(); c.red = r; c.green = g; c.blue = b; return c; }
    var interaction = app.userInteractionLevel, out = {};
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        var doc = app.activeDocument;
        var t = doc.textFrames.add(); t.contents = 'AVMWxy'; t.position = [30, 480]; t.name = 'obj-text';
        var a = t.textRange.characterAttributes;
        a.textFont = app.textFonts.getByName('MinionPro-Regular'); a.size = 48; a.fillColor = rgb(200, 30, 30);
        a.strokeColor = new NoColor();
        doc.selection = null; t.selected = true;
        out.addStroke = send('align-survey', 'stroke-above:7');
        function frame(top, left, name, prev) {
            var r = doc.pathItems.rectangle(top, left, 160, 90);
            var f = prev ? doc.textFrames.areaText(r, TextOrientation.HORIZONTAL, prev) : doc.textFrames.areaText(r);
            f.name = name;
            return f;
        }
        var fa = frame(330, 30, 'thread-a', null), fb = frame(270, 60, 'thread-b', fa);
        fa.contents = 'AVMW xyAV MWxy AVMW xyAV MWxy AVMW xyAV MWxy AVMW xyAV MWxy AVMW';
        var ta = fa.story.textRange.characterAttributes;
        ta.textFont = app.textFonts.getByName('MinionPro-Regular'); ta.size = 30; ta.fillColor = rgb(200, 30, 30);
        ta.strokeColor = lib2.gray(40); ta.strokeWeight = 7;
        out.storyFrames = fa.story.textFrames.length;
        doc.selection = null; doc.pageItems.getByName('dash-sq').selected = true;
        out.inside = send('stroke-align', 'inside');
        doc.selection = null;
        lib.write(args, 'phase2.json', out);
        return 'DONE|phase2';
    } catch (e) { return 'FAIL|' + String(e) + ' line ' + e.line; }
    finally { app.userInteractionLevel = interaction; }
})();
