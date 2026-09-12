// EXPERIMENTAL until its JSON reports passed=true in the target host.
// Runs in disposable documents; it never duplicates or outlines user artwork.
// Run through run-ai-jsx.ps1. No output file is required.
(function () {
    var rows = [], tests = [], prior = null, doc = null;
    var count = app.documents.length, before = [];
    var oldUI = app.userInteractionLevel;
    function quote(s) {
        return '"' + String(s).replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\r/g, '\\r').replace(/\n/g, '\\n') + '"';
    }
    function json(v) {
        if (v === null) { return 'null'; }
        if (typeof v === 'string') { return quote(v); }
        if (typeof v === 'number' || typeof v === 'boolean') { return String(v); }
        var a = [], k;
        if (v instanceof Array) { for (k = 0; k < v.length; k++) { a.push(json(v[k])); } return '[' + a.join(',') + ']'; }
        for (k in v) { if (v.hasOwnProperty(k)) { a.push(quote(k) + ':' + json(v[k])); } }
        return '{' + a.join(',') + '}';
    }
    function state(d) { return [d.saved, d.pageItems.length, d.textFrames.length].join('|'); }
    function method(o, n) { try { return typeof o[n]; } catch (e) { return 'throws: ' + e.message; } }
    function check(label, yes) { tests.push({name: label, passed: Boolean(yes)}); }
    var result = {host: app.name, version: app.version, cases: rows, checks: tests};
    try { result.build = String(app.buildNumber); } catch (e0) { result.build = 'unavailable'; }
    if (count) { prior = app.activeDocument; }
    for (var d = 0; d < count; d++) { before.push({doc: app.documents[d], state: state(app.documents[d])}); }
    try {
        app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
        for (var route = 0; route < 4; route++) {
            doc = app.documents.add(DocumentColorSpace.RGB, 100, 100);
            var original = doc.textFrames.add();
            original.contents = 'MW';
            original.name = 'probe-source';
            original.position = [10, 80];
            var target = original, duplicate = null;
            if (route === 1) { target = doc.textFrames[0]; }
            if (route >= 2) {
                duplicate = original.duplicate();
                duplicate.name = 'probe-duplicate';
                target = duplicate;
                if (route === 3) { target = doc.textFrames.getByName('probe-duplicate'); }
            }
            var row = {route: ['original', 'collection', 'duplicate-result', 'duplicate-collection'][route],
                originalType: original.typename, targetType: target.typename,
                singularType: method(target, 'createOutline'), pluralType: method(target, 'createOutlines')};
            rows.push(row);
            try { target.createOutlines(); row.plural = 'unexpected success'; }
            catch (pluralError) { row.plural = 'rejected'; }
            var outlined = target.createOutline();
            row.resultType = outlined.typename;
            check(row.route + ': singular creates group', row.resultType === 'GroupItem');
            check(row.route + ': plural rejected', row.plural === 'rejected');
            if (route >= 2) { check(row.route + ': original text preserved', original.contents === 'MW'); }
            outlined.remove();
            if (route >= 2) { original.remove(); }
            check(row.route + ': owned art removed', doc.pageItems.length === 0);
            doc.close(SaveOptions.DONOTSAVECHANGES);
            doc = null;
        }
        // Deliberate exception after creating art must still close its owned document.
        try {
            doc = app.documents.add(DocumentColorSpace.RGB, 100, 100);
            doc.textFrames.add().contents = 'failure control';
            throw new Error('injected failure');
        } catch (expected) { check('failure injected', expected.message === 'injected failure'); }
        finally { if (doc) { doc.close(SaveOptions.DONOTSAVECHANGES); doc = null; } }
    } catch (err) { result.error = err.message + ' line ' + err.line; }
    finally {
        try { if (doc) { doc.close(SaveOptions.DONOTSAVECHANGES); } }
        catch (cleanup) { result.cleanupError = cleanup.message; }
        app.userInteractionLevel = oldUI;
        if (prior) { prior.activate(); }
    }
    check('document count preserved', app.documents.length === count);
    for (d = 0; d < before.length; d++) { check('user document ' + d + ' state preserved', state(before[d].doc) === before[d].state); }
    result.passed = !result.error && !result.cleanupError;
    for (var i = 0; i < tests.length; i++) { if (!tests[i].passed) { result.passed = false; } }
    return json(result);
})();
