// Panel test, phase 1 of 2 (region-prep-26): the StrokeMaster panel exists, a switch pressed by
// script (ui-press: BM_CLICK, not a hand's click) changes the settings file and Restore Defaults
// puts it back, and Correct Selection queues the correction for the timer. Builds a two-frame
// threaded story (Minion Pro 38 pt, one red fill, one gray 7 pt stroke; the second frame
// overlaps the first's text), exports <runId>-fill.pdf and <runId>-native.pdf, selects both
// frames, presses Correct Selection, and leaves the document open for ai-panel-final.jsx.
// The settings file is removed at the end of phase 2 if it did not exist before.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId) return 'FAIL|missing arguments';
    if (app.documents.length) return 'FAIL|requires zero preexisting documents';
    var here = new File($.fileName).parent.fsName;
    var lib = $.evalFile(new File(here + '/ai-text-parity-lib.jsx'));
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    var out = {}, interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    try {
        var settingsFile = new File(args.settingsPath || (Folder.userData.fsName + '/StrokeMaster/settings-illustrator.txt'));
        out.settingsExisted = settingsFile.exists;
        out.status0 = send('ui-status');
        out.settings0 = send('settings', '?');
        out.pressStories = send('ui-press', 'text_stories');
        out.settings1 = send('settings', '?');
        out.fileAfterPress = settingsFile.exists;
        out.pressRestore = send('ui-press', 'restore');
        out.settings2 = send('settings', '?');
        out.pressOffMaster = send('ui-press', 'enabled');
        out.statusMasterOff = send('ui-status');
        out.pressOnMaster = send('ui-press', 'enabled');
        var p = new DocumentPreset(); p.colorMode = DocumentColorSpace.RGB; p.width = 480; p.height = 360; p.units = RulerUnits.Points;
        var doc = app.documents.addDocument('Web', p);
        doc.artboards[0].artboardRect = [0, 360, 480, 0];
        var t = doc.textFrames.areaText(doc.pathItems.rectangle(310, 35, 185, 48));
        t.nextFrame = doc.textFrames.areaText(doc.pathItems.rectangle(285, 45, 400, 180));
        t.contents = 'AVMWxy AVMWxy AVMWxy AVMWxy';
        var a = t.story.textRange.characterAttributes;
        a.textFont = app.textFonts.getByName('MinionPro-Regular'); a.size = 38; a.leading = 28; a.autoLeading = false;
        var red = new RGBColor(); red.red = 200; red.green = 30; red.blue = 30;
        a.fillColor = red; a.strokeColor = new NoColor();
        app.redraw(); lib.pdf(doc, args, 'fill');
        a.strokeColor = gray(40); a.strokeWeight = 7;
        app.redraw(); lib.pdf(doc, args, 'native');
        doc.selection = null;
        for (var f = 0; f < t.story.textFrames.length; f++) t.story.textFrames[f].selected = true;
        out.pressCorrect = send('ui-press', 'correct');
        out.groupsAfterPress = doc.groupItems.length;   // still 0: the timer runs it after this script
        lib.write(args, 'phase1.json', out);
        return 'DONE|phase1';
    } catch (e) { return 'FAIL|' + String(e); }
    finally { app.userInteractionLevel = interaction; }
})();
