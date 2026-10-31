// Inspect only the owned synthetic group-fill specimen after native AI reopen.
(function () {
    if (app.documents.length) return 'FAIL|preexisting document';
    var args = $.global.ISE_REGION_ARGS || {}, doc = null;
    var interaction = app.userInteractionLevel;
    app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
    function walk(items, depth, out) {
        if (depth > 4) return;
        for (var i = 0; i < items.length && i < 40; i++) {
            var item = items[i], row = depth + ':' + item.typename;
            if (item.typename === 'TextFrame') row += ':characters=' + item.contents.length;
            if (item.typename === 'GroupItem') row += ':textFrames=' + item.textFrames.length + ':children=' + item.pageItems.length;
            out.push(row);
            if (item.typename === 'GroupItem') walk(item.pageItems, depth + 1, out);
        }
    }
    try {
        doc = app.open(new File(args.outDir + '/story-minion-threaded.ai'));
        var rows = ['documentTextFrames=' + doc.textFrames.length, 'groups=' + doc.groupItems.length];
        walk(doc.pageItems,0,rows);
        var text = rows.join('|');
        var f = new File(args.outDir + '/' + args.runId + '-results.txt');
        if (f.exists || !f.open('w')) return 'FAIL|output exists or cannot open';
        f.write(text); f.close();
        return 'DONE|' + text;
    } finally {
        if (doc) doc.close(SaveOptions.DONOTSAVECHANGES);
        app.userInteractionLevel = interaction;
    }
})();
