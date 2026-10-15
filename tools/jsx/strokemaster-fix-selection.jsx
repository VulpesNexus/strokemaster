// StrokeMaster: fix the selected strokes in Illustrator, natively (research build).
//
// Run it from File > Scripts > Other Script with art selected. Needs the research plugin
// StrokeMasterRegionProbe loaded (region-prep-16 or later). It writes ordinary Illustrator
// attributes, so the result needs no plugin to draw, and text stays live and editable:
//   - text frames: an object-level fill above Characters (one fill color across the frame), an
//     object-level stroke below Characters (one stroke across the frame), or, with both fills
//     and strokes mixed, each character's stroke painted before its own fill;
//   - each character, path, and compound path with a mitered stroke: a miter limit chosen
//     from its outline (whole points kept, glyph tips and escaping interior miters cut);
//   - Outside strokes on overlapping contours: the fill moved above the stroke; Inside strokes
//     on overlapping contours: Illustrator's Effect > Pathfinder > Add, so neither strokes the
//     contour edges inside the filled region;
//   - dashed Center strokes: Illustrator's Align Dashes to Corners, so a dashed point is whole.
// While the plugin runs, an edit to art fixed here is fixed again in the same undo step.
(function () {
    var plugin = 'StrokeMasterRegionProbe';
    // Run by tools/run-ai-fresh.ps1 (ISE_REGION_ARGS set), it returns its message instead of
    // showing it, so the test does not block on a dialog.
    var testing = !!$.global.ISE_REGION_ARGS;
    function tell(msg) { if (testing) { return msg; } alert(msg); return msg; }
    if (app.documents.length === 0) { return tell('StrokeMaster: open a document and select art first.'); }
    var doc = app.activeDocument;
    if (doc.selection.length === 0) { return tell('StrokeMaster: select the art to fix first.'); }
    var route, fix;
    try {
        route = String(app.sendScriptMessage(plugin, 'native-route', ''));
        fix = String(app.sendScriptMessage(plugin, 'native-fix', ''));
    } catch (e) {
        return tell('StrokeMaster: the plugin ' + plugin + ' did not answer (' + e + ').');
    }
    function field(json, block, name) {
        var b = new RegExp('"' + block + '":\\{[^}]*\\}').exec(json);
        if (!b) { return '?'; }
        var m = new RegExp('"' + name + '":("?)([^,"}]*)').exec(b[0]);
        return m ? m[2] : '?';
    }
    var lines = ['StrokeMaster:',
        field(route, 'appearance', 'styled') + ' text frame(s) given an object-level stroke-behind, ' +
            field(route, 'appearance', 'fillFirst') + ' a per-character one;',
        field(fix, 'nativeFix', 'charLimits') + ' character and ' + field(fix, 'nativeFix', 'pathLimits') + ' path miter limit(s) set;',
        field(fix, 'nativeFix', 'reordered') + ' Outside fill(s) moved above the stroke, ' +
            field(fix, 'nativeFix', 'pathfinder') + ' Inside path(s) united by Pathfinder Add;',
        field(fix, 'nativeFix', 'dashes') + ' dashed stroke(s) aligned to corners.'];
    var refused = Number(field(fix, 'nativeFix', 'dashRefused'));
    if (refused > 0) {
        lines.push(refused + ' dashed Inside or Outside stroke(s) left as they are: Illustrator aligns dashes on Center strokes only.');
    }
    return tell(lines.join('\n'));
})();
