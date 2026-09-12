// build-D-faithful.jsx - compare live stroked text with the faithful outline conversion.
// Requires ise-lib.jsx and ise-faithful-outlines.jsx. Argument: ISE_ARGS.outRoot.
// Each row holds live text and, 290 pt to the right, a converted copy of the same frame.
(function () {
    var lib = $.global.ISE_LIB, F = $.global.ISE_FAITHFUL;
    return lib.run(function () {
        var dir = lib.args.outRoot + '/D-create-outlines';
        var doc = lib.newDoc(612, 470);
        var base = lib.baseStyle(doc, 'ISE DF base', { pointSize: 48, strokeWeight: 4, endJoin: EndJoin.ROUND_END_JOIN });

        function row(name, bounds, text, tweak) {
            var tf = lib.frame(doc, name, bounds, text, base);
            if (tweak) { tweak(tf.parentStory.texts[0]); }
            lib.record(tf, name + ': live text', { role: 'text', measure: 'pair' });
            var dup = tf.duplicate(undefined, [290, 0]);
            dup.name = name + ' (converted copy)';
            var g = lib.tryDo('faithful ' + name, function () { return F.convertFrame(doc, dup, lib.L); });
            if (g) { lib.record(g, name + ': faithful outlines', { role: 'outlines' }); }
        }

        row('F01 Outside 4 pt round', [20, 24, 90, 290], 'Hog');
        row('F02 Center 8 pt round', [100, 24, 170, 290], 'Hog', function (t) { t.strokeAlignment = TextStrokeAlign.CENTER_ALIGNMENT; t.strokeWeight = 8; });
        row('F03 Outside 4 pt miter limit 10', [180, 24, 250, 290], 'Hog', function (t) { t.endJoin = EndJoin.MITER_END_JOIN; t.miterLimit = 10; });
        row('F04 Outside 8 pt two lines at 24 pt', [260, 24, 350, 290], 'gyp jq\rHkbd lh', function (t) { t.pointSize = 24; t.strokeWeight = 8; });
        row('F05 Outside 4 pt counters', [370, 24, 440, 290], 'Bog egg');

        lib.saveExport(doc, dir, 'D-faithful', 300);
    });
})();
