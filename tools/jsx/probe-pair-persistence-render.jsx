// Paired-frame persistence: real INDD/IDML and PNG equality must both hold.
(function(){
    var args=$.global.ISE_PROBE_ARGS||{},here=File($.fileName).parent;
    if(!args.root||!new Folder(args.root).exists){throw Error('Need a fresh existing output root.');}
    var files=['before.png','after.png','pairs.indd','pairs.idml','reopened.indd'];
    for(var i=0;i<files.length;i++){if(new File(args.root+'/'+files[i]).exists){throw Error('Refusing overwrite.');}}
    var globalKeys = ['ISE_LIB', 'ISE_ARGS', 'ISE_SPLIT'], oldGlobals = [], lib, done = false;
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
    var report={schema:1,host:'InDesign',version:app.version,passed:false,checks:[],sampledCharacterFields:['contents','horizontalOffset','baseline','fillColor.name','fillTint','strokeColor.name','strokeWeight','strokeTint'],sampledFrameFields:['ISE_SPLIT token and role']};
    function check(name,ok){report.checks.push({name:name,passed:!!ok});if(!ok){throw Error(name);}}
    function state(doc){
        var items=[],tokens={},pairCount=0;
        for(var n=0;n<doc.textFrames.length;n++){
            var tf=doc.textFrames[n],label=String(tf.extractLabel('ISE_SPLIT')),parts=label.split('|');
            check('ISE3 pair label '+n,parts.length===3&&parts[0]==='ISE3'&&/^[^\s|]+$/.test(parts[1])&&(parts[2]==='fill'||parts[2]==='stroke'));
            var tokenKey='$'+parts[1];
            if(!Object.prototype.hasOwnProperty.call(tokens,tokenKey)){tokens[tokenKey]={fill:0,stroke:0};pairCount++;}
            tokens[tokenKey][parts[2]]++;
            check('unique pair member '+n,tokens[tokenKey][parts[2]]===1);
            check('nonempty composed frame '+n,tf.characters.length>0&&!tf.overflows);
            var chars=[];
            for(var c=0;c<tf.characters.length;c++){
                var ch=tf.characters[c];
                if(parts[2]==='fill'){check('true None and zero top stroke '+n+'/'+c,ch.strokeColor.name==='None'&&ch.strokeWeight===0);}
                if(parts[2]==='stroke'){
                    // These fixtures use Outside strokes: the lower glyph body
                    // takes the stroke paint to close the antialiased seam.
                    check('bottom fill matches stroke color '+n+'/'+c,ch.fillColor.name===ch.strokeColor.name);
                    if(ch.strokeColor.name!=='None'){check('bottom fill matches stroke tint '+n+'/'+c,ch.fillTint===ch.strokeTint);}
                }
                chars.push([ch.contents,ch.horizontalOffset,ch.baseline,ch.fillColor.name,ch.fillTint,ch.strokeColor.name,ch.strokeWeight,ch.strokeTint].join('|'));
            }
            items.push(label+':'+chars.join(';'));
        }
        check('exactly two pair tokens',pairCount===2);
        for(var tokenKey in tokens){if(Object.prototype.hasOwnProperty.call(tokens,tokenKey)){
            check('reciprocal pair '+tokenKey,tokens[tokenKey].fill===1&&tokens[tokenKey].stroke===1);
        }}
        check('four paired frames',items.length===4);items.sort();return items.join('\n');
    }
    try{
        delete $.global.ISE_ARGS;
        $.evalFile(new File(here.fsName + '/ise-lib.jsx'));
        lib = $.global.ISE_LIB;
        $.evalFile(new File(here.fsName + '/ise-split-stroked-text.jsx'));
        var split = $.global.ISE_SPLIT;
        report.log=lib.run(function(){
            var doc=lib.newDoc(450,220),style=lib.baseStyle(doc,'Pair persistence',{pointSize:30,leading:17,strokeWeight:6});
            requireValue(style,'pointSize',30);requireValue(style,'leading',17);requireValue(style,'strokeWeight',6);
            requireValue(style,'strokeAlignment',TextStrokeAlign.OUTSIDE_ALIGNMENT);requireValue(style,'endJoin',EndJoin.ROUND_END_JOIN);
            requireValue(doc.documentPreferences,'pageWidth',450);requireValue(doc.documentPreferences,'pageHeight',220);
            var a=lib.frame(doc,'Uniform',[20,20,190,195],'HH\rHH',style);
            var b=lib.frame(doc,'Varied',[20,240,190,415],'HH\rHH',style);
            readbackContext = 'Varied fixture character 0';
            setChecked(b.characters[0],'strokeColor',lib.color(doc,'Blue',[100,70,0,0]));setChecked(b.characters[0],'strokeWeight',3);
            readbackContext = 'Varied fixture character 3';
            setChecked(b.characters[3],'strokeColor',lib.color(doc,'Red',[0,100,100,0]));setChecked(b.characters[3],'strokeWeight',9);
            check('uniform split succeeds',split.splitOne(a).done);check('varied split succeeds',split.splitOne(b).done);
            doc.recompose();var before=state(doc);
            readbackContext = 'PNG preferences';
            var png=app.pngExportPreferences;setChecked(png, 'exportResolution', 600);setChecked(png, 'pngQuality', PNGQualityEnum.MAXIMUM);
            setChecked(png, 'transparentBackground', false);setChecked(png, 'antiAlias', true);setChecked(png, 'pngColorSpace', PNGColorSpaceEnum.RGB);
            setChecked(png, 'pngExportRange', PNGExportRangeEnum.EXPORT_ALL);setChecked(png, 'exportingSpread', false);setChecked(png, 'useDocumentBleeds', false);setChecked(png, 'simulateOverprint', false);
            saveChecked(doc,args.root+'/pairs.indd');
            exportChecked(doc, ExportFormat.PNG_FORMAT, args.root+'/before.png');
            doc.close(SaveOptions.NO);
            var saved=lib.track(app.open(new File(args.root+'/pairs.indd'),false));saved.recompose();
            check('INDD preserves pair identity and sampled character fields',state(saved)===before);
            exportChecked(saved, ExportFormat.INDESIGN_MARKUP, args.root+'/pairs.idml');saved.close(SaveOptions.NO);
            var idml=lib.track(app.open(new File(args.root+'/pairs.idml'),false));idml.recompose();
            check('IDML preserves pair identity and sampled character fields',state(idml)===before);
            saveChecked(idml,args.root+'/reopened.indd');idml.close(SaveOptions.NO);
            var reopened=lib.track(app.open(new File(args.root+'/reopened.indd'),false));reopened.recompose();
            check('final INDD preserves pair identity and sampled character fields',state(reopened)===before);
            exportChecked(reopened, ExportFormat.PNG_FORMAT, args.root+'/after.png');
            report.pairIdentityAndSampledFieldsPreserved=true;report.renderEquality='requires external exact comparison';done=true;
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
