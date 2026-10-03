// In-process tests. Host doubles check dispatch/cleanup, never Adobe geometry.
const assert = require('node:assert/strict');
const vm = require('node:vm');
const path = require('node:path').posix;

function dyadic(value) {
    if (!Number.isFinite(value)) { throw Error('Nonfinite extraction coordinate'); }
    const b=new DataView(new ArrayBuffer(8));b.setFloat64(0,value,false);
    const hi=b.getUint32(0,false), lo=b.getUint32(4,false), exp=(hi>>>20)&2047;
    let n=(BigInt(hi&1048575)<<32n)|BigInt(lo);if(exp) {n|=1n<<52n;}
    if(hi>>>31) {n=-n;}return {n,e:exp?exp-1075:-1074};
}
function sub(a,b) {const e=Math.min(a.e,b.e);return {n:(a.n<<BigInt(a.e-e))-(b.n<<BigInt(b.e-e)),e};}
function mul(a,b) {return {n:a.n*b.n,e:a.e+b.e};}
function collinearExact(a,b,p) {
    const dx=sub(dyadic(b[0]),dyadic(a[0])),dy=sub(dyadic(b[1]),dyadic(a[1]));
    return sub(mul(dx,sub(dyadic(p[1]),dyadic(a[1]))),mul(dy,sub(dyadic(p[0]),dyadic(a[0])))).n===0n;
}
function inspectExtraction(document) {
    const counts = {contours:document.paths.length, twoAnchor:0, twoAnchorCollinear:0,
        twoAnchorNoncollinear:0, collinearClosed:0, collinearClosedWithExtent:0};
    const indices = [];
    for (const contour of document.paths) {
        const points = contour.pts.flatMap(p => [p.a,p.l,p.r]);
        const a = points[0], b = points.find(p => p[0] !== a[0] || p[1] !== a[1]);
        const collinear = !b || points.every(p => collinearExact(a,b,p));
        if (contour.pts.length === 2) {
            counts.twoAnchor++;
            counts[collinear?'twoAnchorCollinear':'twoAnchorNoncollinear']++;
        }
        if (contour.closed && collinear) {
            counts.collinearClosed++;
            if (b) { counts.collinearClosedWithExtent++; }
            indices.push(contour.index);
        }
    }
    return {status:'EXTRACTION_GEOMETRY_ONLY',counts,collinearClosedIndices:indices,
        limitations:['Exact dyadic cross products on extracted binary coordinates only; not a glyph/font mapping.',
        'Collinear Bezier controls imply zero filled area but do not imply zero stroke ink.',
        'No contours were removed; native U-turn/cap/closure semantics remain unmeasured.']};
}

function measureJoins(source, extraction, collinearIndices) {
    const context={$:{global:{}}};vm.runInNewContext(source,context);
    const api=context.$.global.ISE_CLIP, collinear=new Set(collinearIndices), failures=new Set(), errors={};
    const counts={cornerCandidates:0,valid:0,straight:0,tiny:0,failed:0,failedCollinear:0,failedOther:0,failedTwoAnchor:0};
    for(const p of extraction.paths) {
        const flat=api.flatten(p.pts);
        for(let i=0;i<flat.length;i++) {
            const f=flat[i];if(!f.corner) {continue;}counts.cornerCandidates++;
            const a=flat[(i-1+flat.length)%flat.length],b=flat[(i+1)%flat.length];
            const ux=f.x-a.x,uy=f.y-a.y,wx=b.x-f.x,wy=b.y-f.y;
            const un=Math.hypot(ux,uy),wn=Math.hypot(wx,wy);
            if(un<=1e-12||wn<=1e-12) {counts.tiny++;continue;}
            try {
                const poly=api.joinPolygon(f.x,f.y,[ux/un,uy/un],[wx/wn,wy/wn],1.5,'clip',Math.sqrt(2),4);
                counts[poly?'valid':'straight']++;
            } catch(e) {
                counts.failed++;failures.add(p.index);errors[e.message]=(errors[e.message]||0)+1;
                counts[collinear.has(p.index)?'failedCollinear':'failedOther']++;
                if(p.pts.length===2) {counts.failedTwoAnchor++;}
            }
        }
    }
    return {status:'HYPOTHETICAL_LOCAL_JOINS_ONLY',counts,errors,failedContours:failures.size,
        allCollinearContoursHaveFailures:collinearIndices.every(i=>failures.has(i)),
        halfWidth:1.5,clipRatio:Math.sqrt(2),nativeExecuted:false};
}

