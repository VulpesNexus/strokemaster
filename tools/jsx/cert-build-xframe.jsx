// cert-build-xframe.jsx - build the cross-frame victim census (cert-X.indd) and its manifest.
// Requires ise-lib.jsx. Argument: ISE_ARGS.dir, the corpus folder; an existing file is refused.
//
// Every case reuses the certification victim: Minion Pro 30 pt, 14 pt leading, a 12 pt Paper
// Outside stroke over Black fill, on the census backdrop (Black 50%), so a stroke that lands on
// another line's fill erases part of it. The cases separate what the frame boundary does from
// what the story does:
//
//   XA*  independent frames (different stories): overlap in both stacking orders, frames that
//        only touch, separate layers, and a headline halo, which is the same geometry as XA0
//        drawn on purpose
//   XB*  threaded frames (one story): the E00 continuation, the upstream frame in front,
//        overlapping frames, an unrelated rectangle stacked between them, a filled downstream
//        frame, bordered frames, two columns of one frame, and a pair split across layers
//   XD*  nesting and state: a threaded pair in a group, a translucent upstream frame, a hidden
//        upstream layer, and a two-row table
//   XN*  negative controls: stroked text over and under a rectangle, Center strokes, and a
//        nonprinting upstream frame
//
// XA3 and XB0 have identical geometry and differ only in whether the two frames share a story.
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var dir = lib.args.dir;
        if (!dir) { lib.L('ERROR need dir'); return; }
        if (new File(dir + '/cert-X.indd').exists) { lib.L('ERROR refusing to overwrite cert-X.indd'); return; }
        var PITCH = 195, W = 170, H = 165;
        function cell(k) { var x = 20 + (k % 4) * PITCH, y = 20 + Math.floor(k / 4) * PITCH; return [y, x, y + H, x + W]; }

        var doc = lib.newDoc(800, 1190);
        var style = lib.baseStyle(doc, 'Cert victim', { pointSize: 30, leading: 14, strokeWeight: 12 });
        var none = doc.swatches.itemByName('None'), paper = doc.swatches.itemByName('Paper');
        var blue = lib.color(doc, 'Probe Blue', [100, 60, 0, 0]), red = lib.color(doc, 'Probe Red', [0, 100, 100, 0]);
        // Layers by name: doc.layers[0] is re-resolved by index and would follow the new top layer.
        var base = doc.layers.itemByName(doc.layers[0].name);
        var upper = doc.layers.add({ name: 'XA4 upper layer' });
        upper.move(LocationOptions.AT_BEGINNING);
        var hidden = doc.layers.add({ name: 'XD2 hidden layer' });
        hidden.move(LocationOptions.AT_END);

        function frame(name, bounds, layer) {
            var tf = doc.pages[0].textFrames.add(layer || base, { geometricBounds: bounds });
            tf.name = name;
            tf.textFramePreferences.insetSpacing = [0, 0, 0, 0];
            tf.textFramePreferences.firstBaselineOffset = FirstBaseline.LEADING_OFFSET;
            return tf;
        }
        function fill(tf, text) {
            tf.contents = text;
            tf.parentStory.texts[0].applyParagraphStyle(style, true);
            return tf;
        }
        // Two frames, the second 14 pt lower: its stroke reaches over the first frame's fill.
        function overlapPair(k, lowerText, upperText) {
            var b = cell(k);
            var lower = fill(frame('lower', [b[0], b[1], b[0] + 40, b[3]]), lowerText || 'HH');
            var up = fill(frame('upper', [b[0] + 14, b[1], b[0] + 54, b[3]]), upperText || 'HH');
            return { lower: lower, upper: up, bounds: [b[0], b[1], b[0] + 54, b[3]] };
        }
        // A threaded pair holding four lines, two per frame: the E00 construction.
        function threadedPair(k, top, opts) {
            opts = opts || {};
            var b = cell(k), y = b[0] + (top || 0);
            var f1 = frame('f1', [y, b[1], y + 30, b[3]], opts.layer1);
            var between = opts.between ? opts.between(b, y) : null;   // stacked above f1, below f2
            var f2 = frame('f2', [y + (opts.overlap ? 20 : 30), b[1], y + (opts.overlap ? 50 : 60), b[3]], opts.layer2);
            f1.nextTextFrame = f2;
            f1.contents = 'HH\rHH\rHH\rHH';
            f1.parentStory.texts[0].applyParagraphStyle(style, true);
            return { f1: f1, f2: f2, between: between, bounds: [y, b[1], y + (opts.overlap ? 50 : 60), b[3]] };
        }
        function name(items, key) { for (var i = 0; i < items.length; i++) { items[i].name = key + (items.length > 1 ? String.fromCharCode(97 + i) : ''); } }
        function variant(key, desc, fn) {
            try {
                var r = fn();
                doc.recompose();
                var rec = { key: key, name: desc, bounds: r.bounds };
                if (r.note) { rec.note = r.note; }
                lib.manifest.frames.push(rec);
            } catch (e) {
                lib.L(key + ' BUILD FAILED: ' + e.message + ' line ' + e.line);
                lib.manifest.frames.push({ key: key, name: desc, buildError: e.message });
            }
        }
        function lineCounts(p) { return 'f1 lines=' + p.f1.lines.length + ' f2 lines=' + p.f2.lines.length + ' overset=' + p.f2.overflows; }

        // ---- A: independent frames
        variant('XA0', 'independent pair, both stroked: the upper frame\'s stroke over the lower frame\'s fill', function () {
            var p = overlapPair(0); name([p.lower, p.upper], 'XA0'); return p;
        });
        variant('XA1', 'independent pair: stroked upper frame over an unstroked lower frame', function () {
            var p = overlapPair(1); p.lower.parentStory.texts[0].strokeColor = none; name([p.lower, p.upper], 'XA1'); return p;
        });
        variant('XA2', 'independent pair: unstroked upper frame over a stroked lower frame (no damage natively)', function () {
            var p = overlapPair(2); p.upper.parentStory.texts[0].strokeColor = none; name([p.lower, p.upper], 'XA2'); return p;
        });
        variant('XA3', 'independent frames that only touch: two stories, the XB0 geometry', function () {
            var b = cell(3);
            var f1 = fill(frame('f1', [b[0], b[1], b[0] + 30, b[3]]), 'HH\rHH');
            var f2 = fill(frame('f2', [b[0] + 30, b[1], b[0] + 60, b[3]]), 'HH\rHH');
            name([f1, f2], 'XA3');
            return { bounds: [b[0], b[1], b[0] + 60, b[3]] };
        });
        variant('XA4', 'independent pair on two layers: the upper frame on the layer above', function () {
            var b = cell(4);
            var lower = fill(frame('lower', [b[0], b[1], b[0] + 40, b[3]], base), 'HH');
            var up = fill(frame('upper', [b[0] + 14, b[1], b[0] + 54, b[3]], upper), 'HH');
            name([lower, up], 'XA4');
            return { bounds: [b[0], b[1], b[0] + 54, b[3]] };
        });
        variant('XA5', 'halo: a headline with a Paper Outside stroke set over another frame\'s body text', function () {
            var b = cell(5);
            var body = frame('body', [b[0], b[1], b[0] + 100, b[3]]);
            body.contents = 'HHHH HHHH HH\rHHHH HHHH HH\rHHHH HHHH HH\rHHHH HHHH HH\rHHHH HHHH HH\rHHHH HHHH HH';
            var bt = body.parentStory.texts[0];
            bt.applyParagraphStyle(style, true);
            bt.pointSize = 14; bt.leading = 16; bt.strokeColor = none;
            var head = frame('headline', [b[0] + 30, b[1] + 10, b[0] + 80, b[3]]);
            head.contents = 'HH';
            var ht = head.parentStory.texts[0];
            ht.applyParagraphStyle(style, true);
            ht.pointSize = 40; ht.leading = 40; ht.strokeWeight = 8; ht.strokeColor = paper;
            name([body, head], 'XA5');
            return { bounds: [b[0], b[1], b[0] + 100, b[3]] };
        });

        // ---- B: threaded frames
        variant('XB0', 'threaded pair: the downstream frame, in front, strokes over the upstream frame\'s last line (E00)', function () {
            var p = threadedPair(8); name([p.f1, p.f2], 'XB0'); lib.L('XB0 ' + lineCounts(p)); return p;
        });
        variant('XB1', 'threaded pair with the upstream frame in front', function () {
            var p = threadedPair(9); p.f1.bringToFront(); name([p.f1, p.f2], 'XB1'); lib.L('XB1 ' + lineCounts(p)); return p;
        });
        variant('XB2', 'threaded pair whose frames overlap by 10 pt', function () {
            var p = threadedPair(10, 0, { overlap: true }); name([p.f1, p.f2], 'XB2'); lib.L('XB2 ' + lineCounts(p)); return p;
        });
        variant('XB3', 'threaded pair with an unrelated opaque rectangle stacked between the frames, over the left half of the seam', function () {
            var p = threadedPair(11, 0, { between: function (b, y) {
                var r = doc.pages[0].rectangles.add(base, { geometricBounds: [y + 18, b[1], y + 42, (b[1] + b[3]) / 2] });
                r.fillColor = blue; r.strokeColor = none; r.name = 'XB3 rectangle';
                return r;
            } });
            name([p.f1, p.f2], 'XB3');
            var order = [], items = doc.pages[0].pageItems;
            for (var i = 0; i < items.length; i++) { if (/^XB3/.test(items[i].name)) { order.push(items[i].name + '@' + items[i].index); } }
            lib.L('XB3 ' + lineCounts(p) + ' pageItems=' + order.join(','));
            return p;
        });
        variant('XB4', 'threaded pair whose downstream frame has a 20% red frame fill', function () {
            var p = threadedPair(12); p.f2.fillColor = red; p.f2.fillTint = 20; name([p.f1, p.f2], 'XB4'); lib.L('XB4 ' + lineCounts(p)); return p;
        });

        variant('XB5', 'threaded pair, both frames with a 1 pt black border', function () {
            var p = threadedPair(20);
            var black = doc.swatches.itemByName('Black');
            p.f1.strokeColor = black; p.f1.strokeWeight = 1; p.f2.strokeColor = black; p.f2.strokeWeight = 1;
            name([p.f1, p.f2], 'XB5'); lib.L('XB5 ' + lineCounts(p)); return p;
        });
        variant('XB6', 'one frame, two columns, no gutter: the second column\'s stroke over the first column\'s fill', function () {
            var b = cell(21), f = frame('XB6', [b[0], b[1], b[0] + 30, b[3]]);
            f.textFramePreferences.textColumnCount = 2;
            f.textFramePreferences.textColumnGutter = 0;
            f.contents = 'HH\rHH\rHH\rHH';
            var paras = f.parentStory.paragraphs;
            f.parentStory.texts[0].applyParagraphStyle(style, true);
            paras[0].justification = Justification.RIGHT_ALIGN; paras[1].justification = Justification.RIGHT_ALIGN;
            doc.recompose();
            lib.L('XB6 columns=' + f.textFramePreferences.textColumnCount + ' lines=' + f.lines.length + ' overset=' + f.overflows);
            return { bounds: [b[0], b[1], b[0] + 30, b[3]] };
        });
        variant('XB7', 'threaded pair split across layers: the downstream frame on the layer above', function () {
            var p = threadedPair(22, 0, { layer2: upper }); name([p.f1, p.f2], 'XB7'); lib.L('XB7 ' + lineCounts(p)); return p;
        });

        // ---- D: nesting and state
        variant('XD0', 'threaded pair inside a group', function () {
            var p = threadedPair(13); var g = doc.pages[0].groups.add([p.f1, p.f2]); g.name = 'XD0 group'; name([p.f1, p.f2], 'XD0'); return p;
        });
        variant('XD1', 'threaded pair with the upstream frame at 50% object opacity', function () {
            var p = threadedPair(14); p.f1.transparencySettings.blendingSettings.opacity = 50; name([p.f1, p.f2], 'XD1'); return p;
        });
        variant('XD2', 'threaded pair with the upstream frame on a hidden layer', function () {
            var p = threadedPair(15, 0, { layer1: hidden }); name([p.f1, p.f2], 'XD2'); return p;
        });
        variant('XD3', 'two-row table: the second row\'s stroke over the first row\'s fill', function () {
            var b = cell(16), f = frame('XD3', [b[0], b[1], b[0] + 60, b[3]]);
            var tbl = f.insertionPoints[0].tables.add({ bodyRowCount: 2, columnCount: 1 });
            tbl.columns[0].width = 160;
            for (var i = 0; i < 2; i++) {
                var row = tbl.rows[i];
                row.autoGrow = false; row.height = 16;
                var c = tbl.cells[i];
                c.topInset = c.bottomInset = c.leftInset = c.rightInset = 0;
                c.topEdgeStrokeWeight = c.bottomEdgeStrokeWeight = c.leftEdgeStrokeWeight = c.rightEdgeStrokeWeight = 0;
                c.firstBaselineOffset = FirstBaseline.LEADING_OFFSET;
                c.contents = 'HH';
                c.texts[0].applyParagraphStyle(style, true);
            }
            doc.recompose();
            lib.L('XD3 cell overflows=' + tbl.cells[0].overflows + ',' + tbl.cells[1].overflows);
            return { bounds: [b[0], b[1], b[0] + 60, b[3]] };
        });

        // ---- N: negative controls
        variant('XN0', 'stroked text over an opaque rectangle: the stroke must still cover the rectangle', function () {
            var b = cell(17);
            var r = doc.pages[0].rectangles.add(base, { geometricBounds: [b[0], b[1], b[0] + 40, b[3]] });
            r.fillColor = blue; r.strokeColor = none; r.name = 'XN0 rectangle';
            var t = fill(frame('XN0', [b[0] + 14, b[1], b[0] + 54, b[3]]), 'HH');
            return { bounds: [b[0], b[1], b[0] + 54, b[3]] };
        });
        variant('XN1', 'opaque rectangle over stroked text: the rectangle must still cover the stroke', function () {
            var b = cell(18);
            var t = fill(frame('XN1', [b[0], b[1], b[0] + 40, b[3]]), 'HH\rHH');
            var r = doc.pages[0].rectangles.add(base, { geometricBounds: [b[0] + 10, b[1] + 40, b[0] + 30, b[1] + 120] });
            r.fillColor = blue; r.strokeColor = none; r.name = 'XN1 rectangle';
            return { bounds: [b[0], b[1], b[0] + 40, b[3]] };
        });
        variant('XN2', 'threaded pair with Center-aligned strokes (left to InDesign)', function () {
            var p = threadedPair(19); p.f1.parentStory.texts[0].strokeAlignment = TextStrokeAlign.CENTER_ALIGNMENT; name([p.f1, p.f2], 'XN2'); return p;
        });
        variant('XN3', 'threaded pair with a nonprinting upstream frame', function () {
            var p = threadedPair(7); p.f1.nonprinting = true; name([p.f1, p.f2], 'XN3'); return p;
        });

        lib.manifest.app = app.version;
        lib.manifest.layers = [upper.name, base.name, hidden.name];
        hidden.visible = false;
        doc.save(new File(dir + '/cert-X.indd'));
        lib.writeText(dir + '/cert-X.manifest.json', lib.toJSON(lib.manifest));
        lib.L('cert-X saved variants=' + lib.manifest.frames.length);
    });
})();
