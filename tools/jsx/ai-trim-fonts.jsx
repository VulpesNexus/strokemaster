// Trim glyph tips on the owner's fonts (retest P17, 2026-10-31): for each font found among
// args.fonts (name fragments), point text "rawa" (or args.text) at 72 pt, black fill and a red
// stroke of args.weight (default 12), corrected with Trim glyph tips off, then on, then off again;
// each stage's route-stroke limit and per-character limits, and a PNG. ISE_REGION_ARGS: outDir,
// runId, fonts (array), optional weight, text, limit, and object.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId) return 'FAIL|missing arguments';
    if (app.documents.length) return 'FAIL|requires zero preexisting documents';
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function rgb(r, g, b) { var c = new RGBColor(); c.red = r; c.green = g; c.blue = b; return c; }
    function limitOf(dump) {
        var m = /STROKE w [0-9.]+ join 0 limit ([0-9.]+)[^;]*EFFECT 'Adobe Outline Type'/.exec(dump) || /STROKE w [0-9.]+ join 0 limit ([0-9.]+) color [0-5] /.exec(dump);
        return m ? Number(m[1]) : null;
    }
    var interaction = app.userInteractionLevel, out = {fonts: {}, found: []}, doc = null;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        send('settings', 'defaults');
        send('settings', 'follow_edits=0');
        var wanted = args.fonts || [], fonts = [];
        for (var i = 0; i < app.textFonts.length; i++) {
            var f = app.textFonts[i];
            for (var k = 0; k < wanted.length; k++)
                if (f.name.toLowerCase().indexOf(String(wanted[k]).toLowerCase()) >= 0 && fonts.length < 12) { fonts.push(f); out.found.push(f.name + ' / ' + f.family + ' ' + f.style); break; }
        }
        var p = new DocumentPreset(); p.colorMode = DocumentColorSpace.RGB; p.width = 900; p.height = 160 * Math.max(1, fonts.length); p.units = RulerUnits.Points;
        doc = app.documents.addDocument('Web', p);
        doc.artboards[0].artboardRect = [0, p.height, 900, 0];
        for (var n = 0; n < fonts.length; n++) {
            // Three copies side by side: corrected with the switch off; on; and on, then off again.
            var row = {}, plans = [['off', ['glyph_tips=0']], ['on', ['glyph_tips=1']], ['off-again', ['glyph_tips=1', 'glyph_tips=0']]];
            for (var s = 0; s < 3; s++) {
                var t = doc.textFrames.add(); t.contents = args.text || 'rawa'; t.position = [20 + s * 290, p.height - 20 - n * 160];
                var a = t.textRange.characterAttributes;
                a.textFont = fonts[n]; a.size = 72; a.fillColor = rgb(0, 0, 0); a.strokeColor = rgb(190, 40, 20); a.strokeWeight = args.weight || 12;
                // args.limit: the characters' own miter limit first; args.object: no stroke on the
                // characters, an Appearance stroke above Characters instead.
                doc.selection = null; t.selected = true;
                if (args.limit) send('appearance', 'miter:' + args.limit);
                if (args.object) { a.strokeColor = new NoColor(); send('align-survey', 'stroke-above:' + (args.weight || 12)); }
                doc.selection = null;
                var c = '';
                for (var q = 0; q < plans[s][1].length; q++) {
                    send('settings', plans[s][1][q]);
                    doc.selection = null; t.selected = true;
                    c = send('correct');
                }
                doc.selection = null; t.selected = true;
                var d = send('style-dump');
                row[plans[s][0]] = {routeLimit: limitOf(d), charLimits: send('char-limits'), nativeFix: /"nativeFix":\{[^}]*\}/.exec(send('status'))[0]};
                doc.selection = null;
            }
            out.fonts[fonts[n].name] = row;
        }
        var o = new ExportOptionsPNG24(); o.artBoardClipping = true; o.antiAliasing = true;
        app.redraw(); doc.exportFile(lib.path(args, 'trim.png'), ExportType.PNG24, o);
        lib.write(args, 'trim.json', out);
        return 'DONE|' + fonts.length;
    } catch (e) { return 'FAIL|' + String(e) + ' line ' + e.line; }
    finally { app.userInteractionLevel = interaction; }
})();
