// build-S-split.jsx - exercise the fill-over-stroke split (fixture S).
// Requires ise-lib.jsx and ise-split-stroked-text.jsx. Argument: ISE_ARGS.outRoot.
//
// Every variant is built twice, side by side and identical: the left frame is left alone as a
// control and the right frame is split. One export therefore measures both under whatever
// renderer is loaded, which matters because the plugin is normally installed and corrects the
// control as well. What the split must show is not merely "no covered pixels" but the right
// paint order: in the exported PDF all of the stroke frame's objects must precede all of the
// fill frame's, which no renderer can undo.
//
// The geometry is fixture A's, where the defect is known to be large: 24 pt on auto leading with
// an 8 pt Outside stroke, and text whose first line descends directly onto the second line's
// ascenders.
(function () {
    var lib = $.global.ISE_LIB;
    var split = $.global.ISE_SPLIT;
    return lib.run(function () {
        if (!split) { lib.L('ERROR ise-split-stroked-text.jsx is not loaded'); return; }
        var dir = lib.args.outRoot + '/S-split';
        var doc = lib.newDoc(612, 620);
        var magenta = lib.color(doc, 'ISE Magenta', [0, 100, 0, 0]);
        var base = lib.baseStyle(doc, 'ISE S base', { strokeWeight: 8 });

        // Line one descends onto line two's ascenders, so the second line's stroke provably
        // reaches the first line's fill.
        var TEXT = 'gypsy\rHklbd';

        var V = [
            ['S01', 'uniform: black fill, Paper 8 pt Outside', null],
            ['S02', 'varied fill: one magenta character on line one', function (s) { s.characters[2].fillColor = magenta; }],
            ['S03', 'varied stroke: one character at 5 pt magenta stroke', function (s) {
                s.characters[2].strokeColor = magenta;
                s.characters[2].strokeWeight = 5;
            }],
            ['S04', 'CONTROL: no stroke at all (split must refuse)', function (s) {
                s.texts[0].strokeColor = doc.swatches.itemByName('None');
            }],
            ['S05', 'Center alignment (split acts; records what it does)', function (s) {
                s.texts[0].strokeAlignment = TextStrokeAlign.CENTER_ALIGNMENT;
            }]
        ];

        // The two columns are exactly 300 pt apart, which at 300 ppi is exactly 1250 pixels, so
        // a control frame and its split counterpart are rasterized at the same sub-pixel phase
        // and can be compared to each other directly. At an arbitrary spacing they cannot: a
        // 290 pt gap is 1208.33 px, and the third-of-a-pixel offset alone made two frames with
        // identical content differ along every glyph edge, which is a floor under every
        // measurement taken this way.
        var top = 24, rowH = 118, frameH = 96, leftX = 30, rightX = 330, colW = 240;
        var toSplit = [], made = [];
        for (var i = 0; i < V.length; i++) {
            var id = V[i][0], label = V[i][1], how = V[i][2];
            var y = top + i * rowH;
            var ctrl = lib.frame(doc, id + 'c ' + label + ' [control]', [y, leftX, y + frameH, leftX + colW], TEXT, base);
            var test = lib.frame(doc, id + 't ' + label + ' [split]', [y, rightX, y + frameH, rightX + colW], TEXT, base);
            if (how) {
                lib.tryDo(id + ' control', function () { how(ctrl.parentStory); return 'ok'; });
                lib.tryDo(id + ' test', function () { how(test.parentStory); return 'ok'; });
            }
            lib.record(ctrl, label + ' (control)', { key: id + 'c', role: 'control', measure: 'hidden' });
            lib.record(test, label + ' (split)', { key: id + 't', role: 'split', measure: 'hidden' });
            toSplit.push(test);
            made.push(id);
        }

        var results = split.split(toSplit, lib);
        for (var j = 0; j < results.length; j++) {
            var r = results[j];
            lib.L('S result ' + made[j] + ': done=' + r.done + (r.done ? ' ranges=' + r.ranges + ' orderedBehind=' + r.ordered : ' reason=' + r.reason));
        }
        lib.L('items on page after split: ' + doc.pages[0].allPageItems.length);
        lib.saveExport(doc, dir, 'S-split', 300);
    });
})();
