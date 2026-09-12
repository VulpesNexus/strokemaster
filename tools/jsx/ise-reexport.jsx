// ise-reexport.jsx - open a saved document and export it again, without changing it.
// Requires ise-lib.jsx. Arguments: ISE_ARGS.docPath, ISE_ARGS.outDir, ISE_ARGS.stem,
// and optionally ISE_ARGS.ppi (default 300) and ISE_ARGS.eps ('yes' to also write EPS).
//
// This is the control half of every plugin measurement. Rebuilding a fixture twice would
// compare two documents; opening one saved document and exporting it under two different
// renderers compares one document, so the only variable left is the renderer. The document is
// opened without a window and closed with SaveOptions.NO, so the file on disk is not touched.
//
// The export settings are written out in full rather than inherited, and they match
// ise-lib.jsx saveExport exactly, so that a difference between two runs cannot come from a
// preference that happened to be left in a different state.
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var docPath = lib.args.docPath, outDir = lib.args.outDir, stem = lib.args.stem;
        var ppi = Number(lib.args.ppi || 300);
        if (!docPath || !outDir || !stem) {
            lib.L('ERROR need docPath, outDir and stem');
            return;
        }
        var src = new File(docPath);
        if (!src.exists) {
            lib.L('ERROR document not found: ' + docPath);
            return;
        }
        new Folder(outDir).create();

        var doc = lib.track(app.open(src, false));
        lib.L('opened ' + doc.name + ' pages=' + doc.pages.length + ' items=' + doc.pages[0].allPageItems.length);

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
            // EPS is the PostScript imaging path rather than the PDF one, so it is a separate
            // check on whether the adornment is reached at all outside PDF export.
            lib.timed('eps ' + stem, function () {
                doc.exportFile(ExportFormat.EPS_TYPE, new File(outDir + '/' + stem + '.eps'), false);
            });
        }
        lib.L('saved=' + doc.saved + ' modified=' + doc.modified);
    });
})();
