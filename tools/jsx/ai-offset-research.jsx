// ai-offset-research.jsx - what Illustrator writes for Effect > Path > Offset Path (2026-10-16), read
// with StrokeMasterRegionProbe's style-dump: a square with the effect applied by script, each join
// type (jntp 0, 1, 2), offset -4, limit 4. Returns the dumps. Needs $.global.ISE_REGION_ARGS.
(function () {
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try { return run(); } finally { app.userInteractionLevel = interaction; }
    function run() {
    var out = [];
    function note(k, v) { out.push(k + '=' + String(v).replace(/[|\r\n]/g, ' ')); }
    var doc = app.documents.addDocument('Web', new DocumentPreset());
    try {
        for (var j = 0; j < 3; j++) {
            var r = doc.pathItems.rectangle(300 - j * 90, 50, 60, 60);
            r.applyEffect('<LiveEffect name="Adobe Offset Path"><Dict data="R ofst -4 I jntp ' + j + ' R mlim 4 "/></LiveEffect>');
            doc.selection = null; r.selected = true;
            note('jntp ' + j, String(app.sendScriptMessage('StrokeMasterRegionProbe', 'style-dump', '')));
        }
        return 'DONE|' + out.join('|');
    } catch (e) {
        return 'FAIL|' + String(e).replace(/[|\r\n]/g, ' ') + '|' + out.join('|');
    }
    }
})();
