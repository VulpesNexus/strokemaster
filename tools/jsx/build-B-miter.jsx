// build-B-miter.jsx - miter-join spikes (fixture B): the text miter limit through the
// document model, stroke weight, stroke alignment, and page-item wedges of known angle.
// Requires ise-lib.jsx. Argument: ISE_ARGS.outRoot.
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var dir = lib.args.outRoot + '/B-miter-spikes';
        var doc = lib.newDoc(612, 900);
        var base = lib.baseStyle(doc, 'ISE B base', { pointSize: 36, strokeWeight: 4, endJoin: EndJoin.MITER_END_JOIN, miterLimit: 4 });
        var black = doc.swatches.itemByName('Black'), paper = doc.swatches.itemByName('Paper');
        var GLYPHS = 'VWAMNvwkyzK';
        var f, i;

        // Section 1: miter limit on text, set through the document model.
        var limits = [1, 1.5, 2, 3, 4, 10, 100, 500];
        for (i = 0; i < limits.length; i++) {
            f = lib.frame(doc, 'B1 text miter limit ' + limits[i], [20 + i * 50, 24, 64 + i * 50, 300], GLYPHS, base);
            var got = lib.tryDo('set miterLimit ' + limits[i], function () { f.parentStory.texts[0].miterLimit = limits[i]; return f.parentStory.texts[0].miterLimit; });
            lib.record(f, 'text, Outside 4 pt, Miter, miterLimit ' + limits[i], { section: 1, requestedMiterLimit: limits[i], readBackMiterLimit: got, measure: 'spikes', weight: 4 });
        }

        // Section 2: stroke alignment Center at the same outer extent, and weight sweep.
        f = lib.frame(doc, 'B2 center alignment 8 pt, limit 4', [20, 312, 64, 588], GLYPHS, base);
        f.parentStory.texts[0].strokeAlignment = TextStrokeAlign.CENTER_ALIGNMENT;
        f.parentStory.texts[0].strokeWeight = 8;
        lib.record(f, 'text, Center 8 pt, Miter, limit 4', { section: 2, measure: 'spikes', weight: 4 });

        f = lib.frame(doc, 'B2 center alignment 8 pt, limit 1', [70, 312, 114, 588], GLYPHS, base);
        f.parentStory.texts[0].strokeAlignment = TextStrokeAlign.CENTER_ALIGNMENT;
        f.parentStory.texts[0].strokeWeight = 8;
        f.parentStory.texts[0].miterLimit = 1;
        lib.record(f, 'text, Center 8 pt, Miter, limit 1', { section: 2, measure: 'spikes', weight: 4 });

        var weights = [1, 2, 6, 8];
        for (i = 0; i < weights.length; i++) {
            f = lib.frame(doc, 'B2 weight ' + weights[i] + ', limit 4', [120 + i * 50, 312, 164 + i * 50, 588], GLYPHS, base);
            f.parentStory.texts[0].strokeWeight = weights[i];
            lib.record(f, 'text, Outside ' + weights[i] + ' pt, Miter, limit 4', { section: 2, measure: 'spikes', weight: weights[i] });
        }

        // Section 3: isosceles wedges with a known apex angle, apex up, 60 pt tall.
        var angles = [10, 15, 20, 25, 28, 30, 35, 40, 50, 60, 90];
        var aligns = [['outside', StrokeAlignment.OUTSIDE_ALIGNMENT, 4], ['center', StrokeAlignment.CENTER_ALIGNMENT, 8]];
        for (var r = 0; r < aligns.length; r++) {
            var x = 30, yApex = 460 + r * 110, h = 60;
            for (i = 0; i < angles.length; i++) {
                var half = h * Math.tan(angles[i] * Math.PI / 360);
                var p = doc.pages[0].polygons.add();
                p.paths[0].entirePath = [[x, yApex], [x + half, yApex + h], [x - half, yApex + h]];
                p.paths[0].pathType = PathType.CLOSED_PATH;
                p.fillColor = black;
                p.strokeColor = paper;
                p.strokeWeight = aligns[r][2];
                p.strokeAlignment = aligns[r][1];
                p.endJoin = EndJoin.MITER_END_JOIN;
                p.miterLimit = 4;
                p.name = 'B3 wedge ' + angles[i] + ' deg ' + aligns[r][0];
                lib.record(p, 'page-item wedge, apex ' + angles[i] + ' deg, ' + aligns[r][0] + ' ' + aligns[r][2] + ' pt, Miter, limit 4',
                    { section: 3, angle: angles[i], apex: [x, yApex], align: aligns[r][0], weight: aligns[r][2], measure: 'apex' });
                x += Math.max(2 * half, 10) + 30;
            }
        }

        // Section 4: outlines of the limit-4 text. One copy stays as converted; the other gets Outside alignment.
        var src = lib.frame(doc, 'B4 source text, limit 4', [700, 24, 744, 300], GLYPHS, base);
        lib.record(src, 'text source for outlines', { section: 4, measure: 'spikes', weight: 4 });
        var d1 = src.duplicate(undefined, [288, 0]);
        d1.name = 'B4 outlines as converted';
        var o1 = lib.tryDo('createOutlines 1', function () { return d1.createOutlines(true); });
        var d2 = src.duplicate(undefined, [0, 70]);
        d2.name = 'B4 outlines set to Outside';
        var o2 = lib.tryDo('createOutlines 2', function () { return d2.createOutlines(true); });
        function describe(items, label, setOutside) {
            if (!items) { return; }
            for (var k = 0; k < items.length; k++) {
                var it = items[k];
                if (setOutside) { lib.tryDo('set outside', function () { it.strokeAlignment = StrokeAlignment.OUTSIDE_ALIGNMENT; }); }
                lib.record(it, label, { section: 4, cls: it.constructor.name, strokeAlignment: lib.str(it.strokeAlignment), strokeWeight: it.strokeWeight, endJoin: lib.str(it.endJoin), miterLimit: it.miterLimit, fill: lib.str(it.fillColor), stroke: lib.str(it.strokeColor), measure: 'spikes', weight: 4 });
            }
        }
        describe(o1, 'outlines of limit-4 text, as converted', false);
        describe(o2, 'outlines of limit-4 text, alignment set to Outside', true);

        lib.saveExport(doc, dir, 'B-miter', 300);
    });
})();
