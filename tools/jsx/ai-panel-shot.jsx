// Panel snapshot (region-prep-26): shows the StrokeMaster panel and copies the bridge's render of
// it ("ui-press shot", %TEMP%\strokemaster-panel.bmp) to <runId>-panel.bmp, with ui-status. Run
// again as a later phase (region-prep-30), it writes <runId>-panel-2.bmp: the first run ends by
// posting a Tab to the panel (ui-press tab), so the second shows the focus mark.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (!args.outDir || !args.runId) return 'FAIL|missing arguments';
    var answer = String(app.sendScriptMessage('StrokeMasterRegionProbe', 'ui-press', 'shot'));
    var shot = new File(Folder.temp.fsName + '/strokemaster-panel.bmp');
    if (!shot.exists) return 'FAIL|no shot|' + answer;
    var target = new File(args.outDir + '/' + args.runId + '-panel.bmp'), first = !target.exists;
    if (!first) target = new File(args.outDir + '/' + args.runId + '-panel-2.bmp');
    if (target.exists) return 'FAIL|refusing existing shot';
    var copied = shot.copy(target);
    shot.remove();
    var status = String(app.sendScriptMessage('StrokeMasterRegionProbe', 'ui-status', ''));
    if (first) app.sendScriptMessage('StrokeMasterRegionProbe', 'ui-press', 'tab');
    return 'DONE|' + (copied ? 'copied' : 'not copied') + '|' + answer + '|' + status;
})();
