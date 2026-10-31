// The owner's prep-35 retests T1 and T5 (2026-10-31), one phase per run in one Illustrator (the
// phase is the number of <runId>-phase<n>.json files already written), the timer running between:
//   1  args.file opened (their Tests.ai: threaded frames corrected one by one), first seen.
//   2  its first threaded frame moved; new area text "A" with a stroked character run, alone.
//   3  read: the threaded frames' parent (T1: grouped once edited). "A" threaded on to a new frame.
//   4  read: "A" and its new frame grouped. Close holes turned on (T5: the active document follows).
//   5  read: the story group's style. Also documents as they open and Dashed outside strokes on;
//      args.file2 opened (their hand tests, corrected before, with a dashed Outside square).
//   6  read: that document's dashed paths and status. The first document active again.
//   7  read (no second opening). Strokes added in the Appearance panel off.
//   8  read: the first document's text styles.
// ISE_REGION_ARGS: outDir, runId, file; the second file is the first's name with hand-36 for
// tests-36 (a path of its own would lose its non-ASCII letters on the hidden desktop's command line).
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function rgb(r, g, b) { var c = new RGBColor(); c.red = r; c.green = g; c.blue = b; return c; }
    function refix() { var m = /"refix":\{[^}]*\}/.exec(send('status')); return m ? m[0] : ''; }
    function docNamed(part) { for (var i = 0; i < app.documents.length; i++) if (app.documents[i].name.indexOf(part) >= 0) return app.documents[i]; return null; }
    function threadedParents(doc) {
        var out = [];
        for (var i = 0; i < doc.textFrames.length; i++) {
            var f = doc.textFrames[i];
            if (f.story.textFrames.length > 1 && f.name !== 'A' && f.name !== 'B') out.push(f.parent.typename);
        }
        return out;
    }
    function dumpOf(doc, item, parent) { doc.selection = null; item.selected = true; var d = send('style-dump', parent ? 'parent' : ''); doc.selection = null; return d; }
    var phase = 1;
    while (lib.path(args, 'phase' + phase + '.json').exists) phase++;
    var interaction = app.userInteractionLevel, out = {phase: phase};
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        var one = docNamed('tests-36'), two = docNamed('hand-36');
        if (phase === 1) {
            out.build = /"build":"[^"]*"/.exec(send('status'))[0];
            send('settings', 'defaults');
            out.set = send('settings', 'object_strokes=1');
            one = app.open(new File(args.file));
            out.before = threadedParents(one);
        } else if (phase === 2) {
            for (var i = 0; i < one.textFrames.length; i++)
                if (one.textFrames[i].story.textFrames.length > 1) { one.textFrames[i].translate(1, 0); break; }
            var r = one.pathItems.rectangle(-500, 40, 220, 60);
            var a = one.textFrames.areaText(r);
            a.name = 'A';
            a.contents = 'Threaded stroke text runs on into a second frame below this one, then more words to fill it.';
            var c = a.textRange.characterAttributes;
            c.size = 28; c.fillColor = rgb(0, 0, 0); c.strokeColor = rgb(190, 40, 20); c.strokeWeight = 4;
            one.selection = null;
        } else if (phase === 3) {
            out.after = threadedParents(one);
            var a2 = one.textFrames.getByName('A');
            out.aParent = a2.parent.typename;
            out.aDump = dumpOf(one, a2);
            var r2 = one.pathItems.rectangle(-580, 30, 220, 80);
            var b = one.textFrames.areaText(r2, TextOrientation.HORIZONTAL, a2);
            b.name = 'B';
            one.selection = null;
        } else if (phase === 4) {
            var a3 = one.textFrames.getByName('A'), b3 = one.textFrames.getByName('B');
            out.aParent = a3.parent.typename; out.bParent = b3.parent.typename;
            out.same = a3.parent === b3.parent;
            out.set = send('settings', 'close_holes=1');
        } else if (phase === 5) {
            var a4 = one.textFrames.getByName('A');
            out.groupDump = dumpOf(one, a4, true);
            for (var j = 0; j < one.textFrames.length; j++) {
                var f = one.textFrames[j];
                if (f.story.textFrames.length > 1 && f.name !== 'A' && f.name !== 'B') { out.threadedGroupDump = dumpOf(one, f, true); break; }
            }
            out.set = [send('settings', 'open_documents=1'), send('settings', 'outside_dashes=1')];
            two = app.open(new File(String(args.file).replace('tests-36-', 'hand-36-')));
        } else if (phase === 6) {
            out.dashed = [];
            for (var k = 0; k < two.pathItems.length; k++) {
                var p = two.pathItems[k];
                if (p.stroked && p.strokeDashes.length) out.dashed.push({w: p.strokeWidth, dashes: p.strokeDashes.length, dump: dumpOf(two, p)});
            }
            out.texts = [];
            for (var m = 0; m < two.textFrames.length && m < 4; m++) out.texts.push(dumpOf(two, two.textFrames[m]));
            app.activeDocument = one;
        } else if (phase === 7) {
            out.set = send('settings', 'object_strokes=0');
        } else {
            out.texts = [];
            for (var q = 0; q < one.textFrames.length; q++) {
                var t = one.textFrames[q];
                out.texts.push({name: t.name, parent: t.parent.typename, dump: dumpOf(one, t)});
            }
        }
        out.active = app.documents.length ? app.activeDocument.name : '';
        out.refix = refix();
        lib.write(args, 'phase' + phase + '.json', out);
        return 'DONE|phase' + phase;
    } catch (e) { return 'FAIL|' + String(e) + ' line ' + e.line; }
    finally { app.userInteractionLevel = interaction; }
})();
