// cert-xbench.jsx - time route X against route W and native drawing in one InDesign session.
// Requires ise-lib.jsx and cert-lib.jsx, and the research build (build-plugin.ps1 -RouteX
// -Research) launched with ISE_XF_CONTROL naming the control file this script rewrites.
// Arguments: ISE_ARGS.victims, ISE_ARGS.xframe (the frozen census and cross-frame census),
// ISE_ARGS.control (the control file), ISE_ARGS.out (a fresh folder), ISE_ARGS.reps (default 7).
//
// The plugin reads its mode when a spread draw begins, so the modes can alternate inside one
// process: every repetition exports every document once in each mode, in an order that rotates,
// so drift in the machine's load falls on every mode alike. Four documents:
//   victims   the 20-frame census (no threaded story: route X records nothing)
//   xframe    the cross-frame census (22 cases)
//   threaded  20 pages, one story threaded through 160 frames of stroked 30 pt text that
//             overlap frame to frame (route X's work at its largest)
//   body      20 pages, one unstroked 11 pt story threaded through 20 frames (route X records
//             every fill and withholds nothing: its overhead with no benefit)
// Times are wall-clock milliseconds around exportFile (PDF High Quality Print, PNG 150 ppi).
(function () {
    var lib = $.global.ISE_LIB, cert = $.global.ISE_CERT;
    var out = lib.args.out, reps = Number(lib.args.reps || 7);
    var modes = ['native', 'routew', 'story'];
    var hqp = app.pdfExportPresets.itemByName('[High Quality Print]');

    function setMode(mode) {
        var f = new File(lib.args.control);
        f.encoding = 'UTF-8';
        f.lineFeed = 'Unix';
        if (!f.open('w')) { throw new Error('cannot write control file'); }
        f.write('mode=' + mode + '\ntrace=0\n');
        f.close();
        $.sleep(1100);   // the plugin compares the file's modification time
    }
    function longDoc(kind) {
        var doc = lib.track(app.documents.add(false));
        doc.viewPreferences.horizontalMeasurementUnits = MeasurementUnits.POINTS;
        doc.viewPreferences.verticalMeasurementUnits = MeasurementUnits.POINTS;
        var dp = doc.documentPreferences;
        dp.facingPages = false; dp.pageWidth = 600; dp.pageHeight = 800; dp.pagesPerDocument = 20;
        var ps = lib.baseStyle(doc, 'Bench', kind === 'threaded' ? { pointSize: 30, leading: 14, strokeWeight: 12 } : { pointSize: 11, leading: 13, strokeWeight: 0 });
        if (kind === 'body') { ps.strokeColor = doc.swatches.itemByName('None'); }
        var frames = [], p, j;
        for (p = 0; p < doc.pages.length; p++) {
            if (kind === 'threaded') {
                for (j = 0; j < 8; j++) { frames.push(doc.pages[p].textFrames.add({ geometricBounds: [40 + j * 30, 40, 70 + j * 30, 560] })); }
            } else {
                frames.push(doc.pages[p].textFrames.add({ geometricBounds: [40, 40, 760, 560] }));
            }
        }
        for (j = 0; j < frames.length; j++) {
            frames[j].textFramePreferences.insetSpacing = [0, 0, 0, 0];
            frames[j].textFramePreferences.firstBaselineOffset = FirstBaseline.LEADING_OFFSET;
            if (j > 0) { frames[j - 1].nextTextFrame = frames[j]; }
        }
        var line = kind === 'threaded' ? 'HH HH HH HH' : 'HHHH HHHH HHHH HHHH HHHH HHHH HHHH HHHH HHHH HHHH HHHH', n = kind === 'threaded' ? frames.length * 2 : 20 * 58;
        var text = [];
        for (j = 0; j < n; j++) { text.push(line); }
        frames[0].contents = text.join('\r');
        frames[0].parentStory.texts[0].applyParagraphStyle(ps, true);
        doc.recompose();
        lib.L(kind + ' frames=' + frames.length + ' lines=' + frames[0].parentStory.lines.length + ' overset=' + frames[frames.length - 1].overflows);
        return doc;
    }
    function exporters(doc) {
        return [['pdf', function (p) { doc.exportFile(ExportFormat.PDF_TYPE, new File(p + '.pdf'), false, hqp); }],
                ['png150', function (p) { cert.write.png(doc, { ppi: 150 }, p + '.png'); }]];
    }
    return lib.run(function () {
        new Folder(out).create();
        var docs = [
            ['victims', lib.track(app.open(cert.copyFile(lib.args.victims, out + '/work/victims.indd'), false))],
            ['xframe', lib.track(app.open(cert.copyFile(lib.args.xframe, out + '/work/xframe.indd'), false))],
            ['threaded', longDoc('threaded')],
            ['body', longDoc('body')]
        ];
        var times = {}, d, m, k, r;
        for (d = 0; d < docs.length; d++) {
            var ex = exporters(docs[d][1]);
            for (m = 0; m < modes.length; m++) {
                setMode(modes[m]);
                for (k = 0; k < ex.length; k++) {
                    times[docs[d][0] + '.' + ex[k][0] + '.' + modes[m]] = [];
                    ex[k][1](out + '/warm-' + docs[d][0] + '-' + modes[m] + '-' + ex[k][0]);
                }
            }
        }
        for (r = 0; r < reps; r++) {
            for (var mi = 0; mi < modes.length; mi++) {
                var mode = modes[(mi + r) % modes.length];
                setMode(mode);
                for (d = 0; d < docs.length; d++) {
                    var exs = exporters(docs[d][1]);
                    for (k = 0; k < exs.length; k++) {
                        var t0 = new Date().getTime();
                        exs[k][1](out + '/t-' + docs[d][0] + '-' + mode + '-' + exs[k][0]);
                        times[docs[d][0] + '.' + exs[k][0] + '.' + mode].push(new Date().getTime() - t0);
                    }
                }
            }
        }
        setMode('routew');
        for (d = 0; d < docs.length; d++) { docs[d][1].close(SaveOptions.NO); }
        lib.writeText(out + '/xbench.json', lib.toJSON({ reps: reps, modes: modes, times: times, app: app.version }));
        lib.L('xbench written');
    }, ['pdfExportPreferences', 'pngExportPreferences']);
})();
