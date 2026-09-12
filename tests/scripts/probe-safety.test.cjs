// Pure probe orchestration tests. No Adobe, COM, or JSX filesystem access.
// The real library owns preference restoration and document cleanup in every run.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const root = path.resolve(__dirname, '../../tools/jsx');
const libSource = fs.readFileSync(path.join(root, 'ise-lib.jsx'), 'utf8');
const probes = ['probe-victim-census.jsx', 'probe-census-persistence.jsx', 'probe-pair-persistence-render.jsx'];
const sources = Object.fromEntries(probes.map(name => [name, fs.readFileSync(path.join(root, name), 'utf8')]));

// Construct mocks in the VM realm so ExtendScript's Array/Enumerator checks are meaningful.
function makeHost(fault) {
    const files = new Map(), operations = [], docs = [];
    function Enumerator(value) { this.value = value; }
    Enumerator.prototype.toString = function() { return this.value; };
    Object.defineProperty(Enumerator.prototype, 'name', {get() { throw Error('Enumerator has no name'); }});
    let nextId = 1, pairNumber = 0, opening = 0;
    const clone = value => JSON.parse(JSON.stringify(value));
    const enums = {
        MeasurementUnits: {POINTS: 'points'}, RulerOrigin: {PAGE_ORIGIN: 'page'}, Leading: {AUTO: 'auto'},
        ColorModel: {PROCESS: 'process'}, ColorSpace: {CMYK: 'cmyk'},
        TextStrokeAlign: {OUTSIDE_ALIGNMENT: 'outside', CENTER_ALIGNMENT: 'center'},
        EndJoin: {ROUND_END_JOIN: 'round', MITER_END_JOIN: 'miter'}, SaveOptions: {NO: 'discard'},
        UserInteractionLevels: {NEVER_INTERACT: 'suppressed'}, PNGQualityEnum: {MAXIMUM: 'maximum'},
        PNGColorSpaceEnum: {RGB: 'rgb'}, PNGExportRangeEnum: {EXPORT_ALL: 'all'},
        ExportFormat: {PNG_FORMAT: 'png', PDF_TYPE: 'pdf', INDESIGN_MARKUP: 'idml'}
    };
    if (fault.enumerator) enums.PNGQualityEnum.MAXIMUM = new Enumerator('maximum');
    function Folder(name) { if (!(this instanceof Folder)) return new Folder(name); this.fsName = name; this.exists = true; }
    Folder.prototype.create = () => true;
    function File(name) {
        if (!(this instanceof File)) return new File(name);
        this.fsName = name; this.parent = new Folder(name.slice(0, name.lastIndexOf('/'))); this.mode = '';
    }
    Object.defineProperties(File.prototype, {
        exists: {get() { return files.has(this.fsName); }},
        length: {get() { return files.get(this.fsName)?.bytes.length || 0; }}
    });
    File.prototype.open = function(mode) {
        this.mode = mode;
        if (fault.fileOpen === mode && (!fault.fileName || this.fsName.endsWith(fault.fileName))) return false;
        if (mode === 'w') files.set(this.fsName, {bytes: ''});
        return files.has(this.fsName);
    };
    File.prototype.write = function(text) {
        if (fault.manifestWrite) return false;
        files.get(this.fsName).bytes = fault.manifestCorrupt ? 'corrupt' : text; return true;
    };
    File.prototype.read = function(count) {
        if (fault.fileReadThrow) throw Error('injected read failure');
        const bytes = files.get(this.fsName).bytes; return count === undefined ? bytes : bytes.slice(0, count);
    };
    File.prototype.close = function() { return !(fault.fileClose === true || (typeof fault.fileClose === 'string' && this.fsName.endsWith(fault.fileClose))); };
    const preferenceDefaults = {exportResolution: 72, pngQuality: 'old', transparentBackground: true,
        antiAlias: false, pngColorSpace: 'old', pngExportRange: 'old', exportingSpread: true,
        useDocumentBleeds: true, simulateOverprint: true};
    function preferences(isPNG) {
        const target = {...preferenceDefaults, reflect: {properties: Object.keys(preferenceDefaults).map(name => ({name, type: 'readwrite'}))}};
        return new Proxy(target, {set(o, key, value) {
            if (isPNG && fault.pngIgnored === key && value !== preferenceDefaults[key]) return true;
            o[key] = value instanceof Enumerator ? new Enumerator(fault.enumeratorMismatch ? 'wrong' : String(value)) : value; return true;
        }});
    }
    const app = {version: 'mock', documents: docs, scriptPreferences: {userInteractionLevel: 'original'},
        pdfExportPreferences: preferences(false), pngExportPreferences: preferences(true), epsExportPreferences: preferences(false),
        pdfExportPresets: {itemByName() { return {isValid: !fault.presetMissing}; }}
    };
    function range(chars) {
        return new Proxy({}, {
            get(_, key) {
                if (key === 'fillTint' && fault.specialSwatchTint && ['Paper', 'None'].includes(chars[0]?.fillColor?.name)) return -1;
                return chars[0]?.[key];
            },
            set(_, key, value) { chars.forEach(c => { c[key] = value; }); return true; }
        });
    }
    function frame(doc, bounds, source) {
        const tf = {name: '', geometricBounds: bounds.slice(), textFramePreferences: {}, labels: {},
            overflows: false, isValid: true, rotationAngle: 0,
            contentTransparencySettings: {blendingSettings: {opacity: 100}}};
        let contents = '';
        tf.texts = [];
        Object.defineProperty(tf, 'contents', {get() { return contents; }, set(value) {
            contents = value;
            tf.characters = [...value].map((ch, index) => {
                const data = {contents: ch, fillTint: 100, strokeTint: 100, baselineShift: 0, underline: false, strikeThru: false};
                Object.defineProperties(data, {
                    horizontalOffset: {enumerable: true, get() { return bounds[1] + (index % 3) * 25; }},
                    baseline: {enumerable: true, get() { return bounds[0] + 30 + Math.floor(index / 3) * (Number(data.leading) || 17) + data.baselineShift; }}
                });
                return new Proxy(data, {set(o, key, v) {
                    if (fault.characterIgnored === key && v !== o[key]) return true;
                    if (key === 'fillTint' && fault.maskTintIgnored && o.fillColor?.name === 'Probe Mask White') { o[key] = 25; return true; }
                    if (key === 'fillTint' && o.fillColor?.name === 'None') throw Error('meaningless None tint assignment');
                    o[key] = v; return true;
                }});
            });
            const all = range(tf.characters);
            all.applyParagraphStyle = undefined;
            const text = new Proxy(all, {get(o, key) {
                if (key === 'applyParagraphStyle') return ps => {
                    for (const c of tf.characters) for (const k of Object.keys(ps)) c[k] = ps[k];
                };
                return o[key];
            }});
            const paragraphs = [];
            for (let start = 0; start < tf.characters.length; start += 3) paragraphs.push(range(tf.characters.slice(start, start + 3)));
            tf.texts = [text]; tf.parentStory = {texts: [text], paragraphs};
        }});
        tf.extractLabel = key => tf.labels[key] || '';
        tf.insertLabel = (key, value) => { tf.labels[key] = value; };
        tf.sendToBack = () => {};
        tf.remove = () => { doc.textFrames.splice(doc.textFrames.indexOf(tf), 1); tf.isValid = false; };
        tf.snapshot = () => ({name: tf.name, bounds: tf.geometricBounds, contents, labels: tf.labels,
            rotationAngle: tf.rotationAngle, opacity: tf.contentTransparencySettings.blendingSettings.opacity,
            chars: tf.characters.map(c => Object.fromEntries(Object.keys(c).filter(k => !['horizontalOffset', 'baseline'].includes(k)).map(k => [k, c[k]])))});
        tf.duplicate = () => frame(doc, bounds, clone(tf.snapshot()));
        tf.contents = source?.contents || '';
        if (source) {
            tf.name = source.name; tf.labels = clone(source.labels); tf.rotationAngle = source.rotationAngle;
            tf.contentTransparencySettings.blendingSettings.opacity = source.opacity;
            source.chars.forEach((props, i) => { Object.assign(tf.characters[i], clone(props)); });
        }
        doc.textFrames.push(tf); return tf;
    }
    function document(scene) {
        const swatches = ['None', 'Paper', 'Black'].map(name => ({name, isValid: true}));
        swatches.itemByName = name => swatches.find(s => s.name === name) || {name, isValid: false};
        const colors = {itemByName: swatches.itemByName, add(props) { const c = {...props, isValid: true}; swatches.push(c); return c; }};
        const doc = {id: nextId++, name: 'Fixture', saved: false, modified: true, isValid: true,
            textFrames: [], rectangles: [], swatches, colors, viewPreferences: {}, documentPreferences: {},
            paragraphStyles: {add(props) { return new Proxy({...props}, {set(o, k, v) {
                if (k === 'strokeWeight' && v !== 4 && fault.styleThrow) throw Error('injected style failure');
                if (k === 'strokeWeight' && v !== 4 && fault.styleIgnored) return true;
                o[k] = v; return true;
            }}); }},
            recompose() {},
            snapshot() { return clone({frames: this.textFrames.map(tf => tf.snapshot()), prefs: this.documentPreferences}); },
            close(option) {
                if (this === prior) throw Error('BUG: closing user document');
                if (option !== 'discard') throw Error('unexpected save policy');
                operations.push('close:' + this.id);
                this.isValid = false; docs.splice(docs.indexOf(this), 1);
            },
            save(file) {
                if (this === prior) throw Error('BUG: saving user document');
                operations.push('save:' + file.fsName);
                if (fault.saveThrow) throw Error('injected save failure');
                if (!fault.saveMissing) files.set(file.fsName, {bytes: 'INDD mock', scene: this.snapshot()});
                this.saved = !fault.saveState; this.modified = !!fault.saveState; this.fullName = file;
            },
            exportFile(format, file) {
                if (this === prior) throw Error('BUG: exporting user document');
                operations.push('export:' + file.fsName);
                if (fault.exportThrow) throw Error('injected export failure');
                if (fault.outputMissing && file.fsName.endsWith(fault.outputMissing)) return;
                let bytes = format === 'png' ? '\x89PNG\r\n\x1a\nmock' : format === 'pdf' ? '%PDF-mock' : 'PKmock';
                if (fault.outputEmpty && file.fsName.endsWith(fault.outputEmpty)) bytes = '';
                if (fault.outputCorrupt && file.fsName.endsWith(fault.outputCorrupt)) bytes = 'bad output';
                files.set(file.fsName, {bytes, scene: this.snapshot()});
            }
        };
        const rectangles = {add(props) {
            const r = new Proxy({...props, sendToBack() {}}, {set(o, key, value) {
                o[key] = key === 'fillTint' && value === 100 && Object.hasOwn(fault, 'backgroundTintReadback') ? fault.backgroundTintReadback : value;
                return true;
            }});
            doc.rectangles.push(r); return r;
        }};
        const textFrames = {add(props) { return frame(doc, props.geometricBounds); }};
        doc.pages = [{marginPreferences: {}, rectangles, textFrames}];
        if (scene) {
            doc.documentPreferences = clone(scene.prefs);
            scene.frames.forEach(tf => frame(doc, tf.bounds, tf));
        }
        docs.push(doc); return doc;
    }
    const prior = {id: 0, name: 'User unsaved', saved: false, modified: true, isValid: true};
    docs.push(prior); docs.add = () => { operations.push('add'); return document(); };
    app.open = file => {
        operations.push('open:' + file.fsName); opening++;
        if (fault.openPrior) return prior;
        if (fault.openThrow) throw Error('injected open failure');
        const doc = document(files.get(file.fsName).scene);
        doc.saved = true; doc.modified = false; doc.fullName = file;
        if (fault.roundtripChange && opening > 1) doc.textFrames[0].characters[0].strokeWeight++;
        if (fault.roundtripFillTint && opening > 1) doc.textFrames[0].characters[0].fillTint++;
        return doc;
    };
    // Input for the census persistence probe: a generated 20-frame scene, never a user doc.
    const fixture = document();
    for (let i = 0; i < (fault.inputShort ? 19 : 20); i++) {
        const tf = frame(fixture, [20, 20, 180, 190]); tf.name = 'VC' + String(fault.inputDuplicate && i === 1 ? 0 : i).padStart(2, '0'); tf.contents = 'HH\rHH';
        tf.characters.forEach(c => Object.assign(c, {strokeColor: {name: 'Paper'}, fillColor: {name: 'Black'}, strokeWeight: 12, endJoin: 'round'}));
    }
    files.set('/source.indd', {bytes: 'INDD input', scene: fixture.snapshot()}); fixture.close('discard'); operations.length = 0;
    if (fault.existingOutput) files.set('/out/' + fault.existingOutput, {bytes: 'KEEP'});
    function makeSplit() {
        return {splitOne(top) {
            if (fault.splitThrow) throw Error('injected split failure');
            if (fault.variedPairPaint) top.characters.forEach((c, i) => { c.fillTint = 10 + i; c.strokeTint = 40 + i; });
            if (fault.nonePairStroke) top.characters[0].strokeColor = {name: 'None'};
            const bottom = top.duplicate(); pairNumber++;
            top.characters.forEach(c => { c.strokeWeight = 0; c.strokeColor = {name: 'None'}; });
            bottom.characters.forEach(c => {
                c.fillColor = clone(c.strokeColor);
                if (c.strokeColor.name !== 'None') c.fillTint = c.strokeTint;
            });
            if (fault.badBottomColor) bottom.characters[0].fillColor = {name: 'None'};
            if (fault.badBottomTint) bottom.characters[0].fillTint = 25;
            const token = fault.emptyToken && pairNumber === 1 ? '' : 'p' + pairNumber;
            top.insertLabel('ISE_SPLIT', 'ISE3|' + token + '|fill');
            bottom.insertLabel('ISE_SPLIT', 'ISE3|' + (fault.orphanTokens ? token + 'orphan' : token) + '|stroke');
            if (fault.duplicateRole) bottom.insertLabel('ISE_SPLIT', top.extractLabel('ISE_SPLIT'));
            if (fault.badTop) top.characters[0].strokeWeight = 1;
            return {done: true};
        }};
    }
    return {app, File, Folder, Enumerator, ...enums, makeSplit, files, operations, prior,
        $: {fileName: '/scripts/probe.jsx', global: {ISE_PROBE_ARGS: {root: '/out', source: '/source.indd'}}}};
}

