// Read an owned AI specimen only after the separate open phase returns to idle.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (app.documents.length !== 1 || app.activeDocument.name !== args.inspectFile) return 'FAIL|wrong owned document';
    var lib = $.evalFile(new File(new File($.fileName).parent.fsName + '/ai-text-parity-lib.jsx'));
    var doc = app.activeDocument, data = {host:app.version,file:args.inspectFile,groups:doc.groupItems.length,snapshot:lib.snapshot(doc)};
    lib.write(args,'results.json',data);
    return 'DONE|frames=' + doc.textFrames.length + '|groups=' + doc.groupItems.length;
})();
