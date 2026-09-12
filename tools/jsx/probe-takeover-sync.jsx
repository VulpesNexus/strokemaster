// Actual-host split/sync and IDML identity checks on disposable documents only.
(function () {
    var args=$.global.ISE_PROBE_ARGS || {}, here=File($.fileName).parent;
    if(!args.root){throw new Error('Need a fresh existing output directory.');}
    var idml=new File(args.root+'/pairs.idml');
    if(idml.exists || !idml.parent.exists){throw new Error('Refusing existing output or missing directory.');}
    var oldUI=app.scriptPreferences.userInteractionLevel, owned=[], initial=[], checks=[], error=null;
    var savedSync=$.global.ISE_SYNC, savedSplit=$.global.ISE_SPLIT, savedLib=$.global.ISE_LIB;
    var savedCount=$.global.ISE_SYNC_COUNT,savedLast=$.global.ISE_SYNC_LAST;
    function state(d){return [d.id,d.saved,d.modified,d.allPageItems.length].join('|');}
    for(var n=0;n<app.documents.length;n++){initial.push({doc:app.documents[n],state:state(app.documents[n])});}
    function check(name,ok){checks.push({name:name,passed:Boolean(ok)});}
    var report={version:app.version,checks:checks}, lib=null;
    function frame(doc,page,text){
        var f=page.textFrames.add({geometricBounds:[20,20,90,150]});f.fillColor=doc.swatches.itemByName('None');
        f.strokeColor=doc.swatches.itemByName('None');f.contents=text;
        var t=f.parentStory.texts[0];t.appliedFont='Minion Pro\tRegular';t.pointSize=20;
        t.fillColor=doc.swatches.itemByName('Black');t.strokeColor=doc.swatches.itemByName('Paper');
        t.strokeWeight=3;t.strokeAlignment=TextStrokeAlign.OUTSIDE_ALIGNMENT;t.miterLimit=4;
        return f;
    }
    try {
        app.scriptPreferences.userInteractionLevel=UserInteractionLevels.NEVER_INTERACT;
        $.evalFile(new File(here.fsName+'/ise-lib.jsx'));lib=$.global.ISE_LIB;
        $.evalFile(new File(here.fsName+'/ise-split-stroked-text.jsx'));
        $.evalFile(new File(here.fsName+'/ise-live-sync.jsx'));
        var split=$.global.ISE_SPLIT,sync=$.global.ISE_SYNC;
        var doc=app.documents.add(false);owned.push(doc);doc.documentPreferences.pagesPerDocument=2;
        var first=frame(doc,doc.pages[1],'pair A'),second=frame(doc,doc.pages[1],'pair B');
        var a=split.splitOne(first),b=split.splitOne(second);
        report.splits=[a,b];check('both page-two splits succeeded',a.done&&b.done);
        if(!a.done||!b.done){throw new Error('Split refused: '+lib.toJSON(report.splits));}
        var pairs=sync.pairs(doc);check('same-bounds page-two pairs remain distinct',pairs.length===2);
        var pair=pairs[0];pair.top.parentStory.contents='edited before first tick';
        sync.tick(doc);pair=sync.pairs(doc)[0];
        check('first tick repairs stale contents',String(pair.bottom.parentStory.contents)===String(pair.top.parentStory.contents));
        pair.top.parentStory.texts[0].pointSize=27;
        var updated=sync.resyncAll(doc);report.resync=updated;
        pairs=sync.pairs(doc);pair=pairs[0];
        check('explicit resync includes formatting-only edit',updated.resynced===2 && pair.bottom.parentStory.texts[0].pointSize===27);
        check('top has genuinely empty stroke color',String(pair.top.parentStory.texts[0].strokeColor.name)==='None');
        check('top empty stroke reports zero weight',pair.top.parentStory.texts[0].strokeWeight===0);
        check('bottom fill matches stroke',String(pair.bottom.parentStory.texts[0].fillColor.name)===String(pair.bottom.parentStory.texts[0].strokeColor.name));
        check('replacement stays behind fill',pair.bottom.index>pair.top.index);
        var duplicate=pair.top.duplicate();
        var ambiguous=sync.inspect(doc);
        check('duplicate token refused without geometry guessing',ambiguous.pairs.length===1&&ambiguous.skipped.length>0);
        duplicate.remove();
        var label=pair.top.extractLabel('ISE_SPLIT');pair.top.insertLabel('ISE_SPLIT','fill|legacy');
        var legacy=sync.inspect(doc);check('legacy labels refused',legacy.pairs.length===1&&legacy.skipped.length>0);
        pair.top.insertLabel('ISE_SPLIT',label);
        var beforeLabels=[];pairs=sync.pairs(doc);
        for(n=0;n<pairs.length;n++){beforeLabels.push(String(pairs[n].top.extractLabel('ISE_SPLIT')));}
        beforeLabels.sort();
        doc.exportFile(ExportFormat.INDESIGN_MARKUP,idml,false);
        var imported=app.open(idml,false);owned.push(imported);
        var importedPairs=sync.pairs(imported),afterLabels=[];
        var onPageTwo=true,emptyStroke=true;
        for(n=0;n<importedPairs.length;n++){afterLabels.push(String(importedPairs[n].top.extractLabel('ISE_SPLIT')));}
        for(n=0;n<importedPairs.length;n++){
            onPageTwo=onPageTwo&&importedPairs[n].top.parentPage.id===imported.pages[1].id&&importedPairs[n].bottom.parentPage.id===imported.pages[1].id;
            var topText=importedPairs[n].top.parentStory.texts[0];
            emptyStroke=emptyStroke&&String(topText.strokeColor.name)==='None'&&topText.strokeWeight===0;
        }
        afterLabels.sort();
        check('IDML retains pair tokens on page two',importedPairs.length===2&&onPageTwo&&beforeLabels.join(';')===afterLabels.join(';'));
        check('IDML retains None and zero-weight top strokes',importedPairs.length===2&&emptyStroke);
        if(importedPairs.length!==2){throw new Error('Pair discovery failed after IDML.');}
        importedPairs[0].top.parentStory.contents='edited after IDML';
        var roundtrip=sync.resyncAll(imported);report.roundtrip=roundtrip;
        check('resync works after IDML import',roundtrip.resynced===2 && sync.pairs(imported)[0].bottom.parentStory.contents==='edited after IDML');
    }catch(e){error=e.message+' line '+e.line;}
    finally{
        for(n=owned.length-1;n>=0;n--){try{if(owned[n].isValid){owned[n].close(SaveOptions.NO);}}catch(cleanup){error='cleanup: '+cleanup.message;}}
        app.scriptPreferences.userInteractionLevel=oldUI;
        $.global.ISE_SYNC=savedSync;$.global.ISE_SPLIT=savedSplit;$.global.ISE_LIB=savedLib;
        $.global.ISE_SYNC_COUNT=savedCount;$.global.ISE_SYNC_LAST=savedLast;
    }
    check('document count preserved',app.documents.length===initial.length);
    for(n=0;n<initial.length;n++){check('user document '+n+' preserved',state(initial[n].doc)===initial[n].state);}
    check('UI restored',app.scriptPreferences.userInteractionLevel===oldUI);
    report.error=error;report.passed=!error;
    for(n=0;n<checks.length;n++){if(!checks[n].passed){report.passed=false;}}
    if(!lib){throw new Error(error||'Library unavailable');}
    return lib.toJSON(report);
})();
