// The review of 2026-11-01, host-tested: one phase per run in one Illustrator (the phase is the
// number of <runId>-phase<n>.json files already written), the timer running between.
//   1  Strokes added through the Appearance panel, Close holes, and Also documents as they open
//      on; args.file opened (the owner's Tests.ai: threaded frames whose Appearance strokes are
//      lifted onto their story group), corrected as it opens.
//   2  read. The Appearance opt-in off: the lifted stroke goes back to each frame (it was read
//      after being freed, a crash).
//   3  read. On again.
//   4  read. Off again: each text object's stroke order is put back (with Close holes on, the
//      record never matched).
//   5  read. A story of three threaded frames, a filled rectangle between the second and third
//      (two runs), and a star with a thick Miter stroke, corrected as they are made.
//   6  read. The third frame's characters get a miter limit by hand (released), and the story
//      group of the first two 50% opacity.
//   7  read. Trim glyph tips off (Reapply): the released frame keeps its limit and stays released,
//      the group stays as it is (it was rebuilt, losing its opacity).
//   8  read. Automatic correction paused; the star's limit set by hand; resumed.
//   9  read. No miter spikes in shapes and outlines off (Reapply): the star, changed where the
//      timer did not see it, is released with its limit kept.
//  10  read.
// ISE_REGION_ARGS: outDir, runId, file.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function rgb(r, g, b) { var c = new RGBColor(); c.red = r; c.green = g; c.blue = b; return c; }
    function refix() { var m = /"refix":\{[^}]*\}/.exec(send('status')); return m ? m[0] : ''; }
    function dumpOf(doc, item, parent) { doc.selection = null; item.selected = true; var d = send('style-dump', parent ? 'parent' : ''); doc.selection = null; return d; }
    function order(d) { var m = /order '([^']*)'/.exec(d); return m ? m[1] : '?'; }
    function rec(d) { var m = /RECORD[^;]*/.exec(d); return m ? m[0] : ''; }
    function named(doc, n) { try { return doc.pageItems.getByName(n); } catch (e) { return null; } }
    // The owner's text objects that are not ours: parent and stroke order of each.
    function owners(doc) {
        var out = [];
        for (var i = 0; i < doc.textFrames.length && out.length < 12; i++) {
            var f = doc.textFrames[i];
            if (/^rv-/.test(f.name)) continue;
            var d = dumpOf(doc, f);
            out.push(f.parent.typename + ' ' + order(d) + ' strokes ' + (d.match(/STROKE w/g) || []).length + (args.detail ? ' | ' + d : ''));
        }
        return out;
    }
    function ours(doc, o) {
        var b = named(doc, 'rv-b'), star = named(doc, 'rv-star');
        if (b) {
            o.bParent = b.parent.typename;
            o.bRecord = rec(dumpOf(doc, b));
            o.bLimit = b.textRange.characterAttributes.strokeMiterLimit;
            if (b.parent.typename === 'GroupItem') { o.groupOpacity = b.parent.opacity; o.groupName = b.parent.name; }
        }
        var c = named(doc, 'rv-c');
        if (c) { o.cParent = c.parent.typename; o.cRecord = rec(dumpOf(doc, c)); o.cLimit = c.textRange.characterAttributes.strokeMiterLimit; }
        if (star) { o.starLimit = Math.round(star.strokeMiterLimit * 100) / 100; o.starRecord = rec(dumpOf(doc, star)); }
    }
    var phase = 1;
    while (lib.path(args, 'phase' + phase + '.json').exists) phase++;
    var interaction = app.userInteractionLevel, out = {phase: phase};
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        var doc = app.documents.length ? app.activeDocument : null;
        if (phase === 1) {
            out.build = /"build":"[^"]*"/.exec(send('status'))[0];
            if (args.expectedBuild && out.build !== '"build":"' + args.expectedBuild + '"') throw Error('unexpected plugin build');
            send('settings', 'defaults');
            out.set = [send('settings', 'object_strokes=1'), send('settings', 'close_holes=1'), send('settings', 'open_documents=1')];
            doc = app.open(new File(args.file));
        } else if (phase === 2) {
            out.owners = owners(doc);
            out.set = send('settings', 'object_strokes=0');
        } else if (phase === 3) {
            out.owners = owners(doc);
            out.set = send('settings', 'object_strokes=1');
        } else if (phase === 4) {
            out.owners = owners(doc);
            out.set = send('settings', 'object_strokes=0');
        } else if (phase === 5) {
            out.owners = owners(doc);
            var x = doc.artboards[0].artboardRect[2] + 200, y = doc.artboards[0].artboardRect[1];
            function frame(name, top, left) {
                var t = doc.textFrames.areaText(doc.pathItems.rectangle(top, left, 260, 60));
                t.name = name;
                return t;
            }
            var a = frame('rv-a', y, x), b = frame('rv-b', y - 40, x + 40);
            var block = doc.pathItems.rectangle(y - 80, x + 20, 300, 40);
            block.filled = true; block.fillColor = rgb(200, 220, 240); block.stroked = false; block.name = 'rv-block';
            var c = frame('rv-c', y - 100, x + 60);
            b.previousFrame = a;
            c.previousFrame = b;
            a.contents = 'Wavy WAVES vow Wavy WAVES vow Wavy WAVES vow';
            var r = a.textRange.characterAttributes;
            r.size = 40; r.fillColor = rgb(30, 40, 90); r.strokeColor = rgb(240, 160, 30); r.strokeWeight = 6;
            var star = doc.pathItems.star(y - 220, x + 100, 60, 20, 5);
            star.name = 'rv-star';
            star.filled = true; star.fillColor = rgb(30, 40, 90);
            star.stroked = true; star.strokeColor = rgb(240, 160, 30); star.strokeWidth = 12;
            star.strokeJoin = StrokeJoin.MITERENDJOIN; star.strokeMiterLimit = 10;
            doc.selection = null;
        } else if (phase === 6) {
            ours(doc, out);
            // The third frame, a run of its own (the block between), corrected alone: its limit by hand.
            named(doc, 'rv-c').textRange.characterAttributes.strokeMiterLimit = 7;
            var b6 = named(doc, 'rv-b');
            if (b6.parent.typename === 'GroupItem') { b6.parent.opacity = 50; b6.parent.name = 'rv-group'; }
        } else if (phase === 7) {
            ours(doc, out);
            out.set = send('settings', 'glyph_tips=0');
        } else if (phase === 8) {
            ours(doc, out);
            out.pause = send('refix', 'off');
            named(doc, 'rv-star').strokeMiterLimit = 9;
            out.resume = send('refix', 'on');
        } else if (phase === 9) {
            ours(doc, out);
            out.set = send('settings', 'shape_miter=0');
        } else {
            ours(doc, out);
            out.owners = owners(doc);
        }
        out.refix = refix();
        lib.write(args, 'phase' + phase + '.json', out);
        return 'DONE|phase' + phase;
    } catch (e) { return 'FAIL|' + String(e) + ' line ' + e.line; }
    finally { app.userInteractionLevel = interaction; }
})();
