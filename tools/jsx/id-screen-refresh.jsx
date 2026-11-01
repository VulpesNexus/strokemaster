// Disposable layout-window regression for cross-frame screen caches.
// Run separate calls with ISE_ARGS.action: create, move, apart, back, undo, redo,
// edit, restore-text, export, or close. Capture the window after the call returns
// and BEFORE export. Only a fresh host on the isolated desktop may run this test.
(function () {
    var args = $.global.ISE_ARGS || {}, action = args.action || 'create';
    var label = 'StrokeMasterScreenRefreshTest', doc = null;
    var interaction = app.scriptPreferences.userInteractionLevel;
    if (!$.getenv('CODEX_HIDDEN_DESKTOP_NAME')) throw Error('isolated desktop required');
    function topFrame(d) {
        var items=d.allPageItems;
        for(var n=0;n<items.length;n++) if(items[n].name === 'sm-top') return items[n];
        throw Error('top frame missing');
    }
    // No preference writes around undo/redo: a new scripted transaction can consume
    // the undo target or clear the redo stack before the geometry is inspected.
    if (action === 'undo' || action === 'redo' || action === 'inspect') {
        for (var u = 0; u < app.documents.length; u++)
            if (app.documents[u].extractLabel(label) === 'owned') doc = app.documents[u];
        if (!doc) throw Error('owned fixture missing');
        if (action === 'undo') doc.undo(); else if(action === 'redo') doc.redo();
        return 'DONE|' + action + '|windows=' + doc.layoutWindows.length + '|top=' +
               String(topFrame(doc).geometricBounds) +
               '|occluder=' + doc.rectangles.itemByName('sm-cover').isValid;
    }
    app.scriptPreferences.userInteractionLevel = UserInteractionLevels.NEVER_INTERACT;
    try {
        for (var i = 0; i < app.documents.length; i++)
            if (app.documents[i].extractLabel(label) === 'owned') doc = app.documents[i];
        if (action === 'create') {
            if (app.documents.length) throw Error('fresh empty host required');
            doc = app.documents.add(true);
            doc.insertLabel(label, 'owned');
            doc.documentPreferences.properties = {facingPages:false, pageWidth:'400pt', pageHeight:'400pt'};
            doc.viewPreferences.horizontalMeasurementUnits = MeasurementUnits.POINTS;
            doc.viewPreferences.verticalMeasurementUnits = MeasurementUnits.POINTS;
            doc.viewPreferences.rulerOrigin = RulerOrigin.PAGE_ORIGIN;
            var page = doc.pages[0];
            page.marginPreferences.properties = {top:0, left:0, bottom:0, right:0};
            var red = doc.colors.add({name:'SM test red', model:ColorModel.PROCESS,
                                     space:ColorSpace.RGB, colorValue:[220,50,10]});
            var font = app.fonts.itemByName('Myriad Pro\tRegular');
            if (!font.isValid) throw Error('Myriad Pro Regular required');
            function frame(name) {
                var f = page.textFrames.add({geometricBounds:[90,90,260,260], name:name});
                f.strokeWeight = 0;
                f.fillColor = doc.swatches.itemByName('None');
                f.strokeColor = doc.swatches.itemByName('None');
                f.textFramePreferences.insetSpacing = [0,0,0,0];
                f.contents = 'U';
                f.parentStory.texts[0].properties = {appliedFont:font, pointSize:120,
                    fillColor:doc.swatches.itemByName('Black'), strokeColor:red, strokeWeight:12,
                    strokeAlignment:TextStrokeAlign.OUTSIDE_ALIGNMENT, endJoin:EndJoin.MITER_END_JOIN,
                    miterLimit:4};
                return f;
            }
            function glyphBounds(f) {
                var copy = f.duplicate(), outlines;
                copy.parentStory.texts[0].strokeWeight = 0;
                outlines = copy.createOutlines(false);
                var b = outlines[0].geometricBounds;
                for (var q = 0; q < outlines.length; q++) outlines[q].remove();
                if (copy.isValid) copy.remove();
                return b;
            }
            var bottom = frame('sm-bottom'), top = frame('sm-top');
            top.rotationAngle = 180;
            doc.recompose();
            var b = glyphBounds(bottom), t = glyphBounds(top);
            top.move(undefined, [(b[1]+b[3]-t[1]-t[3])/2, b[0]-4-t[2]]);
            if (args.startApart) bottom.move(undefined, [200,0]);
            // Set and verify the actual fixture frames after measuring copies are removed.
            var frames = [bottom, top], frameState = [];
            for (var fi = 0; fi < frames.length; fi++) {
                frames[fi].strokeWeight = 0;
                frames[fi].fillColor = doc.swatches.itemByName('None');
                frames[fi].strokeColor = doc.swatches.itemByName('None');
                if (frames[fi].strokeColor.id !== doc.swatches.itemByName('None').id)
                    throw Error('fixture frame stroke is not None');
                frameState.push(frames[fi].strokeColor.name + ':' + frames[fi].strokeWeight);
            }
            doc.insertLabel('sm-top-origin', String(top.geometricBounds));
            doc.insertLabel('sm-bottom-glyph-bounds', String(b));
            doc.insertLabel('sm-step', '0');
            doc.layoutWindows[0].activePage = page;
            doc.layoutWindows[0].zoom(ZoomOptions.FIT_PAGE);
            doc.layoutWindows[0].screenMode = ScreenModeOptions.PREVIEW_TO_PAGE;
            app.activeDocument = doc;
            app.activeWindow = doc.layoutWindows[0];
            app.select(NothingEnum.NOTHING);
            return 'CREATED|windows=' + doc.layoutWindows.length + '|gap=4|frames=' + frameState.join(',');
        }
        if (!doc || !doc.isValid) throw Error('owned fixture missing');
        var top = null, items = doc.allPageItems;
        for (var ti=0; ti<items.length; ti++) if(items[ti].name === 'sm-top') top=items[ti];
        if(!top && action !== 'close') throw Error('top frame missing');
        if (action === 'move') {
            var step = Number(doc.extractLabel('sm-step'));
            top.move(undefined, step % 2 ? [-2,-1] : [2,1]);
            doc.insertLabel('sm-step', String(step+1));
        } else if (action === 'master-art') {
            var cyan=doc.colors.add({name:'SM parent cyan',model:ColorModel.PROCESS,space:ColorSpace.RGB,colorValue:[0,180,200]});
            var parentPage=doc.masterSpreads[0].pages[0];
            var art=parentPage.rectangles.add({geometricBounds:[280,280,300,300]});
            art.strokeWeight=0; art.strokeColor=doc.swatches.itemByName('None'); art.fillColor=cyan;
            doc.pages[0].appliedMaster=doc.masterSpreads[0];
        } else if (action === 'occluder') {
            var gb=doc.extractLabel('sm-bottom-glyph-bounds').split(','), nums=[];
            for(var gi=0;gi<4;gi++) nums.push(Number(gb[gi]));
            var cyan=doc.colors.itemByName('SM test cyan');
            if(!cyan.isValid) cyan=doc.colors.add({name:'SM test cyan',model:ColorModel.PROCESS,space:ColorSpace.RGB,colorValue:[0,180,200]});
            var cover=doc.pages[0].rectangles.add({name:'sm-cover',geometricBounds:[nums[0],nums[1],nums[2],nums[1]+8]});
            cover.strokeWeight=0; cover.strokeColor=doc.swatches.itemByName('None'); cover.fillColor=cyan;
            cover.sendBackward();
        } else if (action === 'occluder-clear') doc.rectangles.itemByName('sm-cover').remove();
        else if (action === 'occluder-unpaint') doc.rectangles.itemByName('sm-cover').fillColor=doc.swatches.itemByName('None');
        else if (action === 'occluder-repaint') doc.rectangles.itemByName('sm-cover').fillColor=doc.colors.itemByName('SM test cyan');
        else if (action === 'occluder-away') doc.rectangles.itemByName('sm-cover').move(undefined,[250,0]);
        else if (action === 'occluder-return') doc.rectangles.itemByName('sm-cover').move(undefined,[-250,0]);
        else if (action === 'occluder-hide') {
            var c=doc.rectangles.itemByName('sm-cover');
            if(typeof c.visible !== 'boolean') throw Error('item visibility unsupported');
            c.visible=false;
        } else if (action === 'occluder-show') doc.rectangles.itemByName('sm-cover').visible=true;
        else if (action === 'occluder-layer-hide') {
            var layer=doc.layers.itemByName('SM occluder layer');
            if(!layer.isValid) layer=doc.layers.add({name:'SM occluder layer'});
            doc.rectangles.itemByName('sm-cover').itemLayer=layer;
            layer.visible=false;
        } else if (action === 'occluder-layer-show') doc.layers.itemByName('SM occluder layer').visible=true;
        else if (action === 'group') {
            function marker(x) {
                var r = doc.pages[0].rectangles.add({geometricBounds:[300,x,305,x+5]});
                r.strokeWeight=0; r.fillColor=doc.swatches.itemByName('None');
                r.strokeColor=doc.swatches.itemByName('None'); return r;
            }
            var inner = doc.groups.add([top, marker(300)]);
            doc.groups.add([inner, marker(310)]);
            doc.insertLabel('sm-nested', '1');
        } else if (action === 'group-move') {
            if(top.parent.constructor.name !== 'Group' || top.parent.parent.constructor.name !== 'Group') throw Error('nested groups missing');
            top.parent.parent.move(undefined, [2,1]);
        } else if (action === 'link') {
            var bottom = doc.textFrames.itemByName('sm-bottom');
            top.contents=''; bottom.nextTextFrame=top;
            bottom.parentStory.contents='U';
            bottom.parentStory.insertionPoints[-1].contents=SpecialCharacters.FRAME_BREAK;
            bottom.parentStory.insertionPoints[-1].contents='U';
            doc.recompose();
            if(bottom.parentStory.id !== top.parentStory.id || top.characters.length !== 1)
                throw Error('linked fixture did not compose into two parcels');
        } else if (action === 'window-add') {
            var win = doc.windows.add();
            win.zoom(ZoomOptions.FIT_PAGE); win.screenMode=ScreenModeOptions.PREVIEW_TO_PAGE;
        } else if (action === 'window-first') app.activeWindow = doc.layoutWindows[0];
        else if (action === 'window-second') app.activeWindow = doc.layoutWindows[1];
        else if (action === 'bottom-back') doc.textFrames.itemByName('sm-bottom').move(undefined, [-200,0]);
        else if (action === 'bottom-edit') doc.textFrames.itemByName('sm-bottom').contents='C';
        else if (action === 'bottom-restore') doc.textFrames.itemByName('sm-bottom').contents='U';
        else if (action === 'bottom-delete') doc.textFrames.itemByName('sm-bottom').remove();
        else if (action === 'distant-story') {
            var source=doc.textFrames.itemByName('sm-bottom'), extra=source.duplicate();
            extra.name='sm-distant'; extra.contents='I';
            extra.fit(FitOptions.FRAME_TO_CONTENT);
            extra.move(undefined,[130,0]);
            extra.sendBackward();
        }
        else if (action === 'apart') top.move(undefined, [200,0]);
        else if (action === 'back') top.move(undefined, [-200,0]);
        else if (action === 'edit') top.contents = 'C';
        else if (action === 'restore-text') top.contents = 'U';
        else if (action === 'export') {
            if (!args.pdf) throw Error('pdf destination required');
            var dest = new File(args.pdf);
            if (dest.exists) throw Error('refusing to overwrite export');
            doc.exportFile(ExportFormat.PDF_TYPE, dest, false,
                           app.pdfExportPresets.itemByName('[High Quality Print]'));
        } else if (action === 'close') {
            doc.close(SaveOptions.NO);
            return 'CLOSED';
        } else throw Error('unknown action');
        return 'DONE|' + action + '|windows=' + doc.layoutWindows.length + '|top=' + String(top.geometricBounds);
    } finally {
        app.scriptPreferences.userInteractionLevel = interaction;
    }
})();
