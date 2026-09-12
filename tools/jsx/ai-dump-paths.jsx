// ai-dump-paths.jsx - write Illustrator path geometry to JSON, for measuring outside the host.
// Pure ASCII: ExtendScript sources are read in the system codepage.
//
// Run through tools\run-ai-jsx.ps1, which sets ISE_AI_ARGS and deletes it afterward:
//   powershell -NoProfile -File tools\run-ai-jsx.ps1 ai-dump-paths.jsx -ArgJson "{\"out\":\"C:/tmp/p.json\"}"
//
// Arguments:
//   out    where to write the JSON. Required.
//   group  name of a group to read instead of the selection, for example "Flattened".
//
// What it picks, in order: the named group if one is given, else the current selection, else
// every page item. Groups and compound paths are walked down to their leaf paths, because a
// compound path's subpaths are what actually carry points.
//
// Anchors alone are not enough. A corner's angle has to come from the Bezier handles: on a
// curved segment the direction at an anchor is set by its handle, so measuring toward the next
// anchor would report a corner where the outline is smooth and understate the ones that are
// real. Both handles are therefore written for every point.
//
// Coordinates are Illustrator's own, in points, with y increasing upward.
(function () {
    var args = $.global.ISE_AI_ARGS || {};
    if (!args.out) { return 'ERROR: no output path given'; }
    if (app.documents.length === 0) { return 'ERROR: no document is open'; }
    var doc = app.activeDocument;

    var leaves = [];
    function walk(item) {
        var t = item.typename;
        if (t === 'GroupItem') {
            for (var i = 0; i < item.pageItems.length; i++) { walk(item.pageItems[i]); }
        } else if (t === 'CompoundPathItem') {
            for (var j = 0; j < item.pathItems.length; j++) { leaves.push(item.pathItems[j]); }
        } else if (t === 'PathItem') {
            leaves.push(item);
        }
    }

    var source = 'all page items';
    if (args.group) {
        var found = null;
        for (var g = 0; g < doc.pageItems.length; g++) {
            if (String(doc.pageItems[g].name) === String(args.group)) { found = doc.pageItems[g]; }
        }
        if (found === null) { return 'ERROR: no item named ' + args.group; }
        walk(found);
        source = 'group ' + args.group;
    } else if (doc.selection && doc.selection.length > 0) {
        for (var s = 0; s < doc.selection.length; s++) { walk(doc.selection[s]); }
        source = 'selection of ' + doc.selection.length;
    } else {
        for (var p = 0; p < doc.pageItems.length; p++) { walk(doc.pageItems[p]); }
    }
    if (leaves.length === 0) { return 'ERROR: nothing with path points was found (' + source + ')'; }

    // Built by hand rather than with a JSON library: ExtendScript has none, and the format is
    // small enough that a serializer would be more code than the writer.
    var out = '{"source":"' + source + '","paths":[';
    for (var k = 0; k < leaves.length; k++) {
        var path = leaves[k];
        if (k > 0) { out += ','; }
        out += '{"index":' + k +
               ',"points":' + path.pathPoints.length +
               ',"closed":' + path.closed +
               ',"area":' + path.area.toFixed(4) +
               ',"gb":[' + path.geometricBounds.join(',') + ']' +
               ',"pts":[';
        for (var n = 0; n < path.pathPoints.length; n++) {
            var q = path.pathPoints[n];
            if (n > 0) { out += ','; }
            out += '{"a":[' + q.anchor[0].toFixed(4) + ',' + q.anchor[1].toFixed(4) + ']' +
                   ',"l":[' + q.leftDirection[0].toFixed(4) + ',' + q.leftDirection[1].toFixed(4) + ']' +
                   ',"r":[' + q.rightDirection[0].toFixed(4) + ',' + q.rightDirection[1].toFixed(4) + ']}';
        }
        out += ']}';
    }
    out += ']}';

    var file = new File(String(args.out));
    file.parent.create();
    file.encoding = 'UTF-8';
    file.lineFeed = 'Unix';
    file.open('w');
    file.write(out);
    file.close();
    return 'wrote ' + file.fsName + '  paths=' + leaves.length + '  from ' + source;
})();
