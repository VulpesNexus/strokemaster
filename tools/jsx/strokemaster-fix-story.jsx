// Experimental native story route. The plugin writes one group stroke below the text when one
// stroke runs through the frames grouped, else one group fill above it when one fill does;
// fills and strokes both mixed are refused. Select every frame of a linked story.
// - Artwork between the frames in the stacking order: invisible items (unpainted paths, empty
//   text frames, groups of those) are moved below the story's frames, which changes nothing
//   drawn. Visible artwork stays where it is and splits the story into runs of adjacent frames,
//   each corrected within itself (story-route "part"; a frame alone gets native-route); across
//   the artwork the stacking stays, as InDesign's plugin keeps its order where such content
//   covers a letter.
// - Frames of several complete stories may be selected together: that is the opt-in of
//   InDesign's "Across separate stories" switch (one story's strokes also go beneath another's
//   fills, so a deliberate halo over another frame is lost), confirmed before anything changes
//   (in a test, ISE_REGION_ARGS.separate). Separate stories need no visible artwork between.
// Any refusal restores the original order, grouping, and selection.
(function () {
    var quiet = !!$.global.ISE_REGION_ARGS;
    var allowSeparate = quiet && !!$.global.ISE_REGION_ARGS.separate;
    function finish(raw, message) { if (!quiet) alert(message); return raw; }
    function refuse(code, message) {
        return finish('{"ok":false,"appearance":{"gate":"' + code + '","styled":0}}', 'StrokeMaster: ' + message);
    }
    function id(item) {
        var value = item.uuid;
        if (typeof value !== 'string' || !value.length) throw Error('stable item identity unavailable');
        return value;
    }
    function direct(parent) {
        var out = [];
        for (var i = 0; i < parent.pageItems.length; i++)
            if (parent.pageItems[i].parent == parent) out.push(parent.pageItems[i]);
        return out;
    }
    function ids(items) { var out = []; for (var i = 0; i < items.length; i++) out.push(id(items[i])); return out.join('|'); }
    // Whether an item draws nothing at all.
    function invisible(item) {
        try {
            if (item.hidden) return true;
            switch (item.typename) {
            case 'PathItem': return !item.filled && !item.stroked && !item.clipping;
            case 'CompoundPathItem':
                for (var p = 0; p < item.pathItems.length; p++) if (!invisible(item.pathItems[p])) return false;
                return true;
            case 'TextFrame': return item.characters.length === 0;
            case 'GroupItem':
                if (item.clipped) return false;
                for (var g = 0; g < item.pageItems.length; g++) if (!invisible(item.pageItems[g])) return false;
                return true;
            default: return false;
            }
        } catch (e) { return false; }
    }
    if (!app.documents.length) return refuse('story.no-document','open a document first.');
    var doc = app.activeDocument, selected = doc.selection, groups = [], parent = null, original = null;
    if (!(selected instanceof Array) || selected.length < 2 || selected.length > 64)
        return refuse('story.selection','select every frame in one linked story.');
    // Puts every sibling back in its original order and takes apart the groups made here.
    function restore() {
        var ok = true;
        for (var g = groups.length - 1; g >= 0; g--) {
            try {
                var kids = direct(groups[g]);
                for (var k = 0; k < kids.length; k++) kids[k].move(groups[g], ElementPlacement.PLACEBEFORE);
                if (!groups[g].pageItems.length) groups[g].remove(); else ok = false;
            } catch (e) { ok = false; }
        }
        groups = [];
        if (parent && original) {
            try {
                var now = direct(parent), byId = {};
                for (var n = 0; n < now.length; n++) byId[id(now[n])] = now[n];
                for (var o = 0; o < original.length; o++) if (byId[original[o]]) byId[original[o]].move(parent, ElementPlacement.PLACEATEND);
                if (ids(direct(parent)) !== original.join('|')) ok = false;
            } catch (e2) { ok = false; }
        }
        doc.selection = null;
        for (var s = 0; s < selected.length; s++) try { selected[s].selected = true; } catch (e3) { ok = false; }
        return ok;
    }
    try {
        var chosen = {}, i;
        parent = selected[0].parent;
        for (i = 0; i < selected.length; i++) {
            if (selected[i].typename !== 'TextFrame' || selected[i].parent != parent || !selected[i].editable)
                return refuse('story.parent','the selected text frames must be editable siblings.');
            chosen[id(selected[i])] = true;
        }
        // Every story the selection touches must be selected whole.
        var storyFirsts = {}, storyCount = 0;
        for (i = 0; i < selected.length; i++) {
            var linked = selected[i].story.textFrames, key = id(linked[0]);
            if (storyFirsts[key]) continue;
            storyFirsts[key] = true; ++storyCount;
            for (var l = 0; l < linked.length; l++)
                if (!chosen[id(linked[l])]) return refuse('story.incomplete','select every frame of each linked story.');
        }
        if (storyCount > 1 && !quiet)
            allowSeparate = confirm('StrokeMaster: the selection holds ' + storyCount + ' separate stories. Put the strokes of each ' +
                'beneath the fills of the others too? A deliberate halo over another frame would be lost.');
        if (storyCount > 1 && !allowSeparate) return refuse('story.separate','select the frames of one linked story.');
        var siblings = direct(parent), first = -1, last = -1;
        original = [];
        for (i = 0; i < siblings.length; i++) {
            original.push(id(siblings[i]));
            if (chosen[id(siblings[i])]) { if (first < 0) first = i; last = i; }
        }
        // Between the first and last frame: invisible items move below the last frame; visible
        // ones split the frames into runs.
        var runs = [[]], hidden = [], visible = 0;
        for (i = first; i <= last; i++) {
            if (chosen[id(siblings[i])]) { runs[runs.length - 1].push(siblings[i]); continue; }
            if (invisible(siblings[i])) { hidden.push(siblings[i]); continue; }
            ++visible;
            if (runs[runs.length - 1].length) runs.push([]);
        }
        if (visible && storyCount > 1) return refuse('story.intervening-art','separate stories need no visible artwork between their frames.');
        var lastFrame = siblings[last];
        for (i = hidden.length - 1; i >= 0; i--) hidden[i].move(lastFrame, ElementPlacement.PLACEAFTER);
        var expected = ids(direct(parent)), done = [], skipped = 0, alone = [];
        for (var r = 0; r < runs.length; r++) {
            if (runs[r].length < 2) { for (i = 0; i < runs[r].length; i++) alone.push(runs[r][i]); continue; }
            var group = parent.groupItems.add();
            groups.push(group);
            group.move(runs[r][0], ElementPlacement.PLACEBEFORE);
            for (i = 0; i < runs[r].length; i++) runs[r][i].move(group, ElementPlacement.PLACEATEND);
            doc.selection = null; group.selected = true;
            var input = storyCount > 1 ? 'separate' : (visible ? 'part' : '');
            var raw = String(app.sendScriptMessage('StrokeMasterRegionProbe', 'story-route', input));
            // ExtendScript (ES3) has no JSON.parse; the text is the plugin's own bridge answer.
            var result = eval('(' + raw + ')');
            if (!result.ok) throw Error(result.appearance ? result.appearance.gate : 'native story refusal');
            done.push(raw);
        }
        if (!done.length && !alone.length) throw Error('no frames to correct');
        // The flattened order is what it was after the invisible items moved.
        var after = direct(parent), expanded = [];
        for (i = 0; i < after.length; i++) {
            var made = false;
            for (var g = 0; g < groups.length; g++) if (id(groups[g]) === id(after[i])) made = true;
            if (made) { var kids = direct(after[i]); for (var j = 0; j < kids.length; j++) expanded.push(kids[j]); }
            else expanded.push(after[i]);
        }
        if (ids(expanded) !== expected) throw Error('grouping changed paint order');
        // A frame alone between artwork gets the route of a single frame (native-route), so it is
        // corrected within itself.
        for (i = 0; i < alone.length; i++) {
            doc.selection = null; alone[i].selected = true;
            var one = String(app.sendScriptMessage('StrokeMasterRegionProbe', 'native-route', ''));
            if (/"ok":true/.test(one)) done.push(one); else ++skipped;
        }
        doc.selection = null;
        for (var s = 0; s < groups.length; s++) groups[s].selected = true;
        for (i = 0; i < alone.length; i++) alone[i].selected = true;
        var raw0 = done[0];
        if (done.length === 1 && !visible && !hidden.length)
            return finish(raw0, 'StrokeMaster: the linked frames now paint every stroke beneath every fill (' +
                (/"gate":"story\.stroked"/.test(raw0) ? 'one group stroke below the text' : 'one group fill above the text') +
                '). Recolors are followed while StrokeMaster runs.');
        var summary = '{"ok":true,"appearance":{"gate":"story.runs","styled":' + done.length + ',"frames":' + selected.length +
                      ',"alone":' + alone.length + ',"skipped":' + skipped + ',"visible":' + visible + ',"moved":' + hidden.length + '},"runs":[' + done.join(',') + ']}';
        return finish(summary, 'StrokeMaster: ' + done.length + ' run(s) of adjacent frames corrected' +
            (visible ? '; the ' + visible + ' visible item(s) between them keep their place' : '') +
            (hidden.length ? '; ' + hidden.length + ' invisible item(s) moved below the story' : '') +
            (alone.length ? '; ' + (alone.length - skipped) + ' frame(s) alone between artwork corrected within themselves' : '') +
            (skipped ? '; ' + skipped + ' refused' : '') + '.');
    } catch (e) {
        var ok = restore();
        return refuse(ok ? 'story.refused-restored' : 'story.rollback-failed',
            ok ? 'the native route refused this story; the original artwork was restored (' + String(e) + ').' :
                'restoration failed; inspect the retained groups before continuing (' + String(e) + ').');
    }
})();
