// Native smoke/lifecycle proof. No miter clipping or render-fidelity certification.
// Runs only when explicitly evaluated by the supervisor in Illustrator.
// Optional ISE_AI_ARGS: outDir + runId (new files only), lifecycle, diagnostics.
// No app quit, plugin installation, menu guessing, or edits to existing documents.
(function () {
    var identity='StrokeMasterProposedProbe', args=$.global.ISE_AI_ARGS || {};
    var count=app.documents.length, prior=count?app.activeDocument:null, doc=null, rows=[], initial=[];
    var oldUI=app.userInteractionLevel, journal=null, sequence=0, outputs={}, modeName='', failed=false, lastCounters=null;
    var report={schema:2, status:'PROPOSED', nativeExecuted:false, host:app.name, version:app.version,
        build:String(app.buildNumber), checks:rows, stages:[], events:[], files:[],
        nativeStages:{HOST_DISCOVERS:'NOT_MEASURED', HOST_LOADS:'NOT_MEASURED', INITIALIZES:'NOT_MEASURED',
            REGISTERS:'NOT_MEASURED', CALLBACK_EXECUTES:'NOT_MEASURED', TEST_ART_REACHES_CALLBACK:'NOT_MEASURED',
            HOST_CLOSES_CLEANLY:'NOT_MEASURED'},
        limitations:['Session-wide counters require quiet unrelated documents.',
            'PDF file creation is not proof of rendered geometry or missing-plugin fallback.']};
    function quote(s){return '"'+String(s).replace(/\\/g,'\\\\').replace(/"/g,'\\"').replace(/[\u0000-\u001f]/g,function(c){return '\\u'+('0000'+c.charCodeAt(0).toString(16)).slice(-4);})+'"';}
    function json(v){
        if(v===null||v===undefined){return 'null';}
        if(typeof v==='string'){return quote(v);}
        if(typeof v==='number'){if(!isFinite(v)){throw Error('Nonfinite report value');}return String(v);}
        if(typeof v==='boolean'){return String(v);}
        var a=[],i;if(v instanceof Array){for(i=0;i<v.length;i++){a.push(json(v[i]));}return '['+a.join(',')+']';}
        for(i in v){if(v.hasOwnProperty(i)){a.push(quote(i)+':'+json(v[i]));}}return '{'+a.join(',')+'}';
    }
    function event(stage,status,data){
        var e={sequence:++sequence,stage:stage,status:status,data:data||null};
        report.events.push(e);
        if(journal){
            if(!journal.open('a')){throw Error('Cannot append stage journal: '+journal.error);}
            try{if(journal.writeln(json(e))===false){throw Error('Stage journal write failed');}}
            finally{if(journal.close()===false){throw Error('Stage journal close failed');}}
        }
    }
    function check(name,ok){
        rows.push({name:name,passed:!!ok});
        if(!ok){throw Error(name);}
    }
    function integer(text){
        var value=Number(text);
        if(!isFinite(value)||Math.abs(value)>9007199254740991||Math.floor(value)!==value){throw Error('Unsafe protocol integer');}
        return value;
    }
    function call(selector,input,interval){
        event('bridge.'+selector,'BEGIN',{input:input});
        var raw=app.sendScriptMessage(identity,selector,input);
        var m=/^\{"schema":1,"status":"PROPOSED","plugin":"StrokeMasterProposedProbe","ok":(true|false),"error":(-?\d+),"goCalls":(\d+),"mutations":(\d+),"strokeOrigin":([0-3]),"lastGoError":(-?\d+),"effectFixtures":(\d+),"controlFixtures":(\d+)\}$/.exec(raw);
        check('strict native identity/schema for '+selector,!!m);
        var p={schema:1,status:'PROPOSED',plugin:identity,ok:m[1]==='true',error:integer(m[2]),
            goCalls:integer(m[3]),mutations:integer(m[4]),strokeOrigin:integer(m[5]),lastGoError:integer(m[6]),
            effectFixtures:integer(m[7]),controlFixtures:integer(m[8])};
        report.nativeExecuted=true;
        report.lastNativeStatus=p;
        // Keep the attempted edit's counters even when interval validation rejects it.
        if(interval&&/\.edit$/.test(interval)){report.edited=p;}
        check('native counters consistent',p.mutations<=p.goCalls);
        if(lastCounters){
            if(p.goCalls>lastCounters.goCalls){report.nativeStages.CALLBACK_EXECUTES='OBSERVED_COUNTER_DELTA';}
            check((interval||selector)+' counters monotonic',p.goCalls>=lastCounters.goCalls&&p.mutations>=lastCounters.mutations);
            check((interval||selector)+' failed callback count unchanged',p.goCalls-p.mutations===lastCounters.goCalls-lastCounters.mutations);
        }
        lastCounters=p;
        check('native action succeeded '+selector,p.ok&&p.error===0);
        event('bridge.'+selector,'PASS',p);
        return p;
    }
    function diagnostics(){
        var raw=app.sendScriptMessage(identity,'diagnostics','');
        // Persist the returned payload before validation and before owned-doc close.
        report.lastNativeDiagnosticsRaw=String(raw);
        event('native.diagnostics.raw','RECEIVED',{payload:String(raw)});
        var h=/^\{"schema":2,"plugin":"StrokeMasterProposedProbe","build":"([A-Za-z0-9_-]+)","processId":(\d+),"effectRegistered":(true|false),"commandRegistered":(true|false),"acceptedInputs":(\d+),"sequence":(\d+),"events":\[(.*)\]\}$/.exec(raw);
        check('native diagnostics schema',!!h);
        var d={build:h[1],processId:integer(h[2]),effectRegistered:h[3]==='true',commandRegistered:h[4]==='true',
            acceptedInputs:integer(h[5]),sequence:integer(h[6]),events:[]}, tail=h[7];
        check('native process ID is a positive DWORD',d.processId>0&&d.processId<=4294967295);
        if(args.expectedHostPid!==undefined){check('native process matches external process',d.processId===args.expectedHostPid);}
        if(report.nativeBefore){check('native process unchanged',d.processId===report.nativeBefore.processId);}
        while(tail.length){
            var e=/^\{"sequence":(\d+),"stage":"([^"\\\x00-\x1f]*)","detail":"([^"\\\x00-\x1f]*)","version":(-?\d+),"error":(-?\d+)\}/.exec(tail);
            check('native event schema',!!e);
            d.events.push({sequence:integer(e[1]),stage:e[2],detail:e[3],version:integer(e[4]),error:integer(e[5])});
            tail=tail.substr(e[0].length);
            if(tail.length){check('native event separator',tail.charAt(0)===','&&tail.length>1);tail=tail.substr(1);}
            check('bounded native event count',d.events.length<=32);
        }
        check('expected native diagnostic build',d.build===(args.requireBuild||'native-prep-4'));
        check('native effect and command registered',d.effectRegistered&&d.commandRegistered);
        event('native.diagnostics','PASS',d);
        report.nativeStages.INITIALIZES='OBSERVED_STATE';
        report.nativeStages.REGISTERS='OBSERVED_HANDLES';
        return d;
    }
    function signature(path){
        var values=[path.closed,path.pathPoints.length];
        for(var i=0;i<path.pathPoints.length;i++){
            var p=path.pathPoints[i];values.push(String(p.anchor),String(p.leftDirection),String(p.rightDirection));
        }
        return values.join('|');
    }
    function source(){
        check(modeName+' has one editable source',doc.pathItems.length===1);
        var p=doc.pathItems[0];check(modeName+' source retains two open anchors',p.pathPoints.length===2&&!p.closed);
        return p;
    }
    function documentState(d){
        return {ref:d,name:String(d.name),saved:d.saved,paths:d.pathItems.length};
    }
    function owns(d){
        for(var i=0;i<initial.length;i++){if(initial[i].ref===d){return false;}}
        return !!d;
    }
    function closeOwned(){
        if(!doc){return;}
        check('close target is an owned specimen',owns(doc));
        // A broken journal must never prevent cleanup of a specimen we own.
        var logError=null;
        try{event(modeName+'.close','BEGIN');}catch(e){logError=e;}
        doc.close(SaveOptions.DONOTSAVECHANGES);
        doc=null;
        if(logError){throw logError;}
        event(modeName+'.close','PASS');
    }
    function configureOutput(){
        if(args.lifecycle&&!args.outDir){throw Error('Lifecycle requires outDir and runId');}
        if(!args.outDir){return;}
        check('safe unique runId',typeof args.runId==='string'&&/^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$/.test(args.runId));
        var base=new Folder(args.outDir);check('existing output directory',base.exists);
        var names=['stages.jsonl','control.ai','effect.ai','control.pdf','effect.pdf'];
        for(var i=0;i<names.length;i++){
            var f=new File(base.fsName+'/'+args.runId+'-'+names[i]);
            check('new output path '+names[i],!f.exists);outputs[names[i]]=f;
        }
        journal=outputs['stages.jsonl'];
        // Reserve only this run's journal. Never overwrite a preexisting path.
        if(!journal.open('w')){throw Error('Cannot create stage journal');}
        if(journal.close()===false){throw Error('Cannot close stage journal');}
        report.files.push({kind:'journal',path:journal.fsName});
    }
    function save(kind){
        var file=outputs[modeName+'.'+kind];
        check('output remains new '+kind,!file.exists);
        event(modeName+'.save.'+kind,'BEGIN',{path:file.fsName});
        var options=kind==='ai'?new IllustratorSaveOptions():new PDFSaveOptions();
        if(kind==='ai'){options.pdfCompatible=true;}else{options.preserveEditability=false;}
        doc.saveAs(file,options);
        call('status','',modeName+'.save.'+kind);
        check('nonempty saved '+kind,file.exists&&file.length>0);
        report.files.push({kind:kind,mode:modeName,path:file.fsName,bytes:file.length});
        event(modeName+'.save.'+kind,'PASS',{path:file.fsName,bytes:file.length});
    }
    function redraw(label){
        event(label,'BEGIN');app.redraw();var status=call('status','',label);event(label,'PASS',status);return status;
    }
    function dispatch(before,after,label){
        check(label+' callback advanced',after.goCalls>before.goCalls);
        report.nativeStages.CALLBACK_EXECUTES='OBSERVED_COUNTER_DELTA';
        check(label+' replacement succeeded',after.mutations>before.mutations&&after.lastGoError===0&&after.strokeOrigin>0);
        report.nativeStages.TEST_ART_REACHES_CALLBACK='OBSERVED_SUCCESS_AND_ORIGIN';
    }
    function edit(before,label){
        var p=source(), a=p.pathPoints[1].anchor;
        p.pathPoints[1].anchor=[a[0]+20,a[1]];
        p.pathPoints[1].leftDirection=p.pathPoints[1].anchor;p.pathPoints[1].rightDirection=p.pathPoints[1].anchor;
        var after=redraw(label);
        report.edited=after;dispatch(before,after,label);source();return after;
    }
    try{
        for(var i=0;i<count;i++){initial.push(documentState(app.documents[i]));}
        configureOutput();
        event('proof','BEGIN',{lifecycle:!!args.lifecycle});
        var baseline=call('status','');report.baseline=baseline;
        report.nativeStages.HOST_LOADS='OBSERVED_BRIDGE_RESPONSE';
        if(args.diagnostics||args.lifecycle){report.nativeBefore=diagnostics();}
        app.userInteractionLevel=UserInteractionLevel.DONTDISPLAYALERTS;
        for(var mode=0;mode<2;mode++){
            modeName=mode?'effect':'control';
            event(modeName+'.createDocument','BEGIN');
            var created=app.documents.add(DocumentColorSpace.RGB,300,300);
            check('new document is not preexisting',owns(created));doc=created;
            call('status','',modeName+'.createDocument');
            event(modeName+'.createDocument','PASS');
            var before=call('status',''), create=call('create',modeName), after=redraw(modeName+'.redraw');
            report.stages.push({mode:modeName,before:before,after:after,create:create});
            source();
            if(mode){dispatch(before,after,'effect');after=edit(after,'effect.edit');}
            else{check('control has no replacement calls or mutations',after.goCalls===before.goCalls&&after.mutations===before.mutations);}
            if(args.lifecycle){
                var savedGeometry=signature(source());save('ai');closeOwned();
                call('status','',modeName+'.close.ai');
                event(modeName+'.reopen','BEGIN',{path:outputs[modeName+'.ai'].fsName});
                var opened=app.open(outputs[modeName+'.ai']);
                check('reopened fixture is not preexisting',owns(opened));doc=opened;
                call('status','',modeName+'.reopen');
                check('reopened expected owned file',doc.fullName.fsName===outputs[modeName+'.ai'].fsName);
                check('reopened source geometry preserved',signature(source())===savedGeometry);
                var reopened=redraw(modeName+'.reopen.redraw');
                event(modeName+'.reopen','PASS',{geometry:savedGeometry});
                if(mode){after=edit(reopened,'effect.reopen.edit');}
                else{check('reopened control has no replacement',reopened.goCalls===after.goCalls&&reopened.mutations===after.mutations);}
            }
            if(args.outDir){
                save('pdf');
                var exported=call('status','');
                check('PDF export has no new failed callback',exported.goCalls===after.goCalls||exported.lastGoError===0);
                if(!mode){check('control export does not dispatch',exported.goCalls===after.goCalls&&exported.mutations===after.mutations);}
                event(modeName+'.export','FILE_WRITTEN_NOT_RENDER_VERIFIED',exported);
            }
            closeOwned();
            call('status','',modeName+'.close');
        }
        if(args.diagnostics||args.lifecycle){report.nativeAfter=diagnostics();}
        report.status=args.lifecycle?'LIFECYCLE_OBSERVED_ONLY':'DISPATCH_OBSERVED_ONLY';
    }catch(e){
        failed=true;report.error=String(e);
        // Read the bounded native ring before cleanup or further bridge calls evict it.
        if(report.nativeExecuted&&(args.diagnostics||args.lifecycle)){
            try{report.nativeFailure=diagnostics();}catch(diagError){report.diagnosticsError=String(diagError);}
        }
        try{event('proof','FAIL',{error:report.error});}catch(logError){report.journalError=String(logError);}
    }finally{
        try{closeOwned();}catch(cleanup){failed=true;report.cleanupError=String(cleanup);}
        try{app.userInteractionLevel=oldUI;if(prior){prior.activate();}}catch(restore){failed=true;report.restoreError=String(restore);}
        try{
            check('document count preserved',app.documents.length===count);
            for(var j=0;j<initial.length;j++){
                var old=initial[j], now=documentState(old.ref);
                check('preexisting document preserved '+j,now.name===old.name&&now.saved===old.saved&&now.paths===old.paths);
            }
            check('UI level restored',app.userInteractionLevel===oldUI);
        }catch(preservation){failed=true;report.preservationError=String(preservation);}
        if(report.nativeExecuted){
            try{call('status','','final cleanup and restore');}catch(counterError){failed=true;report.cleanupCounterError=String(counterError);}
        }
    }
    report.passed=!failed;
    if(failed){report.status='PROPOSED';}
    try{event('proof',failed?'FAILED':'COMPLETE',{status:report.status});}catch(finalLog){report.passed=false;report.status='PROPOSED';report.journalError=String(finalLog);}
    return json(report);
})();
