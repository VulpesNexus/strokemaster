// build-D-outlines.jsx - reproduce Type > Create Outlines on stroked text (fixture D) and
// record exactly what the conversion produces. Requires ise-lib.jsx. Argument: ISE_ARGS.outRoot.
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var dir = lib.args.outRoot + '/D-create-outlines';
        var doc = lib.newDoc(612, 520);
        var base = lib.baseStyle(doc, 'ISE D base', { pointSize: 48, strokeWeight: 4, endJoin: EndJoin.ROUND_END_JOIN });

        function props(it) {
            var o = { cls: it.constructor.name };
            var keys = ['strokeAlignment', 'strokeWeight', 'endJoin', 'endCap', 'miterLimit', 'fillColor', 'fillTint', 'strokeColor', 'strokeTint', 'overprintFill', 'overprintStroke', 'strokeType'];
            for (var k = 0; k < keys.length; k++) { try { o[keys[k]] = lib.str(it[keys[k]]); } catch (e) { o[keys[k]] = '<n/a>'; } }
            try { o.paths = it.paths.length; } catch (e) {}
            try { o.children = it.allPageItems.length; } catch (e) {}
            return o;
        }
        function convert(tf, label, row, alter) {
            lib.record(tf, label + ': live text', { row: row, role: 'text', measure: 'pair' });
            var dup = tf.duplicate(undefined, [290, 0]);
            dup.name = tf.name + ' (outlined copy)';
            var res = lib.tryDo('createOutlines ' + label, function () { return dup.createOutlines(true); });
            if (!res) { return; }
            for (var i = 0; i < res.length; i++) {
                if (alter) { lib.tryDo('alter ' + label, function () { alter(res[i]); }); }
                lib.record(res[i], label + ': outlines', { row: row, role: 'outlines', result: props(res[i]), measure: 'pair' });
                try {
                    var kids = res[i].allPageItems;
                    for (var j = 0; j < kids.length && j < 6; j++) { lib.manifest.items.push({ parent: res[i].id, row: row, result: props(kids[j]) }); }
                } catch (e) {}
            }
        }

        convert(lib.frame(doc, 'D01 Outside 4 pt round', [20, 24, 90, 290], 'Hog', base), 'D01 Outside 4 pt, Round', 1);

        var f2 = lib.frame(doc, 'D02 Center 8 pt round', [100, 24, 170, 290], 'Hog', base);
        f2.parentStory.texts[0].strokeAlignment = TextStrokeAlign.CENTER_ALIGNMENT;
        f2.parentStory.texts[0].strokeWeight = 8;
        convert(f2, 'D02 Center 8 pt, Round', 2);

        var f3 = lib.frame(doc, 'D03 Outside 4 pt miter limit 10', [180, 24, 250, 290], 'Hog', base);
        f3.parentStory.texts[0].endJoin = EndJoin.MITER_END_JOIN;
        f3.parentStory.texts[0].miterLimit = 10;
        convert(f3, 'D03 Outside 4 pt, Miter, limit 10', 3);

        var f4 = lib.frame(doc, 'D04 two lines', [260, 24, 380, 290], 'Hog\rHog', base);
        convert(f4, 'D04 two paragraphs', 4);

        var f5 = lib.frame(doc, 'D05 Outside 4 pt, outlines set to Outside', [390, 24, 460, 290], 'Hog', base);
        convert(f5, 'D05 Outside 4 pt, outlines then alignment set to Outside', 5, function (it) { it.strokeAlignment = StrokeAlignment.OUTSIDE_ALIGNMENT; });

        // Inline conversion of a text range, as in the fixture: the outlines become anchored objects.
        var f6 = lib.frame(doc, 'D06 inline outlines of a text range', [470, 24, 510, 290], 'Hog', base);
        var inline = lib.tryDo('inline createOutlines', function () { return f6.parentStory.texts[0].createOutlines(true); });
        if (inline) {
            for (var i = 0; i < inline.length; i++) { lib.record(inline[i], 'D06 inline outlines', { row: 6, role: 'outlines', result: props(inline[i]) }); }
            var anchor = f6.parentStory.characters[0];
            lib.manifest.items.push({ row: 6, anchorChar: { strokeWeight: anchor.strokeWeight, strokeAlignment: lib.str(anchor.strokeAlignment), strokeColor: lib.str(anchor.strokeColor), leading: lib.str(anchor.leading) } });
        }
        lib.saveExport(doc, dir, 'D-outlines', 300);
    });
})();
