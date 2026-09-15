// Real JSX in a memory-only runtime: no Adobe, COM, or JSX filesystem access.
const test = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');
const script = fs.readFileSync(path.resolve(__dirname, '../../tools/jsx/ai-probe-native-stroke.jsx'), 'utf8');

function run(fault = {}) {
    const files = new Map(), operations = [], documents = [];
    const clone = x => JSON.parse(JSON.stringify(x));
    const state = {schema:1,status:'PROPOSED',plugin:'StrokeMasterProposedProbe',ok:true,error:0,
        goCalls:0,mutations:0,strokeOrigin:0,lastGoError:0,effectFixtures:0,controlFixtures:0};
    if(fault.pastFailure){state.goCalls=1;state.lastGoError=7;}
    function injectCallbacks(phase) {
        if (fault.callbackPhase === phase) { state.goCalls += 2; state.mutations++; state.strokeOrigin=1; state.lastGoError=0; }
    }
    if (fault.existing) files.set('/out/run-effect.ai', {text:'KEEP'});
    function File(name) { this.fsName = name; this.error = 'injected'; }
    Object.defineProperties(File.prototype, {
        exists:{get(){return files.has(this.fsName);}},
        length:{get(){return files.get(this.fsName)?.text.length || 0;}}
    });
    File.prototype.open = function(mode) {
        if (fault.journal && operations.some(x=>x==='add')) return false;
        if (mode==='w') { assert.ok(!this.exists); files.set(this.fsName,{text:''}); }
        return true;
    };
    File.prototype.writeln = function(text) {files.get(this.fsName).text += text+'\n'; return true;};
    File.prototype.close = () => true;
    function Folder(name) {this.fsName=name; this.exists=true;}
    const app = {name:'Mock Illustrator',version:'mock',buildNumber:'0',documents,userInteractionLevel:'original',
        sendScriptMessage(plugin, selector, input) {
            assert.equal(plugin, state.plugin);
            if (fault.bridge) throw Error('bridge unavailable');
            if (fault.malformed) return '{"schema":1}; app.quit()';
            if (selector==='diagnostics') return JSON.stringify({schema:2,plugin,build:fault.build||'native-prep-4',
                processId:fault.processId===undefined?12345:fault.processId,
                effectRegistered:true,commandRegistered:true,acceptedInputs:state.mutations,sequence:1,
                events:[{sequence:1,stage:'REGISTERS',detail:'',version:0,error:0}]});
            if (selector==='create') {
                operations.push('create:'+input);
                const d=app.activeDocument; assert.notEqual(d,prior);
                d.mode=input;
                d.pathItems=[{closed:false,pathPoints:[100,220].map(x=>({anchor:[x,100],leftDirection:[x,100],rightDirection:[x,100]}))}];
                state[input==='effect'?'effectFixtures':'controlFixtures']++;
                injectCallbacks(input+'.create');
            } else assert.equal(selector,'status');
            return JSON.stringify(state);
        },
        redraw() {
            const d=app.activeDocument;
            if (d.mode!=='effect') return;
            const geometry=JSON.stringify(d.pathItems);
            if (geometry===d.rendered) return;
            const edited=d.rendered!==undefined;
            injectCallbacks(d.reopened ? (edited ? 'effect.reopen.edit' : 'effect.reopen.redraw') : (edited ? 'effect.edit' : 'effect.redraw'));
            if(fault.initialError){state.goCalls++;state.lastGoError=1346458189;return;}
            state.goCalls++;state.mutations++;state.strokeOrigin=1;state.lastGoError=0;
            if (fault.reopenError && d.reopened && edited) {state.goCalls++;state.lastGoError=5;}
            d.rendered=geometry;
        },
        open(file) {
            operations.push('open:'+file.fsName);
            if (fault.open) throw Error('open failed');
            const d=makeDoc();d.reopened=true;
            Object.assign(d,clone(files.get(file.fsName).scene));d.fullName=file;
            injectCallbacks(d.mode+'.reopen');
            if (fault.geometry) d.pathItems[0].pathPoints[0].anchor[0]++;
            return d;
        }
    };
    const prior={name:'Unsaved user document',saved:false,pathItems:[{untouched:true}],activate(){app.activeDocument=this;injectCallbacks('restore');}};
    documents.push(prior);app.activeDocument=prior;
    function makeDoc() {
        const d={name:'Specimen',saved:false,pathItems:[],mode:null,
            saveAs(file) {
                assert.notEqual(this,prior);assert.ok(!file.exists);
                operations.push('save:'+file.fsName);
                if (fault.save && file.fsName.endsWith(fault.save)) throw Error('save failed');
                files.set(file.fsName,{text:'mock artifact',scene:clone({pathItems:d.pathItems,mode:d.mode})});
                d.saved=true;d.fullName=file;
                injectCallbacks(d.mode+'.save.'+(file.fsName.endsWith('.ai')?'ai':'pdf'));
                if(fault.exportError && d.mode==='effect' && file.fsName.endsWith('.pdf')) {state.goCalls++;state.lastGoError=6;}
            },
            close(option) {
                assert.notEqual(this,prior);assert.equal(option,'discard');
                operations.push('close');
                if(fault.closeOnce && !d.closeTried){d.closeTried=true;throw Error('close failed');}
                injectCallbacks(d.mode+'.close');
                documents.splice(documents.indexOf(d),1);app.activeDocument=null;
            }};
        documents.push(d);app.activeDocument=d;return d;
    }
    documents.add=()=>{operations.push('add');const d=makeDoc();injectCallbacks('createDocument');return d;};
    const context=vm.createContext({app,File,Folder,IllustratorSaveOptions:function(){},PDFSaveOptions:function(){},
        $:{global:{ISE_AI_ARGS:{outDir:'/out',runId:'run',lifecycle:true,expectedHostPid:fault.expectedPid}}},
        SaveOptions:{DONOTSAVECHANGES:'discard'},DocumentColorSpace:{RGB:'RGB'},
        UserInteractionLevel:{DONTDISPLAYALERTS:'suppressed'}});
    const report=JSON.parse(vm.runInContext(script,context,{timeout:2000}));
    assert.deepEqual([...documents],[prior],'close only owned documents even after failure');
    assert.equal(app.activeDocument,prior);assert.equal(prior.saved,false);
    assert.deepEqual(prior.pathItems,[{untouched:true}]);assert.equal(app.userInteractionLevel,'original');
    return {report,files,operations,state};
}

