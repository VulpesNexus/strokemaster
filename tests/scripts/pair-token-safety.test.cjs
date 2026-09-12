// Pure Node fault-injection and label portability tests, not Adobe or IDML export tests.
const test = require('node:test');
const assert = require('node:assert/strict');
const { syncFixture } = require('./mock-runtime.cjs');

function sourceFixture(options) {
    const f = syncFixture(options);
    f.top.remove();
    f.bottom.tag = '';
    f.bottom.parentStory.texts[0].fillColor = { name: 'Black' };
    f.rt.load('ise-split-stroked-text.jsx');
    f.split = f.rt.globals.ISE_SPLIT;
    return f;
}

test('PURE pair: actual split writer emits one token in both role labels, without frame IDs', () => {
    const f = sourceFixture();
    const result = f.split.splitOne(f.bottom);
    assert.equal(result.done, true, result.reason);
    const pair = f.api.pairs(f.doc)[0];
    const fill = /^ISE3\|([^|]+)\|fill$/.exec(pair.top.tag);
    const stroke = /^ISE3\|([^|]+)\|stroke$/.exec(pair.bottom.tag);
    assert.ok(fill && stroke);
    assert.equal(fill[1], stroke[1]);
    assert.equal(pair.top.parentStory.texts[0].strokeColor.name, 'None');
    assert.equal(pair.bottom.parentStory.texts[0].fillColor.name, 'Paper');
    assert.equal(f.doc.extractLabel('ISE_SPLIT_DOC'), '');
});

test('PURE split: fill-only top has zero stroke weight and None color while bottom retains its stroke', () => {
    const f = sourceFixture();
    const original = f.bottom.parentStory.texts[0];
    const weight = original.strokeWeight, color = original.strokeColor.name;
    assert.ok(weight > 0);
    const result = f.split.splitOne(f.bottom);
    assert.equal(result.done, true, result.reason);
    const pair = f.api.pairs(f.doc)[0];
    const top = pair.top.parentStory.texts[0], bottom = pair.bottom.parentStory.texts[0];
    assert.equal(top.strokeWeight, 0);
    assert.equal(top.strokeColor.name, 'None');
    assert.equal(bottom.strokeWeight, weight);
    assert.equal(bottom.strokeColor.name, color);
});

test('PURE pair: numeric ID reassignment does not invalidate persisted labels', () => {
    const f = sourceFixture();
    assert.equal(f.split.splitOne(f.bottom).done, true);
    const pair = f.api.pairs(f.doc)[0];
    const labels = [pair.top.tag, pair.bottom.tag];
    pair.top.id = 8001;
    pair.bottom.id = 9002;
    f.doc.id = 7003;
    assert.equal(f.api.pairs(f.doc).length, 1);
    pair.top.parentStory.contents = 'after simulated ID remap';
    const result = f.api.resyncAll(f.doc);
    assert.equal(result.resynced, 1, result.skipped.join(';'));
    const after = f.api.pairs(f.doc)[0];
    assert.deepEqual([after.top.tag, after.bottom.tag], labels);
    assert.equal(after.bottom.parentStory.contents, 'after simulated ID remap');
});

test('PURE pair: a complete pair copied to another document matches only within that document', () => {
    const a = syncFixture();
    const b = syncFixture();
    b.doc.id = 77;
    b.top.id = 101;
    b.bottom.id = 102;
    b.top.tag = a.top.tag;
    b.bottom.tag = a.bottom.tag;
    b.top.parentStory.contents = 'recipient edit';
    const result = b.api.resyncAll(b.doc);
    assert.equal(result.resynced, 1, result.skipped.join(';'));
    assert.equal(a.bottom.parentStory.contents, 'old text');
    assert.equal(a.items.length, 2);
});

