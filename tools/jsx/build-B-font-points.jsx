// build-B-font-points.jsx - exterior miter points on decorative fonts, live and outlined, and
// compound shapes whose contours overlap, drawn natively and by the research build's
// corrections in one process. Requires ise-lib.jsx and a research build of route X (it reads
// the control file named by ISE_XF_CONTROL).
// Arguments: ISE_ARGS.out (folder for the document and PDFs), ISE_ARGS.control (that control
// file), ISE_ARGS.fonts (font names, "Family\tStyle"), ISE_ARGS.text.
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var out = lib.args.out, fonts = lib.args.fonts, text = lib.args.text;
        var rowH = 70, doc = lib.newDoc(612, 420 + fonts.length * 5 * rowH);
        var row = 0;
        function y0() { return 20 + row * rowH; }
        for (var i = 0; i < fonts.length; i++) {
            if (!lib.fontOk(fonts[i])) { lib.L('font missing: ' + fonts[i]); continue; }
            var font = app.fonts.itemByName(fonts[i]);
            lib.L('font ' + fonts[i] + ' at ' + font.location);
            var tag = fonts[i].split('\t')[0];
            var ps = lib.baseStyle(doc, 'ISE font ' + tag, { appliedFont: fonts[i], pointSize: 48, strokeWeight: 4, endJoin: EndJoin.MITER_END_JOIN, miterLimit: 4 });
            var outside = lib.frame(doc, tag + ' live Outside 4 pt', [y0(), 16, y0() + rowH - 6, 596], text, ps);
            lib.record(outside, 'live text, Outside 4 pt, Miter, limit 4', { font: fonts[i], weight: 4 });
            row++;
            var center = lib.frame(doc, tag + ' live Center 8 pt', [y0(), 16, y0() + rowH - 6, 596], text, ps);
            center.parentStory.texts[0].strokeAlignment = TextStrokeAlign.CENTER_ALIGNMENT;
            center.parentStory.texts[0].strokeWeight = 8;
            lib.record(center, 'live text, Center 8 pt, Miter, limit 4', { font: fonts[i], weight: 4 });
            row++;
            var d1 = outside.duplicate(undefined, [0, 2 * rowH]);
            var o1 = lib.tryDo('outline ' + tag + ' Outside', function () { return d1.createOutlines(true); });
            for (var k = 0; o1 && k < o1.length; k++) {
                o1[k].strokeAlignment = StrokeAlignment.OUTSIDE_ALIGNMENT;
                o1[k].name = tag + ' outlined Outside 4 pt';
                lib.record(o1[k], 'outlined text set to Outside 4 pt', { font: fonts[i], weight: 4, align: lib.str(o1[k].strokeAlignment) });
            }
            row++;
            var d2 = center.duplicate(undefined, [0, 2 * rowH]);
            var o2 = lib.tryDo('outline ' + tag + ' Center', function () { return d2.createOutlines(true); });
            for (k = 0; o2 && k < o2.length; k++) {
                o2[k].name = tag + ' outlined Center 8 pt';
                lib.record(o2[k], 'outlined text, Center 8 pt as converted', { font: fonts[i], weight: 4, align: lib.str(o2[k].strokeAlignment), strokeWeight: o2[k].strokeWeight });
            }
            row++;
            // Without a fill, nothing covers an Outside stroke's inner half: the clip alone
            // keeps it outside the letters.
            var d3 = outside.duplicate(undefined, [0, 4 * rowH]);
            var o3 = lib.tryDo('outline ' + tag + ' Outside unfilled', function () { return d3.createOutlines(true); });
            for (k = 0; o3 && k < o3.length; k++) {
                o3[k].strokeAlignment = StrokeAlignment.OUTSIDE_ALIGNMENT;
                o3[k].fillColor = doc.swatches.itemByName('None');
                o3[k].name = tag + ' outlined Outside 4 pt, no fill';
                lib.record(o3[k], 'outlined text set to Outside 4 pt, no fill', { font: fonts[i], weight: 4, align: lib.str(o3[k].strokeAlignment) });
            }
            row++;
        }
        // Thorns: a 10-degree spike on a 40 pt square, its sides `len` long. Its whole miter
        // reaches 1 / sin(5 deg) half widths past the tip; kept whole only when no longer than
        // the sides.
        var black = doc.swatches.itemByName('Black'), paper = doc.swatches.itemByName('Paper');
        var lens = [60, 30, 10], aligns = [['outside', StrokeAlignment.OUTSIDE_ALIGNMENT, 4], ['center', StrokeAlignment.CENTER_ALIGNMENT, 8]];
        var top = y0() + 60;
        for (var r = 0; r < aligns.length; r++) {
            for (var j = 0; j < lens.length; j++) {
                var cx = 60 + (r * lens.length + j) * 90, half = 5 * Math.PI / 180;
                var tipY = top, baseY = top + lens[j] * Math.cos(half), w = lens[j] * Math.sin(half);
                var poly = doc.pages[0].polygons.add();
                poly.paths[0].entirePath = [[cx, tipY], [cx + w, baseY], [cx + 20, baseY], [cx + 20, baseY + 40], [cx - 20, baseY + 40], [cx - 20, baseY], [cx - w, baseY]];
                poly.paths[0].pathType = PathType.CLOSED_PATH;
                poly.fillColor = black;
                poly.strokeColor = paper;
                poly.strokeWeight = aligns[r][2];
                poly.strokeAlignment = aligns[r][1];
                poly.endJoin = EndJoin.MITER_END_JOIN;
                poly.miterLimit = 4;
                poly.name = 'thorn ' + lens[j] + ' pt ' + aligns[r][0];
                lib.record(poly, 'thorn, 10 deg, sides ' + lens[j] + ' pt, ' + aligns[r][0] + ' ' + aligns[r][2] + ' pt, limit 4', { apex: [cx, tipY], side: lens[j], align: aligns[r][0], weight: aligns[r][2] });
            }
        }
        // Compound shapes: two contours each, wound alike unless `reversed`. Overlapping
        // contours and same-direction holes are filled under the nonzero rule, where the
        // even-odd rule would leave holes.
        function square(x, y, a) { return [[x, y], [x + a, y], [x + a, y + a], [x, y + a]]; }
        function circle(cx, cy, r) {
            var k = 0.5522847498 * r;
            return [[[cx + r, cy - k], [cx + r, cy], [cx + r, cy + k]], [[cx + k, cy + r], [cx, cy + r], [cx - k, cy + r]],
                    [[cx - r, cy + k], [cx - r, cy], [cx - r, cy - k]], [[cx - k, cy - r], [cx, cy - r], [cx + k, cy - r]]];
        }
        function reversed(pts) { var r = pts.slice(0); r.reverse(); for (var q = 0; q < r.length; q++) { if (r[q].length === 3 && r[q][0].length === 2) { r[q] = [r[q][2], r[q][1], r[q][0]]; } } return r; }
        var top2 = top + 150, x0 = 30, step = 95;
        var shapes = [
            ['squares overlapping, outside 4 pt', [square(x0, top2, 40), square(x0 + 20, top2 + 20, 40)], StrokeAlignment.OUTSIDE_ALIGNMENT, 4],
            ['squares overlapping, center 8 pt', [square(x0 + step, top2, 40), square(x0 + step + 20, top2 + 20, 40)], StrokeAlignment.CENTER_ALIGNMENT, 8],
            ['circles overlapping, outside 4 pt', [circle(x0 + 2 * step + 22, top2 + 30, 22), circle(x0 + 2 * step + 44, top2 + 30, 22)], StrokeAlignment.OUTSIDE_ALIGNMENT, 4],
            ['same-direction hole, outside 4 pt', [square(x0 + 3 * step, top2, 60), square(x0 + 3 * step + 20, top2 + 20, 20)], StrokeAlignment.OUTSIDE_ALIGNMENT, 4],
            ['rectangular hole, outside 4 pt', [square(x0 + 4 * step, top2, 60), reversed(square(x0 + 4 * step + 20, top2 + 20, 20))], StrokeAlignment.OUTSIDE_ALIGNMENT, 4],
            ['rectangular hole, center 8 pt', [square(x0 + 5 * step, top2, 60), reversed(square(x0 + 5 * step + 20, top2 + 20, 20))], StrokeAlignment.CENTER_ALIGNMENT, 8],
            ['squares overlapping, outside 4 pt, stroke overprint', [square(x0, top2 + 80, 40), square(x0 + 20, top2 + 100, 40)], StrokeAlignment.OUTSIDE_ALIGNMENT, 4, true]
        ];
        var cyan = lib.color(doc, 'ISE cyan', [100, 0, 0, 0]);
        for (var n = 0; n < shapes.length; n++) {
            var sh = doc.pages[0].polygons.add();
            sh.paths[0].entirePath = shapes[n][1][0];
            sh.paths[0].pathType = PathType.CLOSED_PATH;
            var second = lib.tryDo('second contour of ' + shapes[n][0], function () { return sh.paths.add(); });
            if (!second) { continue; }
            second.entirePath = shapes[n][1][1];
            second.pathType = PathType.CLOSED_PATH;
            sh.fillColor = black;
            sh.strokeColor = paper;
            sh.strokeWeight = shapes[n][3];
            sh.strokeAlignment = shapes[n][2];
            sh.endJoin = EndJoin.MITER_END_JOIN;
            sh.miterLimit = 4;
            sh.name = shapes[n][0];
            if (shapes[n][4]) { sh.strokeColor = cyan; sh.overprintStroke = true; }   // Paper cannot overprint
            lib.record(sh, shapes[n][0], { paths: sh.paths.length, weight: shapes[n][3], overprintStroke: !!shapes[n][4] });
        }
        // Text whose stroke overprints: its miter joins are fills, which must overprint too.
        var ops = lib.baseStyle(doc, 'ISE overprint text', { pointSize: 48, strokeColor: cyan, strokeWeight: 4, endJoin: EndJoin.MITER_END_JOIN, miterLimit: 4, overprintStroke: true });
        var opText = lib.frame(doc, 'live text Outside 4 pt, stroke overprint', [top2 + 80, x0 + step, top2 + 140, 596], 'AKMVWkvwyz', ops);
        lib.record(opText, 'live text, Outside 4 pt, Miter, limit 4, stroke overprint', { weight: 4 });
        new Folder(out).create();
        doc.save(new File(out + '/font-points.indd'));
        var hqp = app.pdfExportPresets.itemByName('[High Quality Print]');
        var modes = [['native', 'mode=native\nmiter=0\nshapes=0\n'], ['corrected', 'mode=story\nmiter=1\nshapes=1\n']];
        for (var m = 0; m < modes.length; m++) {
            var ctl = new File(lib.args.control);
            ctl.encoding = 'UTF-8';
            ctl.lineFeed = 'Unix';
            ctl.open('w'); ctl.write(modes[m][1]); ctl.close();
            lib.timed('pdf ' + modes[m][0], function () { doc.exportFile(ExportFormat.PDF_TYPE, new File(out + '/font-points-' + modes[m][0] + '.pdf'), false, hqp); });
        }
        lib.manifest.app = app.version;
        lib.writeText(out + '/font-points.manifest.json', lib.toJSON(lib.manifest));
    });
})();
