// Pure Node regression tests of the actual sync source, not Adobe host tests.
// These six contracts were previously executed expected failures; they must now pass.
const test = require('node:test');
const assert = require('node:assert/strict');
const { syncFixture } = require('./mock-runtime.cjs');

function contract(name, act, verify) {
    test('PURE sync regression: ' + name, () => verify(act()));
}

test('PURE sync control: rebuilding an ordinary pair preserves text, stroke, and order', () => {
    const f = syncFixture();
    const result = f.api.resyncAll(f.doc);
    assert.equal(result.resynced, 1);
    assert.equal(result.skipped.length, 0);
    assert.equal(f.bottom.isValid, false);
    assert.equal(f.items.length, 2);
    const pair = f.api.pairs(f.doc)[0];
    assert.equal(pair.bottom.parentStory.contents, pair.top.parentStory.contents);
    assert.equal(pair.bottom.parentStory.texts[0].strokeColor.name, 'Paper');
    assert.equal(pair.bottom.parentStory.texts[0].strokeWeight, 8);
    assert.ok(pair.bottom.index > pair.top.index);
});

test('PURE sync control: watcher notices an edit after its initial baseline', () => {
    const f = syncFixture({ contents: ['same', 'same'] });
    const task = f.api.start(1000);
    task.listeners[0].callback();
    f.top.parentStory.contents = 'edited after baseline';
    task.listeners[0].callback();
    assert.equal(f.api.pairs(f.doc)[0].bottom.parentStory.contents, 'edited after baseline');
    assert.equal(f.rt.globals.ISE_SYNC_COUNT, 1);
});

contract('find a valid split pair on page 2', () => {
    const f = syncFixture();
    assert.equal(f.api.pairs(f.doc).length, 1);
    f.doc.pages = [{ allPageItems: [] }, f.doc.pages[0]];
    return f.api.pairs(f.doc);
}, pairs => assert.equal(pairs.length, 1));

contract('refuse duplicated pair tokens at identical bounds', () => {
    const f = syncFixture({ contents: ['A', 'A'] });
    assert.equal(f.api.pairs(f.doc).length, 1);
    const topB = f.frame('fill', 'B');
    const bottomB = f.frame('stroke', 'B');
    f.bind(topB, bottomB);
    f.items.splice(0, f.items.length, f.top, topB, bottomB, f.bottom);
    // Copied labels make the entire token ambiguous, including the original pair.
    return f.api.pairs(f.doc);
}, pairs => assert.equal(pairs.length, 0));

contract('explicit resync propagates formatting-only edits', () => {
    const f = syncFixture({ contents: ['same', 'same'] });
    assert.equal(f.api.pairs(f.doc).length, 1);
    assert.equal(f.bottom.parentStory.texts[0].pointSize, 24);
    f.top.parentStory.texts[0].pointSize = 48;
    f.api.resyncAll(f.doc);
    return f.api.pairs(f.doc)[0].bottom.parentStory.texts[0].pointSize;
}, size => assert.equal(size, 48));

contract('watch reconciles a mismatch present when watching starts', () => {
    const f = syncFixture();
    assert.notEqual(f.top.parentStory.contents, f.bottom.parentStory.contents);
    const task = f.api.start(1000);
    task.listeners[0].callback();
    task.listeners[0].callback();
    return f.api.pairs(f.doc)[0].bottom.parentStory.contents;
}, contents => assert.equal(contents, 'new text'));

contract('failed removal of the old bottom leaves no duplicate behind', () => {
    const f = syncFixture({ removeFails: true });
    const before = f.items.map(item => item.id);
    const result = f.api.resync({ top: f.top, bottom: f.bottom });
    assert.equal(result.done, false);
    assert.match(result.reason, /Injected locked bottom/);
    assert.equal(f.bottom.isValid, true);
    return { before, after: f.items.map(item => item.id) };
}, state => assert.deepEqual(state.after, state.before));

contract('blocked stacking is a failed resync, not a successful correction', () => {
    const f = syncFixture({ orderFails: true });
    return f.api.resync({ top: f.top, bottom: f.bottom });
}, result => assert.equal(result.done, false));
