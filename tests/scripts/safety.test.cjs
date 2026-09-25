// Pure Node regression tests. These do not verify ExtendScript or Adobe host behavior.
const test = require('node:test');
const assert = require('node:assert/strict');
const { runtime, document } = require('./mock-runtime.cjs');

function exportFixture() {
    const rt = runtime();
    rt.load('ise-lib.jsx');
    const lib = rt.globals.ISE_LIB;
    lib.manifest = { frames: [] };
    const events = [];
    const doc = document(2, 'new.indd');
    doc.save = () => events.push('save');
    doc.exportFile = type => events.push(type);
    lib.writeText = () => events.push('manifest');
    return { rt, lib, doc, events, run: () => lib.saveExport(doc, 'fixture', 'demo') };
}

for (const modified of [false, true]) {
    test(`PURE: open target is preserved, modified=${modified}`, () => {
        const f = exportFixture();
        const other = document();
        other.modified = modified;
        other.fullName = { fsName: 'FIXTURE\\MINIMAL\\DEMO.INDD' };
        f.rt.app.documents = [other];
        assert.throws(f.run, /Refusing to overwrite/);
        assert.deepEqual(other.closeCalls, []);
        assert.equal(other.isValid, true);
        assert.deepEqual(f.events, []);
        assert.deepEqual(f.rt.calls.files, []);
    });
}

test('PURE: a failed save aborts exports, manifest, and the remaining build', () => {
    const f = exportFixture();
    const other = document();
    other.fullName = { fsName: 'somewhere/unrelated.indd' };
    f.rt.app.documents = [other];
    f.doc.save = () => { throw new Error('disk full'); };
    let continued = false;
    const report = f.lib.run(() => {
        f.lib.track(f.doc);
        f.run();
        continued = true;
    });
    assert.match(report, /disk full/);
    assert.match(report, /exports and manifest were not written/);
    assert.equal(continued, false);
    assert.deepEqual(f.events, []);
    assert.deepEqual(other.closeCalls, []);
    assert.deepEqual(f.doc.closeCalls, ['NO']);
    assert.equal(f.rt.app.scriptPreferences.userInteractionLevel, 1);
});

test('PURE: a successful save precedes every export and the manifest', () => {
    const f = exportFixture();
    const other = document();
    other.fullName = { fsName: 'different.indd' };
    f.rt.app.documents = [other, f.doc];
    f.doc.fullName = { fsName: 'fixture/minimal/demo.indd' };
    f.run();
    assert.deepEqual(f.events, ['save', 'pdf', 'png', 'idml', 'manifest']);
    assert.deepEqual(other.closeCalls, []);
    assert.deepEqual(f.doc.closeCalls, []);
});

test('PURE: an alternate wrapper for the same document is not an open-target conflict', () => {
    const f = exportFixture();
    const alias = document(f.doc.id);
    alias.fullName = { fsName: 'fixture/minimal/demo.indd' };
    f.rt.app.documents = [alias];
    f.run();
    assert.equal(f.events[0], 'save');
    assert.deepEqual(alias.closeCalls, []);
});

test('PURE: tracking an already-open document cannot transfer cleanup ownership', () => {
    const f = exportFixture();
    const other = document();
    const alias = document(other.id);
    f.rt.app.documents = [other];
    let continued = false;
    const report = f.lib.run(() => { f.lib.track(alias); continued = true; });
    assert.match(report, /Refusing to take ownership/);
    assert.equal(continued, false);
    assert.deepEqual(other.closeCalls, []);
    assert.deepEqual(alias.closeCalls, []);
});

function preferences(setup) {
    const rt = runtime();
    rt.load('ise-lib.jsx');
    // Values are created inside the VM so instanceof Array has ExtendScript's single-realm
    // semantics. Objects supplied by the test runner would be a different JavaScript realm.
    rt.evaluate(setup);
    return rt;
}

test('PURE: nested arrays restore from detached values after in-place mutation', () => {
    const rt = preferences(`
        app.pdfExportPreferences = { reflect: { properties: [{ name: 'ranges' }] }, ranges: [[1, 2], [3, 4]] };
    `);
    const report = rt.globals.ISE_LIB.run(() => rt.evaluate('app.pdfExportPreferences.ranges[0][1] = 99; app.pdfExportPreferences.ranges.push([5]);'));
    assert.equal(JSON.stringify(rt.app.pdfExportPreferences.ranges), '[[1,2],[3,4]]');
    assert.match(report, /pdfExportPreferences restore: fails=0 diff=0 unverified=0/);
});

