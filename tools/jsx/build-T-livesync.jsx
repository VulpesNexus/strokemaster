// build-T-livesync.jsx - does a script-driven resync actually keep a split pair together?
// Requires ise-lib.jsx, ise-split-stroked-text.jsx, and ise-live-sync.jsx.
// Argument: ISE_ARGS.outRoot.
//
// The split leaves the text in two frames and InDesign cannot keep them together itself, so the
// script has to. This checks the claim end to end rather than by inspection: build a stroked
// frame, split it, edit the TOP frame only, resync, and then verify three separate things, any
// one of which failing would make the sync useless.
//
//   T1  the bottom frame ends up showing the edited text
//   T2  the bottom frame still carries the stroke it had, at the same weight, color, alignment,
//       and join, with its fill still painted in the stroke color
//   T3  the bottom frame is still behind the top frame
//
// T2 is the one that matters most, because it is exactly what InDesign's own linked stories get
// wrong: they synchronize the text and then overwrite the child's formatting with the parent's
// (tools/jsx/probe-style-mapping.jsx).
(function () {
    var lib = $.global.ISE_LIB;
    var split = $.global.ISE_SPLIT;
    var sync = $.global.ISE_SYNC;
    return lib.run(function () {
        if (!split) { lib.L('ERROR ise-split-stroked-text.jsx is not loaded'); return; }
        if (!sync) { lib.L('ERROR ise-live-sync.jsx is not loaded'); return; }

        var dir = lib.args.outRoot + '/T-livesync';
        var doc = lib.newDoc(612, 260);
        var base = lib.baseStyle(doc, 'ISE T base', { strokeWeight: 8 });
        var tf = lib.frame(doc, 'T01 live sync', [40, 40, 140, 420], 'gypsy\rHklbd', base);

        var res = split.splitOne(tf, lib);
        lib.L('split: done=' + res.done + ' ranges=' + lib.str(res.ranges) + ' reason=' + lib.str(res.reason));
        if (!res.done) { return; }

        var pairs = sync.pairs(doc);
        lib.L('pairs found after split: ' + pairs.length);
        if (pairs.length !== 1) { lib.L('ERROR expected exactly one pair'); return; }
        var top = pairs[0].top, bottom = pairs[0].bottom;

        function describe(frame) {
            var c = frame.parentStory.characters[0];
            return 'weight=' + c.strokeWeight + ' stroke=' + lib.str(c.strokeColor.name) +
                   ' fill=' + lib.str(c.fillColor.name) + ' align=' + lib.str(c.strokeAlignment) +
                   ' join=' + lib.str(c.endJoin);
        }
        var strokeBefore = describe(bottom);
        lib.L('bottom before: "' + String(bottom.contents).replace(/\r/g, '\\r') + '" ' + strokeBefore);
        lib.L('z-order before: top.index=' + top.index + ' bottom.index=' + bottom.index);

        // Edit the top frame only, which is what a user would do.
        var NEW = 'gypsy edited\rHklbd as well';
        top.parentStory.contents = NEW;
        lib.L('after editing the top only, bottom still reads: "' + String(bottom.contents).replace(/\r/g, '\\r') + '"');

        var r = sync.resyncAll(doc);
        lib.L('resyncAll: pairs=' + r.pairs + ' resynced=' + r.resynced + (r.skipped.length ? ' skipped=' + r.skipped.join('; ') : ''));

        // The bottom frame was rebuilt, so the pair has to be found again.
        var after = sync.pairs(doc);
        if (after.length !== 1) { lib.L('ERROR expected one pair after resync, found ' + after.length); return; }
        var top2 = after[0].top, bottom2 = after[0].bottom;

        var t1 = String(bottom2.contents) === String(top2.contents);
        var strokeAfter = describe(bottom2);
        var t2 = (strokeAfter === strokeBefore);
        var t3 = bottom2.index > top2.index;

        lib.L('bottom after : "' + String(bottom2.contents).replace(/\r/g, '\\r') + '" ' + strokeAfter);
        lib.L('T1 text synced           : ' + t1);
        lib.L('T2 stroke survived intact: ' + t2 + (t2 ? '' : ' (was ' + strokeBefore + ')'));
        lib.L('T3 still behind the top  : ' + t3 + ' (top.index=' + top2.index + ' bottom.index=' + bottom2.index + ')');
        lib.L('VERDICT: ' + ((t1 && t2 && t3) ? 'resync keeps the pair together' : 'RESYNC FAILED'));

        lib.record(top2, 'live sync, fill frame', { key: 'T01f', measure: 'hidden' });
        lib.saveExport(doc, dir, 'T-livesync', 300);
    });
})();
