// Inspect a copy of a document the owner saved (args.file, never the owner's own file): every
// text frame and stroked path with its name, layer, character paint, and the plugin's style-dump
// of its own appearance (and its parent group's), then a PNG of the artboards. Read-only: the
// document is closed without saving by the runner. As a later phase it reads the document the first
// phase opened; each read writes inspect-<n>.json and view-<n>.png.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { try { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); } catch (e) { return 'ERR ' + e; } }
    function color(c) {
        if (!c) return null;
        if (c.typename === 'RGBColor') return ['rgb', Math.round(c.red), Math.round(c.green), Math.round(c.blue)];
        if (c.typename === 'GrayColor') return ['gray', c.gray];
        if (c.typename === 'CMYKColor') return ['cmyk', c.cyan, c.magenta, c.yellow, c.black];
        if (c.typename === 'SpotColor') return ['spot', c.spot.name];
        return [c.typename];
    }
    var interaction = app.userInteractionLevel, out = {items: []};
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        var doc = app.documents.length ? app.activeDocument : app.open(new File(args.file));
        var n = 1;
        while (lib.path(args, 'inspect-' + n + '.json').exists) n++;
        out.status = send('status');
        function layerPath(item) {
            var names = [], p = item.parent;
            while (p && p.typename !== 'Document') { names.unshift(p.typename + ':' + p.name); p = p.parent; }
            return names.join('/');
        }
        function visit(item) {
            var row = {type: item.typename, name: item.name, at: layerPath(item), locked: item.locked, hidden: item.hidden};
            if (item.typename === 'TextFrame') {
                row.contents = item.contents.substr(0, 40);
                row.kind = String(item.kind);
                row.storyFrames = item.story.textFrames.length;
                var a = item.textRange.characterAttributes;
                try { row.font = a.textFont.name; } catch (e) { row.font = 'mixed'; }
                row.charFill = color(a.fillColor); row.charStroke = color(a.strokeColor); row.charWeight = a.strokeWeight;
            } else if (item.typename === 'PathItem' || item.typename === 'CompoundPathItem') {
                var p = item.typename === 'PathItem' ? item : (item.pathItems.length ? item.pathItems[0] : null);
                if (p) { row.stroked = p.stroked; row.weight = p.strokeWidth; row.limit = p.strokeMiterLimit; row.join = String(p.strokeJoin); row.dashes = p.strokeDashes.length; }
            }
            if (item.typename === 'TextFrame' || item.typename === 'PathItem' || item.typename === 'CompoundPathItem') {
                doc.selection = null;
                item.selected = true;
                row.style = send('style-dump');
                if (item.parent.typename === 'GroupItem') row.parentStyle = send('style-dump', 'parent');
                if (item.typename === 'TextFrame') row.charLimits = send('char-limits');
                doc.selection = null;
            }
            out.items.push(row);
            if (item.typename === 'GroupItem') for (var i = 0; i < item.pageItems.length; i++) visit(item.pageItems[i]);
            if (item.typename === 'CompoundPathItem') return;
        }
        for (var L = 0; L < doc.layers.length; L++) {
            var layer = doc.layers[L];
            out.items.push({layer: layer.name, locked: layer.locked, visible: layer.visible, items: layer.pageItems.length});
            for (var i = 0; i < layer.pageItems.length; i++) visit(layer.pageItems[i]);
        }
        var o = new ExportOptionsPNG24(); o.artBoardClipping = true; o.antiAliasing = true; o.horizontalScale = 100; o.verticalScale = 100;
        var png = lib.path(args, 'view-' + n + '.png');
        if (!png.exists) { app.redraw(); doc.exportFile(png, ExportType.PNG24, o); }
        lib.write(args, 'inspect-' + n + '.json', out);
        return 'DONE|' + out.items.length;
    } catch (e) { return 'FAIL|' + String(e) + ' line ' + e.line; }
    finally { app.userInteractionLevel = interaction; }
})();