test('PURE: restoration compares types and array structure, not their display strings', () => {
    const rt = preferences(`
        app.pdfExportPreferences = { reflect: { properties: [{ name: 'value' }, { name: 'ranges' }] }, value: 1, ranges: [1, '2,3'] };
    `);
    rt.globals.ISE_LIB.run(() => rt.evaluate("app.pdfExportPreferences.value = '1'; app.pdfExportPreferences.ranges = ['1,2', 3];"));
    assert.equal(rt.app.pdfExportPreferences.value, 1);
    assert.equal(JSON.stringify(rt.app.pdfExportPreferences.ranges), '[1,"2,3"]');
});

test('PURE: null, undefined, NaN, signed zero, and array holes retain their values', () => {
    const rt = preferences(`
        app.pdfExportPreferences = { reflect: { properties: [{ name: 'values' }] }, values: [null, undefined, NaN, -0, , false] };
    `);
    const report = rt.globals.ISE_LIB.run(() => rt.evaluate('app.pdfExportPreferences.values = [0, 0, 0, 0, undefined, true];'));
    assert.equal(rt.evaluate('var v = app.pdfExportPreferences.values; v[0] === null && v[1] === undefined && isNaN(v[2]) && 1/v[3] === -Infinity && !(4 in v) && v[5] === false;'), true);
    assert.match(report, /pdfExportPreferences restore: fails=0 diff=0 unverified=0/);
});

test('PURE: immutable Enumerator tokens survive restoration, including inside arrays', () => {
    const rt = preferences(`
        function Enumerator(value) { this.value = value; Object.freeze(this); }
        Enumerator.prototype.toString = function () { return this.value; };
        var original = new Enumerator('ORIGINAL');
        app.pdfExportPreferences = { reflect: { properties: [{ name: 'standardsCompliance' }, { name: 'tokens' }] }, standardsCompliance: original, tokens: [original] };
    `);
    rt.globals.ISE_LIB.run(() => rt.evaluate("app.pdfExportPreferences.standardsCompliance = new Enumerator('OTHER'); app.pdfExportPreferences.tokens[0] = new Enumerator('OTHER');"));
    assert.equal(rt.evaluate('app.pdfExportPreferences.standardsCompliance === original && app.pdfExportPreferences.tokens[0] === original;'), true);
});

test('PURE: unsupported objects, nested objects, cycles, and unreadable values are explicit', () => {
    const rt = preferences(`
        var cycle = []; cycle.push(cycle);
        app.pdfExportPreferences = { reflect: { properties: ['host', 'nested', 'cycle', 'unreadable'].map(function (name) { return { name: name }; }) }, host: { name: 'same', value: 1 }, nested: [{ value: 1 }], cycle: cycle };
        Object.defineProperty(app.pdfExportPreferences, 'unreadable', { get: function () { throw Error('cannot read'); } });
    `);
    const report = rt.globals.ISE_LIB.run(() => rt.evaluate('app.pdfExportPreferences.host.value = 2;'));
    assert.match(report, /pdfExportPreferences restore: fails=0 diff=0 unverified=4/);
    assert.match(report, /host: unsupported/);
    assert.match(report, /nested: unsupported/);
    assert.match(report, /cycle: cyclic array/);
    assert.match(report, /unreadable: cannot read/);
});

test('PURE: mutation by a restore setter cannot corrupt the verification baseline', () => {
    const rt = preferences(`
        var stored = [1, 2];
        app.pdfExportPreferences = { reflect: { properties: [{ name: 'ranges' }] } };
        Object.defineProperty(app.pdfExportPreferences, 'ranges', {
            get: function () { return stored; },
            set: function (value) { value[0] = 88; stored = value; }
        });
    `);
    const report = rt.globals.ISE_LIB.run(() => rt.evaluate('stored[0] = 99;'));
    assert.match(report, /pdfExportPreferences restore: fails=0 diff=1/);
});

