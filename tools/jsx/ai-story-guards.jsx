// Owned native-story grouping admission and rollback controls.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (app.documents.length) return 'FAIL|preexisting document';
    var lib = $.evalFile(new File(new File($.fileName).parent.fsName + '/ai-text-parity-lib.jsx'));
    var driver = new File(new File($.fileName).parent.fsName + '/strokemaster-fix-story.jsx');
    var doc = null, rows = [], interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    function tree(parent) {
        var out = [];
        for (var i = 0; i < parent.pageItems.length; i++) {
            var x = parent.pageItems[i];
            if (x.parent == parent) out.push([x.uuid,x.typename,x.zOrderPosition]);
        }
        return out;
    }
    function state() {
        var selected = [];
        for (var i = 0; i < doc.selection.length; i++) selected.push(doc.selection[i].uuid);
        selected.sort();
        return lib.json({tree:tree(doc.layers[0]),groups:doc.groupItems.length,selected:selected,text:lib.snapshot(doc)});
    }
    try {
        var cases = [args.guardCase];
        if (!/^(intervening|partial|independent|mixed-fill)$/.test(args.guardCase)) return 'FAIL|unknown control';
        for (var k = 0; k < cases.length; k++) {
            var name = cases[k];
            lib.write(args,'started.json',{name:name});
            doc = app.documents.add(DocumentColorSpace.RGB,480,360);
            var a = doc.textFrames.areaText(doc.pathItems.rectangle(310,35,185,48));
            var b = doc.textFrames.areaText(doc.pathItems.rectangle(285,45,400,180));
            if (name !== 'independent') a.nextFrame = b;
            a.contents = 'AVMWxy AVMWxy AVMWxy AVMWxy';
            if (name === 'independent') b.contents = 'AVMWxy';
            var attrs = a.story.textRange.characterAttributes;
            attrs.textFont = app.textFonts.getByName('MinionPro-Regular'); attrs.size = 38;
            var black = new GrayColor(); black.gray = 100; attrs.fillColor = black;
            if (name === 'mixed-fill') { var gray = new GrayColor(); gray.gray = 65; a.story.textRange.characters[2].characterAttributes.fillColor = gray; }
            if (name === 'intervening') {
                var shape = doc.pathItems.rectangle(300,20,80,80);
                shape.move(b,ElementPlacement.PLACEAFTER);
            }
            doc.selection = null; a.selected = true;
            if (name !== 'partial') b.selected = true;
            app.redraw();
            var before = state(); lib.write(args,'before.json',{state:before});
            var raw = String($.evalFile(driver)), result = eval('(' + raw + ')');
            app.redraw();
            var after = state();
            var expected = name === 'intervening' ? 'story.intervening-art' : (name === 'partial' ? 'story.selection' : (name === 'independent' ? 'story.incomplete' : 'story.refused-restored'));
            rows.push({name:name,gate:result.appearance.gate,expected:expected,unchanged:before === after,ok:!result.ok && result.appearance.gate === expected && before === after});
            doc.close(SaveOptions.DONOTSAVECHANGES); doc = null;
        }
        lib.write(args,'results.json',{host:app.version,rows:rows});
        for (var q = 0; q < rows.length; q++) if (!rows[q].ok) return 'FAIL|guard=' + rows[q].name + '|gate=' + rows[q].gate + '|unchanged=' + rows[q].unchanged;
        return 'DONE|guards=' + rows.length;
    } catch (e) { return 'FAIL|' + String(e); }
    finally { if (doc) doc.close(SaveOptions.DONOTSAVECHANGES); app.userInteractionLevel = interaction; }
})();
