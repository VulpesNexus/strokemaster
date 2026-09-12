// Source-only transaction tests. No Adobe host or rendering is exercised.
const test = require('node:test');
const assert = require('node:assert/strict');
const { runtime, document } = require('./mock-runtime.cjs');

function fixture(fault = '', external = false, externalCopies = false) {
    const rt = runtime(), doc = document(), spread = doc.pages[0].parent;
    const page = { id: 202, parent: spread, constructor: { name: 'Page' }, isValid: true };
    doc.pages.push(page);
    doc.pages[0].isValid = true;
    const layer = { id: 301, locked: false, visible: true };
    Object.assign(rt.context, {
        StrokeAlignment: { CENTER_ALIGNMENT: 'center' }, NothingEnum: { NOTHING: 'nothing' },
        BlendMode: { NORMAL: 'normal' }, ShadowMode: { NONE: 'none' }, FeatherMode: { NONE: 'none' },
        TextWrapModes: { NONE: 'none' }, ListType: { NO_LIST: 'none' },
        LocationOptions: { BEFORE: 'before' }
    });
    const all = [], events = [], overprintReads = [], overprintWrites = []; let id = 400, source, groupCalls = 0;
    Object.defineProperty(doc, 'allPageItems', { get: () => paintItems(spread) });
    for (const targetPage of doc.pages) {
        Object.defineProperty(targetPage, 'allPageItems', {
            get: () => paintItems(spread).filter(x => x.parentPage === targetPage)
        });
    }
    const color = name => ({ id: name, name, isValid: true, constructor: { name: 'Color' } });
    doc.swatches.itemByName = color;
    function effects() {
        const e = { blendingSettings: { blendMode: 'normal', opacity: 100, knockoutGroup: false, isolateBlending: false },
            dropShadowSettings: { mode: 'none' }, featherSettings: { mode: 'none' } };
        for (const key of ['innerShadow', 'outerGlow', 'innerGlow', 'bevelAndEmboss', 'satin', 'directionalFeather', 'gradientFeather']) e[key + 'Settings'] = { applied: false };
        return e;
    }
    function hit(point) { events.push(point); if (point === fault) throw Error('injected ' + point); }
    function children(parent) { return all.filter(x => x.isValid && x.parent === parent); }
    // Host measurements: mixed pageItems are type-ordered, and Group/TextFrame
    // can both have index 0. allPageItems instead flattens actual paint order.
    function collectionChildren(parent) {
        const rank = { TextFrame: 0, Rectangle: 1, Group: 2, Polygon: 3 };
        return children(parent).sort((a, b) => rank[a.constructor.name] - rank[b.constructor.name]);
    }
    function paintItems(parent) {
        return children(parent).flatMap(x => [x, ...paintItems(x)]);
    }
    spread.pageItems = { everyItem: () => ({ getElements: () => collectionChildren(spread) }) };
    function item(type, parent = spread) {
        const x = { id: ++id, constructor: { name: type }, isValid: true, name: '', parent, parentPage: page,
            itemLayer: layer, locked: false, nonprinting: false, geometricBounds: [10, 220, 50, 320],
            absoluteRotationAngle: 0, absoluteShearAngle: 0, horizontalScale: 100, verticalScale: 100,
            fillColor: color('None'), strokeColor: color('None'), textWrapPreferences: { textWrapMode: 'none' },
            remove() {
                hit(this === source ? 'commit' : 'remove-' + type);
                if (this === source && fault === 'ignore-commit') return;
                if (type === 'Group' && fault === 'rollback-fails') throw Error('injected cleanup failure');
                if (type === 'Group' && fault === 'ignore-cleanup') return;
                for (const c of children(this)) c.remove();
                this.isValid = false;
                if (this === source && fault === 'commit-after-delete') throw Error('injected after deletion');
            },
            bringToFront(reference) {
                const point = reference ? 'place' : 'order';
                hit(point);
                if (fault === 'ignore-' + point) return;
                all.splice(all.indexOf(this), 1);
                const before = reference || children(this.parent)[0];
                all.splice(before ? all.indexOf(before) : all.length, 0, this);
            },
            ungroup() { hit('ungroup'); for (const c of children(this)) c.parent = this.parent; this.isValid = false; }
        };
        for (const k of ['transparencySettings', 'strokeTransparencySettings', 'fillTransparencySettings', 'contentTransparencySettings']) x[k] = effects();
        Object.defineProperty(x, 'pageItems', { get: () => ({ everyItem: () => ({ getElements: () => collectionChildren(x) }) }) });
        Object.defineProperty(x, 'index', { get: () => children(x.parent).filter(c => c.constructor.name === type).indexOf(x) });
        all.push(x); return x;
    }
    function polygon(parent) {
        const x = item('Polygon', parent);
        x.paths = [{ entirePath: [[10, 220], [20, 220], [20, 230]] }];
        x.duplicate = () => {
            hit('stroke-duplicate');
            if (fault === 'copy-preexisting') return unrelated;
            const copy = polygon(externalCopies ? spread : x.parent);
            if (fault === 'copy-geometry') copy.paths[0].entirePath[0][0] += 1;
            if (fault === 'copy-bounds') copy.geometricBounds[0] += 1;
            if (fault === 'copy-page') copy.parentPage = doc.pages[0];
            if (fault === 'copy-layer') copy.itemLayer = { id: 999 };
            all.splice(all.indexOf(copy), 1);
            all.splice(all.indexOf(x), 0, copy);
            return copy;
        };
        for (const key of ['overprintStroke', 'overprintFill']) {
            let value = false;
            const paintKey = key === 'overprintStroke' ? 'strokeColor' : 'fillColor';
            function applicable() {
                const name = x[paintKey].name;
                if (name === 'Paper' || name === 'None') throw Error(key + ' is inapplicable for ' + name);
            }
            Object.defineProperty(x, key, {
                get() {
                    overprintReads.push({ item: x, key, color: x[paintKey].name });
                    applicable();
                    hit('get-' + key);
                    return value;
                },
                set(v) {
                    overprintWrites.push({ item: x, key, color: x[paintKey].name, before: value, wanted: v });
                    applicable();
                    hit(key);
                    if (fault !== 'ignore-' + key) value = v;
                }
            });
        }
        return x;
    }
    const paint = { contents: 'Hello', strokeAlignment: 'OUTSIDE_ALIGNMENT', strokeWeight: 2,
        endJoin: 'MITER_END_JOIN', miterLimit: 4, strokeTint: 100, overprintStroke: true,
        fillTint: 100, overprintFill: true, strokeColor: color('Black'), fillColor: color('Cyan'),
        underline: false, strikeThru: false, ruleAbove: false, ruleBelow: false,
        paragraphShadingOn: false, paragraphBorderOn: false, bulletsAndNumberingListType: 'none' };
    function frame() {
        const x = item('TextFrame');
        Object.assign(x, { name: 'page two text', overflows: false, previousTextFrame: null, nextTextFrame: null,
            allPageItems: [], tables: [], footnotes: [] });
        x.parentStory = { id: ++id, contents: 'Hello', textContainers: [x], textStyleRanges: [{ ...paint }], tables: [], footnotes: [] };
        x.duplicate = () => { hit('frame-duplicate'); return frame(); };
        x.createOutlines = deleteOriginal => {
            hit('outlines-start'); assert.notEqual(x, source, 'must never outline the source');
            assert.equal(source.isValid, true);
            const nested = item('Group', external ? spread : x.parent), p = polygon(nested);
            if (fault === 'external-no-handles') {
                item('Rectangle'); // Simulate unrelated event art during the same host call.
                throw Error('injected no returned handles');
            }
            hit('outlines-partial');
            if (fault === 'rollback-fails') throw Error('injected partial outline');
            if (fault === 'ignore-cleanup') throw Error('injected partial outline');
            if (deleteOriginal) x.remove();
            if (fault === 'empty') { nested.remove(); return []; }
            if (fault === 'bad-geometry') p.paths[0].entirePath[0][0] = NaN;
            if (fault === 'escaped') nested.parent = spread;
            if (fault === 'preexisting') return [source];
            if (fault === 'bare-output') { p.parent = spread; nested.remove(); return [p]; }
            if (fault === 'multiple-output') return [nested, polygon(spread)];
            return [nested];
        };
        return x;
    }
    page.groups = { add(items) {
        groupCalls++;
        const prefix = groupCalls === 1 ? 'group' : externalCopies && groupCalls === (external ? 3 : 2) ? 'copy-regroup' : 'regroup';
        hit(prefix + '-start');
        assert.ok(items.every(x => x.parent === spread), 'group only same-parent siblings');
        const g = item('Group');
        for (const x of items) x.parent = g;
        hit(prefix + '-partial');
        if (fault === 'regroup-drift' && groupCalls === 2) {
            all.find(x => x.constructor.name === 'Polygon').geometricBounds[0] += 1;
        }
        if (fault === 'copy-regroup-drift' && prefix === 'copy-regroup') {
            all.find(x => x.constructor.name === 'Polygon').geometricBounds[0] += 1;
        }
        return g;
    } };
    doc.pages[0].groups = { add() { throw Error('wrong page'); } };
    source = frame(); const unrelated = item('Rectangle');
    rt.load('ise-faithful-outlines.jsx');
    return { api: rt.globals.ISE_FAITHFUL, rt, doc, source, unrelated, all, events, paint, overprintReads, overprintWrites,
        convert: () => rt.globals.ISE_FAITHFUL.convertFrame(doc, source, () => hit('log')),
        survivors: () => all.filter(x => x.isValid) };
}