test('PURE: failed restoration and unreadable verification remain visible and restore UI level', () => {
    const rt = preferences(`
        var stored = 1, unreadable = false;
        app.pdfExportPreferences = { reflect: { properties: [{ name: 'value' }] } };
        Object.defineProperty(app.pdfExportPreferences, 'value', {
            get: function () { if (unreadable) { throw Error('verification unavailable'); } return stored; },
            set: function () { throw Error('restore refused'); }
        });
    `);
    let report = rt.globals.ISE_LIB.run(() => rt.evaluate('stored = 2;'));
    assert.match(report, /fails=1 \[value: restore refused\] diff=1/);
    report = rt.globals.ISE_LIB.run(() => rt.evaluate('unreadable = true;'));
    assert.match(report, /value: verification failed: verification unavailable/);
    assert.equal(rt.app.scriptPreferences.userInteractionLevel, 1);
});

test('PURE: dependent preferences restore in priority order even after a build exception', () => {
    const rt = preferences(`
        var restoreOrder = [], standards = 'original-standard', compatibility = 'original-compatibility';
        app.pdfExportPreferences = { reflect: { properties: [{ name: 'acrobatCompatibility' }, { name: 'standardsCompliance' }] } };
        Object.defineProperty(app.pdfExportPreferences, 'standardsCompliance', {
            get: function () { return standards; }, set: function (v) { restoreOrder.push('standard'); standards = v; compatibility = 'side-effect'; }
        });
        Object.defineProperty(app.pdfExportPreferences, 'acrobatCompatibility', {
            get: function () { return compatibility; }, set: function (v) { restoreOrder.push('compatibility'); compatibility = v; }
        });
    `);
    const report = rt.globals.ISE_LIB.run(() => { rt.evaluate("standards = 'changed'; compatibility = 'changed';"); throw Error('build failed'); });
    assert.equal(JSON.stringify(rt.evaluate('restoreOrder')), '["standard","compatibility"]');
    assert.equal(rt.app.pdfExportPreferences.acrobatCompatibility, 'original-compatibility');
    assert.match(report, /ERROR build failed/);
    assert.equal(rt.app.scriptPreferences.userInteractionLevel, 1);
});

test('PURE: a preference reset by a later restore is restored again', () => {
    // Host 21.5.1: restoring the color settings after a preset export reset the destination profile.
    const rt = preferences(`
        var profile = 'WORKING', space = 'REPURPOSE_RGB';
        app.pdfExportPreferences = { reflect: { properties: [{ name: 'pdfDestinationProfile' }, { name: 'pdfColorSpace' }] } };
        Object.defineProperty(app.pdfExportPreferences, 'pdfDestinationProfile', { get: function () { return profile; }, set: function (v) { profile = v; } });
        Object.defineProperty(app.pdfExportPreferences, 'pdfColorSpace', { get: function () { return space; }, set: function (v) { space = v; profile = 'USE_DOCUMENT'; } });
    `);
    const report = rt.globals.ISE_LIB.run(() => rt.evaluate("profile = 'USE_DOCUMENT'; space = 'CMYK';"));
    assert.equal(rt.evaluate('profile + "|" + space'), 'WORKING|REPURPOSE_RGB');
    assert.match(report, /pdfExportPreferences restore: fails=0 diff=0 unverified=0/);
});

test('PURE: a compression threshold refused below the hidden sampling resolution is restored', () => {
    // Host 21.5.1: after [High Quality Print], thresholdToCompressMonochrome = 450 was refused
    // ("Data is out of range") while sampling was off and the hidden resolution was 1200.
    const rt = preferences(`
        var Sampling = { NONE: 'NONE', BICUBIC_DOWNSAMPLE: 'BICUBIC' };
        var sampling = 'NONE', dpi = 300, threshold = 450;
        app.pdfExportPreferences = { reflect: { properties: [{ name: 'monochromeBitmapSampling' }, { name: 'thresholdToCompressMonochrome' }] } };
        Object.defineProperty(app.pdfExportPreferences, 'monochromeBitmapSampling', { get: function () { return sampling; }, set: function (v) { sampling = v; } });
        Object.defineProperty(app.pdfExportPreferences, 'monochromeBitmapSamplingDPI', {
            get: function () { if (sampling === 'NONE') { throw Error('not applicable'); } return dpi; }, set: function (v) { dpi = v; } });
        Object.defineProperty(app.pdfExportPreferences, 'thresholdToCompressMonochrome', {
            get: function () { return threshold; }, set: function (v) { if (v < dpi) { throw Error('Data is out of range.'); } threshold = v; } });
    `);
    const report = rt.globals.ISE_LIB.run(() => rt.evaluate('dpi = 1200; threshold = 1800;'));
    assert.equal(rt.evaluate('sampling + "|" + threshold + "|" + dpi'), 'NONE|450|450');
    assert.match(report, /pdfExportPreferences restore: fails=0 diff=0 unverified=0 adjusted=\[monochromeBitmapSamplingDPI=450\]/);
});

