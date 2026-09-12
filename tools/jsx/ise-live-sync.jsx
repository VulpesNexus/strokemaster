// ise-live-sync.jsx - synchronize explicitly identified split pairs.
// Pure ASCII. Requires ise-lib.jsx for the optional driver below.
// ISE_SPLIT labels use ISE3|unique-pair-token|role, with no persisted numeric IDs.
// Matching is document-local and requires exactly one fill and one stroke for the token.
// Legacy labels and duplicate tokens are refused. IDML portability still needs host proof.
// Explicit resync rebuilds formatting as well as contents. Automatic polling compares
// CONTENTS ONLY, including on its first tick. There is no save/export event backstop.
// Uniform Outside strokes, unthreaded frames, and one unlocked stacking context are required.
$.global.ISE_SYNC = (function () {
    var api = {}, LABEL = 'ISE_SPLIT', TASK = 'iseLiveSyncTask';
    var busy = false;
    api.log = [];
    function L(s) { api.log.push(String(s)); if (api.log.length > 100) { api.log.shift(); } }
    api.L = L;

    function name(c) { return String(c.name); }
    function sameObject(a, b) {
        return a === b || (a && b && a.id !== undefined && a.id === b.id && a.constructor.name === b.constructor.name);
    }
    function documentOf(item) {
        for (var n = 0; item && n < 32; n++, item = item.parent) {
            if (item.constructor.name === 'Document') { return item; }
        }
        throw new Error('cannot resolve frame document');
    }
    function parse(item) {
        var raw = String(item.extractLabel(LABEL));
        var m = /^ISE3\|([A-Za-z0-9-]{1,96})\|(fill|stroke)$/.exec(raw);
        if (!m) { throw new Error('legacy or malformed pair label; automatic migration is refused'); }
        return { token: m[1], role: m[2], raw: raw };
    }
    function label(token, role) { return 'ISE3|' + token + '|' + role; }
    function pairInfo(pair, staged) {
        var top = pair.top, bottom = pair.bottom;
        if (!top || !bottom || !top.isValid || !bottom.isValid) { throw new Error('a frame of the pair is gone'); }
        var doc = documentOf(top);
        if (!sameObject(doc, documentOf(bottom))) { throw new Error('frames belong to different documents'); }
        var a = parse(top), b = parse(bottom);
        if (a.role !== 'fill' || b.role !== 'stroke' || a.token !== b.token) { throw new Error('pair labels do not agree'); }
        if (!staged) {
            var scan = api.inspect(doc), found = false;
            for (var i = 0; i < scan.pairs.length; i++) {
                if (sameObject(scan.pairs[i].top, top) && sameObject(scan.pairs[i].bottom, bottom)) { found = true; break; }
            }
            if (!found) { throw new Error('pair token is ambiguous or does not identify these frames'); }
        }
        return { doc: doc, token: a.token };
    }

    api.inspect = function (doc) {
        var tokens = {}, seen = {}, pairs = [], skipped = [], i;
        // Include pasteboard/group descendants when checking token ambiguity, even though
        // mutation is limited to supported page frames in contextCheck.
        var items = doc.allPageItems;
        for (i = 0; i < items.length; i++) {
            var item = items[i], key;
            try {
                if (item.constructor.name !== 'TextFrame' || !item.isValid) { continue; }
                key = 'f' + item.id;
                if (seen[key]) { continue; }
                seen[key] = true;
                if (String(item.extractLabel(LABEL)) === '') { continue; }
                var meta = parse(item), tk = 'p' + meta.token;
                if (!tokens[tk]) { tokens[tk] = { fill: [], stroke: [], token: meta.token }; }
                tokens[tk][meta.role].push(item);
            } catch (e) { skipped.push('frame ' + item.id + ': ' + e.message); }
        }
        for (var id in tokens) {
            if (!tokens.hasOwnProperty(id)) { continue; }
            var group = tokens[id];
            if (group.fill.length !== 1 || group.stroke.length !== 1) {
                skipped.push('pair ' + group.token + ': ambiguous or incomplete token (fill=' + group.fill.length + ', stroke=' + group.stroke.length + ')');
            } else {
                pairs.push({ top: group.fill[0], bottom: group.stroke[0] });
            }
        }
        return { pairs: pairs, skipped: skipped };
    };
    api.pairs = function (doc) { return api.inspect(doc).pairs; };

    function strokeOf(bottom) {
        var ranges = bottom.parentStory.textStyleRanges;
        if (!ranges.length) { return null; }
        var first = ranges[0], spec = {
            color: name(first.strokeColor), tint: first.strokeTint, weight: first.strokeWeight,
            align: String(first.strokeAlignment), join: String(first.endJoin),
            miter: first.miterLimit, overprint: first.overprintStroke
        };
        for (var i = 1; i < ranges.length; i++) {
            var r = ranges[i];
            if (name(r.strokeColor) !== spec.color || r.strokeTint !== spec.tint || r.strokeWeight !== spec.weight ||
                String(r.strokeAlignment) !== spec.align || String(r.endJoin) !== spec.join ||
                r.miterLimit !== spec.miter || r.overprintStroke !== spec.overprint) { return null; }
        }
        return spec;
    }
    function sameStroke(a, b) {
        if (!a || !b) { return false; }
        for (var k in a) { if (a.hasOwnProperty(k) && a[k] !== b[k]) { return false; } }
        return true;
    }
    // Duplication is supported only for frames without object paint or transparency effects.
    // Unknown or unreadable appearance properties fail closed; do not assume they are off.
    function plainAppearance(tf) {
        if (String(tf.fillColor.name) !== 'None' || String(tf.strokeColor.name) !== 'None') {
            throw new Error('frame backgrounds and borders are unsupported');
        }
        var targets = ['transparencySettings', 'contentTransparencySettings', 'fillTransparencySettings', 'strokeTransparencySettings'];
        var effects = ['innerShadowSettings', 'outerGlowSettings', 'innerGlowSettings', 'bevelAndEmbossSettings', 'satinSettings', 'directionalFeatherSettings', 'gradientFeatherSettings'];
        for (var t = 0; t < targets.length; t++) {
            var settings = tf[targets[t]], blend = settings.blendingSettings;
            if (String(blend.blendMode) !== String(BlendMode.NORMAL) || Number(blend.opacity) !== 100 ||
                blend.knockoutGroup !== false || blend.isolateBlending !== false) {
                throw new Error(targets[t] + ': blending or opacity is unsupported');
            }
            if (String(settings.dropShadowSettings.mode) !== String(ShadowMode.NONE) ||
                String(settings.featherSettings.mode) !== String(FeatherMode.NONE)) {
                throw new Error(targets[t] + ': shadow or feather is unsupported');
            }
            for (var e = 0; e < effects.length; e++) {
                if (settings[effects[e]].applied !== false) { throw new Error(targets[t] + ': ' + effects[e] + ' is active or unverified'); }
            }
        }
    }

    function contextCheck(top, bottom) {
        var frames = [top, bottom];
        for (var i = 0; i < frames.length; i++) {
            var f = frames[i];
            if (f.previousTextFrame !== null || f.nextTextFrame !== null) { throw new Error('threaded frames are refused'); }
            if (!f.parentPage || (f.parent.constructor.name !== 'Spread' && f.parent.constructor.name !== 'Page')) {
                throw new Error('only page frames directly in a spread or page are supported');
            }
            if (f.locked || f.itemLayer.locked) { throw new Error('frame or layer is locked'); }
            plainAppearance(f);
        }
        if (!sameObject(top.parent, bottom.parent) || !sameObject(top.parentPage, bottom.parentPage) || !sameObject(top.itemLayer, bottom.itemLayer)) {
            throw new Error('frames must share a page, parent, and layer');
        }
    }
    function sameBounds(a, b) {
        for (var i = 0; i < 4; i++) {
            if (!isFinite(Number(a[i])) || !isFinite(Number(b[i])) || Math.abs(Number(a[i]) - Number(b[i])) > 0.0001) { return false; }
        }
        return true;
    }
    function placeBehind(item, reference) {
        for (var steps = 0; item.index < reference.index && steps < 4; steps++) {
            var before = item.index;
            item.sendBackward();
            if (item.index === before) { break; }
        }
        return item.index > reference.index;
    }
    function validateFresh(fresh, top, spec) {
        contextCheck(top, fresh);
        if (String(fresh.parentStory.contents) !== String(top.parentStory.contents) || !sameBounds(fresh.geometricBounds, top.geometricBounds)) {
            throw new Error('replacement contents or bounds do not match the fill frame');
        }
        if (!sameStroke(strokeOf(fresh), spec)) { throw new Error('replacement stroke did not survive assignment'); }
        var ranges = fresh.parentStory.textStyleRanges;
        for (var i = 0; i < ranges.length; i++) {
            if (name(ranges[i].fillColor) !== spec.color || ranges[i].fillTint !== spec.tint || ranges[i].overprintFill !== spec.overprint) {
                throw new Error('replacement fill does not match the stroke');
            }
        }
        if (!(fresh.index > top.index)) { throw new Error('replacement is not behind the fill frame'); }
    }

    api.resync = function (pair) {
        var top = pair.top, bottom = pair.bottom, fresh = null, info = null;
        try {
            info = pairInfo(pair);
            contextCheck(top, bottom);
            var spec = strokeOf(bottom);
            if (!spec) { throw new Error('the bottom stroke is not uniform'); }
            if (spec.color === 'None' || !isFinite(Number(spec.weight)) || spec.weight <= 0 || spec.align !== 'OUTSIDE_ALIGNMENT') {
                throw new Error('a positive, uniform Outside stroke is required');
            }
            if (spec.join !== 'MITER_END_JOIN' && spec.join !== 'BEVEL_END_JOIN' && spec.join !== 'ROUND_END_JOIN') {
                throw new Error('unsupported stroke join');
            }
            var swatch = info.doc.swatches.itemByName(spec.color);
            if (!swatch.isValid || swatch.constructor.name === 'Gradient') { throw new Error('missing or unsupported stroke swatch'); }
            // Duplicate without resetting geometricBounds, which could alter rotated text.
            fresh = top.duplicate();
            fresh.insertLabel(LABEL, '');
            var story = fresh.parentStory.texts[0];
            story.strokeColor = swatch;
            story.strokeTint = spec.tint;
            story.strokeWeight = spec.weight;
            story.strokeAlignment = TextStrokeAlign.OUTSIDE_ALIGNMENT;
            story.endJoin = (spec.join === 'MITER_END_JOIN') ? EndJoin.MITER_END_JOIN : ((spec.join === 'BEVEL_END_JOIN') ? EndJoin.BEVEL_END_JOIN : EndJoin.ROUND_END_JOIN);
            story.miterLimit = spec.miter;
            story.overprintStroke = spec.overprint;
            story.fillColor = swatch;
            story.fillTint = spec.tint;
            story.overprintFill = spec.overprint;
            fresh.name = String(bottom.name);
            if (!placeBehind(fresh, top)) { throw new Error('could not order replacement behind the fill frame'); }
            validateFresh(fresh, top, spec);
            fresh.insertLabel(LABEL, label(info.token, 'stroke'));
            // A temporary third member is safe while this synchronous transaction is staged.
            // Discovery refuses that ambiguity; only this private check accepts the replacement.
            pairInfo({ top: top, bottom: fresh }, true);
            validateFresh(fresh, top, spec);
            // The old bottom remains intact until all replacement checks and label writes pass.
            var chars = top.parentStory.characters.length;
            bottom.remove();
            if (bottom.isValid) { throw new Error('old bottom removal had no effect'); }
            pair.bottom = fresh;
            return { done: true, ordered: true, chars: chars };
        } catch (e) {
            var reason = e.message, cleanup = [];
            if (fresh) {
                if (bottom && !bottom.isValid) {
                    // Never remove the only surviving representation after an ambiguous host error.
                    return { done: false, reason: reason + '; old bottom is gone; validated replacement retained for manual review' };
                }
                try { if (fresh.isValid) { fresh.remove(); } if (fresh.isValid) { throw new Error('removal had no effect'); } }
                catch (removeError) { cleanup.push('replacement ' + fresh.id + ' cleanup failed: ' + removeError.message); }
            }
            return { done: false, reason: reason + (cleanup.length ? '; MANUAL RECOVERY: ' + cleanup.join('; ') : '') };
        }
    };

    // Explicit calls rebuild even equal text, carrying fonts and other formatting through
    // native duplication. The watcher's contentsOnly path does not fingerprint formatting.
    api.resyncAll = function (doc, contentsOnly) {
        var scan = api.inspect(doc), done = 0, skipped = scan.skipped.slice(0);
        for (var i = 0; i < scan.pairs.length; i++) {
            var pair = scan.pairs[i];
            try {
                if (contentsOnly === true && String(pair.top.parentStory.contents) === String(pair.bottom.parentStory.contents)) { continue; }
                var r = api.resync(pair);
                if (r.done) { done++; } else { skipped.push('frame ' + pair.top.id + ': ' + r.reason); }
            } catch (e) { skipped.push('frame ' + pair.top.id + ': ' + e.message); }
        }
        return { pairs: scan.pairs.length, resynced: done, skipped: skipped };
    };
    api.tick = function (onlyDocument) {
        if (busy) { return; }
        busy = true;
        var issues = [];
        try {
            // A diagnostic caller may restrict work to its owned document. The installed
            // watcher's no-argument callback intentionally reconciles open documents.
            var documents = onlyDocument ? [onlyDocument] : app.documents;
            for (var d = 0; d < documents.length; d++) {
                try {
                    var result = api.resyncAll(documents[d], true);
                    $.global.ISE_SYNC_COUNT = ($.global.ISE_SYNC_COUNT || 0) + result.resynced;
                    if (result.skipped.length) { issues.push('document ' + documents[d].id + ': ' + result.skipped.join('; ')); }
                } catch (e) { issues.push('document scan failed: ' + e.message); }
            }
            var summary = issues.join('; ');
            if (summary !== $.global.ISE_SYNC_LAST) { if (summary) { L(summary); } $.global.ISE_SYNC_LAST = summary; }
        } finally { busy = false; }
    };
    api.stop = function () {
        var t = app.idleTasks.itemByName(TASK);
        if (t.isValid) { t.sleep = 0; t.remove(); return true; }
        return false;
    };
    api.start = function (intervalMs) {
        var interval = Number(intervalMs || 1000);
        if (!isFinite(interval) || interval < 100) { throw new Error('watch interval must be at least 100 ms'); }
        api.stop();
        $.global.ISE_SYNC_COUNT = 0;
        $.global.ISE_SYNC_LAST = '';
        var t = app.idleTasks.add({ name: TASK, sleep: interval });
        t.addEventListener(IdleEvent.ON_IDLE, function () { $.global.ISE_SYNC.tick(); });
        return t;
    };
    api.status = function () {
        var t = app.idleTasks.itemByName(TASK);
        return 'watching=' + t.isValid + ' automatic=contents-only resyncs=' + ($.global.ISE_SYNC_COUNT || 0) +
            ($.global.ISE_SYNC_LAST ? ' issues=' + $.global.ISE_SYNC_LAST : '');
    };
    return api;
})();