test('PURE pair: duplication of either role invalidates the entire token, including originals', () => {
    for (const role of ['top', 'bottom']) {
        const f = syncFixture();
        f[role].duplicate();
        const before = f.items.map(item => item.id);
        const scan = f.api.inspect(f.doc);
        assert.equal(scan.pairs.length, 0);
        assert.match(scan.skipped.join(';'), /ambiguous/);
        const result = f.api.resync({ top: f.top, bottom: f.bottom });
        assert.equal(result.done, false);
        assert.deepEqual(f.items.map(item => item.id), before);
        assert.equal(f.bottom.isValid, true);
    }
});

test('PURE pair: duplicate complete pairs and missing partners are refused', () => {
    const f = syncFixture();
    f.top.duplicate();
    f.bottom.duplicate();
    assert.equal(f.api.resyncAll(f.doc).resynced, 0);
    assert.equal(f.items.length, 4);
    const orphan = syncFixture();
    orphan.bottom.remove();
    assert.equal(orphan.api.pairs(orphan.doc).length, 0);
    assert.match(orphan.api.resyncAll(orphan.doc).skipped.join(';'), /incomplete/);
});

test('PURE pair: legacy and malformed labels never trigger positional migration or evaluation', () => {
    for (const tag of ['fill', '{"role":"stroke"}', 'ISE2|old-doc|fill|1|2', 'ISE3|bad|other', '$.global.INJECTED=true']) {
        const f = syncFixture();
        f.top.tag = tag;
        const before = f.items.map(item => item.id);
        const result = f.api.resyncAll(f.doc);
        assert.equal(result.resynced, 0);
        assert.match(result.skipped.join(';'), /legacy or malformed/);
        assert.deepEqual(f.items.map(item => item.id), before);
        assert.equal(f.rt.globals.INJECTED, undefined);
    }
});

test('PURE pair: different tokens at the same bounds remain distinct pairs', () => {
    const f = syncFixture();
    const topB = f.frame('fill', 'second edited'), bottomB = f.frame('stroke', 'second old');
    f.bind(topB, bottomB, 'second-token');
    f.items.push(bottomB, topB);
    assert.equal(f.api.pairs(f.doc).length, 2);
    const result = f.api.resyncAll(f.doc);
    assert.equal(result.resynced, 2, result.skipped.join(';'));
    const pairs = f.api.pairs(f.doc);
    for (const pair of pairs) { assert.equal(pair.top.parentStory.contents, pair.bottom.parentStory.contents); }
});

test('PURE pair: bounds changes do not lose identity and native duplication preserves them', () => {
    const f = syncFixture();
    f.top.geometricBounds = [20, 30, 120, 180];
    assert.equal(f.api.pairs(f.doc).length, 1);
    const result = f.api.resyncAll(f.doc);
    assert.equal(result.resynced, 1, result.skipped.join(';'));
    assert.deepEqual(f.api.pairs(f.doc)[0].bottom.geometricBounds, f.top.geometricBounds);
});

test('PURE sync: the old bottom exists until replacement paint, labels, and order validate', () => {
    const f = syncFixture();
    const remove = f.bottom.remove;
    let checked = false;
    f.bottom.remove = function () {
        assert.equal(this.isValid, true);
        const fresh = f.items.find(item => item !== f.top && item !== f.bottom);
        assert.ok(fresh.index > f.top.index);
        assert.equal(fresh.tag, f.bottom.tag);
        assert.equal(fresh.parentStory.contents, f.top.parentStory.contents);
        assert.equal(fresh.parentStory.texts[0].strokeWeight, 8);
        assert.equal(fresh.parentStory.texts[0].fillColor.name, 'Paper');
        checked = true;
        remove.call(this);
    };
    const result = f.api.resync({ top: f.top, bottom: f.bottom });
    assert.equal(result.done, true, result.reason);
    assert.equal(checked, true);
});

