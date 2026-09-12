// build-fixture-plugin.jsx - export the fixture itself, rather than a reproducer.
//
// Every other build script composes its own document. This one opens the working copy of the
// fixture and records a manifest for it, so the probe can measure the real document the same
// way it measures the minimal reproducers.
//
// The manifest asks for the hidden-fill measurement on each text frame. Because the geometry
// is identical in every export of this document, the manifest this script writes also
// describes the exports taken before the plugin existed, which is what makes a before and
// after comparison possible without uninstalling the plugin.
//
// It never touches the pristine copy, and never the author's own file: it opens only
// corpus/working/fixture-working.indd, and ise-lib closes it without saving.
// Pure ASCII: ExtendScript sources are read in the system codepage.
(function () {
    return ISE_LIB.run(function (lib) {
        var root = lib.args.outRoot;
        var src = new File(root + '/corpus/working/fixture-working.indd');
        if (!src.exists) {
            lib.L('working copy missing: ' + src.fsName);
            return;
        }

        var doc = lib.track(app.open(src, false));
        var dp = doc.documentPreferences;
        doc.viewPreferences.horizontalMeasurementUnits = MeasurementUnits.POINTS;
        doc.viewPreferences.verticalMeasurementUnits = MeasurementUnits.POINTS;
        doc.zeroPoint = [0, 0];

        // Recompose before exporting. Each wax run caches the list of draw passes it needs
        // (SDK, IWaxRunShape.h, lines 49 to 52), and an optional global adornment is only in
        // that list if it existed when the run was composed. This document was composed before
        // the plugin was installed and is only being opened, so without this its saved wax can
        // skip the adornment's pass. Every other build script composes its document from
        // scratch with the plugin already loaded, which is why none of them needs this.
        lib.timed('recompose', function () { doc.recompose(); });

        lib.manifest = { page: [dp.pageWidth, dp.pageHeight], frames: [], items: [] };
        lib.L('opened ' + doc.name + ' pages=' + doc.pages.length + ' items=' + doc.allPageItems.length);

        var frames = doc.textFrames;
        for (var i = 0; i < frames.length; i++) {
            var tf = frames[i];
            lib.tryDo('record frame ' + i, function () {
                var rec = lib.record(tf, 'fixture text frame', { measure: 'hidden' });
                lib.L('  frame ' + i + ' name=' + rec.name + ' lines=' + (rec.lines ? rec.lines.length : 0) +
                      ' ranges=' + rec.styleRanges);
            });
        }
        lib.L('text frames recorded=' + lib.manifest.frames.length);

        lib.saveExport(doc, root + '/corpus', 'fixture-plugin', 600);
    });
})();
