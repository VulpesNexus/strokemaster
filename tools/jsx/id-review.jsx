// The review of 2026-11-01, host-tested in InDesign: one document, exported once per set of
// switches by writing the switches' file (IseSettings.h) between exports.
//   tips: Eagle Lake "vwkz", 72 pt, an 8 pt Miter stroke, Outside and Center, limit 4.
//     tips-a.pdf  text_miter=0 glyph_tips=0   (InDesign's miters)
//     tips-b.pdf  text_miter=0 glyph_tips=1   (Trim glyph tips alone: tips trimmed, spikes kept)
//     tips-c.pdf  the defaults
//   clip: a story threaded across two overlapping frames, Outside 6 pt, in a TrueType font, a CFF
//   font, and Bahnschrift (overlapping contours), on one page each with the tips.
//     clip-on.pdf  the defaults;  clip-off.pdf  text_story=0
//   bounds: a thin triangle with a whole point, 50% object opacity.
//     bounds-fg.pdf  exported in the foreground;  bounds-bg.pdf  in the background
//   hollow: "Bog" with no fill and an Outside stroke (the second review).
//     holes-on.pdf  close_holes=1;  the defaults' pages show it off
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
        doc.documentPreferences.pageWidth = 640;
        doc.documentPreferences.pageHeight = 700;
        var page = doc.pages[0];
        page.marginPreferences.properties = { top: 0, left: 0, bottom: 0, right: 0 };
        var ink = lib.color(doc, 'RV ink', [90, 75, 15, 40]), stroke = lib.color(doc, 'RV stroke', [0, 45, 100, 0]);
        function findFont(part) {
            var all = app.fonts.everyItem().name;
            for (var q = 0; q < all.length; q++) { if (String(all[q]).indexOf(part) === 0) { return all[q]; } }
            return null;
        }
        function frame(x, y, w, h, text, fnt, size) {
            var tf = page.textFrames.add({ geometricBounds: [y, x, y + h, x + w] });
            tf.textFramePreferences.insetSpacing = [0, 0, 0, 0];
            tf.contents = text;
            var t = tf.parentStory.texts[0];
            t.appliedFont = fnt; t.pointSize = size; t.fillColor = ink; t.strokeColor = stroke;
            return tf;
        }
        function textStroke(tf, align, w) {
            var t = tf.parentStory.texts[0];
            t.strokeWeight = w; t.strokeAlignment = align; t.endJoin = EndJoin.MITER_END_JOIN; t.miterLimit = 4;
        }
        var fonts = { eagle: findFont('Eagle Lake'), tt: findFont('Times New Roman') || findFont('Georgia'),
                      cff: findFont('Minion Pro'), variable: findFont('Bahnschrift') };
        // Tips, top row.
        if (fonts.eagle) {
            textStroke(frame(20, 20, 290, 100, 'vwkz', fonts.eagle, 72), TextStrokeAlign.OUTSIDE_ALIGNMENT, 8);
            textStroke(frame(330, 20, 290, 100, 'vwkz', fonts.eagle, 72), TextStrokeAlign.CENTER_ALIGNMENT, 8);
        }
        // ISE_ARGS.place: an InDesign document placed first, in a frame at the right, so it is drawn
        // before the stories (a placed page may draw as a spread nested in the page's own draw).
        if (lib.args.place) {
            var holder = page.rectangles.add({ geometricBounds: [140, 480, 560, 630] });
            holder.place(new File(lib.args.place));
            holder.fit(FitOptions.PROPORTIONALLY);
        }
        // Threaded overlapping frames, one row per font.
        var rows = ['tt', 'cff', 'variable'], y = 150;
        for (var r = 0; r < rows.length; r++) {
            var f = fonts[rows[r]];
            if (!f) continue;
            var a = frame(20, y, 600, 56, 'Hogwash\rfjord', f, 56);
            var b = page.textFrames.add({ geometricBounds: [y + 34, 60, y + 120, 620] });
            b.textFramePreferences.insetSpacing = [0, 0, 0, 0];
            a.nextTextFrame = b;
            textStroke(a, TextStrokeAlign.OUTSIDE_ALIGNMENT, 6);
            y += 140;
        }
        // Hollow text: no fill, an Outside stroke; Close holes fills only its holes.
        if (fonts.cff) {
            var hollow = frame(20, 580, 300, 110, 'Bog', fonts.cff, 90);
            hollow.parentStory.texts[0].fillColor = doc.swatches.itemByName('None');
            textStroke(hollow, TextStrokeAlign.OUTSIDE_ALIGNMENT, 1.5);
        }
        // A translucent thin triangle with a whole point.
        var t = page.polygons.add();
        t.paths[0].entirePath = [[540, 590], [570, 690], [510, 690]];
        t.paths[0].pathType = PathType.CLOSED_PATH;
        t.fillColor = ink; t.strokeColor = stroke; t.strokeWeight = 6; t.strokeAlignment = StrokeAlignment.OUTSIDE_ALIGNMENT;
        t.endJoin = EndJoin.MITER_END_JOIN; t.miterLimit = 4;
        t.transparencySettings.blendingSettings.opacity = 50;
        new Folder(out).create();

        var sf = new File(settingsPath), had = sf.exists, old = '';
        if (had) { sf.encoding = 'UTF-8'; sf.open('r'); old = sf.read(); sf.close(); }
        var hqp = app.pdfExportPresets.itemByName('[High Quality Print]');
        function settings(text) {
            sf.encoding = 'UTF-8'; sf.lineFeed = 'Unix';
            sf.open('w'); sf.write('# review\n' + text); sf.close();
            $.sleep(1500);   // a new write time, which the engine compares at each spread
        }
        function pdf(name) { doc.exportFile(ExportFormat.PDF_TYPE, new File(out + '/' + name + '.pdf'), false, hqp); }
        try {
            settings('text_miter=0\nglyph_tips=0\n'); pdf('tips-a');
            settings('text_miter=0\nglyph_tips=1\n'); pdf('tips-b');
            settings(''); pdf('tips-c');
            pdf('clip-on');
            pdf('bounds-fg');
            var task = doc.asynchronousExportFile(ExportFormat.PDF_TYPE, new File(out + '/bounds-bg.pdf'), false, hqp);
            task.waitForTask();
            settings('close_holes=1\n'); pdf('holes-on');
            settings('text_story=0\n'); pdf('clip-off');
        } finally {
            if (had) { sf.encoding = 'UTF-8'; sf.lineFeed = 'Unix'; sf.open('w'); sf.write(old); sf.close(); } else { sf.remove(); }
        }
        var names = [];
        for (var k in fonts) if (fonts.hasOwnProperty(k)) names.push(k + '=' + fonts[k]);
        lib.writeText(out + '/review.json', lib.toJSON({ fonts: names, app: app.version }));
        lib.L('fonts ' + names.join('; '));
    });
})();