test('page 2 conversion stages a duplicate and commits last with checked overprint', () => {
    const f = fixture(), out = f.convert();
    assert.equal(f.source.isValid, false);
    assert.equal(out.parentPage, f.doc.pages[1]);
    assert.equal(out.parent, f.source.parent);
    assert.equal(out.itemLayer, f.source.itemLayer);
    assert.equal(f.events.at(-1), 'commit');
    const paths = f.survivors().filter(x => x.constructor.name === 'Polygon');
    assert.equal(paths.length, 2);
    assert.equal(paths.find(x => x.name === 'stroke').strokeWeight, 4);
    assert.equal(paths.find(x => x.name === 'stroke').overprintStroke, true);
    assert.equal(paths.find(x => x.name === 'fill').overprintFill, true);
    assert.equal(f.survivors().filter(x => x.constructor.name === 'TextFrame').length, 0);
});

for (const fault of ['frame-duplicate', 'group-start', 'group-partial', 'outlines-start', 'outlines-partial', 'ungroup',
    'stroke-duplicate', 'get-overprintStroke', 'get-overprintFill', 'overprintStroke', 'overprintFill', 'ignore-overprintStroke', 'ignore-overprintFill',
    'order', 'place', 'ignore-order', 'ignore-place', 'log', 'commit', 'ignore-commit', 'empty', 'bad-geometry', 'preexisting']) {
    test('rollback preserves source and unrelated art: ' + fault, () => {
        const f = fixture(fault), before = JSON.stringify(f.source.parentStory.textStyleRanges);
        assert.throws(f.convert);
        const point = fault.startsWith('ignore-') ? fault.slice(7) : ['empty', 'bad-geometry', 'escaped', 'preexisting'].includes(fault) ? 'outlines-partial' : fault;
        assert.ok(f.events.includes(point), 'must reach the intended fault: ' + point);
        if (point === 'overprintStroke' || point === 'overprintFill') {
            const write = f.overprintWrites.find(w => w.key === point);
            assert.ok(write, 'must attempt the setter');
            assert.equal(write.before, false);
            assert.equal(write.wanted, true, 'request a changed value so assign cannot skip the fault');
        }
        assert.equal(f.source.isValid, true);
        assert.equal(f.source.parentStory.contents, 'Hello');
        assert.equal(JSON.stringify(f.source.parentStory.textStyleRanges), before);
        assert.deepEqual(f.source.geometricBounds, [10, 220, 50, 320]);
        assert.deepEqual(f.survivors(), [f.source, f.unrelated]);
    });
}

