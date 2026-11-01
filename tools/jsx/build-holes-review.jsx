// The owner's retest BB1 (2026-11-01), in InDesign: Close holes across a frame's lines and across
// separate stories. On a gray page, a two-line frame "VWAMNvwkyzK" and a separate story "gakak"
// overlapping its second line, Minion Pro 72 pt, a white 10 pt Outside Miter stroke, limit 4.
// Exported with Close holes off (holes-off.pdf) and on (holes-on.pdf), with Across separate
// stories on, by writing the switches' file between exports.
// Arguments: ISE_ARGS.out (folder), ISE_ARGS.settings (the switches' file).
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var out = lib.args.out, settingsPath = lib.args.settings;
        var doc = lib.track(app.documents.add(false));
        var vp = doc.viewPreferences;
        vp.horizontalMeasurementUnits = MeasurementUnits.POINTS;
        vp.verticalMeasurementUnits = MeasurementUnits.POINTS;
        vp.rulerOrigin = RulerOrigin.PAGE_ORIGIN;
        doc.documentPreferences.facingPages = false;
        doc.documentPreferences.pageWidth = 560;
        doc.documentPreferences.pageHeight = 330;
        var page = doc.pages[0];
        page.marginPreferences.properties = { top: 0, left: 0, bottom: 0, right: 0 };
        var ink = lib.color(doc, 'HR ink', [0, 0, 0, 100]), white = lib.color(doc, 'HR white', [0, 0, 0, 0]);
        var gray = lib.color(doc, 'HR gray', [0, 0, 0, 45]);
        var back = page.rectangles.add({ geometricBounds: [0, 0, 330, 560] });
        back.fillColor = gray; back.strokeWeight = 0;
        var all = app.fonts.everyItem().name, minion = null;
        for (var q = 0; q < all.length; q++) { if (String(all[q]).indexOf('Minion Pro\tRegular') === 0) { minion = all[q]; break; } }
        if (!minion) for (var r = 0; r < all.length; r++) { if (String(all[r]).indexOf('Minion Pro') === 0) { minion = all[r]; break; } }
        function frame(x, y, w, h, text) {
            var tf = page.textFrames.add({ geometricBounds: [y, x, y + h, x + w] });
            tf.textFramePreferences.insetSpacing = [0, 0, 0, 0];
            tf.contents = text;
            var t = tf.parentStory.texts[0];
            t.appliedFont = minion; t.pointSize = 72; t.leading = 62; t.fillColor = ink; t.strokeColor = white;
            t.strokeWeight = 10; t.strokeAlignment = TextStrokeAlign.OUTSIDE_ALIGNMENT; t.endJoin = EndJoin.MITER_END_JOIN; t.miterLimit = 4;
            return tf;
        }
        frame(20, 30, 520, 140, 'VWAMNvwkyzK\rVWAMNvwkyzK');
        frame(20, 150, 300, 90, 'gakak');
        var variants = [['holes-off', 'text_stories=1\nclose_holes=0\n'], ['holes-on', 'text_stories=1\nclose_holes=1\n']];
        var sf = new File(settingsPath), had = sf.exists, old = '';
        if (had) { sf.encoding = 'UTF-8'; sf.open('r'); old = sf.read(); sf.close(); }
        var hqp = app.pdfExportPresets.itemByName('[High Quality Print]');
        try {
            new Folder(out).create();
            for (var v = 0; v < variants.length; v++) {
                sf.encoding = 'UTF-8';
                sf.lineFeed = 'Unix';
                sf.open('w'); sf.write('# holes review: ' + variants[v][0] + '\n' + variants[v][1]); sf.close();
                $.sleep(1500);   // a new write time, which the engine compares at each spread
                lib.timed('pdf ' + variants[v][0], function () { doc.exportFile(ExportFormat.PDF_TYPE, new File(out + '/' + variants[v][0] + '.pdf'), false, hqp); });
            }
        } finally {
            if (had) { sf.encoding = 'UTF-8'; sf.lineFeed = 'Unix'; sf.open('w'); sf.write(old); sf.close(); } else { sf.remove(); }
        }
        lib.L('cells=2 font=' + minion);
    });
})();
