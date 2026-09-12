// Native dispatch/ownership test. Requires the separately built diagnostic plugin loaded.
// Creates only disposable documents. Does not implement or certify miter clipping.
(function () {
    var identity='StrokeMasterProposedProbe', args=$.global.ISE_AI_ARGS || {};
    var count=app.documents.length, prior=count?app.activeDocument:null, doc=null, error=null, rows=[];
    var oldUI=app.userInteractionLevel;
    function call(selector,input){return eval('('+app.sendScriptMessage(identity,selector,input)+')');}
    function check(name,ok){rows.push({name:name,passed:Boolean(ok)});}
    function quote(s){return '"'+String(s).replace(/\\/g,'\\\\').replace(/"/g,'\\"').replace(/\r/g,'\\r').replace(/\n/g,'\\n')+'"';}
    function json(v){
        if(v===null||v===undefined){return 'null';}
        if(typeof v==='string'){return quote(v);}
        if(typeof v==='number'||typeof v==='boolean'){return String(v);}
        var a=[],i;if(v instanceof Array){for(i=0;i<v.length;i++){a.push(json(v[i]));}return '['+a.join(',')+']';}
        for(i in v){if(v.hasOwnProperty(i)){a.push(quote(i)+':'+json(v[i]));}}return '{'+a.join(',')+'}';
    }
    var report={host:app.name,version:app.version,build:String(app.buildNumber),checks:rows,stages:[]};
    try {
        var baseline=call('status','');report.baseline=baseline;
        check('loaded expected protocol',baseline.schema===1 && baseline.plugin===identity && baseline.ok);
        if(baseline.schema!==1 || baseline.plugin!==identity || baseline.ok!==true){throw new Error('Native protocol identity/status failed.');}
        app.userInteractionLevel=UserInteractionLevel.DONTDISPLAYALERTS;
        for(var mode=0;mode<2;mode++){
            doc=app.documents.add(DocumentColorSpace.RGB,300,300);
            var before=call('status',''), create=call('create',mode?'effect':'control');
            check((mode?'effect':'control')+' command',create.ok);
            if(!create.ok){throw new Error('Native create failed: '+create.error);}
            app.redraw();
            var after=call('status','');
            check('source has one editable path '+mode,doc.pathItems.length===1);
            var path=doc.pathItems[0];
            check('source retains two anchors '+mode,path.pathPoints.length===2 && !path.closed);
            if(mode){
                check('effect dispatched',after.goCalls>before.goCalls);
                check('replacement succeeded',after.mutations>before.mutations && after.lastGoError===0);
                path.pathPoints[1].anchor=[240,100];
                path.pathPoints[1].leftDirection=[240,100];path.pathPoints[1].rightDirection=[240,100];
                app.redraw();
                var edited=call('status','');
                check('anchor edit dispatches again',edited.goCalls>after.goCalls && edited.mutations>after.mutations && edited.lastGoError===0);
                report.edited=edited;
            } else {check('control does not dispatch replacement',after.goCalls===before.goCalls);}
            report.stages.push({mode:mode?'effect':'control',before:before,after:after});
            if(args.outDir){
                var file=new File(args.outDir+'/'+(mode?'effect':'control')+'.pdf');
                if(file.exists || !file.parent.exists){throw new Error('Need new PDF paths in existing output directory.');}
                var options=new PDFSaveOptions();options.preserveEditability=false;
                doc.saveAs(file,options);
            }
            doc.close(SaveOptions.DONOTSAVECHANGES);doc=null;
        }
    }catch(e){error=e.message+' line '+e.line;}
    finally{
        try{if(doc){doc.close(SaveOptions.DONOTSAVECHANGES);}}catch(cleanup){error='cleanup: '+cleanup.message;}
        app.userInteractionLevel=oldUI;if(prior){prior.activate();}
    }
    check('document count preserved',app.documents.length===count);
    report.error=error;report.passed=!error;
    for(var j=0;j<rows.length;j++){if(!rows[j].passed){report.passed=false;}}
    return json(report);
})();
