// StrokeMaster native fixes for page items in Illustrator (2026-10-15), probed on the census
// base set, Outside 4 and Center 8: (a) a whole point by the path's own miter limit (the
// thorn's 10-degree tip has ratio 1 / sin 5 = 11.47, so limit 12 keeps it whole), and
// (b) overlapping contours stroked around the filled region only, by Adobe's own Offset Path
// effect at 0 pt on the compound path (live, always available, follows edits).
// Exports native and geometry PDFs and a manifest in the census layout for
// tools/measure_coverage_census.py. Needs $.global.ISE_REGION_ARGS outDir.
(function () {
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try { return run(); } finally { app.userInteractionLevel = interaction; }
    function run() {
    // documents.add failed with error 129 once Illustrator's recent new-document sizes had
    // filled with scripted ones (2026-10-15); addDocument with a profile still works.
    function newDocument(cs, w, h) {
        try { return app.documents.add(cs, w, h); } catch (e) {
            var pr = new DocumentPreset(); pr.colorMode = cs; pr.width = w; pr.height = h; pr.units = RulerUnits.Points;
            return app.documents.addDocument(cs === DocumentColorSpace.CMYK ? 'Print' : 'Web', pr);
        }
    }
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir) return 'FAIL|missing outDir';
    var plugin = 'StrokeMasterRegionProbe';
    var log = [];
    function note(k, v) { log.push(k + '=' + String(v).replace(/[|\r\n]/g, ' ')); }
    function send(selector, input) { return String(app.sendScriptMessage(plugin, selector, input || '')); }
    var W = 400, ROW = 110, H = 20 + 9 * ROW;
    var doc = newDocument(DocumentColorSpace.CMYK, W, H);
    var MINION = app.textFonts.getByName('MinionPro-Regular');
    var SCRIPT = null; try { SCRIPT = app.textFonts.getByName('SegoeScript'); } catch (e) {}
    var offset001 = '<LiveEffect name="Adobe Offset Path"><Dict data="R ofst 0.01 I jntp 0 R mlim 4 "/></LiveEffect>';
    var rows = [
        { name: 'outside 4 miter', align: 'outside', w: 4 },
        { name: 'outside 4 miter, offset path 0.01 on the squares', align: 'outside', w: 4, fix: 'offset' },
        { name: 'outside 4 miter, live pathfinder add on the squares in a group', align: 'outside', w: 4, fix: 'pathfinder' },
        { name: 'inside 4 miter, live pathfinder add on the squares in a group', align: 'inside', w: 4, fix: 'pathfinder' },
        { name: 'outside 4 miter, live pathfinder add on outlines and squares', align: 'outside', w: 4, fix: 'pathfinder', fixText: true },
        { name: 'center 8 miter, thorn limit 12', align: 'center', w: 8, thornLimit: 12 },
        { name: 'outside 4 miter, thorn limit 12', align: 'outside', w: 4, thornLimit: 12 },
        { name: 'script outside 4 miter', align: 'outside', w: 4, font: SCRIPT, text: 'Ribbons' },
        { name: 'script outside 4 miter, live pathfinder add on the outlines', align: 'outside', w: 4, font: SCRIPT, text: 'Ribbons', fix: 'pathfinder', fixText: true }
    ];    if (rows.length !== 9) { throw 'row count changed: update H'; }
    doc.artboards[0].artboardRect = [0, H, W, 0];
    var base = doc.layers[0]; base.name = 'ISE census';
    var geo = doc.layers.add(); geo.name = 'ISE geometry'; geo.move(base, ElementPlacement.PLACEAFTER);
    function cmyk(k) { var c = new CMYKColor(); c.black = k; return c; }
    var black = cmyk(100), paper = cmyk(0);
    var backdrop = base.pathItems.rectangle(H, 0, W, H); backdrop.fillColor = cmyk(50); backdrop.stroked = false;
    function Y(y) { return H - y; }
    function geomBounds(it) { var b = it.geometricBounds; return [H - b[1], b[0], H - b[3], b[2]]; }
    function allPaths(it) {
        if (it.typename === 'PathItem') { return [it]; }
        if (it.typename === 'CompoundPathItem') { var r = []; for (var q = 0; q < it.pathItems.length; q++) { r.push(it.pathItems[q]); } return r; }
        if (it.typename === 'GroupItem') { var g = []; for (var q2 = 0; q2 < it.pageItems.length; q2++) { g = g.concat(allPaths(it.pageItems[q2])); } return g; }
        return [];
    }
    function deselect() { doc.selection = null; return doc.selection.length; }
    var manifest = { page: [W, H], frames: [], items: [], app: app.version };
    for (var i = 0; i < rows.length; i++) {
        var c = rows[i], y = 20 + i * ROW;
        var t = base.textFrames.add(); t.contents = c.text || 'AMVWkz';
        t.textRange.characterAttributes.textFont = c.font || MINION; t.textRange.characterAttributes.size = 48;
        t.position = [16, Y(y)];
        var g = t.createOutline();
        var word = base.compoundPathItems.add();
        var leaves = allPaths(g);
        for (var q = leaves.length - 1; q >= 0; q--) { leaves[q].move(word, ElementPlacement.PLACEATEND); }
        g.remove();
        var half = 5 * Math.PI / 180, side = 20, cx = 0, tip = y + 25;
        var bw = side * Math.sin(half), by = tip + side * Math.cos(half);
        var thorn = base.pathItems.add();
        thorn.setEntirePath([[cx, Y(tip)], [cx + bw, Y(by)], [cx + 15, Y(by)], [cx + 15, Y(by + 30)], [cx - 15, Y(by + 30)], [cx - 15, Y(by)], [cx - bw, Y(by)]]);
        thorn.closed = true;
        var sq = base.compoundPathItems.add();
        var p1 = sq.pathItems.add(); p1.setEntirePath([[0, Y(y + 15)], [40, Y(y + 15)], [40, Y(y + 55)], [0, Y(y + 55)]]); p1.closed = true;
        var p2 = sq.pathItems.add(); p2.setEntirePath([[20, Y(y + 35)], [60, Y(y + 35)], [60, Y(y + 75)], [20, Y(y + 75)]]); p2.closed = true;
        var items = [word, thorn, sq], x = 16;
        for (var k = 0; k < items.length; k++) {
            var ps = allPaths(items[k]);
            for (var r = 0; r < ps.length; r++) {
                ps[r].evenodd = false; ps[r].filled = true; ps[r].fillColor = black;
                ps[r].stroked = true; ps[r].strokeColor = paper; ps[r].strokeWidth = c.w;
                ps[r].strokeJoin = StrokeJoin.MITERENDJOIN; ps[r].strokeMiterLimit = 4;
            }
            var b = items[k].visibleBounds; items[k].translate(x - b[0], Y(y + 25) - b[1]);
            b = items[k].visibleBounds; x += b[2] - b[0] + 24;
        }
        var rec = { row: i, kind: 'page item', label: c.name, font: (c.font || MINION).family, weight: c.w, cell: [y, 0, y + ROW, W], items: [] };
        var parts = ['text', 'thorn', 'squares'];
        for (k = 0; k < items.length; k++) {
            var gd = items[k].duplicate(geo, ElementPlacement.PLACEATEND);
            var gp = allPaths(gd);
            for (r = 0; r < gp.length; r++) { gp[r].filled = true; gp[r].fillColor = black; gp[r].stroked = false; }
            rec.items.push({ name: c.name + ' #' + k, part: parts[k], geometryBounds: geomBounds(gd) });
        }
        if (c.align !== 'center') {
            deselect(); for (k = 0; k < items.length; k++) { items[k].selected = true; }
            var res = send('stroke-align', c.align);
            if (!/"ok":true/.test(res)) { note('align failed', c.name); }
            deselect();
        }
        if (c.thornLimit) { thorn.strokeMiterLimit = c.thornLimit; }
        function pathfinder(it) {
            var grp = base.groupItems.add();
            it.move(grp, ElementPlacement.PLACEATEND);
            deselect(); grp.selected = true;
            app.executeMenuCommand('Live Pathfinder Add');
            deselect();
        }
        if (c.fix === 'offset') { sq.applyEffect(offset001); }
        if (c.fix === 'pathfinder') { pathfinder(sq); if (c.fixText) { pathfinder(word); } }
        manifest.items.push(rec);
    }
    backdrop.zOrder(ZOrderMethod.SENDTOBACK);
    manifest.rows = rows.length; manifest.rowHeight = ROW;
    app.redraw();
    function json(v) {
        if (v === null || v === undefined) { return 'null'; }
        if (typeof v === 'number') { return isFinite(v) ? String(v) : 'null'; }
        if (typeof v === 'boolean') { return String(v); }
        if (typeof v === 'string') { return '"' + v.replace(/\\/g, '\\\\').replace(/"/g, '\\"') + '"'; }
        if (v instanceof Array) { var a = []; for (var q = 0; q < v.length; q++) { a.push(json(v[q])); } return '[' + a.join(',') + ']'; }
        var o = []; for (var key in v) { if (v.hasOwnProperty(key)) { o.push(json(key) + ':' + json(v[key])); } } return '{' + o.join(',') + '}';
    }
    var folder = new Folder(args.outDir);
    var mf = new File(folder + '/coverage-census.manifest.json'); mf.encoding = 'UTF-8'; mf.lineFeed = 'Unix';
    mf.open('w'); mf.write(json(manifest)); mf.close();
    function exportPdf(name, geometryOnly) {
        base.visible = !geometryOnly; geo.visible = geometryOnly;
        var f = new File(folder + '/coverage-census-' + name + '.pdf');
        if (f.exists) { throw 'refusing to overwrite ' + f.name; }
        var o = new PDFSaveOptions(); o.acrobatLayers = false; o.preserveEditability = false;
        doc.saveAs(f, o);
    }
    exportPdf('native', false);
    exportPdf('geometry', true);
    doc.close(SaveOptions.DONOTSAVECHANGES);
    return 'DONE|rows=' + rows.length + '|' + log.join('|');
    }
})();
