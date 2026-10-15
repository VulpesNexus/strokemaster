// id-align-survey.jsx - which InDesign 2026 targets take which stroke alignment (2026-10-16), the
// InDesign half of the census's AI-03. Runs in the running InDesign through tools/run-id-probe.ps1,
// in one document of its own made without a window (documents.add(false)), closed unsaved at the
// end; every other open document is checked to be left as it was (id, saved, modified, item count),
// and the active document is put back. One target per cell (five per row, 125 x 115 pt, from the
// page's top left at 20, 20), Black fill, a 60% Black 8 pt miter stroke. Each target is set to
// Center, Inside, and Outside (the property the Stroke panel's Align Stroke sets), with the error
// and the value read back, and the page is exported as <stem>-S-<align>.pdf in each state, for
// tools/measure_align_survey.py. ISE_PROBE_ARGS: outDir, stem. Returns {"raw": "DONE|..."}.
(function () {
    function survey() {
    var args = $.global.ISE_PROBE_ARGS || {};
    if (!args.outDir || !args.stem) return 'FAIL|missing outDir or stem';
    var oldUI = app.scriptPreferences.userInteractionLevel, out = [], doc = null, at = 'begin';
    function note(k, v) { out.push(k + '=' + String(v).replace(/[|\r\n]/g, ' ')); }
    function state(d) { return [d.id, d.saved, d.modified, d.allPageItems.length].join('/'); }
    var others = [], active = null;
    for (var n = 0; n < app.documents.length; n++) others.push({ doc: app.documents[n], state: state(app.documents[n]) });
    try { active = app.activeDocument; } catch (e) {}
    try {
        app.scriptPreferences.userInteractionLevel = UserInteractionLevels.NEVER_INTERACT;
        doc = app.documents.add(false);
        var vp = doc.viewPreferences;
        vp.horizontalMeasurementUnits = MeasurementUnits.POINTS; vp.verticalMeasurementUnits = MeasurementUnits.POINTS;
        vp.rulerOrigin = RulerOrigin.PAGE_ORIGIN;
        doc.documentPreferences.pageWidth = 640; doc.documentPreferences.pageHeight = 480;
        doc.documentPreferences.facingPages = false;
        var page = doc.pages[0];
        var black = doc.swatches.itemByName('Black');
        var grey = doc.colors.add({ model: ColorModel.PROCESS, space: ColorSpace.CMYK, colorValue: [0, 0, 0, 60], name: 'ISE survey 60' });
        function paint(it) {
            it.fillColor = black; it.strokeColor = grey; it.strokeWeight = 8;
            it.endJoin = EndJoin.MITER_END_JOIN; it.miterLimit = 4;
        }
        var cells = [];
        function cell(i) { return [20 + Math.floor(i / 5) * 115, 20 + (i % 5) * 125]; }   // top, left
        function rect(t, l, w, h) { var r = page.rectangles.add({ geometricBounds: [t, l, t + h, l + w] }); paint(r); return r; }
        function add(name, make, text) {
            at = 'make ' + name;
            var c = cell(cells.length), item = null;
            try { item = make(c[0], c[1]); } catch (e) { note('make ' + name, 'error ' + e); }
            cells.push({ name: name, item: item, text: !!text });
        }
        add('closed path', function (t, l) { return rect(t + 10, l + 10, 70, 70); });
        add('open path', function (t, l) {
            var g = page.graphicLines.add();
            g.paths[0].entirePath = [[l + 10, t + 80], [l + 45, t + 10], [l + 80, t + 80]];
            paint(g); return g;
        });
        add('compound path, closed contours', function (t, l) {
            var a = rect(t + 10, l + 5, 40, 40), b = rect(t + 40, l + 55, 40, 40);
            return a.makeCompoundPath([b]);
        });
        add('compound path, an open contour', function (t, l) {
            var a = rect(t + 10, l + 5, 40, 40), b = rect(t + 30, l + 55, 40, 50);
            var cp = a.makeCompoundPath([b]);
            cp.paths[1].entirePath = [[l + 55, t + 80], [l + 75, t + 30], [l + 95, t + 80]];
            cp.paths[1].pathType = PathType.OPEN_PATH;
            return cp;
        });
        add('compound path, overlapping squares', function (t, l) {
            var a = rect(t + 10, l + 5, 50, 50), b = rect(t + 35, l + 35, 50, 50);
            return a.makeCompoundPath([b]);
        });
        add('ellipse', function (t, l) { var o = page.ovals.add({ geometricBounds: [t + 10, l + 10, t + 80, l + 80] }); paint(o); return o; });
        function frame(t, l, contents) {
            var f = page.textFrames.add({ geometricBounds: [t, l, t + 100, l + 120] });
            f.fillColor = doc.swatches.itemByName('None'); f.strokeColor = doc.swatches.itemByName('None');
            f.contents = contents;
            var x = f.parentStory.texts[0];
            try { x.appliedFont = 'Minion Pro\tRegular'; } catch (e) {}
            x.pointSize = 60; x.fillColor = black; x.strokeColor = grey; x.strokeWeight = 8; x.miterLimit = 4;
            return f;
        }
        add('text, character strokes', function (t, l) { return frame(t, l, 'Ax'); }, true);
        add('text frame, its own stroke', function (t, l) {
            var f = page.textFrames.add({ geometricBounds: [t + 10, l + 10, t + 80, l + 80] });
            paint(f); return f;
        });
        add('outlined text', function (t, l) {
            var f = frame(t, l, 'O');
            var outs = f.createOutlines(true);
            return outs[0];
        });
        add('group of stroked paths', function (t, l) {
            var a = rect(t + 10, l + 5, 40, 40), b = rect(t + 40, l + 55, 40, 40);
            return page.groups.add([a, b]);
        });
        var kinds = [['center', StrokeAlignment.CENTER_ALIGNMENT], ['inside', StrokeAlignment.INSIDE_ALIGNMENT],
                     ['outside', StrokeAlignment.OUTSIDE_ALIGNMENT]];
        var textKinds = { center: 'CENTER_ALIGNMENT', inside: 'INSIDE_ALIGNMENT', outside: 'OUTSIDE_ALIGNMENT' };
        // The enum's values by reflection: reading a value it lacks raised an error try did not catch.
        var textValues = {}, listed = [];
        var props = TextStrokeAlign.reflect.properties;
        for (var pi = 0; pi < props.length; pi++) { textValues[props[pi].name] = true; listed.push(props[pi].name); }
        function textEnum(name) { return textValues[name] ? TextStrokeAlign[name] : undefined; }
        note('TextStrokeAlign values', listed.join(','));
        for (var s = 0; s < kinds.length; s++) {
            for (var i = 0; i < cells.length; i++) {
                var c = cells[i];
                if (!c.item) continue;
                at = c.name + ' ' + kinds[s][0];
                var result;
                try {
                    if (c.text) {
                        var x = c.item.parentStory.texts[0], v = textEnum(textKinds[kinds[s][0]]);
                        if (v === undefined) throw 'TextStrokeAlign has no ' + textKinds[kinds[s][0]];
                        x.strokeAlignment = v;
                        result = 'ok, reads ' + x.strokeAlignment;
                    } else {
                        c.item.strokeAlignment = kinds[s][1];
                        result = 'ok, reads ' + c.item.strokeAlignment;
                    }
                } catch (e) { result = 'refused: ' + e; }
                note(c.name + ' ' + kinds[s][0], result);
            }
            at = 'export ' + kinds[s][0];
            var f = new File(args.outDir + '/' + args.stem + '-S-' + kinds[s][0] + '.pdf');
            if (f.exists) throw 'refusing to overwrite ' + f.name;
            doc.exportFile(ExportFormat.PDF_TYPE, f, false);
        }
        var names = [];
        for (var q = 0; q < cells.length; q++) names.push(q + ':' + cells[q].name);
        note('cells', names.join(', '));
        return 'DONE|' + out.join('|');
    } catch (error) {
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + ' line ' + error.line + '|' + out.join('|');
    } finally {
        try { if (doc && doc.isValid) doc.close(SaveOptions.NO); } catch (e) {}
        try { if (active && active.isValid) app.activeDocument = active; } catch (e) {}
        app.scriptPreferences.userInteractionLevel = oldUI;
        var changed = [];
        for (var m = 0; m < others.length; m++) {
            try { if (state(others[m].doc) !== others[m].state) changed.push(others[m].state + ' -> ' + state(others[m].doc)); }
            catch (e) { changed.push('unreadable ' + others[m].state); }
        }
        othersNote = changed.length ? changed.join('; ') : 'unchanged';
    }
    }
    var othersNote = 'not checked';
    // tools/run-id-probe.ps1 reads JSON: the result goes in one string field.
    var raw = survey() + '|others=' + othersNote;
    return '{"passed":' + (raw.indexOf('DONE|') === 0) + ',"raw":"' + raw.replace(/\\/g, '\\\\').replace(/"/g, '\\"') + '"}';
})();