function assertConsumed(rt) {
    assert.equal(rt.globals.ISE_LIB, undefined);
    assert.equal(rt.globals.ISE_ARGS, undefined);
}

test('PURE: canceling a panel invocation does not suppress the next dialog', () => {
    const rt = runtime();
    rt.choices.push(null, null);
    assert.equal(rt.load('strokemaster.jsx'), 'cancelled');
    assertConsumed(rt);
    assert.equal(rt.load('strokemaster.jsx'), 'cancelled');
    assert.equal(rt.calls.dialogs, 2);
});

test('PURE: successful panel invocations each show their own dialog and result', () => {
    const rt = runtime();
    rt.app.documents = [document()];
    rt.app.activeDocument = rt.app.documents[0];
    rt.choices.push(5, 5);
    assert.match(rt.load('strokemaster.jsx'), /watching=false/);
    assertConsumed(rt);
    assert.match(rt.load('strokemaster.jsx'), /watching=false/);
    assert.equal(rt.calls.dialogs, 2);
    assert.equal(rt.calls.alerts.length, 2);
});

test('PURE: preloaded headless arguments survive reload, then a panel run starts fresh', () => {
    const rt = runtime();
    rt.globals.ISE_ARGS = { command: 'status' };
    rt.load('ise-lib.jsx');
    assert.match(rt.load('strokemaster.jsx'), /command=status/);
    assert.equal(rt.calls.dialogs, 0);
    assertConsumed(rt);
    rt.choices.push(null);
    assert.equal(rt.load('strokemaster.jsx'), 'cancelled');
});

test('PURE: fresh direct arguments supersede a preloaded command and cannot trigger syncMode', () => {
    const rt = runtime();
    rt.globals.ISE_ARGS = { command: 'watch' };
    rt.load('ise-lib.jsx');
    rt.globals.ISE_ARGS = { command: 'status', syncMode: 'start' };
    assert.match(rt.load('strokemaster.jsx'), /command=status/);
    assert.equal(rt.tasks.size, 0);
    assert.equal(rt.calls.files.length, 0);
    assert.equal(rt.calls.dialogs, 0);
    assertConsumed(rt);
});

test('PURE: missing headless command is never interactive and is consumed', () => {
    const rt = runtime();
    rt.globals.ISE_ARGS = {};
    assert.match(rt.load('strokemaster.jsx'), /without a "command"/);
    assert.equal(rt.calls.dialogs, 0);
    assertConsumed(rt);
    rt.choices.push(null);
    assert.equal(rt.load('strokemaster.jsx'), 'cancelled');
});

test('PURE: load and dialog exceptions also consume invocation arguments', () => {
    const rt = runtime();
    const original = rt.context.$.evalFile;
    rt.globals.ISE_ARGS = { command: 'watch' };
    rt.context.$.evalFile = file => {
        if (file.fsName.endsWith('ise-faithful-outlines.jsx')) { throw Error('missing dependency'); }
        return original(file);
    };
    assert.throws(() => rt.load('strokemaster.jsx'), /missing dependency/);
    assertConsumed(rt);
    rt.context.$.evalFile = original;
    rt.choices.push(Error('dialog failed'), null);
    assert.throws(() => rt.load('strokemaster.jsx'), /dialog failed/);
    assertConsumed(rt);
    assert.equal(rt.load('strokemaster.jsx'), 'cancelled');
});