for (const fault of ['stroke', 'fill', 'contents', 'bounds', 'label']) {
    test(`PURE sync: rejected ${fault} staging preserves the old bottom and cleans the duplicate`, () => {
        const f = syncFixture();
        const duplicate = f.top.duplicate;
        f.top.duplicate = function () {
            const copy = duplicate.call(this);
            const paint = copy.parentStory.texts[0];
            if (fault === 'stroke') { Object.defineProperty(paint, 'strokeWeight', { get: () => 0, set() {} }); }
            if (fault === 'fill') { Object.defineProperty(paint, 'fillTint', { get: () => 75, set() {} }); }
            if (fault === 'contents') { copy.parentStory.contents = 'corrupt copy'; }
            if (fault === 'bounds') { copy.geometricBounds[0] += 10; }
            if (fault === 'label') { copy.insertLabel = function () {}; }
            return copy;
        };
        const before = f.items.map(item => item.id);
        const labels = [f.top.tag, f.bottom.tag];
        const result = f.api.resync({ top: f.top, bottom: f.bottom });
        assert.equal(result.done, false);
        assert.deepEqual(f.items.map(item => item.id), before);
        assert.deepEqual([f.top.tag, f.bottom.tag], labels);
        assert.equal(f.bottom.isValid, true);
    });
}

test('PURE sync: no-op old removal is failure with the original pair retained', () => {
    const f = syncFixture();
    f.bottom.remove = () => {};
    const result = f.api.resync({ top: f.top, bottom: f.bottom });
    assert.equal(result.done, false);
    assert.match(result.reason, /no effect/);
    assert.deepEqual(f.items, [f.top, f.bottom]);
});

test('PURE sync: cleanup failure is explicit and the remaining duplicate token cannot sync', () => {
    const f = syncFixture({ removeFails: true });
    const duplicate = f.top.duplicate;
    f.top.duplicate = function () {
        const fresh = duplicate.call(this);
        fresh.remove = () => { throw Error('injected cleanup refusal'); };
        return fresh;
    };
    const result = f.api.resync({ top: f.top, bottom: f.bottom });
    assert.equal(result.done, false);
    assert.match(result.reason, /MANUAL RECOVERY.*cleanup failed/);
    assert.equal(f.bottom.isValid, true);
    assert.equal(f.items.length, 3);
    assert.equal(f.api.pairs(f.doc).length, 0);
});

test('PURE sync: removal that invalidates then throws retains the validated replacement', () => {
    const f = syncFixture();
    const remove = f.bottom.remove;
    f.bottom.remove = function () { remove.call(this); throw Error('injected post-removal error'); };
    const result = f.api.resync({ top: f.top, bottom: f.bottom });
    assert.equal(result.done, false);
    assert.match(result.reason, /validated replacement retained/);
    assert.equal(f.items.length, 2);
    assert.equal(f.api.pairs(f.doc)[0].bottom.parentStory.contents, 'new text');
});

for (const constraint of ['locked', 'layerLocked', 'threaded', 'grouped', 'otherLayer', 'otherPage', 'mixedStroke']) {
    test(`PURE sync: ${constraint} refuses before creating or removing frames`, () => {
        const f = syncFixture();
        if (constraint === 'locked') { f.bottom.locked = true; }
        if (constraint === 'layerLocked') { f.bottom.itemLayer.locked = true; }
        if (constraint === 'threaded') { f.top.nextTextFrame = f.bottom; }
        if (constraint === 'grouped') { f.top.parent = { constructor: { name: 'Group' }, parent: f.doc }; }
        if (constraint === 'otherLayer') { f.bottom.itemLayer = { id: 999, locked: false, constructor: { name: 'Layer' } }; }
        if (constraint === 'otherPage') { f.bottom.parentPage = { id: 998, constructor: { name: 'Page' } }; }
        if (constraint === 'mixedStroke') { f.bottom.parentStory.textStyleRanges.push({ ...f.bottom.parentStory.texts[0], strokeWeight: 9 }); }
        const result = f.api.resync({ top: f.top, bottom: f.bottom });
        assert.equal(result.done, false);
        assert.deepEqual(f.items, [f.top, f.bottom]);
    });
}

