// build-N-transparency.jsx - transparency, blend mode, gradient and overprint sweep (fixture N).
// Requires ise-lib.jsx. Argument: ISE_ARGS.outRoot.
//
// This fixture exists to answer one question: does repainting a run's fill in the foreground
// pass change anything it should not, when the fill is not a plain opaque solid? A second paint
// of even an opaque Normal fill can change partial pixel coverage. A blend mode, opacity or overprint
// composites against what is already there, so a second paint of the same ink can land at a
// different color than the first. That is a harm, not a correction, and it is invisible to any
// measurement that only counts covered pixels.
//
// Two palettes are used deliberately:
//   classic  Black fill, Paper stroke. Comparable with fixture A, and the reference for the
//            correction itself.
//   mid      50 % black fill, 60 % magenta stroke. Chosen so that compositing the same ink
//            twice lands at a visibly different color: Multiply of black over anything is
//            still black, so a black fill would hide exactly the defect being looked for.
//
// The last variants are controls the adornment must leave alone: a Center stroke is painted
// over its own fill by definition, and a run with no stroke has nothing to correct. If those
// change between a run with the plugin and a run without it, the adornment is reaching runs it
// has no business touching, whatever the other numbers say.
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var dir = lib.args.outRoot + '/N-transparency';
        var doc = lib.newDoc(612, 900);

        var paper = doc.swatches.itemByName('Paper');
        var black = doc.swatches.itemByName('Black');
        var midFill = lib.color(doc, 'ISE Mid Fill', [0, 0, 0, 50]);
        var midStroke = lib.color(doc, 'ISE Mid Stroke', [0, 60, 0, 0]);
        var spot = lib.tryDo('spot', function () {
            var s = doc.colors.itemByName('ISE Spot');
            if (s.isValid) { return s; }
            return doc.colors.add({ name: 'ISE Spot', model: ColorModel.SPOT, space: ColorSpace.CMYK, colorValue: [0, 80, 80, 0] });
        });

        var gradFill = lib.tryDo('gradient fill', function () {
            var g = doc.gradients.add({ name: 'ISE Gradient Fill', type: GradientType.LINEAR });
            g.gradientStops[0].stopColor = black;
            g.gradientStops[0].location = 0;
            g.gradientStops[1].stopColor = midStroke;
            g.gradientStops[1].location = 100;
            return g;
        });
        var gradStroke = lib.tryDo('gradient stroke', function () {
            var g = doc.gradients.add({ name: 'ISE Gradient Stroke', type: GradientType.LINEAR });
            g.gradientStops[0].stopColor = paper;
            g.gradientStops[0].location = 0;
            g.gradientStops[1].stopColor = midStroke;
            g.gradientStops[1].location = 100;
            return g;
        });
        lib.L('gradFill=' + lib.str(gradFill) + ' gradStroke=' + lib.str(gradStroke) + ' spot=' + lib.str(spot));

        // Stroke weight 8 pt at 24 pt on 28.8 pt leading is fixture A's geometry, where the
        // second line's stroke provably reaches the first line's fill.
        var classic = lib.baseStyle(doc, 'ISE N classic', { strokeWeight: 8 });
        var mid = lib.baseStyle(doc, 'ISE N mid', { strokeWeight: 8, fillColor: midFill, strokeColor: midStroke });

        // Ascenders and descenders on both lines, so the two lines' ink provably meets.
        var TEXT = 'Hxjq\rHxjq';

        function blend(tf, target, mode, opacity) {
            var t = tf[target];
            if (mode !== null) { t.blendingSettings.blendMode = mode; }
            if (opacity !== null) { t.blendingSettings.opacity = opacity; }
            return lib.str(t.blendingSettings.blendMode) + '@' + t.blendingSettings.opacity;
        }

        // [key, label, palette, apply(textFrame, story) -> note]
        var V = [
            ['N00', 'control: Normal, opaque, solid', 'classic', null],
            ['N01', 'text blend Multiply', 'mid', function (tf) { return blend(tf, 'contentTransparencySettings', BlendMode.MULTIPLY, null); }],
            ['N02', 'text blend Screen', 'mid', function (tf) { return blend(tf, 'contentTransparencySettings', BlendMode.SCREEN, null); }],
            ['N03', 'text blend Overlay', 'mid', function (tf) { return blend(tf, 'contentTransparencySettings', BlendMode.OVERLAY, null); }],
            ['N04', 'text blend Darken', 'mid', function (tf) { return blend(tf, 'contentTransparencySettings', BlendMode.DARKEN, null); }],
            ['N05', 'text blend Lighten', 'mid', function (tf) { return blend(tf, 'contentTransparencySettings', BlendMode.LIGHTEN, null); }],
            ['N06', 'text blend Difference', 'mid', function (tf) { return blend(tf, 'contentTransparencySettings', BlendMode.DIFFERENCE, null); }],
            ['N07', 'text opacity 50 %', 'mid', function (tf) { return blend(tf, 'contentTransparencySettings', null, 50); }],
            ['N08', 'text blend Multiply at 50 %', 'mid', function (tf) { return blend(tf, 'contentTransparencySettings', BlendMode.MULTIPLY, 50); }],
            ['N09', 'object blend Multiply', 'mid', function (tf) { return blend(tf, 'transparencySettings', BlendMode.MULTIPLY, null); }],
            ['N10', 'object opacity 50 %', 'mid', function (tf) { return blend(tf, 'transparencySettings', null, 50); }],
            ['N11', 'gradient fill, solid stroke', 'classic', gradFill ? function (tf, s) { s.texts[0].fillColor = gradFill; return 'gradient fill'; } : 'skip'],
            ['N12', 'solid fill, gradient stroke', 'classic', gradStroke ? function (tf, s) { s.texts[0].strokeColor = gradStroke; return 'gradient stroke'; } : 'skip'],
            ['N13', 'gradient fill and gradient stroke', 'classic', (gradFill && gradStroke) ? function (tf, s) { s.texts[0].fillColor = gradFill; s.texts[0].strokeColor = gradStroke; return 'both gradient'; } : 'skip'],
            ['N14', 'overprint fill', 'mid', function (tf, s) { s.texts[0].overprintFill = true; return 'overprintFill=' + s.texts[0].overprintFill; }],
            ['N15', 'overprint stroke', 'mid', function (tf, s) { s.texts[0].overprintStroke = true; return 'overprintStroke=' + s.texts[0].overprintStroke; }],
            ['N16', 'overprint fill and stroke', 'mid', function (tf, s) { s.texts[0].overprintFill = true; s.texts[0].overprintStroke = true; return 'both overprint'; }],
            ['N17', 'spot color fill, overprint', 'classic', spot ? function (tf, s) { s.texts[0].fillColor = spot; s.texts[0].overprintFill = true; return 'spot overprint'; } : 'skip'],
            ['N18', 'drop shadow on text', 'mid', function (tf) {
                var d = tf.contentTransparencySettings.dropShadowSettings;
                d.mode = ShadowMode.DROP;
                d.blendMode = BlendMode.MULTIPLY;
                return 'shadow mode=' + lib.str(d.mode);
            }],
            ['N19', 'feather on text', 'mid', function (tf) {
                var f = tf.contentTransparencySettings.featherSettings;
                f.mode = FeatherMode.STANDARD;
                f.width = 3;
                return 'feather mode=' + lib.str(f.mode);
            }],
            ['N20', 'CONTROL: Center stroke, blend Multiply', 'mid', function (tf, s) {
                s.texts[0].strokeAlignment = TextStrokeAlign.CENTER_ALIGNMENT;
                return blend(tf, 'contentTransparencySettings', BlendMode.MULTIPLY, null) + ' center';
            }],
            ['N21', 'CONTROL: no stroke, blend Multiply', 'mid', function (tf, s) {
                // None suppresses paint; this control does not require zeroing retained weight.
                // The takeover text-range probe separately verifies explicit None plus zero.
                s.texts[0].strokeColor = doc.swatches.itemByName('None');
                return blend(tf, 'contentTransparencySettings', BlendMode.MULTIPLY, null) + ' no stroke';
            }],
            ['N22', 'CONTROL: no stroke, gradient fill', 'classic', gradFill ? function (tf, s) {
                s.texts[0].strokeColor = doc.swatches.itemByName('None');
                s.texts[0].fillColor = gradFill;
                return 'no stroke, gradient fill';
            } : 'skip']
        ];

        var cols = [24, 222, 420], colW = 170, rowH = 100, top = 20, frameH = 76;
        for (var i = 0; i < V.length; i++) {
            var id = V[i][0], label = V[i][1], palette = V[i][2], how = V[i][3];
            var col = i % 3, row = Math.floor(i / 3);
            var y = top + row * rowH, x = cols[col];
            var ps = (palette === 'mid') ? mid : classic;
            var tf = lib.frame(doc, id + ' ' + label, [y, x, y + frameH, x + colW], TEXT, ps);
            var story = tf.parentStory, status = 'applied', note = '';
            if (how === 'skip') {
                status = 'skipped: prerequisite unavailable';
            } else if (how) {
                note = lib.tryDo(id, function () { return how(tf, story) || 'applied'; });
                if (note === undefined) { status = 'FAILED (see log)'; note = ''; }
            }
            lib.L(id + ' ' + status + ' ' + note);
            lib.record(tf, label, { key: id, status: status, palette: palette, note: String(note), measure: 'hidden' });
        }
        lib.saveExport(doc, dir, 'N-transparency', 300);
    });
})();
