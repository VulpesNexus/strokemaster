// ai-refix-edit.jsx - the automatic re-fix, phase 2 (see ai-refix-phase1.jsx): the edits, alone
// in one script run so they make one undo step: the thorn's tip sharpened from 10 to 6 degrees (its
// whole point's ratio rises from 11.5 to 19.1), and "vw" added after the K (new characters inherit
// the K's limit). The re-fix runs once this returns and Illustrator is idle.
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
        note('refix.before', block(send('status'), 'refix'));
        function tip(deg) {
            var half = deg / 2 * Math.PI / 180, side = 60;
            return [100 + side * Math.sin(half), 280 - side * Math.cos(half), 100 - side * Math.sin(half)];
        }
        at = 'edit';
        // The tip sharpened to 6 degrees: its anchors move, its ratio rises from 11.5 to 19.1.
        var s = tip(6);
        p.pathPoints[1].anchor = [s[0], s[1]]; p.pathPoints[1].leftDirection = [s[0], s[1]]; p.pathPoints[1].rightDirection = [s[0], s[1]];
        p.pathPoints[6].anchor = [s[2], s[1]]; p.pathPoints[6].leftDirection = [s[2], s[1]]; p.pathPoints[6].rightDirection = [s[2], s[1]];
        t.textRange.characters[5].contents = 'Kvw';
        // The user's squares set to Center: no union is needed any more, and their own
        // Pathfinder Add must stay.
        doc.selection = null; doc.compoundPathItems.getByName('refix user squares').selected = true;
        note('user.center', block(send('stroke-align', 'center'), 'appearance'));
        doc.selection = null;
        note('edited.text', t.contents);
        note('edited.thorn', p.strokeMiterLimit);
        note('edited.limits', limits(t));
        return 'DONE|' + out.join('|');
    } catch (error) {
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + ' line ' + error.line + '|' + out.join('|');
    }
})();
