#targetengine "strokemaster"
// strokemaster.jsx - one entry point for every StrokeMaster correction.
// Pure ASCII: ExtendScript sources are read in the system codepage.
//
// Run it either way:
//   - From InDesign's Scripts panel: double-click it and pick a command in the dialog.
//   - Headless: tools\run-jsx.ps1 strokemaster.jsx -ArgJson "{\"command\":\"split\"}"
//     There is no need to pass -Engine. Measured: a headless run reports its engine as "main",
//     so the #targetengine line above is ignored on that path. The directive applies only to the
//     script InDesign compiles itself, and run-jsx.ps1 reaches this file through $.evalFile. The
//     watcher survives there regardless, because "main" is persistent: a watch started in one
//     invocation reported watching=true from the next. Run from the Scripts panel the directive
//     does apply, and gives this script an engine of its own.
//
// Commands:
//   split    Split each selected stroked text frame into a fill frame over a stroke frame. This
//            is the correction that is carried by the document, so it survives on a machine with
//            no plugin and no script (tests/indesign-stroke/S-split/NOTES.md).
//   outline  Convert the selected text frames to outlines while keeping their appearance. Native
//            Type > Create Outlines writes a centered stroke at half the painted width, over the
//            fill, which loses the fill and halves the halo (docs/STROKE_MECHANISMS.md, M6).
//   resync   Rebuild the stroke frame of every split pair whose text has been edited.
//   watch    Do that automatically, about once a second, until stopped.
//   unwatch  Stop watching.
//   status   Report whether the watcher is running.
//
// The libraries are loaded from beside this file, so the whole tools\jsx folder can be dropped
// into InDesign's Scripts Panel folder and used from there.
(function () {
    var HERE = File($.fileName).parent;

    function load(name) {
        var f = File(HERE.fsName + '/' + name);
        if (!f.exists) { throw new Error('StrokeMaster: missing ' + name + ' beside ' + $.fileName); }
        $.evalFile(f);
    }
    // ise-lib.jsx takes ISE_ARGS and deletes the global, so a caller that already loaded it holds
    // the only copy of the arguments. Reloading it here would therefore wipe them, this script
    // would see no command, and a headless run would open the dialog and wait for a click that
    // no one is there to give. Carry the arguments across the reload instead.
    var carried = null;
    if ($.global.ISE_LIB && $.global.ISE_LIB.args) { carried = $.global.ISE_LIB.args; }
    else if ($.global.ISE_ARGS) { carried = $.global.ISE_ARGS; }

    load('ise-lib.jsx');
    load('ise-split-stroked-text.jsx');
    load('ise-faithful-outlines.jsx');
    // Loaded while ISE_LIB.args is still empty, so its own driver cannot fire off this script's
    // arguments; it acts only on a syncMode of its own.
    load('ise-live-sync.jsx');

    var lib = $.global.ISE_LIB;
    if (carried) { lib.args = carried; }
    var split = $.global.ISE_SPLIT;
    var faithful = $.global.ISE_FAITHFUL;
    var sync = $.global.ISE_SYNC;

    var COMMANDS = [
        ['split', 'Split stroked text into two frames (portable fix)'],
        ['outline', 'Create outlines, keeping the appearance'],
        ['resync', 'Resync split pairs now'],
        ['watch', 'Keep split pairs in sync automatically'],
        ['unwatch', 'Stop keeping them in sync'],
        ['status', 'Is the watcher running?']
    ];

    /** Asks which command to run. Returns null if the dialog is cancelled. */
    function ask() {
        var w = new Window('dialog', 'StrokeMaster');
        w.alignChildren = 'left';
        w.add('statictext', undefined, 'What should this do?');
        var group = w.add('panel', undefined, '');
        group.alignChildren = 'left';
        group.margins = 12;
        var buttons = [];
        for (var i = 0; i < COMMANDS.length; i++) {
            var rb = group.add('radiobutton', undefined, COMMANDS[i][1]);
            buttons.push(rb);
        }
        buttons[0].value = true;
        var row = w.add('group');
        row.add('button', undefined, 'Cancel', { name: 'cancel' });
        row.add('button', undefined, 'Run', { name: 'ok' });
        if (w.show() !== 1) { return null; }
        for (var j = 0; j < buttons.length; j++) {
            if (buttons[j].value) { return COMMANDS[j][0]; }
        }
        return null;
    }

    // The dialog has to come first: lib.run sets the user interaction level to NEVER_INTERACT,
    // which would suppress it.
    // A dialog must never be reachable from an automated run: it would block InDesign waiting for
    // a click nobody is there to give, and the whole application with it. Having been given
    // arguments at all is the signal that this is such a run, so a missing or misspelled command
    // is reported rather than prompted for.
    var invokedWithArgs = (carried !== null);
    var headless = !!(lib.args && lib.args.command);
    var command;
    if (headless) {
        command = String(lib.args.command);
    } else if (invokedWithArgs) {
        return 'StrokeMaster: called with arguments but without a "command"; nothing was done.';
    } else {
        command = ask();
    }
    if (command === null) { return 'cancelled'; }

    var out = lib.run(function () {
        lib.L('command=' + command + ' engine=' + ($.engineName || '(unnamed)'));
        if (app.documents.length === 0) { lib.L('No document is open.'); return; }
        var doc = app.activeDocument;

        if (command === 'split') {
            var frames = split.selectedFrames();
            if (frames.length === 0) { lib.L('Select one or more text frames first.'); return; }
            var results = split.split(frames, lib);
            var done = 0;
            for (var i = 0; i < results.length; i++) {
                if (results[i].done) { done++; }
                else { lib.L('skipped ' + results[i].name + ': ' + results[i].reason); }
            }
            lib.L('Split ' + done + ' of ' + results.length + ' frame(s).');

        } else if (command === 'outline') {
            var sel = split.selectedFrames(), made = 0, skipped = 0;
            if (sel.length === 0) { lib.L('Select one or more text frames first.'); return; }
            for (var k = 0; k < sel.length; k++) {
                if (!faithful.canConvert(sel[k])) {
                    skipped++;
                    lib.L('skipped ' + sel[k].name + ': mixed stroke or fill settings, left to InDesign');
                    continue;
                }
                if (faithful.convertFrame(doc, sel[k], function (s) { lib.L(s); })) { made++; }
            }
            lib.L('Converted ' + made + ' frame(s), skipped ' + skipped + '.');

        } else if (command === 'resync') {
            var r = sync.resyncAll(doc);
            lib.L('Pairs found ' + r.pairs + ', resynced ' + r.resynced + '.');
            for (var s = 0; s < r.skipped.length; s++) { lib.L('skipped: ' + r.skipped[s]); }

        } else if (command === 'watch') {
            var t = sync.start(Number((lib.args && lib.args.intervalMs) || 1000));
            lib.L('Watching every ' + t.sleep + ' ms. Run "unwatch" to stop.');

        } else if (command === 'unwatch') {
            lib.L(sync.stop() ? 'Stopped watching.' : 'It was not watching.');

        } else if (command === 'status') {
            lib.L(sync.status());

        } else {
            lib.L('Unknown command: ' + command);
        }
    });

    if (!headless) {
        // Report only the lines this run produced, not the library's own bookkeeping.
        var lines = String(out).split('\n'), shown = [];
        for (var n = 0; n < lines.length; n++) {
            if (/^(liveBefore|liveAfter|elapsed|pdfExportPreferences|pngExportPreferences|epsExportPreferences|command=)/.test(lines[n])) { continue; }
            if (lines[n] !== '') { shown.push(lines[n]); }
        }
        alert(shown.length ? shown.join('\n') : 'Nothing to report.', 'StrokeMaster');
    }
    return out;
})();
