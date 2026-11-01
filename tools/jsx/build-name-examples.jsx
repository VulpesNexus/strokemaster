// The plugin's name set in fonts whose strokes go wrong in InDesign, for the presentation video
// (tools/make_promo_video.py): a threaded frame whose strokes cover the frame before it, miter
// spikes, long glyph tips, script outlines whose outward offset floods, overlapping contours
// whose translucent strokes darken where they cross, and dashes that cross where letters touch;
// last, a logo of two stacked lines, and the logo's name without its stroke in black, on two lines
// and on one (the video's end, where the logo settles on one line). Exported twice: StrokeMaster off ('none') and at its
// defaults ('defaults'), by writing the switches' file between exports, whose previous contents
// are put back afterward.
//
// Arguments: ISE_ARGS.out (folder), ISE_ARGS.settings (the switches' file).
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var out = lib.args.out, settingsPath = lib.args.settings;
        var W = 760, X = 20, BOX = 520;   // wide enough for the one-line name
        var cells = [];
        var doc = lib.track(app.documents.add(false));
        var vp = doc.viewPreferences;
        vp.horizontalMeasurementUnits = MeasurementUnits.POINTS;
        vp.verticalMeasurementUnits = MeasurementUnits.POINTS;
        vp.rulerOrigin = RulerOrigin.PAGE_ORIGIN;
        doc.documentPreferences.facingPages = false;
        doc.documentPreferences.pageWidth = W;
        doc.documentPreferences.pageHeight = 2000;
        doc.zeroPoint = [0, 0];
        var page = doc.pages[0];
        page.marginPreferences.properties = { top: 0, left: 0, bottom: 0, right: 0 };
        var ink = lib.color(doc, 'FX ink', [90, 75, 15, 40]);       // navy
        var stroke = lib.color(doc, 'FX stroke', [0, 45, 100, 0]);  // amber
        var accent = lib.color(doc, 'FX accent', [70, 0, 30, 0]);   // teal

        function font(name, fallback) { return lib.fontOk(name) ? name : fallback; }
        function findFont(part) {
            var all = app.fonts.everyItem().name;
            for (var q = 0; q < all.length; q++) { if (String(all[q]).indexOf(part) === 0) { return all[q]; } }
            return null;
        }
        var MINION = 'Minion Pro\tRegular', MINION_B = font('Minion Pro\tBold', MINION);
        var SCRIPT = font('Segoe Script\tRegular', MINION), SCRIPT_B = font('Segoe Script\tBold', SCRIPT);
        var VIBES = font('Great Vibes\tRegular', SCRIPT), EAGLE = findFont('Eagle Lake') || SCRIPT_B;
        var BAHN = font('Bahnschrift\tBold', findFont('Bahnschrift') || MINION_B);

        var y = 20;
        function cell(key, h, build) {
            lib.tryDo(key, function () { build(X, y); });
            cells.push({ key: key, top: y, h: h });
            y += h + 60;
        }
        function flat(items) {
            var r = [];
            for (var q = 0; q < items.length; q++) {
                if (items[q].constructor.name === 'Group') { r = r.concat(flat(items[q].pageItems.everyItem().getElements())); } else { r.push(items[q]); }
            }
            return r;
        }
        function frame(x, y1, h, text, fnt, size) {
            var tf = page.textFrames.add({ geometricBounds: [y1, x, y1 + h, x + BOX] });
            tf.textFramePreferences.insetSpacing = [0, 0, 0, 0];
            tf.contents = text;
            var t = tf.parentStory.texts[0];
            t.appliedFont = fnt;
            t.pointSize = size;
            t.fillColor = ink;
            t.strokeColor = stroke;
            t.hyphenation = false;
            return tf;
        }
        function textStroke(tf, w, join, tracking, limit) {
            var t = tf.parentStory.texts[0];
            t.strokeWeight = w;
            t.strokeAlignment = TextStrokeAlign.OUTSIDE_ALIGNMENT;
            t.endJoin = join;
            t.miterLimit = limit || 4;
            if (tracking) { t.tracking = tracking; }
            return tf;
        }
        function paint(it, w, extra) {
            it.fillColor = ink;
            it.strokeColor = stroke;
            it.strokeWeight = w;
            it.strokeAlignment = StrokeAlignment.OUTSIDE_ALIGNMENT;
            it.endJoin = EndJoin.MITER_END_JOIN;
            it.miterLimit = 4;
            if (extra) { extra(it); }
        }
        function outlined(x, y1, h, text, fnt, size, w, extra) {
            var items = flat(frame(x, y1, h, text, fnt, size).createOutlines(true));
            for (var q = 0; q < items.length; q++) { paint(items[q], w, extra); }
        }
        function dashed(it) {
            it.strokeType = doc.strokeStyles.itemByName('Dashed');
            it.strokeDashAndGap = [10, 5];
            it.strokeCornerAdjustment = StrokeCornerAdjustment.NONE;
        }
        var MITER = EndJoin.MITER_END_JOIN, ROUND = EndJoin.ROUND_END_JOIN;

        cell('threaded', 150, function (x, y1) {
            // 'Stroke' fills the first frame; 'Master' flows into the second, which overlaps it.
            var a = frame(x, y1, 80, 'Stroke\rMaster', MINION_B, 84);
            var b = page.textFrames.add({ geometricBounds: [y1 + 52, x + 40, y1 + 160, x + BOX] });
            b.textFramePreferences.insetSpacing = [0, 0, 0, 0];
            a.nextTextFrame = b;
            textStroke(a, 12, ROUND, -10);
            a.parentStory.paragraphs[1].fillColor = accent;
        });
        cell('spikes', 90, function (x, y1) {
            textStroke(frame(x, y1, 110, 'StrokeMaster', MINION, 86), 12, MITER, 0, 10);
        });
        cell('tips', 100, function (x, y1) {
            textStroke(frame(x, y1, 130, 'StrokeMaster', EAGLE, 64), 9, MITER);
        });
        cell('script', 110, function (x, y1) {
            outlined(x, y1, 140, 'StrokeMaster', VIBES, 100, 6);
        });
        cell('overlaps', 100, function (x, y1) {
            // Bahnschrift's contours overlap: a translucent stroke darkens where they cross.
            outlined(x, y1, 120, 'StrokeMaster', BAHN, 84, 8, function (it) { it.strokeTransparencySettings.blendingSettings.opacity = 60; });
        });
        cell('dashes', 130, function (x, y1) {
            // Great Vibes's letters overlap where they join: each contour dashed on its own, so
            // the dashes cross inside the word.
            var tf = frame(x, y1, 176, 'StrokeMaster', VIBES, 110);
            tf.geometricBounds = [y1, x, y1 + 176, x + W - 2 * x];
            tf.parentStory.texts[0].tracking = -40;
            var items = flat(tf.createOutlines(true));
            for (var q = 0; q < items.length; q++) { paint(items[q], 6, dashed); }
        });
        cell('logo', 190, function (x, y1) {
            // Two stacked lines: the second line's stroke over the first's fills, spikes at the joints.
            var tf = textStroke(frame(x, y1, 220, 'Stroke\rMaster', MINION_B, 110), 14, MITER, -20, 10);
            tf.parentStory.texts[0].leading = 72;
            tf.parentStory.paragraphs[1].fillColor = accent;
        });
        // The logo's lines without stroke, in black: as stacked in the logo, and on one line.
        function plain(x, y1, h, w, text) {
            var tf = page.textFrames.add({ geometricBounds: [y1, x, y1 + h, x + w] });
            tf.textFramePreferences.insetSpacing = [0, 0, 0, 0];
            tf.contents = text;
            var t = tf.parentStory.texts[0];
            t.appliedFont = MINION_B;
            t.pointSize = 110;
            t.tracking = -20;
            t.hyphenation = false;
            t.fillColor = doc.swatches.itemByName('Black');
            t.strokeColor = doc.swatches.itemByName('None');
            return tf;
        }
        cell('plain2', 190, function (x, y1) {
            plain(x, y1, 220, BOX, 'Stroke\rMaster').parentStory.texts[0].leading = 72;
        });
        cell('plain1', 100, function (x, y1) {
            plain(x, y1, 130, W - 2 * x, 'StrokeMaster');
        });

        doc.documentPreferences.pageHeight = y + 10;
        // Resizing the page keeps its center: everything moved back by the same amount.
        var grow = (2000 - (y + 10)) / 2, all = doc.spreads[0].pageItems.everyItem().getElements();
        for (var mi = 0; mi < all.length; mi++) { all[mi].move(undefined, [0, grow]); }
        new Folder(out).create();
        doc.save(new File(out + '/name-examples.indd'));

        // 'holes': the defaults with Close holes inside the outline on.
        var variants = [['defaults', ''], ['none', 'enabled=0\n'], ['holes', 'close_holes=1\n']];
        var sf = new File(settingsPath), had = sf.exists, old = '';
        if (had) { sf.encoding = 'UTF-8'; sf.open('r'); old = sf.read(); sf.close(); }
        var hqp = app.pdfExportPresets.itemByName('[High Quality Print]');
        try {
            new Folder(sf.parent.fsName).create();
            for (var v = 0; v < variants.length; v++) {
                sf.encoding = 'UTF-8';
                sf.lineFeed = 'Unix';
                sf.open('w'); sf.write('# name examples: ' + variants[v][0] + '\n' + variants[v][1]); sf.close();
                $.sleep(1500);   // a new write time, which the engine compares at each spread
                lib.timed('pdf ' + variants[v][0], function () { doc.exportFile(ExportFormat.PDF_TYPE, new File(out + '/name-' + variants[v][0] + '.pdf'), false, hqp); });
            }
        } finally {
            if (had) { sf.encoding = 'UTF-8'; sf.lineFeed = 'Unix'; sf.open('w'); sf.write(old); sf.close(); } else { sf.remove(); }
        }
        lib.writeText(out + '/name-examples.json', lib.toJSON({ page: [W, y + 10], cells: cells, app: app.version }));
        lib.L('cells=' + cells.length + ' height=' + (y + 10));
    });
})();
