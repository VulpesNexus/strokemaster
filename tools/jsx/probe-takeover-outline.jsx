// Outline staging, page placement, and refusal checks in an owned hidden document.
(function(){
    var here=File($.fileName).parent,oldUI=app.scriptPreferences.userInteractionLevel;
    var oldLib=$.global.ISE_LIB,oldFaithful=$.global.ISE_FAITHFUL,doc=null,initial=[],checks=[],notes=[],error=null,lib=null;
    function state(d){return [d.id,d.saved,d.modified,d.allPageItems.length].join('|');}
    for(var i=0;i<app.documents.length;i++){initial.push({doc:app.documents[i],state:state(app.documents[i])});}
    function check(name,ok){checks.push({name:name,passed:Boolean(ok)});}
    function make(page,name){
        var f=page.textFrames.add({geometricBounds:[20,20,90,150]});f.name=name;f.contents='HH';
        f.fillColor=doc.swatches.itemByName('None');f.strokeColor=doc.swatches.itemByName('None');
        var t=f.parentStory.texts[0];t.appliedFont='Minion Pro\tRegular';t.pointSize=24;
        t.fillColor=doc.swatches.itemByName('Black');t.strokeColor=doc.swatches.itemByName('Paper');
        t.strokeWeight=3;t.strokeAlignment=TextStrokeAlign.OUTSIDE_ALIGNMENT;t.endJoin=EndJoin.ROUND_END_JOIN;
        return f;
    }
    var report={version:app.version,checks:checks,notes:notes};
    try{
        app.scriptPreferences.userInteractionLevel=UserInteractionLevels.NEVER_INTERACT;
        $.evalFile(new File(here.fsName+'/ise-lib.jsx'));lib=$.global.ISE_LIB;
        $.evalFile(new File(here.fsName+'/ise-faithful-outlines.jsx'));var faithful=$.global.ISE_FAITHFUL;
        doc=app.documents.add(false);doc.documentPreferences.pagesPerDocument=2;
        var page=doc.pages[1],source=make(page,'source'),before=doc.allPageItems.length;
        check('ordinary page-two source accepted',faithful.canConvert(source));
        var caught=false;
        try{faithful.convertFrame(doc,source,function(s){notes.push(s);throw new Error('injected logger failure');});}catch(expected){caught=String(expected).indexOf('injected logger failure')>=0;notes.push(String(expected));}
        check('failure injected after staging',caught);
        check('source text preserved after staged failure',source.isValid&&source.contents==='HH');
        check('staged failure removes only temporary art',doc.allPageItems.length===before);
        if(!caught){throw new Error('Could not reach staged failure point; inspect refusal notes.');}
        var result=faithful.convertFrame(doc,source,function(s){notes.push(s);});
        check('conversion returns a group',result&&result.isValid&&result.constructor.name==='Group');
        if(!result){throw new Error('Supported conversion refused.');}
        check('original replaced only after validation',!source.isValid);
        check('outlines remain on page two',result.parentPage.id===page.id);
        var leaves=result.pageItems.everyItem().getElements(),fill=0,stroke=0;
        for(i=0;i<leaves.length;i++){
            if(leaves[i].name==='fill'&&String(leaves[i].strokeColor.name)==='None'){fill++;}
            if(leaves[i].name==='stroke'&&leaves[i].strokeWeight===6&&String(leaves[i].fillColor.name)==='None'){stroke++;}
        }
        check('painted width and empty fill/stroke roles preserved',fill>0&&fill===stroke&&fill+stroke===leaves.length);
        var unsupported=make(page,'background');unsupported.fillColor=doc.swatches.itemByName('Paper');
        before=doc.allPageItems.length;
        check('background frame refused',!faithful.canConvert(unsupported)&&faithful.convertFrame(doc,unsupported,function(s){notes.push(s);})===null);
        check('refusal preserves source and item count',unsupported.isValid&&unsupported.contents==='HH'&&doc.allPageItems.length===before);
    }catch(e){error=e.message+' line '+e.line;}
    finally{
        try{if(doc&&doc.isValid){doc.close(SaveOptions.NO);}}catch(cleanup){error='cleanup: '+cleanup.message;}
        app.scriptPreferences.userInteractionLevel=oldUI;$.global.ISE_LIB=oldLib;$.global.ISE_FAITHFUL=oldFaithful;
    }
    check('document count preserved',app.documents.length===initial.length);
    for(i=0;i<initial.length;i++){check('user document '+i+' preserved',state(initial[i].doc)===initial[i].state);}
    check('UI restored',app.scriptPreferences.userInteractionLevel===oldUI);
    report.error=error;report.passed=!error;
    for(i=0;i<checks.length;i++){if(!checks[i].passed){report.passed=false;}}
    if(!lib){throw new Error(error||'Library unavailable');}
    return lib.toJSON(report);
})();
