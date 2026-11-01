// Builds one example per StrokeMaster switch, for the feature illustrations
// (tools/compose_feature_examples.py): each cell shows the defect its switch tackles, drawn large
// in contrasting colors. The document is exported once per switch with that switch off and the
// others at their defaults, once with the defaults, and once more with each switch that is off
// by default turned on, by writing the switches' file (IseSettings.h) between exports. The
// file's previous contents are put back afterward.
//
// Arguments: ISE_ARGS.out (folder), ISE_ARGS.settings (the switches' file).
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var out = lib.args.out, settingsPath = lib.args.settings;
        var W = 340, X = 20, BOX = 300;
        var cells = [];
        var doc = lib.track(app.documents.add(false));
        var vp = doc.viewPreferences;
        vp.horizontalMeasurementUnits = MeasurementUnits.POINTS;
        vp.verticalMeasurementUnits = MeasurementUnits.POINTS;
        vp.rulerOrigin = RulerOrigin.PAGE_ORIGIN;
        doc.documentPreferences.facingPages = false;
        doc.documentPreferences.pageWidth = W;
        doc.documentPreferences.pageHeight = 4800;
        doc.zeroPoint = [0, 0];
        var page = doc.pages[0];
        page.marginPreferences.properties = { top: 0, left: 0, bottom: 0, right: 0 };
        var none = doc.swatches.itemByName('None');
        var ink = lib.color(doc, 'FX ink', [90, 75, 15, 40]);       // the fills: deep navy
        var stroke = lib.color(doc, 'FX stroke', [0, 45, 100, 0]);  // the strokes: amber
        var accent = lib.color(doc, 'FX accent', [70, 0, 30, 0]);   // a second fill color: teal

        function font(name, fallback) { return lib.fontOk(name) ? name : fallback; }
        function findFont(part) {
            var all = app.fonts.everyItem().name;
            for (var q = 0; q < all.length; q++) { if (String(all[q]).indexOf(part) === 0) { return all[q]; } }
            return null;
        }
        var MINION = 'Minion Pro\tRegular', MINION_B = font('Minion Pro\tBold', MINION);
        var SCRIPT = font('Segoe Script\tRegular', MINION), SCRIPT_B = font('Segoe Script\tBold', SCRIPT);
        var VIBES = font('Great Vibes\tRegular', SCRIPT), EAGLE = findFont('Eagle Lake') || SCRIPT_B;
        var FANG = findFont('Kerberos Fang');

        var y = 20;
        // One cell `h` points high per switch: `key` names it in the switches' file, `on` says
        // whether it is on by default; `build(x, y)` draws its items.
        // `base`: switches set in both of its exports, when another switch would hide its effect.
        function cell(key, on, h, build, base) {
            lib.tryDo(key, function () { build(X, y); });
            cells.push({ key: key, on: on, top: y, h: h, base: base || '' });
            y += h + 70;
        }
        function flat(items) {
            var r = [];
            for (var q = 0; q < items.length; q++) {
                if (items[q].constructor.name === 'Group') { r = r.concat(flat(items[q].pageItems.everyItem().getElements())); } else { r.push(items[q]); }
            }
            return r;
        }
        function paint(it, align, w, join, extra, limit) {
            it.fillColor = ink;
            it.strokeColor = stroke;
            it.strokeWeight = w;
            it.strokeAlignment = align;
            it.endJoin = join || EndJoin.MITER_END_JOIN;
            it.miterLimit = limit || 4;
            if (extra) { extra(it); }
            return it;
        }
        function frame(x, y1, w, h, text, fnt, size) {
            var tf = page.textFrames.add({ geometricBounds: [y1, x, y1 + h, x + w] });
            tf.textFramePreferences.insetSpacing = [0, 0, 0, 0];
            tf.contents = text;
            var t = tf.parentStory.texts[0];
            t.appliedFont = fnt;
            t.pointSize = size;
            t.fillColor = ink;
            t.strokeColor = stroke;
            return tf;
        }
        function textStroke(tf, align, w, join, tracking, limit) {
            var t = tf.parentStory.texts[0];
            t.strokeWeight = w;
            t.strokeAlignment = align;
            t.endJoin = join || EndJoin.MITER_END_JOIN;
            t.miterLimit = limit || 4;
            if (tracking) { t.tracking = tracking; }
            return tf;
        }
        function outlined(x, y1, text, fnt, size) {
            var tf = frame(x, y1, BOX, size * 1.6, text, fnt, size);
            return flat(tf.createOutlines(true));
        }
        function triangle(x, y1, apex, hgt) {
            var half = apex / 2 * Math.PI / 180, b = hgt * Math.tan(half);
            var p = page.polygons.add();
            p.paths[0].entirePath = [[x + b, y1], [x + 2 * b, y1 + hgt], [x, y1 + hgt]];
            p.paths[0].pathType = PathType.CLOSED_PATH;
            return p;
        }
        function squares(x, y1, a) {
            var sq = page.polygons.add();
            sq.paths[0].entirePath = [[x, y1], [x + a, y1], [x + a, y1 + a], [x, y1 + a]];
            sq.paths[0].pathType = PathType.CLOSED_PATH;
            var p2 = sq.paths.add();
            var o = a / 2;
            p2.entirePath = [[x + o, y1 + o], [x + o + a, y1 + o], [x + o + a, y1 + o + a], [x + o, y1 + o + a]];
            p2.pathType = PathType.CLOSED_PATH;
            return sq;
        }
        function strokeType(name) { return function (it) { it.strokeType = doc.strokeStyles.itemByName(name); }; }
        function dashed(it) {
            it.strokeType = doc.strokeStyles.itemByName('Dashed');
            it.strokeDashAndGap = [12, 5];
            it.strokeCornerAdjustment = StrokeCornerAdjustment.NONE;
        }
        function corners(option, radius) {
            return function (it) {
                var c = ['topLeft', 'topRight', 'bottomLeft', 'bottomRight'];
                for (var q = 0; q < c.length; q++) { it[c[q] + 'CornerOption'] = option; it[c[q] + 'CornerRadius'] = radius || 5; }
            };
        }
        function each(items, f) { for (var q = 0; q < items.length; q++) { f(items[q]); } return items; }
        var OUT = StrokeAlignment.OUTSIDE_ALIGNMENT, CEN = StrokeAlignment.CENTER_ALIGNMENT, INS = StrokeAlignment.INSIDE_ALIGNMENT;
        var TOUT = TextStrokeAlign.OUTSIDE_ALIGNMENT, TCEN = TextStrokeAlign.CENTER_ALIGNMENT;
        var MITER = EndJoin.MITER_END_JOIN, ROUND = EndJoin.ROUND_END_JOIN;

        // ---------------------------------------------------------------- drawing
        cell('shapes', true, 150, function (x, y1) {
            // InDesign's outward offset breaks on script outlines: openings flood, inner edges stroked.
            each(outlined(x, y1, 'Amz', VIBES, 110), function (it) { paint(it, OUT, 6); });
        });
        cell('inside', true, 140, function (x, y1) {
            // InDesign's inward offset drops thin parts or spills past them.
            each(outlined(x, y1, 'Ins', MINION, 120), function (it) { paint(it, INS, 4); });
        });
        cell('open_paths', true, 150, function (x, y1) {
            var p = page.graphicLines.add();
            p.paths[0].entirePath = [[x + 10, y1 + 130], [x + 60, y1 + 20], [x + 75, y1 + 130], [x + 95, y1 + 20], [x + 230, y1 + 130], [x + 280, y1 + 25]];
            p.strokeColor = stroke;
            p.strokeWeight = 12;
            p.endJoin = MITER;
            p.miterLimit = 4;
        });
        cell('transparency', true, 150, function (x, y1) {
            // A 50% stroke over overlapping contours: darker where InDesign strokes each contour.
            each(outlined(x, y1, 'Rib', SCRIPT, 100), function (it) { paint(it, OUT, 8); it.strokeTransparencySettings.blendingSettings.opacity = 50; });
            var sq = squares(x + 200, y1 + 20, 55);
            paint(sq, OUT, 8);
            sq.strokeTransparencySettings.blendingSettings.opacity = 50;
        });
        // ---------------------------------------------------------------- stacking
        cell('text_behind', true, 170, function (x, y1) {
            // Line 2's stroke over line 1's descenders; a recolored run's stroke over the letter before it.
            var tf = frame(x, y1, BOX, 170, 'Jug\rhold', MINION_B, 90);
            textStroke(tf, TOUT, 12, ROUND);
            tf.parentStory.texts[0].leading = 62;
            tf.parentStory.paragraphs[1].characters.itemByRange(2, 3).fillColor = accent;
        });
        cell('text_story', true, 150, function (x, y1) {
            // Line 1 fills the first frame; line 2 flows into the second, which overlaps it.
            var a = frame(x, y1, BOX, 72, 'Thread\rframes', MINION_B, 72);
            var b = page.textFrames.add({ geometricBounds: [y1 + 40, x + 30, y1 + 150, x + BOX] });
            b.textFramePreferences.insetSpacing = [0, 0, 0, 0];
            a.nextTextFrame = b;
            textStroke(a, TOUT, 10, ROUND, -20);
        });
        cell('text_stories', false, 150, function (x, y1) {
            textStroke(frame(x, y1, 230, 80, 'Two', MINION_B, 80), TOUT, 10, ROUND, -20);
            textStroke(frame(x + 60, y1 + 42, 240, 100, 'tales', MINION_B, 80), TOUT, 10, ROUND, -20);
        });
        // ---------------------------------------------------------------- corners
        cell('text_miter', true, 170, function (x, y1) {
            // Spikes at the sharp joints of v, w, and M.
            textStroke(frame(x, y1, BOX, 170, 'vw', MINION, 170), TOUT, 16, MITER, 0, 10);
        });
        cell('shape_miter', true, 150, function (x, y1) {
            // A bar and a block joined by a thin hinge, a narrow notch between them: the notch's
            // inner miter runs into the hinge and out through its far side.
            function notch(x0, s) {
                var p = page.polygons.add();
                p.paths[0].entirePath = [[x0, y1], [x0 + 230 * s, y1], [x0 + 230 * s, y1 + 28 * s], [x0 + 8 * s, y1 + 55 * s],
                                         [x0 + 230 * s, y1 + 82 * s], [x0 + 230 * s, y1 + 110 * s], [x0, y1 + 110 * s]];
                p.paths[0].pathType = PathType.CLOSED_PATH;
                return p;
            }
            paint(notch(x + 30, 1.15), CEN, 10, MITER, null, 10);
        });
        cell('glyph_tips', true, 200, function (x, y1) {
            textStroke(frame(x, y1, BOX, 240, 'vz', EAGLE, 170), TOUT, 10, MITER);
        });
        cell('whole_points', true, 190, function (x, y1) {
            paint(triangle(x + 20, y1 + 50, 10, 120), CEN, 10);
            paint(triangle(x + 100, y1 + 50, 20, 120), CEN, 10);
            paint(triangle(x + 200, y1 + 60, 14, 110), OUT, 8);
        });
        cell('pattern_points', true, 190, function (x, y1) {
            paint(triangle(x + 20, y1 + 60, 14, 110), OUT, 6, MITER, dashed);
            paint(triangle(x + 120, y1 + 60, 14, 110), OUT, 10, MITER, strokeType('Thick - Thin'));
            paint(triangle(x + 220, y1 + 60, 14, 110), CEN, 8, MITER, strokeType('Dotted'));
        });
        cell('effect_corners', true, 160, function (x, y1) {
            // Its own effect shows with Trim glyph tips off, which otherwise trims most of these points.
            each(outlined(x, y1, 'MA', MINION, 150), function (it) { paint(it, OUT, 8, MITER, corners(CornerOptions.FANCY_CORNER), 10); });
        }, 'glyph_tips=0\n');
        // ---------------------------------------------------------------- contours
        cell('overlaps', true, 150, function (x, y1) {
            paint(squares(x + 10, y1 + 10, 80), OUT, 10);
            var o = page.ovals.add({ geometricBounds: [y1 + 10, x + 170, y1 + 100, x + 260] });
            var r = page.rectangles.add({ geometricBounds: [y1 + 45, x + 215, y1 + 135, x + 295] });
            paint(o.makeCompoundPath([r]), OUT, 10);
        });
        cell('word_outline', true, 150, function (x, y1) {
            // Great Vibes's letters overlap where they join, set tight: each contour dashed on its
            // own, the dashes cross inside the word.
            var tf = frame(x, y1, BOX, 160, 'Rib', VIBES, 120);
            tf.parentStory.texts[0].tracking = -40;
            each(flat(tf.createOutlines(true)), function (it) { paint(it, OUT, 6, MITER, dashed); });
            paint(squares(x + 200, y1 + 20, 55), OUT, 6, MITER, dashed);
        });
        cell('close_holes', false, 150, function (x, y1) {
            // On white, as every other example: the counters and the gaps the strokes enclose show
            // white when open and the stroke's color when closed.
            textStroke(frame(x, y1, BOX, 150, 'Bog', MINION_B, 130), TOUT, 6, ROUND, -70);
        });

        doc.documentPreferences.pageHeight = y + 10;
        // Resizing the page keeps its center: everything moved by the same amount, so the cells
        // keep their places relative to the page's new top.
        var grow = (4800 - (y + 10)) / 2, all = doc.spreads[0].pageItems.everyItem().getElements();
        for (var mi = 0; mi < all.length; mi++) { all[mi].move(undefined, [0, grow]); }
        var shift = 0;
        new Folder(out).create();
        doc.save(new File(out + '/feature-examples.indd'));

        // The exports: the defaults, then each switch on by default turned off alone, then each
        // switch off by default turned on alone.
        // 'none': StrokeMaster off, InDesign alone (the baseline of the corner-effect example).
        var variants = [['defaults', ''], ['none', 'enabled=0\n']];
        for (var c = 0; c < cells.length; c++) {
            variants.push([(cells[c].on ? 'off-' : 'on-') + cells[c].key, cells[c].base + cells[c].key + '=' + (cells[c].on ? 0 : 1) + '\n']);
            if (cells[c].base) variants.push(['base-' + cells[c].key, cells[c].base]);
        }
        var sf = new File(settingsPath), had = sf.exists, old = '';
        if (had) { sf.encoding = 'UTF-8'; sf.open('r'); old = sf.read(); sf.close(); }
        var hqp = app.pdfExportPresets.itemByName('[High Quality Print]');
        try {
            new Folder(sf.parent.fsName).create();
            for (var v = 0; v < variants.length; v++) {
                sf.encoding = 'UTF-8';
                sf.lineFeed = 'Unix';
                sf.open('w'); sf.write('# feature examples: ' + variants[v][0] + '\n' + variants[v][1]); sf.close();
                $.sleep(1500);   // a new write time, which the engine compares at each spread
                lib.timed('pdf ' + variants[v][0], function () { doc.exportFile(ExportFormat.PDF_TYPE, new File(out + '/fx-' + variants[v][0] + '.pdf'), false, hqp); });
            }
        } finally {
            if (had) { sf.encoding = 'UTF-8'; sf.lineFeed = 'Unix'; sf.open('w'); sf.write(old); sf.close(); } else { sf.remove(); }
        }
        for (var ci = 0; ci < cells.length; ci++) cells[ci].top += shift;
        lib.writeText(out + '/feature-examples.json', lib.toJSON({ page: [W, y + 10], cells: cells, app: app.version }));
        lib.L('cells=' + cells.length + ' height=' + (y + 10));
    });
})();
