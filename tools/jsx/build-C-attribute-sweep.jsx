// build-C-attribute-sweep.jsx - which character attributes split a paint unit (fixture C).
// Requires ise-lib.jsx. Argument: ISE_ARGS.outRoot.
// One frame per variant. Each frame holds seven characters with one override, normally on
// character index 3. A variant that cannot be applied is logged and recorded as skipped.
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var dir = lib.args.outRoot + '/C-run-fragmentation';
        var doc = lib.newDoc(612, 830);
        var base = lib.baseStyle(doc, 'ISE C base', { strokeWeight: 4 });
        var magenta = lib.color(doc, 'ISE Magenta', [0, 100, 0, 0]);
        var black = doc.swatches.itemByName('Black');
        var emptyCS = doc.characterStyles.add({ name: 'ISE empty character style' });
        var magentaCS = doc.characterStyles.add({ name: 'ISE magenta character style' });
        magentaCS.fillColor = magenta;
        var cond = lib.tryDo('condition', function () {
            var c = doc.conditions.add({ name: 'ISE condition' });
            doc.conditionalTextPreferences.showConditionIndicators = ConditionIndicatorMode.HIDE_INDICATORS;
            return c;
        });
        var altFont = null, altCandidates = ['Minion Pro\tBold', 'Minion Pro\tItalic', 'Minion Pro\tSemibold', 'Arial\tBold'];
        for (var a = 0; a < altCandidates.length && altFont === null; a++) { if (lib.fontOk(altCandidates[a])) { altFont = altCandidates[a]; } }
        var varFont = lib.fontOk('Bahnschrift\tRegular') ? 'Bahnschrift\tRegular' : null;
        lib.L('altFont=' + altFont + ' varFont=' + varFont);

        function grepStyle(name, cs) {
            var ps = doc.paragraphStyles.add({ name: name, basedOn: base });
            ps.nestedGrepStyles.add({ appliedCharacterStyle: cs, grepExpression: '(?<=^nnn)n' });
            return ps;
        }
        function nestedStyle(name, cs) {
            var ps = doc.paragraphStyles.add({ name: name, basedOn: base });
            ps.nestedStyles.add({ appliedCharacterStyle: cs, delimiter: NestedStyleDelimiters.ANY_CHARACTER, repetition: 4, inclusive: true });
            return ps;
        }
        function ch(s) { return s.characters[3]; }

        var V = [
            ['C00', 'control: no override', null, null],
            ['C01', 'fill color Magenta', null, function (s) { ch(s).fillColor = magenta; }],
            ['C02', 'fill tint 50', null, function (s) { ch(s).fillTint = 50; }],
            ['C03', 'stroke color Magenta', null, function (s) { ch(s).strokeColor = magenta; }],
            ['C04', 'stroke tint 50', null, function (s) { ch(s).strokeTint = 50; }],
            ['C05', 'stroke weight 5', null, function (s) { ch(s).strokeWeight = 5; }],
            ['C06', 'stroke weight 4 set locally (same value as style)', null, function (s) { ch(s).strokeWeight = 4; }],
            ['C07', 'join Miter', null, function (s) { ch(s).endJoin = EndJoin.MITER_END_JOIN; }],
            ['C08', 'miter limit 10 with round join (no visual effect)', null, function (s) { ch(s).miterLimit = 10; }],
            ['C09', 'stroke alignment Center', null, function (s) { ch(s).strokeAlignment = TextStrokeAlign.CENTER_ALIGNMENT; }],
            ['C10', 'overprint stroke', null, function (s) { ch(s).overprintStroke = true; }],
            ['C11', 'overprint fill', null, function (s) { ch(s).overprintFill = true; }],
            ['C12', 'font change (' + altFont + ')', null, altFont ? function (s) { ch(s).appliedFont = altFont; } : 'skip'],
            ['C13', 'point size 23.9', null, function (s) { ch(s).pointSize = 23.9; }],
            ['C14', 'horizontal scale 99 %', null, function (s) { ch(s).horizontalScale = 99; }],
            ['C15', 'vertical scale 101 %', null, function (s) { ch(s).verticalScale = 101; }],
            ['C16', 'baseline shift 0.5 pt', null, function (s) { ch(s).baselineShift = 0.5; }],
            ['C17', 'tracking 20', null, function (s) { ch(s).tracking = 20; }],
            ['C18', 'manual kerning -50 before character 3', null, function (s) { s.insertionPoints[3].kerningValue = -50; }],
            ['C19', 'language English: UK', null, function (s) { ch(s).appliedLanguage = 'English: UK'; }],
            ['C20', 'skew -10', null, function (s) { ch(s).skew = -10; }],
            ['C21', 'character rotation 10', null, function (s) { ch(s).characterRotation = 10; }],
            ['C22', 'empty character style', null, function (s) { ch(s).appliedCharacterStyle = emptyCS; }],
            ['C23', 'character style with fill Magenta', null, function (s) { ch(s).appliedCharacterStyle = magentaCS; }],
            ['C24', 'underline', null, function (s) { ch(s).underline = true; }],
            ['C25', 'underline color set, underline off (no visual effect)', null, function (s) { ch(s).underlineColor = magenta; }],
            ['C26', 'strikethrough', null, function (s) { ch(s).strikeThru = true; }],
            ['C27', 'Lowercase on a lowercase letter (no visual effect)', null, function (s) { ch(s).capitalization = Capitalization.LOWER_CASE; }],
            ['C28', 'Small Caps', null, function (s) { ch(s).capitalization = Capitalization.SMALL_CAPS; }],
            ['C29', 'Superscript', null, function (s) { ch(s).position = Position.SUPERSCRIPT; }],
            ['C30', 'No Break (no visual effect)', null, function (s) { ch(s).noBreak = true; }],
            ['C31', 'OpenType slashed zero on a letter (no visual effect)', null, function (s) { ch(s).otfSlashedZero = true; }],
            ['C32', 'OpenType oldstyle figures on a letter (no visual effect)', null, function (s) { ch(s).otfFigureStyle = OTFFigureStyle.PROPORTIONAL_OLDSTYLE; }],
            ['C33c', 'control for C33: text with fi', 'nnfinnn', null],
            ['C33', 'ligatures off on fi', 'nnfinnn', function (s) { s.characters.itemByRange(2, 3).ligatures = false; }],
            ['C34c', 'control for C34: text with st', 'nnstnnn', null],
            ['C34', 'discretionary ligatures on st', 'nnstnnn', function (s) { s.characters.itemByRange(2, 3).otfDiscretionaryLigature = true; }],
            ['C35', 'GREP style applying the empty character style to character 3', null, 'grep-empty'],
            ['C36', 'GREP style applying the magenta character style to character 3', null, 'grep-magenta'],
            ['C37', 'nested style applying the empty character style through 4 characters', null, 'nested-empty'],
            ['C38', 'conditional text condition, indicators hidden', null, cond ? function (s) { ch(s).appliedConditions = [cond]; } : 'skip'],
            ['C39', 'hyperlink text source on character 3', null, function (s) {
                var src = doc.hyperlinkTextSources.add(ch(s));
                var h = doc.hyperlinks.add(src, doc.hyperlinkURLDestinations.add('https://example.com/'));
                h.visible = false;
            }],
            ['C40', 'XML tag on character 3', null, function (s) {
                var storyTag = doc.xmlTags.add('ISEStory'), charTag = doc.xmlTags.add('ISEChar');
                var el = doc.xmlElements[0].xmlElements.add(storyTag, s);
                el.xmlElements.add(charTag, ch(s));
            }],
            ['C41', 'OpenType stylistic set 1', null, function (s) { ch(s).otfStylisticSets = 1; }],
            ['C42c', 'control for C42: variable font Bahnschrift', 'nnnnnnn', varFont ? 'varfont-control' : 'skip'],
            ['C42', 'variable font axis on character 3 (Bahnschrift)', 'nnnnnnn', varFont ? 'varfont-axis' : 'skip'],
            ['C43', 'fill color Black set locally (same value as style)', null, function (s) { ch(s).fillColor = black; }],
            ['C44', 'kerning method Optical on character 3', null, function (s) { ch(s).kerningMethod = '$ID/Optical'; }],
            ['C45', 'stroke tint 100 set explicitly (style uses the swatch tint)', null, function (s) { ch(s).strokeTint = 100; }],
            ['C46', 'variable font named instance Bold on character 3 (Bahnschrift)', 'nnnnnnn', varFont ? 'varfont-style' : 'skip']
        ];

        var cols = [24, 222, 420], rowH = 46, top = 18, colW = 180;
        var styles = {};
        for (var i = 0; i < V.length; i++) {
            var id = V[i][0], label = V[i][1], text = V[i][2] || 'nnnnnnn', how = V[i][3];
            var col = i % 3, row = Math.floor(i / 3);
            var y = top + row * rowH, x = cols[col];
            var tf = lib.frame(doc, id + ' ' + label, [y, x, y + 38, x + colW], text, base);
            var s = tf.parentStory, status = 'applied';
            if (how === 'skip') {
                status = 'skipped: prerequisite unavailable';
            } else if (typeof how === 'string') {
                status = lib.tryDo(id, function () {
                    if (how === 'grep-empty') { s.texts[0].applyParagraphStyle(styles.ge || (styles.ge = grepStyle('ISE GREP empty', emptyCS)), true); }
                    if (how === 'grep-magenta') { s.texts[0].applyParagraphStyle(styles.gm || (styles.gm = grepStyle('ISE GREP magenta', magentaCS)), true); }
                    if (how === 'nested-empty') { s.texts[0].applyParagraphStyle(styles.ne || (styles.ne = nestedStyle('ISE nested empty', emptyCS)), true); }
                    if (how === 'varfont-control' || how === 'varfont-axis' || how === 'varfont-style') {
                        s.texts[0].appliedFont = varFont;
                        if (how === 'varfont-axis') {
                            var fnt = app.fonts.itemByName(varFont);
                            lib.L('varfont axes=' + lib.str(fnt.designAxesName) + ' range=' + lib.str(fnt.designAxesRange) + ' values=' + lib.str(fnt.designAxesValues));
                            lib.L('char designAxes before=' + lib.str(ch(s).designAxes));
                            var vals = fnt.designAxesValues.slice(0);
                            vals[0] = vals[0] + 200;
                            ch(s).designAxes = vals;
                            lib.L('char designAxes after=' + lib.str(ch(s).designAxes));
                        }
                        if (how === 'varfont-style') {
                            ch(s).fontStyle = 'Bold';
                            lib.L('varfont-style char font=' + lib.str(ch(s).appliedFont) + ' style=' + ch(s).fontStyle);
                        }
                    }
                    return 'applied';
                }) || 'FAILED (see log)';
            } else if (how) {
                status = lib.tryDo(id, function () { how(s); return 'applied'; }) || 'FAILED (see log)';
            }
            lib.record(tf, label, { key: id, status: status });
        }
        lib.saveExport(doc, dir, 'C-attribute-sweep', 300);
    });
})();
