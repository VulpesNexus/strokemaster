// cert-build-fixtures.jsx - build the route W certification fixtures beyond the victim census.
// Requires ise-lib.jsx. Argument: ISE_ARGS.dir, the corpus folder; existing files are refused.
//
// Every variant reuses the census victim: 'HH\rHH' in Minion Pro 30 pt with 14 pt leading and a
// 12 pt Outside stroke, so each line's stroke reaches over the other line's fill. InDesign's own
// drawing covers the fills with the stroke; route W should not. Each fixture is one page over
// the census backdrop (Black 50%), and each variant records its key and bounds in a manifest
// beside the document, so the measurements can crop per variant.
//
//   cert-T  transparency and flattening: opacity (text, object, group), blend modes, effects,
//           clipping, transparent artwork above and below
//   cert-C  color: process, spot, RGB, tints, gradient stroke, overprint stroke
//   cert-E  scope and editing: threaded frames, independent overlapping frames, mixed fonts
//           and runs, overset, transforms, a table cell, an anchored frame, text on a path
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var dir = lib.args.dir;
        if (!dir) { lib.L('ERROR need dir'); return; }
        var names = ['cert-T', 'cert-C', 'cert-E'];
        for (var n = 0; n < names.length; n++) {
            if (new File(dir + '/' + names[n] + '.indd').exists) { lib.L('ERROR refusing to overwrite ' + names[n] + '.indd'); return; }
        }
        var PITCH = 195, W = 170, H = 165;
        function cell(k) { var x = 20 + (k % 4) * PITCH, y = 20 + Math.floor(k / 4) * PITCH; return [y, x, y + H, x + W]; }

        function start(w, h) {
            var doc = lib.newDoc(w, h);
            var style = lib.baseStyle(doc, 'Cert victim', { pointSize: 30, leading: 14, strokeWeight: 12 });
            return { doc: doc, style: style, none: doc.swatches.itemByName('None'),
                red: lib.color(doc, 'Probe Red', [0, 100, 100, 0]), blue: lib.color(doc, 'Probe Blue', [100, 60, 0, 0]) };
        }
        function victim(c, key, bounds, text) {
            var tf = lib.frame(c.doc, key, bounds, text || 'HH\rHH', c.style);
            c.doc.recompose();
            return tf;
        }
        function variant(c, key, desc, fn) {
            try {
                var item = fn();
                c.doc.recompose();
                var gb = item.geometricBounds, over = false;
                try { over = item.overflows; } catch (e0) {}
                lib.manifest.frames.push({ key: key, name: desc, bounds: [gb[0], gb[1], gb[2], gb[3]], overflows: over });
            } catch (e) {
                lib.L(key + ' BUILD FAILED: ' + e.message + ' line ' + e.line);
                lib.manifest.frames.push({ key: key, name: desc, buildError: e.message });
            }
        }
        function finish(c, name) {
            lib.manifest.app = app.version;
            c.doc.save(new File(dir + '/' + name + '.indd'));
            lib.writeText(dir + '/' + name + '.manifest.json', lib.toJSON(lib.manifest));
            lib.L(name + ' saved variants=' + lib.manifest.frames.length);
        }
        function rect(c, bounds, color, tint) {
            var r = c.doc.pages[0].rectangles.add({ geometricBounds: bounds });
            r.fillColor = color;
            r.fillTint = tint === undefined ? 100 : tint;
            r.strokeColor = c.none;
            return r;
        }

        // ---- cert-T: transparency and flattening
        var t = start(800, 600);
        variant(t, 'T00', 'no transparency (control)', function () { return victim(t, 'T00', cell(0)); });
        variant(t, 'T01', 'text opacity 50%', function () { var f = victim(t, 'T01', cell(1)); f.contentTransparencySettings.blendingSettings.opacity = 50; return f; });
        variant(t, 'T02', 'object opacity 50%', function () { var f = victim(t, 'T02', cell(2)); f.transparencySettings.blendingSettings.opacity = 50; return f; });
        variant(t, 'T03', 'group opacity 50%', function () {
            var f = victim(t, 'T03', cell(3)), b = cell(3);
            var r = rect(t, [b[2] - 20, b[1], b[2], b[3]], t.blue);
            var g = t.doc.pages[0].groups.add([f, r]);
            g.transparencySettings.blendingSettings.opacity = 50;
            return f;
        });
        variant(t, 'T04', 'transparent multiply rectangle above', function () {
            var f = victim(t, 'T04', cell(4)), b = cell(4);
            var r = rect(t, [b[0], b[1], b[2], (b[1] + b[3]) / 2], t.red);
            r.transparencySettings.blendingSettings.opacity = 50;
            r.transparencySettings.blendingSettings.blendMode = BlendMode.MULTIPLY;
            r.bringToFront();
            return f;
        });
        variant(t, 'T05', 'transparent rectangle below', function () {
            var b = cell(5), r = rect(t, [b[0], b[1], b[2], b[3]], t.blue);
            r.transparencySettings.blendingSettings.opacity = 60;
            var f = victim(t, 'T05', b);
            f.bringToFront();
            return f;
        });
        variant(t, 'T06', 'object blend mode Multiply', function () { var f = victim(t, 'T06', cell(6)); f.transparencySettings.blendingSettings.blendMode = BlendMode.MULTIPLY; return f; });
        variant(t, 'T07', 'drop shadow', function () {
            var f = victim(t, 'T07', cell(7)), ds = f.transparencySettings.dropShadowSettings;
            ds.mode = ShadowMode.DROP; ds.xOffset = 4; ds.yOffset = 4; ds.size = 3; ds.opacity = 75;
            return f;
        });
        variant(t, 'T08', 'basic feather', function () {
            var f = victim(t, 'T08', cell(8)), fs = f.transparencySettings.featherSettings;
            fs.mode = FeatherMode.STANDARD; fs.width = 3;
            return f;
        });
        variant(t, 'T09', 'frame clipped inside a rectangle', function () {
            var b = cell(9), r = rect(t, [b[0], b[1], b[2] - 40, b[3]], t.none);
            var f = r.textFrames.add({ geometricBounds: b });
            f.name = 'T09';
            f.textFramePreferences.insetSpacing = [0, 0, 0, 0];
            f.contents = 'HH\rHH';
            f.parentStory.texts[0].applyParagraphStyle(t.style, true);
            return f;
        });
        variant(t, 'T10', 'object blend mode Screen over blue', function () {
            var b = cell(10);
            rect(t, [b[0], b[1], b[2], b[3]], t.blue);
            var f = victim(t, 'T10', b);
            f.bringToFront();
            f.transparencySettings.blendingSettings.blendMode = BlendMode.SCREEN;
            return f;
        });
        variant(t, 'T11', 'gradient feather', function () {
            var f = victim(t, 'T11', cell(11)), gf = f.transparencySettings.gradientFeatherSettings;
            gf.applied = true; gf.angle = 90;
            return f;
        });
        finish(t, 'cert-T');

        // ---- cert-C: color
        var c = start(800, 405);
        var spotA = c.doc.colors.add({ name: 'Cert Spot Orange', model: ColorModel.SPOT, space: ColorSpace.CMYK, colorValue: [0, 50, 100, 0] });
        var spotB = c.doc.colors.add({ name: 'Cert Spot Violet', model: ColorModel.SPOT, space: ColorSpace.CMYK, colorValue: [60, 90, 0, 0] });
        var rgbA = c.doc.colors.add({ name: 'Cert RGB Green', model: ColorModel.PROCESS, space: ColorSpace.RGB, colorValue: [0, 200, 80] });
        var rgbB = c.doc.colors.add({ name: 'Cert RGB Navy', model: ColorModel.PROCESS, space: ColorSpace.RGB, colorValue: [10, 20, 120] });
        function colored(key, k, fill, stroke, extra) {
            var f = victim(c, key, cell(k)), tx = f.parentStory.texts[0];
            tx.fillColor = fill; tx.strokeColor = stroke;
            if (extra) { extra(tx); }
            return f;
        }
        variant(c, 'C00', 'Paper stroke over black fill (census colors)', function () { return victim(c, 'C00', cell(0)); });
        variant(c, 'C01', 'process fill and stroke', function () { return colored('C01', 1, c.red, c.blue); });
        variant(c, 'C02', 'spot stroke over process fill', function () { return colored('C02', 2, c.doc.swatches.itemByName('Black'), spotA); });
        variant(c, 'C03', 'spot fill and spot stroke', function () { return colored('C03', 3, spotB, spotA); });
        variant(c, 'C04', 'RGB fill and stroke', function () { return colored('C04', 4, rgbB, rgbA); });
        variant(c, 'C05', 'tints: fill 70%, stroke 50%', function () { return colored('C05', 5, c.blue, c.red, function (tx) { tx.fillTint = 70; tx.strokeTint = 50; }); });
        variant(c, 'C06', 'gradient stroke', function () {
            var g = c.doc.gradients.add({ name: 'Cert Gradient', type: GradientType.LINEAR });
            g.gradientStops[0].stopColor = c.red; g.gradientStops[1].stopColor = spotA;
            return colored('C06', 6, c.doc.swatches.itemByName('Black'), g);
        });
        variant(c, 'C07', 'overprint stroke', function () { return colored('C07', 7, c.blue, c.red, function (tx) { tx.overprintStroke = true; }); });
        finish(c, 'cert-C');

        // ---- cert-E: scope and editing
        var e = start(800, 795);
        var myriad = lib.fontOk('Myriad Pro\tRegular') ? 'Myriad Pro\tRegular' : null;
        lib.L('Myriad Pro available=' + !!myriad);
        variant(e, 'E00', 'threaded pair: last line of frame 1 above first line of frame 2', function () {
            var b = cell(0);
            var f1 = e.doc.pages[0].textFrames.add({ geometricBounds: [b[0], b[1], b[0] + 30, b[3]] });
            var f2 = e.doc.pages[0].textFrames.add({ geometricBounds: [b[0] + 30, b[1], b[0] + 60, b[3]] });
            f1.name = 'E00a'; f2.name = 'E00b';
            f1.textFramePreferences.insetSpacing = [0, 0, 0, 0]; f2.textFramePreferences.insetSpacing = [0, 0, 0, 0];
            f1.textFramePreferences.firstBaselineOffset = FirstBaseline.LEADING_OFFSET;
            f2.textFramePreferences.firstBaselineOffset = FirstBaseline.LEADING_OFFSET;
            f1.nextTextFrame = f2;
            f1.contents = 'HH\rHH\rHH\rHH';
            f1.parentStory.texts[0].applyParagraphStyle(e.style, true);
            e.doc.recompose();
            lib.L('E00 lines frame1=' + f1.lines.length + ' frame2=' + f2.lines.length);
            var g = [f1.geometricBounds, f2.geometricBounds];
            return { geometricBounds: [g[0][0], g[0][1], g[1][2], g[1][3]], overflows: f2.overflows };
        });
        variant(e, 'E01', 'same four lines in one frame (control for E00)', function () {
            var b = cell(1), f = e.doc.pages[0].textFrames.add({ geometricBounds: [b[0], b[1], b[0] + 60, b[3]] });
            f.name = 'E01';
            f.textFramePreferences.insetSpacing = [0, 0, 0, 0];
            f.textFramePreferences.firstBaselineOffset = FirstBaseline.LEADING_OFFSET;
            f.contents = 'HH\rHH\rHH\rHH';
            f.parentStory.texts[0].applyParagraphStyle(e.style, true);
            return f;
        });
        variant(e, 'E02', 'independent frames, second stroke over first fill', function () {
            var b = cell(2);
            var f1 = victim(e, 'E02a', [b[0], b[1], b[0] + 40, b[3]], 'HH');
            var f2 = victim(e, 'E02b', [b[0] + 14, b[1], b[0] + 54, b[3]], 'HH');
            return { geometricBounds: [b[0], b[1], b[0] + 54, b[3]] };
        });
        variant(e, 'E03', 'mixed runs: fonts, weights, colors, unstroked', function () {
            var f = victim(e, 'E03', cell(3), 'HHHH\rHHHH'), ch = f.parentStory.characters;
            if (myriad) { ch[1].appliedFont = myriad; ch[6].appliedFont = myriad; }
            ch[2].strokeWeight = 4; ch[3].strokeColor = e.red; ch[5].strokeColor = e.none; ch[8].strokeColor = e.blue; ch[8].strokeWeight = 8;
            return f;
        });
        variant(e, 'E04', 'overset: third paragraph does not fit', function () {
            var b = cell(4), f = victim(e, 'E04', [b[0], b[1], b[0] + 45, b[3]], 'HH\rHH\rHH');
            return f;
        });
        variant(e, 'E05', 'rotated 30 degrees', function () { var f = victim(e, 'E05', cell(5)); f.rotationAngle = 30; return f; });
        variant(e, 'E06', 'scaled 110% horizontally, 80% vertically', function () {
            var f = victim(e, 'E06', cell(6));
            f.transform(CoordinateSpaces.PASTEBOARD_COORDINATES, AnchorPoint.TOP_LEFT_ANCHOR, app.transformationMatrices.add({ horizontalScaleFactor: 1.1, verticalScaleFactor: 0.8 }));
            return f;
        });
        variant(e, 'E07', 'sheared 20 degrees', function () { var f = victim(e, 'E07', cell(7)); f.shearAngle = 20; return f; });
        variant(e, 'E08', 'table cell', function () {
            var b = cell(8), f = e.doc.pages[0].textFrames.add({ geometricBounds: b });
            f.name = 'E08';
            var tbl = f.insertionPoints[0].tables.add({ bodyRowCount: 1, columnCount: 1 });
            tbl.columns[0].width = 160;
            var cl = tbl.cells[0];
            cl.topInset = cl.bottomInset = cl.leftInset = cl.rightInset = 0;
            cl.contents = 'HH\rHH';
            cl.texts[0].applyParagraphStyle(e.style, true);
            return f;
        });
        variant(e, 'E09', 'anchored inline frame', function () {
            var b = cell(9), host = e.doc.pages[0].textFrames.add({ geometricBounds: b });
            host.name = 'E09';
            host.contents = 'x';
            var a = host.insertionPoints[0].textFrames.add({ geometricBounds: [0, 0, 60, 150] });
            a.name = 'E09anchored';
            a.textFramePreferences.insetSpacing = [0, 0, 0, 0];
            a.contents = 'HH\rHH';
            a.parentStory.texts[0].applyParagraphStyle(e.style, true);
            return host;
        });
        variant(e, 'E10', 'text on a path', function () {
            var b = cell(10), ov = e.doc.pages[0].ovals.add({ geometricBounds: [b[0] + 20, b[1] + 10, b[2] - 20, b[3] - 10] });
            ov.strokeColor = e.none; ov.fillColor = e.none; ov.name = 'E10';
            var tp = ov.textPaths.add();
            tp.contents = 'HHHHHHHH';
            tp.parentStory.texts[0].applyParagraphStyle(e.style, true);
            tp.parentStory.texts[0].leading = 30;
            return ov;
        });
        variant(e, 'E11', 'stroked text over another frame\'s fill, stroke frame behind', function () {
            var b = cell(11);
            var back = victim(e, 'E11a', [b[0] + 14, b[1], b[0] + 54, b[3]], 'HH');
            var front = victim(e, 'E11b', [b[0], b[1], b[0] + 40, b[3]], 'HH');
            back.bringToFront();
            return { geometricBounds: [b[0], b[1], b[0] + 54, b[3]] };
        });
        finish(e, 'cert-E');
    });
})();
