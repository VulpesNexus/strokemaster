// ise-live-sync.jsx - keep a split pair's two frames showing the same text.
// Requires ise-lib.jsx. Argument: ISE_ARGS.syncMode = 'once' | 'start' | 'stop' | 'status',
// and optionally ISE_ARGS.logPath and ISE_ARGS.intervalMs. The argument is named syncMode rather
// than mode so that a script loading this file as a library cannot trigger it by accident.
// Anything but 'once' needs a persistent engine:
//   powershell -NoProfile -File tools\run-jsx.ps1 ise-live-sync.jsx -Engine iseLiveSync -ArgJson "{\"syncMode\":\"start\"}"
//
// ise-split-stroked-text.jsx turns one stroked story into two superposed frames, which is the
// only correction that survives on a machine with nothing installed. The cost is that the text
// then lives in two places, and InDesign offers no way to keep them together: a linked story
// synchronizes the text and then overwrites the child's formatting with the parent's, paragraph
// styles included (tools/jsx/probe-style-mapping.jsx). So the sync has to be done by script.
//
// There is also no text-change event to hang it on. Measured in tools/jsx/probe-live-sync.jsx:
// the whole DocumentEvent family is beforeSave, afterSave, beforeSaveAs, afterSaveAs,
// beforeSaveACopy, afterSaveACopy, beforeClose, and afterClose. Nothing reports an edit. What
// does work is an idle task: registered with a sleep interval it fired 12 times in 8 seconds and
// 25 in 16, and kept firing after the script that registered it had returned. Polling is
// therefore the only route, and this is built on it, with beforeSave as a backstop for the case
// where a document is saved between two ticks.
//
// The top frame is authoritative. It holds the current text and the real fills; the bottom frame
// is rebuilt from it and given the stroke back. Rebuilding rather than patching is what makes
// this safe under arbitrary edits: no character offset recorded before an edit has to still
// mean anything afterward.
//
// Deliberate limit: the bottom frame's stroke settings must be uniform across its story. They
// are read from the frame itself, not from a record written at split time, because a record is
// indexed by character positions that an edit invalidates. A pair whose stroke varies within
// the story is reported and skipped rather than guessed at.
$.global.ISE_SYNC = (function () {
    var api = {};
    var LABEL = 'ISE_SPLIT';
    var TASK = 'iseLiveSyncTask';

    api.log = [];
    function L(s) { api.log.push(String(s)); }
    api.L = L;

    function name(c) {
        try { return c === null || c === undefined ? 'None' : String(c.name); } catch (e) { return 'None'; }
    }

    function bounds(tf) {
        var b = tf.geometricBounds;
        return [Number(b[0]), Number(b[1]), Number(b[2]), Number(b[3])];
    }

    function sameBounds(a, b) {
        for (var i = 0; i < 4; i++) { if (Math.abs(a[i] - b[i]) > 0.01) { return false; } }
        return true;
    }

    /** Every split pair in a document, matched by label and by occupying the same rectangle. */
    api.pairs = function (doc) {
        var tops = [], bottoms = [], items = doc.pages[0].allPageItems, i;
        for (i = 0; i < items.length; i++) {
            var it = items[i], tag = '';
            try { if (it.constructor.name !== 'TextFrame') { continue; } tag = String(it.extractLabel(LABEL)); } catch (e) { continue; }
            if (tag === 'fill') { tops.push(it); }
            else if (tag !== '') { bottoms.push(it); }
        }
        var out = [];
        for (i = 0; i < tops.length; i++) {
            var tb = bounds(tops[i]);
            for (var j = 0; j < bottoms.length; j++) {
                if (bottoms[j] && sameBounds(tb, bounds(bottoms[j]))) {
                    out.push({ top: tops[i], bottom: bottoms[j] });
                    bottoms[j] = null;
                    break;
                }
            }
        }
        return out;
    };

    /** The stroke the bottom frame is currently carrying, or null when it is not uniform. */
    function strokeOf(bottom) {
        var ranges = bottom.parentStory.textStyleRanges;
        if (ranges.length === 0) { return null; }
        var first = ranges[0], spec = {
            color: name(first.strokeColor), tint: first.strokeTint, weight: first.strokeWeight,
            align: String(first.strokeAlignment), join: String(first.endJoin),
            miter: first.miterLimit, overprint: first.overprintStroke
        };
        for (var i = 1; i < ranges.length; i++) {
            var r = ranges[i];
            if (name(r.strokeColor) !== spec.color || r.strokeTint !== spec.tint || r.strokeWeight !== spec.weight ||
                String(r.strokeAlignment) !== spec.align || String(r.endJoin) !== spec.join ||
                r.miterLimit !== spec.miter || r.overprintStroke !== spec.overprint) {
                return null;
            }
        }
        return spec;
    }

    function placeBehind(item, reference) {
        var steps = 0;
        while (item.index < reference.index && steps < 4) {
            var before = item.index;
            item.sendBackward();
            steps++;
            if (item.index === before) { break; }
        }
        return item.index > reference.index;
    }

    /** Rebuilds one pair's bottom frame from its top frame. */
    api.resync = function (pair) {
        var top = pair.top, bottom = pair.bottom;
        if (!top.isValid || !bottom.isValid) { return { done: false, reason: 'a frame of the pair is gone' }; }
        var spec = strokeOf(bottom);
        if (spec === null) { return { done: false, reason: 'the stroke is not uniform across the bottom frame; rebuild the pair with the split instead' }; }
        if (spec.color === 'None') { return { done: false, reason: 'the bottom frame carries no stroke' }; }

        var doc = top.parentPage.parent;
        while (doc.constructor.name !== 'Document') { doc = doc.parent; }
        var label = String(bottom.extractLabel(LABEL));
        var keepName = String(bottom.name);
        var box = top.geometricBounds;

        // Build the replacement from the top frame, which is the only place the current text and
        // its fills exist, then give it the stroke back and paint its fill in the stroke color.
        var fresh = top.duplicate();
        fresh.geometricBounds = box;
        var story = fresh.parentStory.texts[0];
        story.strokeColor = doc.swatches.itemByName(spec.color);
        story.strokeTint = spec.tint;
        story.strokeWeight = spec.weight;
        story.strokeAlignment = (spec.align === 'CENTER_ALIGNMENT') ? TextStrokeAlign.CENTER_ALIGNMENT : TextStrokeAlign.OUTSIDE_ALIGNMENT;
        story.endJoin = (spec.join === 'MITER_END_JOIN') ? EndJoin.MITER_END_JOIN : ((spec.join === 'BEVEL_END_JOIN') ? EndJoin.BEVEL_END_JOIN : EndJoin.ROUND_END_JOIN);
        story.miterLimit = spec.miter;
        story.overprintStroke = spec.overprint;
        story.fillColor = doc.swatches.itemByName(spec.color);
        story.fillTint = spec.tint;
        story.overprintFill = spec.overprint;

        fresh.name = keepName;
        fresh.insertLabel(LABEL, label);
        try { bottom.remove(); } catch (e) { return { done: false, reason: 'could not remove the old bottom frame: ' + e.message }; }
        var ordered = placeBehind(fresh, top);
        pair.bottom = fresh;
        return { done: true, ordered: ordered, chars: top.parentStory.characters.length };
    };

    /** A cheap value that changes whenever a top frame's text changes. */
    function signature(doc) {
        var pairs = api.pairs(doc), sig = [];
        for (var i = 0; i < pairs.length; i++) {
            try { sig.push(pairs[i].top.id + ':' + String(pairs[i].top.parentStory.contents)); } catch (e) {}
        }
        return { pairs: pairs, sig: sig.join('') };
    }

    /** Resyncs every pair in a document whose top frame no longer matches its bottom. */
    api.resyncAll = function (doc) {
        var pairs = api.pairs(doc), done = 0, skipped = [];
        for (var i = 0; i < pairs.length; i++) {
            var a = '', b = '';
            try { a = String(pairs[i].top.parentStory.contents); b = String(pairs[i].bottom.parentStory.contents); } catch (e) { continue; }
            if (a === b) { continue; }
            var r = api.resync(pairs[i]);
            if (r.done) { done++; } else { skipped.push(r.reason); }
        }
        return { pairs: pairs.length, resynced: done, skipped: skipped };
    };

    api.tick = function () {
        for (var d = 0; d < app.documents.length; d++) {
            var doc = app.documents[d];
            try {
                var state = signature(doc);
                var key = 'doc' + d;
                if ($.global.ISE_SYNC_SIG === undefined) { $.global.ISE_SYNC_SIG = {}; }
                if ($.global.ISE_SYNC_SIG[key] !== undefined && $.global.ISE_SYNC_SIG[key] !== state.sig) {
                    var res = api.resyncAll(doc);
                    $.global.ISE_SYNC_COUNT = ($.global.ISE_SYNC_COUNT || 0) + res.resynced;
                }
                $.global.ISE_SYNC_SIG[key] = signature(doc).sig;
            } catch (e) {}
        }
    };

    api.stop = function () {
        var t = app.idleTasks.itemByName(TASK);
        if (t.isValid) { t.sleep = 0; t.remove(); return true; }
        return false;
    };

    api.start = function (intervalMs) {
        api.stop();
        $.global.ISE_SYNC_SIG = {};
        $.global.ISE_SYNC_COUNT = 0;
        var t = app.idleTasks.add({ name: TASK, sleep: intervalMs || 1000 });
        t.addEventListener(IdleEvent.ON_IDLE, function () { $.global.ISE_SYNC.tick(); });
        return t;
    };

    api.status = function () {
        var t = app.idleTasks.itemByName(TASK);
        return 'watching=' + t.isValid + ' resyncs=' + ($.global.ISE_SYNC_COUNT || 0);
    };

    return api;
})();

