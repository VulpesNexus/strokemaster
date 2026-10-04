// build-B-font-points.jsx - exterior miter points on decorative fonts, live and outlined, drawn
// natively and by the research build's corrections in one process. Requires ise-lib.jsx and a
// research build of route X (it reads the control file named by ISE_XF_CONTROL).
// Arguments: ISE_ARGS.out (folder for the document and PDFs), ISE_ARGS.control (that control
// file), ISE_ARGS.fonts (font names, "Family\tStyle"), ISE_ARGS.text.
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var out = lib.args.out, fonts = lib.args.fonts, text = lib.args.text;
        var rowH = 70, doc = lib.newDoc(612, 200 + fonts.length * 4 * rowH);
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
