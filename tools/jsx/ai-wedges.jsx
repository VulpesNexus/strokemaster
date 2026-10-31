// Wedged corners on a spiky font (the owner's hand test 17, 2026-10-30): one miter limit for a
// whole frame (the stroke route's object stroke) against one per character. Three copies of the
// same point text (Kerberos Fang 72 pt, black fill, white 14 pt stroke) on a gray rectangle:
//   A  Correct Selection with the defaults: the stroke route, one limit for the frame
//   B  Strokes behind other letters' fills off: the characters keep their strokes, one limit each
//   C  the defaults with Trim glyph tips off
// Records native-fix's tallies after each (corners, kept beveled, cut mitered) and exports
// <runId>-native.pdf and <runId>-fixed.pdf. ISE_REGION_ARGS: outDir, runId; optional font.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId) return 'FAIL|missing arguments';
    if (app.documents.length) return 'FAIL|requires zero preexisting documents';
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    var interaction = app.userInteractionLevel, at = 'start', out = {};
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        var font = app.textFonts.getByName(args.font || 'KerberosFang');
        var p = new DocumentPreset(); p.colorMode = DocumentColorSpace.RGB; p.width = 900; p.height = 620; p.units = RulerUnits.Points;
        var doc = app.documents.addDocument('Web', p);
        doc.artboards[0].artboardRect = [0, 620, 900, 0];
        var bg = doc.pathItems.rectangle(620, 0, 900, 620); bg.filled = true; bg.fillColor = gray(60); bg.stroked = false;
        function frame(top) {
            var t = doc.textFrames.add(); t.contents = 'Lorem ipsum dolor sit amet,'; t.position = [30, top];
            var a = t.textRange.characterAttributes;
            a.textFont = font; a.size = 72; a.fillColor = gray(100); a.strokeColor = gray(0); a.strokeWeight = 14;
            return t;
        }
        var A = frame(600), B = frame(400), C = frame(200);
        app.redraw(); lib.pdf(doc, args, 'native');
        function correct(name, t, settings) {
            at = name;
            send('settings', 'defaults');
            for (var i = 0; i < settings.length; i++) send('settings', settings[i]);
            doc.selection = null; t.selected = true;
            var o = {correct: send('correct')};
            o.nativeFix = /"nativeFix":\{[^}]*\}/.exec(send('status'))[0];
            doc.selection = null; t.selected = true;
            o.limits = send('char-limits'); o.dump = send('style-dump');
            out[name] = o;
        }
        correct('A', A, []);
        correct('B', B, ['text_behind=0']);
        correct('C', C, ['glyph_tips=0']);
        send('settings', 'defaults');
        doc.selection = null; app.redraw(); lib.pdf(doc, args, 'fixed');
        doc.close(SaveOptions.DONOTSAVECHANGES);
        lib.write(args, 'results.json', out);
        return 'DONE';
    } catch (e) {
        out.error = String(e); out.stage = at;
        try { lib.write(args, 'results.json', out); } catch (w) {}
        return 'FAIL|stage=' + at + '|error=' + String(e);
    } finally { app.userInteractionLevel = interaction; }
})();