test('mixed collections reproduce type order and paint order before source commit', () => {
    const f = fixture('', true, true), page = f.doc.pages[1], spread = f.source.parent;
    let inspected = false;
    const result = f.api.convertFrame(f.doc, f.source, () => {
        const mixed = spread.pageItems.everyItem().getElements().filter(x => x !== f.unrelated);
        assert.deepEqual(mixed.map(x => x.constructor.name), ['TextFrame', 'Group']);
        assert.deepEqual(mixed.map(x => x.index), [0, 0]);
        const paint = page.allPageItems.filter(x => x !== f.unrelated);
        assert.deepEqual(paint.map(x => x.constructor.name), ['Group', 'Polygon', 'Polygon', 'TextFrame']);
        assert.deepEqual(paint.filter(x => x.parent === spread), [mixed[1], f.source]);
        assert.equal(f.source.isValid, true);
        inspected = true;
    });
    assert.equal(inspected, true);
    assert.equal(result.isValid, true);
    assert.equal(f.source.isValid, false);
});

for (const [role, swatch] of [['stroke', 'Paper'], ['fill', 'Paper'], ['fill', 'None']]) {
    const key = role === 'stroke' ? 'overprintStroke' : 'overprintFill';
    for (const requested of [false, true]) test(`${swatch} ${role}: requested overprint ${requested}`, () => {
        const f = fixture('', true, true), style = f.source.parentStory.textStyleRanges[0];
        style[role + 'Color'] = f.doc.swatches.itemByName(swatch);
        style[key] = requested;
        if (requested) {
            assert.throws(f.convert, /overprinting Paper\/None is unsupported/);
            assert.equal(f.source.isValid, true);
            assert.equal(f.source.parentStory.contents, 'Hello');
            assert.deepEqual(f.survivors(), [f.source, f.unrelated]);
        } else {
            const out = f.convert();
            assert.equal(out.isValid, true);
            assert.equal(f.source.isValid, false);
        }
        assert.equal(f.overprintReads.some(r => r.key === key && r.color === swatch), false, 'helper must not read an inapplicable property');
        assert.equal(f.overprintWrites.some(w => w.key === key && w.color === swatch), false, 'helper must not write an inapplicable property');
    });
}

