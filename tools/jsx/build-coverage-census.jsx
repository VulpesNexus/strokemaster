// build-coverage-census.jsx - every stroke the plugin leaves to InDesign, beside the strokes it
// corrects, drawn natively and by the research build's corrections in one process. Answers
// which of the uncovered cases show the defects the plugin removes (escaping miter tips,
// broken Outside fills, beveled points, strokes over neighboring letters), so coverage goes
// where the damage is. Requires ise-lib.jsx and a research build of route X (it reads the
// control file named by ISE_XF_CONTROL).
//
// One row per case: a base set (outlined text, a 10-degree thorn, two overlapping squares) or
// live text, styled as the case says, in Minion or German Beauty. The layer "ISE geometry"
// holds every item again at the same place, filled Black with no stroke, effect, or
// transparency (text converted to outlines; corner effects and transforms kept): the outline
// the stroke follows, exported alone for tools/measure_coverage_census.py.
// A transformed item that keeps its transform (shear, a scale kept as a percentage) gets a twin
// one row down: the same outline drawn untransformed at the weight the case should show.
// Arguments: ISE_ARGS.out (folder for the document and PDFs), ISE_ARGS.control (that file),
// optionally ISE_ARGS.modes: the exports, as [name, control file text, geometry layer only],
// written to coverage-census-<name>.pdf (default: native, corrected, geometry).
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var out = lib.args.out;
        var W = 400, ROW = 110;
        var MINION = lib.FONT, GB = 'German Beauty\tRegular', TEXT = 'VWAMNvwkyzK';
        var cases = [];
        var A = StrokeAlignment, J = EndJoin, TA = TextStrokeAlign;
        function P(name, align, w, join, extra, transform, font) { cases.push({ kind: 'page item', name: name, align: align, w: w, join: join, extra: extra, transform: transform, font: font || MINION }); }
        function T(name, align, w, extra, font) { cases.push({ kind: 'text', name: name, align: align, w: w, extra: extra || {}, font: font || MINION }); }
        function dashed(it, d) { it.strokeType = d.strokeStyles.itemByName('Dashed'); it.strokeDashAndGap = [12, 4]; }
        function opacity(it) { it.transparencySettings.blendingSettings.opacity = 50; }
        function styled(name) { return function (it, d) { it.strokeType = d.strokeStyles.itemByName(name); }; }
        function limit2(it) { it.miterLimit = 2; }
        function both(f, g) { return function (it, d) { f(it, d); g(it, d); }; }
        // Two overlapping parallelograms: the squares sheared into their path (not a transform).
        function shearSquares(it) {
            if (it.paths.length !== 2 || it.paths[0].pathPoints.length !== 4 || it.paths[1].pathPoints.length !== 4) { return; }
            var k = Math.tan(20 * Math.PI / 180), y0 = it.geometricBounds[0];
            for (var q = 0; q < 2; q++) {
                var pts = it.paths[q].entirePath;
                for (var r = 0; r < pts.length; r++) { pts[r] = [pts[r][0] + k * (pts[r][1] - y0), pts[r][1]]; }
                it.paths[q].entirePath = pts;
            }
        }

        P('center 8 miter (covered)', A.CENTER_ALIGNMENT, 8, J.MITER_END_JOIN);
        P('outside 4 miter (covered)', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN);
        P('inside 4 miter', A.INSIDE_ALIGNMENT, 4, J.MITER_END_JOIN);
        P('inside 8 miter', A.INSIDE_ALIGNMENT, 8, J.MITER_END_JOIN);
        P('outside 4 round', A.OUTSIDE_ALIGNMENT, 4, J.ROUND_END_JOIN);
        P('outside 4 bevel', A.OUTSIDE_ALIGNMENT, 4, J.BEVEL_END_JOIN);
        P('center 8 round', A.CENTER_ALIGNMENT, 8, J.ROUND_END_JOIN);
        P('center 8 bevel', A.CENTER_ALIGNMENT, 8, J.BEVEL_END_JOIN);
        P('outside 4 miter dashed 12/4', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, dashed);
        P('center 8 miter dashed 12/4', A.CENTER_ALIGNMENT, 8, J.MITER_END_JOIN, dashed);
        P('outside 8 miter thick-thin', A.OUTSIDE_ALIGNMENT, 8, J.MITER_END_JOIN, function (it, d) { it.strokeType = d.strokeStyles.itemByName('Thick - Thin'); });
        P('center 8 miter thin-thin', A.CENTER_ALIGNMENT, 8, J.MITER_END_JOIN, function (it, d) { it.strokeType = d.strokeStyles.itemByName('Thin - Thin'); });
        P('outside 4 miter fancy corners 4', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, function (it) { corner(it, CornerOptions.FANCY_CORNER); });
        P('center 8 miter fancy corners 4', A.CENTER_ALIGNMENT, 8, J.MITER_END_JOIN, function (it) { corner(it, CornerOptions.FANCY_CORNER); });
        P('outside 4 miter inset corners 4', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, function (it) { corner(it, CornerOptions.INSET_CORNER); });
        P('outside 4 miter rounded corners 4', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, function (it) { corner(it, CornerOptions.ROUNDED_CORNER); });
        P('outside 4 miter opacity 50', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, opacity);
        P('center 8 miter opacity 50', A.CENTER_ALIGNMENT, 8, J.MITER_END_JOIN, opacity);
        P('outside 4 miter drop shadow', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, function (it) { shadow(it.transparencySettings.dropShadowSettings); });
        P('outside 4 miter gradient fill (covered)', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, function (it, d) { it.fillColor = d.gradients.itemByName('ISE dark gradient'); });
        P('outside 4 miter gradient stroke (covered)', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, function (it, d) { it.strokeColor = d.gradients.itemByName('ISE light gradient'); });
        // Scaling by script, as by default in the interface, is applied to the path and the
        // weight, which leaves the item untransformed; shear, and a scale kept as a percentage
        // (Preferences > General > When Scaling), stay on the item. The base set is built at
        // the size the transform brings back to the controls' size.
        P('outside 4 miter scaled 50%, applied to content (covered)', A.OUTSIDE_ALIGNMENT, 8, J.MITER_END_JOIN, null, { scale: 2, h: 50, v: 50 });
        P('outside 4 miter scaled 50%, kept as a percentage', A.OUTSIDE_ALIGNMENT, 8, J.MITER_END_JOIN, null, { scale: 2, h: 50, v: 50, keep: true, twinWeight: 4 });
        P('center 8 miter scaled 50%, kept as a percentage', A.CENTER_ALIGNMENT, 16, J.MITER_END_JOIN, null, { scale: 2, h: 50, v: 50, keep: true, twinWeight: 8 });
        P('outside 4 miter sheared 20', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, null, { scale: 1, shear: 20, twinWeight: 4 });
        P('center 8 miter sheared 20', A.CENTER_ALIGNMENT, 8, J.MITER_END_JOIN, null, { scale: 1, shear: 20, twinWeight: 8 });
        P('outside 4 miter rotated 10 (covered)', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, null, { scale: 1, rotate: 10 });
        P('outside 4 miter flipped (covered)', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, null, { scale: 1, flip: true });
        P('center 8 miter open, filled', A.CENTER_ALIGNMENT, 8, J.MITER_END_JOIN, function (it) { open(it); });
        P('center 8 miter open, unfilled', A.CENTER_ALIGNMENT, 8, J.MITER_END_JOIN, function (it, d) { open(it); it.fillColor = d.swatches.itemByName('None'); });
        P('outside 4 miter open (alignment as InDesign keeps it)', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, function (it) { open(it); });
        // German Beauty: pointy, and its outlines overlap (the fill rules disagree).
        P('GB outside 4 miter (covered)', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, null, null, GB);
        P('GB center 8 miter (covered)', A.CENTER_ALIGNMENT, 8, J.MITER_END_JOIN, null, null, GB);
        P('GB inside 4 miter', A.INSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, null, null, GB);
        P('GB outside 4 round', A.OUTSIDE_ALIGNMENT, 4, J.ROUND_END_JOIN, null, null, GB);
        P('GB outside 4 bevel', A.OUTSIDE_ALIGNMENT, 4, J.BEVEL_END_JOIN, null, null, GB);
        P('GB outside 4 miter dashed 12/4', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, dashed, null, GB);
        P('GB outside 4 miter opacity 50', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, opacity, null, GB);
        P('GB center 8 miter opacity 50', A.CENTER_ALIGNMENT, 8, J.MITER_END_JOIN, opacity, null, GB);
        P('GB outside 4 miter sheared 20', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, null, { scale: 1, shear: 20, twinWeight: 4 }, GB);
        // Rows added with the full coverage (2026-10-04): every remaining variant of each alignment.
        P('inside 4 round', A.INSIDE_ALIGNMENT, 4, J.ROUND_END_JOIN);
        P('inside 4 bevel', A.INSIDE_ALIGNMENT, 4, J.BEVEL_END_JOIN);
        P('inside 4 miter limit 2', A.INSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, limit2);
        P('outside 4 bevel limit 2', A.OUTSIDE_ALIGNMENT, 4, J.BEVEL_END_JOIN, limit2);
        P('inside 4 miter dashed 12/4', A.INSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, dashed);
        P('inside 4 miter thick-thin', A.INSIDE_ALIGNMENT, 8, J.MITER_END_JOIN, styled('Thick - Thin'));
        P('outside 4 miter dotted', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, styled('Dotted'));
        P('outside 4 miter dashed 12/4, gap color', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, function (it, d) { dashed(it, d); it.gapColor = d.colors.itemByName('ISE black 20'); });
        P('outside 4 round dashed 12/4', A.OUTSIDE_ALIGNMENT, 4, J.ROUND_END_JOIN, dashed);
        P('center 8 miter wavy', A.CENTER_ALIGNMENT, 8, J.MITER_END_JOIN, styled('Wavy'));
        P('center 8 miter inset corners 4', A.CENTER_ALIGNMENT, 8, J.MITER_END_JOIN, function (it) { corner(it, CornerOptions.INSET_CORNER); });
        P('inside 4 miter fancy corners 4', A.INSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, function (it) { corner(it, CornerOptions.FANCY_CORNER); });
        P('outside 4 round rounded corners 4', A.OUTSIDE_ALIGNMENT, 4, J.ROUND_END_JOIN, function (it) { corner(it, CornerOptions.ROUNDED_CORNER); });
        P('outside 4 miter bevel corners 4', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, function (it) { corner(it, CornerOptions.BEVEL_CORNER); });
        P('outside 4 miter inverse rounded corners 4', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, function (it) { corner(it, CornerOptions.INVERSE_ROUNDED_CORNER); });
        P('outside 4 miter stroke opacity 50', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, function (it) { it.strokeTransparencySettings.blendingSettings.opacity = 50; });
        P('outside 4 miter fill opacity 50', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, function (it) { it.fillTransparencySettings.blendingSettings.opacity = 50; });
        P('center 8 miter stroke opacity 50', A.CENTER_ALIGNMENT, 8, J.MITER_END_JOIN, function (it) { it.strokeTransparencySettings.blendingSettings.opacity = 50; });
        P('inside 4 miter opacity 50', A.INSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, opacity);
        P('outside 4 miter stroke drop shadow', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, function (it) { shadow(it.strokeTransparencySettings.dropShadowSettings); });
        P('inside 4 miter gradient fill', A.INSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, function (it, d) { it.fillColor = d.gradients.itemByName('ISE dark gradient'); });
        P('outside 4 miter squares sheared into the path', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, shearSquares);
        P('center 8 miter squares sheared into the path', A.CENTER_ALIGNMENT, 8, J.MITER_END_JOIN, shearSquares);
        P('outside 4 miter scaled 150% x 100%, kept as a percentage', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, null, { scale: 1, h: 150, v: 100, keep: true });
        P('inside 4 miter sheared 20', A.INSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, null, { scale: 1, shear: 20, twinWeight: 4 });
        P('outside 4 bevel sheared 20', A.OUTSIDE_ALIGNMENT, 4, J.BEVEL_END_JOIN, null, { scale: 1, shear: 20, twinWeight: 4 });
        P('center 8 miter open, round caps', A.CENTER_ALIGNMENT, 8, J.MITER_END_JOIN, function (it) { open(it); it.endCap = EndCap.ROUND_END_CAP; });
        P('GB inside 4 round', A.INSIDE_ALIGNMENT, 4, J.ROUND_END_JOIN, null, null, GB);
        P('GB outside 4 miter stroke opacity 50', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, function (it) { it.strokeTransparencySettings.blendingSettings.opacity = 50; }, null, GB);

        // Live text as suite B sets it (Minion 36 pt, Outside 4 pt), where InDesign's miters spike.
        function frameOpacity(f) { f.transparencySettings.blendingSettings.opacity = 50; }
        T('text outside 4 miter (covered)', TA.OUTSIDE_ALIGNMENT, 4);
        T('text center 8 miter (covered)', TA.CENTER_ALIGNMENT, 8);
        T('text outside 4 miter, frame opacity 50', TA.OUTSIDE_ALIGNMENT, 4, { frame: frameOpacity });
        T('text center 8 miter, frame opacity 50', TA.CENTER_ALIGNMENT, 8, { frame: frameOpacity });
        T('text outside 4 miter, frame drop shadow', TA.OUTSIDE_ALIGNMENT, 4, { frame: function (f) { shadow(f.transparencySettings.dropShadowSettings); } });
        T('text outside 4 miter, frame scaled 200%, applied to content', TA.OUTSIDE_ALIGNMENT, 2, { size: 18, transform: { h: 200, v: 200 } });
        T('text outside 4 miter, frame scaled 200%, kept as a percentage', TA.OUTSIDE_ALIGNMENT, 2, { size: 18, transform: { h: 200, v: 200, keep: true } });
        T('text outside 4 miter, frame sheared 20 (covered)', TA.OUTSIDE_ALIGNMENT, 4, { transform: { shear: 20 } });
        T('text outside 4 miter, frame rotated 10 (covered)', TA.OUTSIDE_ALIGNMENT, 4, { transform: { rotate: 10 } });
        T('text outside 4 miter, skew 20, horizontal scale 120% (covered)', TA.OUTSIDE_ALIGNMENT, 4, { text: function (t) { t.skew = 20; t.horizontalScale = 120; } });
        T('text outside 4 miter, type on a path', TA.OUTSIDE_ALIGNMENT, 4, { path: true });
        T('text outside 4 miter, gradient fill (covered)', TA.OUTSIDE_ALIGNMENT, 4, { text: function (t, d) { t.fillColor = d.gradients.itemByName('ISE dark gradient'); } });
        T('text outside 4 miter, gradient stroke (covered)', TA.OUTSIDE_ALIGNMENT, 4, { text: function (t, d) { t.strokeColor = d.gradients.itemByName('ISE light gradient'); } });
        T('text GB outside 4 miter (covered)', TA.OUTSIDE_ALIGNMENT, 4, null, GB);
        T('text GB center 8 miter (covered)', TA.CENTER_ALIGNMENT, 8, null, GB);
        T('text GB outside 4 miter, frame opacity 50', TA.OUTSIDE_ALIGNMENT, 4, { frame: frameOpacity }, GB);
        T('text GB center 8 miter, frame opacity 50', TA.CENTER_ALIGNMENT, 8, { frame: frameOpacity }, GB);
        T('text GB outside 4 miter, frame drop shadow', TA.OUTSIDE_ALIGNMENT, 4, { frame: function (f) { shadow(f.transparencySettings.dropShadowSettings); } }, GB);
        T('text GB outside 4 miter, type on a path', TA.OUTSIDE_ALIGNMENT, 4, { path: true }, GB);
        // Transforms kept on the frame, set by matrix (setting horizontalScale on a text frame
        // resized the frame and left the text unscaled, 2026-10-04).
        T('text outside 4 miter, frame scaled 200% by matrix, kept', TA.OUTSIDE_ALIGNMENT, 2, { size: 18, transform: { matrix: { horizontalScaleFactor: 2, verticalScaleFactor: 2 }, keep: true, factor: 2 } });
        T('text outside 4 miter, frame scaled 150% x 100% by matrix, kept', TA.OUTSIDE_ALIGNMENT, 4, { size: 24, transform: { matrix: { horizontalScaleFactor: 1.5 }, keep: true, factor: Math.sqrt(1.5) } });
        T('text center 8 miter, frame sheared 20', TA.CENTER_ALIGNMENT, 8, { transform: { shear: 20 } });
        T('text outside 4 miter, frame sheared 20 and rotated 10', TA.OUTSIDE_ALIGNMENT, 4, { transform: { shear: 20, rotate: 10 } });
        T('text GB outside 4 miter, frame sheared 20', TA.OUTSIDE_ALIGNMENT, 4, { transform: { shear: 20 } }, GB);
        // Frames with content: InDesign strokes a frame after its text, so nothing drawn at the
        // frame's stroke may paint the frame's fill again over the text.
        function frameStroke(align, kind) {
            return function (f) {
                f.fillColor = doc.colors.itemByName('ISE black 20');
                f.strokeColor = doc.swatches.itemByName('Black');
                f.strokeWeight = 4;
                f.strokeAlignment = align;
                if (kind) { f.strokeType = doc.strokeStyles.itemByName(kind); }
            };
        }
        T('text frame filled, outside 4 frame stroke', TA.OUTSIDE_ALIGNMENT, 4, { frame: frameStroke(StrokeAlignment.OUTSIDE_ALIGNMENT) });
        T('text frame filled, inside 4 frame stroke', TA.OUTSIDE_ALIGNMENT, 4, { frame: frameStroke(StrokeAlignment.INSIDE_ALIGNMENT) });
        T('text frame filled, outside 4 dashed frame stroke', TA.OUTSIDE_ALIGNMENT, 4, { frame: frameStroke(StrokeAlignment.OUTSIDE_ALIGNMENT, 'Dashed') });
        // Appended so earlier rows keep their numbers (2026-10-05): arrowheads, which a claimed
        // stroke would drop, stay InDesign's.
        P('center 8 miter open, arrowheads', A.CENTER_ALIGNMENT, 8, J.MITER_END_JOIN, function (it) {
            open(it); it.leftLineEnd = ArrowHead.TRIANGLE_ARROW_HEAD; it.rightLineEnd = ArrowHead.TRIANGLE_ARROW_HEAD;
        });
        // The thorn's path starts at its tip, so opening it caps the tip; here it is opened at a
        // base corner instead, and its 10 degree tip stays a corner past the limit.
        P('center 8 miter open at the thorn base, unfilled', A.CENTER_ALIGNMENT, 8, J.MITER_END_JOIN, function (it, d) {
            if (it.paths.length === 1 && it.paths[0].pathPoints.length === 7) {
                var pts = it.paths[0].entirePath;
                it.paths[0].entirePath = pts.slice(3).concat(pts.slice(0, 3));
            }
            open(it); it.fillColor = d.swatches.itemByName('None');
        });

        // Font sweep (ISE_ARGS.sweep, a list of "family<TAB>style[<TAB>sample text]"): the core rows for each
        // font instead of the census, labeled "sweep <n> ..." with the font in the manifest; for
        // the plugin's corrections across typefaces (2026-10-05). Rows: outlined text, the thorn,
        // and the squares Outside 4 and 8, Center 8, and Inside 4; live text Outside 4 and 8 and
        // Center 8; all Miter, limit 4.
        if (lib.args.sweep) {
            cases = [];
            for (var s0 = 0; s0 < lib.args.sweep.length; s0++) {
                var sp = String(lib.args.sweep[s0]).split('\t'), sf = sp[0] + '\t' + sp[1], tag = 'sweep ' + (s0 + 1) + ' ', first = cases.length;
                P(tag + 'outside 4 miter', A.OUTSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, null, null, sf);
                P(tag + 'outside 8 miter', A.OUTSIDE_ALIGNMENT, 8, J.MITER_END_JOIN, null, null, sf);
                P(tag + 'center 8 miter', A.CENTER_ALIGNMENT, 8, J.MITER_END_JOIN, null, null, sf);
                P(tag + 'inside 4 miter', A.INSIDE_ALIGNMENT, 4, J.MITER_END_JOIN, null, null, sf);
                T(tag + 'text outside 4 miter', TA.OUTSIDE_ALIGNMENT, 4, null, sf);
                T(tag + 'text outside 8 miter', TA.OUTSIDE_ALIGNMENT, 8, null, sf);
                T(tag + 'text center 8 miter', TA.CENTER_ALIGNMENT, 8, null, sf);
                for (var s1 = first; s1 < cases.length; s1++) { cases[s1].text = sp[2] || null; }
            }
        }

        var twins = 0;
        for (var c0 = 0; c0 < cases.length; c0++) { if (cases[c0].transform && cases[c0].transform.twinWeight) { twins++; } }
        var doc = lib.newDoc(W, 20 + (cases.length + twins) * ROW);
        var baseLayer = doc.activeLayer;
        var geoLayer = doc.layers.add({ name: 'ISE geometry' });
        doc.activeLayer = baseLayer;   // a new layer becomes the active one
        var black = doc.swatches.itemByName('Black'), paper = doc.swatches.itemByName('Paper'), none = doc.swatches.itemByName('None');
        gradient('ISE dark gradient', lib.color(doc, 'ISE black 70', [0, 0, 0, 70]), black);
        gradient('ISE light gradient', paper, lib.color(doc, 'ISE black 20', [0, 0, 0, 20]));
        var page = doc.pages[0];
        var whenScaling = app.transformPreferences.whenScaling;

        function gradient(name, a, b) {
            var g = doc.gradients.add({ name: name, type: GradientType.LINEAR });
            g.gradientStops[0].stopColor = a;
            g.gradientStops[1].stopColor = b;
        }
        function corner(it, option) {
            var corners = ['topLeft', 'topRight', 'bottomLeft', 'bottomRight'];
            for (var q = 0; q < corners.length; q++) {
                it[corners[q] + 'CornerOption'] = option;
                it[corners[q] + 'CornerRadius'] = 4;
            }
        }
        function shadow(s) { s.mode = ShadowMode.DROP; s.xOffset = 4; s.yOffset = 4; s.size = 2; s.opacity = 75; }
        function open(it) { for (var q = 0; q < it.paths.length; q++) { it.paths[q].pathType = PathType.OPEN_PATH; } }
        function flat(items) {
            var r = [];
            for (var q = 0; q < items.length; q++) {
                if (items[q].constructor.name === 'Group') { r = r.concat(flat(items[q].pageItems.everyItem().getElements())); } else { r.push(items[q]); }
            }
            return r;
        }
        var styles = {};
        function textStyle(font) {
            if (!styles[font]) {
                styles[font] = lib.baseStyle(doc, 'ISE census ' + font.split('\t').join(' '), { appliedFont: font, pointSize: 36, strokeWeight: 4, endJoin: EndJoin.MITER_END_JOIN, miterLimit: 4 });
            }
            return styles[font];
        }
        // The base set, `s` times its size, its top left at x, y.
        function baseSet(x, y, s, font, text) {
            var tf = page.textFrames.add({ geometricBounds: [y, x, y + 70 * s, x + 300 * s] });
            tf.textFramePreferences.insetSpacing = [0, 0, 0, 0];
            tf.contents = text || 'AMVWkz';
            tf.parentStory.texts[0].applyParagraphStyle(textStyle(font), true);
            tf.parentStory.texts[0].pointSize = 48 * s;
            var items = flat(tf.createOutlines(true));
            var half = 5 * Math.PI / 180, side = 20 * s, cx = x + 240 * s, tip = y + 25 * s;
            var bw = side * Math.sin(half), by = tip + side * Math.cos(half);
            var thorn = page.polygons.add();
            thorn.paths[0].entirePath = [[cx, tip], [cx + bw, by], [cx + 15 * s, by], [cx + 15 * s, by + 30 * s], [cx - 15 * s, by + 30 * s], [cx - 15 * s, by], [cx - bw, by]];
            thorn.paths[0].pathType = PathType.CLOSED_PATH;
            items.push(thorn);
            var sq = page.polygons.add();
            var x2 = x + 280 * s, y2 = y + 15 * s, a = 40 * s, o = 20 * s;
            sq.paths[0].entirePath = [[x2, y2], [x2 + a, y2], [x2 + a, y2 + a], [x2, y2 + a]];
            sq.paths[0].pathType = PathType.CLOSED_PATH;
            var p2 = sq.paths.add();
            p2.entirePath = [[x2 + o, y2 + o], [x2 + o + a, y2 + o], [x2 + o + a, y2 + o + a], [x2 + o, y2 + o + a]];
            p2.pathType = PathType.CLOSED_PATH;
            items.push(sq);
            return items;
        }
        function matrixOf(it) {
            try { return it.transformValuesOf(CoordinateSpaces.PASTEBOARD_COORDINATES)[0].matrixValues; } catch (e) { return null; }
        }
        function moveTo(it, x, y) { var b = it.visibleBounds; it.move(undefined, [x - b[1], y - b[0]]); }
        // Left to right from x, each item's top at y.
        function layOut(items, x, y) {
            for (var q = 0; q < items.length; q++) {
                var b = items[q].visibleBounds;
                items[q].move(undefined, [x - b[1], y - b[0]]);
                x += b[3] - b[1] + 24;
            }
        }
        function style(it, c) {
            it.fillColor = black;
            it.strokeColor = paper;
            it.strokeWeight = c.w;
            lib.tryDo('alignment ' + c.name, function () { it.strokeAlignment = c.align; });
            it.endJoin = c.join;
            it.miterLimit = 4;
            if (c.extra) { lib.tryDo('extra ' + c.name, function () { c.extra(it, doc); }); }
        }
        function transform(it, tr) {
            lib.tryDo('transform', function () {
                if (tr.keep) { app.transformPreferences.whenScaling = WhenScalingOptions.ADJUST_SCALING_PERCENTAGE; }
                try {
                    if (tr.h) { it.horizontalScale = tr.h; it.verticalScale = tr.v; }
                    if (tr.shear) { it.shearAngle = tr.shear; }
                    if (tr.rotate) { it.rotationAngle = tr.rotate; }
                    if (tr.flip) { it.flipItem(Flip.HORIZONTAL); }
                    if (tr.matrix) { it.transform(CoordinateSpaces.PASTEBOARD_COORDINATES, AnchorPoint.TOP_LEFT_ANCHOR, app.transformationMatrices.add(tr.matrix)); }
                } finally {
                    app.transformPreferences.whenScaling = whenScaling;
                }
            });
        }
        function strip(g) {
            lib.tryDo('geometry opacity', function () { g.transparencySettings.blendingSettings.opacity = 100; g.transparencySettings.dropShadowSettings.mode = ShadowMode.NONE; });
        }
        // The geometry layer: the same item, filled, unstroked, opaque, effect-free.
        function geometry(it) {
            var g = it.duplicate();
            g.itemLayer = geoLayer;
            g.strokeColor = none;
            g.fillColor = black;
            lib.tryDo('geometry alignment', function () { g.strokeAlignment = StrokeAlignment.CENTER_ALIGNMENT; });
            strip(g);
            return g;
        }
        function describe(it) {
            var r = { strokeWeight: it.strokeWeight, matrix: matrixOf(it) };
            lib.tryDo('describe', function () {
                r.align = lib.str(it.strokeAlignment); r.join = lib.str(it.endJoin); r.strokeType = it.strokeType.name;
                r.corner = lib.str(it.topLeftCornerOption); r.opacity = it.transparencySettings.blendingSettings.opacity;
                r.open = []; for (var q = 0; q < it.paths.length; q++) { r.open.push(it.paths[q].pathType === PathType.OPEN_PATH); }
            });
            return r;
        }
        function part(k, n) { return k === n - 1 ? 'squares' : (k === n - 2 ? 'thorn' : 'text'); }

        var row = 0, i, c, k, rec;
        function y0() { return 20 + row * ROW; }
        for (i = 0; i < cases.length; i++) {
            c = cases[i];
            if (c.kind !== 'page item') { continue; }
            var tr = c.transform || null;
            var items = baseSet(16, y0(), tr ? tr.scale : 1, c.font, c.text);
            for (k = 0; k < items.length; k++) {
                style(items[k], c);
                if (tr) { transform(items[k], tr); }
            }
            layOut(items, 16, y0() + 25);
            rec = { row: row, kind: c.kind, label: c.name, font: c.font.split('\t')[0], weight: c.w * (tr && tr.h && tr.h === tr.v && !tr.keep ? tr.h / 100 : 1), cell: [y0(), 0, y0() + ROW, W], items: [] };
            for (k = 0; k < items.length; k++) {
                var g = geometry(items[k]);
                items[k].name = c.name + ' #' + k;
                rec.items.push({ name: items[k].name, part: part(k, items.length), id: items[k].id, geometryId: g.id, geometryBounds: g.visibleBounds, props: describe(items[k]) });
            }
            if (tr && tr.keep) { rec.weight = c.w * tr.h / 100; }
            lib.manifest.items.push(rec);
            row++;
            if (tr && tr.twinWeight) {
                var twinRec = { row: row, kind: c.kind, label: c.name + ' - twin, untransformed', font: rec.font, weight: tr.twinWeight, cell: [y0(), 0, y0() + ROW, W], twinOf: c.name, items: [] };
                for (k = 0; k < items.length; k++) {
                    var src = items[k], tw = page.polygons.add(), paths = src.paths;
                    tw.paths[0].entirePath = paths[0].entirePath;
                    for (var q = 1; q < paths.length; q++) { tw.paths.add().entirePath = paths[q].entirePath; }
                    for (q = 0; q < paths.length; q++) { tw.paths[q].pathType = paths[q].pathType; }
                    style(tw, { name: c.name + ' twin', align: c.align, w: tr.twinWeight, join: c.join });
                    tw.move(undefined, [0, ROW]);
                    var gt = geometry(tw);
                    tw.name = c.name + ' twin #' + k;
                    twinRec.items.push({ name: tw.name, part: part(k, items.length), id: tw.id, geometryId: gt.id, geometryBounds: gt.visibleBounds, props: describe(tw) });
                }
                lib.manifest.items.push(twinRec);
                row++;
            }
        }
        for (i = 0; i < cases.length; i++) {
            c = cases[i];
            if (c.kind !== 'text') { continue; }
            var x = c.extra, host, story;
            if (x.path) {
                host = page.graphicLines.add();
                host.paths[0].entirePath = [[[16, y0() + 80], [16, y0() + 80], [120, y0() + 50]], [[260, y0() + 60], [380, y0() + 80], [380, y0() + 80]]];
                host.strokeColor = none;
                story = host.textPaths.add({ contents: TEXT }).parentStory;
            } else {
                var fw = x.size ? 180 : (x.transform && x.transform.rotate ? 280 : W - 32);
                host = page.textFrames.add({ geometricBounds: [y0() + 25, 16, y0() + 75, 16 + fw] });
                host.textFramePreferences.insetSpacing = [0, 0, 0, 0];
                host.contents = c.text || TEXT;
                story = host.parentStory;
            }
            var tx = story.texts[0];
            tx.applyParagraphStyle(textStyle(c.font), true);
            tx.strokeAlignment = c.align;
            tx.strokeWeight = c.w;
            if (x.size) { tx.pointSize = x.size; }
            if (x.text) { lib.tryDo('text ' + c.name, function () { x.text(tx, doc); }); }
            if (x.frame) { lib.tryDo('frame ' + c.name, function () { x.frame(host); }); }
            if (x.transform) {
                transform(host, x.transform);
                moveTo(host, 16, y0() + 5);
            }
            host.name = c.name;
            // The outline: a copy of the text on the geometry layer, converted.
            var outl;
            if (x.path) {
                // Text on a duplicated path is laid out again, not where it was: outline the
                // text itself, keeping it.
                outl = lib.tryDo('outline ' + c.name, function () { return tx.createOutlines(false); });
                for (k = 0; outl && k < outl.length; k++) { outl[k].itemLayer = geoLayer; }
                outl = outl ? flat(outl) : outl;
            } else {
                var hd = host.duplicate();
                hd.itemLayer = geoLayer;
                strip(hd);
                outl = lib.tryDo('outline ' + c.name, function () { return flat(hd.createOutlines(true)); });
            }
            rec = { row: row, kind: c.kind, label: c.name, font: c.font.split('\t')[0], weight: c.w * (x.transform && x.transform.h ? x.transform.h / 100 : (x.transform && x.transform.factor ? x.transform.factor : 1)), cell: [y0(), 0, y0() + ROW, W], items: [] };
            for (k = 0; outl && k < outl.length; k++) {
                outl[k].fillColor = black;
                outl[k].strokeColor = none;
                strip(outl[k]);
                rec.items.push({ name: c.name + ' outline #' + k, part: 'text', geometryId: outl[k].id, geometryBounds: outl[k].visibleBounds });
            }
            rec.host = { id: host.id, bounds: host.visibleBounds, matrix: matrixOf(host) };
            lib.tryDo('describe text', function () { rec.text = { overset: story.overflows, align: lib.str(tx.strokeAlignment), weight: tx.strokeWeight, size: tx.pointSize, tracking: tx.tracking, skew: tx.skew, hscale: tx.horizontalScale }; });
            lib.manifest.items.push(rec);
            row++;
        }
        lib.manifest.rows = row;
        lib.manifest.rowHeight = ROW;
        new Folder(out).create();
        doc.save(new File(out + '/coverage-census.indd'));
        var hqp = app.pdfExportPresets.itemByName('[High Quality Print]');
        var others = doc.layers.everyItem().getElements();
        function show(geometryOnly) {
            for (var q = 0; q < others.length; q++) { others[q].visible = geometryOnly ? others[q] === geoLayer : others[q] !== geoLayer; }
        }
        var modes = lib.args.modes || [['native', 'mode=native\nmiter=0\nshapes=0\n', false], ['corrected', 'mode=story\nmiter=1\nshapes=1\n', false], ['geometry', 'mode=native\nmiter=0\nshapes=0\n', true]];
        for (var m = 0; m < modes.length; m++) {
            var ctl = new File(lib.args.control);
            ctl.encoding = 'UTF-8';
            ctl.lineFeed = 'Unix';
            ctl.open('w'); ctl.write(modes[m][1]); ctl.close();
            show(modes[m][2]);
            lib.timed('pdf ' + modes[m][0], function () { doc.exportFile(ExportFormat.PDF_TYPE, new File(out + '/coverage-census-' + modes[m][0] + '.pdf'), false, hqp); });
        }
        lib.manifest.app = app.version;
        lib.writeText(out + '/coverage-census.manifest.json', lib.toJSON(lib.manifest));
    }, ['transformPreferences']);
})();
