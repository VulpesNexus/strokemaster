// StrokeMaster AI-13 census: what does Illustrator hand a native post-effect
// whose source is live text? Two region-probe effects are compared:
//   region  declares group input only
//   census  declares group, path, and compound input but not text, so the host
//           decides what live text becomes before Go
// Both record the processing art at Go and pass it through unchanged. Cases,
// one at a time in one owned document, each removed before the next:
//   T1  point text "HH", character fill black and 6 pt gray character stroke
//   T2  the same text with no character paint (fill and stroke None)
//   T3  two-line point text "HH" / "HH", character fill and stroke as T1
//   P1  a filled and stroked rectangle, for comparison
// For each case the text must still be a live TextFrame with its contents.
// T1 under the census effect is also exported as a flat PDF when
// $.global.ISE_REGION_ARGS supplies outDir and runId. Returns a
// pipe-delimited record. Alerts are suppressed and the level restored.
(function () {
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try { return run(); } finally { app.userInteractionLevel = interaction; }
    function run() {
    var args = $.global.ISE_REGION_ARGS || {};
    var plugin = 'StrokeMasterRegionProbe';
    var effects = {
        region: 'org.strokemaster.proposed.closed-region.v1',
        census: 'org.strokemaster.proposed.input-census.v1'
    };
    var out = [];
    function note(k, v) { out.push(k + '=' + String(v).replace(/[|\r\n]/g, ' ')); }
    function send(selector, input) { return String(app.sendScriptMessage(plugin, selector, input || '')); }
    function field(json, name) {
        var m = new RegExp('"' + name + '":("[^"]*"|[-0-9.e]+|true|false)').exec(json);
        return m ? m[1] : '?';
    }
    function censusBlock(json) {
        var m = /"census":(\{[^}]*\})/.exec(json);
        return m ? m[1] : '?';
    }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    var doc = null, at = 'begin';
    function clear() { while (doc.pageItems.length > 0) doc.pageItems[0].remove(); app.redraw(); }
    function makeText(contents, painted) {
        var t = doc.textFrames.add();
        t.contents = contents;
        t.position = [0, 150];
        var a = t.textRange.characterAttributes;
        a.size = 48;
        if (painted) {
            a.fillColor = gray(100);
            a.strokeColor = gray(40);
            a.strokeWeight = 6;
        } else {
            a.fillColor = new NoColor();
            a.strokeColor = new NoColor();
        }
        return t;
    }
    function makeRect() {
        var r = doc.pathItems.rectangle(150, 0, 100, 60);
        r.filled = true; r.fillColor = gray(100);
        r.stroked = true; r.strokeColor = gray(40); r.strokeWidth = 6;
        return r;
    }
    function apply(item, which) {
        var xml = '<LiveEffect name="' + effects[which] + '"><Dict data="I StrokeMasterRegionFixtureVersion 1 "/></LiveEffect>';
        item.applyEffect(xml);
        app.redraw();
    }
    function textCase(name, which, make, contents) {
        at = name + '.' + which;
        var tag = name + '.' + which;
        var item = make();
        var before = send('status');
        apply(item, which);
        var after = send('status');
        if (which === 'region') {
            note(tag + '.goDelta', Number(field(after, 'goCalls')) - Number(field(before, 'goCalls')));
            note(tag + '.gate', field(after, 'gate'));
            note(tag + '.tree', field(after, 'processingCensus'));
        } else {
            var b = censusBlock(before), c = censusBlock(after);
            note(tag + '.callDelta', Number(field(c, 'calls')) - Number(field(b, 'calls')));
            note(tag + '.census', c);
        }
        if (contents !== null) {
            note(tag + '.liveTextKept', doc.textFrames.length === 1 && doc.textFrames[0].contents === contents);
        }
        return item;
    }
    try {
        note('build', field(send('status'), 'build'));
        doc = app.documents.add(DocumentColorSpace.RGB, 400, 200);
        var cases = [
            ['T1', function () { return makeText('HH', true); }, 'HH'],
            ['T2', function () { return makeText('HH', false); }, 'HH'],
            ['T3', function () { return makeText('HH\rHH', true); }, 'HH\rHH'],
            ['P1', makeRect, null]
        ];
        for (var i = 0; i < cases.length; ++i) {
            textCase(cases[i][0], 'region', cases[i][1], cases[i][2]);
            clear();
            textCase(cases[i][0], 'census', cases[i][1], cases[i][2]);
            if (cases[i][0] === 'T1' && args.outDir && args.runId) {
                at = 'pdf';
                var f = new File(args.outDir + '/' + args.runId + '-T1-census.pdf');
                if (f.exists) throw 'refusing to overwrite ' + f.name;
                var options = new PDFSaveOptions();
                options.acrobatLayers = false;
                doc.saveAs(f, options);
                note('T1.census.pdf', f.length);
            }
            clear();
        }
        at = 'close';
        doc.close(SaveOptions.DONOTSAVECHANGES);
        doc = null;
        return 'DONE|' + out.join('|');
    } catch (error) {
        if (doc) { try { doc.close(SaveOptions.DONOTSAVECHANGES); } catch (ignored) {} }
        return 'FAIL|' + at + ': ' + String(error).replace(/[|\r\n]/g, ' ') + '|' + out.join('|');
    }
    }
})();