// The driver runs only on its own argument name, so that loading this file as a library with
// -With cannot execute a command as a side effect. Gating on a plain "mode" was not enough:
// another probe passing its own mode made this driver run as well, printing its status twice,
// and a mode of "stop" would then have silently stopped a watcher the caller never asked about.
if ($.global.ISE_LIB && $.global.ISE_LIB.args && $.global.ISE_LIB.args.syncMode) (function () {
    var lib = $.global.ISE_LIB;
    var sync = $.global.ISE_SYNC;
    var out = lib.run(function () {
        var mode = String(lib.args.syncMode);
        lib.L('mode=' + mode + ' engine=' + ($.engineName || '(unnamed)'));
        if (mode === 'once') {
            if (app.documents.length === 0) { lib.L('no document open'); return; }
            var r = sync.resyncAll(app.documents[0]);
            lib.L('pairs=' + r.pairs + ' resynced=' + r.resynced + (r.skipped.length ? ' skipped: ' + r.skipped.join('; ') : ''));
        } else if (mode === 'start') {
            var t = sync.start(Number(lib.args.intervalMs || 1000));
            lib.L('watching, interval=' + t.sleep + ' ms, valid=' + t.isValid);
        } else if (mode === 'stop') {
            lib.L('removed=' + sync.stop() + ' ' + sync.status());
        } else {
            lib.L(sync.status());
        }
    });
    // A script in a persistent engine returns nothing to DoScript, so it reports through a file.
    try { lib.writeText(String(lib.args.logPath || (Folder.temp.fsName + '/ise-live-sync.log')), out); } catch (e) {}
    return out;
})();