test('PURE sync: explicit resync carries font edits; automatic polling remains contents-only', () => {
    const f = syncFixture({ contents: ['same', 'same'] });
    f.bottom.parentStory.texts[0].appliedFont = 'Original Font';
    f.top.parentStory.texts[0].appliedFont = 'Edited Font';
    const id = f.bottom.id;
    f.api.tick();
    assert.equal(f.api.pairs(f.doc)[0].bottom.id, id);
    assert.match(f.api.status(), /automatic=contents-only/);
    const result = f.api.resyncAll(f.doc);
    assert.equal(result.resynced, 1);
    assert.equal(f.api.pairs(f.doc)[0].bottom.parentStory.texts[0].appliedFont, 'Edited Font');
});

test('PURE sync: scoped tick repairs only its target without reading or mutating another open document', () => {
    const f = syncFixture();
    const other = syncFixture({ contents: ['unrelated edited text', 'unrelated stale text'] });
    other.doc.id = 77;
    const accesses = [];
    const guarded = new Proxy(other.doc, {
        get(target, key, receiver) { accesses.push(`read:${String(key)}`); return Reflect.get(target, key, receiver); },
        set(target, key, value, receiver) { accesses.push(`write:${String(key)}`); return Reflect.set(target, key, value, receiver); }
    });
    const snapshot = () => other.items.map(item => ({
        id: item.id, valid: item.isValid, label: item.tag, contents: item.parentStory.contents,
        weight: item.parentStory.texts[0].strokeWeight, color: item.parentStory.texts[0].strokeColor.name
    }));
    const before = snapshot();
    f.rt.app.documents = [guarded, f.doc];
    f.rt.app.activeDocument = guarded;
    f.api.tick(f.doc);
    assert.deepEqual(accesses, [], 'an unrelated open document must not even be inspected');
    assert.deepEqual(snapshot(), before);
    assert.deepEqual(other.doc.closeCalls, []);
    assert.equal(f.api.pairs(f.doc)[0].bottom.parentStory.contents, 'new text');
    assert.equal(f.bottom.isValid, false, 'the target replacement actually committed');
    assert.equal(f.rt.globals.ISE_SYNC_COUNT, 1);
    assert.equal(f.rt.globals.ISE_SYNC_LAST, '');
});

test('PURE sync: unchanged text polling does not inspect stroke or font formatting', () => {
    const f = syncFixture({ contents: ['same', 'same'] });
    Object.defineProperty(f.bottom.parentStory.texts[0], 'strokeWeight', { get() { throw Error('formatting was read'); } });
    f.api.tick();
    assert.equal(f.rt.globals.ISE_SYNC_LAST, '');
    assert.equal(f.rt.globals.ISE_SYNC_COUNT, 0);
});

test('PURE split: label-write failure restores source appearance and removes the copy', () => {
    const f = sourceFixture();
    const duplicate = f.bottom.duplicate;
    f.bottom.duplicate = function () {
        const copy = duplicate.call(this);
        copy.insertLabel = () => { throw Error('injected label failure'); };
        return copy;
    };
    const result = f.split.splitOne(f.bottom);
    assert.equal(result.done, false);
    assert.match(result.reason, /injected label failure/);
    assert.deepEqual(f.items, [f.bottom]);
    assert.equal(f.bottom.tag, '');
    assert.equal(f.bottom.parentStory.texts[0].fillColor.name, 'Black');
    assert.equal(f.bottom.parentStory.texts[0].strokeWeight, 8);
});

test('PURE split: token collisions are detected before another source is changed', () => {
    const f = sourceFixture();
    f.rt.evaluate('Date = function () { this.getTime = function () { return 1; }; }; Math.random = function () { return 0; };');
    assert.equal(f.split.splitOne(f.bottom).done, true);
    const source = f.frame('', 'another source');
    f.items.push(source);
    const result = f.split.splitOne(source);
    assert.equal(result.done, false);
    assert.match(result.reason, /token collision/);
    assert.equal(f.items.length, 3);
    assert.equal(source.tag, '');
});