test('Paper and None polygon overprint getters and setters both throw in the mock', () => {
    const f = fixture();
    f.source.parentStory.textStyleRanges[0].strokeColor = f.doc.swatches.itemByName('Paper');
    f.source.parentStory.textStyleRanges[0].overprintStroke = false;
    f.convert();
    const stroke = f.survivors().find(x => x.name === 'stroke');
    for (const swatch of ['Paper', 'None']) {
        stroke.strokeColor = f.doc.swatches.itemByName(swatch);
        stroke.fillColor = f.doc.swatches.itemByName(swatch);
        for (const key of ['overprintStroke', 'overprintFill']) {
            assert.throws(() => stroke[key], /inapplicable/);
            assert.throws(() => { stroke[key] = false; }, /inapplicable/);
            assert.throws(() => { stroke[key] = true; }, /inapplicable/);
        }
    }
});

for (const key of ['overprintStroke', 'overprintFill']) test('identical normal-color false skips faulting setter: ' + key, () => {
    const f = fixture(key);
    f.source.parentStory.textStyleRanges[0][key] = false;
    const out = f.convert();
    assert.equal(out.isValid, true);
    assert.ok(f.overprintReads.some(r => r.key === key));
    assert.equal(f.overprintWrites.some(w => w.key === key), false);
    assert.equal(f.events.includes(key), false);
});

