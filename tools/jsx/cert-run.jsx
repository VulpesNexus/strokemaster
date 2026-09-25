// cert-run.jsx - run a list of route W certification jobs in the running InDesign.
// Requires ise-lib.jsx and cert-lib.jsx (run-jsx.ps1 -With cert-lib.jsx).
// Argument: ISE_ARGS.spec, a JSON file { "results": <jsonl path>, "jobs": [ ... ] } written by
// tools/certify_routew.py. Every job works on a copy of its source (see cert-lib.jsx), and
// lib.run restores the export preferences and the PDF placement preferences a job changes,
// whether the job succeeds or throws.
(function () {
    var lib = $.global.ISE_LIB, cert = $.global.ISE_CERT;
    return lib.run(function () {
        var spec = cert.readJSON(lib.args.spec);
        var only = lib.args.only ? String(lib.args.only) : '';
        var ran = 0, failed = 0;
        for (var i = 0; i < spec.jobs.length; i++) {
            var job = spec.jobs[i];
            if (only && job.id.indexOf(only) !== 0) { continue; }
            var rec = cert.runJob(job, spec.results);
            ran++;
            var bad = rec.error ? 1 : 0;
            for (var j = 0; j < rec.outputs.length; j++) { if (rec.outputs[j].error) { bad = 1; } }
            failed += bad;
            lib.L(job.id + (bad ? ' FAILED ' + (rec.error || '') : ' ok') + ' ms=' + rec.ms);
        }
        lib.L('jobs ran=' + ran + ' failed=' + failed);
    }, ['pdfPlacePreferences']);
})();
