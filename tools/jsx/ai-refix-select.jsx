// ai-refix-select.jsx - the automatic re-fix, a phase after ai-refix-edit.jsx: selecting fixed art is
// not an edit, so it must not start a re-fix (the next phase, ai-refix-override.jsx, compares the
// re-fix counters). Run: -Script ai-refix-phase1.jsx -Script2 ai-refix-edit.jsx,ai-refix-select.jsx,
// ai-refix-override.jsx,ai-refix-final.jsx
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
        note('refix.before-select', block(send('status'), 'refix'));
        doc.selection = null; p.selected = true; t.selected = true;
        app.redraw();
        return 'DONE|' + out.join('|');
    } catch (error) {
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + ' line ' + error.line + '|' + out.join('|');
    }
})();
