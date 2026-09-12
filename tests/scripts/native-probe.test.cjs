// Execute the real JSX against in-memory bridge/document mocks. No Adobe host,
// COM connection, plugin load, export, or filesystem write is available to JSX.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const root = path.resolve(__dirname, '../..');
const probes = {
    native: 'tools/jsx/ai-probe-native-stroke.jsx',
    proof: 'plugin/illustrator-probe/host-proof.jsx'
};

function runProbe(kind, { baseline = {}, unavailable = false, finalError = 0 } = {}) {
    const documents = [], calls = [];
    let created = 0, closed = 0, statusCalls = 0;
    // Preserve property order: host-proof intentionally parses a fixed wire schema.
    const state = {
        schema: 1, status: 'PROPOSED', plugin: 'StrokeMasterProposedProbe',
        ok: true, error: 0, goCalls: 0, mutations: 0, strokeOrigin: 0,
        lastGoError: 0, effectFixtures: 0, controlFixtures: 0
    };
    const app = {
        name: 'Mock Illustrator', version: 'mock', buildNumber: '0',
        userInteractionLevel: 'original', documents, activeDocument: null,
        sendScriptMessage(plugin, selector, input) {
            calls.push({ plugin, selector, input });
            assert.equal(plugin, state.plugin);
            if (selector === 'status') {
                statusCalls++;
                if (unavailable) throw new Error('Mock native status unavailable');
                return JSON.stringify(statusCalls === 1 ? { ...state, ...baseline } : state);
            }
            assert.equal(selector, 'create');
            assert.ok(input === 'control' || input === 'effect');
            const doc = app.activeDocument;
            assert.ok(doc && documents.includes(doc), 'create requires an owned document');
            const source = {
                name: input, closed: false,
                pathPoints: [{ anchor: [0, 0] }, { anchor: [100, 0] }]
            };
            doc.pathItems.unshift(source);
            if (input === 'effect') {
                state.effectFixtures++;
                doc.effect = source;
            } else {
                state.controlFixtures++;
            }
            return JSON.stringify(state);
        },
        redraw() {
            const doc = app.activeDocument;
            if (!doc || !doc.effect) return;
            const anchor = doc.effect.pathPoints[1].anchor;
            if (!doc.rendered) {
                doc.rendered = true;
                state.goCalls++;
                state.mutations++;
                state.strokeOrigin = 1;
            } else if (anchor[0] !== 100 || anchor[1] !== 0) {
                // One successful reevaluation followed by a failed final callback.
                // Aggregate counters increase even though the latest result failed.
                state.goCalls += finalError ? 2 : 1;
                state.mutations++;
                state.lastGoError = finalError;
            }
        }
    };
    documents.add = function () {
        created++;
        const doc = {
            pathItems: [], effect: null, rendered: false,
            close(option) {
                assert.equal(option, 'discard');
                const index = documents.indexOf(doc);
                assert.notEqual(index, -1, 'close only a still-owned document');
                documents.splice(index, 1);
                closed++;
                if (app.activeDocument === doc) app.activeDocument = null;
            }
        };
        documents.push(doc);
        app.activeDocument = doc;
        return doc;
    };
    const filename = path.join(root, probes[kind]);
    const context = vm.createContext({
        app, $: { global: {} },
        DocumentColorSpace: { RGB: 'RGB' },
        UserInteractionLevel: { DONTDISPLAYALERTS: 'suppressed' },
        SaveOptions: { DONOTSAVECHANGES: 'discard' }
    });
    const raw = vm.runInContext(fs.readFileSync(filename, 'utf8'), context, {
        filename, timeout: 2000
    });
    assert.equal(typeof raw, 'string', 'probe returns its structured report');
    const report = JSON.parse(raw);
    assert.equal(documents.length, 0, 'all owned documents are closed');
    assert.equal(closed, created);
    assert.equal(app.userInteractionLevel, 'original');
    return { report, calls, created, statusCalls };
}

function certified(kind, report) {
    return kind === 'native' ? report.passed === true : report.status === 'DISPATCH_OBSERVED_ONLY';
}

for (const kind of Object.keys(probes)) {
    test(`${kind}: healthy dispatch and source edit are accepted`, () => {
        const result = runProbe(kind);
        assert.equal(certified(kind, result.report), true);
        assert.equal(result.report.edited.lastGoError, 0);
        assert.deepEqual(result.calls.filter(c => c.selector === 'create').map(c => c.input),
            ['control', 'effect']);
    });

    for (const [field, value] of [['schema', 999], ['plugin', 'WrongPlugin']]) {
        test(`${kind}: wrong ${field} stops before document creation or native create`, () => {
            const result = runProbe(kind, { baseline: { [field]: value } });
            assert.equal(certified(kind, result.report), false);
            assert.equal(result.created, 0);
            assert.deepEqual(result.calls.map(c => c.selector), ['status']);
        });
    }

    test(`${kind}: increasing counters cannot hide a failed final reevaluation`, () => {
        const { report } = runProbe(kind, { finalError: 5 });
        const beforeEdit = kind === 'native' ? report.stages.find(s => s.mode === 'effect').after : report.effect;
        assert.ok(report.edited.goCalls > beforeEdit.goCalls);
        assert.ok(report.edited.mutations > beforeEdit.mutations);
        assert.equal(report.edited.lastGoError, 5, 'reach the intended counterexample');
        assert.equal(certified(kind, report), false, 'final native failure must reject the proof');
    });

    test(`${kind}: unavailable status cannot claim native execution`, () => {
        const result = runProbe(kind, { unavailable: true });
        assert.equal(result.statusCalls, 1);
        assert.equal(result.created, 0);
        assert.equal(certified(kind, result.report), false);
        assert.notEqual(result.report.nativeExecuted, true);
        assert.match(result.report.error, /Mock native status unavailable/);
    });
}
