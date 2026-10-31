// Inspect the live edit after idle and retain both the successful and failed gates.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    var lib = $.evalFile(new File(new File($.fileName).parent.fsName + '/ai-text-parity-lib.jsx'));
    if (app.documents.length !== 1) return 'FAIL|owned document missing';
    var doc = app.documents[0], kind = args.kinds[0];
    if (doc.name !== args.runId + '-' + kind + '-reopen.pdf') return 'FAIL|foreign document';
    try {
        var data = lib.read(args,'phase3-results.json'), row = data.rows[0];
        row.afterEdit = lib.snapshot(doc);
        row.editable = doc.textFrames.length === row.after.frames.length && doc.textFrames.length > 0 &&
            doc.textFrames[0].story.textRange.contents === row.expectedEdit;
        if (kind === 'threaded') for (var i = 0; i < row.afterEdit.frames.length; i++)
            if (row.afterEdit.frames[i].storyFrames !== row.after.frames[i].storyFrames) row.editable = false;
        lib.pdf(doc,args,kind + '-edited');
        if (args.editedReference) {
            for (var f = 0; f < doc.textFrames.length; f++) {
                var chars = doc.textFrames[f].story.textRange.characters;
                for (var c = 0; c < chars.length; c++) chars[c].characterAttributes.strokeColor = new NoColor();
            }
            app.redraw();
            lib.pdf(doc,args,kind + '-edited-fill');
        }
        lib.write(args,'results.json',data);
        return (row.reopenPreserved && row.editable ? 'DONE' : 'FAIL') + '|reopen=' + row.reopenPreserved + '|editable=' + row.editable;
    } catch (e) { return 'FAIL|final|' + String(e); }
})();
