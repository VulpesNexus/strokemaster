// Automatic correction tests: the art they make, and what each item carries (ai-auto-*.jsx).
(function () {
    function gray(k) { var c = new GrayColor(); c.gray = k; return c; }
    function rgb(r, g, b) { var c = new RGBColor(); c.red = r; c.green = g; c.blue = b; return c; }
    function send(s, i) { return String(app.sendScriptMessage('StrokeMasterRegionProbe', s, i || '')); }
    // Point text, red fill, gray 7 pt stroke (Minion Pro 38 pt): the route takes its stroke.
    function text(doc, name, x, y) {
        var t = doc.textFrames.add(); t.contents = 'AVMWxy AVMWxy'; t.position = [x, y]; t.name = name;
        var a = t.textRange.characterAttributes;
        a.textFont = app.textFonts.getByName('MinionPro-Regular'); a.size = 38;
        a.fillColor = rgb(200, 30, 30); a.strokeColor = gray(40); a.strokeWeight = 7;
        return t;
    }
    // An acute triangle (about 20 degrees at its apex, a miter ratio of 5.76), 10 pt Center
    // stroke with a limit of 4: Whole points past the miter limit raises it to keep the apex whole.
    function triangle(doc, name, x) {
        var t = doc.pathItems.add();
        t.setEntirePath([[x, 100], [x + 120, 100], [x + 60, 440]]); t.closed = true; t.name = name;
        t.filled = true; t.fillColor = rgb(30, 60, 200); t.stroked = true; t.strokeColor = gray(40); t.strokeWidth = 10;
        t.strokeMiterLimit = 4;
        return t;
    }
    // What an item carries: its style dump (a route's Outline Object pass, a path's limit).
    function read(doc, name) {
        var item = null;
        try { item = doc.pageItems.getByName(name); } catch (e) { return {exists: false}; }
        var wasLocked = item.locked;
        if (wasLocked) item.locked = false;
        doc.selection = null; item.selected = true;
        var dump = send('style-dump');
        doc.selection = null;
        if (wasLocked) item.locked = true;
        var limit = /STROKE w [0-9.]+ join 0 limit ([0-9.]+)/.exec(dump);
        return {exists: true, routed: dump.indexOf("'Adobe Outline Type'") >= 0, limit: limit ? Number(limit[1]) : null, dump: dump};
    }
    return {text: text, triangle: triangle, read: read, gray: gray};
})();