function run(name, fault = {}, globalMode = 'objects') {
    const context = vm.createContext({configuration: fault});
    vm.runInContext(`Object.assign(globalThis, (${makeHost.toString()})(configuration));`, context);
    const keys = ['ISE_LIB', 'ISE_ARGS', 'ISE_SPLIT'];
    if (globalMode !== 'absent') for (const key of keys) context.$.global[key] = globalMode === 'undefined' ? undefined : {sentinel: key};
    const before = keys.map(key => ({key, present: Object.hasOwn(context.$.global, key), value: context.$.global[key]}));
    let loads = 0;
    context.$.evalFile = file => {
        loads++;
        if (fault.load === loads) { context.$.global.ISE_LIB = {partial: true}; if (loads === 2) context.$.global.ISE_SPLIT = {partial: true}; throw Error('injected dependency failure'); }
        if (file.fsName.endsWith('/ise-lib.jsx')) vm.runInContext(libSource, context);
        else context.$.global.ISE_SPLIT = context.makeSplit();
    };
    let report, error;
    try { report = JSON.parse(vm.runInContext(sources[name], context, {timeout: 3000})); }
    catch (e) { error = e; }
    for (const {key, present, value} of before) {
        assert.equal(Object.hasOwn(context.$.global, key), present, key + ' presence restored');
        assert.equal(context.$.global[key], value, key + ' identity restored');
    }
    assert.equal(context.app.documents.length, 1, 'only preexisting document remains');
    assert.equal(context.app.documents[0], context.prior);
    assert.equal(context.prior.saved, false); assert.equal(context.prior.modified, true);
    assert.equal(context.app.scriptPreferences.userInteractionLevel, 'original');
    assert.equal(context.app.pngExportPreferences.exportResolution, 72);
    return {report, error, operations: [...context.operations], files: context.files};
}

