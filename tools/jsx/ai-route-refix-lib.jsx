// The route re-fix test's recolor, shared by its phases. Returned by evalFile.
(function () {
    function green() { var c = new RGBColor(); c.red = 20; c.green = 160; c.blue = 40; return c; }
    // One entry per story (its first frame), top to bottom: cases A to E. A to D get their first
    // word green; E, the last, all of its text.
    function recolor(doc) {
        var firsts = [], i;
        for (i = 0; i < doc.textFrames.length; i++) {
            var t = doc.textFrames[i];
            if (t.story.textFrames[0].uuid === t.uuid) firsts.push(t);
        }
        firsts.sort(function (a, b) { return b.position[1] - a.position[1]; });
        for (i = 0; i < firsts.length; i++) {
            var range = firsts[i].story.textRange;
            if (i === firsts.length - 1) { range.characterAttributes.fillColor = green(); continue; }
            for (var r = 0; r < 6; r++) range.characters[r].characterAttributes.fillColor = green();
        }
        app.redraw();
        return firsts.length;
    }
    return {recolor: recolor};
})();
