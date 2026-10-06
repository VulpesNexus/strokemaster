// Builds the StrokeMaster showcase: one cell per case, each labeled, either a defect the plugin
// fixes ("F" cases) or a form it handles with less certainty ("L" cases). Saves the document,
// and exports it once per set of switches, by writing the switches' file (IseSettings.h)
// between exports: "before" (StrokeMaster off), "after" (the defaults), and "stories" (the
// defaults with the experimental separate-stories switch). The file's previous contents are
// put back afterward. tools/compose_showcase.py then sets the exports side by side.
//
// Arguments: ISE_ARGS.out (folder), ISE_ARGS.settings (the switches' file).
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var out = lib.args.out, settingsPath = lib.args.settings;
        var W = 600, CELL = 560, X = 20;
        var cells = [];
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
        var gap = lib.color(doc, 'SM gap', [60, 0, 0, 0]);
        var labelStyle = doc.paragraphStyles.add({ name: 'SM label' });
        labelStyle.appliedFont = 'Minion Pro\tBold';
        labelStyle.pointSize = 10;
        labelStyle.fillColor = black;
        labelStyle.strokeColor = none;

        function font(name, fallback) { return lib.fontOk(name) ? name : fallback; }
        function findFont(part) {
            var all = app.fonts.everyItem().name;
            for (var q = 0; q < all.length; q++) { if (String(all[q]).indexOf(part) === 0) { return all[q]; } }
            return null;
        }
        var MINION = 'Minion Pro\tRegular', MINION_B = font('Minion Pro\tBold', MINION);
        var SCRIPT = font('Segoe Script\tRegular', MINION), SCRIPT_B = font('Segoe Script\tBold', SCRIPT);
        var VIBES = font('Great Vibes\tRegular', SCRIPT), MINCHO = font('Yu Mincho\tDemibold', null);
        var FANG = findFont('Kerberos Fang');

        var y = 20;
        // A labeled cell `h` points high; `build(x, y)` draws its items below the label.
        function cell(id, kind, title, h, build) {
            var tf = page.textFrames.add({ geometricBounds: [y, X, y + 14, X + CELL] });
            tf.textFramePreferences.insetSpacing = [0, 0, 0, 0];
            tf.contents = id + '   ' + title;
            tf.parentStory.texts[0].applyParagraphStyle(labelStyle, true);
            lib.tryDo(id, function () { build(X, y + 20); });
            y += h;
            cells.push({ id: id, kind: kind, title: title, label: tf, h: h });
        }
        function flat(items) {
            var r = [];
            for (var q = 0; q < items.length; q++) {
                if (items[q].constructor.name === 'Group') { r = r.concat(flat(items[q].pageItems.everyItem().getElements())); } else { r.push(items[q]); }
            }
            return r;
        }
        function paint(it, align, w, join, extra) {
            it.fillColor = fill;
            it.strokeColor = black;
            it.strokeWeight = w;
            it.strokeAlignment = align;
            it.endJoin = join || EndJoin.MITER_END_JOIN;
            it.miterLimit = 4;
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
            t.fillColor = fill;
            t.strokeColor = black;
            return tf;
        }
        function textStroke(tf, align, w, join, tracking) {
            var t = tf.parentStory.texts[0];
            t.strokeWeight = w;
            t.strokeAlignment = align;
            t.endJoin = join || EndJoin.MITER_END_JOIN;
            t.miterLimit = 4;
            if (tracking) { t.tracking = tracking; }
        }
        function outlined(x, y1, text, fnt, size) {
            var tf = frame(x, y1, 520, size * 1.6, text, fnt, size);
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
        // Without corner adjustment: InDesign fits dashes to every corner of an outlined letter,
        // whose many corners left no gap at all (the owner's check, 2026-10-06).
        function dashed(it) {
            it.strokeType = doc.strokeStyles.itemByName('Dashed');
            it.strokeDashAndGap = [12, 4];
            it.strokeCornerAdjustment = StrokeCornerAdjustment.NONE;
        }
        function corners(option) {
            return function (it) {
                var c = ['topLeft', 'topRight', 'bottomLeft', 'bottomRight'];
                for (var q = 0; q < c.length; q++) { it[c[q] + 'CornerOption'] = option; it[c[q] + 'CornerRadius'] = 4; }
            };
        }
        function each(items, f) { for (var q = 0; q < items.length; q++) { f(items[q]); } return items; }
        var OUT = StrokeAlignment.OUTSIDE_ALIGNMENT, CEN = StrokeAlignment.CENTER_ALIGNMENT, INS = StrokeAlignment.INSIDE_ALIGNMENT;
        var TOUT = TextStrokeAlign.OUTSIDE_ALIGNMENT, TCEN = TextStrokeAlign.CENTER_ALIGNMENT;

        // ---------------------------------------------------------------- what it fixes
        cell('F1', 'fix', 'Live text, Outside stroke: no stroke over the line above or the run before', 150, function (x, y1) {
            // InDesign strokes each line, and each run of one format, over the fills drawn before
            // it (fixture A): line 2's ascenders reach line 1's descenders, and a recolored run
            // starts inside a word.
            var tf = frame(x, y1, 540, 130, 'Typography\rblack ink', MINION_B, 60);
            textStroke(tf, TOUT, 10, EndJoin.ROUND_END_JOIN);
            tf.parentStory.texts[0].leading = 38;
            tf.parentStory.paragraphs[0].characters.itemByRange(4, 7).fillColor = gap;
        });
        cell('F2', 'fix', 'Live text across two threaded frames of one story', 120, function (x, y1) {
            var a = frame(x, y1, 250, 50, 'Threaded frames', MINION_B, 44);
            var b = page.textFrames.add({ geometricBounds: [y1 + 28, x + 150, y1 + 90, x + 450] });
            b.textFramePreferences.insetSpacing = [0, 0, 0, 0];
            a.nextTextFrame = b;
            textStroke(a, TOUT, 6, EndJoin.ROUND_END_JOIN, -40);
        });
        cell('F3', 'fix', 'Live text, Outside Miter strokes (8 pt, 6 pt): no spikes at sharp joints', 110, function (x, y1) {
            // Suite B's glyphs and weights, where InDesign's spikes are plain (v, w, M, y).
            textStroke(frame(x, y1, 275, 90, 'vwMyz', MINION, 56), TOUT, 8, EndJoin.MITER_END_JOIN);
            textStroke(frame(x + 280, y1, 275, 90, 'vwMyz', MINION, 56), TOUT, 6, EndJoin.MITER_END_JOIN);
        });
        cell('F4', 'fix', 'Shapes past the miter limit: whole points instead of flat tops', 190, function (x, y1) {
            paint(triangle(x + 10, y1 + 70, 10, 80), CEN, 8);
            paint(triangle(x + 80, y1 + 70, 20, 80), CEN, 8);
            paint(triangle(x + 200, y1 + 80, 10, 70), OUT, 6);
            paint(triangle(x + 280, y1 + 80, 20, 70), OUT, 6);
        });
        cell('F5', 'fix', 'Outlined script, Outside stroke: InDesign\'s offset fill breaks and strokes inner edges', 110, function (x, y1) {
            each(outlined(x, y1, 'Amazing', VIBES, 64), function (it) { paint(it, OUT, 5); });
        });
        cell('F6', 'fix', 'Outlined text, Inside stroke: no spill outside thin parts, none dropped', 110, function (x, y1) {
            each(outlined(x, y1, 'Inside serifs', MINION, 64), function (it) { paint(it, INS, 4); });
        });
        cell('F7', 'fix', 'Corner effects (fancy) on outlined text: no long points', 110, function (x, y1) {
            each(outlined(x, y1, 'MAW kz', MINION, 64), function (it) { paint(it, OUT, 4, EndJoin.MITER_END_JOIN, corners(CornerOptions.FANCY_CORNER)); });
        });
        cell('F8', 'fix', 'Overlapping shapes, Outside stroke: one outline around the filled shape', 130, function (x, y1) {
            paint(squares(x + 10, y1 + 5, 56), OUT, 6);
            var o = page.ovals.add({ geometricBounds: [y1 + 5, x + 150, y1 + 75, x + 220] });
            var r = page.rectangles.add({ geometricBounds: [y1 + 25, x + 190, y1 + 85, x + 270] });
            paint(o.makeCompoundPath([r]), OUT, 6);
        });
        cell('F9', 'fix', 'Dashes on joined script and overlapping shapes: one outline per word', 140, function (x, y1) {
            each(outlined(x, y1, 'Ribbons', SCRIPT, 56), function (it) { paint(it, OUT, 4, EndJoin.MITER_END_JOIN, dashed); });
            paint(squares(x + 330, y1 + 10, 56), OUT, 4, EndJoin.MITER_END_JOIN, dashed);
        });
        cell('F10', 'fix', 'Stripes (Thick - Thin) on joined script and overlapping shapes: one outline per word', 145, function (x, y1) {
            each(outlined(x, y1, 'Ribbons', SCRIPT, 56), function (it) { paint(it, OUT, 8, EndJoin.MITER_END_JOIN, strokeType('Thick - Thin')); });
            paint(squares(x + 330, y1 + 10, 56), OUT, 8, EndJoin.MITER_END_JOIN, strokeType('Thick - Thin'));
        });
        cell('F11', 'fix', 'Dashed and striped points past the limit: whole points added', 200, function (x, y1) {
            paint(triangle(x + 10, y1 + 90, 12, 70), OUT, 4, EndJoin.MITER_END_JOIN, dashed);
            paint(triangle(x + 90, y1 + 90, 20, 70), OUT, 4, EndJoin.MITER_END_JOIN, dashed);
            paint(triangle(x + 190, y1 + 90, 12, 70), OUT, 8, EndJoin.MITER_END_JOIN, strokeType('Thick - Thin'));
            paint(triangle(x + 280, y1 + 90, 12, 70), CEN, 6, EndJoin.MITER_END_JOIN, strokeType('Dotted'));
        });
        cell('F12', 'fix', 'Script glyph tips: spikes within the miter limit trimmed', 110, function (x, y1) {
            each(outlined(x, y1, 'Wave war', SCRIPT_B, 60), function (it) { paint(it, OUT, 8); });
        });

        // ---------------------------------------------------------------- least confident
        cell('L1', 'doubt', 'Stripes drawn by StrokeMaster around counters (layouts measured from InDesign)', 120, function (x, y1) {
            each(outlined(x, y1, 'BOOK', MINION_B, 60), function (it) { paint(it, OUT, 8, EndJoin.MITER_END_JOIN, strokeType('Thin - Thick - Thin')); });
        });
        cell('L2', 'doubt', 'Stripes with a gap color, drawn by StrokeMaster beneath its stripes', 145, function (x, y1) {
            var g = function (it) { it.strokeType = doc.strokeStyles.itemByName('Thick - Thin'); it.gapColor = gap; };
            each(outlined(x, y1, 'Ribbons', SCRIPT, 56), function (it) { paint(it, OUT, 8, EndJoin.MITER_END_JOIN, g); });
            paint(squares(x + 330, y1 + 10, 56), OUT, 8, EndJoin.MITER_END_JOIN, g);
        });
        cell('L3', 'doubt', 'Wavy, its pattern drawn by InDesign along one outline per word', 145, function (x, y1) {
            each(outlined(x, y1, 'Ribbons', SCRIPT, 56), function (it) { paint(it, OUT, 6, EndJoin.MITER_END_JOIN, strokeType('Wavy')); });
            paint(squares(x + 330, y1 + 10, 56), OUT, 6, EndJoin.MITER_END_JOIN, strokeType('Wavy'));
        });
        cell('L4', 'doubt', 'Two separate stories overlapping (experimental switch, off by default)', 120, function (x, y1) {
            textStroke(frame(x, y1, 260, 60, 'Separate', MINION_B, 52), TOUT, 6, EndJoin.ROUND_END_JOIN, -20);
            textStroke(frame(x + 90, y1 + 20, 300, 60, 'stories', MINION_B, 52), TOUT, 6, EndJoin.ROUND_END_JOIN, -20);
        });
        cell('L5', 'doubt', 'Text in a frame both sheared and rotated', 130, function (x, y1) {
            var tf = frame(x + 20, y1 + 10, 300, 60, 'Sheared', MINION, 48);
            textStroke(tf, TOUT, 5, EndJoin.MITER_END_JOIN);
            tf.shearAngle = 20;
            tf.rotationAngle = 10;
        });
        if (FANG) {
            cell('L6', 'doubt', 'A decorative font with thousands of corners (Kerberos Fang, outlined)', 120, function (x, y1) {
                each(outlined(x, y1, 'Fang', FANG, 56), function (it) { paint(it, OUT, 4); });
            });
        }
        cell('L7', 'doubt', 'Open path, Center Miter: whole points beside caps', 160, function (x, y1) {
            var p = page.graphicLines.add();
            p.paths[0].entirePath = [[x + 10, y1 + 80], [x + 60, y1 + 30], [x + 70, y1 + 100], [x + 80, y1 + 30], [x + 200, y1 + 100], [x + 230, y1 + 35]];
            p.strokeColor = black;
            p.strokeWeight = 8;
            p.endJoin = EndJoin.MITER_END_JOIN;
            p.miterLimit = 4;
        });
        cell('L8', 'doubt', 'Inside stroke with inset corner effects on outlined text', 110, function (x, y1) {
            each(outlined(x, y1, 'MAW kz', MINION_B, 60), function (it) { paint(it, INS, 4, EndJoin.MITER_END_JOIN, corners(CornerOptions.INSET_CORNER)); });
        });
        cell('L9', 'doubt', 'Object opacity and drop shadow on outlined text', 110, function (x, y1) {
            each(outlined(x, y1, 'Shadows', MINION_B, 60), function (it) {
                paint(it, OUT, 5);
                it.transparencySettings.blendingSettings.opacity = 70;
                var s = it.transparencySettings.dropShadowSettings;
                s.mode = ShadowMode.DROP; s.xOffset = 4; s.yOffset = 4; s.size = 2; s.opacity = 75;
            });
        });
        if (MINCHO) {
            cell('L10', 'doubt', 'Brush serifs (Yu Mincho): their sharp tips are trimmed as glyph tips', 120, function (x, y1) {
                each(outlined(x, y1, '\u6C38\u3042\u9B31', MINCHO, 60), function (it) { paint(it, OUT, 8); });
            });
        }
        cell('L11', 'doubt', 'Custom stripes (0-20 and 50-100 percent), read from the style', 145, function (x, y1) {
            var st = doc.stripedStrokeStyles.add({ name: 'SM stripes', stripeArray: [0, 20, 50, 100] });
            var g = function (it) { it.strokeType = st; };
            each(outlined(x, y1, 'Ribbons', SCRIPT, 56), function (it) { paint(it, OUT, 8, EndJoin.MITER_END_JOIN, g); });
            paint(squares(x + 330, y1 + 10, 56), OUT, 8, EndJoin.MITER_END_JOIN, g);
        });
        cell('L12', 'doubt', 'Straight Hash and Japanese Dots along one outline per word', 145, function (x, y1) {
            each(outlined(x, y1, 'Ribbons', SCRIPT, 56), function (it) { paint(it, OUT, 6, EndJoin.MITER_END_JOIN, strokeType('Straight Hash')); });
            paint(squares(x + 330, y1 + 10, 56), OUT, 6, EndJoin.MITER_END_JOIN, strokeType('Japanese Dots'));
        });
        cell('L13', 'doubt', 'A custom dash style (corner adjustment on) and White Diamond along one outline per word', 145, function (x, y1) {
            var ds = doc.dashedStrokeStyles.add({ name: 'SM dashes', dashArray: [10, 3, 2, 3] });
            var g = function (it) { it.strokeType = ds; };
            each(outlined(x, y1, 'Ribbons', SCRIPT, 56), function (it) { paint(it, OUT, 4, EndJoin.MITER_END_JOIN, g); });
            paint(squares(x + 330, y1 + 10, 56), OUT, 6, EndJoin.MITER_END_JOIN, strokeType('White Diamond'));
        });

        // Resizing the page keeps its center, which leaves the first cells above its new top: move
        // everything back down so the first label is at 20 again, then measure each cell from
        // its label.
        doc.documentPreferences.pageHeight = y + 10;
        var shift = 20 - cells[0].label.geometricBounds[0], all = doc.spreads[0].pageItems.everyItem().getElements();
        for (var mi = 0; mi < all.length; mi++) { all[mi].move(undefined, [0, shift]); }
        for (var ci = 0; ci < cells.length; ci++) {
            var top = cells[ci].label.geometricBounds[0];
            cells[ci].bounds = [top - 6, X - 10, top + cells[ci].h - 6, X + CELL + 10];
            delete cells[ci].label;
        }
        new Folder(out).create();
        doc.save(new File(out + '/showcase.indd'));

        // The exports, one per set of switches; the switches' file is put back afterward.
        var sf = new File(settingsPath), had = sf.exists, old = '';
        if (had) { sf.encoding = 'UTF-8'; sf.open('r'); old = sf.read(); sf.close(); }
        var variants = [['before', 'enabled=0\n'], ['after', ''], ['stories', 'text_stories=1\n']];
        var hqp = app.pdfExportPresets.itemByName('[High Quality Print]');
        try {
            new Folder(sf.parent.fsName).create();
            for (var v = 0; v < variants.length; v++) {
                sf.encoding = 'UTF-8';
                sf.lineFeed = 'Unix';
                sf.open('w'); sf.write('# showcase export: ' + variants[v][0] + '\n' + variants[v][1]); sf.close();
                $.sleep(300);
                lib.timed('pdf ' + variants[v][0], function () { doc.exportFile(ExportFormat.PDF_TYPE, new File(out + '/showcase-' + variants[v][0] + '.pdf'), false, hqp); });
            }
        } finally {
            if (had) { sf.encoding = 'UTF-8'; sf.lineFeed = 'Unix'; sf.open('w'); sf.write(old); sf.close(); } else { sf.remove(); }
        }
        lib.writeText(out + '/showcase.json', lib.toJSON({ page: [W, y + 10], cells: cells, app: app.version }));
        lib.L('cells=' + cells.length + ' height=' + (y + 10));
    });
})();
