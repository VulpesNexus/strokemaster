// Builds heavy material for StrokeMaster's performance: pages of outlined text in a decorative
// font with thousands of corners (Kerberos Fang, when installed), outlined script, and live
// text, stroked Outside solid, dashed, and striped, and exports it as PDF with StrokeMaster off
// and at its defaults, twice each (the first export prepares each outline, the second reuses
// what was prepared), writing the switches' file (IseSettings.h) between exports and putting
// its previous contents back. Reports each export's time and size.
//
// Arguments: ISE_ARGS.out (folder), ISE_ARGS.settings (the switches' file),
// optionally ISE_ARGS.pages (default 4).
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var out = lib.args.out, settingsPath = lib.args.settings, pages = lib.args.pages || 4;
        var doc = lib.track(app.documents.add(false));
        var vp = doc.viewPreferences;
        vp.horizontalMeasurementUnits = MeasurementUnits.POINTS;
        vp.verticalMeasurementUnits = MeasurementUnits.POINTS;
        doc.documentPreferences.facingPages = false;
        doc.documentPreferences.pageWidth = 600;
        doc.documentPreferences.pageHeight = 800;
        while (doc.pages.length < pages) { doc.pages.add(); }
        var black = doc.swatches.itemByName('Black'), none = doc.swatches.itemByName('None');
        var fill = lib.color(doc, 'SM fill', [0, 12, 90, 0]);
        function findFont(part) {
            var all = app.fonts.everyItem().name;
            for (var q = 0; q < all.length; q++) { if (String(all[q]).indexOf(part) === 0) { return all[q]; } }
            return null;
        }
        var FANG = findFont('Kerberos Fang'), SCRIPT = lib.fontOk('Segoe Script\tRegular') ? 'Segoe Script\tRegular' : lib.FONT;
        var MINION = lib.FONT;
        function flat(items) {
            var r = [];
            for (var q = 0; q < items.length; q++) {
                if (items[q].constructor.name === 'Group') { r = r.concat(flat(items[q].pageItems.everyItem().getElements())); } else { r.push(items[q]); }
            }
            return r;
        }
        function frame(pg, y, h, text, font, size) {
            var tf = pg.textFrames.add({ geometricBounds: [y, 20, y + h, 580] });
            tf.textFramePreferences.insetSpacing = [0, 0, 0, 0];
            tf.contents = text;
            var t = tf.parentStory.texts[0];
            t.appliedFont = font; t.pointSize = size; t.fillColor = fill; t.strokeColor = black;
            return tf;
        }
        function paint(it, style) {
            it.fillColor = fill; it.strokeColor = black; it.strokeWeight = 3;
            it.strokeAlignment = StrokeAlignment.OUTSIDE_ALIGNMENT; it.endJoin = EndJoin.MITER_END_JOIN; it.miterLimit = 4;
            if (style) { it.strokeType = doc.strokeStyles.itemByName(style); }
        }
        var styles = [null, 'Dashed', 'Thick - Thin'];
        var counts = { outlined: 0, live: 0 };
        for (var p = 0; p < pages; p++) {
            var pg = doc.pages[p], y = 20;
            // Three lines of outlined decorative text, one per stroke style.
            for (var s = 0; s < styles.length; s++) {
                var items = flat(frame(pg, y, 70, 'Fangs and Fury', FANG || MINION, 44).createOutlines(true));
                for (var k = 0; k < items.length; k++) { paint(items[k], styles[s]); counts.outlined++; }
                y += 80;
            }
            // Three lines of outlined script.
            for (s = 0; s < styles.length; s++) {
                var sc = flat(frame(pg, y, 60, 'Ribbons and Rivers', SCRIPT, 40).createOutlines(true));
                for (k = 0; k < sc.length; k++) { paint(sc[k], styles[s]); counts.outlined++; }
                y += 70;
            }
            // A paragraph of live text with an Outside stroke, tight leading.
            var tf = frame(pg, y, 800 - y - 20, '', MINION, 18);
            var words = [];
            for (var w = 0; w < 160; w++) { words.push(['stroke', 'behind', 'fill', 'Typography', 'yWAVE'][w % 5]); }
            tf.contents = words.join(' ');
            var t = tf.parentStory.texts[0];
            t.appliedFont = MINION; t.pointSize = 18; t.leading = 17; t.fillColor = fill; t.strokeColor = black;
            t.strokeWeight = 2; t.strokeAlignment = TextStrokeAlign.OUTSIDE_ALIGNMENT; t.endJoin = EndJoin.MITER_END_JOIN;
            counts.live++;
        }
        new Folder(out).create();
        doc.save(new File(out + '/performance.indd'));

        var sf = new File(settingsPath), had = sf.exists, old = '';
        if (had) { sf.encoding = 'UTF-8'; sf.open('r'); old = sf.read(); sf.close(); }
        var hqp = app.pdfExportPresets.itemByName('[High Quality Print]');
        var runs = [['off', 'enabled=0\n', 1], ['off', 'enabled=0\n', 2], ['on', '', 1], ['on', '', 2]];
        var report = [];
        try {
            new Folder(sf.parent.fsName).create();
            for (var r = 0; r < runs.length; r++) {
                sf.encoding = 'UTF-8'; sf.lineFeed = 'Unix'; sf.open('w'); sf.write('# performance: ' + runs[r][0] + '\n' + runs[r][1]); sf.close();
                $.sleep(300);
                var f = new File(out + '/performance-' + runs[r][0] + '-' + runs[r][2] + '.pdf');
                var t0 = new Date().getTime();
                doc.exportFile(ExportFormat.PDF_TYPE, f, false, hqp);
                report.push(runs[r][0] + ' run ' + runs[r][2] + ': ' + (new Date().getTime() - t0) + ' ms, ' + f.length + ' bytes');
            }
        } finally {
            if (had) { sf.encoding = 'UTF-8'; sf.lineFeed = 'Unix'; sf.open('w'); sf.write(old); sf.close(); } else { sf.remove(); }
        }
        lib.L('font=' + (FANG || MINION) + ' pages=' + pages + ' outlined items=' + counts.outlined + ' live frames=' + counts.live + '\n' + report.join('\n'));
    });
})();