test('lifecycle saves/reopens each specimen, re-dispatches effect, and writes separate PDFs',()=>{
    const {report,files,operations}=run();
    assert.equal(report.passed,true,report.error);
    assert.equal(report.status,'LIFECYCLE_OBSERVED_ONLY');
    assert.equal(report.nativeBefore.processId,12345);
    assert.equal(report.nativeStages.HOST_CLOSES_CLEANLY,'NOT_MEASURED');
    assert.equal(operations.filter(x=>x.startsWith('save:')).length,4);
    assert.equal(operations.filter(x=>x.startsWith('open:')).length,2);
    assert.ok(report.edited.goCalls>report.stages[1].after.goCalls);
    const events=files.get('/out/run-stages.jsonl').text.trim().split('\n').map(JSON.parse);
    assert.equal(events.at(-1).status,'COMPLETE');
    events.forEach((e,i)=>assert.equal(e.sequence,i+1));
});
for(const [name,fault,pattern] of [
    ['existing output',{existing:true},/new output path/],
    ['unavailable bridge',{bridge:true},/bridge unavailable/],
    ['malformed payload',{malformed:true},/strict native identity/],
    ['wrong binary identity',{build:'old'},/expected native diagnostic build/],
    ['zero native PID',{processId:0},/native process ID/],
    ['out-of-range native PID',{processId:4294967296},/native process ID/],
    ['native PID disagrees with external enumeration',{expectedPid:999},/native process matches/],
    ['AI save failure',{save:'.ai'},/save failed/],
    ['PDF save failure',{save:'.pdf'},/save failed/],
    ['reopen failure',{open:true},/open failed/],
    ['changed reopened geometry',{geometry:true},/geometry preserved/],
    ['first host PARM reproduction',{initialError:true},/failed callback count unchanged/],
    ['success then failed reopened edit',{reopenError:true},/failed callback count unchanged/],
    ['export callback failure',{exportError:true},/failed callback count unchanged/],
    ['journal failure after document creation',{journal:true},/Cannot append stage journal/],
    ['first close failure retried in cleanup',{closeOnce:true},/close failed/]
]) test(name+' fails closed and preserves preexisting document',()=>{
    const {report,operations,files}=run(fault);
    assert.equal(report.passed,false);assert.equal(report.status,'PROPOSED');assert.match(report.error,pattern);
    if(fault.initialError||fault.reopenError||fault.exportError){
        assert.equal(report.nativeStages.CALLBACK_EXECUTES,'OBSERVED_COUNTER_DELTA');
        assert.equal(report.nativeFailure.build,'native-prep-4');
    }
    if(fault.initialError){
        assert.equal(report.nativeStages.TEST_ART_REACHES_CALLBACK,'NOT_MEASURED');
        assert.equal(report.lastNativeStatus.mutations,0);
        assert.equal(report.lastNativeStatus.lastGoError,1346458189);
        const events=files.get('/out/run-stages.jsonl').text.trim().split('\n').map(JSON.parse);
        const rawAt=events.findLastIndex(e=>e.stage==='native.diagnostics.raw');
        const closeAt=events.findIndex(e=>e.stage==='effect.close');
        assert.ok(rawAt>=0&&rawAt<closeAt,'failure payload is durable before owned effect close');
    }
    if(fault.existing) {assert.equal(files.size,1);assert.equal(files.get('/out/run-effect.ai').text,'KEEP');}
    if(fault.bridge||fault.malformed||fault.build||fault.existing||fault.processId!==undefined||fault.expectedPid!==undefined) assert.ok(!operations.includes('add'));
    if(fault.bridge||fault.malformed) assert.equal(report.nativeExecuted,false);
});
for (const phase of ['createDocument','effect.create','effect.redraw','effect.edit','effect.save.ai','effect.close','effect.reopen','effect.reopen.redraw','effect.reopen.edit','effect.save.pdf','restore']) {
    test('failure then success during '+phase+' cannot pass',()=>{
        const {report,state}=run({callbackPhase:phase});
        assert.equal(state.lastGoError,0,'latest success hides the prior failure from lastGoError');
        assert.ok(state.goCalls-state.mutations>0);
        assert.equal(report.passed,false);assert.equal(report.status,'PROPOSED');
        assert.match(report.error||report.cleanupCounterError,/failed callback count unchanged/);
    });
}
test('preexisting failed-call count is allowed when it never grows during the probe',()=>{
    const {report,state}=run({pastFailure:true});assert.equal(report.passed,true,report.error);
    assert.equal(report.baseline.goCalls-report.baseline.mutations,1);assert.equal(state.goCalls-state.mutations,1);
});

