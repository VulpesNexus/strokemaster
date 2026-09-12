// probe-live-sync-watch.jsx - does the watcher notice an edit on its own?
// Requires ise-lib.jsx, ise-split-stroked-text.jsx, and ise-live-sync.jsx.
// Arguments: ISE_ARGS.mode = 'open' | 'edit' | 'check' | 'finish', ISE_ARGS.docPath for 'open'.
// Needs a persistent engine, because both the idle task and the open document have to outlive
// each call:
//   powershell -NoProfile -File tools\run-jsx.ps1 probe-live-sync-watch.jsx -Engine iseLiveSync ...
//
// Two links in the chain are already measured. Idle tasks fire and keep firing after the script
// that registered them returned (tools/jsx/probe-live-sync.jsx), and a resync rebuilds a split
// pair's bottom frame with its text updated and its stroke intact
// (tools/jsx/build-T-livesync.jsx). What is still assumed is the link between them: that a tick
// actually notices a changed top frame and resyncs it without anyone asking. That is the whole
// claim of "live", so it is worth a test of its own rather than an inference from the two halves.
//
// The document is opened WITHOUT lib.track, so it survives between calls; 'finish' closes it.
(function () {
    var lib = $.global.ISE_LIB;
    var sync = $.global.ISE_SYNC;
    return lib.run(function () {
        if (!sync) { lib.L('ERROR ise-live-sync.jsx is not loaded'); return; }
        var mode = String(lib.args.mode || 'check');
        lib.L('mode=' + mode + ' engine=' + ($.engineName || '(unnamed)'));

        function doc() {
            for (var i = 0; i < app.documents.length; i++) {
                if (String(app.documents[i].name).indexOf('T-livesync') === 0) { return app.documents[i]; }
            }
            return null;
        }

        function report(d, label) {
            var pairs = sync.pairs(d);
            lib.L(label + ': pairs=' + pairs.length);
            for (var i = 0; i < pairs.length; i++) {
                var t = String(pairs[i].top.contents), b = String(pairs[i].bottom.contents);
                lib.L(label + ' top   : "' + t.replace(/\r/g, '\\r') + '"');
                lib.L(label + ' bottom: "' + b.replace(/\r/g, '\\r') + '"');
                lib.L(label + ' MATCH : ' + (t === b));
            }
            return pairs;
        }

        if (mode === 'open') {
            var existing = doc();
            if (existing === null) {
                var f = new File(String(lib.args.docPath));
                if (!f.exists) { lib.L('ERROR document not found: ' + lib.args.docPath); return; }
                // Deliberately not tracked: lib.run closes tracked documents, and this one has
                // to stay open for the later calls to act on.
                existing = app.open(f, true);
            }
            lib.L('open: ' + existing.name);
            report(existing, 'open');
            var task = sync.start(Number(lib.args.intervalMs || 700));
            lib.L('watcher started, interval=' + task.sleep + ' ms, valid=' + task.isValid);
            lib.L('status: ' + sync.status());
        } else if (mode === 'edit') {
            var d1 = doc();
            if (d1 === null) { lib.L('ERROR the fixture is not open'); return; }
            var pairs = sync.pairs(d1);
            if (pairs.length === 0) { lib.L('ERROR no pair found'); return; }
            // Edit the top frame only, exactly as a user typing in it would.
            pairs[0].top.parentStory.contents = 'watched edit\rsecond line';
            lib.L('edited the top frame only');
            report(d1, 'just after the edit');
            lib.L('status: ' + sync.status());
        } else if (mode === 'finish') {
            var d3 = doc();
            lib.L('stopping watcher, removed=' + sync.stop());
            lib.L('status: ' + sync.status());
            if (d3 !== null) { d3.close(SaveOptions.NO); lib.L('closed the fixture without saving'); }
        } else {
            var d2 = doc();
            if (d2 === null) { lib.L('ERROR the fixture is not open'); return; }
            report(d2, 'check');
            lib.L('status: ' + sync.status());
        }
    });
})();
