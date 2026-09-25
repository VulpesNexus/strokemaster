// Opens one AI file, appends one character to its first text frame's last
// character, exports a flat PDF, and closes without saving. For checking what
// an effected text renders as after an edit when the plugin is missing.
// Needs $.global.ISE_REGION_ARGS: input (AI path), output (PDF path, must not
// exist). Alerts are suppressed and the previous level restored.
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
        at = 'open';
        doc = app.open(input);
        app.redraw();
        var t = doc.textFrames[0];
        at = 'edit';
        var chars = t.textRange.characters;
        chars[chars.length - 1].contents = chars[chars.length - 1].contents + 'H';
        app.redraw();
        note('contents', t.contents.replace(/\r/g, '/'));
        note('visibleBounds', t.visibleBounds.join(','));
        at = 'export';
        var options = new PDFSaveOptions();
        options.acrobatLayers = false;
        doc.saveAs(output, options);
        note('pdf', output.length);
        doc.close(SaveOptions.DONOTSAVECHANGES);
        doc = null;
        return 'DONE|' + out.join('|');
    } catch (error) {
        if (doc) { try { doc.close(SaveOptions.DONOTSAVECHANGES); } catch (ignored) {} }
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + '|' + out.join('|');
    }
    }
})();
