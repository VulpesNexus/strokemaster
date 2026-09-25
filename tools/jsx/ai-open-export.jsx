// Opens one AI file, exports it as a flat PDF, reports, and closes it without
// saving. For cross-process and missing-plugin checks. Needs
// $.global.ISE_REGION_ARGS: input (AI path), output (PDF path, must not
// exist), and optional plugin (a script-message plugin name whose status is
// read if it answers). Alerts are suppressed, so a missing-plugin warning
// cannot block the call, and the previous level is restored.
(function () {
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try { return run(); } finally { app.userInteractionLevel = interaction; }
    function run() {
    var args = $.global.ISE_REGION_ARGS || {};
    var out = [];
    function note(k, v) { out.push(k + '=' + String(v).replace(/[|\r\n]/g, ' ')); }
    var doc = null, at = 'begin';
    try {
        var input = new File(args.input), output = new File(args.output);
        if (!input.exists) return 'FAIL|input missing';
        if (output.exists) return 'FAIL|refusing to overwrite output';
        var status = function (tag) {
            if (!args.plugin) return;
            try { note(tag, String(app.sendScriptMessage(args.plugin, 'status', ''))); }
            catch (e) { note(tag, 'no answer: ' + e); }
        };
        status('before');
        at = 'open';
        doc = app.open(input);
        app.redraw();
        note('textFrames', doc.textFrames.length);
        note('contents', doc.textFrames.length ? doc.textFrames[0].contents.replace(/\r/g, '/') : '');
        status('afterOpen');
        at = 'export';
        var options = new PDFSaveOptions();
        options.acrobatLayers = false;
        doc.saveAs(output, options);
        note('pdf', output.length);
        at = 'close';
        doc.close(SaveOptions.DONOTSAVECHANGES);
        doc = null;
        return 'DONE|' + out.join('|');
    } catch (error) {
        if (doc) { try { doc.close(SaveOptions.DONOTSAVECHANGES); } catch (ignored) {} }
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + '|' + out.join('|');
    }
    }
})();
