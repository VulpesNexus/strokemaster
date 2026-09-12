// ise-split-stroked-text.jsx - split stroked text into a fill-only frame over a stroke-only one.
// A library. Load it, then call $.global.ISE_SPLIT.split(frames).
//
// This is the document-level workaround for fixtures A and C, for anyone who cannot install the
// plugin. Painting every stroke before every fill is exactly what the plugin does at draw time;
// here the same ordering is built out of two real frames, so the corrected appearance is carried
// by the document itself and survives on any machine, any InDesign version, and any export.
//
// How the two frames are derived, per text style range rather than per frame, so that a story
// with varied stroke settings splits correctly:
//
//   bottom   keeps the range's stroke exactly as it was, and takes the STROKE color as its FILL.
//            Filling the glyph body with the stroke color is what removes the seam: if the
//            bottom frame were stroke-only, the fill above would meet bare halo along the glyph
//            outline and antialiasing would leave a hairline. A range with no stroke becomes
//            invisible here (fill None), so it cannot darken the range above it.
//   top      keeps the range's fill exactly as it was, and has no stroke at all.
//
// Registration is safe: measured in InDesign 2026, adding an Outside stroke moves no glyph.
// Word offsets are identical between a stroked and an unstroked copy of the same text, and the
// baseline difference between two frames equals their frame offset exactly
// (tools/jsx/probe-linked-stories.jsx, Q7). The two frames therefore compose identically.
//
// The stroke color is never cleared by setting a weight of zero. On a text range, setting
// strokeWeight to 0 after clearing the color switches a 1 pt black stroke back on, so only the
// color is set to None (docs/INDESIGN_STROKE_INVESTIGATION.md).
//
// What this does NOT do is keep the two frames in sync afterward. InDesign's linked stories
// synchronize the text but overwrite the child's formatting with the parent's, paragraph styles
// included, and no style mapping is reachable from scripting to prevent it
// (tools/jsx/probe-style-mapping.jsx). The original settings are therefore recorded in the
// bottom frame's script label so that a later pass can rebuild the pair.
//
// Pure ASCII: ExtendScript sources are read in the system codepage.
$.global.ISE_SPLIT = (function () {
    var api = {};
    var LABEL = 'ISE_SPLIT';

    function log(lib, s) { if (lib && lib.L) { lib.L(s); } }

    function swatchName(c) {
        try { return c === null || c === undefined ? 'None' : String(c.name); } catch (e) { return 'None'; }
    }

    /** True for a frame this library is prepared to handle.

        Threaded frames are refused rather than mishandled: a duplicate of one frame in a thread
        does not carry the thread, so the copy's text would not match what the original shows. */
    api.check = function (tf) {
        if (!tf || !tf.isValid) { return 'not a valid page item'; }
        if (tf.constructor.name !== 'TextFrame') { return 'not a text frame (' + tf.constructor.name + ')'; }
        if (tf.previousTextFrame !== null || tf.nextTextFrame !== null) { return 'threaded; split the thread by hand first'; }
        if (tf.parentStory.characters.length === 0) { return 'empty'; }
        if (tf.extractLabel(LABEL) !== '') { return 'already split'; }
        var ranges = tf.parentStory.textStyleRanges, stroked = 0, outside = 0;
        for (var i = 0; i < ranges.length; i++) {
            if (swatchName(ranges[i].strokeColor) !== 'None') {
                stroked++;
                if (String(ranges[i].strokeAlignment) === 'OUTSIDE_ALIGNMENT') { outside++; }
            }
        }
        if (stroked === 0) { return 'no stroked text'; }
        if (outside === 0) { return 'stroked, but no Outside-aligned range; a Center stroke is drawn over its own fill by design'; }
        return '';
    };

    /** True when this range's stroke is the kind that lands under the fill.

        Only Outside strokes are moved to the bottom frame. A Center stroke is painted over its
        own fill by definition, so moving it underneath would not correct anything: it would
        change how the text looks. The plugin skips Center runs for the same reason, and the two
        corrections have to agree, or a document would look different depending on which one was
        applied. Measured: splitting a Center range takes its covered fill from the whole glyph
        core to zero, which is a redesign of the text, not a repair. */
    function isOutside(rec) {
        return rec.strokeAlignment === 'OUTSIDE_ALIGNMENT';
    }

    /** The per-range settings the split needs, captured before anything is changed.

        Applying a color to one range can merge it with its neighbor, which renumbers the ranges
        still to come, so every range is read first and only then written. */
    function capture(tf) {
        var out = [], ranges = tf.parentStory.textStyleRanges;
        for (var i = 0; i < ranges.length; i++) {
            var r = ranges[i];
            out.push({
                start: r.index,
                length: r.characters.length,
                fillColor: swatchName(r.fillColor),
                fillTint: r.fillTint,
                strokeColor: swatchName(r.strokeColor),
                strokeTint: r.strokeTint,
                strokeAlignment: String(r.strokeAlignment),
                overprintFill: r.overprintFill,
                overprintStroke: r.overprintStroke
            });
        }
        return out;
    }

    function rangeOf(tf, rec) {
        return tf.parentStory.characters.itemByRange(rec.start, rec.start + rec.length - 1);
    }

    function sw(doc, name) { return doc.swatches.itemByName(name); }

    /** Moves item behind reference, without sending it behind unrelated content.

        sendToBack would put the frame behind the whole page, including any backdrop. A duplicate
        is created adjacent to its original, so at most one step is ever needed.

        The step count is capped hard and low on purpose. An earlier version looped until the
        indices crossed, which never happened when sendBackward could not move the frame past a
        locked item: it then issued thousands of no-op z-order operations, each forcing a redraw,
        and InDesign stopped responding for minutes. A z-order helper must never be able to spin,
        so this one gives up and reports failure instead of retrying. */
    function placeBehind(item, reference) {
        var steps = 0;
        while (item.index < reference.index && steps < 4) {
            var before = item.index;
            item.sendBackward();
            steps++;
            if (item.index === before) { break; }   // it cannot move; stop rather than repeat
        }
        return item.index > reference.index;
    }

    /** Splits one frame. Returns a record describing what happened. */
    api.splitOne = function (tf, lib) {
        var why = api.check(tf);
        if (why) { return { name: String(tf.isValid ? tf.name : '?'), done: false, reason: why }; }

        var doc = tf.parentPage ? tf.parentPage.parent : app.activeDocument;
        while (doc.constructor.name !== 'Document') { doc = doc.parent; }
        var none = sw(doc, 'None');
        var recs = capture(tf);
        var bounds = tf.geometricBounds;
        var originalName = String(tf.name);

        // A duplicate is created directly in front of its original, so making the DUPLICATE the
        // fill frame and leaving the ORIGINAL as the stroke frame produces the required order at
        // creation, with no z-order call at all. Deriving the order from the duplicate's own
        // placement rather than rearranging afterward is what keeps this safe: the first
        // version duplicated into the back and then stepped the copy backwards until the indices
        // crossed, which wedged InDesign when the step could not move past a locked item.
        var bottom = tf;
        var top = tf.duplicate();
        top.geometricBounds = bounds;
        bottom.name = (originalName || 'text') + ' [ISE stroke]';
        top.name = (originalName || 'text') + ' [ISE fill]';

        var i, rec;
        for (i = 0; i < recs.length; i++) {
            rec = recs[i];
            var br = rangeOf(bottom, rec);
            if (rec.strokeColor === 'None' || !isOutside(rec)) {
                // Nothing belongs underneath. An unstroked range has no halo, and a Center
                // stroke stays on the top frame so that it still paints over its own fill; both
                // must vanish from the bottom frame or they would add ink that was never there.
                br.fillColor = none;
                br.strokeColor = none;
            } else {
                br.fillColor = sw(doc, rec.strokeColor);
                br.fillTint = rec.strokeTint;
                br.overprintFill = rec.overprintStroke;
            }
        }
        for (i = 0; i < recs.length; i++) {
            rec = recs[i];
            if (rec.strokeColor !== 'None' && isOutside(rec)) {
                // Only the color is cleared: a weight of zero would switch a stroke back on.
                // A Center range keeps its stroke here, which is where it already belonged.
                rangeOf(top, rec).strokeColor = none;
            }
        }

        // Expected to be true already, with no step taken; checked rather than assumed.
        var ordered = placeBehind(bottom, top);
        bottom.insertLabel(LABEL, lib && lib.toJSON ? lib.toJSON({ role: 'stroke', of: originalName, ranges: recs }) : 'stroke');
        top.insertLabel(LABEL, 'fill');

        log(lib, 'split ' + originalName + ': ' + recs.length + ' range(s), ordered=' + ordered);
        return {
            name: originalName, done: true, ranges: recs.length, ordered: ordered,
            top: top.id, bottom: bottom.id, bounds: bounds
        };
    };

    /** Splits every frame given, skipping those it cannot handle and saying why. */
    api.split = function (frames, lib) {
        var out = [];
        for (var i = 0; i < frames.length; i++) {
            out.push(api.splitOne(frames[i], lib));
        }
        return out;
    };

    /** The text frames in the current selection, ignoring anything else that is selected. */
    api.selectedFrames = function () {
        var out = [];
        for (var i = 0; i < app.selection.length; i++) {
            var s = app.selection[i];
            try {
                if (s.constructor.name === 'TextFrame') { out.push(s); }
                else if (s.hasOwnProperty('parentTextFrames') && s.parentTextFrames.length) { out.push(s.parentTextFrames[0]); }
            } catch (e) {}
        }
        return out;
    };

    return api;
})();
