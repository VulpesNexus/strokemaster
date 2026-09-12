// PROPOSED, UNEXECUTED host harness. Run only after a separate plugin installation.
// Uses the documented script bridge. Never guesses executeMenuCommand names.
(function () {
    var plugin = "StrokeMasterProposedProbe";
    var previous = app.documents.length ? app.activeDocument : null;
    var specimen = null;
    var record = {status: "PROPOSED", nativeExecuted: false, checks: []};
    function encode(value) {
        if (value === null) { return "null"; }
        if (typeof value === "string") {
            return '"' + value.replace(/\\/g, "\\\\").replace(/"/g, '\\"').replace(/[\u0000-\u001f]/g, function (ch) {
                return "\\u" + ("0000" + ch.charCodeAt(0).toString(16)).slice(-4);
            }) + '"';
        }
        if (typeof value === "boolean" || typeof value === "number") { return String(value); }
        var fields = [], key, i;
        if (value instanceof Array) {
            for (i = 0; i < value.length; i++) { fields.push(encode(value[i])); }
            return "[" + fields.join(",") + "]";
        }
        for (key in value) { if (value.hasOwnProperty(key)) { fields.push(encode(key) + ":" + encode(value[key])); } }
        return "{" + fields.join(",") + "}";
    }
    function check(ok, text) {
        record.checks.push({name: text, ok: !!ok});
        if (!ok) { throw new Error(text); }
    }
    function send(selector, input) {
        var raw = app.sendScriptMessage(plugin, selector, input);
        // Strictly parse this one fixed native schema; no eval or JSON global required.
        var match = /^\{"schema":1,"status":"PROPOSED","plugin":"StrokeMasterProposedProbe","ok":(true|false),"error":(-?\d+),"goCalls":(\d+),"mutations":(\d+),"strokeOrigin":([0-3]),"lastGoError":(-?\d+),"effectFixtures":(\d+),"controlFixtures":(\d+)\}$/.exec(raw);
        check(!!match, "strict bridge payload");
        var payload = {schema: 1, status: "PROPOSED", plugin: plugin, ok: match[1] === "true", error: Number(match[2]),
            goCalls: Number(match[3]), mutations: Number(match[4]), strokeOrigin: Number(match[5]),
            lastGoError: Number(match[6]), effectFixtures: Number(match[7]), controlFixtures: Number(match[8])};
        check(payload.schema === 1 && payload.plugin === plugin && payload.status === "PROPOSED", "bridge identity/schema");
        record.nativeExecuted = true; // A checked response proves the native bridge ran.
        check(payload.ok === true && payload.error === 0, "bridge action succeeded");
        return payload;
    }
    try {
        record.before = send("status", "");
        specimen = app.documents.add(DocumentColorSpace.RGB);
        send("create", "control");
        app.redraw();
        record.control = send("status", "");
        check(record.control.goCalls === record.before.goCalls && record.control.mutations === record.before.mutations,
              "ordinary control did not invoke replacement");
        check(specimen.pathItems.length === 1 && specimen.pathItems[0].pathPoints.length === 2, "control source is two anchors");
        var control = specimen.pathItems[0];
        control.name = "StrokeMaster probe control";
        send("create", "effect");
        app.redraw();
        record.effect = send("status", "");
        check(record.effect.goCalls > record.control.goCalls && record.effect.mutations > record.control.mutations,
              "effect dispatch and successful replacement observed");
        check(record.effect.lastGoError === 0 && record.effect.strokeOrigin > 0, "stroke origin and success observed");
        check(specimen.pathItems.length === 2 && control.pathPoints.length === 2, "owned source pair preserved");
        var effect = specimen.pathItems[0].name === control.name ? specimen.pathItems[1] : specimen.pathItems[0];
        check(effect.pathPoints.length === 2, "effect source remains two anchors");
        var anchor = effect.pathPoints[1].anchor;
        effect.pathPoints[1].anchor = [anchor[0] + 20, anchor[1]];
        effect.pathPoints[1].leftDirection = effect.pathPoints[1].anchor;
        effect.pathPoints[1].rightDirection = effect.pathPoints[1].anchor;
        app.redraw();
        record.edited = send("status", "");
        check(record.edited.goCalls > record.effect.goCalls && record.edited.mutations > record.effect.mutations &&
              record.edited.lastGoError === 0 && record.edited.strokeOrigin > 0 && effect.pathPoints.length === 2,
              "source edit reevaluates replacement without a final dispatch error");
        record.status = "DISPATCH_OBSERVED_ONLY";
    } catch (error) {
        record.error = String(error);
    } finally {
        // Close only the document created by this harness; user documents stay open.
        try { if (specimen) { specimen.close(SaveOptions.DONOTSAVECHANGES); } }
        catch (cleanupError) { record.cleanupError = String(cleanupError); record.status = "PROPOSED"; }
        try { if (previous) { app.activeDocument = previous; } }
        catch (restoreError) { record.restoreError = String(restoreError); record.status = "PROPOSED"; }
    }
    // Caller captures the returned record; no filesystem writes or exports here.
    return encode(record);
}());
