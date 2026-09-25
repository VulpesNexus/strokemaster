// ise-export-open.jsx - export a document the owner has open, and save a copy of it, without changing it.
// Requires ise-lib.jsx. Arguments: ISE_ARGS.docPath (the open document's file), ISE_ARGS.outDir,
// ISE_ARGS.stem, and optionally ISE_ARGS.ppi (default 300), ISE_ARGS.eps ('yes' to also write
// EPS), and ISE_ARGS.copy ('yes' to also save a copy as <stem>.indd).
//
// ise-reexport.jsx opens a closed document without a window. This script is its counterpart for
// a document that is open in a window, whose screen drawing is the thing under test: route W
// mutes a stroke in one adornment pass and restores it in a later one, so a screen draw stopped
// between the two could leave a muted stroke in memory, where an export or a save would find it.
// The document is found by its full path, never by position, and exactly one match is required.
// Exporting does not modify it, and saveACopy writes a new file without renaming, saving, or
// closing it; lib.run's liveState line confirms its saved and modified flags are unchanged.
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var docPath = lib.args.docPath, outDir = lib.args.outDir, stem = lib.args.stem;
        var ppi = Number(lib.args.ppi || 300);
        if (!docPath || !outDir || !stem) {
            lib.L('ERROR need docPath, outDir and stem');
            return;
        }
        var want = new File(docPath).fsName.toLowerCase();
        var matches = [];
        for (var i = 0; i < app.documents.length; i++) {
            var d = app.documents[i];
            try { if (d.saved && d.fullName.fsName.toLowerCase() === want) { matches.push(d); } } catch (e) {}
        }
        if (matches.length !== 1) {
            lib.L('ERROR expected one open document at ' + docPath + ', found ' + matches.length);
            return;
        }
        var doc = matches[0];
        new Folder(outDir).create();
        lib.L('found ' + doc.name + ' pages=' + doc.pages.length + ' items=' + doc.pages[0].allPageItems.length +
            ' modified=' + doc.modified + ' windows=' + doc.layoutWindows.length);

        lib.timed('pdf ' + stem, function () {
            doc.exportFile(ExportFormat.PDF_TYPE, new File(outDir + '/' + stem + '.pdf'), false, app.pdfExportPresets.itemByName('[High Quality Print]'));
        });
        lib.timed('png ' + stem, function () {
            var png = app.pngExportPreferences;
            png.exportResolution = ppi;
            png.pngQuality = PNGQualityEnum.MAXIMUM;
            png.transparentBackground = false;
            png.antiAlias = true;
            png.pngColorSpace = PNGColorSpaceEnum.RGB;
            png.pngExportRange = PNGExportRangeEnum.EXPORT_ALL;
            png.exportingSpread = false;
            png.useDocumentBleeds = false;
            png.simulateOverprint = false;
            doc.exportFile(ExportFormat.PNG_FORMAT, new File(outDir + '/' + stem + '-' + ppi + 'ppi.png'), false);
        });
        if (String(lib.args.eps) === 'yes') {
            lib.timed('eps ' + stem, function () {
                doc.exportFile(ExportFormat.EPS_TYPE, new File(outDir + '/' + stem + '.eps'), false);
            });
        }
        if (String(lib.args.copy) === 'yes') {
            lib.timed('copy ' + stem, function () {
                doc.saveACopy(new File(outDir + '/' + stem + '.indd'));
            });
        }
        lib.L('saved=' + doc.saved + ' modified=' + doc.modified);
    });
})();