for (const name of probes) {
    test(`${name}: distinct Enumerators with throwing name compare by value`, () => {
        const r = run(name, {enumerator: true}); assert.ifError(r.error); assert.equal(r.report.passed, true, r.report.log);
    });
    test(`${name}: mismatched Enumerator readback fails safely`, () => {
        const r = run(name, {enumerator: true, enumeratorMismatch: true}); assert.ifError(r.error);
        assert.equal(r.report.passed, false); assert.match(r.report.log, /Setup readback failed: pngQuality/);
    });
    test(`${name}: unsupported named-object access refuses without throwing`, () => {
        const helper = sources[name].slice(sources[name].indexOf('function same('), sources[name].indexOf('function requireValue('));
        const context = vm.createContext({});
        assert.equal(vm.runInContext(`${helper}; same({}, {get name() {throw Error('unsupported');}})`, context), false);
        assert.equal(vm.runInContext(`same({get name() {throw Error('unsupported');}}, {name: 'Paper'})`, context), false);
        assert.equal(vm.runInContext(`same({name: 'Paper'}, {name: 'Paper'})`, context), true);
        assert.equal(vm.runInContext(`same({name: 'Black'}, {name: 'Paper'})`, context), false);
    });
    for (const mode of ['objects', 'absent', 'undefined']) {
        test(`${name}: success restores ${mode} globals and user document`, () => {
            const r = run(name, {}, mode); assert.ifError(r.error); assert.equal(r.report.passed, true, r.report.log);
            assert.ok(r.report.sampledCharacterFields.length > 0);
            assert.equal(r.report.glyphPositionsUnchanged, undefined);
            assert.equal(r.report.characterStateAndPositionsPreserved, undefined);
        });
        test(`${name}: failed dependency restores ${mode} globals`, () => {
            const r = run(name, {load: 1}, mode); assert.match(r.error.message, /dependency failure/);
            assert.equal(r.operations.length, 0);
        });
    }
    for (const [label, fault, expected] of [
        ['ignored PNG setting', {pngIgnored: 'exportResolution'}, /Setup readback/],
        ['export throws', {exportThrow: true}, /export failure/],
        ['output read fails', {fileOpen: 'r'}, /Cannot read/],
        ['output read throws', {fileReadThrow: true}, /read failure/],
        ['output close fails', {fileClose: true}, /Cannot close/],
    ]) test(`${name}: ${label} fails without harming user state`, () => {
        const r = run(name, fault); assert.ifError(r.error); assert.equal(r.report.passed, false); assert.match(r.report.log, expected);
    });
    const first = name === probes[0] ? 'native.png' : name === probes[1] ? 'direct.pdf' : 'before.png';
    for (const problem of ['outputMissing', 'outputEmpty', 'outputCorrupt']) test(`${name}: ${problem} stops the pipeline`, () => {
        const r = run(name, {[problem]: first}); assert.ifError(r.error); assert.equal(r.report.passed, false);
        assert.equal(r.operations.filter(op => op.startsWith('export:')).length, 1);
    });
    test(`${name}: existing output aborts before dependency/host work`, () => {
        const r = run(name, {existingOutput: first}); assert.match(r.error.message, /overwrite|already exists/i);
        assert.equal(r.operations.length, 0); assert.equal(r.files.get('/out/' + first).bytes, 'KEEP');
    });
}
for (const name of [probes[0], probes[2]]) {
    for (const fault of [{styleThrow: true}, {styleIgnored: true}, {characterIgnored: 'strokeWeight'}, {saveThrow: true}, {saveMissing: true}, {saveState: true}]) {
        test(`${name}: ${Object.keys(fault)[0]} cannot produce success`, () => {
            const r = run(name, fault); assert.ifError(r.error); assert.equal(r.report.passed, false);
            if (fault.saveThrow || fault.saveMissing || fault.saveState) assert.equal(r.operations.filter(op => op.startsWith('export:')).length, 0);
        });
    }
}
for (const fault of [{manifestWrite: true}, {manifestCorrupt: true}, {fileOpen: 'w', fileName: 'manifest.json'}, {fileClose: 'manifest.json'}]) {
    test(`manifest ${Object.keys(fault)[0]} fails collection`, () => {
        const r = run(probes[0], fault); assert.ifError(r.error); assert.equal(r.report.passed, false); assert.match(r.report.log, /manifest/i);
    });
}
for (const name of [probes[1], probes[2]]) {
    for (const fault of [{openPrior: true}, {openThrow: true}, {roundtripChange: true}]) {
        test(`${name}: ${Object.keys(fault)[0]} fails safely`, () => {
            const r = run(name, fault); assert.ifError(r.error); assert.equal(r.report.passed, false);
        });
    }
}
for (const mode of ['objects', 'absent', 'undefined']) test(`split dependency failure restores ${mode} globals`, () => {
    const r = run(probes[2], {load: 2}, mode); assert.match(r.error.message, /dependency failure/); assert.equal(r.operations.length, 0);
});
for (const fault of [{orphanTokens: true}, {emptyToken: true}, {duplicateRole: true}, {badTop: true}]) {
    test(`pair validator rejects ${Object.keys(fault)[0]} before export`, () => {
        const r = run(probes[2], fault); assert.ifError(r.error); assert.equal(r.report.passed, false);
        assert.equal(r.operations.filter(op => op.startsWith('export:')).length, 0);
    });
}