const appearanceFaults = {
    background: frame => { frame.fillColor = { name: 'Paper' }; },
    border: frame => { frame.strokeColor = { name: 'Black' }; },
    opacity: frame => { frame.transparencySettings.blendingSettings.opacity = 50; },
    textBlend: frame => { frame.contentTransparencySettings.blendingSettings.blendMode = 'MULTIPLY'; },
    knockout: frame => { frame.fillTransparencySettings.blendingSettings.knockoutGroup = true; },
    isolate: frame => { frame.strokeTransparencySettings.blendingSettings.isolateBlending = true; },
    shadow: frame => { frame.contentTransparencySettings.dropShadowSettings.mode = 'DROP'; },
    feather: frame => { frame.transparencySettings.featherSettings.mode = 'STANDARD'; },
    missingSettings: frame => { delete frame.fillTransparencySettings; },
    unreadableSettings: frame => { Object.defineProperty(frame, 'strokeTransparencySettings', { get() { throw Error('unreadable appearance'); } }); }
};
for (const effect of ['innerShadowSettings', 'outerGlowSettings', 'innerGlowSettings', 'bevelAndEmbossSettings', 'satinSettings', 'directionalFeatherSettings', 'gradientFeatherSettings']) {
    appearanceFaults[effect] = frame => { frame.transparencySettings[effect].applied = true; };
}
for (const [fault, inject] of Object.entries(appearanceFaults)) {
    test(`PURE appearance: split and sync refuse ${fault} before mutation`, () => {
        const source = sourceFixture();
        inject(source.bottom);
        const split = source.split.splitOne(source.bottom);
        assert.equal(split.done, false, fault);
        assert.deepEqual(source.items, [source.bottom]);
        assert.equal(source.bottom.tag, '');
        assert.equal(source.bottom.parentStory.texts[0].fillColor.name, 'Black');
        for (const role of ['top', 'bottom']) {
            const f = syncFixture();
            inject(f[role]);
            const result = f.api.resync({ top: f.top, bottom: f.bottom });
            assert.equal(result.done, false, `${fault} on ${role}`);
            assert.deepEqual(f.items, [f.top, f.bottom]);
            assert.equal(f.bottom.isValid, true);
        }
    });
}

test('PURE split: failed source restoration retains the duplicate and disables pairing', () => {
    const f = sourceFixture();
    const paint = f.bottom.parentStory.texts[0];
    let color = paint.fillColor;
    Object.defineProperty(paint, 'fillColor', {
        enumerable: true,
        get: () => color,
        set(value) { if (value.name === 'Black') { throw Error('injected restore failure'); } color = value; }
    });
    const duplicate = f.bottom.duplicate;
    f.bottom.duplicate = function () {
        const copy = duplicate.call(this);
        const insert = copy.insertLabel;
        copy.insertLabel = function (key, value) {
            if (value) { throw Error('injected label failure'); }
            insert.call(this, key, value);
        };
        return copy;
    };
    const result = f.split.splitOne(f.bottom);
    assert.equal(result.done, false);
    assert.match(result.reason, /MANUAL RECOVERY.*original restore failed.*duplicate retained/);
    assert.equal(f.items.length, 2);
    assert.equal(f.items.every(item => item.isValid && item.tag === ''), true);
    assert.equal(f.items.find(item => item !== f.bottom).parentStory.texts[0].fillColor.name, 'Black');
    assert.equal(f.api.pairs(f.doc).length, 0);
});

test('PURE pair: a duplicate token on the pasteboard invalidates the page pair', () => {
    const f = syncFixture();
    const copy = f.frame('fill', 'pasteboard copy');
    copy.tag = f.top.tag;
    copy.parentPage = null;
    Object.defineProperty(f.doc, 'allPageItems', { get: () => f.items.concat(copy) });
    const scan = f.api.inspect(f.doc);
    assert.equal(scan.pairs.length, 0);
    assert.match(scan.skipped.join(';'), /ambiguous/);
    assert.equal(f.api.resync({ top: f.top, bottom: f.bottom }).done, false);
    assert.deepEqual(f.items, [f.top, f.bottom]);
});
