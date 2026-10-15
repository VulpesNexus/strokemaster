// ai-coverage-census.jsx - Illustrator's port of build-coverage-census.jsx (InDesign): every
// stroke case of the InDesign census that Illustrator has, drawn by Illustrator, beside its
// geometry layer, for tools/measure_coverage_census.py. Runs inside Illustrator through
// tools/run-ai-fresh.ps1, with StrokeMasterRegionProbe loaded for what scripting cannot set
// (stroke alignment, the native text routes, the text-kept effect).
//
// One row per case, 160 pt high, on a page 400 pt wide; the same base set as InDesign's
// (outlined "AMVWkz" at 48 pt, a 10-degree thorn with 20 pt sides, two overlapping squares in
// one compound path) or live text ("VWAMNvwkyzK", 36 pt). Black fill, Paper (white) stroke,
// on a Black 50% backdrop, so the levels match the InDesign census. The layer "ISE geometry"
// holds every item again, filled Black, unstroked, opaque, effect-free: the outline the stroke
// follows. Illustrator applies transforms to the art, so no row needs an untransformed twin.
//
// Illustrator text strokes are always centered. A text row labeled "outside w" draws a center
// stroke 2w wide under the route that hides its inner half (an object-level fill above the
// characters, or the text-kept effect), which is what InDesign's Outside w shows.
//
// Exports coverage-census-native.pdf (everything but the geometry) and
// coverage-census-geometry.pdf (the geometry layer alone), and writes
// coverage-census.manifest.json. Needs $.global.ISE_REGION_ARGS outDir (and runId, unused).
// Alerts suppressed and restored.
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
    function tryDo(what, f) { try { return f(); } catch (e) { note('error ' + what, e + ' line ' + e.line); return null; } }

    // Rows 160 pt high, items 70 pt down: a whole point kept by the native limit (the thorn's
    // reaches 46 pt at Center 8) stays in its own row (at 110 / 25 it reached into the next).
    var W = 400, ROW = 160, TOP = 70, TEXT = 'VWAMNvwkyzK';
    function font(names) {
        for (var q = 0; q < names.length; q++) { try { return app.textFonts.getByName(names[q]); } catch (e) {} }
        return null;
    }
    var MINION = font(['MinionPro-Regular']), GB = font(['GermanBeauty', 'GermanBeauty-Regular', 'German Beauty']);
    var SCRIPT = font(['SegoeScript']), VIBES = font(['GreatVibes-Regular']), TNR = font(['TimesNewRomanPSMT']);
    var fontNames = { MINION: MINION ? MINION.name : 'missing', GB: GB ? GB.name : 'missing', SCRIPT: SCRIPT ? SCRIPT.name : 'missing',
                      VIBES: VIBES ? VIBES.name : 'missing', TNR: TNR ? TNR.name : 'missing' };
    for (var fk in fontNames) { note('font ' + fk, fontNames[fk]); }
    if (!MINION) return 'FAIL|Minion Pro missing|' + log.join('|');

    var cases = [];
    function P(name, align, w, join, extra, font2, text) { cases.push({ kind: 'page item', name: name, align: align, w: w, join: join, extra: extra, font: font2 || MINION, text: text }); }
    function T(name, w, route, extra, font2) { cases.push({ kind: 'text', name: name, w: w, route: route, extra: extra || {}, font: font2 || MINION }); }
    var M = StrokeJoin.MITERENDJOIN, R = StrokeJoin.ROUNDENDJOIN, B = StrokeJoin.BEVELENDJOIN;
    function dashed(it) { paths(it, function (p) { p.strokeDashes = [12, 4]; }); }
    function opacity(it) { it.opacity = 50; }
    function limit2(it) { paths(it, function (p) { p.strokeMiterLimit = 2; }); }
    function roundCorners(it) { it.applyEffect('<LiveEffect name="Adobe Round Corners"><Dict data="R radius 4 "/></LiveEffect>'); }
    function shadow(it) { it.applyEffect('<LiveEffect name="Adobe Drop Shadow"><Dict data="R horz 4 R vert 4 R blur 2 R opac 0.75 I blnd 1 I csrc 1 R dark 0 "/></LiveEffect>'); }
    function both(f, g) { return function (it) { f(it); g(it); }; }

    P('center 8 miter', 'center', 8, M);
    P('outside 4 miter', 'outside', 4, M);
    P('inside 4 miter', 'inside', 4, M);
    P('inside 8 miter', 'inside', 8, M);
    P('outside 4 round', 'outside', 4, R);
    P('outside 4 bevel', 'outside', 4, B);
    P('center 8 round', 'center', 8, R);
    P('center 8 bevel', 'center', 8, B);
    P('outside 4 miter dashed 12/4', 'outside', 4, M, dashed);
    P('center 8 miter dashed 12/4', 'center', 8, M, dashed);
    P('inside 4 miter dashed 12/4', 'inside', 4, M, dashed);
    P('outside 4 round dashed 12/4', 'outside', 4, R, dashed);
    P('outside 4 miter round corners effect 4', 'outside', 4, M, roundCorners);
    P('center 8 miter round corners effect 4', 'center', 8, M, roundCorners);
    P('outside 4 miter opacity 50', 'outside', 4, M, opacity);
    P('center 8 miter opacity 50', 'center', 8, M, opacity);
    P('inside 4 miter opacity 50', 'inside', 4, M, opacity);
    P('outside 4 miter drop shadow', 'outside', 4, M, shadow);
    P('outside 4 miter gradient fill', 'outside', 4, M, function (it) { paths(it, function (p) { p.fillColor = gradientColor(darkGradient); }); });
    P('outside 4 miter gradient stroke', 'outside', 4, M, function (it) { paths(it, function (p) { p.strokeColor = gradientColor(lightGradient); }); });
    P('inside 4 miter gradient fill', 'inside', 4, M, function (it) { paths(it, function (p) { p.fillColor = gradientColor(darkGradient); }); });
    P('outside 4 miter scaled 50%, strokes scaled', 'outside', 8, M, null, null, null);
    cases[cases.length - 1].transform = { scale: 50 };
    P('outside 4 miter sheared 20', 'outside', 4, M);
    cases[cases.length - 1].transform = { shear: 20 };
    P('center 8 miter sheared 20', 'center', 8, M);
    cases[cases.length - 1].transform = { shear: 20 };
    P('inside 4 miter sheared 20', 'inside', 4, M);
    cases[cases.length - 1].transform = { shear: 20 };
    P('outside 4 miter rotated 10', 'outside', 4, M);
    cases[cases.length - 1].transform = { rotate: 10 };
    P('outside 4 miter flipped', 'outside', 4, M);
    cases[cases.length - 1].transform = { flip: true };
    P('center 8 miter open, filled', 'center', 8, M);
    cases[cases.length - 1].open = 'filled';
    P('center 8 miter open, unfilled', 'center', 8, M);
    cases[cases.length - 1].open = 'unfilled';
    P('outside 4 miter open (alignment as Illustrator keeps it)', 'outside', 4, M);
    cases[cases.length - 1].open = 'filled';
    P('center 8 miter open, round caps', 'center', 8, M, function (it) { paths(it, function (p) { p.strokeCap = StrokeCap.ROUNDENDCAP; }); });
    cases[cases.length - 1].open = 'filled';
    P('inside 4 round', 'inside', 4, R);
    P('inside 4 bevel', 'inside', 4, B);
    P('inside 4 miter limit 2', 'inside', 4, M, limit2);
    P('outside 4 bevel limit 2', 'outside', 4, B, limit2);
    P('outside 4 miter limit 2', 'outside', 4, M, limit2);
    P('center 8 miter limit 2', 'center', 8, M, limit2);
    if (GB) {
        P('GB outside 4 miter', 'outside', 4, M, null, GB);
        P('GB center 8 miter', 'center', 8, M, null, GB);
        P('GB inside 4 miter', 'inside', 4, M, null, GB);
        P('GB outside 4 round', 'outside', 4, R, null, GB);
        P('GB outside 4 bevel', 'outside', 4, B, null, GB);
        P('GB inside 4 round', 'inside', 4, R, null, GB);
        P('GB outside 4 miter dashed 12/4', 'outside', 4, M, dashed, GB);
        P('GB outside 4 miter opacity 50', 'outside', 4, M, opacity, GB);
        P('GB center 8 miter opacity 50', 'center', 8, M, opacity, GB);
    }
    if (SCRIPT) {
        P('script outside 4 miter', 'outside', 4, M, null, SCRIPT, 'Ribbons');
        P('script center 8 miter', 'center', 8, M, null, SCRIPT, 'Ribbons');
        P('script outside 4 miter dashed 12/4', 'outside', 4, M, dashed, SCRIPT, 'Ribbons');
    }
    if (VIBES) {
        P('vibes outside 4 miter', 'outside', 4, M, null, VIBES, 'Angel');
        P('vibes center 8 miter', 'center', 8, M, null, VIBES, 'Angel');
    }

    // Live text. Routes: native (character strokes as typed), fill (an object-level fill above
    // the characters), effect (the text-kept stroke-behind effect).
    T('text center 8 miter, native', 8, 'native');
    T('text outside 4 miter, fill above (stroked 8)', 8, 'fill');
    T('text outside 4 miter, effect (stroked 8)', 8, 'effect');
    T('text center 8 miter limit 2, native', 8, 'native', { join: '2' });
    T('text outside 4 miter limit 2, fill above (stroked 8)', 8, 'fill', { join: '2' });
    T('text outside 2 miter, fill above (stroked 4)', 4, 'fill');
    T('text outside 4 miter, fill above, frame opacity 50', 8, 'fill', { frame: opacity });
    T('text outside 4 miter, fill above, drop shadow', 8, 'fill', { frame: shadow });
    T('text outside 4 miter, fill above, rotated 10', 8, 'fill', { transform: { rotate: 10 } });
    // No sheared text row: a matrix-sheared text frame was drawn across rows 17 to 51 of the
    // export (2026-10-15). Page items are sheared in their points instead.
    T('text outside 4 miter, fill above, scaled 150% x 100%', 8, 'fill', { transform: { sx: 150, sy: 100 } });
    T('text outside 4 miter, fill above, type on a path', 8, 'fill', { path: true });
    T('text outside 4 miter, effect, type on a path', 8, 'effect', { path: true });
    T('text outside 4 miter, fill above, area text', 8, 'fill', { area: true });
    T('text outside 4 miter, fill above, gradient stroke', 8, 'fill', { text: function (t) { t.strokeColor = gradientColor(lightGradient); } });
    if (GB) {
        T('text GB center 8 miter, native', 8, 'native', null, GB);
        T('text GB outside 4 miter, fill above (stroked 8)', 8, 'fill', null, GB);
        T('text GB outside 4 miter, effect (stroked 8)', 8, 'effect', null, GB);
    }
    if (SCRIPT) {
        T('text script center 8 miter, native', 8, 'native', { text2: 'Ribbons' }, SCRIPT);
        T('text script outside 4 miter, fill above (stroked 8)', 8, 'fill', { text2: 'Ribbons' }, SCRIPT);
    }
    if (VIBES) {
        T('text vibes outside 4 miter, fill above (stroked 8)', 8, 'fill', { text2: 'Angel' }, VIBES);
    }

    var H = 20 + cases.length * ROW;
    var doc = newDocument(DocumentColorSpace.CMYK, W, H);
    doc.artboards[0].artboardRect = [0, H, W, 0];
    var baseLayer = doc.layers[0];
    baseLayer.name = 'ISE census';
    var geoLayer = doc.layers.add();
    geoLayer.name = 'ISE geometry';
    geoLayer.move(baseLayer, ElementPlacement.PLACEAFTER);
    function cmyk(k) { var c = new CMYKColor(); c.cyan = 0; c.magenta = 0; c.yellow = 0; c.black = k; return c; }
    var black = cmyk(100), paper = cmyk(0), none = new NoColor();
    function gradient(name, a, b, type) {
        var g = doc.gradients.add();
        g.name = name;
        g.type = type || GradientType.LINEAR;
        g.gradientStops[0].color = a;
        g.gradientStops[1].color = b;
        return g;
    }
    function gradientColor(g) { var c = new GradientColor(); c.gradient = g; return c; }
    var darkGradient = gradient('ISE dark gradient', cmyk(70), cmyk(100));
    var lightGradient = gradient('ISE light gradient', cmyk(0), cmyk(20));
    var backdrop = baseLayer.pathItems.rectangle(H, 0, W, H);
    backdrop.filled = true;
    backdrop.fillColor = cmyk(50);
    backdrop.stroked = false;
    backdrop.name = 'backdrop: Black 50%, opaque';

    // Page coordinates (top down, as the manifest and the measurement use) to Illustrator's.
    function Y(y) { return H - y; }
    function pageBounds(it) { var b = it.visibleBounds; return [H - b[1], b[0], H - b[3], b[2]]; }
    function geomBounds(it) { var b = it.geometricBounds; return [H - b[1], b[0], H - b[3], b[2]]; }
    function leafPaths(it) {
        if (it.typename === 'PathItem') { return [it]; }
        if (it.typename === 'CompoundPathItem') { return it.pathItems.length ? [it.pathItems[0]] : []; }
        if (it.typename === 'GroupItem') { var r = []; for (var q = 0; q < it.pageItems.length; q++) { r = r.concat(leafPaths(it.pageItems[q])); } return r; }
        return [];
    }
    function paths(it, f) { var l = leafPaths(it); for (var q = 0; q < l.length; q++) { f(l[q]); } }
    function allPaths(it) {
        if (it.typename === 'PathItem') { return [it]; }
        if (it.typename === 'CompoundPathItem') { var r = []; for (var q = 0; q < it.pathItems.length; q++) { r.push(it.pathItems[q]); } return r; }
        return [];
    }
    function flat(it) {
        if (it.typename === 'GroupItem') { var r = []; for (var q = 0; q < it.pageItems.length; q++) { r = r.concat(flat(it.pageItems[q])); } return r; }
        return [it];
    }
    // Clearing the selection takes effect only once it is read back (2026-10-15: an item
    // applyEffect had selected stayed selected and was styled with the next row).
    function deselect() { doc.selection = null; return doc.selection.length; }
    function select(items) {
        deselect();
        for (var q = 0; q < items.length; q++) { items[q].selected = true; }
        if (doc.selection.length !== items.length) { note('selection mismatch', doc.selection.length + ' for ' + items.length); }
    }
    function align(items, a) {
        if (a === 'center') { return; }
        select(items);
        var r = send('stroke-align', a);
        if (!/"ok":true/.test(r)) { note('align failed ' + a, r.substring(0, 80)); }
        deselect();
    }
    // The base set, its top left at page x, y.
    function baseSet(x, y, f, text) {
        var t = baseLayer.textFrames.add();
        t.contents = text || 'AMVWkz';
        t.textRange.characterAttributes.textFont = f;
        t.textRange.characterAttributes.size = 48;
        t.position = [x, Y(y)];
        // One compound path for the whole word, as InDesign's createOutlines makes it
        // (Illustrator makes one per glyph).
        var g = t.createOutline();
        var word = baseLayer.compoundPathItems.add();
        var glyphs = flat(g);
        for (var q = 0; q < glyphs.length; q++) {
            var gp = allPaths(glyphs[q]);
            for (var r = gp.length - 1; r >= 0; r--) { gp[r].move(word, ElementPlacement.PLACEATEND); }
        }
        g.remove();
        var items = [word];
        var half = 5 * Math.PI / 180, side = 20, cx = x + 240, tip = y + 25;
        var bw = side * Math.sin(half), by = tip + side * Math.cos(half);
        var thorn = baseLayer.pathItems.add();
        thorn.setEntirePath([[cx, Y(tip)], [cx + bw, Y(by)], [cx + 15, Y(by)], [cx + 15, Y(by + 30)], [cx - 15, Y(by + 30)], [cx - 15, Y(by)], [cx - bw, Y(by)]]);
        thorn.closed = true;
        items.push(thorn);
        var sq = baseLayer.compoundPathItems.add();
        var x2 = x + 280, y2 = y + 15, a = 40, o = 20;
        var p1 = sq.pathItems.add();
        p1.setEntirePath([[x2, Y(y2)], [x2 + a, Y(y2)], [x2 + a, Y(y2 + a)], [x2, Y(y2 + a)]]);
        p1.closed = true;
        var p2 = sq.pathItems.add();
        p2.setEntirePath([[x2 + o, Y(y2 + o)], [x2 + o + a, Y(y2 + o)], [x2 + o + a, Y(y2 + o + a)], [x2 + o, Y(y2 + o + a)]]);
        p2.closed = true;
        items.push(sq);
        // Nonzero, as InDesign fills them: the squares unite, overlapping glyph contours too.
        for (var q2 = 0; q2 < items.length; q2++) { var ps = allPaths(items[q2]); for (var r2 = 0; r2 < ps.length; r2++) { ps[r2].evenodd = false; } }
        return items;
    }
    // Left to right from page x, each item's top at page y, 24 pt apart.
    function layOut(items, x, y) {
        for (var q = 0; q < items.length; q++) {
            var b = items[q].visibleBounds;
            items[q].translate(x - b[0], Y(y) - b[1]);
            b = items[q].visibleBounds;
            x += b[2] - b[0] + 24;
        }
    }
    function paint(it, c) {
        var l = leafPaths(it);
        for (var q = 0; q < l.length; q++) {
            l[q].filled = true; l[q].fillColor = black;
            l[q].stroked = true; l[q].strokeColor = paper;
            l[q].strokeWidth = c.w; l[q].strokeJoin = c.join; l[q].strokeMiterLimit = 4;
        }
    }
    function shear(it, deg) {
        // Every value set: a matrix from getIdentityMatrix after a translate sheared and grew
        // items several times over (2026-10-15).
        var m = app.getIdentityMatrix();
        m.mValueA = 1; m.mValueB = 0; m.mValueC = Math.tan(deg * Math.PI / 180); m.mValueD = 1; m.mValueTX = 0; m.mValueTY = 0;
        var before = it.geometricBounds;
        it.transform(m, true, true, true, true, 100, Transformation.CENTER);
        var after = it.geometricBounds;
        if (Math.abs((after[1] - after[3]) - (before[1] - before[3])) > 0.01) { note('shear grew ' + it.typename, (before[1] - before[3]) + ' -> ' + (after[1] - after[3])); }
    }
    function shearPoints(it, deg) {
        var k = Math.tan(deg * Math.PI / 180), gb = it.geometricBounds, yc = (gb[1] + gb[3]) / 2;
        var l = allPaths(it);
        for (var q = 0; q < l.length; q++) {
            var pts = l[q].pathPoints;
            for (var r = 0; r < pts.length; r++) {
                var pp = pts[r], an = pp.anchor, ld = pp.leftDirection, rd = pp.rightDirection;
                pp.anchor = [an[0] + k * (an[1] - yc), an[1]];
                pp.leftDirection = [ld[0] + k * (ld[1] - yc), ld[1]];
                pp.rightDirection = [rd[0] + k * (rd[1] - yc), rd[1]];
            }
        }
    }
    function transform(it, tr) {
        if (tr.scale) { it.resize(tr.scale, tr.scale, true, true, true, true, tr.scale, Transformation.CENTER); }
        if (tr.sx) { it.resize(tr.sx, tr.sy, true, true, true, true, 100, Transformation.CENTER); }
        if (tr.shear) {
            // Page items are sheared in their points: a matrix transform of a compound path
            // left the next transform in the document wrong (items grew several times over,
            // 2026-10-15). Text frames keep the matrix.
            if (it.typename === 'TextFrame') { shear(it, tr.shear); } else { shearPoints(it, tr.shear); }
        }
        if (tr.rotate) { it.rotate(tr.rotate, true, true, true, true, Transformation.CENTER); }
        if (tr.flip) { it.resize(-100, 100, true, true, true, true, 100, Transformation.CENTER); }
    }
    function open(it, how) {
        var l = allPaths(it);
        for (var q = 0; q < l.length; q++) { l[q].closed = false; if (how === 'unfilled') { l[q].filled = false; } }
    }
    // The geometry layer: the same outline, filled Black, unstroked, opaque, effect-free.
    function geometry(it) {
        var g = it.duplicate(geoLayer, ElementPlacement.PLACEATEND);
        var l = allPaths(g);
        for (var q = 0; q < l.length; q++) { l[q].filled = true; l[q].fillColor = black; l[q].stroked = false; }
        return g;
    }
    function part(k, n) { return k === n - 1 ? 'squares' : (k === n - 2 ? 'thorn' : 'text'); }
    var manifest = { page: [W, H], frames: [], items: [], app: app.version, fonts: fontNames };
    var row = 0, i, c, k;
    function y0() { return 20 + row * ROW; }

    for (i = 0; i < cases.length; i++) {
        c = cases[i];
        if (c.kind !== 'page item') { continue; }
        var items = tryDo('base ' + c.name, function () { return baseSet(16, y0(), c.font, c.text); });
        if (!items) { row++; continue; }
        var rec = { row: row, kind: c.kind, label: c.name, font: c.font.family, weight: c.w * (c.transform && c.transform.scale ? c.transform.scale / 100 : 1), cell: [y0(), 0, y0() + ROW, W], items: [] };
        for (k = 0; k < items.length; k++) {
            paint(items[k], c);
            if (c.open) { open(items[k], c.open); }
        }
        if (c.transform) { for (k = 0; k < items.length; k++) { tryDo('transform ' + c.name, function () { transform(items[k], c.transform); }); } }
        layOut(items, 16, y0() + TOP);
        for (k = 0; k < items.length; k++) {
            var g = geometry(items[k]);
            items[k].name = c.name + ' #' + k;
            rec.items.push({ name: items[k].name, part: part(k, items.length), geometryBounds: geomBounds(g) });
        }
        align(items, c.align);
        if (c.extra) { for (k = 0; k < items.length; k++) { tryDo('extra ' + c.name, function () { c.extra(items[k]); }); } }
        manifest.items.push(rec);
        row++;
    }
    var textBehind = '<LiveEffect name="org.strokemaster.proposed.stroke-behind-text.v1"><Dict data="I StrokeMasterRegionFixtureVersion 1 "/></LiveEffect>';
    for (i = 0; i < cases.length; i++) {
        c = cases[i];
        if (c.kind !== 'text') { continue; }
        var x = c.extra || {}, t;
        if (x.path) {
            var host = baseLayer.pathItems.add();
            host.setEntirePath([[16, Y(y0() + TOP + 55)], [380, Y(y0() + TOP + 55)]]);
            host.pathPoints[0].rightDirection = [120, Y(y0() + TOP + 25)];
            host.pathPoints[1].leftDirection = [260, Y(y0() + TOP + 35)];
            host.stroked = false; host.filled = false;
            t = baseLayer.textFrames.pathText(host);
        } else if (x.area) {
            var box = baseLayer.pathItems.rectangle(Y(y0() + TOP - 5), 16, W - 32, 80);
            t = baseLayer.textFrames.areaText(box);
        } else {
            t = baseLayer.textFrames.add();
        }
        t.contents = x.text2 || TEXT;
        var ca = t.textRange.characterAttributes;
        ca.textFont = c.font; ca.size = 36;
        ca.fillColor = black; ca.strokeColor = paper; ca.strokeWeight = c.w;
        if (x.text) { tryDo('text ' + c.name, function () { x.text(ca); }); }
        if (!x.path && !x.area) { t.position = [16, Y(y0() + TOP)]; }
        if (x.transform) {
            tryDo('transform ' + c.name, function () { transform(t, x.transform); });
            // position, not translate: a sheared frame did not move by what translate asked.
            t.position = [16, Y(y0() + TOP - 15)];
        }
        if (x.join) { select([t]); note('join ' + c.name, send('appearance', 'miter:' + x.join).indexOf('"gate":"joined"') >= 0); deselect(); }
        // The outline on the geometry layer: a duplicate of the text, converted.
        var rec2 = { row: row, kind: 'text', label: c.name, font: c.font.family, weight: c.w / (c.route === 'native' ? 1 : 2), cell: [y0(), 0, y0() + ROW, W], items: [] };
        tryDo('outline ' + c.name, function () {
            var d = t.duplicate(geoLayer, ElementPlacement.PLACEATEND);
            var og = d.createOutline();
            var outl = flat(og);
            for (var q = 0; q < outl.length; q++) {
                var l = allPaths(outl[q]);
                for (var r = 0; r < l.length; r++) { l[r].filled = true; l[r].fillColor = black; l[r].stroked = false; }
                rec2.items.push({ name: c.name + ' outline #' + q, part: 'text', geometryBounds: geomBounds(outl[q]) });
            }
        });
        if (c.route === 'fill') { select([t]); note('fill ' + c.name, send('appearance', 'fill-above').indexOf('"gate":"styled"') >= 0); deselect(); }
        if (c.route === 'effect') { tryDo('effect ' + c.name, function () { t.applyEffect(textBehind); }); }
        if (x.frame) { tryDo('frame ' + c.name, function () { x.frame(t); }); }
        t.name = c.name;
        for (k = 0; k < rec2.items.length; k++) {
            var gbk = rec2.items[k].geometryBounds;
            if (gbk[0] < y0() - 5 || gbk[2] > y0() + ROW + 5 || gbk[1] < -5 || gbk[3] > W + 5) { note('outside its cell', c.name); break; }
        }
        manifest.items.push(rec2);
        row++;
    }
    backdrop.zOrder(ZOrderMethod.SENDTOBACK);   // items moved to the layer's end went behind it
    manifest.rows = row;
    manifest.rowHeight = ROW;
    app.redraw();

    function json(v) {
        if (v === null || v === undefined) { return 'null'; }
        if (typeof v === 'number') { return isFinite(v) ? String(v) : 'null'; }
        if (typeof v === 'boolean') { return String(v); }
        if (typeof v === 'string') { return '"' + v.replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/[\x00-\x1f]/g, ' ') + '"'; }
        if (v instanceof Array) { var a = []; for (var q = 0; q < v.length; q++) { a.push(json(v[q])); } return '[' + a.join(',') + ']'; }
        var o = []; for (var key in v) { if (v.hasOwnProperty(key)) { o.push(json(key) + ':' + json(v[key])); } } return '{' + o.join(',') + '}';
    }
    var folder = new Folder(args.outDir);
    var mf = new File(folder + '/coverage-census.manifest.json');
    mf.encoding = 'UTF-8'; mf.lineFeed = 'Unix';
    mf.open('w'); mf.write(json(manifest)); mf.close();
    function exportPdf(name, geometryOnly) {
        baseLayer.visible = !geometryOnly;
        geoLayer.visible = geometryOnly;
        var f = new File(folder + '/coverage-census-' + name + '.pdf');
        if (f.exists) { throw 'refusing to overwrite ' + f.name; }
        var o = new PDFSaveOptions();
        o.acrobatLayers = false;
        o.preserveEditability = false;
        doc.saveAs(f, o);
        note(name + '.pdf', f.length);
    }
    exportPdf('native', false);
    // Corrected: the native fixes on every census item (not the backdrop): the text route
    // (native-route; frames already routed or carrying effects are refused) and the miter
    // limits and Outside paint order (native-fix). Ordinary document data, as a user gets it.
    baseLayer.visible = true; geoLayer.visible = false;
    deselect();
    for (var s2 = 0; s2 < baseLayer.pageItems.length; s2++) {
        var it2 = baseLayer.pageItems[s2];
        if (it2 !== backdrop) { it2.selected = true; }
    }
    note('selected for fixes', doc.selection.length);
    note('native-route', /"appearance":(\{[^}]*\})/.exec(send('native-route'))[1]);
    note('native-fix', /"nativeFix":(\{[^}]*\})/.exec(send('native-fix'))[1]);
    deselect();
    app.redraw();
    exportPdf('corrected', false);
    exportPdf('geometry', true);
    doc.close(SaveOptions.DONOTSAVECHANGES);
    return 'DONE|rows=' + row + '|' + log.join('|');
    }
})();
