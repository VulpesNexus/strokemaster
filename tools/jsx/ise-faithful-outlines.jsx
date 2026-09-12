// ise-faithful-outlines.jsx - Create Outlines that keeps the appearance of stroked text.
// Pure ASCII. Defines $.global.ISE_FAITHFUL; the caller deletes it afterward.
//
// Native Type > Create Outlines turns text with a stroke into paths whose stroke is centered
// on the outline, painted over the fill, at half the width InDesign actually paints for the
// text. Live text with an Outside stroke is painted as a centered stroke of twice the weight
// under the fill; live text with a Center stroke is painted as a centered stroke of the weight
// over the fill. This converter reads the text's stroke settings first, converts, and then
// rebuilds every resulting path as a stroke-only copy at the painted width plus a fill-only
// copy, grouped so that Outside strokes sit under every fill and Center strokes over them.
// Scope of this prototype: text frames whose stroke and fill settings are uniform.

$.global.ISE_FAITHFUL = (function () {
    var F = {};
    var KEYS = ['strokeAlignment', 'strokeWeight', 'endJoin', 'miterLimit', 'strokeTint', 'overprintStroke', 'fillTint', 'overprintFill'];
    var CR = String.fromCharCode(13), LF = String.fromCharCode(10);

    function signature(t) {
        var a = [];
        for (var i = 0; i < KEYS.length; i++) { a.push(String(t[KEYS[i]])); }
        a.push(t.strokeColor.name);
        a.push(t.fillColor.name);
        return a.join('|');
    }

    function isBlank(text) {
        return String(text).split(CR).join('').split(LF).join('').split(' ').join('') === '';
    }

    function uniformSource(story) {
        var ranges = story.textStyleRanges, first = null, src = null;
        for (var i = 0; i < ranges.length; i++) {
            if (isBlank(ranges[i].contents)) { continue; }
            var sig = signature(ranges[i]);
            if (first === null) { first = sig; src = ranges[i]; } else if (sig !== first) { return null; }
        }
        return src;
    }

    function leaves(item, out) {
        if (item.constructor.name === 'Group') {
            var kids = item.pageItems.everyItem().getElements();
            item.ungroup();
            for (var k = 0; k < kids.length; k++) { leaves(kids[k], out); }
        } else {
            out.push(item);
        }
    }

    F.canConvert = function (tf) {
        try { return tf.constructor.name === 'TextFrame' && uniformSource(tf.parentStory) !== null; } catch (e) { return false; }
    };

    F.convertFrame = function (doc, tf, log) {
        var src = uniformSource(tf.parentStory);
        if (!src) { log('faithful: ' + tf.name + ' has mixed stroke or fill settings; not converted'); return null; }
        var a = {
            align: src.strokeAlignment, weight: src.strokeWeight, join: src.endJoin, miter: src.miterLimit,
            sColor: src.strokeColor, sTint: src.strokeTint, sOver: src.overprintStroke,
            fColor: src.fillColor, fTint: src.fillTint, fOver: src.overprintFill
        };
        var outside = (a.align === TextStrokeAlign.OUTSIDE_ALIGNMENT);
        var painted = outside ? 2 * a.weight : a.weight;
        var hasStroke = a.weight > 0 && a.sColor.name !== 'None';
        var name = tf.name;
        var none = doc.swatches.itemByName('None');

        var result = tf.createOutlines(true);
        var polys = [];
        for (var i = 0; i < result.length; i++) { leaves(result[i], polys); }

        var strokes = [], fills = [];
        for (var p = 0; p < polys.length; p++) {
            var P = polys[p];
            if (hasStroke) {
                var S = P.duplicate(undefined, [0, 0]);
                S.fillColor = none;
                S.strokeColor = a.sColor;
                S.strokeTint = a.sTint;
                S.strokeWeight = painted;
                S.strokeAlignment = StrokeAlignment.CENTER_ALIGNMENT;
                S.endJoin = a.join;
                S.miterLimit = a.miter;
                try { S.overprintStroke = a.sOver; } catch (e1) {}
                S.name = 'stroke';
                strokes.push(S);
            }
            // Setting the stroke color to None already zeroes the weight. Setting strokeWeight = 0
            // afterward gives the path a 1 pt stroke in an unnamed black (InDesign 21.5.1).
            P.strokeColor = none;
            P.fillColor = a.fColor;
            P.fillTint = a.fTint;
            try { P.overprintFill = a.fOver; } catch (e2) {}
            P.name = 'fill';
            fills.push(P);
        }
        var all = strokes.concat(fills);
        var group = all.length > 1 ? doc.pages[0].groups.add(all) : all[0];
        var front = outside ? fills : strokes;
        for (var t = 0; t < front.length; t++) { front[t].bringToFront(); }
        group.name = name + ' (faithful outlines)';
        log('faithful: ' + name + ' alignment=' + a.align + ' weight=' + a.weight + ' painted width=' + painted + ' paths=' + polys.length);
        return group;
    };

    return F;
})();