// Exercise the exact wrapper expression shipped by the PowerShell runner.
const capture = fs.readFileSync(path.join(__dirname,'capture-host.ps1'),'utf8');
const wrapperLine = capture.split(/\r?\n/).find(line=>/^\s*\$probeJs=/.test(line));
const wrapperParts = /^\s*\$probeJs='([^']*)'\+\$probeArgs\+'([^']*)'\+\$probeLiteral\+'([^']*)'\s*$/.exec(wrapperLine);
assert.ok(wrapperParts,'runner wrapper remains a checked three-part expression');
const wrapper = wrapperParts[1]+JSON.stringify({runId:'mock'})+wrapperParts[2]+JSON.stringify('/probe.jsx')+wrapperParts[3];
for (const mode of ['absent','undefined','object']) for (const throws of [false,true]) {
    test(`capture wrapper restores ${mode} arguments after ${throws?'failure':'success'} without globals`,()=>{
        const previous={sentinel:true}, globals={};
        if(mode!=='absent') globals.ISE_AI_ARGS=mode==='undefined'?undefined:previous;
        const context=vm.createContext({$:{global:globals,evalFile(file){
            assert.equal(file.fsName,'/probe.jsx');assert.equal(globals.ISE_AI_ARGS.runId,'mock');
            globals.ISE_AI_ARGS={partial:true};
            if(throws) throw Error('injected eval failure');return 'RESULT';
        }},File:function(name){this.fsName=name;}});
        const keys=Object.keys(context);
        if(throws) assert.throws(()=>vm.runInContext(wrapper,context),/injected eval failure/);
        else assert.equal(vm.runInContext(wrapper,context),'RESULT');
        assert.equal(Object.hasOwn(globals,'ISE_AI_ARGS'),mode!=='absent');
        assert.equal(globals.ISE_AI_ARGS,mode==='object'?previous:undefined);
        assert.deepEqual(Object.keys(context),keys);assert.equal(Object.hasOwn(context,'__iseResult'),false);
    });
}
