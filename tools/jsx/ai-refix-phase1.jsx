// ai-refix-phase1.jsx - the automatic re-fix (region-prep-16), phase 1. Draws the census
// thorn (Center 8, miter limit 4), live text ("VW AMK", Minion Pro 36 pt, Center 8), and two
// overlapping squares, Inside 8, carrying the user's own Effect > Pathfinder > Add; fixes all three
// with native-route and native-fix (which marks them). Phase 2 (ai-refix-edit.jsx) edits them, in
// an undo step of its own; then either ai-refix-phase2.jsx (the re-fix and undo) or
// ai-refix-select.jsx, ai-refix-override.jsx, ai-refix-final.jsx (selection, a hand-set limit):
//   run-ai-fresh.ps1 -Script ai-refix-phase1.jsx -Script2 ai-refix-edit.jsx,ai-refix-phase2.jsx
// Leaves the document open. Alerts suppressed and restored.
(function () {
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try { return run(); } finally { app.userInteractionLevel = interaction; }
    function run() {
    function newDocument(cs, w, h) {
        try { return app.documents.add(cs, w, h); } catch (e) {
            var pr = new DocumentPreset(); pr.colorMode = cs; pr.width = w; pr.height = h; pr.units = RulerUnits.Points;
            return app.documents.addDocument(cs === DocumentColorSpace.CMYK ? 'Print' : 'Web', pr);
        }
    }
    var plugin = 'StrokeMasterRegionProbe';
    var out = [];
    function note(k, v) { out.push(k + '=' + String(v).replace(/[|\r\n]/g, ' ')); }
    function send(selector, input) { return String(app.sendScriptMessage(plugin, selector, input || '')); }
    function block(json, name) { var m = new RegExp('"' + name + '":(\\{[^}]*\\})').exec(json); return m ? m[1] : '?'; }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    function limits(t) {
        app.activeDocument.selection = null; t.selected = true;
        var v = send('char-limits');
        app.activeDocument.selection = null;
        return v;
    }
    var at = 'begin';
    try {
        note('build', /"build":"([^"]*)"/.exec(send('status'))[1]);
        var doc = newDocument(DocumentColorSpace.RGB, 400, 300);
        doc.artboards[0].artboardRect = [0, 300, 400, 0];
        at = 'draw';
        function tip(deg) {
            var half = deg / 2 * Math.PI / 180, side = 60;
            return [100 + side * Math.sin(half), 280 - side * Math.cos(half), 100 - side * Math.sin(half)];
        }
        var k = tip(10), y = 280 - 60 * Math.cos(5 * Math.PI / 180);
        var p = doc.pathItems.add();
        p.setEntirePath([[100, 280], [k[0], k[1]], [145, y], [145, y - 60], [55, y - 60], [55, y], [k[2], k[1]]]);
        p.closed = true; p.filled = true; p.fillColor = gray(100);
        p.stroked = true; p.strokeColor = gray(40); p.strokeWidth = 8; p.strokeJoin = StrokeJoin.MITERENDJOIN; p.strokeMiterLimit = 4;
        p.name = 'refix thorn';
        var t = doc.textFrames.add();
        t.contents = 'VW AMK';
        var ca = t.textRange.characterAttributes;
        try { ca.textFont = app.textFonts.getByName('MinionPro-Regular'); } catch (e) {}
        ca.size = 36; ca.fillColor = gray(100); ca.strokeColor = gray(40); ca.strokeWeight = 8;
        t.position = [180, 250];
        t.name = 'refix text';
        // The user's own Pathfinder Add on Inside overlapping squares: the fix must use it, never
        // claim or remove it.
        var u = doc.compoundPathItems.add();
        var u1 = doc.pathItems.rectangle(120, 20, 60, 60); u1.move(u, ElementPlacement.PLACEATEND);
        var u2 = doc.pathItems.rectangle(90, 50, 60, 60); u2.move(u, ElementPlacement.PLACEATEND);
        for (var q = 0; q < u.pathItems.length; q++) {
            u.pathItems[q].filled = true; u.pathItems[q].fillColor = gray(100);
            u.pathItems[q].stroked = true; u.pathItems[q].strokeColor = gray(40); u.pathItems[q].strokeWidth = 8;
        }
        u.name = 'refix user squares';
        doc.selection = null; u.selected = true;
        send('stroke-align', 'inside');
        app.executeMenuCommand('Live Pathfinder Add');
        note('user.style', send('style-dump').replace(/^.*?(PRE [^;]*Pathfinder[^{]*).*$/, '$1'));
        doc.selection = null; p.selected = true; t.selected = true; u.selected = true;
        at = 'fix';
        note('route', block(send('native-route'), 'appearance'));
        note('fix', block(send('native-fix'), 'nativeFix'));
        note('fixed.thorn', p.strokeMiterLimit);
        note('fixed.text', limits(t));
        doc.selection = null;
        note('refix', block(send('status'), 'refix'));
        return 'DONE|' + out.join('|');
    } catch (error) {
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + ' line ' + error.line + '|' + out.join('|');
    }
    }
})();
