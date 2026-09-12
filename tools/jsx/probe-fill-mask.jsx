// Generate a white fill-only coverage mask from an owned copy of fixture N.
// Does not export PDF, overwrite the fixture, or depend on the repaint plugin.
(function () {
    var args=$.global.ISE_PROBE_ARGS||{},here=File($.fileName).parent;
    if(!args.root){throw new Error('Need a fresh output root.');}
    var source=new File(here.parent.parent.fsName+'/tests/indesign-stroke/N-transparency/minimal/N-transparency.indd');
    var copy=new File(args.root+'/mask-source.indd'),pngFile=new File(args.root+'/fill-mask.png');
    if(!source.exists||!copy.parent.exists||copy.exists||pngFile.exists){throw new Error('Missing input/root or existing output.');}
    var previous=$.global.ISE_LIB;$.evalFile(new File(here.fsName+'/ise-lib.jsx'));var lib=$.global.ISE_LIB;
    var report={version:app.version,passed:false},completed=false;
    function positions(doc){
        var a=[];
        for(var i=0;i<doc.textFrames.length;i++){
            var f=doc.textFrames[i],words=f.parentStory.words;
            for(var j=0;j<words.length;j++){a.push([String(words[j].contents),Number(words[j].horizontalOffset),Number(words[j].baseline)]);}
        }
        return a;
    }
    try {
        report.log=lib.run(function(){
            if(!source.copy(copy)){throw new Error('Could not create owned fixture copy.');}
            var doc=lib.track(app.open(copy,false)),before=positions(doc),none=doc.swatches.itemByName('None');
            var paper=doc.swatches.itemByName('Paper'),black=doc.swatches.itemByName('Black');
            for(var i=0;i<doc.allPageItems.length;i++){
                var item=doc.allPageItems[i];item.locked=false;
                if(item.constructor.name==='TextFrame'){
                    item.fillColor=none;item.strokeColor=none;
                    var t=item.parentStory.texts[0];t.strokeColor=none;t.fillColor=paper;t.fillTint=100;
                    t.overprintFill=false;t.overprintStroke=false;
                    var targets=['transparencySettings','contentTransparencySettings','fillTransparencySettings','strokeTransparencySettings'];
                    for(var k=0;k<targets.length;k++){
                        var s=item[targets[k]];s.blendingSettings.blendMode=BlendMode.NORMAL;s.blendingSettings.opacity=100;
                        s.dropShadowSettings.mode=ShadowMode.NONE;s.featherSettings.mode=FeatherMode.NONE;
                    }
                }else if(item.constructor.name==='Rectangle'){
                    item.fillColor=black;item.fillTint=100;item.strokeColor=none;
                }else{throw new Error('Unexpected non-text fixture object: '+item.constructor.name);}
            }
            doc.recompose();var after=positions(doc),max=0;
            if(before.length!==after.length){throw new Error('Word count changed.');}
            for(i=0;i<before.length;i++){
                if(before[i][0]!==after[i][0]){throw new Error('Text order changed.');}
                max=Math.max(max,Math.abs(before[i][1]-after[i][1]),Math.abs(before[i][2]-after[i][2]));
            }
            if(max>0.0001){throw new Error('Geometry shifted while making mask.');}
            var png=app.pngExportPreferences;png.exportResolution=300;png.pngQuality=PNGQualityEnum.MAXIMUM;
            png.transparentBackground=false;png.antiAlias=true;png.pngColorSpace=PNGColorSpaceEnum.RGB;
            png.pngExportRange=PNGExportRangeEnum.EXPORT_ALL;png.exportingSpread=false;
            png.useDocumentBleeds=false;png.simulateOverprint=false;
            doc.exportFile(ExportFormat.PNG_FORMAT,pngFile,false);
            report.wordCount=before.length;report.maxWordShiftPt=max;report.textFrames=doc.textFrames.length;
            completed=true;
        });
        report.passed=completed&&report.log.indexOf('ERROR ')<0&&report.log.indexOf(' (CHANGED)')<0&&
            !/restore: fails=[1-9]|restore: fails=0 diff=[1-9]|close failed:/.test(report.log);
    }finally{$.global.ISE_LIB=previous;}
    return lib.toJSON(report);
})();
