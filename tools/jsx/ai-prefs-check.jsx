// ai-prefs-check.jsx - whether Illustrator starts on its preferences and documents.add works
// (2026-10-16): documents.add failed with error 129 while File > New's recent sizes were full of
// scripted entries. tools/run-ai-fresh.ps1 puts the preferences back after the quit, so the
// document made here leaves no recent entry. Returns the outcome; makes no file.
(function () {
    var out = ['version=' + app.version];
    try {
        var doc = app.documents.add(DocumentColorSpace.RGB, 200, 200);
        out.push('documents.add=ok ' + doc.width + 'x' + doc.height);
        doc.close(SaveOptions.DONOTSAVECHANGES);
    } catch (e) {
        out.push('documents.add=error ' + String(e).replace(/[|\r\n]/g, ' '));
    }
    return 'DONE|' + out.join('|');
})();