function mockHost(libSource, caseSource, probeSource, fault) {
    const files = new Map(), folders = new Set(['/repo/build']);
    const calls = [], globals = {ISE_LIB:{sentinel:1},ISE_ARGS:{sentinel:2},ISE_MITER_CASES:{sentinel:3}};
    const originals = {...globals};
    function File(p) {
        if (!(this instanceof File)) { return new File(p); }
        this.fsName = p; this.name = path.basename(p);
        Object.defineProperty(this,'parent',{get:()=>new Folder(path.dirname(p))});
        Object.defineProperty(this,'exists',{get:()=>files.has(p)});
        Object.defineProperty(this,'length',{get:()=>files.get(p)?.length || 0});
        this.open=()=>{ files.set(p,''); return true; };
        this.write=t=>{ files.set(p,t); return true; };
        this.close=()=>true;
    }
    function Folder(p) {
        this.fsName=p; this.name=path.basename(p);
        Object.defineProperty(this,'parent',{get:()=>new Folder(path.dirname(p))});
        Object.defineProperty(this,'exists',{get:()=>folders.has(p)});
        this.create=()=>{folders.add(p);return true;};
        this.getFiles=()=>[...files.keys()].filter(f=>f.startsWith(p+'/')&&f.endsWith('.png')).map(f=>new File(f));
    }
    function prefs(values) {
        return {...values,reflect:{properties:Object.keys(values).map(name=>({name,type:'readwrite'}))}};
    }
    const existing = {name:'owner',saved:false,modified:true,id:1,isValid:true,
        close(){throw Error('owner closed');}};
    const docs=[existing];
    function page() {
        const p={marginPreferences:{}};
        for (const type of ['rectangles','graphicLines','polygons']) {
            const items=[];
            items.add=(props={})=>{
                if(fault==='geometry'&&type==='polygons') {throw Error('two anchors refused');}
                const it={...props,isValid:true,paths:[{}],remove(){this.isValid=false;items.splice(items.indexOf(this),1);}};
                if(fault==='normalized'&&type==='graphicLines') {
                    Object.defineProperty(it.paths[0],'entirePath',{get(){return [[35,50]];},set(v){}});
                }
                if(fault==='rounding'&&type!=='rectangles') {
                    // InDesign 21.6 read 50.01 back as 50.0100000000002.
                    let stored=[];
                    Object.defineProperty(it.paths[0],'entirePath',{get(){return stored.map(p=>[p[0],p[1]+2e-13]);},set(v){stored=v;}});
                }
                if(fault==='style'&&type==='graphicLines') {
                    Object.defineProperty(it,'endCap',{set(){throw Error('cap unavailable');}});
                }
                items.push(it);return it;
            };
            p[type]=items;
        }
        return p;
    }
    docs.add=visible=>{
        assert.equal(visible,false,'specimen document must be hidden');calls.push('hidden-document');
        const pages=[page()];pages.add=()=>{const p=page();pages.push(p);return p;};
        const doc={id:2,name:'specimen',saved:false,modified:false,isValid:true,pages,
            viewPreferences:{},documentPreferences:{},swatches:{itemByName:name=>({name})},
            save:f=>files.set(f.fsName,'INDD fixture double'),
            exportFile(format,f,show,preset){
                assert.equal(show,false);assert.equal(preset,undefined,'preset can override viewPDF');
                calls.push(format);
                if(fault==='export') {throw Error('export unavailable');}
                if(format==='pdf') {assert.equal(app.pdfExportPreferences.viewPDF,false);files.set(f.fsName,'PDF double');}
                else for(let i=0;i<pages.length;i++) {files.set(f.fsName.replace('.png','-'+(i+1)+'.png'),'PNG double');}
            },
            close(){this.isValid=false;docs.splice(docs.indexOf(this),1);calls.push('close-owned');}};
        docs.push(doc);return doc;
    };
    const app={version:'21.5.1',documents:docs,
        pdfExportPreferences:prefs({viewPDF:true,useSecurity:false,exportAsSinglePages:true,pageRange:'owner',exportReaderSpreads:true,standardsCompliance:0,acrobatCompatibility:8,pdfColorSpace:'CMYK'}),
        pngExportPreferences:prefs({exportResolution:72,pngQuality:0,transparentBackground:true,antiAlias:false,pngColorSpace:0,
            pngExportRange:0,exportingSpread:true,useDocumentBleeds:true,simulateOverprint:true}),
        epsExportPreferences:prefs({epsColor:1}),scriptPreferences:{userInteractionLevel:7}};
    // Unreadable in the snapshot, as InDesign 21.6 reports security properties while useSecurity
    // is off; non-enumerable so the before/after comparison below never reads them.
    function unreadable(o,name) {
        Object.defineProperty(o,name,{get(){throw Error('The property is not applicable in the current state.');},set(v){},enumerable:false});
        if(!o.reflect.properties.some(p=>p.name===name)) {o.reflect.properties.push({name,type:'readwrite'});}
    }
    if(fault==='unreadable') {unreadable(app.pdfExportPreferences,'disallowPrinting');}
    if(fault==='unreadable-written') {unreadable(app.pngExportPreferences,'simulateOverprint');}
    const before=JSON.stringify(app);
    const context=vm.createContext({app,File,Folder,$:{global:globals,fileName:'/repo/tools/jsx/probe-miter-host-controls.jsx'},
        EndJoin:{MITER_END_JOIN:1,BEVEL_END_JOIN:2,ROUND_END_JOIN:3},
        EndCap:{BUTT_END_CAP:1,ROUND_END_CAP:2,PROJECTING_END_CAP:3},
        PathType:{CLOSED_PATH:1,OPEN_PATH:2},StrokeAlignment:{CENTER_ALIGNMENT:1},
        MeasurementUnits:{POINTS:1},RulerOrigin:{PAGE_ORIGIN:1},SaveOptions:{NO:1},
        UserInteractionLevels:{NEVER_INTERACT:1},ExportFormat:{PDF_TYPE:'pdf',PNG_FORMAT:'png'},
        PageRange:{ALL_PAGES:'all'},PNGQualityEnum:{MAXIMUM:1},PNGColorSpaceEnum:{RGB:1},PNGExportRangeEnum:{EXPORT_ALL:1}});
    context.$.evalFile=f=>vm.runInContext(f.name==='ise-lib.jsx'?libSource:caseSource,context);
    const raw=vm.runInContext(probeSource,context);
    assert.equal(JSON.stringify(app),before,'owner documents and preferences must be restored');
    for(const k of Object.keys(originals)) {assert.equal(globals[k],originals[k],'restore '+k);}
    const record=[...files.entries()].find(([name])=>name.endsWith('/controls.json'));
    const result=record?JSON.parse(record[1]):null;
    return {raw,result,calls};
}

