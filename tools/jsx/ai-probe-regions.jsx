// Diagnostic P1/P2/P3 probe. All constructed art belongs to disposable documents.
// Existence of a menu command is not a passing test: inspect actual union geometry.
(function () {
    var prior = app.documents.length ? app.activeDocument : null;
    var count = app.documents.length, doc = null, checks = [], error = null;
    var oldUI = app.userInteractionLevel;
    function check(name, pass, value) { checks.push({name:name, passed:Boolean(pass), value:String(value)}); }
    function quote(s) { return '"' + String(s).replace(/\\/g,'\\\\').replace(/"/g,'\\"').replace(/\n/g,'\\n').replace(/\r/g,'\\r') + '"'; }
    function json(v) {
        if (v === null) { return 'null'; }
        if (typeof v === 'string') { return quote(v); }
        if (typeof v === 'number' || typeof v === 'boolean') { return String(v); }
        var a=[], k;
        if (v instanceof Array) { for(k=0;k<v.length;k++){a.push(json(v[k]));} return '['+a.join(',')+']'; }
        for(k in v){if(v.hasOwnProperty(k)){a.push(quote(k)+':'+json(v[k]));}}return '{'+a.join(',')+'}';
    }
    function poly(points) {
        var p=doc.pathItems.add();p.setEntirePath(points);p.closed=true;p.filled=true;p.stroked=false;return p;
    }
    function area(item) {
        if(item.typename==='PathItem'){return Number(item.area);}
        var s=0, items=item.typename==='CompoundPathItem'?item.pathItems:item.pageItems;
        for(var i=0;i<items.length;i++){s+=area(items[i]);}return s;
    }
    var sourceSaved = prior ? prior.saved : null, sourceItems = prior ? prior.pageItems.length : null;
    try {
        app.userInteractionLevel=UserInteractionLevel.DONTDISPLAYALERTS;
        doc=app.documents.add(DocumentColorSpace.RGB,300,300);
        var p=poly([[0,0],[100,0],[100,100],[0,100]]);
        p.stroked=true;p.strokeWidth=16;p.strokeJoin=StrokeJoin.MITERENDJOIN;p.strokeMiterLimit=2;
        p.strokeCap=StrokeCap.BUTTENDCAP;
        check('P1 basic width',p.strokeWidth===16,p.strokeWidth);
        check('P1 basic limit',p.strokeMiterLimit===2,p.strokeMiterLimit);
        check('P1 basic join',p.strokeJoin===StrokeJoin.MITERENDJOIN,p.strokeJoin);
        p.remove();check('P2 cleanup',doc.pageItems.length===0,doc.pageItems.length);
        var curve=poly([[0,0],[100,0],[100,100]]);
        curve.pathPoints[0].rightDirection=[20,40];
        check('P2 Bezier handle',curve.pathPoints[0].rightDirection[1]===40,curve.pathPoints[0].rightDirection);
        curve.remove();
        var a=poly([[0,0],[100,0],[100,100],[0,100]]);
        var b=poly([[50,0],[150,0],[150,100],[50,100]]);
        doc.selection=null;a.selected=true;b.selected=true;
        app.executeMenuCommand('group');
        app.executeMenuCommand('Live Pathfinder Add');
        app.executeMenuCommand('expandStyle');
        var selection=doc.selection, united=0;
        for(var i=0;i<selection.length;i++){united+=area(selection[i]);}
        check('P3 actual Unite area',Math.abs(Math.abs(united)-15000)<0.001,united);
        // A hole must remain a hole in a compound path; no claim about Boolean holes yet.
        doc.close(SaveOptions.DONOTSAVECHANGES);doc=null;
        doc=app.documents.add(DocumentColorSpace.RGB,300,300);
        var compound=doc.compoundPathItems.add();
        var outer=compound.pathItems.add();outer.setEntirePath([[0,0],[100,0],[100,100],[0,100]]);outer.closed=true;
        var hole=compound.pathItems.add();hole.setEntirePath([[25,25],[25,75],[75,75],[75,25]]);hole.closed=true;
        check('P2 hole signed area',Math.abs(Math.abs(area(compound))-7500)<0.001,area(compound));
        compound.remove();check('P2 compound cleanup',doc.pageItems.length===0,doc.pageItems.length);
    } catch(e){error=e.message+' line '+e.line;}
    finally {
        try{if(doc){doc.close(SaveOptions.DONOTSAVECHANGES);}}catch(cleanup){error='cleanup: '+cleanup.message;}
        app.userInteractionLevel=oldUI;if(prior){prior.activate();}
    }
    check('document count preserved',app.documents.length===count,app.documents.length);
    if(prior){check('user document preserved',prior.saved===sourceSaved && prior.pageItems.length===sourceItems,prior.pageItems.length);}
    var passed=!error;for(var j=0;j<checks.length;j++){if(!checks[j].passed){passed=false;}}
    return json({host:app.name,version:app.version,build:String(app.buildNumber),passed:passed,error:error,checks:checks,
        limitations:'Appearance stacks, Boolean holes/self-intersections, transforms, and undo remain unverified'});
})();
