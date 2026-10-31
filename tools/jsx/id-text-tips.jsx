// Trim glyph tips on live text in InDesign (the owner's answer of 2026-10-31: both applications).
// In an invisible document, for each font found among ISE_ARGS.fonts (name fragments): "rawa" (or
// ISE_ARGS.text) at 72 pt, black fill and a red 6 pt stroke (or ISE_ARGS.weight), one row Center
// and one Outside. Exported to PDF twice,
// with ISE_ARGS.settings (the file STROKEMASTER_SETTINGS names) written glyph_tips=1, then 0:
// tips-on.pdf and tips-off.pdf in ISE_ARGS.outDir; then with close_holes=1 too (region-prep-33's Close holes),
// holes-on.pdf. With ISE_ARGS.outline, each frame is outlined first (createOutlines), so the
// shapes route draws the same letters. Returns the fonts found.
(function () {
    // ise-lib.jsx, loaded first by run-jsx.ps1, moves the arguments to ISE_LIB.args.
    var a = ($.global.ISE_LIB && $.global.ISE_LIB.args) || $.global.ISE_ARGS || {};
    if (!a.outDir || !a.settings || !a.fonts) return 'FAIL|missing arguments';
    var level = app.scriptPreferences.userInteractionLevel, doc = null, found = [];
    var text = a.text || 'rawa', weight = Number(a.weight) || 6;
    app.scriptPreferences.userInteractionLevel = UserInteractionLevels.NEVER_INTERACT;
    function settings(tips, holes) {
        var f = new File(a.settings);
        f.encoding = 'UTF-8';
        if (!f.open('w')) throw Error('cannot write the test settings');
        f.write('glyph_tips=' + tips + '\nclose_holes=' + (holes ? 1 : 0) + '\n');
        f.close();
        $.sleep(1500);   // a new write time, which the engine compares at each spread
    }
    function color(name, r, g, b) {
        var c = doc.colors.add(); c.name = name; c.model = ColorModel.PROCESS; c.space = ColorSpace.RGB; c.colorValue = [r, g, b];
        return c;
    }
    try {
        // A document this test left when its InDesign was killed, recovered at this launch: only
        // its own frames in it, closed unsaved. Nothing else is touched.
        for (var d = app.documents.length - 1; d >= 0; d--) {
            var r0 = app.documents[d], ours = r0.recovered && r0.textFrames.length > 0;
            for (var q = 0; ours && q < r0.textFrames.length; q++) ours = r0.textFrames[q].contents === text;
            if (ours) r0.close(SaveOptions.NO);
        }
        // Every font's name in one call: a property read per font took minutes on a large library.
        var names = app.fonts.everyItem().name;
        for (var i = 0; i < names.length && found.length < 8; i++) {
            var name = String(names[i]).toLowerCase();
            for (var k = 0; k < a.fonts.length; k++)
                if (name.indexOf(String(a.fonts[k]).toLowerCase()) >= 0) { found.push(app.fonts[i]); break; }
        }
        doc = app.documents.add(false);
        doc.documentPreferences.pageWidth = '640pt'; doc.documentPreferences.pageHeight = (40 + found.length * 120) + 'pt';
        doc.viewPreferences.horizontalMeasurementUnits = MeasurementUnits.POINTS; doc.viewPreferences.verticalMeasurementUnits = MeasurementUnits.POINTS;
        var red = color('tips-red', 190, 40, 20), black = doc.swatches.itemByName('Black');
        var page = doc.pages[0];
        for (var n = 0; n < found.length; n++) {
            for (var s = 0; s < 2; s++) {
                var y = 20 + n * 120, x = 20 + s * 310;
                var t = page.textFrames.add({geometricBounds: [y, x, y + 110, x + 300]});
                t.contents = text;
                var r = t.texts[0];
                r.appliedFont = found[n]; r.pointSize = 72; r.fillColor = black; r.strokeColor = red; r.strokeWeight = weight;
                r.endJoin = EndJoin.MITER_END_JOIN; r.miterLimit = 4;
                r.strokeAlignment = s === 0 ? TextStrokeAlign.CENTER_ALIGNMENT : TextStrokeAlign.OUTSIDE_ALIGNMENT;
                if (!a.outline) continue;
                // Outlining keeps neither the alignment nor an Outside stroke: set on each shape.
                var shapes = t.createOutlines(true), all = [];
                for (var o = 0; o < shapes.length; o++) all = all.concat([shapes[o]], shapes[o].allPageItems);
                for (var q = 0; q < all.length; q++) {
                    if (all[q].constructor.name !== 'Polygon') continue;
                    all[q].fillColor = black; all[q].strokeColor = red; all[q].strokeWeight = weight;
                    all[q].endJoin = EndJoin.MITER_END_JOIN; all[q].miterLimit = 4;
                    all[q].strokeAlignment = s === 0 ? StrokeAlignment.CENTER_ALIGNMENT : StrokeAlignment.OUTSIDE_ALIGNMENT;
                }
            }
        }
        var preset = app.pdfExportPresets.itemByName('[High Quality Print]');
        settings(1);
        doc.exportFile(ExportFormat.PDF_TYPE, new File(a.outDir + '/tips-on.pdf'), false, preset);
        settings(0);
        doc.exportFile(ExportFormat.PDF_TYPE, new File(a.outDir + '/tips-off.pdf'), false, preset);
        settings(0, true);
        doc.exportFile(ExportFormat.PDF_TYPE, new File(a.outDir + '/holes-on.pdf'), false, preset);
        var names = [];
        for (var m = 0; m < found.length; m++) names.push(found[m].name);
        return 'DONE|' + names.join(';');
    } catch (e) {
        return 'FAIL|' + e.message + ' line ' + e.line;
    } finally {
        if (doc && doc.isValid) doc.close(SaveOptions.NO);
        app.scriptPreferences.userInteractionLevel = level;
    }
})();
