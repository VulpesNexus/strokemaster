// ai-align-survey.jsx - which Illustrator targets take which stroke alignment (2026-10-16), for the
// census's open item AI-03. One target per cell, Black fill, 60% gray 8 pt miter stroke; each is
// selected alone and sent align-survey "inside", then "outside", then "center" (AIPaintStyleSuite::
// SetStrokeAlignment, the call behind the Stroke panel's Align Stroke buttons), each followed by the
// alignment read back from its style. The page is exported with every target Center, Inside, and
// Outside (S-<align>.pdf), for tools/measure_align_survey.py. Needs StrokeMasterRegionProbe
// region-prep-17 and $.global.ISE_REGION_ARGS outDir and runId. Alerts suppressed and restored.
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
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    var doc = null, at = 'begin';
    function select(items) { doc.selection = null; for (var i = 0; i < items.length; i++) items[i].selected = true; app.redraw(); }
    function paint(p) {
        p.filled = true; p.fillColor = gray(100);
        p.stroked = true; p.strokeColor = gray(60); p.strokeWidth = 8; p.strokeJoin = StrokeJoin.MITERENDJOIN;
    }
    function pdf(name) {
        var f = new File(args.outDir + '/' + args.runId + '-' + name + '.pdf');
        if (f.exists) throw 'refusing to overwrite ' + f.name;
        var o = new PDFSaveOptions(); o.acrobatLayers = false; o.preserveEditability = false;
        doc.saveAs(f, o);
    }
    try {
        doc = newDocument(DocumentColorSpace.RGB, 640, 480);
        doc.artboards[0].artboardRect = [0, 480, 640, 0];
        var cells = [];
        function cell(i) { return [20 + (i % 5) * 125, 460 - Math.floor(i / 5) * 115]; }   // left, top
        function rect(x, y, w, h) { var r = doc.pathItems.rectangle(y, x, w, h); paint(r); return r; }
        function add(name, make) {
            at = 'make ' + name;
            var c = cell(cells.length), item = null;
            try { item = make(c[0], c[1]); } catch (e) { note('make ' + name, 'error ' + e); }
            cells.push({ name: name, item: item });
        }
        add('closed path', function (x, y) { return rect(x + 10, y - 10, 70, 70); });
        add('open path', function (x, y) {
            var p = doc.pathItems.add(); p.setEntirePath([[x + 10, y - 80], [x + 45, y - 10], [x + 80, y - 80]]); paint(p); return p;
        });
        add('compound path, closed contours', function (x, y) {
            var cp = doc.compoundPathItems.add();
            rect(x + 5, y - 10, 40, 40).move(cp, ElementPlacement.PLACEATEND);
            rect(x + 55, y - 40, 40, 40).move(cp, ElementPlacement.PLACEATEND);
            paint(cp.pathItems[0]); return cp;
        });
        add('compound path, an open contour', function (x, y) {
            var cp = doc.compoundPathItems.add();
            rect(x + 5, y - 10, 40, 40).move(cp, ElementPlacement.PLACEATEND);
            var o = doc.pathItems.add(); o.setEntirePath([[x + 55, y - 80], [x + 75, y - 30], [x + 95, y - 80]]);
            o.move(cp, ElementPlacement.PLACEATEND);
            paint(cp.pathItems[0]); return cp;
        });
        add('compound path, outlined text', function (x, y) {
            var t = doc.textFrames.add(); t.contents = 'O'; t.position = [x + 15, y - 5];
            t.textRange.characterAttributes.size = 72;
            var g = t.createOutline(), cp = g.compoundPathItems[0];
            cp.move(doc, ElementPlacement.PLACEATEND); g.remove();
            paint(cp.pathItems[0]); return cp;
        });
        add('group of stroked paths', function (x, y) {
            var g = doc.groupItems.add();
            rect(x + 5, y - 10, 40, 40).move(g, ElementPlacement.PLACEATEND);
            rect(x + 55, y - 40, 40, 40).move(g, ElementPlacement.PLACEATEND);
            return g;
        });
        add('group with its own stroke', function (x, y) {
            var g = doc.groupItems.add();
            var a = rect(x + 5, y - 10, 40, 40), b = rect(x + 55, y - 40, 40, 40);
            a.stroked = false; b.stroked = false;
            a.move(g, ElementPlacement.PLACEATEND); b.move(g, ElementPlacement.PLACEATEND);
            select([g]); note('group stroke', send('align-survey', 'stroke'));
            return g;
        });
        function type(x, y, contents) {
            var t = doc.textFrames.add(); t.contents = contents; t.position = [x + 10, y - 5];
            var ca = t.textRange.characterAttributes; ca.size = 60; ca.fillColor = gray(100); ca.strokeColor = gray(60); ca.strokeWeight = 8;
            return t;
        }
        add('point text, character strokes', function (x, y) { return type(x, y, 'Ax'); });
        add('point text with its own stroke', function (x, y) {
            var t = type(x, y, 'Ax'); t.textRange.characterAttributes.strokeColor = new NoColor();
            select([t]); note('text stroke', send('align-survey', 'stroke'));
            return t;
        });
        add('area text, character strokes', function (x, y) {
            var box = doc.pathItems.rectangle(y - 5, x + 5, 110, 90);
            var t = doc.textFrames.areaText(box); t.contents = 'Ax';
            var ca = t.textRange.characterAttributes; ca.size = 60; ca.fillColor = gray(100); ca.strokeColor = gray(60); ca.strokeWeight = 8;
            return t;
        });
        add('live paint group', function (x, y) {
            var a = rect(x + 5, y - 10, 50, 50), b = rect(x + 35, y - 40, 50, 50);
            select([a, b]); app.executeMenuCommand('Make Planet X');
            return doc.selection.length ? doc.selection[0] : null;
        });
        add('compound shape', function (x, y) {
            var a = rect(x + 5, y - 10, 50, 50), b = rect(x + 35, y - 40, 50, 50);
            var g = doc.groupItems.add(); a.move(g, ElementPlacement.PLACEATEND); b.move(g, ElementPlacement.PLACEATEND);
            select([g]);
            var made = 'none';
            var names = ['Make Compound Shape', 'makeCompoundShape', 'Live Pathfinder Make Compound Shape'];
            for (var n = 0; n < names.length && made === 'none'; n++) { try { app.executeMenuCommand(names[n]); made = names[n]; } catch (e) {} }
            note('compound shape command', made);
            return doc.selection.length ? doc.selection[0] : g;
        });
        add('path with a brush', function (x, y) {
            var p = rect(x + 10, y - 10, 70, 70);
            note('brushes', doc.brushes.length);
            if (doc.brushes.length) doc.brushes[0].applyTo(p);
            return p;
        });
        add('clipping group', function (x, y) {
            var art = rect(x + 5, y - 5, 90, 90), clip = rect(x + 20, y - 20, 60, 60);
            var g = doc.groupItems.add(); art.move(g, ElementPlacement.PLACEATEND); clip.move(g, ElementPlacement.PLACEATBEGINNING);
            g.clipped = true; return g;
        });
        add('ellipse', function (x, y) { var e = doc.pathItems.ellipse(y - 10, x + 10, 70, 70); paint(e); return e; });
        add('type on a path', function (x, y) {
            var h = doc.pathItems.add(); h.setEntirePath([[x + 5, y - 70], [x + 110, y - 70]]);
            var t = doc.textFrames.pathText(h); t.contents = 'Ax';
            var ca = t.textRange.characterAttributes; ca.size = 50; ca.fillColor = gray(100); ca.strokeColor = gray(60); ca.strokeWeight = 8;
            return t;
        });
        app.redraw();
        // Each target alone: Inside, read; Outside, read; Center, read.
        var steps = ['?', 'inside', 'outside', 'center'];
        for (var i = 0; i < cells.length; i++) {
            if (!cells[i].item) continue;
            at = 'survey ' + cells[i].name;
            for (var k = 0; k < steps.length; k++) {
                select([cells[i].item]);
                note(cells[i].name + ' ' + steps[k], send('align-survey', steps[k]));
            }
        }
        // Every target Center, then Inside, then Outside, for the drawings
        // (tools/measure_align_survey.py compares each cell across the three).
        var passes = ['center', 'inside', 'outside'];
        for (var pass = 0; pass < passes.length; pass++) {
            at = 'pass ' + passes[pass];
            for (var j = 0; j < cells.length; j++) {
                if (!cells[j].item) continue;
                select([cells[j].item]);
                send('align-survey', passes[pass]);
            }
            doc.selection = null;
            app.redraw();
            pdf('S-' + passes[pass]);
        }
        var names = [];
        for (var q = 0; q < cells.length; q++) names.push(q + ':' + cells[q].name);
        note('cells', names.join(', '));
        return 'DONE|' + out.join('|');
    } catch (error) {
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + ' line ' + error.line + '|' + out.join('|');
    }
    }
})();
