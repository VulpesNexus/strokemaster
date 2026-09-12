// Measure stroke-state assignment order on disposable text, without claiming zero works.
(function(){
    var here=File($.fileName).parent,doc=null,oldLib=$.global.ISE_LIB,rows=[],error=null,initial=app.documents.length;
    $.evalFile(new File(here.fsName+'/ise-lib.jsx'));var lib=$.global.ISE_LIB;
    try{
        doc=app.documents.add(false);var none=doc.swatches.itemByName('None'),black=doc.swatches.itemByName('Black');
        var methods=[
            function(t){t.strokeColor=none;},
            function(t){t.strokeColor=none;t.strokeWeight=0;},
            function(t){t.strokeWeight=0;t.strokeColor=none;},
            function(t){t.properties={strokeWeight:0,strokeColor:none};},
            function(t){t.properties={strokeColor:none,strokeWeight:0};},
            function(t){t.strokeWeight=0.001;t.strokeColor=none;}
        ];
        for(var i=0;i<methods.length*4;i++){
            var f=doc.pages[0].textFrames.add({geometricBounds:[10+i*15,10,24+i*15,100]});f.contents='H';
            var t=f.parentStory.texts[0];t.strokeColor=black;t.strokeWeight=3;
            t.strokeAlignment=TextStrokeAlign.OUTSIDE_ALIGNMENT;t.appliedFont='Minion Pro\tRegular';
            var target=Math.floor(i/methods.length);
            if(target===1){t=f.parentStory.characters.itemByRange(0,0);}
            if(target===2){t=f.parentStory.textStyleRanges[0];}
            if(target===3){t=f.parentStory.characters[0];}
            var row={method:i%methods.length,target:target,type:t.constructor.name};try{methods[i%methods.length](t);}catch(e){row.error=e.message;}
            var resolved=target===1?t.getElements()[0]:t;
            row.color=String(resolved.strokeColor.name);row.weight=resolved.strokeWeight;rows.push(row);
        }
    }catch(e2){error=e2.message;}
    finally{if(doc&&doc.isValid){doc.close(SaveOptions.NO);}$.global.ISE_LIB=oldLib;}
    return lib.toJSON({version:app.version,passed:!error&&app.documents.length===initial,error:error,rows:rows});
})();
