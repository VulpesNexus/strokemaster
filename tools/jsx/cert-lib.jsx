// cert-lib.jsx - route W certification helpers: document preparations, output writers, and a
// job runner that works only on copies. Requires ise-lib.jsx; load with run-jsx.ps1 -With.
// Pure ASCII: ExtendScript sources are read in the system codepage.
//
// A job names a source document, the copy it works on, the preparations applied to that copy,
// and the outputs written from it. The source is never opened: it is copied first, and the copy
// is closed without saving. Each finished job is appended to <out>/results.jsonl at once, so a
// run that stops part way leaves every completed job recorded and nothing that looks finished.
//
// Preparations turn a copy into one of the measurement twins:
//   fillOnly        every text stroke set to [None]; the fill-core oracle, since removing the
//                   strokes changes nothing a correct fill should show
//   mask            fills process white, strokes [None], decorations off, transparency and
//                   effects reset, other artwork emptied, over a black backdrop: the fill glyphs
//   splitReference  the census oracle: each unthreaded text frame gets a copy behind it holding
//                   centered, doubled strokes and no fill; the frame itself keeps only its fills
//   storyReference  the cross-frame oracle: splitReference for single-frame stories; a threaded
//                   story on one layer is duplicated as a chain (grouped, so the copy keeps its
//                   thread) and set at the back of the page, above the backdrop, holding the
//                   story's strokes; the story keeps only its fills
//   flatten:<spec>  a spread flattener override (document-scoped, not an application preference)
//
// Only [None] is ever assigned to strokeColor when removing a stroke: assigning weight 0 after
// None turns a 1 pt black stroke back on in InDesign 21.5.
$.global.ISE_CERT = (function () {
    var lib = $.global.ISE_LIB, cert = {};

    cert.readJSON = function (path) {
        var f = new File(path);
        f.encoding = 'UTF-8';
        if (!f.open('r')) { throw new Error('cannot read ' + path); }
        var text;
        try { text = f.read(); } finally { f.close(); }
        // JSON is a subset of ExtendScript's object literal syntax; the file is our own job list.
        return eval('(' + text + ')');
    };

    cert.appendLine = function (path, text) {
        var f = new File(path);
        f.parent.create();
        f.encoding = 'UTF-8';
        f.lineFeed = 'Unix';
        if (!f.open(f.exists ? 'e' : 'w')) { throw new Error('cannot append ' + path); }
        try { f.seek(0, 2); f.writeln(text); } finally { f.close(); }
    };

    cert.copyFile = function (from, to) {
        var src = new File(from), dst = new File(to);
        if (!src.exists) { throw new Error('source not found: ' + from); }
        dst.parent.create();
        if (dst.exists && !dst.remove()) { throw new Error('cannot replace ' + to); }
        if (!src.copy(dst.fsName)) { throw new Error('copy failed: ' + from + ' -> ' + to); }
        return dst;
    };

    function enumValue(enumName, key) {
        var e = $.global[enumName];
        if (!e || e[key] === undefined) { throw new Error('unknown ' + enumName + '.' + key); }
        return e[key];
    }

    function none(doc) { return doc.swatches.itemByName('None'); }

    function eachStory(doc, fn) {
        for (var i = 0; i < doc.stories.length; i++) { fn(doc.stories[i]); }
    }

    function resetTransparency(item) {
        var names = ['transparencySettings', 'strokeTransparencySettings', 'fillTransparencySettings', 'contentTransparencySettings'];
        for (var i = 0; i < names.length; i++) {
            var ts;
            try { ts = item[names[i]]; } catch (e) { continue; }
            if (!ts) { continue; }
            try { ts.blendingSettings.opacity = 100; } catch (e1) {}
            try { ts.blendingSettings.blendMode = BlendMode.NORMAL; } catch (e2) {}
            try { ts.dropShadowSettings.mode = ShadowMode.NONE; } catch (e3) {}
            try { ts.featherSettings.mode = FeatherMode.NONE; } catch (e4) {}
            var effects = ['innerShadowSettings', 'outerGlowSettings', 'innerGlowSettings', 'bevelAndEmbossSettings', 'satinSettings', 'directionalFeatherSettings', 'gradientFeatherSettings'];
            for (var j = 0; j < effects.length; j++) { try { ts[effects[j]].applied = false; } catch (e5) {} }
        }
    }

    cert.prep = {};

    cert.prep.fillOnly = function (doc) {
        var n = 0;
        eachStory(doc, function (story) {
            if (story.characters.length) { story.texts[0].strokeColor = none(doc); n++; }
        });
        doc.recompose();
        return 'stories=' + n;
    };

    cert.prep.mask = function (doc) {
        var white = lib.color(doc, 'Cert Mask White', [0, 0, 0, 0]);
        eachStory(doc, function (story) {
            if (!story.characters.length) { return; }
            var t = story.texts[0];
            t.fillColor = white;
            t.fillTint = 100;
            t.strokeColor = none(doc);
            try { t.underline = false; } catch (e1) {}
            try { t.strikeThru = false; } catch (e2) {}
        });
        var items = doc.allPageItems, hidden = 0, emptied = 0;
        for (var i = 0; i < items.length; i++) {
            var it = items[i], kind = it.constructor.name;
            try { it.locked = false; } catch (e3) {}
            resetTransparency(it);
            if (kind === 'Group' || kind === 'TextFrame') {
                if (kind === 'TextFrame') { try { it.fillColor = none(doc); it.strokeColor = none(doc); } catch (e4) {} }
                continue;
            }
            if (kind === 'Image' || kind === 'PDF' || kind === 'EPS' || kind === 'ImportedPage' || kind === 'PICT' || kind === 'WMF') {
                try { it.visible = false; hidden++; } catch (e5) {}
                continue;
            }
            try { it.fillColor = none(doc); it.strokeColor = none(doc); emptied++; } catch (e6) {}
        }
        var layer = doc.layers.add({ name: 'cert mask backdrop' });
        layer.move(LocationOptions.AT_END);
        for (var p = 0; p < doc.pages.length; p++) {
            var b = doc.pages[p].bounds;
            var r = doc.pages[p].rectangles.add(layer, { geometricBounds: [b[0] - 20, b[1] - 20, b[2] + 20, b[3] + 20] });
            r.fillColor = doc.swatches.itemByName('Black');
            r.fillTint = 100;
            r.strokeColor = none(doc);
        }
        doc.recompose();
        return 'emptied=' + emptied + ' hidden=' + hidden;
    };

    cert.prep.splitReference = function (doc) {
        // Collect ids, not doc.textFrames[i]: an item taken by index is re-resolved by index on
        // each use, so after the first duplicate every one of them named the frame at index 0.
        var ids = [], frames = [], skipped = 0, i;
        for (i = 0; i < doc.textFrames.length; i++) { ids.push(doc.textFrames[i].id); }
        for (i = 0; i < ids.length; i++) { frames.push(doc.textFrames.itemByID(ids[i])); }
        var made = 0;
        for (i = 0; i < frames.length; i++) {
            var top = frames[i];
            if (top.previousTextFrame || top.nextTextFrame || !top.characters.length) { skipped++; continue; }
            var wasLocked = top.locked;
            top.locked = false;
            var bottom = top.duplicate();
            bottom.sendBackward();
            splitStrokes(doc, top.parentStory, bottom.parentStory, top.name);
            top.locked = wasLocked;
            made++;
        }
        doc.recompose();
        return 'frames=' + made + ' skippedThreadedOrEmpty=' + skipped;
    };

    // Moves a story's strokes to its copy: the copy keeps each stroke, centered and doubled if it
    // was Outside, and no fill; the original keeps its fills and no stroke.
    function splitStrokes(doc, top, bottom, label) {
        if (bottom.characters.length !== top.characters.length) { throw new Error('duplicate changed text in ' + label); }
        for (var c = 0; c < top.characters.length; c++) {
            var tc = top.characters[c], bc = bottom.characters[c];
            var color = tc.strokeColor, width = tc.strokeWeight, align = tc.strokeAlignment;
            bc.fillColor = none(doc);
            try { bc.underline = false; bc.strikeThru = false; } catch (e1) {}
            if (color.name === 'None' || !(width > 0)) { bc.strokeColor = none(doc); continue; }
            bc.strokeAlignment = TextStrokeAlign.CENTER_ALIGNMENT;
            bc.strokeWeight = align === TextStrokeAlign.OUTSIDE_ALIGNMENT ? 2 * width : width;
            bc.strokeColor = color;
            tc.strokeWeight = 0;
            tc.strokeColor = none(doc);
        }
    }

    cert.prep.storyReference = function (doc) {
        var single = cert.prep.splitReference(doc);
        // Stories by id: a threaded story's frames, all on one layer, not inside another group.
        var ids = [], i, made = 0, skipped = [];
        for (i = 0; i < doc.stories.length; i++) { ids.push(doc.stories[i].id); }
        for (i = 0; i < ids.length; i++) {
            var story = doc.stories.itemByID(ids[i]), frames = story.textContainers;
            if (frames.length < 2) { continue; }
            var layer = frames[0].itemLayer.name, parent = frames[0].parent, ok = true, j;
            for (j = 0; j < frames.length; j++) {
                if (frames[j].constructor.name !== 'TextFrame' || frames[j].itemLayer.name !== layer) { ok = false; }
                if (frames[j].parent.constructor.name !== parent.constructor.name) { ok = false; }
            }
            if (!ok) { skipped.push(frames[0].name); continue; }
            var copy, group = null;
            if (parent.constructor.name === 'Group') {
                copy = parent.duplicate();
            } else {
                group = doc.pages[0].groups.add(frames);
                copy = group.duplicate();
                group.ungroup();
            }
            copy.sendToBack();
            copy.bringForward();   // above the backdrop rectangle, below everything else on its layer
            var copyStory = copy.textFrames[0].parentStory;
            if (copyStory.id === story.id || copy.textFrames[0].previousTextFrame && copy.textFrames[0].previousTextFrame.parentStory.id === story.id) {
                throw new Error('copy of ' + frames[0].name + ' is not a separate story');
            }
            splitStrokes(doc, doc.stories.itemByID(ids[i]), copyStory, frames[0].name);
            made++;
        }
        doc.recompose();
        return single + ' threadedStories=' + made + ' skippedCrossLayerOrMixed=' + skipped.join(',');
    };

    // flatten:<level>:<textOutlines>:<strokeOutlines>, for example flatten:LOW:no:yes. Applies a
    // custom flattener to every spread of the copy; ignoreSpreadOverrides stays false everywhere.
    cert.prep.flatten = function (doc, spec) {
        var parts = spec.split(':');
        var balance = { LOW: 0, MEDIUM: 50, HIGH: 100 }[parts[1]];
        if (balance === undefined) { throw new Error('flatten level ' + parts[1]); }
        for (var i = 0; i < doc.spreads.length; i++) {
            var s = doc.spreads[i];
            s.flattenerOverride = SpreadFlattenerLevel.CUSTOM;
            var fp = s.flattenerPreferences;
            fp.rasterVectorBalance = balance;
            fp.lineArtAndTextResolution = balance === 100 ? 1200 : 288;
            fp.gradientAndMeshResolution = balance === 100 ? 300 : 150;
            fp.convertAllTextToOutlines = parts[2] === 'yes';
            fp.convertAllStrokesToOutlines = parts[3] === 'yes';
            fp.clipComplexRegions = balance !== 0;
        }
        return 'spreads=' + doc.spreads.length + ' balance=' + balance;
    };

    cert.applyPrep = function (doc, name) {
        var key = name.split(':')[0];
        if (!cert.prep[key]) { throw new Error('unknown preparation ' + name); }
        return cert.prep[key](doc, name);
    };

    function pdfPrefsFrom(preset, overrides, log) {
        var pdf = app.pdfExportPreferences, props = preset.reflect.properties, first = ['standardsCompliance', 'acrobatCompatibility'];
        var skip = { name: 1, parent: 1, reflect: 1, properties: 1, isValid: 1, events: 1, eventListeners: 1, id: 1, index: 1 };
        var i, n;
        for (i = 0; i < first.length; i++) { try { pdf[first[i]] = preset[first[i]]; } catch (e) { log.push('set ' + first[i] + ': ' + e.message); } }
        for (i = 0; i < props.length; i++) {
            n = props[i].name;
            if (skip[n] || props[i].type === 'readonly' || n === 'appliedFlattenerPreset') { continue; }
            try { var v = preset[n]; pdf[n] = v; } catch (e1) {}
        }
        for (n in overrides) {
            if (!overrides.hasOwnProperty(n)) { continue; }
            var val = overrides[n];
            if (typeof val === 'string' && val.indexOf('enum:') === 0) { var p = val.split(':'); val = enumValue(p[1], p[2]); }
            pdf[n] = val;
            log.push('override ' + n + '=' + lib.str(pdf[n]));
        }
    }

    cert.write = {};

    cert.write.pdf = function (doc, o, path, log) {
        var preset = app.pdfExportPresets.itemByName(o.preset);
        if (!preset.isValid) { throw new Error('no PDF preset ' + o.preset); }
        var hasOverrides = false;
        for (var k in (o.overrides || {})) { if (o.overrides.hasOwnProperty(k)) { hasOverrides = true; } }
        if (!hasOverrides) {
            doc.exportFile(ExportFormat.PDF_TYPE, new File(path), false, preset);
            log.push('route=preset');
            return;
        }
        pdfPrefsFrom(preset, o.overrides, log);
        var pdf = app.pdfExportPreferences;
        log.push('route=preferences compat=' + lib.str(pdf.acrobatCompatibility) + ' std=' + lib.str(pdf.standardsCompliance) + ' color=' + lib.str(pdf.pdfColorSpace));
        doc.exportFile(ExportFormat.PDF_TYPE, new File(path), false);
    };

    cert.write.png = function (doc, o, path) {
        var png = app.pngExportPreferences;
        png.exportResolution = o.ppi || 600;
        png.pngQuality = PNGQualityEnum.MAXIMUM;
        png.transparentBackground = !!o.transparentBackground;
        png.antiAlias = o.antiAlias !== false;
        png.pngColorSpace = o.gray ? PNGColorSpaceEnum.GRAY : PNGColorSpaceEnum.RGB;
        png.pngExportRange = PNGExportRangeEnum.EXPORT_ALL;
        png.exportingSpread = false;
        png.useDocumentBleeds = false;
        png.simulateOverprint = !!o.simulateOverprint;
        doc.exportFile(ExportFormat.PNG_FORMAT, new File(path), false);
    };

    cert.write.eps = function (doc, o, path) {
        var eps = app.epsExportPreferences;
        eps.epsColor = enumValue('EPSColorSpace', o.color || 'CMYK');
        eps.postscriptLevel = enumValue('PostScriptLevels', o.level || 'LEVEL_2');
        eps.dataFormat = DataFormat.ASCII;
        eps.fontEmbedding = FontEmbedding.SUBSET;
        eps.preview = PreviewTypes.NONE;
        eps.pageRange = PageRange.ALL_PAGES;
        eps.bleedTop = eps.bleedBottom = eps.bleedInside = eps.bleedOutside = 0;
        doc.exportFile(ExportFormat.EPS_TYPE, new File(path), false);
    };

    cert.write.ps = function (doc, o, path, log) {
        var pp = doc.printPreferences;
        // Settings that define the test must take; the rest are logged when the host refuses them
        // (copies, for one, is locked while the printer is a PostScript file).
        function opt(name, value) { try { pp[name] = value; } catch (e) { log.push(name + ' refused: ' + e.message); } }
        pp.printer = Printer.POSTSCRIPT_FILE;
        pp.ppd = o.ppd || 'Adobe PDF';
        pp.printFile = new File(path);
        opt('copies', 1);
        opt('pageRange', PageRange.ALL_PAGES);
        opt('printSpreads', false);
        opt('printBlankPages', false);
        opt('paperSize', PaperSizes.CUSTOM);
        opt('paperWidth', doc.documentPreferences.pageWidth);
        opt('paperHeight', doc.documentPreferences.pageHeight);
        opt('scaleMode', ScaleModes.SCALE_WIDTH_HEIGHT);
        opt('scaleWidth', 100);
        opt('scaleHeight', 100);
        opt('pagePosition', PagePositions.UPPER_LEFT);
        pp.colorOutput = enumValue('ColorOutputModes', o.colorOutput || 'COMPOSITE_CMYK');
        pp.postscriptLevel = enumValue('PostScriptLevels', o.level || 'LEVEL_3');
        pp.flattenerPresetName = o.flattener || '[High Resolution]';
        opt('textAsBlack', false);
        opt('fontDownloading', FontDownloading.COMPLETE);
        log.push('ppd=' + pp.ppd + ' flattener=' + pp.flattenerPresetName + ' level=' + lib.str(pp.postscriptLevel) +
            ' color=' + lib.str(pp.colorOutput) + ' paper=' + lib.str(pp.paperSize) + ' ' + lib.str(pp.paperWidth) + 'x' + lib.str(pp.paperHeight));
        doc.print(false);
    };

    cert.write.idml = function (doc, o, path) { doc.exportFile(ExportFormat.INDESIGN_MARKUP, new File(path), false); };

    cert.write.indd = function (doc, o, path) {
        if (new File(path).exists) { throw new Error('refusing to overwrite ' + path); }
        doc.saveACopy(new File(path));
    };

    cert.write.pkg = function (doc, o, path, log) {
        var folder = new Folder(path);
        folder.create();
        // Fonts, links, and profiles copied; no report, IDML, or PDF; ignore preflight errors.
        var ok = doc.packageForPrint(folder, true, true, true, false, false, true, false);
        log.push('package returned ' + ok);
    };

    function fileLength(path) {
        var f = new File(path);
        if (f.exists && f instanceof File) { return f.length; }
        var d = new Folder(path);
        return d.exists ? -1 : 0;
    }

    cert.writeOutputs = function (doc, job, rec) {
        new Folder(job.outDir).create();
        for (var i = 0; i < (job.outputs || []).length; i++) {
            var o = job.outputs[i], path = job.outDir + '/' + o.file, log = [];
            var t0 = new Date().getTime(), err = null;
            try {
                if (!cert.write[o.kind]) { throw new Error('unknown output kind ' + o.kind); }
                cert.write[o.kind](doc, o, path, log);
            } catch (e) { err = e.message + ' line ' + e.line; }
            rec.outputs.push({ file: o.file, kind: o.kind, ms: new Date().getTime() - t0, bytes: fileLength(path), error: err, log: log.join('; ') });
        }
    };

    function openCopy(job, rec) {
        var doc;
        if (job.place) {
            var size = job.place.size;
            doc = lib.newDoc(size[0], size[1]);
            var bg = doc.pages[0].rectangles[0];
            bg.locked = false;
            bg.remove();
            var ppl = app.pdfPlacePreferences;
            ppl.pdfCrop = PDFCrop.CROP_MEDIA;
            ppl.pageNumber = 1;
            ppl.transparentBackground = true;
            var placed = doc.pages[0].place(new File(job.place.file), [0, 0])[0];
            rec.placed = placed.constructor.name + ' bounds=' + placed.parent.geometricBounds.join(',');
            return doc;
        }
        var copy = cert.copyFile(job.source, job.copy);
        doc = lib.track(app.open(copy, false));
        return doc;
    }

    cert.runJob = function (job, resultsPath) {
        var rec = { id: job.id, source: job.source || null, copy: job.copy || null, prep: [], outputs: [], error: null };
        var t0 = new Date().getTime();
        var doc = null;
        try {
            new Folder(job.outDir).create();
            doc = openCopy(job, rec);
            rec.opened = doc.name + ' pages=' + doc.pages.length + ' stories=' + doc.stories.length;
            for (var i = 0; i < (job.prep || []).length; i++) {
                rec.prep.push(job.prep[i] + ' -> ' + cert.applyPrep(doc, job.prep[i]));
            }
            cert.writeOutputs(doc, job, rec);
            rec.state = 'saved=' + doc.saved + ' modified=' + doc.modified;
        } catch (e) {
            rec.error = e.message + ' line ' + e.line;
        } finally {
            if (doc && doc.isValid) { try { doc.close(SaveOptions.NO); } catch (e2) { rec.closeError = e2.message; } }
        }
        rec.ms = new Date().getTime() - t0;
        cert.appendLine(resultsPath, lib.toJSON(rec));
        return rec;
    };

    return cert;
})();
