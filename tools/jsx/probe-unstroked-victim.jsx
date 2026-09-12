// Isolate an unstroked victim run with all-stroked and no-stroke controls.
// Source, split reference, and independent fill mask are PNG only, from an owned document.
(function(){
    var args=$.global.ISE_PROBE_ARGS||{},here=File($.fileName).parent;
    if(!args.root||!new Folder(args.root).exists){throw new Error('Need a fresh existing output root.');}
    var names=['native.png','split.png','mask.png'];
    for(var i=0;i<names.length;i++){if(new File(args.root+'/'+names[i]).exists){throw new Error('Refusing overwrite.');}}
    var oldLib=$.global.ISE_LIB,oldSplit=$.global.ISE_SPLIT;
    $.evalFile(new File(here.fsName+'/ise-lib.jsx'));var lib=$.global.ISE_LIB;
    $.evalFile(new File(here.fsName+'/ise-split-stroked-text.jsx'));var split=$.global.ISE_SPLIT;
    var report={version:app.version,passed:false,frames:[]},done=false;
    function positions(tf){var a=[];for(var j=0;j<tf.parentStory.words.length;j++){var w=tf.parentStory.words[j];a.push([w.horizontalOffset,w.baseline].join(','));}return a.join(';');}
    try{
        report.log=lib.run(function(){
            var doc=lib.newDoc(600,150),style=lib.baseStyle(doc,'victim',{pointSize:30,leading:14,strokeWeight:12});
            var fs=[],tops=[],bottoms=[],geometry=[];
            for(var k=0;k<3;k++){
                var tf=lib.frame(doc,'V'+k,[20,20+k*195,120,190+k*195],'HH\rHH',style);
                if(k===0){tf.parentStory.paragraphs[0].strokeColor=doc.swatches.itemByName('None');}
                if(k===2){tf.parentStory.texts[0].strokeColor=doc.swatches.itemByName('None');}
                fs.push(tf);geometry.push(positions(tf));report.frames.push({key:'V'+k,name:'V'+k,bounds:tf.geometricBounds});
            }
            var png=app.pngExportPreferences;png.exportResolution=600;png.pngQuality=PNGQualityEnum.MAXIMUM;
            png.transparentBackground=false;png.antiAlias=true;png.pngColorSpace=PNGColorSpaceEnum.RGB;
            png.pngExportRange=PNGExportRangeEnum.EXPORT_ALL;png.exportingSpread=false;png.useDocumentBleeds=false;png.simulateOverprint=false;
            doc.exportFile(ExportFormat.PNG_FORMAT,new File(args.root+'/native.png'),false);
            for(k=0;k<3;k++){
                if(k===2){tops.push(fs[k]);continue;}
                var r=split.splitOne(fs[k]);if(!r.done){throw new Error('Split reference failed: '+r.reason);}
                tops.push(doc.pageItems.itemByID(r.top));bottoms.push(doc.pageItems.itemByID(r.bottom));
            }
            doc.recompose();
            for(k=0;k<3;k++){if(positions(tops[k])!==geometry[k]){throw new Error('Reference changed glyph positions.');}}
            doc.exportFile(ExportFormat.PNG_FORMAT,new File(args.root+'/split.png'),false);
            for(k=0;k<bottoms.length;k++){bottoms[k].remove();}
            for(k=0;k<tops.length;k++){tops[k].parentStory.texts[0].fillColor=doc.swatches.itemByName('Paper');tops[k].parentStory.texts[0].fillTint=100;}
            var bg=doc.rectangles[0];bg.locked=false;bg.fillColor=doc.swatches.itemByName('Black');bg.fillTint=100;
            doc.exportFile(ExportFormat.PNG_FORMAT,new File(args.root+'/mask.png'),false);
            lib.writeText(args.root+'/manifest.json',lib.toJSON({frames:report.frames}));
            done=true;
        });
        report.passed=done&&report.log.indexOf('ERROR ')<0&&report.log.indexOf(' (CHANGED)')<0&&
            !/restore: fails=[1-9]|restore: fails=0 diff=[1-9]|close failed:/.test(report.log);
    }finally{$.global.ISE_LIB=oldLib;$.global.ISE_SPLIT=oldSplit;}
    return lib.toJSON(report);
})();
