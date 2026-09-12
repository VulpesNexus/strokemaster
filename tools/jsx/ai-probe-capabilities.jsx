// ai-probe-capabilities.jsx - what the Illustrator command layer can actually rely on.
// Pure ASCII. Run through tools\run-ai-jsx.ps1 so that any error reports a real line number.
//
// Written defensively on purpose. An earlier version of this probe reached app.activeDocument
// before checking whether a document was open, and Illustrator answered "Error 1302: No such
// element", which says nothing about which statement failed. It also used typeof on host
// properties: Illustrator's object model can throw on reading a property that does not exist
// rather than returning undefined, so absence has to be discovered with try/catch around real
// use, not with typeof.
//
// The three facts the clipped-miter command depends on:
//
//   P1  a selected path reports the stroke parameters the outline has to be built from,
//   P2  a path can be created from a plain array of points, which is how the computed outline
//       gets back into the document,
//   P3  something can resolve a self-intersecting region into clean geometry, because the
//       stroke region is a union of one quad per segment and one wedge per corner.
(function () {
    var out = [];
    function say(s) { out.push(String(s)); }

    say('application: ' + app.name + ' ' + app.version);
    say('documents open: ' + app.documents.length);
    if (app.documents.length === 0) {
        say('No document is open, so nothing further can be probed.');
        return out.join('\n');
    }
    var doc = app.activeDocument;
    say('document: ' + doc.name + '  rulerUnits=' + doc.rulerUnits + '  layers=' + doc.layers.length);

    // ---- P1: what does the selection expose? -------------------------------------------------
    say('');
    say('P1 selection');
    var sel = [];
    try { sel = doc.selection || []; } catch (e0) { say('  selection unreadable: ' + e0.message); }
    say('  selected items: ' + sel.length);
    for (var i = 0; i < Math.min(sel.length, 6); i++) {
        var s = sel[i], line = '  [' + i + '] ' + s.typename;
        try { line += ' name="' + s.name + '"'; } catch (e1) {}
        try {
            if (s.stroked) {
                line += ' STROKED width=' + s.strokeWidth + ' join=' + s.strokeJoin + ' miterLimit=' + s.strokeMiterLimit;
            } else {
                line += ' unstroked';
            }
        } catch (e2) { line += ' stroke properties unreadable (' + e2.message + ')'; }
        try { if (s.typename === 'PathItem') { line += ' points=' + s.pathPoints.length + ' closed=' + s.closed; } } catch (e3) {}
        try { if (s.typename === 'CompoundPathItem') { line += ' subpaths=' + s.pathItems.length; } } catch (e4) {}
        say(line);
    }

    // ---- P2: can a path be built from points and removed again? ------------------------------
    say('');
    say('P2 building a path from a point array');
    var made = null;
    try {
        made = doc.layers[0].pathItems.add();
        made.setEntirePath([[0, 0], [12, 0], [12, 12]]);
        made.closed = true;
        made.filled = true;
        made.stroked = false;
        say('  setEntirePath ok: points=' + made.pathPoints.length + ' closed=' + made.closed + ' filled=' + made.filled);
    } catch (e5) {
        say('  FAILED: ' + e5.message);
    }

    // ---- P3: what can resolve overlapping polygons? -------------------------------------------
    say('');
    say('P3 resolving a self-intersecting region');
    try {
        var fn = app.executeMenuCommand;
        say('  app.executeMenuCommand present: ' + (fn ? 'yes' : 'no'));
    } catch (e6) {
        say('  app.executeMenuCommand absent: ' + e6.message);
    }
    try {
        var n = doc.compoundPathItems.length;
        say('  doc.compoundPathItems reachable, count=' + n);
    } catch (e7) {
        say('  doc.compoundPathItems unreachable: ' + e7.message);
    }
    try {
        var grp = doc.groupItems.length;
        say('  doc.groupItems reachable, count=' + grp);
    } catch (e8) {
        say('  doc.groupItems unreachable: ' + e8.message);
    }

    // Leave the document exactly as it was found.
    try { if (made) { made.remove(); say(''); say('temporary path removed'); } } catch (e9) { say('could not remove the temporary path: ' + e9.message); }
    return out.join('\n');
})();
