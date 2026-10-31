// Second COM phase, after the timer has registered the owned marked frames.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (app.documents.length !== 1) return 'FAIL|owned document missing';
    var doc = app.documents[0];
    if (doc.textFrames.length !== 2) return 'FAIL|frames missing';
    for (var i = 0; i < 2; i++)
        if (doc.textFrames[i].note !== 'StrokeMasterParityRefix-' + args.runId) return 'FAIL|foreign frame';
    var range = doc.textFrames[0].story.textRange;
    range.characters[range.characters.length - 1].contents += ' AV';
    app.redraw();
    return 'DONE|edited=' + range.contents.length;
})();
