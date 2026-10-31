// Close holes (region-prep-33, the owner's request of 2026-10-31: "close all holes"): for each font
// found among ISE_REGION_ARGS.fonts (name fragments), args.text (default "uoazRtyu") at 72 pt, black
// fill and a red stroke of args.weight (default 12), three copies corrected by Correct Selection:
// with close_holes off, on, and on then off again (the fill taken off). Each copy's style dump,
// then a PNG of the artboard. ISE_REGION_ARGS: outDir, runId, fonts; optional text, weight, scale (PNG, %), expectedBuild.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId) return 'FAIL|missing arguments';
    if (app.documents.length) return 'FAIL|requires zero preexisting documents';
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function rgb(r, g, b) { var c = new RGBColor(); c.red = r; c.green = g; c.blue = b; return c; }
    var interaction = app.userInteractionLevel, out = {fonts: {}, found: []};
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        out.build = /"build":"[^"]*"/.exec(send('status'))[0];
        if (args.expectedBuild && out.build !== '"build":"' + args.expectedBuild + '"') throw Error('unexpected plugin build');
        send('settings', 'defaults');
        send('settings', 'follow_edits=0');
        var wanted = args.fonts || ['gorefont'], fonts = [];
        for (var i = 0; i < app.textFonts.length; i++) {
            var f = app.textFonts[i];
            for (var k = 0; k < wanted.length; k++)
                if (f.name.toLowerCase().indexOf(String(wanted[k]).toLowerCase()) >= 0 && fonts.length < 8) { fonts.push(f); out.found.push(f.name); break; }
        }
        var p = new DocumentPreset(); p.colorMode = DocumentColorSpace.RGB; p.width = 1300; p.height = 60 + 130 * Math.max(1, fonts.length); p.units = RulerUnits.Points;
        var doc = app.documents.addDocument('Web', p);
        doc.artboards[0].artboardRect = [0, p.height, 1300, 0];
        var plans = [['off', ['close_holes=0']], ['on', ['close_holes=1']], ['off-again', ['close_holes=1', 'close_holes=0']]];
        for (var n = 0; n < fonts.length; n++) {
            var row = {};
            for (var s = 0; s < 3; s++) {
                var t = doc.textFrames.add(); t.contents = args.text || 'uoazRtyu'; t.position = [20 + s * 430, p.height - 110 - n * 130];
                var a = t.textRange.characterAttributes;
                a.textFont = fonts[n]; a.size = 72; a.fillColor = rgb(10, 10, 10); a.strokeColor = rgb(190, 40, 20); a.strokeWeight = args.weight || 12;
                for (var q = 0; q < plans[s][1].length; q++) {
                    send('settings', plans[s][1][q]);
                    doc.selection = null; t.selected = true;
                    send('correct');
                }
                doc.selection = null; t.selected = true;
                row[plans[s][0]] = {dump: send('style-dump'), nativeFix: /"nativeFix":\{[^}]*\}/.exec(send('status'))[0]};
                doc.selection = null;
            }
            out.fonts[fonts[n].name] = row;
        }
        var o = new ExportOptionsPNG24(); o.artBoardClipping = true; o.antiAliasing = true;
        o.horizontalScale = o.verticalScale = args.scale || 100;
        app.redraw(); doc.exportFile(lib.path(args, 'holes.png'), ExportType.PNG24, o);
        // Live effects run lazily: read after the redraw and the export.
        out.effect = /"closeHoles":\{[^}]*\}/.exec(send('status'))[0];
        lib.write(args, 'holes.json', out);
        return 'DONE|' + fonts.length;
    } catch (e) { return 'FAIL|' + String(e) + ' line ' + e.line; }
    finally { app.userInteractionLevel = interaction; }
})();
