// StrokeMaster status, for the owner to run in their own Illustrator (File > Scripts > Other
// Script...) right after automatic correction seemed to do nothing. Read-only: it changes nothing
// in any document and writes the plugin's status, the open documents' names and counts, and the
// selection's style dump to strokemaster-status.txt in the temporary folder, then says where.
(function () {
    function send(s, i) { try { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); } catch (e) { return 'ERROR ' + e; } }
    var lines = ['time ' + new Date(), 'documents ' + app.documents.length];
    for (var i = 0; i < app.documents.length; i++) {
        var d = app.documents[i];
        lines.push('  ' + (d === app.activeDocument ? '* ' : '  ') + d.name + ' items ' + d.pageItems.length + ' text ' + d.textFrames.length);
    }
    lines.push('settings ' + send('settings', '?').replace(/\n/g, ' '));
    lines.push('status ' + send('status'));
    lines.push('selection ' + (app.documents.length ? app.activeDocument.selection.length : 0));
    if (app.documents.length && app.activeDocument.selection.length) lines.push('style ' + send('style-dump'));
    var f = new File(Folder.temp.fsName + '/strokemaster-status.txt');
    f.encoding = 'UTF-8';
    if (!f.open('w')) { alert('StrokeMaster status: cannot write ' + f.fsName); return; }
    f.write(lines.join('\n'));
    f.close();
    alert('StrokeMaster status written to\n' + f.fsName);
})();
