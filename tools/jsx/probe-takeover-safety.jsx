// Host preservation tests. Mutates only documents created by this probe.
// args.root must be a fresh existing test directory. No user document is closed.
(function () {
    var args=$.global.ISE_PROBE_ARGS || {}, here=File($.fileName).parent;
    if(!args.root){throw new Error('Need a test root.');}
    var target=new File(args.root+'/minimal/conflict.indd');
    if(target.exists){throw new Error('Need a fresh test root; refusing overwrite.');}
    new Folder(args.root+'/minimal').create();
    var oldUI=app.scriptPreferences.userInteractionLevel, docs=[], initial=[], checks=[], error=null;
    function state(d){return [d.id,d.saved,d.modified,d.allPageItems.length].join('|');}
    for(var i=0;i<app.documents.length;i++){initial.push({doc:app.documents[i],value:state(app.documents[i])});}
    function check(n,p){checks.push({name:n,passed:Boolean(p)});}
    var report={version:app.version,checks:checks}, lib=null;
    try {
        app.scriptPreferences.userInteractionLevel=UserInteractionLevels.NEVER_INTERACT;
        var other=app.documents.add(false);docs.push(other);
        var tf=other.pages[0].textFrames.add({geometricBounds:[10,10,100,200]});tf.contents='saved sentinel';
        other.save(target);tf.contents='unsaved sentinel';
        var fresh=app.documents.add(false);docs.push(fresh);
        $.evalFile(new File(here.fsName+'/ise-lib.jsx'));lib=$.global.ISE_LIB;
        lib.manifest={frames:[]};
        var refused=false;
        try{lib.saveExport(fresh,args.root,'conflict');}catch(expected){refused=String(expected.message).indexOf('Refusing')>=0;}
        check('open output conflict refused',refused);
        check('modified open document remains valid',other.isValid && other.modified);
        check('unsaved sentinel preserved',tf.contents==='unsaved sentinel');
        check('no PDF after conflict',!new File(args.root+'/actual/conflict.pdf').exists);
        report.preferenceLog=lib.run(function(l){
            var denied=false;try{l.track(other);}catch(e){denied=true;}
            check('cannot take ownership of already open document',denied);
            throw new Error('injected build error');
        });
        check('conflicting document survives failure cleanup',other.isValid && tf.contents==='unsaved sentinel');
        // Explicit arguments avoid dialogs; this also compiles the real entry point in host.
        $.global.ISE_ARGS={command:'status'};
        var first=$.evalFile(new File(here.fsName+'/strokemaster.jsx'));
        $.global.ISE_ARGS={command:'status'};
        var second=$.evalFile(new File(here.fsName+'/strokemaster.jsx'));
        check('repeated headless invocation returns result',Boolean(first) && Boolean(second));
        check('invocation globals cleaned',!$.global.ISE_ARGS && !$.global.ISE_LIB);
    } catch(e){error=e.message+' line '+e.line;}
    finally {
        for(var d=docs.length-1;d>=0;d--){try{if(docs[d].isValid){docs[d].close(SaveOptions.NO);}}catch(cleanup){error='cleanup: '+cleanup.message;}}
        app.scriptPreferences.userInteractionLevel=oldUI;
        try{delete $.global.ISE_LIB;delete $.global.ISE_ARGS;}catch(e2){}
    }
    check('document count preserved',app.documents.length===initial.length);
    for(i=0;i<initial.length;i++){check('user document '+i+' preserved',state(initial[i].doc)===initial[i].value);}
    check('UI preference restored',app.scriptPreferences.userInteractionLevel===oldUI);
    report.error=error;report.passed=!error;
    for(i=0;i<checks.length;i++){if(!checks[i].passed){report.passed=false;}}
    // lib's serializer is local even after the entry point cleans its globals.
    if(!lib){throw new Error(error || 'Library unavailable');}
    return lib.toJSON(report);
})();