test('PURE: cleanup preserves an active watcher across later command invocations', () => {
    const rt = runtime();
    rt.app.documents = [document()];
    rt.app.activeDocument = rt.app.documents[0];
    rt.globals.ISE_ARGS = { command: 'watch', intervalMs: 700 };
    assert.match(rt.load('strokemaster.jsx'), /Watching every 700 ms/);
    const task = rt.tasks.get('iseLiveSyncTask');
    assert.equal(task.isValid, true);
    assertConsumed(rt);
    assert.doesNotThrow(() => task.listeners[0].callback());
    rt.globals.ISE_ARGS = { command: 'status' };
    assert.match(rt.load('strokemaster.jsx'), /watching=true/);
    assert.doesNotThrow(() => task.listeners[0].callback());
    assert.equal(task.isValid, true);
});

test('PURE: the panel report does not hide unverified preference restoration', () => {
    const rt = runtime();
    rt.evaluate("app.pdfExportPreferences = { reflect: { properties: [{ name: 'host' }] }, host: { name: 'same' } };");
    rt.choices.push(5);
    rt.load('strokemaster.jsx');
    assert.match(rt.calls.alerts[0], /unverified=1 \[host: unsupported/);
});

test('PURE: a named preset reference is kept and restored by name, not reported unverified', () => {
    const rt = preferences(`
        function FlattenerPreset(name) { this.name = name; }
        var high = new FlattenerPreset('[High Resolution]');
        app.epsExportPreferences = { reflect: { properties: [{ name: 'appliedFlattenerPreset' }] }, appliedFlattenerPreset: high };
    `);
    const report = rt.globals.ISE_LIB.run(() => rt.evaluate("app.epsExportPreferences.appliedFlattenerPreset = new FlattenerPreset('[Low Resolution]');"));
    assert.equal(rt.evaluate("app.epsExportPreferences.appliedFlattenerPreset === high;"), true);
    assert.match(report, /epsExportPreferences restore: fails=0 diff=0 unverified=0/);
});

test('PURE: extra preference objects are restored after success and after a thrown error', () => {
    for (const body of ["app.pdfPlacePreferences.pdfCrop = 'CROP_ART';", "app.pdfPlacePreferences.pdfCrop = 'CROP_ART'; throw Error('export failed');"]) {
        const rt = preferences(`
            app.pdfPlacePreferences = { reflect: { properties: [{ name: 'pdfCrop' }] }, pdfCrop: 'CROP_MEDIA' };
        `);
        const report = rt.globals.ISE_LIB.run(() => rt.evaluate(body), ['pdfPlacePreferences']);
        assert.equal(rt.app.pdfPlacePreferences.pdfCrop, 'CROP_MEDIA');
        assert.match(report, /pdfPlacePreferences restore: fails=0 diff=0 unverified=0/);
        if (body.includes('throw')) { assert.match(report, /ERROR export failed/); }
    }
});

test('PURE: export preferences changed before a failed export are restored', () => {
    const rt = preferences(`
        app.pdfExportPreferences = { reflect: { properties: [{ name: 'acrobatCompatibility' }, { name: 'pdfColorSpace' }] }, acrobatCompatibility: 'ACROBAT_6', pdfColorSpace: 'REPURPOSE_RGB' };
        app.pngExportPreferences = { reflect: { properties: [{ name: 'exportResolution' }] }, exportResolution: 72 };
    `);
    const report = rt.globals.ISE_LIB.run(() => rt.evaluate("app.pdfExportPreferences.acrobatCompatibility = 'ACROBAT_4'; app.pdfExportPreferences.pdfColorSpace = 'GRAY'; app.pngExportPreferences.exportResolution = 600; throw Error('disk full');"));
    assert.equal(rt.app.pdfExportPreferences.acrobatCompatibility, 'ACROBAT_6');
    assert.equal(rt.app.pdfExportPreferences.pdfColorSpace, 'REPURPOSE_RGB');
    assert.equal(rt.app.pngExportPreferences.exportResolution, 72);
    assert.match(report, /ERROR disk full/);
    assert.match(report, /pdfExportPreferences restore: fails=0 diff=0 unverified=0/);
    assert.match(report, /pngExportPreferences restore: fails=0 diff=0 unverified=0/);
    assert.equal(rt.app.scriptPreferences.userInteractionLevel, 1);
});
