// Story runs (region-prep-24): artwork between a story's frames in the stacking order.
// ISE_REGION_ARGS: outDir, runId, font, expectedBuild, cases (hidden, split3, split4). Each case
// builds a story of 2 to 4 threaded area frames (two lines each) down the page, each frame
// overlapping the text of the one above, 38 pt, one red fill, one gray 7 pt stroke, plus a rectangle placed in the
// stacking order between two frames (left of the text, so it covers no letter):
//   hidden  2 frames, an unpainted rectangle between them: moved below, one story group
//   split3  3 frames, a filled rectangle between frames 2 and 3: frames 1-2 grouped, 3 alone
//   split4  4 frames, a filled rectangle between frames 2 and 3: two runs grouped
// Exports <case>-fill.pdf, <case>-native.pdf, <case>-route.pdf; the receipt records the
// script's answer, the sibling order before and after (flattened through the groups made),
// and the rectangle's stacking neighbours.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId || !args.font) return 'FAIL|missing arguments';
    if (app.documents.length) return 'FAIL|requires zero preexisting documents';
    var interaction = app.userInteractionLevel, rows = [], doc = null, at = 'start';
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    function rgb(r, g, b) { var c = new RGBColor(); c.red = r; c.green = g; c.blue = b; return c; }
    // Top-level items, top first, groups flattened, as uuids; the rectangle as "R".
    function order(rect) {
        var out = [], items = doc.layers[0].pageItems;
        for (var i = 0; i < items.length; i++) {
            var it = items[i];
            if (it.parent != doc.layers[0]) continue;
            if (it.typename === 'GroupItem') { for (var k = 0; k < it.pageItems.length; k++) out.push(it.pageItems[k].uuid === rect.uuid ? 'R' : 'T'); }
            else out.push(it.uuid === rect.uuid ? 'R' : (it.typename === 'TextFrame' ? 'T' : it.typename));
        }
        return out.join('');
    }
    try {
        if (send('status').indexOf('"build":"' + (args.expectedBuild || 'region-prep-24') + '"') < 0) throw Error('unexpected plugin build');
        var font = app.textFonts.getByName(args.font), cases = args.cases || ['hidden', 'split3', 'split4'];
        for (var c = 0; c < cases.length; c++) {
            var kind = cases[c], row = {kind: kind}, n = kind === 'hidden' ? 2 : (kind === 'split3' ? 3 : 4);
            rows.push(row); at = kind + '.create';
            var p = new DocumentPreset(); p.colorMode = DocumentColorSpace.RGB; p.width = 480; p.height = 500; p.units = RulerUnits.Points;
            doc = app.documents.addDocument('Web', p);
            doc.artboards[0].artboardRect = [0, 500, 480, 0];
            var frames = [];
            for (var f = 0; f < n; f++) {
                var t = doc.textFrames.areaText(doc.pathItems.rectangle(470 - f * 52, 60 + f * 8, 400, 64));
                if (f) frames[f - 1].nextFrame = t;
                frames.push(t);
            }
            frames[0].contents = 'AVMWxy AVMWxy AVMWxy AVMWxy AVMWxy AVMWxy AVMWxy AVMWxy AVMWxy AVMWxy AVMWxy AVMWxy AVMWxy AVMWxy AVMWxy AVMWxy';
            var a = frames[0].story.textRange.characterAttributes;
            a.textFont = font; a.size = 38; a.leading = 28; a.autoLeading = false;
            a.fillColor = rgb(200, 30, 30); a.strokeColor = new NoColor();
            // The rectangle, left of the text, stacked between frames 1 and 2 (hidden) or 2 and 3.
            var rect = doc.pathItems.rectangle(300, 5, 30, 30);
            if (kind === 'hidden') { rect.filled = false; rect.stroked = false; } else { rect.filled = true; rect.fillColor = gray(60); rect.stroked = false; }
            // A later frame is stacked above an earlier one: just above frame 1 (hidden) or frame 2
            // is between it and the next frame.
            rect.move(kind === 'hidden' ? frames[0] : frames[1], ElementPlacement.PLACEBEFORE);
            app.redraw(); lib.pdf(doc, args, kind + '-fill');
            a.strokeColor = gray(40); a.strokeWeight = 7;
            app.redraw(); lib.pdf(doc, args, kind + '-native');
            row.before = order(rect);
            row.frameChars = [];
            for (f = 0; f < n; f++) row.frameChars.push(frames[f].textRange.characters.length);
            at = kind + '.route';
            doc.selection = null;
            for (f = 0; f < n; f++) frames[f].selected = true;
            // viaCorrect: the panel's Correct Selection (bridge "correct") instead of the script.
            row.route = args.viaCorrect ? send('correct') : String($.evalFile(new File(here + '/strokemaster-fix-story.jsx')));
            row.gate = (/"appearance":\{"gate":"([^"]*)"/.exec(row.route) || [])[1];
            doc.selection = null; app.redraw();
            row.after = order(rect);
            row.groups = doc.groupItems.length;
            lib.pdf(doc, args, kind + '-route');
            doc.close(SaveOptions.DONOTSAVECHANGES); doc = null;
        }
        lib.write(args, 'results.json', {schema: 1, host: app.version, font: args.font, rows: rows});
        return 'DONE|cases=' + rows.length;
    } catch (e) {
        if (doc) try { doc.close(SaveOptions.DONOTSAVECHANGES); } catch (ignored) {}
        rows.push({error: String(e), stage: at});
        try { lib.write(args, 'results.json', {schema: 1, host: app.version, font: args.font, rows: rows}); } catch (w) {}
        return 'FAIL|stage=' + at + '|error=' + String(e);
    } finally { app.userInteractionLevel = interaction; }
})();
