// cert-bench.jsx - time route W's exports against the control, one InDesign session at a time.
// Requires ise-lib.jsx and cert-lib.jsx. Arguments: ISE_ARGS.victims (the frozen census),
// ISE_ARGS.out (a fresh folder), ISE_ARGS.reps (timed repetitions per kind, default 15).
//
// Two documents: the 20-frame victim census, and a 20-page copy of it (400 stroked frames)
// built the same way in every session. Each is opened once, from a copy; two warm-up exports
// of each kind are discarded; then the kinds are interleaved (PDF High Quality Print, PNG 300
// ppi, PNG 600 ppi for the census; PDF and PNG 150 ppi for the long document) so that drift in
// the machine's load falls on every kind alike. Times are wall-clock milliseconds around
// exportFile and include writing the file.
(function () {
    var lib = $.global.ISE_LIB, cert = $.global.ISE_CERT;
    var out = lib.args.out, reps = Number(lib.args.reps || 15);
    var hqp = app.pdfExportPresets.itemByName('[High Quality Print]');
    function png(doc, ppi, path) {
        cert.write.png(doc, { ppi: ppi }, path);
    }
    function kinds(doc, isLong) {
        if (isLong) {
            return [['pdf', function (p) { doc.exportFile(ExportFormat.PDF_TYPE, new File(p + '.pdf'), false, hqp); }],
                    ['png150', function (p) { png(doc, 150, p + '.png'); }]];
        }
        return [['pdf', function (p) { doc.exportFile(ExportFormat.PDF_TYPE, new File(p + '.pdf'), false, hqp); }],
                ['png300', function (p) { png(doc, 300, p + '.png'); }],
                ['png600', function (p) { png(doc, 600, p + '.png'); }]];
    }
    function bench(doc, name, isLong) {
        var ks = kinds(doc, isLong), times = {}, i, k;
        for (k = 0; k < ks.length; k++) { times[ks[k][0]] = []; for (i = 0; i < 2; i++) { ks[k][1](out + '/warm-' + name + '-' + ks[k][0]); } }
        for (i = 0; i < reps; i++) {
            for (k = 0; k < ks.length; k++) {
                var t0 = new Date().getTime();
                ks[k][1](out + '/t-' + name + '-' + ks[k][0]);
                times[ks[k][0]].push(new Date().getTime() - t0);
            }
        }
        return times;
    }
    return lib.run(function () {
        new Folder(out).create();
        var rec = { version: app.version, reps: reps, started: new Date().toString() };
        var copy = cert.copyFile(lib.args.victims, out + '/bench-victims.indd');
        var doc = lib.track(app.open(copy, false));
        rec.victims = bench(doc, 'victims', false);
        var longPath = out + '/bench-long.indd';
        for (var p = 1; p < 20; p++) { doc.pages[0].duplicate(LocationOptions.AT_END); }
        doc.save(new File(longPath));
        rec.longPages = doc.pages.length;
        rec.longFrames = doc.textFrames.length;
        rec.longDoc = bench(doc, 'long', true);
        doc.close(SaveOptions.NO);
        rec.finished = new Date().toString();
        lib.writeText(out + '/bench.json', lib.toJSON(rec));
        lib.L('bench pages=' + rec.longPages + ' frames=' + rec.longFrames);
    });
})();
