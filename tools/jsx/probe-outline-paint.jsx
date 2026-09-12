// Read and assign overprint state on ordinary page-item paint in a disposable document.
(function(){
    var here=File($.fileName).parent,oldLib=$.global.ISE_LIB,doc=null,rows=[],count=app.documents.length,error=null;
    $.evalFile(new File(here.fsName+'/ise-lib.jsx'));var lib=$.global.ISE_LIB;
    function read(item,key){try{return item[key];}catch(e){return 'ERROR: '+e.message;}}
    try{
        doc=app.documents.add(false);
        var colors=[doc.swatches.itemByName('Black'),doc.swatches.itemByName('Paper'),doc.swatches.itemByName('None'),
            doc.colors.add({name:'probe magenta',model:ColorModel.PROCESS,space:ColorSpace.CMYK,colorValue:[0,100,0,0]})];
        for(var i=0;i<colors.length;i++){
            var p=doc.pages[0].polygons.add();p.paths[0].entirePath=[[10,10],[50,10],[50,50],[10,50]];
            p.fillColor=colors[i];p.strokeColor=colors[i];p.strokeWeight=6;
            var row={color:String(colors[i].name),beforeFill:read(p,'overprintFill'),beforeStroke:read(p,'overprintStroke')};
            try{p.overprintStroke=false;row.setFalseStroke=true;}catch(e){row.setFalseStroke=e.message;}
            try{p.overprintFill=false;row.setFalseFill=true;}catch(e2){row.setFalseFill=e2.message;}
            row.afterFill=read(p,'overprintFill');row.afterStroke=read(p,'overprintStroke');rows.push(row);
        }
    }catch(e3){error=e3.message;}
    finally{if(doc&&doc.isValid){doc.close(SaveOptions.NO);}$.global.ISE_LIB=oldLib;}
    return lib.toJSON({version:app.version,passed:!error&&app.documents.length===count,error:error,rows:rows});
})();
