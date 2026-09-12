// Owned-document census with independent stroke-only / fill-only reference layers.
// Results certify collection and preservation, not that the installed plugin passes.
(function () {
    var args = $.global.ISE_PROBE_ARGS || {}, here = File($.fileName).parent;
    if (!args.root || !new Folder(args.root).exists) { throw new Error('Need an existing fresh output directory.'); }
    var outputNames = ['native.png', 'reference.png', 'mask.png', 'manifest.json', 'fixture.indd'];
    for (var ni = 0; ni < outputNames.length; ni++) {
        if (new File(args.root + '/' + outputNames[ni]).exists) { throw new Error('Refusing output overwrite.'); }
    }
    var globalKeys = ['ISE_LIB', 'ISE_ARGS'], oldGlobals = [], lib, done = false;
    for (var gi = 0; gi < globalKeys.length; gi++) {
        var key = globalKeys[gi];
        oldGlobals.push({key: key, present: Object.prototype.hasOwnProperty.call($.global, key), value: $.global[key]});
    }
    // Validate supported scalar, Enumerator, and named-swatch readbacks.
    function same(a, b) {
        if (b instanceof Array) { if (!(a instanceof Array) || a.length !== b.length) { return false; } for (var vi = 0; vi < b.length; vi++) { if (!same(a[vi], b[vi])) { return false; } } return true; }
        if (typeof b === 'number') { return isFinite(Number(a)) && Math.abs(Number(a) - b) <= 0.000001; }
        if (typeof b === 'boolean' || typeof b === 'string') { return a === b; }
        if (a === b) { return true; }
        // Enumerators can throw on unsupported properties such as name.
        if (typeof Enumerator !== 'undefined' && b instanceof Enumerator) { return String(a) === String(b); }
        try { if (b && b.name !== undefined) { return !!a && a.name === b.name; } } catch (nameError) { return false; }
        return false;
    }
    var readbackContext = 'initial setup';
    function describeValue(value) {
        if (value === null) { return 'null'; }
        if (value === undefined) { return 'undefined'; }
        if (typeof Enumerator !== 'undefined' && value instanceof Enumerator) { return 'Enumerator(' + String(value) + ')'; }
        try { if (value && value.name !== undefined) { return 'name=' + String(value.name); } } catch (nameError) {}
        try { return typeof value + '(' + String(value) + ')'; } catch (stringError) { return '<unreadable value>'; }
    }
    function requireValue(object, key, expected) {
        var actual, readError;
        try { actual = object[key]; } catch (e) { readError = String(e); }
        if (readError || !same(actual, expected)) {
            throw Error('Setup readback failed: ' + key + ' context=' + readbackContext +
                ' expected=' + describeValue(expected) + ' actual=' + (readError ? '<read threw: ' + readError + '>' : describeValue(actual)));
        }
    }
    function setChecked(object, key, value) { object[key] = value; requireValue(object, key, value); }
    function verifyFile(path, prefix) {
        var file = new File(path);
        if (!file.exists || file.length <= 0) { throw Error('Missing or empty output: ' + path); }
        if (prefix) {
            file.encoding = 'BINARY';
            if (!file.open('r')) { throw Error('Cannot read output: ' + path); }
            var actual;
            try { actual = file.read(prefix.length); }
            finally { if (!file.close()) { throw Error('Cannot close output readback: ' + path); } }
            if (actual !== prefix) { throw Error('Invalid output signature: ' + path); }
        }
    }
    function saveChecked(doc, path) {
        if (new File(path).exists) { throw Error('Refusing overwrite: ' + path); }
        doc.save(new File(path));
        if (!doc.saved || doc.modified || String(doc.fullName.fsName).replace(/\\/g, '/').toLowerCase() !== String(new File(path).fsName).replace(/\\/g, '/').toLowerCase()) {
            throw Error('Save readback failed: ' + path);
        }
        verifyFile(path);
    }
    function exportChecked(doc, format, path, preset) {
        if (new File(path).exists) { throw Error('Refusing overwrite: ' + path); }
        if (preset) { doc.exportFile(format, new File(path), false, preset); }
        else { doc.exportFile(format, new File(path), false); }
        verifyFile(path, /\.png$/i.test(path) ? '\x89PNG\r\n\x1a\n' : (/\.pdf$/i.test(path) ? '%PDF-' : 'PK'));
    }
    var report = {schema: 1, version: app.version, passed: false, frames: [], sampledCharacterFields: ['contents', 'horizontalOffset', 'baseline'], oracle: 'separate centered doubled strokes before original fills', renderValidationRequired: true};
    function setMaskBackgroundTint(background) {
        background.fillTint = 100;
        var actual = background.fillTint;
        report.maskBackgroundTintReadback = actual;
        report.maskBackgroundColorName = background.fillColor.name;
        report.maskBackgroundTintResolution = actual === -1 ? 'unresolved inherited/overridden' : 'explicit readback';
        // Only this mask background admits the observed 100 -> -1 readback.
        // Effective tint remains unverified until external background pixel and
        // recorded-mask equality checks pass. Generic tint checks stay exact.
        if (actual !== 100 && actual !== -1) { requireValue({fillTint: actual}, 'fillTint', 100); }
    }
    function writeManifest(path, text) {
        if (new File(path).exists) { throw Error('Refusing overwrite: ' + path); }
        var file = new File(path), wrote, readback;
        file.encoding = 'UTF-8'; file.lineFeed = 'Unix';
        if (!file.open('w')) { throw Error('Cannot open manifest: ' + path); }
        try { wrote = file.write(text); }
        finally { if (!file.close()) { throw Error('Cannot close manifest: ' + path); } }
        if (!wrote) { throw Error('Cannot write manifest: ' + path); }
        verifyFile(path);
        file = new File(path); file.encoding = 'UTF-8';
        if (!file.open('r')) { throw Error('Cannot read manifest: ' + path); }
        try { readback = file.read(); }
        finally { if (!file.close()) { throw Error('Cannot close manifest readback: ' + path); } }
        if (readback !== text) { throw Error('Manifest readback mismatch: ' + path); }
    }
    function positions(tf) {
        var out = [];
        for (var i = 0; i < tf.characters.length; i++) {
            var c = tf.characters[i]; out.push([c.contents, c.horizontalOffset, c.baseline].join('|'));
        }
        return out.join(';');
    }
    try {
        delete $.global.ISE_ARGS;
        $.evalFile(new File(here.fsName + '/ise-lib.jsx'));
        lib = $.global.ISE_LIB;
        report.log = lib.run(function () {
            var doc = lib.newDoc(800, 1000), none = doc.swatches.itemByName('None');
            var red = lib.color(doc, 'Probe Red', [0, 100, 100, 0]);
            var blue = lib.color(doc, 'Probe Blue', [100, 60, 0, 0]);
            var style = lib.baseStyle(doc, 'Victim census', {pointSize: 30, leading: 14, strokeWeight: 12});
            requireValue(style, 'pointSize', 30); requireValue(style, 'leading', 14); requireValue(style, 'strokeWeight', 12);
            requireValue(style, 'strokeAlignment', TextStrokeAlign.OUTSIDE_ALIGNMENT); requireValue(style, 'endJoin', EndJoin.ROUND_END_JOIN);
            requireValue(doc.documentPreferences, 'pageWidth', 800); requireValue(doc.documentPreferences, 'pageHeight', 1000);
            requireValue(red, 'colorValue', [0, 100, 100, 0]); requireValue(blue, 'colorValue', [100, 60, 0, 0]);
            var cases = [
                ['VC00', 'unstroked first line, stroked second'],
                ['VC01', 'stroked first line, unstroked second'],
                ['VC02', 'both stroked'], ['VC03', 'both unstroked'],
                ['VC04', 'alternating three lines'], ['VC05', 'different stroke weights'],
                ['VC06', 'different stroke and fill colors'], ['VC07', 'None with positive stored weight'],
                ['VC08', 'zero weight with Paper stroke color'], ['VC09', '0 percent text opacity'],
                ['VC10', 'character mixed stroke states'], ['VC11', 'thin benign round strokes'],
                ['VC12', 'thin benign miter strokes'], ['VC13', 'thick nonoverlapping lines'],
                ['VC14', 'underline decoration'], ['VC15', 'strikethrough decoration'],
                ['VC16', 'stroke tint zero'], ['VC17', 'None plus zero weight'],
                ['VC18', 'rotated frame'], ['VC19', 'baseline shift boundary']
            ];
            var frames = [], originalPositions = [];
            for (var k = 0; k < cases.length; k++) {
                readbackContext = 'fixture ' + cases[k][0];
                var x = 20 + (k % 4) * 195, y = 20 + Math.floor(k / 4) * 195;
                var tf = lib.frame(doc, cases[k][0], [y, x, y + 165, x + 170], k === 4 ? 'HH\rHH\rHH' : 'HH\rHH', style);
                var pars = tf.parentStory.paragraphs, text = tf.parentStory.texts[0];
                if (k === 0 || k === 7 || k === 17) { setChecked(pars[0], 'strokeColor', none); }
                if (k === 1) { setChecked(pars[1], 'strokeColor', none); }
                if (k === 3) { setChecked(text, 'strokeColor', none); }
                if (k === 4) { setChecked(pars[0], 'strokeColor', none); setChecked(pars[2], 'strokeColor', none); }
                if (k === 5) { setChecked(pars[0], 'strokeWeight', 3); setChecked(pars[1], 'strokeWeight', 12); }
                if (k === 6) { setChecked(pars[0], 'fillColor', red); setChecked(pars[1], 'fillColor', blue); setChecked(pars[0], 'strokeColor', blue); setChecked(pars[1], 'strokeColor', red); }
                if (k === 8 || k === 17) { setChecked(pars[0], 'strokeWeight', 0); }
                if (k === 17) { setChecked(pars[0], 'strokeColor', none); }
                if (k === 9) { setChecked(tf.texts[0], 'strokeColor', doc.swatches.itemByName('Paper')); setChecked(tf.contentTransparencySettings.blendingSettings, 'opacity', 0); }
                if (k === 10) { setChecked(tf.characters[0], 'strokeColor', none); setChecked(tf.characters[4], 'strokeColor', none); }
                if (k === 11 || k === 12) { setChecked(text, 'strokeWeight', 0.1); setChecked(text, 'leading', 42); }
                if (k === 12) { setChecked(text, 'endJoin', EndJoin.MITER_END_JOIN); }
                if (k === 13) { setChecked(text, 'leading', 60); }
                if (k === 14) { setChecked(text, 'underline', true); setChecked(text, 'underlineWeight', 5); setChecked(text, 'underlineOffset', -3); setChecked(text, 'underlineColor', red); }
                if (k === 15) { setChecked(text, 'strikeThru', true); setChecked(text, 'strikeThroughWeight', 5); setChecked(text, 'strikeThroughColor', red); }
                if (k === 16) { setChecked(text, 'strokeTint', 0); }
                if (k === 18) { setChecked(tf, 'rotationAngle', 8); }
                if (k === 19) { setChecked(tf.characters[1], 'baselineShift', 4); }
                doc.recompose();
                if (tf.overflows) { throw new Error(cases[k][0] + ' overset'); }
                frames.push(tf); originalPositions.push(positions(tf));
                report.frames.push({key: cases[k][0], name: cases[k][1], bounds: tf.geometricBounds, sourceCharacters: tf.characters.length});
            }
            var png = app.pngExportPreferences;
            readbackContext = 'PNG preferences';
            setChecked(png, 'exportResolution', 600); setChecked(png, 'pngQuality', PNGQualityEnum.MAXIMUM); setChecked(png, 'transparentBackground', false);
            setChecked(png, 'antiAlias', true); setChecked(png, 'pngColorSpace', PNGColorSpaceEnum.RGB); setChecked(png, 'pngExportRange', PNGExportRangeEnum.EXPORT_ALL);
            setChecked(png, 'exportingSpread', false); setChecked(png, 'useDocumentBleeds', false); setChecked(png, 'simulateOverprint', false);
            saveChecked(doc, args.root + '/fixture.indd');
            exportChecked(doc, ExportFormat.PNG_FORMAT, args.root + '/native.png');
            var bottoms = [];
            for (k = 0; k < frames.length; k++) {
                var top = frames[k], bottom = top.duplicate(); bottoms.push(bottom); bottom.sendToBack();
                // Character access avoids mutable range segmentation during paint edits.
                for (var ci = 0; ci < top.characters.length; ci++) {
                    readbackContext = 'reference ' + cases[k][0] + ' character ' + ci;
                    var tc = top.characters[ci], bc = bottom.characters[ci];
                    var color = tc.strokeColor, width = tc.strokeWeight;
                    setChecked(bc, 'fillColor', none); setChecked(bc, 'underline', false); setChecked(bc, 'strikeThru', false);
                    setChecked(bc, 'strokeAlignment', TextStrokeAlign.CENTER_ALIGNMENT);
                    setChecked(bc, 'strokeWeight', 2 * width); setChecked(bc, 'strokeColor', color);
                    setChecked(tc, 'strokeWeight', 0); setChecked(tc, 'strokeColor', none);
                }
            }
            readbackContext = 'reference background';
            var bg = doc.rectangles[0]; setChecked(bg, 'locked', false); bg.sendToBack();
            doc.recompose();
            for (k = 0; k < frames.length; k++) {
                if (positions(frames[k]) !== originalPositions[k] || positions(bottoms[k]) !== originalPositions[k]) { throw new Error('Oracle moved glyphs: ' + cases[k][0]); }
            }
            exportChecked(doc, ExportFormat.PNG_FORMAT, args.root + '/reference.png');
            for (k = 0; k < bottoms.length; k++) { bottoms[k].remove(); }
            // Use tintable process white, not the special Paper swatch. None fills
            // in the stroke-only reference need no tint assignment or readback.
            readbackContext = 'mask process white';
            var maskWhite = lib.color(doc, 'Probe Mask White', [0, 0, 0, 0]);
            requireValue(maskWhite, 'colorValue', [0, 0, 0, 0]);
            requireValue(maskWhite, 'model', ColorModel.PROCESS); requireValue(maskWhite, 'space', ColorSpace.CMYK);
            for (k = 0; k < frames.length; k++) {
                for (ci = 0; ci < frames[k].characters.length; ci++) {
                    readbackContext = 'mask ' + cases[k][0] + ' character ' + ci;
                    var mc = frames[k].characters[ci];
                    setChecked(mc, 'fillColor', maskWhite); setChecked(mc, 'fillTint', 100);
                }
                readbackContext = 'mask ' + cases[k][0] + ' decorations';
                var ft = frames[k].parentStory.texts[0];
                setChecked(ft, 'underline', false); setChecked(ft, 'strikeThru', false);
            }
            readbackContext = 'mask background';
            setChecked(bg, 'fillColor', doc.swatches.itemByName('Black')); setMaskBackgroundTint(bg);
            exportChecked(doc, ExportFormat.PNG_FORMAT, args.root + '/mask.png');
            writeManifest(args.root + '/manifest.json', lib.toJSON({frames: report.frames}));
            report.sampledCharacterPositionsPreserved = true; done = true;
        });
        report.passed = done && report.log.indexOf('ERROR ') < 0 && report.log.indexOf(' (CHANGED)') < 0 &&
            !/restore: fails=[1-9]|restore: fails=0 diff=[1-9]|failed:/i.test(report.log);
    } finally {
        for (var ri = 0; ri < oldGlobals.length; ri++) {
            var savedGlobal = oldGlobals[ri];
            if (savedGlobal.present) { $.global[savedGlobal.key] = savedGlobal.value; }
            else { delete $.global[savedGlobal.key]; }
        }
    }
    return lib.toJSON(report);
})();
