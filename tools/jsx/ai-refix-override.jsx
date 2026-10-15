// ai-refix-override.jsx - the automatic re-fix, after ai-refix-select.jsx: the re-fix counters (a
// selection must not have started one), then the thorn's miter limit set by hand to 7, a value the
// fix did not choose. The re-fix must leave it, and stop following the thorn (ai-refix-final.jsx).
(function () {
    var plugin = 'StrokeMasterRegionProbe';
    var out = [];
    function note(k, v) { out.push(k + '=' + String(v).replace(/[|\r\n]/g, ' ')); }
    function send(selector, input) { return String(app.sendScriptMessage(plugin, selector, input || '')); }
    function block(json, name) { var m = new RegExp('"' + name + '":(\\{[^}]*\\})').exec(json); return m ? m[1] : '?'; }
    function limits(t) {
        app.activeDocument.selection = null; t.selected = true;
        var v = send('char-limits');
        app.activeDocument.selection = null;
        return v;
    }
    var at = 'begin';
    try {
        var doc = app.activeDocument;
        var p = doc.pathItems.getByName('refix thorn'), t = doc.textFrames.getByName('refix text');
        note('refix.after-select', block(send('status'), 'refix'));
        doc.selection = null;
        at = 'override';
        p.strokeMiterLimit = 7;
        note('override.thorn', p.strokeMiterLimit);
        return 'DONE|' + out.join('|');
    } catch (error) {
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + ' line ' + error.line + '|' + out.join('|');
    }
})();
