// ai-cert-roundtrip.jsx - route W PDFs through Illustrator: open, inspect, re-save with and without
// Illustrator editing capabilities, reopen the editable copy, and save it once more.
// Run with tools/run-ai-jsx.ps1. Arguments ($.global.ISE_AI_ARGS): inputs (PDF paths), out (a
// fresh folder). Writes <out>/ai-results.json and returns a one-line summary.
//
// Illustrator opens a PDF by converting its content into Illustrator objects; that conversion,
// not InDesign's export, decides what survives. For each input the script records what the
// conversion made (text frames and their contents and fonts, paths, stroked paths, raster and
// placed items, groups, and clipping groups) and then writes:
//   <stem>-edit-on.pdf    PDFSaveOptions.preserveEditability = true (Illustrator data embedded)
//   <stem>-edit-off.pdf   preserveEditability = false
//   <stem>-reopen.pdf     <stem>-edit-on.pdf reopened (Illustrator reads its own data), saved
//                         again with editability off
// Alerts are suppressed and the previous interaction level restored; every document opened
// here is closed without saving.
(function () {
    var args = $.global.ISE_AI_ARGS || {}, results = [], interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    function toJSON(v) {
        if (v === null || v === undefined) { return 'null'; }
        var t = typeof v;
        if (t === 'number') { return isFinite(v) ? String(v) : 'null'; }
        if (t === 'boolean') { return String(v); }
        if (t === 'string') {
            var r = '';
            for (var i = 0; i < v.length; i++) {
                var c = v.charCodeAt(i), ch = v.charAt(i);
                if (ch === '"') { r += '\\"'; } else if (ch === '\\') { r += '\\\\'; } else if (c < 32 || c > 126) { var h = c.toString(16); while (h.length < 4) { h = '0' + h; } r += '\\u' + h; } else { r += ch; }
            }
            return '"' + r + '"';
        }
        if (v instanceof Array) { var a = []; for (var j = 0; j < v.length; j++) { a.push(toJSON(v[j])); } return '[' + a.join(',') + ']'; }
        var parts = [];
        for (var k in v) { if (v.hasOwnProperty(k)) { parts.push(toJSON(k) + ':' + toJSON(v[k])); } }
        return '{' + parts.join(',') + '}';
    }
    function stemOf(path) { return path.replace(/^.*[\\\/]/, '').replace(/\.pdf$/i, ''); }
    function inventory(doc) {
        var inv = { textFrames: doc.textFrames.length, pathItems: doc.pathItems.length, compoundPaths: doc.compoundPathItems.length,
            rasterItems: doc.rasterItems.length, placedItems: doc.placedItems.length, groups: doc.groupItems.length, strokedPaths: 0,
            clipGroups: 0, contents: [], fonts: {}, strokedText: 0, artboard: String(doc.artboards[0].artboardRect) };
        for (var i = 0; i < doc.pathItems.length; i++) { if (doc.pathItems[i].stroked) { inv.strokedPaths++; } }
        for (var g = 0; g < doc.groupItems.length; g++) { if (doc.groupItems[g].clipped) { inv.clipGroups++; } }
        for (var t = 0; t < doc.textFrames.length; t++) {
            var tf = doc.textFrames[t];
            inv.contents.push(String(tf.contents).replace(/[\r\n]/g, '/'));
            try {
                var ca = tf.textRange.characterAttributes;
                inv.fonts[ca.textFont.name] = (inv.fonts[ca.textFont.name] || 0) + 1;
                if (ca.strokeColor && ca.strokeColor.typename !== 'NoColor') { inv.strokedText++; }
            } catch (e) {}
        }
        inv.contents = inv.contents.join('|');
        return inv;
    }
    function save(doc, path, editable) {
        var o = new PDFSaveOptions();
        o.preserveEditability = editable;
        o.acrobatLayers = false;
        o.viewAfterSaving = false;
        var f = new File(path);
        doc.saveAs(f, o);
        return f.length;
    }
    try {
        new Folder(args.out).create();
        try { results.push({ minionAvailable: !!app.textFonts.getByName('MinionPro-Regular') }); } catch (eFont) { results.push({ minionAvailable: false }); }
        for (var i = 0; i < args.inputs.length; i++) {
            var input = args.inputs[i], stem = args.stems[i], rec = { input: stemOf(input), stem: stem }, doc = null, t0 = new Date().getTime();
            try {
                doc = app.open(new File(input));
                app.redraw();
                rec.opened = inventory(doc);
                rec.editOnBytes = save(doc, args.out + '/' + stem + '-edit-on.pdf', true);
                rec.editOffBytes = save(doc, args.out + '/' + stem + '-edit-off.pdf', false);
                doc.close(SaveOptions.DONOTSAVECHANGES);
                doc = app.open(new File(args.out + '/' + stem + '-edit-on.pdf'));
                app.redraw();
                rec.reopened = inventory(doc);
                rec.reopenBytes = save(doc, args.out + '/' + stem + '-reopen.pdf', false);
                doc.close(SaveOptions.DONOTSAVECHANGES);
                doc = null;
            } catch (e) {
                rec.error = e.message + ' line ' + e.line;
                if (doc) { try { doc.close(SaveOptions.DONOTSAVECHANGES); } catch (e2) {} }
            }
            rec.ms = new Date().getTime() - t0;
            results.push(rec);
        }
    } finally {
        app.userInteractionLevel = interaction;
    }
    var f = new File(args.out + '/ai-results.json');
    f.encoding = 'UTF-8';
    f.open('w');
    f.write(toJSON(results));
    f.close();
    var errors = 0;
    for (var r = 0; r < results.length; r++) { if (results[r].error) { errors++; } }
    return 'DONE|inputs=' + args.inputs.length + '|errors=' + errors;
})();
