// StrokeMaster: fix the selected strokes in Illustrator, natively (research build).
//
// Run it from File > Scripts > Other Script with art selected. Needs the research plugin
// StrokeMasterRegionProbe loaded (region-prep-14 or later). It writes ordinary Illustrator
// attributes, so the result needs no plugin to draw, and text stays live and editable:
//   - text frames: an object-level fill above Characters (one fill color across the frame)
//     or an object-level stroke below Characters (one stroke across the frame), so no
//     stroke paints over any fill; frames with both mixed fills and mixed strokes are left
//     as they are and reported;
//   - each character, path, and compound path with a mitered stroke: a miter limit chosen
//     from its outline (whole points kept, glyph tips and escaping interior miters cut);
//   - Outside strokes on compound paths: the fill moved above the stroke, so contour edges
//     inside the filled region are not stroked;
//   Dashed strokes are left as they are: tick Align Dashes to Corners in the Stroke panel
//   for a dashed point to be drawn whole.
// An edit can leave a new corner with a limit chosen for the old outline: run it again.
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
    var mixed = Math.floor(Number(field(route, 'appearance', 'contentsAfter')) / 1000);
    return tell('StrokeMaster: ' + field(route, 'appearance', 'styled') + ' text frame(s) given a native stroke-behind, ' +
          field(fix, 'nativeFix', 'charLimits') + ' character and ' + field(fix, 'nativeFix', 'pathLimits') + ' path miter limit(s) set, ' +
          field(fix, 'nativeFix', 'reordered') + ' Outside fill(s) moved above the stroke.' +
          (mixed > 0 ? '\n' + mixed + ' frame(s) with both mixed fills and mixed strokes were left as they are.' : ''));
})();
