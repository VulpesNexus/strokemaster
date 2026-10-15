// ai-pathfinder-twice.jsx - the fix's own Pathfinder Add found again by its effect's name
// (region-prep-18, 2026-10-16): two overlapping squares in a compound path, Inside 4, native-fix
// twice: one Pathfinder Add after each (style-dump), the second fix adding none. Needs
// StrokeMasterRegionProbe region-prep-18 and $.global.ISE_REGION_ARGS.
(function () {
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try { return run(); } finally { app.userInteractionLevel = interaction; }
    function run() {
    var out = [];
    function note(k, v) { out.push(k + '=' + String(v).replace(/[|\r\n]/g, ' ')); }
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function block(json, name) { var m = new RegExp('"' + name + '":(\\{[^}]*\\})').exec(json); return m ? m[1] : '?'; }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    function sel(it) { app.activeDocument.selection = null; it.selected = true; app.redraw(); }
    function count(dump) { return (dump.match(/'Adobe Pathfinder'/g) || []).length; }
    var doc = app.documents.addDocument('Web', new DocumentPreset());
    try {
        var cp = doc.compoundPathItems.add();
        doc.pathItems.rectangle(200, 40, 80, 80).move(cp, ElementPlacement.PLACEATEND);
        doc.pathItems.rectangle(160, 80, 80, 80).move(cp, ElementPlacement.PLACEATEND);
        var p = cp.pathItems[0]; p.filled = true; p.fillColor = gray(100); p.stroked = true; p.strokeColor = gray(60); p.strokeWidth = 4;
        sel(cp); note('align', send('stroke-align', 'inside').length > 0);
        sel(cp); note('fix1', block(send('native-fix'), 'nativeFix'));
        sel(cp); var d1 = send('style-dump'); note('pathfinders1', count(d1));
        sel(cp); note('fix2', block(send('native-fix'), 'nativeFix'));
        sel(cp); var d2 = send('style-dump'); note('pathfinders2', count(d2));
        note('style', d2);
        return 'DONE|' + out.join('|');
    } catch (e) {
        return 'FAIL|' + String(e).replace(/[|\r\n]/g, ' ') + ' line ' + e.line + '|' + out.join('|');
    }
    }
})();
