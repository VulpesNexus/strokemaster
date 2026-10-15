// ai-refix-phase2.jsx - the automatic re-fix, after ai-refix-edit.jsx (see ai-refix-phase1.jsx) and
// idle time: what the timer's re-fix did, and whether one undo takes the re-fix back together with
// the edit that caused it (the re-fix is appended to that edit's step). A script's undo is part of
// the script's own undo step, so redo cannot be tested from a script.
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
        note('refix', block(send('status'), 'refix'));
        note('after.thorn', p.strokeMiterLimit);
        note('after.text', t.contents + ' ' + limits(t));
        function undoState(k) { note(k, block(send('status'), 'refix').replace(/.*"undoPast":(-?\d+),"undoFuture":(-?\d+).*/, 'past $1 future $2')); }
        function step(what, f) { try { f(); app.redraw(); return 'ok'; } catch (e) { return 'error ' + e; } }
        // With ISE_REGION_ARGS.pause, the re-fix is paused first: undo and redo as scripting does them,
        // without the plugin, for comparison.
        if (($.global.ISE_REGION_ARGS || {}).pause) note('paused', block(send('refix', 'off'), 'refix'));
        undoState('undo.before');
        // One undo: the re-fix was appended to the last edit's step, so both go back together.
        at = 'undo';
        note('undo1', step('undo', function () { app.executeMenuCommand('undo'); }));
        undoState('undo1.counts');
        note('undo1.thorn', p.strokeMiterLimit + ' tip ' + Math.round(p.pathPoints[1].anchor[0] * 100) / 100);
        note('undo1.text', t.contents + ' ' + limits(t));
        note('refix.end', block(send('status'), 'refix'));
        return 'DONE|' + out.join('|');
    } catch (error) {
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + ' line ' + error.line + '|' + out.join('|');
    }
})();
