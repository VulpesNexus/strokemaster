// Export the supplied census without saving it; track and close only newly opened documents.
(function () {
    var args=$.global.ISE_PROBE_ARGS||{}, here=File($.fileName).parent;
    if(!args.source||!new File(args.source).exists||!args.root||!new Folder(args.root).exists){throw Error('Need source fixture and fresh output root.');}
    var names=['direct.pdf','direct.png','roundtrip.idml','reopened.indd','reopened.pdf','reopened.png'];
    for(var i=0;i<names.length;i++){if(new File(args.root+'/'+names[i]).exists){throw Error('Output already exists.');}}
    for(i=0;i<app.documents.length;i++){
        if(app.documents[i].saved&&app.documents[i].fullName.fsName===new File(args.source).fsName){throw Error('Source is already open; refusing ownership.');}
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
    var report={schema:1,version:app.version,passed:false,sampledCharacterFields:['contents','horizontalOffset','baseline','strokeColor.name','strokeWeight','fillColor.name','strokeTint','endJoin'],sampledFrameFields:['name']};
    function census(doc){
        var out=[], names={};
        if(doc.textFrames.length!==20){throw Error('Expected 20 census frames.');}
        for(var i=0;i<doc.textFrames.length;i++){
            var tf=doc.textFrames[i],a=[];
            if(!/^VC(?:0[0-9]|1[0-9])$/.test(tf.name)||names[tf.name]||tf.overflows||tf.characters.length===0){throw Error('Invalid census frame.');}
            names[tf.name]=true;
            for(var c=0;c<tf.characters.length;c++){
                var ch=tf.characters[c];
                a.push([ch.contents,ch.horizontalOffset,ch.baseline,ch.strokeColor.name,ch.strokeWeight,ch.fillColor.name,ch.strokeTint,ch.endJoin.toString()].join('|'));
            }
            out.push(tf.name+':'+a.join(';'));
        }
        out.sort();return out.join('\n');
    }
    try{
        delete $.global.ISE_ARGS;
        $.evalFile(new File(here.fsName + '/ise-lib.jsx'));
        lib = $.global.ISE_LIB;
        report.log=lib.run(function(){
            var doc=lib.track(app.open(new File(args.source),false));doc.recompose();
            var signature=census(doc);report.frames=doc.textFrames.length;
            var png=app.pngExportPreferences;
            readbackContext = 'PNG preferences';
            setChecked(png, 'exportResolution', 300);setChecked(png, 'pngQuality', PNGQualityEnum.MAXIMUM);setChecked(png, 'transparentBackground', false);
            setChecked(png, 'antiAlias', true);setChecked(png, 'pngColorSpace', PNGColorSpaceEnum.RGB);setChecked(png, 'pngExportRange', PNGExportRangeEnum.EXPORT_ALL);
            setChecked(png, 'exportingSpread', false);setChecked(png, 'useDocumentBleeds', false);setChecked(png, 'simulateOverprint', false);
            var preset=app.pdfExportPresets.itemByName('[PDF/X-4:2008]');if(!preset.isValid){throw Error('Required PDF preset unavailable.');}
            exportChecked(doc, ExportFormat.PDF_TYPE, args.root+'/direct.pdf', preset);
            exportChecked(doc, ExportFormat.PNG_FORMAT, args.root+'/direct.png');
            exportChecked(doc, ExportFormat.INDESIGN_MARKUP, args.root+'/roundtrip.idml');
            doc.close(SaveOptions.NO);
            var roundtrip=lib.track(app.open(new File(args.root+'/roundtrip.idml'),false));roundtrip.recompose();
            if(census(roundtrip)!==signature){throw Error('IDML changed sampled character state or positions.');}
            saveChecked(roundtrip,args.root+'/reopened.indd');roundtrip.close(SaveOptions.NO);
            var reopened=lib.track(app.open(new File(args.root+'/reopened.indd'),false));reopened.recompose();
            if(census(reopened)!==signature){throw Error('INDD save/reopen changed sampled character state or positions.');}
            exportChecked(reopened, ExportFormat.PDF_TYPE, args.root+'/reopened.pdf', preset);
            exportChecked(reopened, ExportFormat.PNG_FORMAT, args.root+'/reopened.png');
            report.sampledCharacterStatePreserved=true;report.renderEquality='requires external comparison';done=true;
        });
        report.passed=done&&report.log.indexOf('ERROR ')<0&&report.log.indexOf(' (CHANGED)')<0&&
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
