// The owner's hand-test document (2026-10-30): every situation scripts cannot test, laid out and
// labeled on one artboard, for docs/ILLUSTRATOR_HAND_TESTS_2026-10-30.md. Saved as
// <outDir>/StrokeMaster hand tests.ai. Synthetic text only. ISE_REGION_ARGS: outDir.
//   1  a threaded story (two frames, the second overlapping the first's text): one fill, one stroke
//   2  point text, one fill, mixed strokes (the fill route): typing at the end of a stroked word
//   3  point text, one fill, one stroke (the stroke route): recoloring with the Swatches panel
//   4  two separate one-frame stories overlapping: the separate-stories opt-in
//   5  a dashed square (the dashed Outside opt-in) and an open zigzag (the Align commands, refused: no side)
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir) return 'FAIL|missing outDir';
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    function rgb(r, g, b) { var c = new RGBColor(); c.red = r; c.green = g; c.blue = b; return c; }
    try {
        var p = new DocumentPreset(); p.colorMode = DocumentColorSpace.RGB; p.width = 612; p.height = 1000; p.units = RulerUnits.Points;
        var doc = app.documents.addDocument('Web', p);
        doc.artboards[0].artboardRect = [0, 1000, 612, 0];
        var font = app.textFonts.getByName('MinionPro-Regular');
        function label(n, text, top) {
            var t = doc.textFrames.add();
            t.contents = n + '  ' + text;
            t.position = [24, top];
            var a = t.textRange.characterAttributes;
            a.size = 11; a.fillColor = gray(70); a.strokeColor = new NoColor();
            try { a.textFont = app.textFonts.getByName('MyriadPro-Regular'); } catch (e) {}
            t.name = 'Label ' + n;
        }
        function styleText(range, fill) {
            var a = range.characterAttributes;
            a.textFont = font; a.size = 38; a.leading = 28; a.autoLeading = false;
            a.fillColor = fill; a.strokeColor = new NoColor();
        }
        // 1. A threaded story.
        label('1', 'Threaded story: select both frames, Correct Selection; then undo and redo.', 985);
        var s1 = doc.textFrames.areaText(doc.pathItems.rectangle(960, 30, 185, 48));
        s1.nextFrame = doc.textFrames.areaText(doc.pathItems.rectangle(935, 40, 400, 120));
        s1.contents = 'AVMWxy AVMWxy AVMWxy AVMWxy';
        styleText(s1.story.textRange, rgb(200, 30, 30));
        s1.story.textRange.characterAttributes.strokeColor = gray(40);
        s1.story.textRange.characterAttributes.strokeWeight = 7;
        // 2. Point text, one fill, mixed strokes.
        label('2', 'One fill, mixed strokes: correct, then type AV at the end of the last word.', 790);
        var s2 = doc.textFrames.add(); s2.position = [30, 752]; s2.contents = 'AVMWxy AVMWxy\rAVMWxy AVMWxy';
        styleText(s2.textRange, rgb(200, 30, 30));
        var ch = s2.textRange.characters;
        for (var j = 0; j < ch.length; j++) {
            if (/[ \r\n]/.test(ch[j].contents) || j % 3 === 0) continue;
            ch[j].characterAttributes.strokeColor = gray(j % 2 ? 40 : 70);
            ch[j].characterAttributes.strokeWeight = 7;
        }
        // 3. Point text, one fill, one stroke.
        label('3', 'One fill, one stroke: correct, then recolor a word with the Swatches panel.', 640);
        var s3 = doc.textFrames.add(); s3.position = [30, 602]; s3.contents = 'AVMWxy AVMWxy\rAVMWxy AVMWxy';
        styleText(s3.textRange, rgb(200, 30, 30));
        s3.textRange.characterAttributes.strokeColor = gray(40); s3.textRange.characterAttributes.strokeWeight = 7;
        // 4. Two separate stories overlapping.
        label('4', 'Two separate stories: Across separate stories on, select both, correct.', 490);
        var s4a = doc.textFrames.add(); s4a.position = [30, 452]; s4a.contents = 'AVMWxy AVMWxy';
        var s4b = doc.textFrames.add(); s4b.position = [40, 428]; s4b.contents = 'AVMWxy AVMWxy';
        for (var k = 0; k < 2; k++) {
            var r = (k ? s4b : s4a).textRange;
            styleText(r, rgb(200, 30, 30));
            r.characterAttributes.strokeColor = gray(40); r.characterAttributes.strokeWeight = 7;
        }
        // 5. The opt-ins: an open path and a dashed square, both meant to be Outside.
        label('5', 'Dashed square: Outside, the dashes opt-in, correct. Zigzag: panel menu Align Selection Outside.', 340);
        var open = doc.pathItems.add();
        open.setEntirePath([[40, 200], [120, 300], [200, 200], [280, 300]]);
        open.filled = false; open.stroked = true; open.strokeWidth = 10; open.strokeColor = gray(60);
        open.strokeJoin = StrokeJoin.MITERENDJOIN; open.strokeMiterLimit = 4;
        var sq = doc.pathItems.rectangle(300, 340, 160, 160);
        sq.filled = true; sq.fillColor = rgb(30, 60, 200); sq.stroked = true; sq.strokeWidth = 8; sq.strokeColor = gray(40);
        sq.strokeDashes = [12, 6];
        var f = new File(args.outDir + '/StrokeMaster hand tests.ai');
        var so = new IllustratorSaveOptions(); so.pdfCompatible = true;
        doc.saveAs(f, so);
        doc.close(SaveOptions.DONOTSAVECHANGES);
        return 'DONE|' + f.exists;
    } catch (e) { return 'FAIL|' + String(e); }
    finally { app.userInteractionLevel = interaction; }
})();
