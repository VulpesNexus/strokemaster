// Actual-host fault checks for split and resync (Q09), on disposable documents only: every
// constraint and appearance the scripts must refuse, refused before anything changes, as
// tests/scripts/pair-token-safety.test.cjs checks on mocks. The failures midway through (a
// cleanup refused, a removal that throws, a label that cannot be written) need a page item's
// methods overridden; InDesign 21.6 holds them read only, so they are recorded as not
// injectable here and stay covered by the mocks.
(function () {
    var here = File($.fileName).parent;
    var oldUI = app.scriptPreferences.userInteractionLevel, owned = [], initial = [], checks = [], error = null;
    var savedSync = $.global.ISE_SYNC, savedSplit = $.global.ISE_SPLIT, savedLib = $.global.ISE_LIB;
    function state(d) { return [d.id, d.saved, d.modified, d.allPageItems.length].join('|'); }
    for (var n = 0; n < app.documents.length; n++) initial.push({ doc: app.documents[n], state: state(app.documents[n]) });
    function check(name, ok, detail) { checks.push({ name: name, passed: Boolean(ok), detail: detail === undefined ? null : String(detail) }); }
    var report = { version: app.version, checks: checks }, lib = null, split = null, sync = null;

    function newDoc() {
        var doc = app.documents.add(false);
        owned.push(doc);
        doc.documentPreferences.pagesPerDocument = 2;
        return doc;
    }
    function closeDoc(doc) { try { if (doc.isValid) doc.close(SaveOptions.NO); } catch (e) {} }
    function frame(doc, text) {
        var f = doc.pages[0].textFrames.add({ geometricBounds: [20, 20, 90, 150] });
        f.fillColor = doc.swatches.itemByName('None');
        f.strokeColor = doc.swatches.itemByName('None');
        f.contents = text;
        var t = f.parentStory.texts[0];
        t.appliedFont = 'Minion Pro\tRegular'; t.pointSize = 20;
        t.fillColor = doc.swatches.itemByName('Black'); t.strokeColor = doc.swatches.itemByName('Paper');
        t.strokeWeight = 3; t.strokeAlignment = TextStrokeAlign.OUTSIDE_ALIGNMENT; t.miterLimit = 4;
        return f;
    }
    // Everything a refusal must leave as it was: each page item, its label, place, and paint.
    function snapshot(doc) {
        var items = doc.allPageItems, rows = [];
        for (var i = 0; i < items.length; i++) {
            var it = items[i], row = [it.constructor.name, it.id, it.extractLabel('ISE_SPLIT'), it.locked, it.itemLayer.name, it.index];
            try { row.push(it.parentPage ? it.parentPage.name : '-'); } catch (e) { row.push('?'); }
            if (it.constructor.name === 'TextFrame') {
                var t = it.parentStory.texts[0];
                row.push(it.parentStory.contents, t.strokeWeight, String(t.fillColor.name), String(t.strokeColor.name), t.pointSize);
            }
            rows.push(row.join('|'));
        }
        rows.sort();
        return rows.join('\n');
    }
    function pairOf(doc, text) {
        var r = split.splitOne(frame(doc, text));
        if (!r.done) throw new Error('setup split refused: ' + r.reason);
        var pairs = sync.pairs(doc);
        if (pairs.length !== 1) throw new Error('setup found ' + pairs.length + ' pairs');
        pairs[0].top.parentStory.contents = text + ' edited';
        return pairs[0];
    }
    function guarded(name, f) {
        try { f(); } catch (e) { check(name + ': ran', false, e.message + ' line ' + e.line); }
    }

    try {
        app.scriptPreferences.userInteractionLevel = UserInteractionLevels.NEVER_INTERACT;
        $.evalFile(new File(here.fsName + '/ise-lib.jsx')); lib = $.global.ISE_LIB;
        $.evalFile(new File(here.fsName + '/ise-split-stroked-text.jsx'));
        $.evalFile(new File(here.fsName + '/ise-live-sync.jsx'));
        split = $.global.ISE_SPLIT; sync = $.global.ISE_SYNC;

        // Control: an ordinary pair resyncs.
        guarded('control', function () {
            var doc = newDoc(), pair = pairOf(doc, 'control');
            var r = sync.resync(pair);
            check('control: an ordinary pair resyncs', r.done && sync.pairs(doc)[0].bottom.parentStory.contents === 'control edited', r.reason);
            closeDoc(doc);
        });

        // Constraints: resync refuses before creating or removing anything.
        var constraints = {
            locked: function (doc, p) { p.bottom.locked = true; },
            layerLocked: function (doc, p) { p.bottom.itemLayer.locked = true; },
            threaded: function (doc, p) { p.top.nextTextFrame = p.bottom; },
            grouped: function (doc, p) { doc.pages[0].groups.add([p.top, p.bottom]); },
            otherLayer: function (doc, p) { p.bottom.itemLayer = doc.layers.add({ name: 'other' }); },
            otherPage: function (doc, p) { p.bottom.move(doc.pages[1]); },
            mixedStroke: function (doc, p) { p.bottom.parentStory.characters[0].strokeWeight = 9; }
        };
        for (var c in constraints) {
            if (!constraints.hasOwnProperty(c)) continue;
            (function (name, apply) {
                guarded('constraint ' + name, function () {
                    var doc = newDoc(), pair = pairOf(doc, name);
                    apply(doc, pair);
                    var before = snapshot(doc), r = sync.resync(pair), after = snapshot(doc);
                    check('constraint ' + name + ': resync refused, nothing changed', !r.done && before === after, r.reason);
                    try { doc.layers.everyItem().locked = false; } catch (e) {}
                    closeDoc(doc);
                });
            })(c, constraints[c]);
        }

        // Appearance the pair cannot carry: split and resync refuse it on either frame.
        var appearance = {
            background: null,   // the frame filled with Paper (set below, with the document's swatch)
            border: null,       // the frame stroked 1 pt Black (likewise)
            opacity: function (f) { f.transparencySettings.blendingSettings.opacity = 50; },
            textBlend: function (f) { f.contentTransparencySettings.blendingSettings.blendMode = BlendMode.MULTIPLY; },
            knockout: function (f) { f.fillTransparencySettings.blendingSettings.knockoutGroup = true; },
            isolate: function (f) { f.strokeTransparencySettings.blendingSettings.isolateBlending = true; },
            shadow: function (f) { f.contentTransparencySettings.dropShadowSettings.mode = ShadowMode.DROP; },
            feather: function (f) { f.transparencySettings.featherSettings.mode = FeatherMode.STANDARD; }
        };
        var effects = ['innerShadowSettings', 'outerGlowSettings', 'innerGlowSettings', 'bevelAndEmbossSettings', 'satinSettings',
                       'directionalFeatherSettings', 'gradientFeatherSettings'];
        for (var e = 0; e < effects.length; e++) {
            (function (fx) { appearance[fx] = function (f) { f.transparencySettings[fx].applied = true; }; })(effects[e]);
        }
        for (var a in appearance) {
            if (!appearance.hasOwnProperty(a)) continue;
            (function (name, apply) {
                guarded('appearance ' + name, function () {
                    var doc = newDoc();
                    var paper = doc.swatches.itemByName('Paper'), black = doc.swatches.itemByName('Black');
                    var set = name === 'background' ? function (f) { f.fillColor = paper; }
                            : name === 'border' ? function (f) { f.strokeColor = black; f.strokeWeight = 1; } : apply;
                    var source = frame(doc, 'source ' + name);
                    set(source);
                    var before = snapshot(doc), r = split.splitOne(source), after = snapshot(doc);
                    check('appearance ' + name + ': split refused, nothing changed', !r.done && before === after, r.reason);
                    source.remove();
                    var roles = ['top', 'bottom'];
                    for (var k = 0; k < roles.length; k++) {
                        var pair = pairOf(doc, name + ' ' + roles[k]);
                        set(pair[roles[k]]);
                        before = snapshot(doc); r = sync.resync(pair); after = snapshot(doc);
                        check('appearance ' + name + ' on the ' + roles[k] + ': resync refused, nothing changed', !r.done && before === after, r.reason);
                        pair.top.remove(); pair.bottom.remove();
                    }
                    closeDoc(doc);
                });
            })(a, appearance[a]);
        }

        // Failures midway: a page item's methods overridden, where the host allows it.
        var midway = {
            'cleanup refused': ['duplicate', function (doc) {
                var pair = pairOf(doc, 'cleanup'), dup = pair.top.duplicate;
                pair.top.duplicate = function () {
                    var fresh = dup.call(this);
                    fresh.parentStory.contents = 'corrupt copy';   // rejected staging: the copy must be cleaned up
                    fresh.remove = function () { throw new Error('injected cleanup refusal'); };
                    return fresh;
                };
                var r = sync.resync(pair);
                return [!r.done && /MANUAL RECOVERY/.test(r.reason) && pair.bottom.isValid && sync.pairs(doc).length === 0, r.reason];
            }],
            'removal throws': ['remove', function (doc) {
                var pair = pairOf(doc, 'removal'), rm = pair.bottom.remove;
                pair.bottom.remove = function () { rm.call(this); throw new Error('injected post-removal error'); };
                var r = sync.resync(pair), pairs = sync.pairs(doc);
                return [!r.done && /validated replacement retained/.test(r.reason) && pairs.length === 1 &&
                        pairs[0].bottom.parentStory.contents === 'removal edited', r.reason];
            }],
            'label failure on split': ['duplicate', function (doc) {
                var source = frame(doc, 'label'), dup = source.duplicate;
                source.duplicate = function () {
                    var copy = dup.call(this);
                    copy.insertLabel = function () { throw new Error('injected label failure'); };
                    return copy;
                };
                var before = snapshot(doc), r = split.splitOne(source), after = snapshot(doc);
                return [!r.done && /injected label failure/.test(r.reason) && before === after, r.reason];
            }]
        };
        report.notInjectable = [];
        for (var m in midway) {
            if (!midway.hasOwnProperty(m)) continue;
            (function (name, method, run) {
                var doc = newDoc();
                try {
                    var result = run(doc);
                    check(name + ': handled as on the mocks', result[0], result[1]);
                } catch (x) {
                    if (/read only/.test(x.message)) {
                        report.notInjectable.push(name);
                        check(name + ': not injectable in the host (' + method + ' is read only)', true, x.message);
                    } else {
                        check(name + ': ran', false, x.message + ' line ' + x.line);
                    }
                }
                closeDoc(doc);
            })(m, midway[m][0], midway[m][1]);
        }
    } catch (e) { error = e.message + ' line ' + e.line; }
    finally {
        for (n = owned.length - 1; n >= 0; n--) { try { if (owned[n].isValid) { owned[n].close(SaveOptions.NO); } } catch (cleanup) { error = 'cleanup: ' + cleanup.message; } }
        app.scriptPreferences.userInteractionLevel = oldUI;
        $.global.ISE_SYNC = savedSync; $.global.ISE_SPLIT = savedSplit; $.global.ISE_LIB = savedLib;
    }
    check('document count preserved', app.documents.length === initial.length);
    for (n = 0; n < initial.length; n++) check('user document ' + n + ' preserved', state(initial[n].doc) === initial[n].state);
    check('UI restored', app.scriptPreferences.userInteractionLevel === oldUI);
    report.error = error; report.passed = !error;
    for (n = 0; n < checks.length; n++) if (!checks[n].passed) report.passed = false;
    if (!lib) throw new Error(error || 'Library unavailable');
    return lib.toJSON(report);
})();