const unsupported = {
    threaded: f => { f.source.nextTextFrame = {}; },
    'multiple containers': f => { f.source.parentStory.textContainers.push({}); },
    overset: f => { f.source.overflows = true; },
    background: f => { f.source.fillColor.name = 'Paper'; },
    'frame stroke': f => { f.source.strokeColor.name = 'Black'; },
    effects: f => { f.source.contentTransparencySettings.innerShadowSettings.applied = true; },
    opacity: f => { f.source.transparencySettings.blendingSettings.opacity = 90; },
    'unknown effect': f => { delete f.source.transparencySettings.satinSettings; },
    grouped: f => { f.source.parent = { constructor: { name: 'Group' } }; },
    pasteboard: f => { f.source.parentPage = null; },
    anchored: f => { f.source.allPageItems.push({}); },
    tables: f => { f.source.parentStory.tables.push({}); },
    footnotes: f => { f.source.parentStory.footnotes.push({}); },
    shading: f => { f.source.parentStory.textStyleRanges[0].paragraphShadingOn = true; },
    underline: f => { f.source.parentStory.textStyleRanges[0].underline = true; },
    mixed: f => { f.source.parentStory.textStyleRanges.push({ ...f.paint, strokeWeight: 8 }); },
    rotation: f => { f.source.absoluteRotationAngle = 20; },
    'text wrap': f => { f.source.textWrapPreferences.textWrapMode = 'bounding'; },
    'unknown paint': f => { f.source.parentStory.textStyleRanges[0].strokeWeight = undefined; },
    gradient: f => { f.source.parentStory.textStyleRanges[0].fillColor.constructor.name = 'Gradient'; },
    'blank paragraph shading': f => { f.source.parentStory.textStyleRanges.push({ ...f.paint, contents: '\r', paragraphShadingOn: true }); },
    'unreadable effect': f => { Object.defineProperty(f.source, 'transparencySettings', { get() { throw Error('unreadable'); } }); }
};
for (const [name, alter] of Object.entries(unsupported)) test('refuses ' + name + ' before staging', () => {
    const f = fixture(); alter(f);
    assert.equal(f.api.canConvert(f.source), false);
    assert.equal(f.convert(), null);
    assert.equal(f.source.isValid, true);
    assert.deepEqual(f.survivors(), [f.source, f.unrelated]);
    assert.equal(f.events.includes('frame-duplicate'), false);
});

test('external sibling outline output is regrouped and converted on page two', () => {
    const f = fixture('', true), out = f.convert();
    assert.equal(out.parentPage, f.doc.pages[1]);
    assert.equal(out.parent, f.source.parent);
    assert.equal(f.source.isValid, false);
    assert.ok(f.events.includes('regroup-partial'));
    assert.equal(f.survivors().filter(x => x.constructor.name === 'Group').length, 1);
    assert.equal(f.survivors().filter(x => x.constructor.name === 'Polygon').length, 2);
    assert.equal(f.events.at(-1), 'commit');
});

for (const mode of ['bare-output', 'multiple-output']) test('external returned shape: ' + mode, () => {
    const f = fixture(mode, true), out = f.convert();
    assert.equal(f.source.isValid, false);
    assert.equal(out.parentPage, f.doc.pages[1]);
    assert.equal(f.survivors().filter(x => x.constructor.name === 'Group').length, 1);
    assert.equal(f.survivors().filter(x => x.constructor.name === 'Polygon').length, mode === 'bare-output' ? 2 : 4);
});

test('unreturned external output reports incomplete cleanup without sweeping unrelated new art', () => {
    const f = fixture('external-no-handles', true);
    assert.throws(f.convert, /rollback incomplete.*remaining new art IDs/);
    assert.equal(f.source.isValid, true);
    assert.equal(f.source.parentStory.contents, 'Hello');
    assert.equal(f.unrelated.isValid, true);
    assert.equal(f.survivors().filter(x => x.constructor.name === 'TextFrame').length, 1);
    assert.equal(f.survivors().filter(x => x.constructor.name === 'Rectangle').length, 2);
    assert.equal(f.events.includes('commit'), false);
});

