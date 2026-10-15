// StrokeMaster: Outside or Inside where Illustrator has no stroke alignment (an opt-in; research
// build).
//
// Illustrator draws Inside and Outside on closed paths and on compound paths of closed contours
// only. It ignores them on open paths, on every contour of a compound path that has an open one,
// and on type (it stores the setting but draws Center). Run this from File > Scripts > Other
// Script with art selected, and choose:
//   Outside or Inside  closed shapes get Illustrator's own alignment; open paths a one-sided width
//                      profile at the full weight (the side the path's closing would enclose is
//                      Inside); a compound path with an open contour, Outside only, a Center
//                      stroke at twice the weight beneath its fill; type, Outside only, a stroke
//                      of twice the weight beneath the characters. Inside on those two has no
//                      faithful native form and is left as it is.
//   Restore            what this script or strokemaster-outside-dashes.jsx made of the selection,
//                      put back as it was.
// Needs the research plugin StrokeMasterRegionProbe (region-prep-17 or later).
(function () {
    var plugin = 'StrokeMasterRegionProbe';
    var args = $.global.ISE_REGION_ARGS;
    var testing = !!args;
    function tell(msg) { if (testing) { return msg; } alert(msg); return msg; }
    if (app.documents.length === 0) { return tell('StrokeMaster: open a document and select art first.'); }
    if (app.activeDocument.selection.length === 0) { return tell('StrokeMaster: select the art first.'); }
    var mode = testing ? args.align : null;
    if (!testing) {
        // Asked before anything changes.
        var w = new Window('dialog', 'StrokeMaster: align strokes');
        w.add('statictext', undefined, 'Align the selected strokes:');
        var outside = w.add('radiobutton', undefined, 'Outside'), inside = w.add('radiobutton', undefined, 'Inside');
        var restore = w.add('radiobutton', undefined, 'Restore what StrokeMaster changed');
        outside.value = true;
        var row = w.add('group');
        row.add('button', undefined, 'OK', { name: 'ok' });
        row.add('button', undefined, 'Cancel', { name: 'cancel' });
        if (w.show() !== 1) { return 'cancelled'; }
        mode = outside.value ? 'outside' : (inside.value ? 'inside' : 'restore');
    }
    var align, fix;
    try {
        align = String(app.sendScriptMessage(plugin, 'native-align', mode));
        fix = mode === 'restore' ? '' : String(app.sendScriptMessage(plugin, 'native-fix', ''));
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
    var msg = mode === 'restore'
        ? 'StrokeMaster: ' + field(align, 'align', 'restored') + ' item(s) put back as they were.'
        : 'StrokeMaster: ' + field(align, 'align', 'native') + ' with Illustrator\'s own alignment, ' +
          field(align, 'align', 'profiled') + ' open path(s) one-sided, ' + field(align, 'align', 'doubled') +
          ' compound path(s) doubled beneath the fill, ' + field(align, 'align', 'typeOutside') + ' type object(s) stroked beneath the characters.';
    if (refused > 0) { msg += '\n' + refused + ' left as they were (' + field(align, 'align', 'reasons') + ').'; }
    return tell(msg);
})();
