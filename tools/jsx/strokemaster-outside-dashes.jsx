// StrokeMaster: dashed Outside strokes with whole points (an opt-in; research build).
//
// Illustrator fits dashes to corners on Center strokes only, so on a dashed Outside stroke a gap
// can fall on a point. Run this from File > Scripts > Other Script with art selected: each selected
// path or compound path with a dashed Outside stroke and one opaque fill is rewritten as what
// Illustrator draws the same way, a Center stroke at twice the weight beneath its fill, with
// Align Dashes to Corners on. The Stroke panel then shows Center and the doubled weight, which is
// why it is not part of the main fix. strokemaster-align-selection.jsx with Restore puts such a
// stroke back as it was (Outside, its own weight); Edit > Undo does too.
// Needs the research plugin StrokeMasterRegionProbe (region-prep-17 or later).
(function () {
    var plugin = 'StrokeMasterRegionProbe';
    var testing = !!$.global.ISE_REGION_ARGS;
    function tell(msg) { if (testing) { return msg; } alert(msg); return msg; }
    if (app.documents.length === 0) { return tell('StrokeMaster: open a document and select art first.'); }
    if (app.activeDocument.selection.length === 0) { return tell('StrokeMaster: select the art first.'); }
    var align, fix;
    try {
        align = String(app.sendScriptMessage(plugin, 'native-align', 'dashes'));
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
    var refused = Number(field(align, 'align', 'refused'));
    return tell('StrokeMaster: ' + field(align, 'align', 'doubled') + ' dashed Outside stroke(s) rewritten as Center beneath the fill, ' +
                'dashes aligned to corners; ' + field(fix, 'nativeFix', 'pathLimits') + ' miter limit(s) set.' +
                (refused > 0 ? '\n' + refused + ' left as they were (' + field(align, 'align', 'reasons') + ').' : ''));
})();
