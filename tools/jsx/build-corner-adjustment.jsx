// Builds the corner adjustment study: each subject (simple corners, curves, overlapping shapes,
// outlined serif and script text, a point past the miter limit) stroked Outside with a dashed
// and a dotted stroke under each of InDesign's four corner adjustments (None, Dashes, Gaps,
// Dashes and Gaps). Saves the document and exports it with StrokeMaster off ("before") and at
// its defaults ("after"), writing the switches' file (IseSettings.h) between exports and
// putting its previous contents back. tools/compose_corner_adjustment.py sets the exports out
// as a grid.
//
// Arguments: ISE_ARGS.out (folder), ISE_ARGS.settings (the switches' file).
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var out = lib.args.out, settingsPath = lib.args.settings;
        var W = 640, X = 20, COL = 150, ROW = 105;
        var doc = lib.track(app.documents.add(false));
        var vp = doc.viewPreferences;
        vp.horizontalMeasurementUnits = MeasurementUnits.POINTS;
        vp.verticalMeasurementUnits = MeasurementUnits.POINTS;
        vp.rulerOrigin = RulerOrigin.PAGE_ORIGIN;
        doc.documentPreferences.facingPages = false;
        doc.documentPreferences.pageWidth = W;
        doc.documentPreferences.pageHeight = 4000;
        doc.zeroPoint = [0, 0];
        var page = doc.pages[0];
        page.marginPreferences.properties = { top: 0, left: 0, bottom: 0, right: 0 };
        var black = doc.swatches.itemByName('Black'), none = doc.swatches.itemByName('None');
        var fill = lib.color(doc, 'SM fill', [0, 12, 90, 0]);
        var labelStyle = doc.paragraphStyles.add({ name: 'SM label' });
        labelStyle.appliedFont = 'Minion Pro\tBold';
        labelStyle.pointSize = 9;
        labelStyle.fillColor = black;
        labelStyle.strokeColor = none;

        function font(name, fallback) { return lib.fontOk(name) ? name : fallback; }
        var MINION = 'Minion Pro\tRegular', MINION_B = font('Minion Pro\tBold', MINION);
        var SCRIPT = font('Segoe Script\tRegular', MINION);

        function label(x, y, text) {
            var tf = page.textFrames.add({ geometricBounds: [y, x, y + 12, x + COL * 4] });
            tf.textFramePreferences.insetSpacing = [0, 0, 0, 0];
            tf.contents = text;
            tf.parentStory.texts[0].applyParagraphStyle(labelStyle, true);
            return tf;
        }
        function flat(items) {
            var r = [];
            for (var q = 0; q < items.length; q++) {
                if (items[q].constructor.name === 'Group') { r = r.concat(flat(items[q].pageItems.everyItem().getElements())); } else { r.push(items[q]); }
            }
            return r;
        }
        function outlined(x, y1, text, fnt, size) {
            var tf = page.textFrames.add({ geometricBounds: [y1, x, y1 + size * 1.6, x + COL] });
            tf.textFramePreferences.insetSpacing = [0, 0, 0, 0];
            tf.contents = text;
            var t = tf.parentStory.texts[0];
            t.appliedFont = fnt;
            t.pointSize = size;
            return flat(tf.createOutlines(true));
        }
        function polygon(pts) {
            var p = page.polygons.add();
            p.paths[0].entirePath = pts;
            p.paths[0].pathType = PathType.CLOSED_PATH;
            return [p];
        }
        // The subjects, each drawn in a cell COL wide from (x, y).
        var subjects = [
            ['Square', function (x, y) { return polygon([[x + 40, y + 10], [x + 110, y + 10], [x + 110, y + 80], [x + 40, y + 80]]); }],
            ['Star (acute corners)', function (x, y) {
                var pts = [], cx = x + 75, cy = y + 47;
                for (var k = 0; k < 10; k++) {
                    var r = k % 2 ? 16 : 40, a = -Math.PI / 2 + k * Math.PI / 5;
                    pts.push([cx + r * Math.cos(a), cy + r * Math.sin(a)]);
                }
                return polygon(pts);
            }],
            ['Ellipse (smooth anchors only)', function (x, y) { return [page.ovals.add({ geometricBounds: [y + 15, x + 30, y + 80, x + 120] })]; }],
            ['Overlapping squares (one outline)', function (x, y) {
                var sq = page.polygons.add();
                sq.paths[0].entirePath = [[x + 30, y + 10], [x + 80, y + 10], [x + 80, y + 60], [x + 30, y + 60]];
                sq.paths[0].pathType = PathType.CLOSED_PATH;
                var p2 = sq.paths.add();
                p2.entirePath = [[x + 65, y + 35], [x + 115, y + 35], [x + 115, y + 85], [x + 65, y + 85]];
                p2.pathType = PathType.CLOSED_PATH;
                return [sq];
            }],
            ['Outlined serif text (Minion Bold)', function (x, y) { return outlined(x + 12, y + 5, 'MAW', MINION_B, 48); }],
            ['Outlined script (Segoe Script)', function (x, y) { return outlined(x + 6, y + 12, 'Ribbons', SCRIPT, 30); }],
            ['A point past the miter limit (12 degrees)', function (x, y) {
                var half = 6 * Math.PI / 180, h = 60, b = h * Math.tan(half);
                return polygon([[x + 75, y + 25], [x + 75 + b, y + 25 + h], [x + 75 - b, y + 25 + h]]);
            }],
        ];
        var adjustments = [['None', StrokeCornerAdjustment.NONE], ['Dashes', StrokeCornerAdjustment.DASHES],
                           ['Gaps', StrokeCornerAdjustment.GAPS], ['Dashes and gaps', StrokeCornerAdjustment.DASHES_AND_GAPS]];
        // Dashed takes the adjustment on the item; a custom dotted or dashed style carries it
        // in the style (InDesign refuses it on the item), so each gets one style per adjustment.
        function perStyle(collection, prefix, props) {
            var made = [];
            for (var q = 0; q < adjustments.length; q++) {
                props.name = prefix + ' ' + adjustments[q][0];
                props.strokeCornerAdjustment = adjustments[q][1];
                made.push(collection.add(props));
            }
            return function (it, ai) { it.strokeType = made[ai]; };
        }
        var styles = [
            ['Dashed (9, 3)', function (it, ai) {
                it.strokeType = doc.strokeStyles.itemByName('Dashed');
                it.strokeDashAndGap = [9, 3];
                it.strokeCornerAdjustment = adjustments[ai][1];
            }],
            ['A custom dotted style', perStyle(doc.dottedStrokeStyles, 'SM dots', { dotArray: [4] })],
            ['A custom dash style (10, 3, 2, 3)', perStyle(doc.dashedStrokeStyles, 'SM dashes', { dashArray: [10, 3, 2, 3] })],
        ];

        var cells = [], y = 20;
        for (var si = 0; si < styles.length; si++) {
            for (var bi = 0; bi < subjects.length; bi++) {
                var tf = label(X, y, styles[si][0] + ', ' + subjects[bi][0]);
                for (var ai = 0; ai < adjustments.length; ai++) {
                    var x = X + ai * COL, id = 'S' + si + 'B' + bi + 'A' + ai;
                    (function (style, subject, ai, x0, y0, cellId) {
                        lib.tryDo(cellId, function () {
                            var items = subject[1](x0, y0 + 16);
                            for (var q = 0; q < items.length; q++) {
                                var it = items[q];
                                it.fillColor = fill;
                                it.strokeColor = black;
                                it.strokeWeight = 3;
                                it.strokeAlignment = StrokeAlignment.OUTSIDE_ALIGNMENT;
                                it.endJoin = EndJoin.MITER_END_JOIN;
                                it.miterLimit = 4;
                                style[1](it, ai);
                            }
                        });
                    })(styles[si], subjects[bi], ai, x, y, id);
                    cells.push({ id: id, style: styles[si][0], subject: subjects[bi][0], adjust: adjustments[ai][0], col: ai, row: bi,
                                 section: si, label: tf, dy: 14 });
                }
                y += ROW + 16;
            }
        }

        // Resizing the page keeps its center: move everything back down so the first label is at
        // 20 again, then measure each cell from its label.
        doc.documentPreferences.pageHeight = y + 10;
        var shift = 20 - cells[0].label.geometricBounds[0], all = doc.spreads[0].pageItems.everyItem().getElements();
        for (var mi = 0; mi < all.length; mi++) { all[mi].move(undefined, [0, shift]); }
        for (var ci = 0; ci < cells.length; ci++) {
            var top = cells[ci].label.geometricBounds[0] + cells[ci].dy;
            var left = X + cells[ci].col * COL;
            cells[ci].bounds = [top, left, top + ROW, left + COL];
            cells[ci].title = cells[ci].label.contents;
            delete cells[ci].label;
        }
        new Folder(out).create();
        doc.save(new File(out + '/corner-adjustment.indd'));

        var sf = new File(settingsPath), had = sf.exists, old = '';
        if (had) { sf.encoding = 'UTF-8'; sf.open('r'); old = sf.read(); sf.close(); }
        var variants = [['before', 'enabled=0\n'], ['after', '']];
        var hqp = app.pdfExportPresets.itemByName('[High Quality Print]');
        try {
            new Folder(sf.parent.fsName).create();
            for (var v = 0; v < variants.length; v++) {
                sf.encoding = 'UTF-8';
                sf.lineFeed = 'Unix';
                sf.open('w'); sf.write('# corner adjustment export: ' + variants[v][0] + '\n' + variants[v][1]); sf.close();
                $.sleep(300);
                lib.timed('pdf ' + variants[v][0], function () { doc.exportFile(ExportFormat.PDF_TYPE, new File(out + '/corner-adjustment-' + variants[v][0] + '.pdf'), false, hqp); });
            }
        } finally {
            if (had) { sf.encoding = 'UTF-8'; sf.lineFeed = 'Unix'; sf.open('w'); sf.write(old); sf.close(); } else { sf.remove(); }
        }
        lib.writeText(out + '/corner-adjustment.json', lib.toJSON({ page: [W, y + 10], cells: cells, app: app.version }));
        lib.L('cells=' + cells.length + ' height=' + (y + 10));
    });
})();
