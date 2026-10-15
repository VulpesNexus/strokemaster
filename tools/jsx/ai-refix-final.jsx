// ai-refix-final.jsx - the automatic re-fix, after ai-refix-override.jsx: the hand-set limit must
// still be 7 and the thorn released (the counter), the text (with a space) still followed, and the
// user's own Pathfinder Add still on their squares.
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
        note('refix.after-override', block(send('status'), 'refix'));
        note('final.thorn', p.strokeMiterLimit);
        note('final.text', t.contents + ' ' + limits(t));
        var u = doc.compoundPathItems.getByName('refix user squares');
        doc.selection = null; u.selected = true;
        note('final.user', /Adobe Pathfinder/.test(send('style-dump')) ? 'user Pathfinder kept' : 'user Pathfinder GONE');
        doc.selection = null;
        return 'DONE|' + out.join('|');
    } catch (error) {
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + ' line ' + error.line + '|' + out.join('|');
    }
})();
