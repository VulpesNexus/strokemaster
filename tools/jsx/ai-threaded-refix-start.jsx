// Owned fresh-process fixture: edit fixed threaded text, then allow the timer to idle.
(function () {
    if (app.documents.length) return 'FAIL|preexisting documents';
    var args = $.global.ISE_REGION_ARGS || {}, doc = null;
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        var p = new DocumentPreset(); p.width = 480; p.height = 360;
        p.colorMode = DocumentColorSpace.RGB;
        doc = app.documents.addDocument('Web', p);
        var a = doc.textFrames.areaText(doc.pathItems.rectangle(310,35,185,48));
        var b = doc.textFrames.areaText(doc.pathItems.rectangle(285,45,400,180));
        a.nextFrame = b;
        a.note = b.note = 'StrokeMasterParityRefix-' + args.runId;
        a.contents = 'AVMWxy AVMWxy AVMWxy AVMWxy';
        var style = a.story.textRange.characterAttributes;
        style.textFont = app.textFonts.getByName('MinionPro-Regular');
        style.size = 38; style.leading = 28; style.autoLeading = false;
        var black = new GrayColor(); black.gray = 100;
        var gray = new GrayColor(); gray.gray = 40;
        style.fillColor = black; style.strokeColor = gray; style.strokeWeight = 7;
        a.selected = b.selected = true;
        var route = String(app.sendScriptMessage('StrokeMasterRegionProbe','native-route',''));
        var fix = String(app.sendScriptMessage('StrokeMasterRegionProbe','native-fix',''));
        if (route.indexOf('"ok":true') < 0 || fix.indexOf('"ok":true') < 0) throw Error('correction failed');
        // The first idle phase registers the newly marked frames before the edit phase.
        doc.selection = null;
        return 'DONE|marked=2|storyFrames=' + a.story.textFrames.length;
    } catch (e) {
        if (doc) try { doc.close(SaveOptions.DONOTSAVECHANGES); } catch (ignored) {}
        return 'FAIL|' + String(e);
    } finally { app.userInteractionLevel = interaction; }
})();
