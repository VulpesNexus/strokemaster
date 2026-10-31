// Read the reopened native document after idle, then edit and return to idle again.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    var lib = $.evalFile(new File(new File($.fileName).parent.fsName + '/ai-text-parity-lib.jsx'));
    if (app.documents.length !== 1) return 'FAIL|owned document missing';
    var doc = app.documents[0], kind = args.kinds[0];
    if (doc.name !== args.runId + '-' + kind + '.ai') return 'FAIL|foreign document';
    try {
        var data = lib.read(args,'phase1-results.json'), row = data.rows[0];
        if (args.pluginAbsent) {
            var absent = false, reply = '';
            try { reply = String(app.sendScriptMessage('StrokeMasterRegionProbe','status','')); absent = reply === ''; }
            catch (missingPlugin) { absent = true; reply = String(missingPlugin); }
            data.pluginAbsence = {confirmed:absent,reply:reply};
            if (!absent) throw Error('StrokeMaster unexpectedly available');
        }
        row.reopened = lib.snapshot(doc);
        row.reopenPreserved = lib.json(row.after) === lib.json(row.reopened);
        lib.pdf(doc,args,kind + '-reopen');
        if (!doc.textFrames.length) throw Error('no editable frames after idle');
        var range = doc.textFrames[0].story.textRange;
        row.expectedEdit = range.contents + ' AV';
        range.characters[range.characters.length - 1].contents += ' AV';
        app.redraw();
        lib.write(args,'phase3-results.json',data);
        return 'DONE|reopened-frames=' + row.reopened.frames.length + '|edited-for-next-phase';
    } catch (e) { return 'FAIL|inspect-edit|' + String(e); }
})();