function run(inputs) {
    const checks=[];
    const check=(name,fn)=>{fn();checks.push({name,passed:true});};
    const box={$:{global:{}}};vm.runInNewContext(inputs.caseSource,box);
    const cases=JSON.parse(JSON.stringify(box.$.global.ISE_MITER_CASES));
    check('42 unique controls',()=>{assert.equal(cases.length,42);assert.equal(new Set(cases.map(c=>c.id)).size,42);});
    check('join/cap census',()=>{
        for(const kind of ['segment','exact-uturn','closed-two-anchor']) {
            const group=cases.filter(c=>c.kind===kind);
            assert.equal(new Set(group.map(c=>c.join+'/'+c.cap)).size,9);
        }
    });
    check('exact reversal and closed degeneracy are separate inputs',()=>{
        for(const c of cases.filter(c=>c.kind==='exact-uturn')) {
            assert.equal(c.closed,false);assert.deepEqual(c.points[0],c.points[2]);
        }
        for(const c of cases.filter(c=>c.kind==='closed-two-anchor')) {assert.equal(c.closed,true);assert.equal(c.points.length,2);}
    });
    check('near reversal has both signs at two scales',()=>{
        const near=cases.filter(c=>c.kind==='near-uturn');assert.equal(near.length,12);
        assert.deepEqual([...new Set(near.map(c=>c.epsilon))].sort((a,b)=>a-b),[-0.01,-0.001,0.001,0.01]);
        for(const c of near) {assert.notEqual(30*c.points[2][1],0);}
    });
    check('ASCII and syntax',()=>{
        for(const s of [inputs.caseSource,inputs.probeSource]) {assert.ok(!/[^\x00-\x7f]/.test(s));new vm.Script(s);}
    });
    check('successful export double restores owner state',()=>{
        const m=mockHost(inputs.libSource,inputs.caseSource,inputs.probeSource);
        assert.equal(m.result.harnessPassed,true);assert.equal(m.result.nativeGeometryVerdict,'NOT_MEASURED');
        assert.equal(m.result.cases.filter(c=>c.state==='CREATED').length,42);assert.equal(m.result.artifacts.length,44);
    });
    check('host geometry refusal remains in census',()=>{
        const m=mockHost(inputs.libSource,inputs.caseSource,inputs.probeSource,'geometry');
        assert.equal(m.result.harnessPassed,true);assert.equal(m.result.cases.filter(c=>c.state==='HOST_INPUT_REFUSED').length,9);
    });
    check('host normalization is recorded instead of accepted as requested geometry',()=>{
        const m=mockHost(inputs.libSource,inputs.caseSource,inputs.probeSource,'normalized');
        assert.equal(m.result.harnessPassed,true);assert.equal(m.result.cases.filter(c=>c.state==='HOST_INPUT_NORMALIZED').length,33);
    });
    check('rounding-level readback noise keeps the requested geometry',()=>{
        const m=mockHost(inputs.libSource,inputs.caseSource,inputs.probeSource,'rounding');
        assert.equal(m.result.harnessPassed,true);assert.equal(m.result.cases.filter(c=>c.state==='CREATED').length,42);
        for(const c of m.result.cases) {assert.ok(c.readbackDeviation>0&&c.readbackDeviation<=m.result.readbackTolerance);}
    });
    check('a preference unreadable at snapshot and never written is recorded, not failed',()=>{
        const m=mockHost(inputs.libSource,inputs.caseSource,inputs.probeSource,'unreadable');
        assert.equal(m.result.harnessPassed,true);assert.deepEqual(m.result.unverifiedProblems,[]);
        assert.equal(m.result.unreadablePreferences.length,1);assert.match(m.result.unreadablePreferences[0],/^pdfExportPreferences\.disallowPrinting: /);
    });
    check('an unreadable preference the probe writes fails the harness',()=>{
        const m=mockHost(inputs.libSource,inputs.caseSource,inputs.probeSource,'unreadable-written');
        assert.equal(m.result.harnessPassed,false);assert.equal(m.result.unverifiedProblems.length,1);
        assert.match(m.result.unverifiedProblems[0],/^pngExportPreferences\.simulateOverprint: /);
    });
    check('style failure cannot become a geometry pass',()=>{
        const m=mockHost(inputs.libSource,inputs.caseSource,inputs.probeSource,'style');assert.equal(m.result.harnessPassed,false);
    });
    check('export failure restores state and cannot pass',()=>{
        const m=mockHost(inputs.libSource,inputs.caseSource,inputs.probeSource,'export');assert.equal(m.result.harnessPassed,false);
    });
    check('zero-fill-area contours can still have extent',()=>{
        const r=inspectExtraction({paths:[{index:0,closed:true,pts:[{a:[0,0],l:[0,0],r:[0,0]},
            {a:[30,0],l:[30,0],r:[30,0]}]}]});
        assert.equal(r.counts.collinearClosedWithExtent,1);
    });
    check('two-anchor curved loops are not classified by cardinality',()=>{
        const r=inspectExtraction({paths:[{index:0,closed:true,pts:[{a:[0,0],l:[0,-1],r:[0,1]},
            {a:[30,0],l:[30,1],r:[30,-1]}]}]});assert.equal(r.counts.twoAnchorNoncollinear,1);
    });
    check('exact determinant survives rounded-product cancellation',()=>{
        const k=2**52;assert.equal(k*(k-2)-(k-1)*(k-1),0);
        assert.equal(collinearExact([0,0],[k,k-1],[k-1,k-2]),false);
    });
    check('existing JSX still refuses exact reversals in every join mode',()=>{
        const box={$:{global:{}}};vm.runInNewContext(inputs.clipSource,box);
        for(const mode of ['clip','miter','bevel','round']) {
            assert.throws(()=>box.$.global.ISE_CLIP.joinPolygon(0,0,[1,0],[-1,0],1.5,mode,Math.sqrt(2),4),/cusp or unresolved turn/);
        }
    });
    const inventory=inspectExtraction(inputs.extraction);
    const joins=measureJoins(inputs.clipSource,inputs.extraction,inventory.collinearClosedIndices);
    check('retained benchmark failure census reproduced',()=>{
        assert.equal(joins.counts.cornerCandidates,10960);assert.equal(joins.counts.valid,10781);
        assert.equal(joins.counts.failed,179);assert.equal(joins.failedContours,92);
        assert.equal(joins.counts.valid+joins.counts.failed+joins.counts.straight+joins.counts.tiny,joins.counts.cornerCandidates);
    });
    return {status:'IN_PROCESS_TESTS_ONLY',checks,passed:checks.length,
        nativeExecuted:false,extraction:inventory,joins};
}
module.exports={run,inspectExtraction,measureJoins};
if(require.main===module) {
    const fs=require('node:fs'), base=require('node:path').resolve(__dirname,'../..');
    const read=name=>fs.readFileSync(require('node:path').join(base,name),'utf8');
    console.log(JSON.stringify(run({caseSource:read('tools/jsx/miter-host-cases.jsx'),
        probeSource:read('tools/jsx/probe-miter-host-controls.jsx'),libSource:read('tools/jsx/ise-lib.jsx'),
        clipSource:read('tools/jsx/ise-clip-miter.jsx'),extraction:JSON.parse(read('build/takeover/extreme.json'))}),null,2));
}
