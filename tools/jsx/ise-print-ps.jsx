// ise-print-ps.jsx - print a saved document to a PostScript file, without changing it.
// Requires ise-lib.jsx. Arguments: ISE_ARGS.docPath, ISE_ARGS.outDir, ISE_ARGS.stem.
//
// Export and print are different imaging paths in InDesign. PDF export is the one every other
// measurement in this repository uses, so on its own it cannot tell whether the adornment is
// reached when a page is printed. Printing to a PostScript file exercises the print path and
// leaves an artifact on disk that can be compared between a run with the plugin and a run
// without it.
//
// Print settings live on the document, and the document is closed with SaveOptions.NO, so
// nothing set here outlives the run. The application's own print preferences are left alone.
// Every step is logged rather than assumed: printing depends on a PPD being available, and a
// missing PPD has to read as a failed test rather than as a silent absence of any difference.
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var docPath = lib.args.docPath, outDir = lib.args.outDir, stem = lib.args.stem;
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
        lib.L('opened ' + doc.name);

        var pp = doc.printPreferences;
        var ok = lib.tryDo('printer', function () {
            pp.printer = Printer.POSTSCRIPT_FILE;
            return String(pp.printer);
        });
        lib.L('printer=' + lib.str(ok));

        // A PostScript file still needs a PPD. "Device Independent" is the generic one and is
        // the only candidate that does not depend on which printers this machine has.
        var ppdTried = [];
        var ppdSet = lib.tryDo('ppd list', function () {
            var names = app.ppds.everyItem().name;
            lib.L('ppds available=' + lib.str(names));
            return names;
        });
        var candidates = ['Device Independent'];
        if (ppdSet && ppdSet.length) {
            for (var i = 0; i < ppdSet.length; i++) { candidates.push(String(ppdSet[i])); }
        }
        var chosen = null;
        for (var c = 0; c < candidates.length && chosen === null; c++) {
            ppdTried.push(candidates[c]);
            chosen = lib.tryDo('ppd ' + candidates[c], function () {
                pp.ppd = candidates[c];
                return String(pp.ppd);
            }) || null;
        }
        lib.L('ppd tried=' + ppdTried.join(', ') + ' chosen=' + lib.str(chosen));

        var target = new File(outDir + '/' + stem + '.ps');
        lib.tryDo('printFile', function () { pp.printFile = target; return 'set'; });
        lib.tryDo('print options', function () {
            pp.copies = 1;
            pp.pageRange = PageRange.ALL_PAGES;
            pp.printSpreads = false;
            pp.printBlankPages = false;
            return 'set';
        });

        var printed = lib.timed('print ' + stem, function () { doc.print(false); });
        lib.L('printed=' + printed + ' exists=' + (target.exists ? 'yes' : 'no') + ' bytes=' + (target.exists ? target.length : 0));
        lib.L('saved=' + doc.saved + ' modified=' + doc.modified);
    });
})();
