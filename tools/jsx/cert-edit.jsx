// cert-edit.jsx - route W under editing, undo, redo, save, and reopen, on copies only.
// Requires ise-lib.jsx and cert-lib.jsx. Arguments: ISE_ARGS.victims and ISE_ARGS.scope (the
// frozen victim census and cert-E documents), ISE_ARGS.out (a fresh folder).
//
// Route W reads InDesign's own stroke attributes at every draw and keeps no state in the
// document, so an edit must draw exactly as a document built that way would, and undoing every
// edit must draw exactly as the untouched document. Each stage exports the same pair (High
// Quality Print PDF, 600 ppi PNG) from the live document; the edited states are also saved as
// copies, from which the measurement derives their fill twins and masks.
//
//   ED1  victim census: text, run, stroke, fill, font, tracking, kerning, frame size, reflow,
//        line breaks, paragraph and character styles, overset and back
//   ED2  scope fixture: duplicate, move, rotate, scale, shear, thread and overset changes,
//        anchored and path text, a table cell
//   For both: live -> undo all -> redo all -> save -> close -> reopen, exporting at each step.
//   ED3  five cycles of edit, save, close, reopen on one copy, exporting after each.
(function () {
    var lib = $.global.ISE_LIB, cert = $.global.ISE_CERT;
    var out = lib.args.out, results = out + '/results.jsonl';
    var std = [{ kind: 'pdf', preset: '[High Quality Print]', overrides: {}, file: 'hqp.pdf' }, { kind: 'png', ppi: 600, file: 'png600.png' }];

    function stage(doc, id, extra) {
        var rec = { id: id, outputs: [], undo: history(doc) };
        cert.writeOutputs(doc, { outDir: out + '/' + id, outputs: std }, rec);
        for (var i = 0; i < rec.outputs.length; i++) {
            if (rec.outputs[i].error) { lib.L('ERROR ' + id + ' ' + rec.outputs[i].file + ': ' + rec.outputs[i].error); }
        }
        if (extra) { for (var k in extra) { if (extra.hasOwnProperty(k)) { rec[k] = extra[k]; } } }
        cert.appendLine(results, lib.toJSON(rec));
        return rec;
    }
    function history(doc) { try { return doc.undoHistory.length; } catch (e) { return -1; } }
    function frame(doc, name) {
        var f = doc.textFrames.itemByName(name);
        if (f.isValid) { return f; }
        // Anchored and grouped frames are not in doc.textFrames.
        var all = doc.allPageItems;
        for (var i = 0; i < all.length; i++) { if (all[i].name === name && all[i].constructor.name === 'TextFrame') { return all[i]; } }
        throw new Error('no frame ' + name);
    }
    function sw(doc, name) { return doc.swatches.itemByName(name); }

    function edits1(doc, log) {
        var myriad = lib.fontOk('Myriad Pro\tRegular') ? 'Myriad Pro\tRegular' : null;
        var steps = [
            ['replace text VC04', function () { frame(doc, 'VC04').parentStory.contents = 'HXH\rHH\rH'; }],
            ['insert at run boundary VC10', function () { frame(doc, 'VC10').parentStory.insertionPoints[1].contents = 'H'; }],
            ['delete character VC02', function () { frame(doc, 'VC02').parentStory.characters[1].remove(); }],
            ['stroke weight VC05', function () { frame(doc, 'VC05').parentStory.paragraphs[0].strokeWeight = 8; }],
            ['stroke color VC06', function () { frame(doc, 'VC06').parentStory.texts[0].strokeColor = sw(doc, 'Probe Red'); }],
            ['fill color VC02', function () { frame(doc, 'VC02').parentStory.texts[0].fillColor = sw(doc, 'Probe Blue'); }],
            ['font VC13', function () { if (myriad) { frame(doc, 'VC13').parentStory.texts[0].appliedFont = myriad; } }],
            ['tracking VC01', function () { frame(doc, 'VC01').parentStory.texts[0].tracking = 200; }],
            ['kerning VC00', function () { frame(doc, 'VC00').parentStory.insertionPoints[1].kerningValue = -150; }],
            ['resize and reflow VC07', function () { var f = frame(doc, 'VC07'), b = f.geometricBounds; f.geometricBounds = [b[0], b[1], b[2], b[1] + 40]; }],
            ['add line break VC19', function () { frame(doc, 'VC19').parentStory.insertionPoints[1].contents = '\r'; }],
            ['remove line break VC11', function () { frame(doc, 'VC11').parentStory.characters[2].remove(); }],
            ['paragraph style VC12', function () {
                var ps = doc.paragraphStyles.add({ name: 'Cert alt', basedOn: doc.paragraphStyles.itemByName('Victim census') });
                ps.strokeWeight = 6; ps.strokeColor = sw(doc, 'Probe Blue');
                frame(doc, 'VC12').parentStory.texts[0].applyParagraphStyle(ps, true);
            }],
            ['character style VC16', function () {
                var cs = doc.characterStyles.add({ name: 'Cert char' });
                cs.strokeColor = sw(doc, 'Probe Blue'); cs.strokeWeight = 3; cs.strokeTint = 100;
                var st = frame(doc, 'VC16').parentStory;
                doc.recompose();
                st.characters.itemByRange(0, 1).applyCharacterStyle(cs);
            }],
            ['overset VC08', function () { var f = frame(doc, 'VC08'), b = f.geometricBounds; f.geometricBounds = [b[0], b[1], b[0] + 20, b[3]]; }],
            ['back from overset VC08', function () { var f = frame(doc, 'VC08'), b = f.geometricBounds; f.geometricBounds = [b[0], b[1], b[0] + 165, b[3]]; }]
        ];
        run(doc, steps, log);
    }

    function edits2(doc, log) {
        var steps = [
            ['duplicate E01 into free row', function () { var d = frame(doc, 'E01').duplicate(); d.name = 'E01dup'; d.move([20, 605]); }],
            ['move E03', function () { frame(doc, 'E03').move(undefined, [6, -4]); }],
            ['rotate E07 by 15', function () { frame(doc, 'E07').rotationAngle = 15; }],
            ['shear E05', function () { frame(doc, 'E05').shearAngle = 12; }],
            ['scale E06 again', function () {
                frame(doc, 'E06').transform(CoordinateSpaces.PASTEBOARD_COORDINATES, AnchorPoint.TOP_LEFT_ANCHOR, app.transformationMatrices.add({ horizontalScaleFactor: 0.9, verticalScaleFactor: 1.2 }));
            }],
            ['thread: more text flows into frame 2', function () { frame(doc, 'E00a').parentStory.contents = 'HH\rHH\rHH\rHH\rHH'; }],
            ['overset to fitting E04', function () { var f = frame(doc, 'E04'), b = f.geometricBounds; f.geometricBounds = [b[0], b[1], b[0] + 165, b[3]]; }],
            ['anchored frame text E09', function () { frame(doc, 'E09anchored').parentStory.contents = 'HHH\rHH'; }],
            ['path text E10', function () { doc.ovals.itemByName('E10').textPaths[0].parentStory.contents = 'HEHEHEHE'; }],
            ['table cell E08', function () { frame(doc, 'E08').tables[0].cells[0].texts[0].strokeWeight = 6; }],
            ['paragraph style change E03', function () { frame(doc, 'E03').parentStory.texts[0].leading = 20; }]
        ];
        run(doc, steps, log);
    }

    function run(doc, steps, log) {
        for (var i = 0; i < steps.length; i++) {
            try { steps[i][1](); doc.recompose(); log.push(steps[i][0] + ' ok'); }
            catch (e) { log.push(steps[i][0] + ' FAILED ' + e.message); }
        }
    }

    function lifecycle(id, source, editFn) {
        var copy = cert.copyFile(source, out + '/work/' + id + '.indd');
        var doc = lib.track(app.open(copy, false)), log = [];
        var h0 = history(doc);
        stage(doc, id + '-original');
        editFn(doc, log);
        var h1 = history(doc);
        stage(doc, id + '-live', { log: log.join('; ') });
        doc.saveACopy(new File(out + '/' + id + '-edited.indd'));
        var undone = 0, redone = 0, guard = 0;
        while (history(doc) > h0 && guard++ < 500) { doc.undo(); undone++; }
        doc.recompose();
        stage(doc, id + '-undone', { undoSteps: undone, historyBefore: h0, historyEdited: h1 });
        guard = 0;
        while (guard++ < 500) { try { doc.redo(); redone++; } catch (e) { break; } if (history(doc) >= h1) { break; } }
        doc.recompose();
        stage(doc, id + '-redone', { redoSteps: redone });
        doc.save();
        doc.close(SaveOptions.NO);
        doc = lib.track(app.open(copy, false));
        stage(doc, id + '-reopened');
        doc.close(SaveOptions.NO);
    }

    return lib.run(function () {
        if (new File(results).exists) { lib.L('ERROR results exist: ' + results); return; }
        new Folder(out + '/work').create();
        try { lifecycle('ED1', lib.args.victims, edits1); } catch (e1) { lib.L('ED1 ERROR ' + e1.message + ' line ' + e1.line); }
        try { lifecycle('ED2', lib.args.scope, edits2); } catch (e2) { lib.L('ED2 ERROR ' + e2.message + ' line ' + e2.line); }
        try {
            var copy = cert.copyFile(lib.args.victims, out + '/work/ED3.indd');
            for (var c = 1; c <= 5; c++) {
                var doc = lib.track(app.open(copy, false));
                frame(doc, 'VC05').parentStory.paragraphs[0].strokeWeight = (c % 2) ? 8 : 3;
                doc.recompose();
                doc.save();
                doc.close(SaveOptions.NO);
                doc = lib.track(app.open(copy, false));
                stage(doc, 'ED3-cycle' + c, { vc05Weight: (c % 2) ? 8 : 3 });
                doc.close(SaveOptions.NO);
            }
        } catch (e3) { lib.L('ED3 ERROR ' + e3.message + ' line ' + e3.line); }
        lib.L('done');
    });
})();
