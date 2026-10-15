// ai-gap-research.jsx - research probe for the native route's remaining gaps (2026-10-16), with
// StrokeMasterRegionProbe region-prep-16 loaded. Each part exports PDFs for
// tools/measure_gap_research.py:
//   T  text with mixed fills and mixed strokes ("HH" / "HH", 48 pt, leading 34, 8 pt strokes):
//      T-fill (strokes removed), T-native, T-ff (native-route: ATE FillFirst off), then
//      saved, reopened (T-reopen) and edited (T-edited, "H" appended to line 2).
//   I  Inside 8 on two overlapping squares in one compound path: I-fill, I-native, Illustrator's own
//      Effect > Pathfinder > Add on the compound path from its menu (I-pf), native-fix (I-fix), and
//      the menu command on a group holding it (I-gpf).
//   D  dashes: the 10-degree thorn, Center 4, dashed 12/4 (D-native, D-align: native-fix, which sets
//      Illustrator's Align Dashes to Corners); a straight line with and without a collinear middle anchor
//      (L-plain-align, L-mid-align), which tells whether a smooth anchor also anchors dashes.
// One document throughout, part T last (it saves and reopens): closing a document and making a
// new one crashed Illustrator 30.8.2 (2026-10-16, memory illustrator-doc-churn-crash).
// Needs $.global.ISE_REGION_ARGS outDir and runId. Alerts suppressed and restored.
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
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId) return 'FAIL|missing outDir or runId';
    var plugin = 'StrokeMasterRegionProbe';
    var out = [];
    function note(k, v) { out.push(k + '=' + String(v).replace(/[|\r\n]/g, ' ')); }
    function send(selector, input) { return String(app.sendScriptMessage(plugin, selector, input || '')); }
    function block(json, name) { var m = new RegExp('"' + name + '":(\\{[^}]*\\})').exec(json); return m ? m[1] : '?'; }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    var doc = null, at = 'begin';
    function clear() { while (doc.pageItems.length > 0) doc.pageItems[0].remove(); }
    function select(items) {
        doc.selection = null;
        for (var i = 0; i < items.length; i++) items[i].selected = true;
        app.redraw();
    }
    function pdf(name) {
        var f = new File(args.outDir + '/' + args.runId + '-' + name + '.pdf');
        if (f.exists) throw 'refusing to overwrite ' + f.name;
        var options = new PDFSaveOptions();
        options.acrobatLayers = false;
        doc.saveAs(f, options);
        note(name + '.pdf', f.length);
    }
    function textFrame(stroked) {
        clear();
        var t = doc.textFrames.add();
        t.contents = 'HH\rHH';
        t.position = [20, 180];
        var all = t.textRange.characterAttributes;
        all.size = 48; all.leading = 34; all.autoLeading = false;
        all.fillColor = gray(100);
        all.strokeColor = new NoColor();
        var chars = t.textRange.characters;
        chars[1].characterAttributes.fillColor = gray(85);
        chars[3].characterAttributes.fillColor = gray(85);
        if (!stroked) return t;
        chars[1].characterAttributes.strokeColor = gray(40);
        chars[1].characterAttributes.strokeWeight = 8;
        for (var i = 3; i < 5; ++i) {
            chars[i].characterAttributes.strokeColor = gray(70);
            chars[i].characterAttributes.strokeWeight = 8;
        }
        return t;
    }
    function rect(top, left, w, h) { return doc.pathItems.rectangle(top, left, w, h); }
    function squares(stroked) {
        clear();
        var cp = doc.compoundPathItems.add();
        var a = rect(150, 40, 80, 80); a.move(cp, ElementPlacement.PLACEATEND);
        var b = rect(110, 80, 80, 80); b.move(cp, ElementPlacement.PLACEATEND);
        for (var i = 0; i < cp.pathItems.length; i++) {
            var p = cp.pathItems[i];
            p.filled = true; p.fillColor = gray(20);
            p.stroked = stroked; if (stroked) { p.strokeColor = gray(100); p.strokeWidth = 8; p.strokeJoin = StrokeJoin.MITERENDJOIN; }
        }
        if (stroked) { select([cp]); note('I.align', block(send('stroke-align', 'inside'), 'appearance')); }
        return cp;
    }
    try {
        note('build', /"build":"([^"]*)"/.exec(send('status'))[1]);
        doc = newDocument(DocumentColorSpace.RGB, 200, 200);
        doc.artboards[0].artboardRect = [0, 200, 200, 0];

        // I: Inside on overlapping contours.
        at = 'I.fill'; squares(false); app.redraw(); pdf('I-fill');
        at = 'I.native'; squares(true); app.redraw(); pdf('I-native');
        at = 'I.pf'; var cp = squares(true);
        select([cp]);
        try { app.executeMenuCommand('Live Pathfinder Add'); note('I.pf.menu', 'ok'); } catch (e) { note('I.pf.menu', e); }
        app.redraw();
        note('I.pf.type', doc.pageItems[0].typename + ' ' + doc.pageItems.length);
        pdf('I-pf');
        at = 'I.fix'; cp = squares(true);
        select([cp]);
        note('I.fix', block(send('native-fix'), 'nativeFix'));
        app.redraw(); pdf('I-fix');
        at = 'I.gpf'; cp = squares(true);
        var g = doc.groupItems.add(); cp.move(g, ElementPlacement.PLACEATEND);
        select([g]);
        try { app.executeMenuCommand('Live Pathfinder Add'); note('I.gpf.menu', 'ok'); } catch (e) { note('I.gpf.menu', e); }
        app.redraw();
        note('I.gpf.type', doc.pageItems[0].typename + ' ' + doc.pageItems.length);
        pdf('I-gpf');

        // D: dashes.
        function thorn() {
            clear();
            var half = 5 * Math.PI / 180, side = 60, bw = side * Math.sin(half), by = side * Math.cos(half);
            var p = doc.pathItems.add();
            var x0 = 100, y0 = 190;
            p.setEntirePath([[x0, y0], [x0 + bw, y0 - by], [x0 + 45, y0 - by], [x0 + 45, y0 - by - 60], [x0 - 45, y0 - by - 60], [x0 - 45, y0 - by], [x0 - bw, y0 - by]]);
            p.closed = true;
            p.filled = true; p.fillColor = gray(20);
            p.stroked = true; p.strokeColor = gray(100); p.strokeWidth = 4; p.strokeJoin = StrokeJoin.MITERENDJOIN;
            p.strokeMiterLimit = 20; p.strokeDashes = [12, 4];
            return p;
        }
        at = 'D.native'; thorn(); app.redraw(); pdf('D-native');
        at = 'D.align'; var th = thorn(); select([th]);
        note('D.align', block(send('native-fix'), 'nativeFix'));
        app.redraw(); pdf('D-align');
        note('D.align.read', block(send('dash-align', '?'), 'appearance'));
        function line(mid) {
            clear();
            var p = doc.pathItems.add();
            p.setEntirePath(mid ? [[10, 100], [93, 100], [190, 100]] : [[10, 100], [190, 100]]);
            p.filled = false; p.stroked = true; p.strokeColor = gray(100); p.strokeWidth = 6;
            p.strokeDashes = [20, 10];
            return p;
        }
        at = 'L.plain'; var l = line(false); app.redraw(); pdf('L-plain');
        select([l]); send('dash-align', 'on'); app.redraw(); pdf('L-plain-align');
        at = 'L.mid'; l = line(true); select([l]); note('L.mid', block(send('dash-align', 'on'), 'appearance')); app.redraw(); pdf('L-mid-align');
        // T: FillFirst.
        at = 'T.fill'; textFrame(false); app.redraw(); pdf('T-fill');
        at = 'T.native'; var t = textFrame(true); app.redraw(); pdf('T-native');
        at = 'T.ff';
        select([t]);
        note('T.ff.before', block(send('appearance', 'fill-first:?'), 'appearance'));
        note('T.ff.set', block(send('native-route'), 'appearance'));
        note('T.ff.after', block(send('appearance', 'fill-first:?'), 'appearance'));
        app.redraw(); pdf('T-ff');
        at = 'T.save';
        var ai = new File(args.outDir + '/' + args.runId + '-T.ai');
        if (ai.exists) throw 'refusing to overwrite ' + ai.name;
        var so = new IllustratorSaveOptions(); so.pdfCompatible = true;
        doc.saveAs(ai, so);
        doc.close(SaveOptions.DONOTSAVECHANGES);
        doc = app.open(ai);
        at = 'T.reopen';
        select([doc.textFrames[0]]);
        note('T.reopen.ff', block(send('appearance', 'fill-first:?'), 'appearance'));
        app.redraw(); pdf('T-reopen');
        at = 'T.edit';
        doc.textFrames[0].textRange.characters[4].contents = 'HH';
        note('T.edited', doc.textFrames[0].contents.replace(/\r/g, '/'));
        select([doc.textFrames[0]]);
        note('T.edited.ff', block(send('appearance', 'fill-first:?'), 'appearance'));
        app.redraw(); pdf('T-edited');
        doc = null;   // left open: the runner closes it as Illustrator quits
        return 'DONE|' + out.join('|');
    } catch (error) {
        if (doc) { try { doc.close(SaveOptions.DONOTSAVECHANGES); } catch (ignored) {} }
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + ' line ' + error.line + '|' + out.join('|');
    }
    }
})();
