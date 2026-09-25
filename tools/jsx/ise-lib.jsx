// ise-lib.jsx - shared helpers for the InDesign Stroke Engine build scripts.
// Pure ASCII: ExtendScript sources are read in the system codepage.
// Load with $.evalFile after setting $.global.ISE_ARGS; the library copies the arguments
// and deletes that global at once. The library itself lives in $.global.ISE_LIB, which
// the caller deletes when the build returns.
// Export preference primitives and arrays are copied by value; immutable Enumerators are
// retained. Other objects and unreadable properties are reported as unverified.
// Cleanup closes only explicitly tracked documents that were not open when the run began.

$.global.ISE_LIB = (function () {
    var lib = {};
    lib.args = $.global.ISE_ARGS || {};
    try { delete $.global.ISE_ARGS; } catch (e) {}
    lib.log = [];
    lib.L = function (s) { lib.log.push(String(s)); };
    lib.FONT = 'Minion Pro\tRegular';

    var SKIPP = { parent: 1, reflect: 1, properties: 1, isValid: 1, events: 1, eventListeners: 1 };

    lib.str = function (v) {
        try {
            if (v === null || v === undefined) { return String(v); }
            if (v.constructor && v.constructor.name === 'Enumerator') { return v.toString(); }
            if (v instanceof Array) { return '[' + v.join(',') + ']'; }
            if (typeof v === 'object') { return '<' + v.constructor.name + ' ' + (v.name !== undefined ? v.name : '') + '>'; }
            return String(v);
        } catch (e) { return '<err>'; }
    };

    function isEnum(v) {
        return v !== null && v !== undefined && v.constructor && v.constructor.name === 'Enumerator';
    }

    // Never keep a mutable reference as though it were a value snapshot. Cyclic arrays and
    // host objects have no supported copy contract here, so report them instead of guessing.
    function copyValue(v, parents) {
        var t = typeof v;
        if (v === null || t === 'undefined' || t === 'string' || t === 'number' || t === 'boolean' || isEnum(v)) { return v; }
        if (v instanceof Array) {
            var i, out = new Array(v.length);
            parents = parents || [];
            for (i = 0; i < parents.length; i++) {
                if (parents[i] === v) { throw new Error('cyclic array'); }
            }
            parents.push(v);
            for (i = 0; i < v.length; i++) {
                if (i in v) { out[i] = copyValue(v[i], parents); }
            }
            parents.pop();
            return out;
        }
        throw new Error('unsupported preference value (' + t + ')');
    }

    function sameValue(a, b) {
        if (a === b) { return a !== 0 || 1 / a === 1 / b; }
        if (typeof a !== typeof b) { return false; }
        if (typeof a === 'number' && isNaN(a) && isNaN(b)) { return true; }
        if (a instanceof Array && b instanceof Array) {
            if (a.length !== b.length) { return false; }
            for (var i = 0; i < a.length; i++) {
                if ((i in a) !== (i in b) || ((i in a) && !sameValue(a[i], b[i]))) { return false; }
            }
            return true;
        }
        return !!(isEnum(a) && isEnum(b) && String(a) === String(b));
    }

    function snap(o) {
        var s = { order: [], vals: {}, unverified: [] }, p = o.reflect.properties;
        for (var i = 0; i < p.length; i++) {
            var n = p[i].name;
            if (SKIPP[n] || p[i].type === 'readonly') { continue; }
            try { s.vals[n] = copyValue(o[n]); s.order.push(n); }
            catch (e) { s.unverified.push(n + ': ' + e.message); }
        }
        return s;
    }

    // A compression threshold below the hidden sampling resolution a preset left behind is
    // refused while sampling is off; InDesign 21.5 then keeps the preset's threshold.
    var THRESHOLDS = [['thresholdToCompressColor', 'colorBitmapSampling', 'colorBitmapSamplingDPI'],
        ['thresholdToCompressGray', 'grayscaleBitmapSampling', 'grayscaleBitmapSamplingDPI'],
        ['thresholdToCompressMonochrome', 'monochromeBitmapSampling', 'monochromeBitmapSamplingDPI']];

    function restoreThresholds(o, s, adjusted) {
        if (typeof Sampling === 'undefined') { return; }
        for (var t = 0; t < THRESHOLDS.length; t++) {
            var key = THRESHOLDS[t][0], sampling = THRESHOLDS[t][1], dpi = THRESHOLDS[t][2];
            if (!s.vals.hasOwnProperty(key) || typeof s.vals[key] !== 'number') { continue; }
            try {
                if (sameValue(o[key], s.vals[key])) { continue; }
                var mode = o[sampling];
                o[sampling] = Sampling.BICUBIC_DOWNSAMPLE;
                try {
                    if (o[dpi] > s.vals[key]) { o[dpi] = s.vals[key]; adjusted.push(dpi + '=' + s.vals[key]); }
                    o[key] = s.vals[key];
                } finally { o[sampling] = mode; }
            } catch (e) { adjusted.push(key + ': ' + e.message); }
        }
    }

    function restore(o, s, first) {
        var fails = [], done = {}, diff = [], adjusted = [], unverified = s.unverified.slice(0), i, n, pass;
        for (i = 0; i < first.length; i++) {
            n = first[i];
            if (s.vals.hasOwnProperty(n)) { try { o[n] = copyValue(s.vals[n]); } catch (e) { fails.push(n + ': ' + e.message); } done[n] = 1; }
        }
        // Setting one preference can reset another (a preset's color settings reset the
        // destination profile), so repeat until nothing changes; report the last pass only.
        var firstFails = fails;
        for (pass = 0; pass < 3; pass++) {
            fails = firstFails.slice(0);
            var changed = false;
            for (i = 0; i < s.order.length; i++) {
                n = s.order[i];
                if (done[n] && pass === 0) { continue; }
                try { if (!sameValue(o[n], s.vals[n])) { changed = true; o[n] = copyValue(s.vals[n]); } } catch (e) { fails.push(n + ': ' + e.message); }
            }
            if (!changed) { break; }
        }
        restoreThresholds(o, s, adjusted);
        if (adjusted.length) {
            var remaining = [];
            for (i = 0; i < fails.length; i++) {
                var failedKey = fails[i].split(':')[0], cleared = false;
                for (var t = 0; t < THRESHOLDS.length; t++) {
                    if (THRESHOLDS[t][0] === failedKey && sameValue(o[failedKey], s.vals[failedKey])) { cleared = true; }
                }
                if (!cleared) { remaining.push(fails[i]); }
            }
            fails = remaining;
        }
        for (i = 0; i < s.order.length; i++) {
            n = s.order[i];
            try { if (!sameValue(o[n], s.vals[n])) { diff.push(n + ' now=' + lib.str(o[n]) + ' saved=' + lib.str(s.vals[n])); } }
            catch (e) { unverified.push(n + ': verification failed: ' + e.message); }
        }
        return 'fails=' + fails.length + (fails.length ? ' [' + fails.join('; ') + ']' : '') + ' diff=' + diff.length + (diff.length ? ' [' + diff.join('; ') + ']' : '') +
            ' unverified=' + unverified.length + (unverified.length ? ' [' + unverified.join('; ') + ']' : '') +
            (adjusted.length ? ' adjusted=[' + adjusted.join('; ') + ']' : '');
    }

    function sameDocument(a, b) {
        return a === b || (a.id !== undefined && a.id === b.id);
    }

    lib.liveState = function () {
        var a = [];
        for (var i = 0; i < app.documents.length; i++) {
            var d = app.documents[i];
            a.push(d.name + '|saved=' + d.saved + '|modified=' + d.modified);
        }
        return a.join(';');
    };

    lib.timed = function (label, fn) {
        var t0 = new Date().getTime();
        try { fn(); lib.L(label + ' ok ms=' + (new Date().getTime() - t0)); return true; } catch (e) { lib.L(label + ' ERROR ' + e.message + ' line ' + e.line); return false; }
    };

    lib.tryDo = function (label, fn) {
        try { return fn(); } catch (e) { lib.L(label + ' FAILED: ' + e.message + ' line ' + e.line); return undefined; }
    };

    lib.run = function (fn) {
        var t0 = new Date().getTime();
        var before = lib.liveState();
        var existing = [];
        for (var di = 0; di < app.documents.length; di++) { existing.push(app.documents[di]); }
        lib.L('liveBefore=' + before);
        var snaps = [
            { name: 'pdfExportPreferences', s: snap(app.pdfExportPreferences), first: ['standardsCompliance', 'acrobatCompatibility'] },
            { name: 'pngExportPreferences', s: snap(app.pngExportPreferences), first: [] },
            { name: 'epsExportPreferences', s: snap(app.epsExportPreferences), first: [] }
        ];
        var oldUIL = app.scriptPreferences.userInteractionLevel;
        app.scriptPreferences.userInteractionLevel = UserInteractionLevels.NEVER_INTERACT;
        var opened = [];
        lib.track = function (doc) {
            for (var d = 0; d < existing.length; d++) {
                if (sameDocument(doc, existing[d])) { throw new Error('Refusing to take ownership of an already open document: ' + doc.name); }
            }
            opened.push(doc);
            return doc;
        };
        try {
            fn(lib);
        } catch (e) {
            lib.L('ERROR ' + e.message + ' line ' + e.line);
        } finally {
            for (var i = 0; i < opened.length; i++) {
                try { if (opened[i].isValid) { opened[i].close(SaveOptions.NO); } } catch (e2) { lib.L('close failed: ' + e2.message); }
            }
            for (var j = 0; j < snaps.length; j++) {
                lib.L(snaps[j].name + ' restore: ' + restore(app[snaps[j].name], snaps[j].s, snaps[j].first));
            }
            app.scriptPreferences.userInteractionLevel = oldUIL;
        }
        var after = lib.liveState();
        lib.L('liveAfter=' + after + (after === before ? ' (unchanged)' : ' (CHANGED)'));
        lib.L('elapsed ms=' + (new Date().getTime() - t0));
        return lib.log.join('\n');
    };

    lib.fontOk = function (name) {
        try { return app.fonts.itemByName(name).isValid; } catch (e) { return false; }
    };

    lib.color = function (doc, name, cmyk) {
        var c = doc.colors.itemByName(name);
        if (c.isValid) { return c; }
        return doc.colors.add({ name: name, model: ColorModel.PROCESS, space: ColorSpace.CMYK, colorValue: cmyk });
    };

    lib.newDoc = function (w, h) {
        var doc = lib.track(app.documents.add(false));
        var vp = doc.viewPreferences;
        vp.horizontalMeasurementUnits = MeasurementUnits.POINTS;
        vp.verticalMeasurementUnits = MeasurementUnits.POINTS;
        vp.rulerOrigin = RulerOrigin.PAGE_ORIGIN;
        var dp = doc.documentPreferences;
        dp.facingPages = false;
        dp.pagesPerDocument = 1;
        dp.pageWidth = w;
        dp.pageHeight = h;
        doc.zeroPoint = [0, 0];
        doc.pages[0].marginPreferences.properties = { top: 0, left: 0, bottom: 0, right: 0 };
        var bg = doc.pages[0].rectangles.add({ geometricBounds: [0, 0, h, w] });
        bg.fillColor = doc.swatches.itemByName('Black');
        bg.fillTint = 50;
        bg.strokeColor = doc.swatches.itemByName('None');
        bg.strokeWeight = 0;
        bg.name = 'backdrop: Black 50% tint, opaque';
        bg.locked = true;
        lib.manifest = { page: [w, h], frames: [], items: [] };
        return doc;
    };

    lib.baseStyle = function (doc, name, props) {
        var ps = doc.paragraphStyles.add({ name: name });
        ps.appliedFont = lib.FONT;
        ps.pointSize = 24;
        ps.leading = Leading.AUTO;
        ps.autoLeading = 120;
        ps.fillColor = doc.swatches.itemByName('Black');
        ps.strokeColor = doc.swatches.itemByName('Paper');
        ps.strokeWeight = 4;
        ps.strokeAlignment = TextStrokeAlign.OUTSIDE_ALIGNMENT;
        ps.endJoin = EndJoin.ROUND_END_JOIN;
        ps.miterLimit = 4;
        ps.hyphenation = false;
        if (props) {
            for (var k in props) {
                if (props.hasOwnProperty(k)) {
                    try { ps[k] = props[k]; } catch (e) { lib.L('style ' + name + ' ' + k + ' failed: ' + e.message); }
                }
            }
        }
        return ps;
    };

    lib.frame = function (doc, name, bounds, text, ps) {
        var tf = doc.pages[0].textFrames.add({ geometricBounds: bounds });
        tf.name = name;
        tf.textFramePreferences.insetSpacing = [0, 0, 0, 0];
        tf.contents = text;
        tf.parentStory.texts[0].applyParagraphStyle(ps, true);
        return tf;
    };

    lib.lines = function (tf) {
        var out = [], ln = tf.lines;
        for (var i = 0; i < ln.length; i++) {
            out.push({ contents: String(ln[i].contents), baseline: ln[i].baseline, x0: ln[i].horizontalOffset, x1: ln[i].endHorizontalOffset, ranges: ln[i].textStyleRanges.length });
        }
        return out;
    };

    lib.record = function (item, variant, extra) {
        var gb = item.geometricBounds;
        var rec = { name: item.name, id: item.id, variant: variant, bounds: [gb[0], gb[1], gb[2], gb[3]] };
        try { if (item.constructor.name === 'TextFrame') { rec.lines = lib.lines(item); rec.styleRanges = item.parentStory.textStyleRanges.length; } } catch (e) {}
        if (extra) { for (var k in extra) { if (extra.hasOwnProperty(k)) { rec[k] = extra[k]; } } }
        lib.manifest.frames.push(rec);
        return rec;
    };

    lib.toJSON = function (v) {
        if (v === null || v === undefined) { return 'null'; }
        var t = typeof v;
        if (t === 'number') { return isFinite(v) ? String(v) : 'null'; }
        if (t === 'boolean') { return String(v); }
        if (t === 'string') {
            var r = '';
            for (var i = 0; i < v.length; i++) {
                var c = v.charCodeAt(i), ch = v.charAt(i);
                if (ch === '"') { r += '\\"'; } else if (ch === '\\') { r += '\\\\'; } else if (c < 32 || c > 126) { var h = c.toString(16); while (h.length < 4) { h = '0' + h; } r += '\\u' + h; } else { r += ch; }
            }
            return '"' + r + '"';
        }
        if (v instanceof Array) {
            var a = [];
            for (var j = 0; j < v.length; j++) { a.push(lib.toJSON(v[j])); }
            return '[' + a.join(',') + ']';
        }
        if (t === 'object') {
            if (v.constructor && v.constructor.name === 'Enumerator') { return lib.toJSON(v.toString()); }
            var parts = [];
            for (var k in v) { if (v.hasOwnProperty(k)) { parts.push(lib.toJSON(k) + ':' + lib.toJSON(v[k])); } }
            return '{' + parts.join(',') + '}';
        }
        return lib.toJSON(String(v));
    };

    lib.writeText = function (path, text) {
        var f = new File(path);
        f.parent.create();
        f.encoding = 'UTF-8';
        f.lineFeed = 'Unix';
        f.open('w');
        f.write(text);
        f.close();
    };

    lib.saveExport = function (doc, issueDir, stem, ppi) {
        ppi = ppi || 300;
        var mdir = issueDir + '/minimal', adir = issueDir + '/actual';
        // A target already open in another document belongs to that document's caller. Refuse
        // the build without closing it, whether or not it has unsaved changes.
        var target = new File(mdir + '/' + stem + '.indd');
        var targetPath = String(target.fsName).replace(/\\/g, '/').toLowerCase();
        for (var di = app.documents.length - 1; di >= 0; di--) {
            var other = app.documents[di], same = false;
            try { same = (!sameDocument(other, doc) && other.fullName && String(other.fullName.fsName).replace(/\\/g, '/').toLowerCase() === targetPath); } catch (e) { same = false; }
            if (same) {
                throw new Error('Refusing to overwrite a target open in another document: ' + target.fsName);
            }
        }
        new Folder(mdir).create();
        if (!lib.timed('save ' + stem, function () { doc.save(target); })) {
            throw new Error('Save failed for ' + stem + '; exports and manifest were not written.');
        }
        new Folder(adir).create();
        lib.timed('pdf ' + stem, function () {
            doc.exportFile(ExportFormat.PDF_TYPE, new File(adir + '/' + stem + '.pdf'), false, app.pdfExportPresets.itemByName('[High Quality Print]'));
        });
        lib.timed('png ' + stem, function () {
            var png = app.pngExportPreferences;
            png.exportResolution = ppi;
            png.pngQuality = PNGQualityEnum.MAXIMUM;
            png.transparentBackground = false;
            png.antiAlias = true;
            png.pngColorSpace = PNGColorSpaceEnum.RGB;
            png.pngExportRange = PNGExportRangeEnum.EXPORT_ALL;
            png.exportingSpread = false;
            png.useDocumentBleeds = false;
            png.simulateOverprint = false;
            doc.exportFile(ExportFormat.PNG_FORMAT, new File(adir + '/' + stem + '-' + ppi + 'ppi.png'), false);
        });
        lib.timed('idml ' + stem, function () { doc.exportFile(ExportFormat.INDESIGN_MARKUP, new File(adir + '/' + stem + '.idml'), false); });
        lib.manifest.app = app.version;
        lib.writeText(adir + '/' + stem + '.manifest.json', lib.toJSON(lib.manifest));
        lib.L('manifest frames=' + lib.manifest.frames.length);
    };

    return lib;
})();
