// Builds the screen-drawing test: chosen showcase cells (tools/jsx/build-showcase.jsx), one per
// page, copied from showcase.indd into a new document opened in a window, for screen captures
// with StrokeMaster off and on. Leaves the document open (unsaved) for the capture driver.
//
// Arguments: ISE_ARGS.showcase (showcase.indd), ISE_ARGS.cells (showcase.json), ISE_ARGS.ids.
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var src = app.open(new File(lib.args.showcase), false);
        // ExtendScript has no JSON parser; showcase.json is written by build-showcase.jsx itself.
        var meta = eval('(' + (function () { var f = new File(lib.args.cells); f.encoding = 'UTF-8'; f.open('r'); var t = f.read(); f.close(); return t; })() + ')');
        var want = lib.args.ids, cells = {};
        for (var c = 0; c < meta.cells.length; c++) { cells[meta.cells[c].id] = meta.cells[c]; }
        var doc = app.documents.add(true);
        doc.viewPreferences.horizontalMeasurementUnits = MeasurementUnits.POINTS;
        doc.viewPreferences.verticalMeasurementUnits = MeasurementUnits.POINTS;
        doc.documentPreferences.facingPages = false;
        doc.documentPreferences.pageWidth = 600;
        doc.documentPreferences.pageHeight = 210;
        doc.zeroPoint = [0, 0];
        while (doc.pages.length < want.length) { doc.pages.add(); }
        // Swatches and styles come over with the items; the source's own are named alike.
        var items = src.pages[0].allPageItems, report = [];
        for (var w = 0; w < want.length; w++) {
            var cell = cells[want[w]], pg = doc.pages[w], n = 0;
            if (!cell) { report.push(want[w] + ' missing'); continue; }
            var b = cell.bounds;   // [top, left, bottom, right]
            for (var q = 0; q < items.length; q++) {
                var it = items[q];
                if (it.parent.constructor.name !== 'Spread') { continue; }
                var gb = it.visibleBounds;
                if (gb[0] >= b[0] - 1 && gb[2] <= b[2] + 40 && gb[0] < b[2]) {
                    var dup = it.duplicate(pg);
                    var db = dup.visibleBounds;
                    dup.move(undefined, [0, (gb[0] - b[0]) - db[0] + 5]);
                    n++;
                }
            }
            report.push(want[w] + ' items=' + n);
        }
        src.close(SaveOptions.NO);
        lib.L('screen doc pages=' + doc.pages.length + '\n' + report.join('\n'));
    });
})();
