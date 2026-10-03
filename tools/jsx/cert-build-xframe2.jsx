// cert-build-xframe2.jsx - build the cross-frame extension census (cert-X2.indd) and its manifest.
// Requires ise-lib.jsx. Argument: ISE_ARGS.dir, the corpus folder; an existing file is refused.
//
// The cases the cross-frame research (CROSS_FRAME_RESEARCH_2026-09-26.md, section 12) left
// untested, and neighbors of them. Every case uses the cert-X victim (Minion Pro 30 pt, 14 pt
// leading, 12 pt Paper Outside stroke over Black) and the E00 geometry unless stated: one story
// threaded through two frames, the downstream frame's first line stroking over the upstream
// frame's last line, so native drawing erases part of that fill.
//
//   XE0  threaded pair on the parent page, drawn on the document page
//   XE1  path text threaded into a frame below it
//   XE2  threaded pair of inline anchored frames in a host frame
//   XE3  threaded pair rotated 30 degrees as a unit
//   XE4  three frames threaded in a chain, each over the one before
//   XE5  threaded pair whose downstream frame is scaled 125%
//   XE6  threaded pair with an invisible rectangle (no fill, no stroke) stacked between the
//        frames over the seam: nothing is drawn in between, so the pair must still be corrected
//   XE7  threaded pair inside a group nested in another group
//   XE8  XE6 with a 1 pt black border on the rectangle: content drawn in between, so glyphs
//        under the border keep InDesign's order (partial, like cert-X XB3)
//
// Parent-page items are drawn beneath every page item, so the census backdrop (Black 50%) lives
// on the parent page here, not on the document page as in the other corpus files.
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var dir = lib.args.dir;
        if (!dir) { lib.L('ERROR need dir'); return; }
        if (new File(dir + '/cert-X2.indd').exists) { lib.L('ERROR refusing to overwrite cert-X2.indd'); return; }
        var PITCH = 195, W = 170, H = 165;
        function cell(k) { var x = 20 + (k % 4) * PITCH, y = 20 + Math.floor(k / 4) * PITCH; return [y, x, y + H, x + W]; }

        var PW = 800, PH = 600;   // three rows of cells
        var doc = lib.newDoc(PW, PH);
        // Move the backdrop to the parent page.
        var pageBackdrop = doc.pages[0].rectangles[0];
        pageBackdrop.locked = false;
        pageBackdrop.remove();
        var parent = doc.masterSpreads[0], parentPage = parent.pages[0];
        parentPage.marginPreferences.properties = { top: 0, left: 0, bottom: 0, right: 0 };
        var bg = parentPage.rectangles.add({ geometricBounds: [0, 0, PH, PW] });
        bg.fillColor = doc.swatches.itemByName('Black'); bg.fillTint = 50;
        bg.strokeColor = doc.swatches.itemByName('None'); bg.strokeWeight = 0;
        bg.name = 'backdrop: Black 50% tint, opaque, on the parent page';
        doc.pages[0].appliedMaster = parent;

        var style = lib.baseStyle(doc, 'Cert victim', { pointSize: 30, leading: 14, strokeWeight: 12 });
        var none = doc.swatches.itemByName('None');
        var base = doc.layers.itemByName(doc.layers[0].name);

        function frame(name, bounds, container) {
            var tf = (container || doc.pages[0]).textFrames.add(base, { geometricBounds: bounds });
            tf.name = name;
            tf.textFramePreferences.insetSpacing = [0, 0, 0, 0];
            tf.textFramePreferences.firstBaselineOffset = FirstBaseline.LEADING_OFFSET;
            return tf;
        }
        function styleStory(story) { story.texts[0].applyParagraphStyle(style, true); }
        // The E00 pair: four lines, two per frame, the second frame directly below the first.
        function threadedPair(k, container, f2Top) {
            var b = cell(k), y = b[0];
            var f1 = frame('f1', [y, b[1], y + 30, b[3]], container);
            var f2 = frame('f2', [y + (f2Top || 30), b[1], y + (f2Top || 30) + 30, b[3]], container);
            f1.nextTextFrame = f2;
            f1.contents = 'HH\rHH\rHH\rHH';
            styleStory(f1.parentStory);
            return { f1: f1, f2: f2, bounds: [y, b[1], y + 60, b[3]] };
        }
        function name(items, key) { for (var i = 0; i < items.length; i++) { items[i].name = key + String.fromCharCode(97 + i); } }
        function union(items) {
            var u = null;
            for (var i = 0; i < items.length; i++) {
                var v = items[i].visibleBounds;
                u = u ? [Math.min(u[0], v[0]), Math.min(u[1], v[1]), Math.max(u[2], v[2]), Math.max(u[3], v[3])] : v;
            }
            return u;
        }
        function variant(key, desc, fn) {
            try {
                var r = fn();
                doc.recompose();
                var rec = { key: key, name: desc, bounds: r.bounds };
                if (r.note) { rec.note = r.note; }
                lib.manifest.frames.push(rec);
                lib.L(key + ' built' + (r.note ? ' ' + r.note : ''));
            } catch (e) {
                lib.L(key + ' BUILD FAILED: ' + e.message + ' line ' + e.line);
                lib.manifest.frames.push({ key: key, name: desc, buildError: e.message });
            }
        }
        function lines(f) { return f.lines.length; }

        variant('XE0', 'threaded pair on the parent page, drawn on the document page', function () {
            var p = threadedPair(0, parentPage);
            name([p.f1, p.f2], 'XE0');
            return { bounds: p.bounds, note: 'f1 lines=' + lines(p.f1) + ' f2 lines=' + lines(p.f2) };
        });
        variant('XE1', 'path text threaded into a frame: the frame\'s first line strokes over the path text', function () {
            var b = cell(1), y = b[0];
            var line = doc.pages[0].graphicLines.add(base, { geometricBounds: [y + 28, b[1], y + 28, b[3]] });
            line.strokeColor = none; line.strokeWeight = 0;
            var tp = line.textPaths.add();
            var f2 = frame('f2', [y + 30, b[1], y + 60, b[3]]);
            tp.nextTextFrame = f2;
            tp.contents = 'HH\rHH\rHH';
            styleStory(tp.parentStory);
            line.name = 'XE1a path'; f2.name = 'XE1b';
            return { bounds: [y, b[1], y + 60, b[3]], note: 'path chars=' + tp.characters.length + ' f2 lines=' + lines(f2) };
        });
        variant('XE2', 'threaded pair of inline anchored frames in an unstroked host frame', function () {
            var b = cell(2), y = b[0];
            var host = frame('XE2 host', [y, b[1], y + 70, b[3]]);
            host.contents = ' \r ';
            var ht = host.parentStory.texts[0];
            ht.applyParagraphStyle(style, true);
            ht.strokeColor = none; ht.fillColor = none; ht.pointSize = 6; ht.autoLeading = 100; ht.leading = 30;
            var fr = [];
            for (var i = 0; i < 2; i++) {
                // Fetched fresh each time: a stored insertion point re-resolves after an insertion.
                var f = host.parentStory.paragraphs[i].insertionPoints[0].textFrames.add();
                f.geometricBounds = [0, 0, 30, W];
                f.textFramePreferences.insetSpacing = [0, 0, 0, 0];
                f.textFramePreferences.firstBaselineOffset = FirstBaseline.LEADING_OFFSET;
                // Custom anchored position: InDesign 21.6 refused to thread two inline frames
                // ("Invalid object for this request").
                var a = f.anchoredObjectSettings;
                a.anchoredPosition = AnchorPosition.ANCHORED;
                a.anchorPoint = AnchorPoint.TOP_LEFT_ANCHOR;
                a.horizontalReferencePoint = AnchoredRelativeTo.TEXT_FRAME;
                a.horizontalAlignment = HorizontalAlignment.LEFT_ALIGN;
                a.verticalReferencePoint = VerticallyRelativeTo.TEXT_FRAME;
                a.verticalAlignment = VerticalAlignment.TOP_ALIGN;
                a.anchorXoffset = 0;
                a.anchorYoffset = 30 * i;
                fr.push(f);
            }
            fr[0].nextTextFrame = fr[1];
            fr[0].contents = 'HH\rHH\rHH\rHH';
            styleStory(fr[0].parentStory);
            name(fr, 'XE2');
            doc.recompose();
            return { bounds: [y, b[1], y + 70, b[3]],
                     note: 'anchored tops=' + fr[0].geometricBounds[0] + ',' + fr[1].geometricBounds[0] + ' lines=' + lines(fr[0]) + ',' + lines(fr[1]) };
        });
        variant('XE3', 'threaded pair rotated 30 degrees as a unit', function () {
            var p = threadedPair(3);
            var g = doc.pages[0].groups.add([p.f1, p.f2]);
            g.rotationAngle = 30;
            g.move(undefined, [0, 50]);   // the rotated pair reaches 38 pt above its cell
            g.ungroup();
            name([p.f1, p.f2], 'XE3');
            return { bounds: union([p.f1, p.f2]), note: 'rotation=' + p.f1.rotationAngle + ',' + p.f2.rotationAngle };
        });
        variant('XE4', 'three frames threaded in a chain, each downstream frame over the one before', function () {
            var b = cell(4), y = b[0], f = [];
            for (var i = 0; i < 3; i++) { f.push(frame('f' + i, [y + 30 * i, b[1], y + 30 * (i + 1), b[3]])); }
            f[0].nextTextFrame = f[1]; f[1].nextTextFrame = f[2];
            f[0].contents = 'HH\rHH\rHH\rHH\rHH\rHH';
            styleStory(f[0].parentStory);
            name(f, 'XE4');
            return { bounds: [y, b[1], y + 90, b[3]], note: 'lines=' + lines(f[0]) + ',' + lines(f[1]) + ',' + lines(f[2]) };
        });
        variant('XE5', 'threaded pair whose downstream frame is scaled 125% about its center', function () {
            var b = cell(5), y = b[0], x0 = b[1] + 20, x1 = b[3] - 20;
            var f1 = frame('f1', [y, x0, y + 30, x1]);
            var f2 = frame('f2', [y + 34, x0, y + 64, x1]);
            f1.nextTextFrame = f2;
            f1.contents = 'HH\rHH\rHH\rHH';
            styleStory(f1.parentStory);
            // A transformation matrix scales the frame's content; setting horizontalScale on a
            // text frame resizes the frame instead (measured: it read back 100).
            // InDesign's default "when scaling: apply to content" turns a scale into a larger
            // point size; adjusting the scaling percentage keeps it in the frame's matrix. This
            // is an application preference, so it is put back whatever happens.
            var tp = app.transformPreferences, whenScaling = tp.whenScaling;
            try {
                tp.whenScaling = WhenScalingOptions.ADJUST_SCALING_PERCENTAGE;
                f2.transform(CoordinateSpaces.PASTEBOARD_COORDINATES, AnchorPoint.CENTER_ANCHOR,
                    app.transformationMatrices.add({ horizontalScaleFactor: 1.25, verticalScaleFactor: 1.25 }));
            } finally { tp.whenScaling = whenScaling; }
            if (tp.whenScaling !== whenScaling) { throw Error('whenScaling not restored'); }
            var m = f2.transformValuesOf(CoordinateSpaces.PARENT_COORDINATES)[0];
            name([f1, f2], 'XE5');
            return { bounds: union([f1, f2]), note: 'f2 matrix scale=' + m.horizontalScaleFactor + ',' + m.verticalScaleFactor + ' lines=' + lines(f1) + ',' + lines(f2) };
        });
        variant('XE6', 'threaded pair with an invisible rectangle (no fill, no stroke) stacked between the frames over the seam', function () {
            var b = cell(6), y = b[0];
            var f1 = frame('f1', [y, b[1], y + 30, b[3]]);
            var r = doc.pages[0].rectangles.add(base, { geometricBounds: [y + 18, b[1], y + 42, (b[1] + b[3]) / 2] });
            // Stroke None only: a weight of 0 set after None turns a 1 pt black stroke back on
            // (the first build of this case did that, and became XE8).
            r.fillColor = none; r.strokeColor = none; r.name = 'XE6 invisible rectangle';
            r.textWrapPreferences.textWrapMode = TextWrapModes.NONE;
            var f2 = frame('f2', [y + 30, b[1], y + 60, b[3]]);
            f1.nextTextFrame = f2;
            f1.contents = 'HH\rHH\rHH\rHH';
            styleStory(f1.parentStory);
            // Explicit stacking: f2 on top, the rectangle between, f1 beneath both.
            f2.bringToFront(); f1.sendToBack();
            name([f1, f2], 'XE6');
            // allPageItems is in stacking order, top first; pageItems is not.
            var order = [], items = doc.pages[0].allPageItems;
            for (var i = 0; i < items.length; i++) { if (/^XE6/.test(items[i].name)) { order.push(items[i].name); } }
            return { bounds: [y, b[1], y + 60, b[3]], note: 'stacking top first=' + order.join(',') };
        });
        variant('XE7', 'threaded pair inside a group nested in another group with an unrelated rectangle', function () {
            var p = threadedPair(7);
            var b = cell(7);
            var r = doc.pages[0].rectangles.add(base, { geometricBounds: [b[0] + 100, b[1], b[0] + 120, b[3]] });
            r.fillColor = doc.swatches.itemByName('Paper'); r.strokeColor = none; r.name = 'XE7 rectangle';
            var inner = doc.pages[0].groups.add([p.f1, p.f2]);
            var outer = doc.pages[0].groups.add([inner, r]);
            inner.name = 'XE7 inner group'; outer.name = 'XE7 outer group';
            name([p.f1, p.f2], 'XE7');
            return { bounds: p.bounds };
        });

        variant('XE8', 'threaded pair with an unfilled rectangle with a 1 pt black border stacked between the frames over the seam', function () {
            var b = cell(8), y = b[0];
            var f1 = frame('f1', [y, b[1], y + 30, b[3]]);
            var r = doc.pages[0].rectangles.add(base, { geometricBounds: [y + 18, b[1], y + 42, (b[1] + b[3]) / 2] });
            r.fillColor = none; r.strokeColor = doc.swatches.itemByName('Black'); r.strokeWeight = 1; r.name = 'XE8 bordered rectangle';
            var f2 = frame('f2', [y + 30, b[1], y + 60, b[3]]);
            f1.nextTextFrame = f2;
            f1.contents = 'HH\rHH\rHH\rHH';
            styleStory(f1.parentStory);
            f2.bringToFront(); f1.sendToBack();
            name([f1, f2], 'XE8');
            return { bounds: [y, b[1], y + 60, b[3]], note: 'stroke weight=' + r.strokeWeight };
        });

        lib.manifest.app = app.version;
        lib.manifest.parentBackdrop = true;
        doc.save(new File(dir + '/cert-X2.indd'));
        lib.writeText(dir + '/cert-X2.manifest.json', lib.toJSON(lib.manifest));
        lib.L('cert-X2 saved variants=' + lib.manifest.frames.length);
    });
})();