// The driver runs only on its own argument name, so that loading this file as a library with
// -With cannot execute a command as a side effect. Gating on a plain "mode" was not enough:
// another probe passing its own mode made this driver run as well, printing its status twice,
// and a mode of "stop" would then have silently stopped a watcher the caller never asked about.
if ($.global.ISE_LIB && $.global.ISE_LIB.args && $.global.ISE_LIB.args.syncMode) (function () {
    var lib = $.global.ISE_LIB;
    var sync = $.global.ISE_SYNC;
    var out = lib.run(function () {
        var mode = String(lib.args.syncMode);
        lib.L('mode=' + mode + ' engine=' + ($.engineName || '(unnamed)'));
        if (mode === 'once') {
            if (app.documents.length === 0) { lib.L('no document open'); return; }
            var r = sync.resyncAll(app.activeDocument);
            lib.L('pairs=' + r.pairs + ' resynced=' + r.resynced + (r.skipped.length ? ' skipped: ' + r.skipped.join('; ') : ''));
        } else if (mode === 'start') {
            var t = sync.start(Number(lib.args.intervalMs || 1000));
            lib.L('watching, interval=' + t.sleep + ' ms, valid=' + t.isValid);
        } else if (mode === 'stop') {
            lib.L('removed=' + sync.stop() + ' ' + sync.status());
        } else {
            lib.L(sync.status());
        }
    });
    // A script in a persistent engine returns nothing to DoScript, so it reports through a file.
    try { lib.writeText(String(lib.args.logPath || (Folder.temp.fsName + '/ise-live-sync.log')), out); } catch (e) {}
    return out;
})();