for (const fault of [{inputShort: true}, {inputDuplicate: true}, {presetMissing: true}]) {
    test(`census source/setup rejects ${Object.keys(fault)[0]} before export`, () => {
        const r = run(probes[1], fault); assert.ifError(r.error); assert.equal(r.report.passed, false);
        assert.equal(r.operations.filter(op => op.startsWith('export:')).length, 0);
    });
}
for (const fault of [{saveThrow: true}, {saveMissing: true}, {saveState: true}]) {
    test(`persistence ${Object.keys(fault)[0]} stops before final reopen/export`, () => {
        const r = run(probes[1], fault); assert.ifError(r.error); assert.equal(r.report.passed, false);
        assert.equal(r.operations.filter(op => op.startsWith('open:')).length, 2);
        assert.ok(!r.files.has('/out/reopened.png') && !r.files.has('/out/reopened.pdf'));
    });
}
test('census mask uses checked process white without asserting special-swatch tint', () => {
    const r = run(probes[0], {specialSwatchTint: true}); assert.ifError(r.error);
    assert.equal(r.report.passed, true, r.report.log);
    const mask = r.files.get('/out/mask.png').scene;
    assert.equal(mask.frames.length, 20);
    for (const tf of mask.frames) for (const ch of tf.chars) {
        assert.equal(ch.fillColor.name, 'Probe Mask White'); assert.deepEqual([...ch.fillColor.colorValue], [0, 0, 0, 0]);
        assert.equal(ch.fillTint, 100);
    }
    assert.equal(mask.frames[9].opacity, 0, 'transparent negative control stays transparent');
    assert.equal(r.files.get('/out/fixture.indd').scene.frames[0].chars[0].fillColor.name, 'Black', 'saved native fixture stays unchanged');
});
test('census incorrect process-white tint fails with fixture, character, expected, and actual', () => {
    const r = run(probes[0], {maskTintIgnored: true}); assert.ifError(r.error);
    assert.equal(r.report.passed, false);
    assert.match(r.report.log, /fillTint context=mask VC00 character 0 expected=number\(100\) actual=number\(25\)/);
    assert.ok(!r.files.has('/out/mask.png') && !r.files.has('/out/manifest.json'));
});
for (const value of [100, -1]) test(`mask background ${value} records exact tint and requires external render validation`, () => {
    const r = run(probes[0], {backgroundTintReadback: value}); assert.ifError(r.error);
    assert.equal(r.report.passed, true, r.report.log);
    assert.equal(r.report.maskBackgroundTintReadback, value);
    assert.equal(r.report.maskBackgroundColorName, 'Black');
    assert.equal(r.report.renderValidationRequired, true);
    if (value === -1) assert.equal(r.report.maskBackgroundTintResolution, 'unresolved inherited/overridden');
    assert.ok(r.files.has('/out/mask.png'));
});
for (const value of [25, 99, 0, '-1']) test(`mask background rejects unexpected readback ${JSON.stringify(value)}`, () => {
    const r = run(probes[0], {backgroundTintReadback: value}); assert.ifError(r.error);
    assert.equal(r.report.passed, false);
    assert.match(r.report.log, /fillTint context=mask background expected=number\(100\) actual=/);
    assert.ok(!r.files.has('/out/mask.png'));
});
test('background sentinel exception does not normalize generic tint comparisons', () => {
    for (const name of probes) {
        const helper = sources[name].slice(sources[name].indexOf('function same('), sources[name].indexOf('function requireValue('));
        assert.equal(vm.runInNewContext(`${helper}; same(-1, 100)`), false);
    }
    const r = run(probes[0], {backgroundTintReadback: -1, maskTintIgnored: true});
    assert.equal(r.report.passed, false); assert.match(r.report.log, /context=mask VC00 character 0/);
});
test('pair contract preserves distinct source fills and strokes with varied tints', () => {
    const r = run(probes[2], {variedPairPaint: true}); assert.ifError(r.error);
    assert.equal(r.report.passed, true, r.report.log); assert.ok(r.report.sampledCharacterFields.includes('fillTint'));
    const frames = r.files.get('/out/pairs.indd').scene.frames;
    const colors = new Set();
    for (const tf of frames) for (const [i, ch] of tf.chars.entries()) {
        if (tf.labels.ISE_SPLIT.endsWith('|stroke')) {
            assert.equal(ch.fillColor.name, ch.strokeColor.name); colors.add(ch.fillColor.name);
            assert.equal(ch.fillTint, 40 + i); assert.equal(ch.fillTint, ch.strokeTint);
        } else {
            assert.equal(ch.fillColor.name, 'Black'); assert.equal(ch.fillTint, 10 + i);
            assert.equal(ch.strokeColor.name, 'None'); assert.equal(ch.strokeWeight, 0);
        }
    }
    assert.deepEqual([...colors].sort(), ['Blue', 'Paper', 'Red']);
});
for (const fault of [{badBottomColor: true}, {badBottomTint: true}]) test(`pair validator rejects ${Object.keys(fault)[0]}`, () => {
    const r = run(probes[2], fault); assert.ifError(r.error); assert.equal(r.report.passed, false);
    assert.match(r.report.log, /bottom fill matches stroke (color|tint)/);
    assert.equal(r.operations.filter(op => op.startsWith('export:')).length, 0);
});
test('pair validator accepts None bottom fill only where its stroke is None', () => {
    const r = run(probes[2], {nonePairStroke: true}); assert.ifError(r.error); assert.equal(r.report.passed, true, r.report.log);
});
test('pair persistence signature detects changed top fill tint after IDML', () => {
    const r = run(probes[2], {roundtripFillTint: true}); assert.ifError(r.error); assert.equal(r.report.passed, false);
    assert.match(r.report.log, /IDML preserves pair identity and sampled character fields/);
    assert.ok(!r.files.has('/out/after.png'));
});
