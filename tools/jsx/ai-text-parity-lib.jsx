// Shared synthetic parity receipt and snapshot helpers. Returned by evalFile.
(function () {
    function json(x) {
        if (x === null || x === undefined) return 'null';
        if (typeof x === 'string') return '"' + x.replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\r/g, '\\r').replace(/\n/g, '\\n').replace(/\t/g, '\\t') + '"';
        if (typeof x === 'number' || typeof x === 'boolean') return String(x);
        var a = [], k;
        if (x instanceof Array) { for (k = 0; k < x.length; k++) a.push(json(x[k])); return '[' + a.join(',') + ']'; }
        for (k in x) if (x.hasOwnProperty(k)) a.push(json(k) + ':' + json(x[k]));
        return '{' + a.join(',') + '}';
    }

    function color(c) { return c.typename === 'GrayColor' ? ['gray', c.gray] : [c.typename]; }
    function snapshot(doc) {
        var frames = [], i, j;
        for (i = 0; i < doc.textFrames.length; i++) {
            var t = doc.textFrames[i], chars = [], range = t.story.textRange;
            for (j = 0; j < range.characters.length; j++) {
                var c = range.characters[j], a = c.characterAttributes;
                chars.push([c.contents, a.textFont.name, a.size, a.leading, a.autoLeading,
                    a.tracking, a.horizontalScale, a.verticalScale, a.baselineShift,
                    color(a.fillColor), color(a.strokeColor), a.strokeWeight]);
            }
            var m = t.matrix;
            frames.push({kind:String(t.kind), storyFrames:t.story.textFrames.length,
                frameCharacters:t.textRange.characters.length, frameLines:t.lines.length, chars:chars, contents:t.contents,
                matrix:[m.mValueA,m.mValueB,m.mValueC,m.mValueD,m.mValueTX,m.mValueTY]});
        }
        return {frames:frames, raster:doc.rasterItems.length, placed:doc.placedItems.length};
    }

    function path(args, suffix) { return new File(args.outDir + "/" + args.runId + "-" + suffix); }
    function write(args, suffix, data) {
        var f = path(args, suffix); f.encoding = "UTF-8";
        if (f.exists || !f.open("w")) throw Error("refusing existing output or open failure");
        f.write(json(data)); f.close();
    }
    function read(args, suffix) {
        var f = path(args, suffix); f.encoding = "UTF-8";
        if (!f.open("r")) throw Error("owned receipt missing");
        var text = f.read(); f.close(); return eval("(" + text + ")");
    }
    function pdf(doc, args, stem) {
        var f = path(args, stem + ".pdf");
        if (f.exists) throw Error("refusing existing PDF");
        var o = new PDFSaveOptions(); o.preserveEditability = false;
        o.acrobatLayers = false; o.viewAfterSaving = false;
        app.redraw(); doc.saveAs(f, o);
    }
    return {json:json, snapshot:snapshot, path:path, write:write, read:read, pdf:pdf};
})();
