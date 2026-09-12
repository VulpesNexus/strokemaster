// ai-outline-for-measure.jsx - take a copy of stroked text to outlines and write its geometry.
// Pure ASCII. Run through tools\run-ai-jsx.ps1.
//
// Arguments:
//   out     where to write the JSON. Required.
//   name    name of the page item to use; otherwise the selection, otherwise the first text frame.
//
// Why a copy. Live text will not tell you what you need. A TextFrame does not expose stroked,
// strokeWidth, strokeJoin or strokeMiterLimit at all, and characterAttributes.strokeWeight
// reports the character default rather than a stroke applied through the object's appearance:
// on a 50 pt W whose real stroke was 16 pt it read 1. Outlined paths do expose those properties,
// so outlining a DUPLICATE answers what the stroke actually is, and hands over the glyph
// geometry to measure, without touching the original.
//
// The duplicate is removed in a finally, so the document is left as it was found whether or not
// the rest succeeds.
(function () {
    var args = $.global.ISE_AI_ARGS || {};
    var log = [];
    function say(s) { log.push(String(s)); }

    if (!args.out) { return 'ERROR: no output path given'; }
    if (app.documents.length === 0) { return 'ERROR: no document is open'; }
    var doc = app.activeDocument;

    // ---- pick the source ---------------------------------------------------------------------
    var source = null, how = '';
    if (args.name) {
        for (var i = 0; i < doc.pageItems.length; i++) {
            if (String(doc.pageItems[i].name) === String(args.name)) { source = doc.pageItems[i]; }
        }
        if (source === null) { return 'ERROR: no item named ' + args.name; }
        how = 'item named ' + args.name;
    } else {
        var sel = [];
        try { sel = doc.selection || []; } catch (e0) { sel = []; }
        if (sel.length > 0) { source = sel[0]; how = 'first selected item'; }
    }
    if (source === null) {
        for (var t = 0; t < doc.pageItems.length; t++) {
            if (doc.pageItems[t].typename === 'TextFrame') { source = doc.pageItems[t]; how = 'first text frame'; break; }
        }
    }
    if (source === null) { return 'ERROR: found nothing to measure'; }
    say('source: ' + source.typename + ' (' + how + ')');
    try { say('source bounds: [' + source.geometricBounds.join(', ') + ']'); } catch (e1) {}
    try {
        if (source.typename === 'TextFrame') {
            var ca = source.textRange.characterAttributes;
            say('source text: "' + String(source.contents).substring(0, 40) + '"');
            say('source font: ' + ca.textFont.name + '  size: ' + ca.size +
                '  characterAttributes.strokeWeight: ' + ca.strokeWeight + ' (unreliable, see header)');
        }
    } catch (e2) {}

    // ---- outline a duplicate -----------------------------------------------------------------
    var copy = null, outlined = null, leaves = [];
    try {
        copy = source.duplicate();
        if (copy.typename === 'TextFrame') {
            outlined = copy.createOutlines();
            // createOutlines replaces the frame, so the duplicate handle is spent either way.
            copy = null;
        } else {
            outlined = copy;
            copy = null;
        }
        say('outlined: ' + (outlined ? outlined.typename : 'nothing returned'));

        function walk(item) {
            var ty = item.typename;
            if (ty === 'GroupItem') {
                for (var g = 0; g < item.pageItems.length; g++) { walk(item.pageItems[g]); }
            } else if (ty === 'CompoundPathItem') {
                for (var c = 0; c < item.pathItems.length; c++) { leaves.push(item.pathItems[c]); }
            } else if (ty === 'PathItem') {
                leaves.push(item);
            }
        }
        if (outlined) { walk(outlined); }
        say('leaf paths: ' + leaves.length);

        // ---- what is the stroke, really? -----------------------------------------------------
        var reported = 0;
        for (var k = 0; k < leaves.length && reported < 4; k++) {
            var p = leaves[k], line = '  path[' + k + '] points=' + p.pathPoints.length + ' closed=' + p.closed;
            try {
                if (p.stroked) {
                    line += '  STROKED width=' + p.strokeWidth + ' join=' + p.strokeJoin + ' miterLimit=' + p.strokeMiterLimit;
                } else {
                    line += '  unstroked';
                }
            } catch (e3) { line += '  stroke properties unreadable: ' + e3.message; }
            try { line += ' filled=' + p.filled; } catch (e4) {}
            say(line);
            reported++;
        }

        // ---- write the geometry --------------------------------------------------------------
        var json = '{"source":"' + how + '","paths":[';
        for (var m = 0; m < leaves.length; m++) {
            var path = leaves[m];
            if (m > 0) { json += ','; }
            json += '{"index":' + m +
                    ',"points":' + path.pathPoints.length +
                    ',"closed":' + path.closed +
                    ',"area":' + path.area.toFixed(4) +
                    ',"gb":[' + path.geometricBounds.join(',') + ']';
            try { json += ',"strokeWidth":' + (path.stroked ? path.strokeWidth : 0); } catch (e5) { json += ',"strokeWidth":0'; }
            json += ',"pts":[';
            for (var n = 0; n < path.pathPoints.length; n++) {
                var q = path.pathPoints[n];
                if (n > 0) { json += ','; }
                json += '{"a":[' + q.anchor[0].toFixed(4) + ',' + q.anchor[1].toFixed(4) + ']' +
                        ',"l":[' + q.leftDirection[0].toFixed(4) + ',' + q.leftDirection[1].toFixed(4) + ']' +
                        ',"r":[' + q.rightDirection[0].toFixed(4) + ',' + q.rightDirection[1].toFixed(4) + ']}';
            }
            json += ']}';
        }
        json += ']}';

        var file = new File(String(args.out));
        file.parent.create();
        file.encoding = 'UTF-8';
        file.lineFeed = 'Unix';
        file.open('w');
        file.write(json);
        file.close();
        say('wrote ' + file.fsName);
    } catch (err) {
        say('FAILED: ' + err.message + ' line ' + err.line);
    } finally {
        // Leave the document exactly as it was found.
        try { if (outlined && outlined.isValid !== false) { outlined.remove(); say('removed the outlined duplicate'); } } catch (e6) { say('could not remove the outlined duplicate: ' + e6.message); }
        try { if (copy) { copy.remove(); say('removed the duplicate'); } } catch (e7) {}
    }
    say('page items remaining: ' + doc.pageItems.length);
    return log.join('\n');
})();
