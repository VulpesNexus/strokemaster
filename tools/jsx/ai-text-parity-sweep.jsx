// Synthetic text parity sweep. Own documents only; never activate the host.
// ISE_REGION_ARGS: outDir, runId, font. Refuses preexisting documents.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId || !args.font) return 'FAIL|missing arguments';
    if (app.documents.length) return 'FAIL|requires zero preexisting documents';
    var interaction = app.userInteractionLevel, doc = null, rows = [], at = 'start';
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    var lib = $.evalFile(new File(new File($.fileName).parent.fsName + "/ai-text-parity-lib.jsx"));
    function json(x) { return lib.json(x); }
    function file(suffix) {
        var f = new File(args.outDir + '/' + args.runId + '-' + suffix);
        if (f.exists) throw Error('refusing existing ' + f.name);
        return f;
    }
    function writeResult() {
        var f = file(args.deferredReopen ? 'phase1-results.json' : 'results.json'); f.encoding = 'UTF-8';
        if (!f.open('w')) throw Error('cannot open results');
        f.write(json({schema:1, host:app.version, font:args.font,
            build:args.expectedBuild || 'region-prep-18', control:!!args.control, flow:!!args.flow,
            routeOnly:!!args.routeOnly, fixOnly:!!args.fixOnly, storyGroup:!!args.storyGroup, storyDriver:!!args.storyDriver, deferredReopen:!!args.deferredReopen, rows:rows})); f.close();
    }
    function send(s) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, '')); }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    function snapshot() { return lib.snapshot(doc); }
    function pdf(stem) {
        var o = new PDFSaveOptions(); o.preserveEditability = false;
        o.acrobatLayers = false; o.viewAfterSaving = false;
        app.redraw(); doc.saveAs(file(stem + '.pdf'), o);
    }
    function make(kind, font) {
        var p = new DocumentPreset(); p.colorMode = DocumentColorSpace.RGB;
        p.width = 480; p.height = 360; p.units = RulerUnits.Points;
        doc = app.documents.addDocument('Web', p);
        doc.artboards[0].artboardRect = [0,360,480,0];
        var t, boundary, second;
        if (kind === 'area' || kind === 'threaded') {
            // Great Vibes needs the wider first frame measured in the original
            // fixture: an entire word must fit before Illustrator places it there.
            var wideFlow = args.flow && args.font === 'GreatVibes-Regular';
            boundary = doc.pathItems.rectangle(310,35,kind === 'threaded' && args.flow ? (wideFlow ? 400 : 185) : 400,
                kind === 'threaded' ? (args.flow ? (wideFlow ? 62 : 48) : 62) : 240);
            t = doc.textFrames.areaText(boundary);
            if (kind === 'threaded') {
                second = doc.textFrames.areaText(doc.pathItems.rectangle(args.flow ? 285 : 265,45,400,180));
                t.nextFrame = second;
            }
        } else if (kind === 'path') {
            boundary = doc.pathItems.add(); boundary.setEntirePath([[30,240],[170,285],[320,245],[440,270]]);
            boundary.filled = false; boundary.stroked = false;
            t = doc.textFrames.pathText(boundary);
        } else {
            t = doc.textFrames.add(); t.position = [35,275];
        }
        t.contents = kind === 'path' ? 'AVMWxy AVMWxy' : 'AVMWxy AVMWxy AVMWxy AVMWxy';
        if (kind !== 'path' && kind !== 'threaded') t.contents = 'AVMWxy AVMWxy\rAVMWxy AVMWxy';
        var a = t.story.textRange.characterAttributes;
        a.textFont = font; a.size = 38; a.leading = 28; a.autoLeading = false;
        a.fillColor = gray(100); a.strokeColor = new NoColor();
        // A local size/tracking override must survive native-route and native-fix.
        var ca = t.story.textRange.characters[1].characterAttributes;
        ca.size = 42; ca.tracking = 25;
        if (kind === 'rotated') t.rotate(17);
        if (kind === 'scaled') t.resize(112,76);
        app.redraw(); return t;
    }
    function strokes(t) {
        var chars = t.story.textRange.characters;
        for (var j = 0; j < chars.length; j++) {
            if (/[ \r\n]/.test(chars[j].contents) || j % 3 === 0) continue;
            chars[j].characterAttributes.strokeColor = gray(j % 2 ? 40 : 70);
            chars[j].characterAttributes.strokeWeight = 7;
        }
    }
    try {
        var font = app.textFonts.getByName(args.font), kinds = args.kinds || ['point','area','path','rotated','scaled','threaded'];
        if (args.deferredReopen && kinds.length !== 1) throw Error('deferred reopen requires one kind');
        var status = send('status');
        if (status.indexOf('"build":"' + (args.expectedBuild || 'region-prep-18') + '"') < 0) throw Error('unexpected plugin build');
        for (var k = 0; k < kinds.length; k++) {
            var kind = kinds[k], row = {kind:kind}, start = new Date().getTime();
            rows.push(row); at = kind + '.create';
            var t = make(kind, font);
            row.before = snapshot(); pdf(kind + '-fill'); row.afterFillExport = snapshot();
            row.fillExportMs = new Date().getTime() - start;
            var tick = new Date().getTime();
            strokes(t); app.redraw(); pdf(kind + '-native'); row.beforeRoute = snapshot();
            row.nativeExportMs = new Date().getTime() - tick;
            doc.selection = null;
            for (var q = 0; q < doc.textFrames.length; q++) doc.textFrames[q].selected = true;
            at = kind + '.route'; tick = new Date().getTime();
            if (args.storyGroup) {
                var members = [], group = null;
                for (var qg = 0; qg < doc.textFrames.length; qg++) members.push(doc.textFrames[qg]);
                if (args.storyDriver) row.route = String($.evalFile(new File(new File($.fileName).parent.fsName + "/strokemaster-fix-story.jsx")));
                else {
                    group = doc.groupItems.add();
                    for (var mg = 0; mg < members.length; mg++) members[mg].move(group, ElementPlacement.PLACEATEND);
                    doc.selection = null; group.selected = true;
                    row.route = send('story-fill');
                }
                doc.selection = null;
                for (var sf = 0; sf < members.length; sf++) members[sf].selected = true;
            } else if (!args.control && !args.fixOnly) row.route = send('native-route');
            row.afterRoute = snapshot();
            if (!args.control && !args.routeOnly) row.fix = send('native-fix');
            doc.selection = null; app.redraw();
            row.after = snapshot(); row.stylesPreserved = json(row.beforeRoute) === json(row.after);
            row.correctionMs = new Date().getTime() - tick;
            pdf(kind + '-route');
            var ai = file(kind + '.ai'), so = new IllustratorSaveOptions(); so.pdfCompatible = true;
            doc.saveAs(ai, so); doc.close(SaveOptions.DONOTSAVECHANGES); doc = null;
            if (args.deferredReopen) { row.ms = new Date().getTime() - start; continue; }
            at = kind + '.reopen'; doc = app.open(ai); app.redraw();
            row.reopened = snapshot(); row.reopenPreserved = json(row.after) === json(row.reopened);
            pdf(kind + '-reopen');
            // Exercise live text after reopening. No outlining or expanding.
            var editFrame = 0;
            while (editFrame < doc.textFrames.length && !doc.textFrames[editFrame].story.textRange.characters.length) editFrame++;
            var range = doc.textFrames[editFrame].story.textRange, old = range.contents;
            range.characters[range.characters.length - 1].contents += ' AV';
            app.redraw();
            row.editable = range.contents === old + ' AV' && doc.textFrames.length === row.after.frames.length;
            row.ms = new Date().getTime() - start;
            doc.close(SaveOptions.DONOTSAVECHANGES); doc = null;
        }
        writeResult(); return 'DONE|font=' + args.font + '|cases=' + rows.length;
    } catch (e) {
        if (doc) try { doc.close(SaveOptions.DONOTSAVECHANGES); } catch (ignored) {}
        rows.push({error:String(e), stage:at});
        try { writeResult(); } catch (writeError) {}
        return 'FAIL|stage=' + at + '|error=' + String(e);
    } finally { app.userInteractionLevel = interaction; }
})();