for (const fault of ['regroup-start', 'regroup-partial', 'regroup-drift', 'ungroup', 'overprintStroke', 'log']) {
    test('external output rollback, including an unreturned new group: ' + fault, () => {
        const f = fixture(fault, true);
        assert.throws(f.convert, fault === 'log' ? /injected log/ : /./);
        assert.ok(f.events.includes(fault === 'regroup-drift' ? 'regroup-partial' : fault));
        assert.equal(f.source.isValid, true);
        assert.equal(f.source.parentStory.contents, 'Hello');
        assert.deepEqual(f.survivors(), [f.source, f.unrelated]);
    });
}

for (const mode of ['', 'multiple-output']) test('external stroke copies are adopted after external outlines: ' + mode, () => {
    const f = fixture(mode, true, true), out = f.convert();
    assert.equal(out.parentPage, f.doc.pages[1]);
    assert.equal(f.source.isValid, false);
    assert.ok(f.events.includes('copy-regroup-partial'));
    assert.equal(f.survivors().filter(x => x.constructor.name === 'Group').length, 1);
    assert.equal(f.survivors().filter(x => x.constructor.name === 'Polygon').length, mode ? 4 : 2);
});

for (const fault of ['copy-regroup-start', 'copy-regroup-partial', 'copy-regroup-drift', 'log', 'copy-geometry', 'copy-bounds', 'copy-page', 'copy-layer', 'copy-preexisting']) {
    test('external stroke copy refusal/failure preserves source: ' + fault, () => {
        const f = fixture(fault, true, true);
        const expected = fault === 'copy-geometry' ? /geometryEqual=false/ : fault === 'copy-bounds' ? /boundsEqual=false/ :
            fault === 'copy-page' ? /samePage=false/ : fault === 'copy-layer' ? /sameLayer=false/ :
            fault === 'copy-preexisting' ? /fresh=false/ : fault === 'log' ? /injected log/ : /./;
        assert.throws(f.convert, expected);
        if (fault.startsWith('copy-regroup')) assert.ok(f.events.includes(fault === 'copy-regroup-drift' ? 'copy-regroup-partial' : fault));
        assert.equal(f.source.isValid, true);
        assert.equal(f.source.parentStory.contents, 'Hello');
        assert.deepEqual(f.survivors(), [f.source, f.unrelated]);
    });
}

test('a source deletion that throws afterward retains the validated replacement', () => {
    const f = fixture('commit-after-delete');
    assert.throws(f.convert, /source removed, validated replacement retained/);
    assert.equal(f.source.isValid, false);
    assert.equal(f.survivors().filter(x => x.constructor.name === 'Polygon').length, 2);
    assert.equal(f.unrelated.isValid, true);
});

test('cleanup failures are reported and never cause source removal', () => {
    const f = fixture('rollback-fails');
    assert.throws(f.convert, /rollback incomplete.*cleanup failure/);
    assert.equal(f.source.isValid, true);
    assert.equal(f.unrelated.isValid, true);
    assert.equal(f.events.includes('commit'), false);
});

test('ignored cleanup removal is reported as incomplete rollback', () => {
    const f = fixture('ignore-cleanup');
    assert.throws(f.convert, /rollback incomplete.*owned art removal did not complete/);
    assert.equal(f.source.isValid, true);
    assert.equal(f.unrelated.isValid, true);
    assert.equal(f.events.includes('commit'), false);
});

test('center stroke uses original width, and no-stroke paint needs no stroke duplicate', () => {
    const centered = fixture();
    centered.source.parentStory.textStyleRanges[0].strokeAlignment = 'CENTER_ALIGNMENT';
    centered.convert();
    assert.equal(centered.survivors().find(x => x.name === 'stroke').strokeWeight, 2);
    const fillOnly = fixture();
    fillOnly.source.parentStory.textStyleRanges[0].strokeWeight = 0;
    fillOnly.convert();
    assert.equal(fillOnly.events.includes('stroke-duplicate'), false);
    assert.equal(fillOnly.survivors().filter(x => x.constructor.name === 'Polygon').length, 1);
});
