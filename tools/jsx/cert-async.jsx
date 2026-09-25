// cert-async.jsx - background PDF exports, the path File > Export > Adobe PDF takes by default.
// Requires ise-lib.jsx and cert-lib.jsx. Argument: ISE_ARGS.spec, a JSON file
//   { "results": "<jsonl>", "jobs": [ { "id", "source", "copy", "preset", "out" } ] }
// Each job copies its source, opens the copy without a window, exports it with
// Document.asynchronousExportFile and the named preset, waits for the task, and appends its
// status, size, and time to the results file. The copy is closed without saving.
//
// A plugin declared as a UI plugin is not loaded on the export thread, so its drawing does not
// happen there: route W's certified build exports background PDFs natively. Route X is a model
// plugin and runs there too; these jobs are the evidence either way.
(function () {
    var lib = $.global.ISE_LIB, cert = $.global.ISE_CERT;
    var spec = cert.readJSON(lib.args.spec);
    return lib.run(function () {
        var ran = 0, failed = 0;
        for (var i = 0; i < spec.jobs.length; i++) {
            var job = spec.jobs[i], rec = { id: job.id, preset: job.preset, error: null };
            var doc = null, t0 = new Date().getTime();
            try {
                var copy = cert.copyFile(job.source, job.copy);
                doc = lib.track(app.open(copy, false));
                var out = new File(job.out);
                out.parent.create();
                if (out.exists && !out.remove()) { throw new Error('cannot replace ' + job.out); }
                var task = doc.asynchronousExportFile(ExportFormat.PDF_TYPE, out, false, app.pdfExportPresets.itemByName(job.preset));
                task.waitForTask();
                rec.status = String(task.status);
                rec.bytes = out.exists ? out.length : 0;
                if (!out.exists) { throw new Error('no output, task status ' + task.status); }
            } catch (e) {
                rec.error = e.message; failed++;
            } finally {
                try { if (doc && doc.isValid) { doc.close(SaveOptions.NO); } } catch (e2) {}
            }
            rec.ms = new Date().getTime() - t0;
            cert.appendLine(spec.results, lib.toJSON(rec));
            lib.L(job.id + (rec.error ? ' FAILED ' + rec.error : ' ok bytes=' + rec.bytes + ' ms=' + rec.ms));
            ran++;
        }
        lib.L('async jobs ran=' + ran + ' failed=' + failed);
    }, ['pdfExportPreferences']);
})();
