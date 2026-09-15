// StrokeMaster region host proof. Runs inside Illustrator via COM DoJavaScript.
// Creates the marked analytic fixture, lets the live effect fire, exports
// effect/control PDFs for independent measurement, and reports bridge evidence.
// ExtendScript has no JSON object, so the return record is pipe-delimited.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    var logFile = args.log ? new File(args.log) : null;
    function checkpoint(stage, detail) {
        if (!logFile) return;
        try {
            logFile.encoding = 'UTF-8';
            logFile.open('a');
            logFile.writeln(new Date().toISOString() + ' ' + stage + (detail ? ' ' + detail : ''));
            logFile.close();
        } catch (ignored) {}
    }
    function fail(message) { return 'FAIL|' + String(message).replace(/[|\r\n]/g, ' '); }
    try {
        var plugin = 'StrokeMasterRegionProbe';
        var requireBuild = args.requireBuild || 'region-prep-2';
        var at = 'begin';
        function send(selector, input) {
            var raw = app.sendScriptMessage(plugin, selector, input || '');
            if (raw === undefined || raw === null) throw 'sendScriptMessage returned no payload for ' + selector;
            return String(raw);
        }
        try {
        at = 'status-before';
        var statusBefore = send('status');
        checkpoint('status-before', statusBefore);
        if (statusBefore.indexOf('"build":"' + requireBuild + '"') < 0) throw 'unexpected plugin build: ' + statusBefore;
        if (statusBefore.indexOf('"effectRegistered":true') < 0) throw 'region effect not registered';
        if (statusBefore.indexOf('"geometryEnabled":true') < 0) throw 'region effect is not geometry enabled';

        at = 'add-document';
        var doc = null, addError = '';
        for (var attempt = 0; attempt < 10 && !doc; ++attempt) {
            try {
                doc = app.documents.add(DocumentColorSpace.RGB, 140, 140);
            } catch (addErrorIgnored) {
                addError = String(addErrorIgnored);
                $.sleep(2000);
            }
        }
        if (!doc) throw 'documents.add failed repeatedly: ' + addError;
        checkpoint('document-added');
        at = 'artboard-rect';
        doc.artboards[0].artboardRect = [-20, 120, 120, -20];
        at = 'redraw-1';
        app.redraw();

        at = 'create-region';
        var created = send('create', 'region');
        checkpoint('created-region', created);
        if (created.indexOf('"ok":true') < 0) throw 'create region failed: ' + created;
        at = 'redraw-2';
        app.redraw();
        checkpoint('redrawn-after-region');

        at = 'save-effect';
        var options = new PDFSaveOptions();
        doc.saveAs(new File(args.effectPdf), options);
        checkpoint('saved-effect-pdf');

        at = 'status-after';
        var statusAfterEffect = send('status');
        checkpoint('status-after', statusAfterEffect);
        if (statusAfterEffect.indexOf('"goCalls":0') >= 0) throw 'live effect never executed (goCalls=0)';
        if (statusAfterEffect.indexOf('"mutations":0') >= 0) throw 'live effect never committed (mutations=0)';
        if (statusAfterEffect.indexOf('"lastGoError":0') < 0) throw 'live effect reported an error: ' + statusAfterEffect;
        if (statusAfterEffect.indexOf('"gate":"published"') < 0) throw 'unexpected effect gate: ' + statusAfterEffect;

        at = 'remove-fixture';
        while (doc.pageItems.length > 0) doc.pageItems[0].remove();
        at = 'create-control';
        var control = send('create', 'control');
        if (control.indexOf('"ok":true') < 0) throw 'create control failed: ' + control;
        at = 'redraw-3';
        app.redraw();
        at = 'save-control';
        doc.saveAs(new File(args.controlPdf), options);
        checkpoint('saved-control-pdf');

        at = 'status-final';
        var statusFinal = send('status');
        at = 'diagnostics';
        var diagnostics = send('diagnostics');
        at = 'close';
        // Close the disposable document without saving; user documents untouched.
        doc.close(SaveOptions.DONOTSAVECHANGES);
        checkpoint('closed-document');
        return 'PASS|' + statusBefore + '|' + statusAfterEffect + '|' + statusFinal + '|' + diagnostics;
        } catch (error) {
            return fail(at + ': ' + error);
        }
    } catch (error) {
        return fail(error);
    }
})();
