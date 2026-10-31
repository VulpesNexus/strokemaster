// Third COM phase, after idle: the automatic correction must keep the story linked.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (app.documents.length !== 1) return 'FAIL|owned document missing';
    var doc = app.documents[0], frames = doc.textFrames;
    if (frames.length !== 2) return 'FAIL|frames missing';
    for (var i = 0; i < 2; i++) {
        if (frames[i].note !== 'StrokeMasterParityRefix-' + args.runId) return 'FAIL|foreign frame';
        if (frames[i].story.textFrames.length !== 2) return 'FAIL|thread split';
        if (frames[i].story.textRange.contents !== 'AVMWxy AVMWxy AVMWxy AVMWxy AV') return 'FAIL|text changed';
    }
    var raw = String(app.sendScriptMessage('StrokeMasterRegionProbe','status',''));
    var status = eval('(' + raw + ')');
    var passed = status.build === 'region-prep-20' && status.refix.runs > 0 && status.refix.lastError === 0;
    var file = new File(args.outDir + '/' + args.runId + '-results.json');
    if (file.exists || !file.open('w')) return 'FAIL|result exists or cannot be opened';
    file.encoding = 'UTF-8';
    file.write('{"schema":1,"host":"' + app.version + '","storyFrames":2,"textPreserved":true,"passed":' + passed + ',"status":' + raw + '}');
    file.close();
    return (passed ? 'DONE' : 'FAIL') + '|storyFrames=2|refixRuns=' + status.refix.runs + '|lastError=' + status.refix.lastError;
})();
