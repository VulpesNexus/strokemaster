// Close holes across two separate live stories. Run one phase at a time after the host
// finishes drawing. ISE_REGION_ARGS supplies outDir and runId.
// Correct them separately first, so owned tip and hole fields also exercise group admission.
(function () {
    if (!$.getenv('CODEX_HIDDEN_DESKTOP_NAME') || !$.getenv('STROKEMASTER_SETTINGS'))
        throw Error('isolated test host and private settings required');
    var args = $.global.ISE_REGION_ARGS || {};
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function rgb(r, g, b) { var c = new RGBColor(); c.red = r; c.green = g; c.blue = b; return c; }
    function select(doc) {
        doc.selection = null;
        for (var i = 0; i < doc.textFrames.length; i++) doc.textFrames[i].selected = true;
    }
    function dress(t) {
        t.contents = 'U';
        var a = t.textRange.characterAttributes;
        a.textFont = app.textFonts.getByName('MyriadPro-Regular');
        a.size = 120;
        a.fillColor = rgb(0, 0, 0);
        a.strokeColor = rgb(220, 50, 10);
        a.strokeWeight = 12;
    }
    // A text object's glyph bounds [left, top, right, bottom], from an outlined copy.
    function glyphBounds(t) {
        var g = t.duplicate().createOutline(), b = g.geometricBounds;
        g.remove();
        return b;
    }
    function styles(doc) {
        var out = [];
        for (var i = 0; i < doc.textFrames.length; i++) {
            var t = doc.textFrames[i];
            doc.selection = null;
            t.selected = true;
            out.push({name:t.name, storyFrames:t.story.textFrames.length, parent:t.parent.typename,
                      own:send('style-dump'), group:send('style-dump', 'parent')});
        }
        doc.selection = null;
        return out;
    }
    var phase = 1;
    while (lib.path(args, 'phase' + phase + '.json').exists) phase++;
    if (lib.path(args, 'failed.json').exists) throw Error('previous failure: use a fresh run directory');
    var interaction = app.userInteractionLevel, out = {phase:phase};
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        var doc = app.documents.length ? app.activeDocument : null;
        if (phase === 1) {
            if (app.documents.length) throw Error('fresh empty test host required');
            out.pause = send('refix', 'off');
            out.defaults = send('settings', 'defaults');
            out.set = [send('settings', 'text_stories=0'), send('settings', 'text_miter=1'),
                       send('settings', 'glyph_tips=0'), send('settings', 'outside_dashes=0'), send('settings', 'close_holes=1')];
            doc = app.documents.add(DocumentColorSpace.RGB, 400, 400);
            var bottom = doc.textFrames.pointText([90, 90]);
            dress(bottom); bottom.name = 'p51-bottom';
            var top = doc.textFrames.pointText([90, 90]);
            dress(top); top.name = 'p51-top';
            if (args.mixedStroke) top.textRange.characterAttributes.strokeWeight = 16;
            top.rotate(180);
            // The fills are 4 pt apart; uniform 12 pt strokes overlap by 8 pt. Placed by
            // the glyphs' own bounds: a point text frame's bounds include its ascent and descent.
            var b = glyphBounds(bottom), t = glyphBounds(top);
            top.translate((b[0] + b[2]) / 2 - (t[0] + t[2]) / 2, b[1] + 4 - t[3]);
            out.glyphs = [String(glyphBounds(bottom)), String(glyphBounds(top))];
            out.fillGap = 4;
            out.strokeWeight = 12;
            out.strokeWeights = [12, args.mixedStroke ? 16 : 12];
            out.before = lib.snapshot(doc);
        } else if (phase === 2) {
            select(doc);
            out.correct = send('correct');
        } else if (phase === 3) {
            out.individual = styles(doc);
            if (args.mixedStroke) lib.pdf(doc, args, 'individual');
            out.set = [send('settings', 'text_stories=1'), send('settings', 'close_holes=0')];
            if (args.existingGroup) out.set.push(send('settings', 'text_miter=0'));
        } else if (phase === 4) {
            select(doc);
            out.correct = send('correct');
        } else if (phase === 5) {
            if (args.existingGroup) {
                var ownerGroup = doc.textFrames[0].parent;
                if (ownerGroup.typename !== 'GroupItem') throw Error('expected compatible fill group');
                ownerGroup.name = 'owner-preserved';
                ownerGroup.opacity = 73;
            }
            out.styles = styles(doc);
            lib.pdf(doc, args, 'off');
            out.status = send('status');
            out.set = send('settings', 'close_holes=1');
            if (args.existingGroup) out.miter = send('settings', 'text_miter=1');
        } else if (phase === 6) {
            select(doc);
            out.correct = send('correct');
        } else if (phase === 7) {
            out.styles = styles(doc);
            out.after = lib.snapshot(doc);
            if (out.styles.length !== 2 || out.after.frames.length !== 2) throw Error('two live frames required');
            for (var f = 0; f < out.styles.length; f++) {
                var fieldStyle = args.mixedStroke ? out.styles[f].own : out.styles[f].group;
                if (!args.existingGroup && fieldStyle.indexOf('org.strokemaster.close-holes.v1') < 0)
                    throw Error('missing ' + (args.mixedStroke ? 'child' : 'group') + ' hole field');
                if (out.after.frames[f].storyFrames !== 1 || out.after.frames[f].contents !== 'U')
                    throw Error('live story contents changed');
                if (args.mixedStroke && !args.existingGroup && out.styles[f].parent !== 'Layer')
                    throw Error('incompatible mixed strokes were grouped');
            }
            if (args.mixedStroke && !args.existingGroup && lib.read(args, 'phase6.json').correct.indexOf('story.mixed-stroke-fields') < 0)
                throw Error('mixed-stroke refusal not reported');
            if (args.existingGroup) {
                var keptGroup = doc.textFrames[0].parent;
                if (keptGroup.typename !== 'GroupItem' || keptGroup.name !== 'owner-preserved' || Math.abs(keptGroup.opacity - 73) > 0.01)
                    throw Error('owner group attributes changed');
                if (lib.read(args, 'phase6.json').correct.indexOf('correct.existing-mixed-group') < 0)
                    throw Error('existing group refusal not reported');
                out.ownerGroupPreserved = true;
            }
            out.fieldsVerified = !args.existingGroup;
            lib.pdf(doc, args, 'on');
            out.status = send('status');
            out.resume = send('refix', 'on');
        } else throw Error('all seven phases are complete');
        lib.write(args, 'phase' + phase + '.json', out);
        return 'DONE|phase' + phase;
    } catch (e) {
        send('refix', 'on');
        lib.write(args, 'failed.json', {phase:phase, error:String(e), line:e.line});
        return 'FAIL|' + String(e) + ' line ' + e.line;
    }
    finally { app.userInteractionLevel = interaction; }
})();
